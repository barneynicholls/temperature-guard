// Mock device for working on the web UI without hardware, and for README screenshots.
// Serves the real page from src/web_ui.h plus a fake JSON API with ~16 days of
// simulated readings (a cold studio, frost guard switching a heater at night).
//
//   node tools/mock-server.js            -> http://localhost:8080
//   PORT=9000 node tools/mock-server.js
//   MOCK_NOW=2026-11-26T05:48 node tools/mock-server.js   (pin the simulated clock)
//
// No dependencies. Plain HTTP only; nothing is written to disk.

const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = +process.env.PORT || 8080;
const SRC = fs.readFileSync(path.join(__dirname, '..', 'src', 'web_ui.h'), 'utf8');
const HTML = SRC.slice(SRC.indexOf('R"HTML(') + 7, SRC.lastIndexOf(')HTML"'));
const FW = (fs.readFileSync(path.join(__dirname, '..', 'src', 'main.cpp'), 'utf8').match(/FW_VERSION = "([^"]+)"/) || [])[1] || 'dev';

const NOW = process.env.MOCK_NOW ? new Date(process.env.MOCK_NOW).getTime() : Date.now();
const settings = {
  enabled: true, onTemp: 11, offTemp: 15, tempOffset: -0.4, nightStart: 22 * 60, nightEnd: 7 * 60, weekendAllDay: true,
  logIntervalS: 60, retentionDays: 90, heaterMode: 1, tuyaId: 'bf0123456789abcdefxyz1', tuyaKey: '********', tuyaIp: '192.168.1.50',
  tuyaVer: '3.4', tuyaDp: 1, hookOn: '', hookOff: '', ledLevel: 1, title: 'Studio', wifiSsid: 'HomeNetwork', wifiPass: '********',
  hostname: 'studio-temp', webPass: '********',
};

// ---------- simulation ----------
let seed = 7;
const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
const pad = n => String(n).padStart(2, '0');
const stamp = t => { const d = new Date(t); return d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate()) + ' ' + pad(d.getHours()) + ':' + pad(d.getMinutes()) + ':' + pad(d.getSeconds()); };
function inWin(t) {
  const d = new Date(t), m = d.getHours() * 60 + d.getMinutes(), s = settings.nightStart, e = settings.nightEnd;
  if (settings.weekendAllDay && (d.getDay() === 0 || d.getDay() === 6)) return true;
  return s > e ? (m >= s || m < e) : (m >= s && m < e);
}

const MIN = 60e3, DAYS = 16;
const start = Math.floor((NOW - DAYS * 864e5) / MIN) * MIN;
const samples = [], events = [];
(function simulate() {
  let T = 12, heater = false, dayMild = 0, lastDay = -1;
  events.push([start, 'Boot, firmware ' + FW], [start + 4e3, 'WiFi connected 192.168.1.42'], [start + 6e3, 'Time synced']);
  for (let t = start; t <= NOW; t += MIN) {
    const d = new Date(t), h = d.getHours() + d.getMinutes() / 60;
    if (d.getDate() !== lastDay) { lastDay = d.getDate(); dayMild = (rnd() - 0.5) * 5; }
    const outside = 6.5 + dayMild + 3.5 * Math.sin((h - 9) / 24 * 2 * Math.PI);
    const sun = Math.max(0, Math.sin((h - 8) / 9 * Math.PI)) * (6 + dayMild);
    T += 0.0045 * (outside + sun - T) + (heater ? 0.085 : 0) + (rnd() - 0.5) * 0.02;
    const win = inWin(t);
    let want = heater, why = '';
    if (!win) { want = false; why = 'outside schedule'; }
    else if (T <= settings.onTemp) { want = true; why = 'below on temp'; }
    else if (T >= settings.offTemp) { want = false; why = 'above off temp'; }
    if (want !== heater) { heater = want; events.push([t + 1500, 'Heater ' + (heater ? 'ON' : 'OFF') + ' (' + why + ')']); }
    const hum = 62 - (T - 12) * 2.2 + Math.sin(t / 5e6) * 4 + (rnd() - 0.5) * 1.5;
    samples.push({ t, temp: Math.round(T * 10) / 10, hum: Math.round(hum * 10) / 10, h: heater ? 1 : 0, w: win ? 1 : 0 });
  }
  events.push([NOW - 9 * 864e5 + 3 * 36e5, 'Settings saved']);
  events.push([NOW - 2 * 864e5 - 5 * 36e5, 'Boost 60 min'], [NOW - 2 * 864e5 - 4 * 36e5, 'Boost ended']);
  events.sort((a, b) => a[0] - b[0]);
})();
const cur = samples[samples.length - 1];

function dayCsv(name) {
  const d0 = new Date(name.slice(0, 10) + 'T00:00:00').getTime(), d1 = d0 + 864e5 + 2 * 36e5; // DST-safe upper bound
  const inDay = t => t >= d0 && t < d1 && stamp(t).startsWith(name.slice(0, 10));
  const rows = samples.filter(s => inDay(s.t)).map(s => [s.t, stamp(s.t) + ',' + s.temp.toFixed(1) + ',' + s.hum.toFixed(1) + ',' + s.h + ',' + s.w + ',']);
  events.filter(e => inDay(e[0])).forEach(e => rows.push([e[0], stamp(e[0]) + ',,,,,' + e[1]]));
  rows.sort((a, b) => a[0] - b[0]);
  return 'time,temp_c,humidity_pct,heater,window,event\n' + rows.map(r => r[1]).join('\n') + '\n';
}
const dayNames = [...new Set(samples.map(s => stamp(s.t).slice(0, 10)))].map(d => d + '.csv');

// ---------- API ----------
function status() {
  const last = events.filter(e => e[1].startsWith('Heater ')).pop();
  return {
    temp: cur.temp, hum: cur.hum, sensorAgeS: 3, sensorErrors: 2, timeValid: true, now: Math.floor(NOW / 1000),
    localTime: stamp(NOW), window: inWin(NOW), enabled: settings.enabled, heaterOn: !!cur.h, heaterKnown: true,
    heaterChangedAt: Math.floor(last[0] / 1000), heaterMode: settings.heaterMode,
    reason: !cur.w ? 'outside schedule' : cur.temp <= settings.onTemp ? 'below on temp' : cur.temp >= settings.offTemp ? 'above off temp' : 'hysteresis',
    wantOn: !!cur.h, lastCmd: 'OK: ' + (cur.h ? 'ON' : 'OFF'), lastCmdTime: Math.floor(NOW / 1000) - 140, boostLeftS: 0, testHoldS: 0,
    onTemp: settings.onTemp, offTemp: settings.offTemp, nightStart: settings.nightStart, nightEnd: settings.nightEnd,
    weekendAllDay: settings.weekendAllDay, retentionDays: settings.retentionDays, logIntervalS: settings.logIntervalS,
    hostname: settings.hostname, title: settings.title, webPassSet: true, sd: true, sdTotalMB: 15193, sdUsedMB: 2,
    rssi: -58, ip: '192.168.1.42', uptimeS: Math.floor((NOW - start) / 1000), heap: 182344, fw: FW,
    events: events.slice(-50).reverse().map(e => stamp(e[0]) + '  ' + e[1]),
  };
}

function send(res, code, type, body) {
  res.writeHead(code, { 'Content-Type': type, 'Cache-Control': 'no-store' });
  res.end(body);
}
const json = (res, o, code = 200) => send(res, code, 'application/json', JSON.stringify(o));

http.createServer((req, res) => {
  const u = new URL(req.url, 'http://x');
  let body = '';
  req.on('data', c => (body += c));
  req.on('end', () => {
    const p = u.pathname, m = req.method;
    if (p === '/' && m === 'GET') return send(res, 200, 'text/html; charset=utf-8', HTML);
    if (p === '/api/status') return json(res, status());
    if (p === '/api/history') return send(res, 200, 'text/csv', samples.filter(s => s.t > NOW - 864e5)
      .map(s => Math.floor(s.t / 1000) + ',' + s.temp.toFixed(1) + ',' + s.hum.toFixed(1) + ',' + s.h + ',' + s.w).join('\n') + '\n');
    if (p === '/api/settings' && m === 'GET') return json(res, settings);
    if (p === '/api/settings' && m === 'POST') {
      try { Object.assign(settings, JSON.parse(body)); } catch (e) { return json(res, { error: 'invalid JSON' }, 400); }
      return json(res, { ...settings, netChanged: false });
    }
    if (p === '/api/heater') return json(res, { ok: true, result: 'OK: ' + (u.searchParams.get('cmd') || '').toUpperCase() });
    if (p === '/api/boost') return json(res, { ok: true });
    if (p === '/api/logs') return json(res, { sd: true, files: dayNames.map(n => ({ name: n, size: Buffer.byteLength(dayCsv(n)) })) });
    if (p === '/api/log') {
      const f = u.searchParams.get('f');
      if (!dayNames.includes(f)) return json(res, { error: 'not found' }, 404);
      if (m === 'DELETE') return json(res, { ok: true });
      return send(res, 200, 'text/csv', dayCsv(f));
    }
    if (p === '/api/cert') return json(res, { sha256: '3F:A1:7C:02:9B:E4:55:D8:10:6E:C3:2A:F9:84:47:BD:0C:71:E2:5A:98:13:6F:D4:A0:3B:C7:59:E8:21:04:9D', pem: '' });
    if (p === '/api/reboot' || p === '/api/update') return json(res, { ok: true });
    send(res, 404, 'text/plain', 'Not found');
  });
}).listen(PORT, '127.0.0.1', () => console.log('Mock Temperature Guard on http://localhost:' + PORT + '  (clock ' + stamp(NOW) + ')'));
