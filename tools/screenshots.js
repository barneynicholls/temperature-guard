// Regenerates docs/screenshots/*.png from the mock device, using headless Chrome
// over the DevTools protocol. No npm dependencies (needs Node 22+ for WebSocket).
//
//   node tools/screenshots.js
//   CHROME="/path/to/chrome" node tools/screenshots.js
//
// Starts tools/mock-server.js itself with a fixed clock, so the shots are repeatable.

const { spawn } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

const MOCK_NOW = process.env.MOCK_NOW || '2026-11-26T06:25:00';
const PORT = 8091, DBG = 9333;
const OUT = path.join(__dirname, '..', 'docs', 'screenshots');
const CHROME = process.env.CHROME || [
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  '/usr/bin/google-chrome', '/usr/bin/chromium', '/usr/bin/chromium-browser',
].find(p => fs.existsSync(p));

const SHOTS = [
  { file: 'status.png', hash: '', w: 1280, hover: 0.72 },
  { file: 'status-dark.png', hash: '', w: 1280, dark: true, hover: 0.72 },
  { file: 'logs.png', hash: '#logs', w: 1280, wait: 3500, js: "document.querySelectorAll('#days button')[1].click()" },
  { file: 'settings.png', hash: '#settings', w: 1280 },
  { file: 'mobile.png', hash: '', w: 390, h: 844, mobile: true, scale: 2 },
];

const sleep = ms => new Promise(r => setTimeout(r, ms));

async function main() {
  if (!CHROME) throw new Error('Chrome not found; set CHROME=/path/to/chrome');
  fs.mkdirSync(OUT, { recursive: true });
  const prof = fs.mkdtempSync(path.join(os.tmpdir(), 'tg-shots-'));
  const server = spawn(process.execPath, [path.join(__dirname, 'mock-server.js')], { env: { ...process.env, PORT, MOCK_NOW }, stdio: 'inherit' });
  const chrome = spawn(CHROME, ['--headless=new', '--remote-debugging-port=' + DBG, '--user-data-dir=' + prof, '--hide-scrollbars',
    '--no-first-run', '--no-default-browser-check', '--force-color-profile=srgb', 'about:blank'], { stdio: 'ignore' });
  try {
    let tabs;
    for (let i = 0; i < 50 && !tabs; i++) { await sleep(200); tabs = await fetch('http://127.0.0.1:' + DBG + '/json/list').then(r => r.json()).catch(() => null); }
    if (!tabs) throw new Error('Chrome did not open its DevTools port (' + DBG + '); is another instance using it?');
    const page = tabs.find(t => t.type === 'page');
    const ws = new WebSocket(page.webSocketDebuggerUrl);
    await new Promise((ok, bad) => { ws.onopen = ok; ws.onerror = bad; });
    let id = 0; const pending = new Map();
    ws.onmessage = e => { const m = JSON.parse(e.data); if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); } };
    const cdp = (method, params = {}) => new Promise((ok, bad) => { const i = ++id; pending.set(i, m => m.error ? bad(new Error(method + ': ' + m.error.message)) : ok(m.result)); ws.send(JSON.stringify({ id: i, method, params })); });
    const evalJs = async expr => (await cdp('Runtime.evaluate', { expression: expr, returnByValue: true })).result.value;

    await cdp('Page.enable');
    // Run the page's clock at the mock time so "last 24 h" and "next change" line up with the data.
    await cdp('Page.addScriptToEvaluateOnNewDocument', { source: `(()=>{const R=Date,off=new R(${JSON.stringify(MOCK_NOW)}).getTime()-R.now();
      function D(...a){if(!new.target)return new D().toString();return a.length?new R(...a):new R(R.now()+off)}
      D.prototype=R.prototype;D.now=()=>R.now()+off;D.parse=R.parse;D.UTC=R.UTC;globalThis.Date=D})()` });

    for (const s of SHOTS) {
      const scale = s.scale || 1.5, h = s.h || 900;
      await cdp('Emulation.setDeviceMetricsOverride', { width: s.w, height: h, deviceScaleFactor: scale, mobile: !!s.mobile });
      await cdp('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-color-scheme', value: s.dark ? 'dark' : 'light' }] });
      await cdp('Page.navigate', { url: 'about:blank' });  // a hash-only change would not reload the page
      await sleep(200);
      await cdp('Page.navigate', { url: 'http://localhost:' + PORT + '/' + s.hash });
      await sleep(s.wait || 2000);
      await evalJs('document.fonts.ready.then(()=>1)');
      if (s.js) { await evalJs(s.js); await sleep(1000); }
      if (!s.mobile) {
        const full = await evalJs('Math.ceil(document.documentElement.scrollHeight)');
        await cdp('Emulation.setDeviceMetricsOverride', { width: s.w, height: full, deviceScaleFactor: scale, mobile: false });
        await sleep(400);
      }
      if (s.hover) {
        const r = await evalJs(`(()=>{const b=document.querySelector('#chart svg').getBoundingClientRect();return {x:b.left+b.width*${s.hover},y:b.top+b.height*.5}})()`);
        await evalJs(`document.querySelector('#chart svg').dispatchEvent(new PointerEvent('pointermove',{clientX:${r.x},clientY:${r.y},bubbles:true}))`);
        await sleep(150);
      }
      const { data } = await cdp('Page.captureScreenshot', { format: 'png' });
      fs.writeFileSync(path.join(OUT, s.file), Buffer.from(data, 'base64'));
      console.log('wrote docs/screenshots/' + s.file);
    }
    ws.close();
  } finally {
    chrome.kill(); server.kill();
    await sleep(500);
    try { fs.rmSync(prof, { recursive: true, force: true }); } catch (e) {}
  }
}

main().catch(e => { console.error(e); process.exit(1); });
