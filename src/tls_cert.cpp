#include "tls_cert.h"
#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/pk.h>
#include <mbedtls/ecp.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/oid.h>
#include <mbedtls/sha256.h>
#include <lwip/ip4_addr.h>
#include <vector>

namespace tlscert {

static String gCert, gKey, gId;
static uint32_t gExp = 0;
static const char* AP_IP = "192.168.4.1";  // ESP32 soft-AP default
static const char* NS = "tempguardtls";
static const char* OLD_NS = "studiotls";  // pre-rename namespace, migrated once
static const uint32_t VALID_DAYS = 800;  // Apple rejects server certs valid > 825 days

static int rng(void*, unsigned char* out, size_t len) { esp_fill_random(out, len); return 0; }

// DER for a GeneralNames SEQUENCE with dNSName [2] and iPAddress [7] entries.
static std::vector<uint8_t> buildSan(const std::vector<String>& dns, const std::vector<String>& ips) {
  std::vector<uint8_t> inner;
  for (auto& d : dns) {
    inner.push_back(0x82); inner.push_back((uint8_t)d.length());
    inner.insert(inner.end(), d.begin(), d.end());
  }
  for (auto& ip : ips) {
    ip4_addr_t a;
    if (!ip.length() || !ip4addr_aton(ip.c_str(), &a)) continue;
    uint32_t v = a.addr;  // network order already
    inner.push_back(0x87); inner.push_back(4);
    for (int i = 0; i < 4; i++) inner.push_back((v >> (8 * i)) & 0xFF);
  }
  std::vector<uint8_t> out = {0x30, (uint8_t)inner.size()};
  out.insert(out.end(), inner.begin(), inner.end());
  return out;
}

static String fmtTime(time_t t) {
  struct tm tm; gmtime_r(&t, &tm);
  char b[16]; strftime(b, sizeof(b), "%Y%m%d%H%M%S", &tm);
  return b;
}

static bool generate(const String& hostname, const String& ip, time_t now, String& cert, String& key) {
  mbedtls_pk_context pk; mbedtls_pk_init(&pk);
  mbedtls_x509write_cert crt; mbedtls_x509write_crt_init(&crt);
  mbedtls_mpi serial; mbedtls_mpi_init(&serial);
  bool ok = false;
  std::vector<unsigned char> buf(2048);
  do {
    if (mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY))) break;
    if (mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(pk), rng, nullptr)) break;

    String cn = "CN=" + hostname + ".local,O=Temperature Guard";
    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&crt, &pk);
    mbedtls_x509write_crt_set_issuer_key(&crt, &pk);
    if (mbedtls_x509write_crt_set_subject_name(&crt, cn.c_str())) break;
    if (mbedtls_x509write_crt_set_issuer_name(&crt, cn.c_str())) break;
    uint8_t sb[16]; esp_fill_random(sb, sizeof(sb)); sb[0] &= 0x7F;
    mbedtls_mpi_read_binary(&serial, sb, sizeof(sb));
    if (mbedtls_x509write_crt_set_serial(&crt, &serial)) break;
    time_t from = now - 86400;
    if (mbedtls_x509write_crt_set_validity(&crt, fmtTime(from).c_str(), fmtTime(from + VALID_DAYS * 86400UL).c_str())) break;
    mbedtls_x509write_crt_set_basic_constraints(&crt, 0, -1);
    mbedtls_x509write_crt_set_key_usage(&crt, MBEDTLS_X509_KU_DIGITAL_SIGNATURE | MBEDTLS_X509_KU_KEY_AGREEMENT);
    std::vector<String> ips = {AP_IP};
    if (ip.length() && ip != AP_IP) ips.insert(ips.begin(), ip);
    auto san = buildSan({hostname + ".local", hostname}, ips);
    if (mbedtls_x509write_crt_set_extension(&crt, MBEDTLS_OID_SUBJECT_ALT_NAME, MBEDTLS_OID_SIZE(MBEDTLS_OID_SUBJECT_ALT_NAME),
                                            0, san.data(), san.size())) break;
    // extendedKeyUsage = serverAuth: Apple platforms reject trusted server certs without it.
    static const unsigned char eku[] = {0x30, 0x0A, 0x06, 0x08, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01};
    if (mbedtls_x509write_crt_set_extension(&crt, MBEDTLS_OID_EXTENDED_KEY_USAGE, MBEDTLS_OID_SIZE(MBEDTLS_OID_EXTENDED_KEY_USAGE),
                                            0, eku, sizeof(eku))) break;

    if (mbedtls_x509write_crt_pem(&crt, buf.data(), buf.size(), rng, nullptr)) break;
    cert = (const char*)buf.data();
    if (mbedtls_pk_write_key_pem(&pk, buf.data(), buf.size())) break;
    key = (const char*)buf.data();
    ok = true;
  } while (0);
  mbedtls_mpi_free(&serial);
  mbedtls_x509write_crt_free(&crt);
  mbedtls_pk_free(&pk);
  return ok;
}

bool ensure(const String& hostname, const String& ip) {
  String id = hostname + "|" + ip;
  time_t now = time(nullptr);
  bool timeValid = now > 1700000000;

  Preferences p;
  p.begin(NS, true);
  String cert = p.getString("crt", ""), key = p.getString("key", ""), sid = p.getString("id", "");
  uint32_t exp = p.getUInt("exp", 0);
  p.end();
  if (cert.isEmpty()) {  // keep the certificate people already trusted across the rename
    p.begin(OLD_NS, true);
    cert = p.getString("crt", ""); key = p.getString("key", ""); sid = p.getString("id", ""); exp = p.getUInt("exp", 0);
    p.end();
    if (!cert.isEmpty()) {
      p.begin(NS, false);
      p.putString("crt", cert); p.putString("key", key); p.putString("id", sid); p.putUInt("exp", exp);
      p.end();
      p.begin(OLD_NS, false); p.clear(); p.end();
    }
  }

  // AP-only (no WiFi IP): keep any certificate for this hostname rather than replacing a
  // trusted one just because the router was slow to come back after a power cut.
  bool idOk = ip.isEmpty() ? sid.startsWith(hostname + "|") : sid == id;
  bool need = cert.isEmpty() || key.isEmpty() || !idOk || (timeValid && (time_t)exp < now + 30 * 86400L);
  if (!need) { gCert = cert; gKey = key; gId = sid; gExp = exp; return true; }

  // Without NTP yet, date the cert from the build date so it is at least plausible.
  if (!timeValid) {
    struct tm tm = {}; strptime(__DATE__, "%b %d %Y", &tm); now = mktime(&tm) + 86400;
  }
  uint32_t t0 = millis();
  if (!generate(hostname, ip, now, cert, key)) { Serial.println("[TLS] certificate generation failed"); return false; }
  Serial.printf("[TLS] generated certificate for %s (%u ms)\n", id.c_str(), (unsigned)(millis() - t0));

  uint32_t newExp = (uint32_t)(now - 86400 + VALID_DAYS * 86400UL);
  p.begin(NS, false);
  p.putString("crt", cert); p.putString("key", key); p.putString("id", id);
  p.putUInt("exp", newExp);
  p.end();
  gCert = cert; gKey = key; gId = id; gExp = newExp;
  return true;
}

bool stale(const String& hostname, const String& ip) {
  if (gCert.isEmpty() || gId != hostname + "|" + ip) return true;
  time_t now = time(nullptr);
  return now > 1700000000 && (time_t)gExp < now + 30 * 86400L;
}

const String& certPem() { return gCert; }
const String& keyPem() { return gKey; }

String fingerprintSha256() {
  mbedtls_x509_crt c; mbedtls_x509_crt_init(&c);
  String out;
  if (mbedtls_x509_crt_parse(&c, (const unsigned char*)gCert.c_str(), gCert.length() + 1) == 0) {
    uint8_t h[32];
    mbedtls_sha256_ret(c.raw.p, c.raw.len, h, 0);
    char b[4];
    for (int i = 0; i < 32; i++) { snprintf(b, sizeof(b), i ? ":%02X" : "%02X", h[i]); out += b; }
  }
  mbedtls_x509_crt_free(&c);
  return out;
}

}  // namespace tlscert
