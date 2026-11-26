#include "tuya.h"
#include <WiFi.h>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>
#include <esp_random.h>
#include <vector>

namespace tuya {

using Bytes = std::vector<uint8_t>;

static const uint32_t PREFIX = 0x000055AA;
static const uint32_t SUFFIX = 0x0000AA55;
static const uint16_t PORT = 6668;
static const uint32_t TIMEOUT_MS = 4000;

enum Cmd : uint32_t {
  SESS_KEY_NEG_START = 3, SESS_KEY_NEG_RESP = 4, SESS_KEY_NEG_FINISH = 5,
  CONTROL = 7, STATUS = 8, CONTROL_NEW = 13,
};

static void put32(Bytes& b, uint32_t v) {
  b.push_back(v >> 24); b.push_back(v >> 16); b.push_back(v >> 8); b.push_back(v);
}
static uint32_t get32(const uint8_t* p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static uint32_t crc32(const uint8_t* d, size_t n) {
  uint32_t c = 0xFFFFFFFF;
  for (size_t i = 0; i < n; i++) {
    c ^= d[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320 & (0 - (c & 1)));
  }
  return ~c;
}

// AES-128-ECB. pad=true applies PKCS7.
static Bytes aesEnc(const uint8_t key[16], const uint8_t* in, size_t n, bool pad = true) {
  Bytes buf(in, in + n);
  if (pad) { uint8_t p = 16 - (n % 16); buf.insert(buf.end(), p, p); }
  Bytes out(buf.size());
  mbedtls_aes_context ctx; mbedtls_aes_init(&ctx);
  mbedtls_aes_setkey_enc(&ctx, key, 128);
  for (size_t i = 0; i + 16 <= buf.size(); i += 16)
    mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, &buf[i], &out[i]);
  mbedtls_aes_free(&ctx);
  return out;
}

static Bytes aesDec(const uint8_t key[16], const uint8_t* in, size_t n) {
  Bytes out(n - (n % 16));
  mbedtls_aes_context ctx; mbedtls_aes_init(&ctx);
  mbedtls_aes_setkey_dec(&ctx, key, 128);
  for (size_t i = 0; i + 16 <= out.size(); i += 16)
    mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_DECRYPT, in + i, &out[i]);
  mbedtls_aes_free(&ctx);
  if (!out.empty()) {
    uint8_t p = out.back();
    if (p >= 1 && p <= 16 && p <= out.size()) out.resize(out.size() - p);
  }
  return out;
}

static void hmac(const uint8_t key[16], const uint8_t* d, size_t n, uint8_t out[32]) {
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, 16, d, n, out);
}

// Build a 55AA frame. v34 => HMAC-SHA256 trailer, else CRC32.
static Bytes frame(uint32_t seq, uint32_t cmd, const Bytes& payload, bool v34, const uint8_t key[16]) {
  Bytes f;
  put32(f, PREFIX); put32(f, seq); put32(f, cmd);
  put32(f, payload.size() + (v34 ? 32 : 4) + 4);
  f.insert(f.end(), payload.begin(), payload.end());
  if (v34) { uint8_t h[32]; hmac(key, f.data(), f.size(), h); f.insert(f.end(), h, h + 32); }
  else put32(f, crc32(f.data(), f.size()));
  put32(f, SUFFIX);
  return f;
}

struct Msg { uint32_t cmd = 0; uint32_t retcode = 0; bool hasRet = false; Bytes payload; };

// Read one frame. Payload excludes retcode and trailer.
static bool readMsg(WiFiClient& c, bool v34, Msg& m, String& err) {
  uint8_t hdr[16];
  uint32_t t0 = millis();
  size_t got = 0;
  while (got < 16) {
    if (millis() - t0 > TIMEOUT_MS) { err = "timeout waiting for reply"; return false; }
    if (!c.connected() && !c.available()) { err = "connection closed"; return false; }
    int r = c.read(hdr + got, 16 - got);
    if (r > 0) got += r; else delay(5);
  }
  if (get32(hdr) != PREFIX) { err = "bad prefix"; return false; }
  m.cmd = get32(hdr + 8);
  uint32_t len = get32(hdr + 12);
  if (len > 4096 || len < 8) { err = "bad length"; return false; }
  Bytes body(len);
  got = 0;
  while (got < len) {
    if (millis() - t0 > TIMEOUT_MS) { err = "timeout reading body"; return false; }
    int r = c.read(body.data() + got, len - got);
    if (r > 0) got += r; else delay(5);
  }
  size_t trailer = (v34 ? 32 : 4) + 4;
  if (len < trailer) { err = "short frame"; return false; }
  size_t dataLen = len - trailer;
  size_t off = 0;
  // Device->client frames normally carry a 4-byte return code before the data.
  // Encrypted data is a multiple of 16, so a remainder of 4 means a retcode is present.
  bool hasRet = v34 ? (dataLen % 16 == 4) : (dataLen >= 4 && body[0] == 0 && body[1] == 0);
  if (hasRet) { m.retcode = get32(body.data()); m.hasRet = true; off = 4; }
  m.payload.assign(body.begin() + off, body.begin() + dataLen);
  return true;
}

static String controlJson(const String& devId, uint8_t dp, bool on, bool v34) {
  uint32_t t = (uint32_t)time(nullptr);
  String s;
  if (v34) {
    s = "{\"protocol\":5,\"t\":" + String(t) + ",\"data\":{\"dps\":{\"" + String(dp) + "\":" + (on ? "true" : "false") + "}}}";
  } else {
    s = "{\"devId\":\"" + devId + "\",\"uid\":\"" + devId + "\",\"t\":\"" + String(t) +
        "\",\"dps\":{\"" + String(dp) + "\":" + (on ? "true" : "false") + "}}";
  }
  return s;
}

bool setSwitch(const String& ip, const String& devId, const String& localKey,
               const String& version, uint8_t dp, bool on, String& err) {
  if (localKey.length() != 16) { err = "local key must be 16 characters"; return false; }
  if (devId.isEmpty() || ip.isEmpty()) { err = "device id / ip not set"; return false; }
  bool v34 = version == "3.4";
  if (!v34 && version != "3.3") { err = "unsupported protocol " + version; return false; }

  uint8_t key[16];
  memcpy(key, localKey.c_str(), 16);

  WiFiClient c;
  c.setTimeout(TIMEOUT_MS / 1000);
  if (!c.connect(ip.c_str(), PORT, TIMEOUT_MS)) { err = "cannot connect to " + ip; return false; }
  c.setNoDelay(true);

  uint32_t seq = 1;
  uint8_t sessKey[16];
  memcpy(sessKey, key, 16);

  if (v34) {
    // Session key negotiation.
    uint8_t localNonce[16];
    esp_fill_random(localNonce, 16);
    Bytes p = aesEnc(key, localNonce, 16);
    Bytes f = frame(seq++, SESS_KEY_NEG_START, p, true, key);
    c.write(f.data(), f.size());

    Msg m;
    do { if (!readMsg(c, true, m, err)) { c.stop(); return false; } } while (m.cmd != SESS_KEY_NEG_RESP);
    Bytes dec = aesDec(key, m.payload.data(), m.payload.size());
    if (dec.size() < 48) { err = "bad key negotiation reply (wrong local key?)"; c.stop(); return false; }
    uint8_t expect[32];
    hmac(key, localNonce, 16, expect);
    if (memcmp(expect, dec.data() + 16, 32) != 0) { err = "key negotiation HMAC mismatch (wrong local key?)"; c.stop(); return false; }
    uint8_t remoteNonce[16];
    memcpy(remoteNonce, dec.data(), 16);

    uint8_t rh[32];
    hmac(key, remoteNonce, 16, rh);
    Bytes p2 = aesEnc(key, rh, 32);
    Bytes f2 = frame(seq++, SESS_KEY_NEG_FINISH, p2, true, key);
    c.write(f2.data(), f2.size());

    uint8_t x[16];
    for (int i = 0; i < 16; i++) x[i] = localNonce[i] ^ remoteNonce[i];
    Bytes sk = aesEnc(key, x, 16, false);
    memcpy(sessKey, sk.data(), 16);

    String js = controlJson(devId, dp, on, true);
    Bytes plain = {'3', '.', '4'};
    plain.insert(plain.end(), 12, 0);
    plain.insert(plain.end(), js.begin(), js.end());
    Bytes enc = aesEnc(sessKey, plain.data(), plain.size());
    Bytes f3 = frame(seq++, CONTROL_NEW, enc, true, sessKey);
    c.write(f3.data(), f3.size());
  } else {
    String js = controlJson(devId, dp, on, false);
    Bytes enc = aesEnc(key, (const uint8_t*)js.c_str(), js.length());
    Bytes p = {'3', '.', '3'};
    p.insert(p.end(), 12, 0);
    p.insert(p.end(), enc.begin(), enc.end());
    Bytes f = frame(seq++, CONTROL, p, false, key);
    c.write(f.data(), f.size());
  }

  // Expect an ack for the control command (and usually a STATUS push).
  uint32_t ctrlCmd = v34 ? CONTROL_NEW : CONTROL;
  for (int i = 0; i < 4; i++) {
    Msg m;
    if (!readMsg(c, v34, m, err)) break;
    if (m.cmd == ctrlCmd || m.cmd == STATUS) {
      c.stop();
      if (m.hasRet && m.retcode != 0) { err = "device returned error " + String(m.retcode); return false; }
      err = "";
      return true;
    }
  }
  c.stop();
  if (err.isEmpty()) err = "no acknowledgement";
  return false;
}

}  // namespace tuya
