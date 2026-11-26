#pragma once
#include <pgmspace.h>

// Single-file web UI (design from the Claude Design canvas). No external
// resources: system fonts, inline SVG, vanilla JS.
static const char WEB_UI[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Temperature Guard</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='%23C2410C' stroke-width='2.4' stroke-linecap='round'%3E%3Cpath d='M14 14.76V3.5a2.5 2.5 0 0 0-5 0v11.26a4.5 4.5 0 1 0 5 0z'/%3E%3C/svg%3E">
<script>try{var t=localStorage.getItem('st-theme');if(t)document.documentElement.dataset.theme=t}catch(e){}</script>
<style>
:root{color-scheme:light dark;--bg:#F5F4F0;--card:#FFFFFF;--ink:#16181B;--mute:#5B6068;--line:#E4E1DA;--soft:#F0EEE9;--field:#D5D1C8;--heat:#C2410C;--cool:#1E5AA8;--heatT:#FDEBDD;--coolT:#E6EEF8;--coolM:#A9C3E6;--band:#F7C9A8;--ok:#2F7D4A;--bad:#A61B1B;--warnB:#E8A33D;--warnT:#FFF8EC;--bar:#16181B;--barInk:#FFFFFF;--shadow:0 1px 2px rgba(0,0,0,.08)}
:root[data-theme=light]{color-scheme:light}
:root[data-theme=dark]{color-scheme:dark;--bg:#121315;--card:#1B1D20;--ink:#ECEDEE;--mute:#9DA3AA;--line:#2C2F34;--soft:#24272B;--field:#3A3E44;--heat:#F0874A;--cool:#7DAEF0;--heatT:#3A2318;--coolT:#18263A;--coolM:#34557F;--band:#5C3220;--ok:#5CC184;--bad:#F27878;--warnB:#C98A2A;--warnT:#2B2415;--bar:#ECEDEE;--barInk:#16181B;--shadow:0 1px 2px rgba(0,0,0,.5)}
@media (prefers-color-scheme:dark){:root:not([data-theme=light]){--bg:#121315;--card:#1B1D20;--ink:#ECEDEE;--mute:#9DA3AA;--line:#2C2F34;--soft:#24272B;--field:#3A3E44;--heat:#F0874A;--cool:#7DAEF0;--heatT:#3A2318;--coolT:#18263A;--coolM:#34557F;--band:#5C3220;--ok:#5CC184;--bad:#F27878;--warnB:#C98A2A;--warnT:#2B2415;--bar:#ECEDEE;--barInk:#16181B;--shadow:0 1px 2px rgba(0,0,0,.5)}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;-webkit-text-size-adjust:100%}
a{color:var(--cool)}
button,input,select{font:inherit;color:inherit}
.num{font-variant-numeric:tabular-nums}
.hide{display:none!important}
header{background:var(--card);border-bottom:1px solid var(--line)}
.hd{max-width:1120px;margin:0 auto;padding:12px 24px;display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:12px}
.brand{display:flex;align-items:center;gap:10px;font-weight:700;font-size:17px;letter-spacing:-.01em}
nav{display:flex;gap:4px;background:var(--bg);padding:4px;border-radius:10px}
nav button{border:0;background:none;padding:8px 16px;border-radius:7px;color:var(--mute);cursor:pointer;min-height:40px}
nav button[aria-current]{background:var(--card);color:var(--ink);font-weight:600;box-shadow:var(--shadow)}
.conn{display:flex;align-items:center;gap:8px;font-size:13px;color:var(--mute)}
.dot{width:8px;height:8px;border-radius:50%;background:var(--mute);flex:none}
main{max-width:1120px;margin:0 auto;padding:28px 24px 48px;display:flex;flex-direction:column;gap:20px}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:24px;display:flex;flex-direction:column;gap:14px;min-width:0}
.cols{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px}
.lbl{font-size:12px;font-weight:600;letter-spacing:.08em;text-transform:uppercase;color:var(--mute)}
h2{margin:0;font-size:18px;font-weight:650}
.sub{font-size:13px;color:var(--mute)}
.big{font-size:72px;font-weight:650;letter-spacing:-.03em;line-height:1}
.mid{font-size:28px;font-weight:650;letter-spacing:-.02em;line-height:1.2}
.scale{position:relative;height:10px;border-radius:5px;background:linear-gradient(90deg,var(--cool) 0%,var(--coolT) 30%,var(--soft) 50%,var(--heatT) 70%,var(--heat) 100%);margin-top:4px}
.tick{position:absolute;top:-4px;width:2px;height:18px;margin-left:-1px}
.knob{position:absolute;top:-5px;width:20px;height:20px;margin-left:-10px;border-radius:50%;background:var(--ink);border:3px solid var(--card);box-shadow:0 1px 3px rgba(0,0,0,.3);transition:left .6s}
.scl{position:relative;height:18px;font-size:12px;color:var(--mute)}
.scl span{position:absolute;white-space:nowrap}
.hicon{width:56px;height:56px;border-radius:14px;background:var(--soft);display:flex;align-items:center;justify-content:center;color:var(--mute);flex:none}
.hicon.on{background:var(--heatT);color:var(--heat)}
.split{border-top:1px solid var(--line);padding-top:14px;display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px;font-size:14px}
.split b{display:block;font-weight:600}
.k{color:var(--mute);font-size:12px}
.week{display:grid;grid-template-columns:repeat(7,minmax(0,1fr));gap:6px;font-size:12px;text-align:center;color:var(--mute)}
.week div{display:flex;flex-direction:column;gap:4px}
.week i{height:32px;border-radius:6px;background:var(--soft)}
.week .today span{color:var(--ink);font-weight:700}
.week .today i{outline:2px solid var(--ink);outline-offset:1px}
.row{display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:12px}
.legend{display:flex;flex-wrap:wrap;gap:16px;font-size:13px;color:var(--mute)}
.legend span{display:flex;align-items:center;gap:6px}
.sw{width:12px;height:12px;border-radius:3px;display:inline-block}
svg.chart{width:100%;height:auto;display:block;touch-action:pan-y}
.chart .gl{stroke:var(--line);stroke-width:1}
.chart .tl{fill:var(--mute);font-size:12px}
.chart .bw{fill:var(--coolT)}.chart .bh{fill:var(--band)}
.chart .on{stroke:var(--cool);stroke-width:1.5;stroke-dasharray:5 5}
.chart .off{stroke:var(--heat);stroke-width:1.5;stroke-dasharray:5 5}
.chart .ln{fill:none;stroke:var(--ink);stroke-width:2.5;stroke-linejoin:round;stroke-linecap:round}
.chart .end{fill:var(--ink);stroke:var(--card);stroke-width:2}
.chart .cur{stroke:var(--mute);stroke-width:1}
.chart .empty{fill:var(--mute);font-size:15px}
.readout{font-size:13px;color:var(--mute);min-height:19px}
.btn{min-height:44px;padding:0 16px;border-radius:10px;border:1px solid var(--line);background:var(--card);font-weight:600;cursor:pointer;display:inline-flex;align-items:center;justify-content:center;gap:8px;text-decoration:none;color:var(--ink)}
.btn:hover{border-color:var(--mute)}
.btn.pri{background:var(--heat);border-color:var(--heat);color:#fff}
.btn.danger{color:var(--bad)}
.btn:disabled{opacity:.5;cursor:default}
.boost{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px}
.boost .btn{min-height:48px}
.boostOn{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;background:var(--heatT);border-radius:12px;padding:10px 10px 10px 16px;font-weight:600;color:var(--heat)}
ol.ev{list-style:none;margin:0;padding:0;font-size:14px;max-height:360px;overflow:auto}
ol.ev li{display:grid;grid-template-columns:76px 12px minmax(0,1fr);gap:10px;align-items:center;padding:9px 0;border-bottom:1px solid var(--soft)}
ol.ev li:last-child{border-bottom:0}
ol.ev .t{color:var(--mute);font-variant-numeric:tabular-nums;white-space:nowrap}
footer.sys{font-size:12px;color:var(--mute);display:flex;flex-wrap:wrap;gap:6px 18px}
.logs{display:flex;flex-wrap:wrap;gap:20px;align-items:flex-start}
.logs aside{flex:1 1 300px;padding:20px;gap:12px}
.logs section{flex:999 1 560px;min-width:0;display:flex;flex-direction:column;gap:20px}
.meter{height:6px;border-radius:3px;background:var(--soft);overflow:hidden}
.meter div{height:100%;background:var(--mute)}
ul.days{list-style:none;margin:4px 0 0;padding:0;display:flex;flex-direction:column;gap:4px;max-height:640px;overflow:auto}
ul.days button{width:100%;text-align:left;border:0;background:none;display:grid;grid-template-columns:minmax(0,1fr) auto;gap:2px 10px;align-items:center;padding:10px 12px;border-radius:10px;cursor:pointer}
ul.days button:hover{background:var(--soft)}
ul.days button[aria-current]{background:var(--heatT)}
ul.days b{font-weight:600}
ul.days small{font-size:12px;color:var(--mute)}
.stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:12px}
.stat{background:var(--bg);border-radius:12px;padding:14px}
.stat .v{font-size:24px;font-weight:650;font-variant-numeric:tabular-nums}
.set{max-width:760px;width:100%;margin:0 auto;padding-bottom:90px}
.set .card{gap:20px}
.desc{margin:2px 0 0;font-size:14px;color:var(--mute)}
.g3{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:16px}
.g2{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:16px}
label.f{display:flex;flex-direction:column;gap:6px;font-size:14px;font-weight:600;min-width:0}
.in,.unit{border:1px solid var(--field);border-radius:10px;background:var(--card);min-height:46px}
.in{padding:11px 12px;width:100%;min-width:0}
.unit{display:flex;align-items:center;overflow:hidden}
.unit input{flex:1;min-width:0;border:0;background:none;padding:11px 12px;outline:0}
.unit span{padding:0 12px;color:var(--mute);font-weight:400}
.unit:focus-within,.in:focus{outline:2px solid var(--cool);outline-offset:1px}
.unit.v input{font-size:20px;font-weight:650;font-variant-numeric:tabular-nums}
.unit.cold input{color:var(--cool)}.unit.hot input{color:var(--heat)}
.mono{font:13px ui-monospace,Consolas,monospace!important}
.check{display:flex;align-items:center;gap:12px;cursor:pointer;font-weight:600}
.check input{width:22px;height:22px;flex:none;margin:0}
.wk{padding:14px;border-radius:12px;background:var(--coolT)}
.wk input{accent-color:var(--cool)}
#en{accent-color:var(--heat)}
.methods{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:8px}
.methods label{display:flex;gap:10px;align-items:flex-start;padding:12px;border:1px solid var(--line);border-radius:12px;cursor:pointer}
.methods label:has(input:checked){border:2px solid var(--heat);background:var(--heatT);padding:11px}
.methods input{margin-top:3px;accent-color:var(--heat)}
.methods b{display:block;font-weight:600}
.methods small{font-size:13px;color:var(--mute)}
fieldset{border:0;margin:0;padding:0;min-width:0}
legend{font-size:14px;font-weight:600;margin-bottom:8px;padding:0}
.warn .in{border-color:var(--warnB);background:var(--warnT)}
.res{font-size:14px;font-weight:600}
.res.ok{color:var(--ok)}.res.bad{color:var(--bad)}
.sep{display:flex;flex-wrap:wrap;gap:10px;padding-top:16px;border-top:1px solid var(--soft);align-items:center}
.savebar{position:fixed;left:50%;bottom:16px;transform:translateX(-50%);width:min(712px,calc(100% - 32px));display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;background:var(--bar);color:var(--barInk);border-radius:14px;padding:10px 10px 10px 20px;box-shadow:0 8px 24px rgba(0,0,0,.22);z-index:5}
.savebar .btn{border-color:transparent}
.savebar .ghost{background:transparent;color:var(--barInk);border:1px solid rgba(128,128,128,.5);font-weight:400}
.savebar .go{background:#E8590C;color:#16181B;border:0;font-weight:700;padding:0 20px}
#toast{position:fixed;left:50%;bottom:88px;transform:translateX(-50%) translateY(10px);background:var(--bar);color:var(--barInk);padding:10px 16px;border-radius:10px;opacity:0;pointer-events:none;transition:.25s;z-index:6;max-width:calc(100% - 32px);font-size:14px}
#toast.show{opacity:1;transform:translateX(-50%)}
@media (max-width:640px){.hd{padding:10px 16px}main{padding:20px 16px 40px}.card{padding:18px}.big{font-size:60px}nav{order:3;width:100%}nav button{flex:1}.cols{grid-template-columns:minmax(0,1fr)}ol.ev li{grid-template-columns:64px 10px minmax(0,1fr);gap:8px}}
</style></head><body>
<header><div class="hd">
 <div class="brand"><svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="var(--heat)" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M14 14.76V3.5a2.5 2.5 0 0 0-5 0v11.26a4.5 4.5 0 1 0 5 0z"/></svg><span id="brand">Temperature Guard</span></div>
 <nav aria-label="Sections"><button data-t="status" aria-current="page">Status</button><button data-t="logs">Logs</button><button data-t="settings">Settings</button></nav>
 <div class="conn" aria-live="polite"><span class="dot" id="cdot"></span><span id="ctext">Connecting…</span></div>
</div></header>

<main id="v-status">
 <section class="cols" aria-label="Current conditions">
  <div class="card" style="gap:18px">
   <div class="lbl">Room now</div>
   <div style="display:flex;align-items:baseline;gap:16px;flex-wrap:wrap"><span class="big num" id="temp">–</span><span class="num" style="font-size:20px;color:var(--mute)" id="hum"></span></div>
   <div><div class="scale"><span class="tick" id="tkOn" style="background:var(--cool)"></span><span class="tick" id="tkOff" style="background:var(--heat)"></span><span class="knob" id="knob" style="left:50%"></span></div>
    <div class="scl num" style="margin-top:8px"><span id="slo" style="left:0"></span><span id="son" style="color:var(--cool);font-weight:600;transform:translateX(-50%)"></span><span id="soff" style="color:var(--heat);font-weight:600;transform:translateX(-50%)"></span><span id="shi" style="right:0"></span></div></div>
   <div class="sub" id="sensor"></div>
  </div>
  <div class="card">
   <div class="lbl">Heater</div>
   <div style="display:flex;align-items:center;gap:14px"><span class="hicon" id="hicon"><svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 2v10"/><path d="M18.4 6.6a9 9 0 1 1-12.77.04"/></svg></span>
    <div><div class="mid" id="hstate">–</div><div style="font-size:14px;color:var(--mute)" id="hwhy"></div></div></div>
   <div class="split"><div><span class="k">Heater ran · 24 h</span><b class="num" id="ran">–</b></div><div><span class="k">Low · 24 h</span><b class="num" id="low">–</b></div></div>
   <div class="sub" id="via"></div>
  </div>
  <div class="card">
   <div class="lbl">Schedule</div>
   <div><div class="mid" id="snext">–</div><div style="font-size:14px;color:var(--mute)" id="sdesc"></div></div>
   <div class="week" id="week"></div>
   <div class="sub">Blue = heating allowed (UK time)</div>
  </div>
 </section>

 <section class="card" aria-label="Last 24 hours" style="gap:12px">
  <div class="row"><h2>Last 24 hours</h2>
   <div class="legend"><span><span class="sw" style="height:3px;width:14px;background:var(--ink)"></span>Temperature</span><span><span class="sw" style="background:var(--band)"></span>Heater on</span><span><span class="sw" style="background:var(--coolT)"></span>Schedule</span></div></div>
  <div id="chart"></div>
  <div class="readout num" id="rd">Hover or tap the chart for readings</div>
 </section>

 <section class="cols" style="grid-template-columns:repeat(auto-fit,minmax(320px,1fr))">
  <div class="card">
   <h2>Boost</h2>
   <p class="desc" style="margin:0">Run the heater now, whatever the schedule. Stops early at <span id="boff">15</span>°.</p>
   <div class="boostOn hide" id="bon"><span id="bleft"></span><button class="btn" type="button" onclick="boost(0)">Cancel boost</button></div>
   <div class="boost"><button class="btn" type="button" onclick="boost(30)">30 min</button><button class="btn pri" type="button" onclick="boost(60)">1 hour</button><button class="btn" type="button" onclick="boost(120)">2 hours</button></div>
  </div>
  <div class="card" style="gap:6px"><h2>Activity</h2><ol class="ev" id="events"></ol></div>
 </section>
 <footer class="sys num" id="sys"></footer>
</main>

<main id="v-logs" class="hide"><div class="logs">
 <aside class="card" aria-label="Log files">
  <div class="row" style="align-items:baseline"><h2>Daily logs</h2><span class="sub" id="keep"></span></div>
  <div class="meter"><div id="sdbar" style="width:0"></div></div>
  <div class="sub" id="sdtxt"></div>
  <ul class="days" id="days"></ul>
 </aside>
 <section aria-label="Selected day">
  <div class="card" style="gap:18px">
   <div class="row"><div><h2 style="font-size:22px;letter-spacing:-.01em" id="dtitle">Choose a day</h2><div style="font-size:14px;color:var(--mute)" id="dfile"></div></div>
    <div style="display:flex;gap:10px"><a class="btn" id="ddl" href="#"><svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 3v12"/><path d="m7 10 5 5 5-5"/><path d="M5 21h14"/></svg>Download CSV</a>
     <button class="btn danger" type="button" id="ddel" aria-label="Delete this log" style="padding:0;width:44px"><svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 6h18"/><path d="M8 6V4h8v2"/><path d="M6 6l1 14h10l1-14"/></svg></button></div></div>
   <div class="stats" id="dstats"></div>
   <div id="dchart"></div>
   <div class="readout num" id="drd"></div>
  </div>
  <div class="card" style="gap:6px"><h2>Events this day</h2><ol class="ev" id="devents" style="max-height:none"></ol></div>
 </section>
</div></main>

<main id="v-settings" class="hide"><form class="set" id="sf" autocomplete="off" style="display:flex;flex-direction:column;gap:20px" onsubmit="save(event)">
 <section class="card" aria-labelledby="h0">
  <div><h2 id="h0">Name</h2><p class="desc">What this guard is called, and its address on your network.</p></div>
  <div class="g2">
   <label class="f">Title<input class="in" name="title" required maxlength="40" placeholder="e.g. Studio, Garage, Greenhouse"><span class="sub" style="font-weight:400">Shown at the top of every page.</span></label>
   <label class="f">Web address<span class="unit"><span style="padding:0 0 0 12px">https://</span><input name="hostname" required maxlength="32" pattern="[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?" spellcheck="false" style="padding-left:2px"><span>.local</span></span><span class="sub" style="font-weight:400" id="hostnote">Letters, digits and hyphens.</span></label>
  </div>
 </section>
 <section class="card" aria-labelledby="h1">
  <div class="row" style="flex-wrap:nowrap;align-items:flex-start"><div><h2 id="h1">Frost guard</h2><p class="desc">Switch the heater automatically when it gets cold.</p></div>
   <label class="check"><input type="checkbox" name="enabled" id="en">Enabled</label></div>
  <div class="g3">
   <label class="f">Heater on at or below<span class="unit v cold"><input type="number" name="onTemp" step="0.5" min="-10" max="40" required><span>°C</span></span></label>
   <label class="f">Heater off at or above<span class="unit v hot"><input type="number" name="offTemp" step="0.5" min="-10" max="40" required><span>°C</span></span></label>
   <label class="f">Sensor correction<span class="unit v"><input type="number" name="tempOffset" step="0.1" min="-10" max="10"><span>°C</span></span></label>
  </div>
 </section>
 <section class="card" aria-labelledby="h2">
  <div><h2 id="h2">When heating is allowed</h2><p class="desc">UK time, adjusts for BST automatically.</p></div>
  <div class="g3"><label class="f">Every night from<input class="in" type="time" name="nightStart" required></label><label class="f">until<input class="in" type="time" name="nightEnd" required></label></div>
  <label class="check wk"><input type="checkbox" name="weekendAllDay"><span>All day Saturday and Sunday<br><span style="font-size:13px;font-weight:400;color:var(--mute)">Friday night runs straight through to Monday morning</span></span></label>
 </section>
 <section class="card" aria-labelledby="h3">
  <div><h2 id="h3">Heater plug</h2><p class="desc">How the device switches the fan heater.</p></div>
  <fieldset><legend>Method</legend><div class="methods">
   <label><input type="radio" name="heaterMode" value="0"><span><b>Monitor only</b><small>Log, don't switch</small></span></label>
   <label><input type="radio" name="heaterMode" value="2"><span><b>Alexa routine</b><small>Voice Monkey webhooks</small></span></label>
   <label><input type="radio" name="heaterMode" value="1"><span><b>Smart Life local</b><small>Tuya LAN protocol</small></span></label>
  </div></fieldset>
  <div id="m2" style="display:flex;flex-direction:column;gap:16px">
   <label class="f">Turn-on URL<input class="in mono" type="password" name="hookOn" spellcheck="false" placeholder="https://api-v3.voicemonkey.io/trigger?token=…&amp;device=heater-on"></label>
   <label class="f">Turn-off URL<input class="in mono" type="password" name="hookOff" spellcheck="false" placeholder="https://api-v3.voicemonkey.io/trigger?token=…&amp;device=heater-off"></label>
  </div>
  <div id="m1" class="g2">
   <label class="f">Plug IP address<input class="in" name="tuyaIp" placeholder="192.168.1.x" spellcheck="false"></label>
   <label class="f">Device ID<input class="in mono" name="tuyaId" spellcheck="false"></label>
   <label class="f">Local key<input class="in mono" type="password" name="tuyaKey"></label>
   <div class="g3" style="grid-template-columns:1fr 1fr"><label class="f">Protocol<select class="in" name="tuyaVer"><option>3.3</option><option>3.4</option></select></label><label class="f">Switch DP<input class="in" type="number" name="tuyaDp" min="1" max="255"></label></div>
  </div>
  <div id="tst" style="display:flex;flex-wrap:wrap;align-items:center;gap:10px">
   <button class="btn" type="button" onclick="test('on')">Test on</button><button class="btn" type="button" onclick="test('off')">Test off</button><span class="res" id="tres" aria-live="polite"></span></div>
 </section>
 <section class="card" aria-labelledby="h4">
  <h2 id="h4">Logging</h2>
  <div class="g3">
   <label class="f">Record every<select class="in" name="logIntervalS"><option value="10">10 seconds</option><option value="30">30 seconds</option><option value="60">1 minute</option><option value="120">2 minutes</option><option value="300">5 minutes</option><option value="600">10 minutes</option></select></label>
   <label class="f">Keep logs for<span class="unit"><input type="number" name="retentionDays" min="1" max="3650" required><span>days</span></span></label>
  </div>
 </section>
 <section class="card" aria-labelledby="h6">
  <div><h2 id="h6">Status light</h2><p class="desc">The board's LED stays on in a colour showing what the guard is doing, and pulses brighter with each reading.</p></div>
  <div class="g3"><label class="f">Brightness<select class="in" name="ledLevel"><option value="0">Off</option><option value="1">Low</option><option value="2">Medium</option><option value="3">High</option></select></label></div>
  <div class="legend" style="gap:8px 18px">
   <span><span class="sw" style="border-radius:50%;background:#FF3700"></span>Heater on</span><span><span class="sw" style="border-radius:50%;background:#00C81E"></span>Heating allowed</span><span><span class="sw" style="border-radius:50%;background:#0046FF"></span>Outside schedule</span><span><span class="sw" style="border-radius:50%;background:#E6E0D8;outline:1px solid var(--field)"></span>Turned off</span>
   <span><span class="sw" style="border-radius:50%;background:#E8A000"></span>Waiting for clock</span><span><span class="sw" style="border-radius:50%;background:#9600FF"></span>No WiFi</span><span><span class="sw" style="border-radius:50%;background:#E00000"></span>Fault</span></div>
 </section>
 <section class="card" aria-labelledby="h7">
  <div><h2 id="h7">Appearance</h2><p class="desc">Colour theme for this page. It applies to everyone who opens it.</p></div>
  <div class="g3"><label class="f">Theme<select class="in" name="theme"><option value="0">Match device</option><option value="1">Light</option><option value="2">Dark</option></select></label></div>
 </section>
 <section class="card" aria-labelledby="h5">
  <h2 id="h5">Network &amp; security</h2>
  <div class="g2">
   <label class="f">WiFi network<input class="in" name="wifiSsid" required spellcheck="false"></label>
   <label class="f">WiFi password<input class="in" type="password" name="wifiPass"></label>
   <label class="f" id="wpw">Web password<input class="in" type="password" name="webPass" maxlength="63" placeholder="Not set: anyone on the WiFi can change settings"><span class="sub" style="font-weight:400">Sign in as <b>admin</b>. Leave empty for no login. With 8 or more characters it is also the password for the TempGuard-Setup WiFi.</span></label>
  </div>
  <div class="sep"><button class="btn" type="button" onclick="$('#fw').click()">Update firmware…</button><input type="file" id="fw" accept=".bin" class="hide"><button class="btn" type="button" onclick="reboot()">Restart device</button><span class="res" id="fwres" aria-live="polite"></span></div>
  <div class="sub" style="overflow-wrap:anywhere">Certificate fingerprint (SHA-256): <span class="mono" id="fp">…</span></div>
 </section>
 <div class="savebar hide" id="bar"><span id="dirty" style="font-size:14px"></span><div style="display:flex;gap:8px"><button class="btn ghost" type="button" onclick="discard()">Discard</button><button class="btn go" type="submit">Save settings</button></div></div>
</form></main>
<div id="toast" role="status"></div>

<script>
const $=s=>document.querySelector(s),$$=s=>[...document.querySelectorAll(s)];
const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const pad=n=>String(n).padStart(2,'0'),hm=m=>pad(Math.floor(m/60))+':'+pad(m%60),clk=d=>pad(d.getHours())+':'+pad(d.getMinutes());
const DAY=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'],DAYL=['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'],MON=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'],MONL=['January','February','March','April','May','June','July','August','September','October','November','December'];
const dkey=d=>d.getFullYear()+'-'+pad(d.getMonth()+1)+'-'+pad(d.getDate());
const dur=min=>{min=Math.round(min);if(min<60)return min+' min';const h=Math.floor(min/60),m=min%60;return h+' h'+(m?' '+pad(m)+' m':'')};
const ago=s=>s<60?s+' s':s<3600?Math.round(s/60)+' min':s<86400?(s/3600).toFixed(1).replace('.0','')+' h':Math.floor(s/86400)+' d '+Math.floor(s%86400/3600)+' h';
const f1=v=>v==null||isNaN(v)?'–':(+v).toFixed(1)+'°';
let S=null,hist=[],tab='status';
// Theme: 0 follows the OS setting, 1 light, 2 dark. Cached so the head script applies it before first paint.
function theme(v){const t=['','light','dark'][v]||'',r=document.documentElement;if(t)r.dataset.theme=t;else delete r.dataset.theme;try{t?localStorage.setItem('st-theme',t):localStorage.removeItem('st-theme')}catch(e){}}
function brand(t){if(!t)return;$('#brand').textContent=t;document.title=t}
function toast(t){const m=$('#toast');m.textContent=t;m.classList.add('show');clearTimeout(m._t);m._t=setTimeout(()=>m.classList.remove('show'),3500)}
async function j(u,o){const r=await fetch(u,o);const d=await r.json().catch(()=>({}));if(!r.ok)throw new Error(d.error||r.status+' '+r.statusText);return d}
const ls={get(k){try{return JSON.parse(localStorage.getItem(k))}catch(e){return null}},set(k,v){try{localStorage.setItem(k,JSON.stringify(v))}catch(e){}}};

// ---------- tabs ----------
function show(t){if(tab=='settings'&&t!='settings'&&dirtyN())toast('Unsaved settings are kept until you save or discard');tab=t;
 $$('nav button').forEach(b=>b.dataset.t==t?b.setAttribute('aria-current','page'):b.removeAttribute('aria-current'));
 ['status','logs','settings'].forEach(id=>$('#v-'+id).classList.toggle('hide',id!=t));
 if(history.replaceState)history.replaceState(null,'',t=='status'?location.pathname:'#'+t);
 if(t=='logs')loadLogs();if(t=='settings'&&!dirtyN())loadSettings();if(t=='status')drawMain()}
$$('nav button').forEach(b=>b.onclick=()=>show(b.dataset.t));

// ---------- schedule ----------
function inWin(d,c){if(c.weekendAllDay&&(d.getDay()==0||d.getDay()==6))return true;const m=d.getHours()*60+d.getMinutes(),s=c.nightStart,e=c.nightEnd;if(s==e)return false;return s>e?(m>=s||m<e):(m>=s&&m<e)}
function nextChange(now,c){const cur=inWin(now,c),d=new Date(now);d.setSeconds(0,0);for(let i=0;i<8*96;i++){d.setMinutes(d.getMinutes()+15-d.getMinutes()%15);if(inWin(d,c)!=cur){for(let k=0;k<15;k++){const p=new Date(d-60000);if(inWin(p,c)!=cur)d.setTime(p);else break}return d}}return null}
function when(d,now){const t=clk(d);if(dkey(d)==dkey(now))return t;const tm=new Date(now);tm.setDate(tm.getDate()+1);if(dkey(d)==dkey(tm))return 'tomorrow '+t;return DAY[d.getDay()]+' '+t}
function weekStrip(now,c){const mon=new Date(now);mon.setHours(0,0,0,0);mon.setDate(mon.getDate()-((mon.getDay()+6)%7));let h='';
 for(let i=0;i<7;i++){const d=new Date(mon);d.setDate(mon.getDate()+i);let st=[],prev=null,start=0;
  for(let s=0;s<=96;s++){const v=s<96?inWin(new Date(d.getFullYear(),d.getMonth(),d.getDate(),0,s*15),c):null;if(v!==prev){if(prev!==null)st.push((prev?'var(--coolM)':'var(--soft)')+' '+(start/96*100).toFixed(1)+'% '+(s/96*100).toFixed(1)+'%');prev=v;start=s}}
  const today=dkey(d)==dkey(now);h+='<div class="'+(today?'today':'')+'" title="'+DAYL[d.getDay()]+'"><span>'+DAYL[d.getDay()][0]+'</span><i style="background:linear-gradient(180deg,'+st.join(',')+')"></i></div>'}
 $('#week').innerHTML=h}
function schedText(c){const n=c.nightStart==c.nightEnd?'':'Nightly '+hm(c.nightStart)+'–'+hm(c.nightEnd);return [n,c.weekendAllDay?'all day at weekends':''].filter(Boolean).join(' · ')||'No heating window set'}

// ---------- chart ----------
const GAP=15*60e3;
function chart(el,pts,t0,t1,o){o=o||{};const W=960,H=220,on=o.on,off=o.off;const vis=pts.filter(p=>p.t>=t0&&p.t<=t1&&!isNaN(p.temp));
 const tv=vis.map(p=>p.temp);let lo=Math.min(on??99,...tv),hi=Math.max(off??-99,...tv);if(!isFinite(lo)||!isFinite(hi)){lo=8;hi=20}
 lo=Math.floor(lo-1);hi=Math.ceil(hi+1);let step=[1,2,5,10].find(s=>(hi-lo)/s<=6)||10;lo=Math.floor(lo/step)*step;hi=Math.ceil(hi/step)*step;if(hi==lo)hi=lo+step;
 const X=t=>(t-t0)/(t1-t0)*W,Y=v=>(hi-v)/(hi-lo)*H;let s='';
 const band=(k,cl)=>{let a=null;for(let i=0;i<vis.length;i++){const p=vis[i],n=vis[i+1];if(p[k]&&a==null)a=p.t;const endHere=!n||!n[k]||n.t-p.t>GAP;if(a!=null&&p[k]&&endHere){const e=n&&n.t-p.t<=GAP?n.t:p.t+60e3;s+='<rect class="'+cl+'" x="'+X(a).toFixed(1)+'" y="0" width="'+Math.max(1,X(e)-X(a)).toFixed(1)+'" height="'+H+'"/>';a=null}}};
 band('w','bw');band('h','bh');
 for(let v=lo;v<=hi;v+=step){const y=Y(v).toFixed(1);s+='<line class="gl" x1="0" x2="'+W+'" y1="'+y+'" y2="'+y+'"/><text class="tl" x="-8" y="'+(+y+4)+'" text-anchor="end">'+v+'°</text>'}
 if(on!=null)s+='<line class="on" x1="0" x2="'+W+'" y1="'+Y(on).toFixed(1)+'" y2="'+Y(on).toFixed(1)+'"/>';
 if(off!=null)s+='<line class="off" x1="0" x2="'+W+'" y1="'+Y(off).toFixed(1)+'" y2="'+Y(off).toFixed(1)+'"/>';
 let d='',prev=null;vis.forEach(p=>{d+=(prev&&p.t-prev<=GAP?'L':'M')+X(p.t).toFixed(1)+','+Y(p.temp).toFixed(1);prev=p.t});
 if(d)s+='<path class="ln" d="'+d+'"/>';
 if(o.live&&vis.length){const p=vis[vis.length-1];s+='<circle class="end" cx="'+X(p.t).toFixed(1)+'" cy="'+Y(p.temp).toFixed(1)+'" r="5"/>'}
 if(!vis.length)s+='<text class="empty" x="'+W/2+'" y="'+H/2+'" text-anchor="middle">No readings yet</text>';
 const every=o.labelH||4,first=new Date(t0);first.setMinutes(0,0,0);if(+first<t0)first.setHours(first.getHours()+1);while(first.getHours()%every)first.setHours(first.getHours()+1);
 if(!o.live)s+='<text class="tl" x="0" y="242">'+clk(new Date(t0))+'</text>';
 for(let t=new Date(first);+t<t1-30*60e3;t.setHours(t.getHours()+every)){if(+t-t0<30*60e3)continue;s+='<text class="tl" x="'+X(+t).toFixed(0)+'" y="242" text-anchor="middle">'+clk(t)+'</text>'}
 s+='<text class="tl" x="'+W+'" y="242" text-anchor="end">'+(o.live?'now':'24:00')+'</text>';
 s+='<line class="cur hide" x1="0" x2="0" y1="0" y2="'+H+'"/><circle class="end hide" r="4"/>';
 el.innerHTML='<svg class="chart" viewBox="-40 -10 1010 262" role="img" aria-label="'+esc(o.label||'Temperature chart')+'">'+s+'</svg>';
 const svg=el.firstChild,cur=svg.querySelector('.cur'),dot=svg.querySelector('circle.end.hide')||svg.lastChild,rd=o.readout;
 const mv=e=>{if(!vis.length||!rd)return;const r=svg.getBoundingClientRect(),x=(e.clientX-r.left)/r.width*1010-40,t=t0+x/W*(t1-t0);let b=vis[0];for(const p of vis)if(Math.abs(p.t-t)<Math.abs(b.t-t))b=p;
  cur.setAttribute('x1',X(b.t));cur.setAttribute('x2',X(b.t));cur.classList.remove('hide');dot.setAttribute('cx',X(b.t));dot.setAttribute('cy',Y(b.temp));dot.classList.remove('hide');
  rd.textContent=clk(new Date(b.t))+' · '+b.temp.toFixed(1)+'° · '+Math.round(b.hum)+'% RH'+(b.h?' · heater on':'')+(b.w?'':' · outside schedule')};
 svg.addEventListener('pointermove',mv);svg.addEventListener('pointerdown',mv);
 svg.addEventListener('pointerleave',()=>{cur.classList.add('hide');dot.classList.add('hide');if(rd)rd.textContent=o.hint||''})}

function summary(pts){let lo=null,hi=null,hl=1e9,hh=-1e9,run=0,cyc=0;for(let i=0;i<pts.length;i++){const p=pts[i],n=pts[i+1];if(isNaN(p.temp))continue;
  if(!lo||p.temp<lo.temp)lo=p;if(!hi||p.temp>hi.temp)hi=p;hl=Math.min(hl,p.hum);hh=Math.max(hh,p.hum);
  if(p.h){if(n&&n.t-p.t<=GAP)run+=n.t-p.t;else run+=60e3;if(!i||!pts[i-1].h)cyc++}}
 return lo?{lo:lo.temp,loT:lo.t,hi:hi.temp,hiT:hi.t,run:run/60e3,cyc,hl,hh,n:pts.length}:{n:0,run:0,cyc:0}}

// ---------- status ----------
const why={'below on temp':s=>'Below '+s.onTemp+'°, heating to '+s.offTemp+'°','above off temp':s=>'Warm enough (above '+s.offTemp+'°)','hysteresis':s=>s.heaterOn?'Heating up to '+s.offTemp+'°':'Comes on at '+s.onTemp+'°',
 'outside schedule':()=>'Outside schedule','disabled':()=>'Frost guard is turned off','time not synced':()=>'Waiting for the clock to sync','sensor failure':()=>'Sensor not responding, kept off',
 'boost':s=>'Boost, '+dur(s.boostLeftS/60)+' left','boost, reached off temp':s=>'Boost paused, reached '+s.offTemp+'°'};
function conn(ok,txt){$('#cdot').style.background=ok==1?'var(--ok)':ok==2?'var(--warnB)':'var(--bad)';$('#ctext').textContent=txt}
async function status(){let d;try{d=await j('/api/status')}catch(e){conn(0,'Offline');return}S=d;brand(d.title);if(!dirtyN())theme(d.theme);
 const now=new Date(d.now*1000);
 conn(d.timeValid?1:2,d.timeValid?'Online · '+DAY[now.getDay()]+' '+now.getDate()+' '+MON[now.getMonth()]+', '+clk(now):'Online · waiting for time');
 $('#temp').textContent=d.temp==null?'–':d.temp.toFixed(1)+'°';$('#hum').textContent=d.hum==null?'':Math.round(d.hum)+'% RH';
 const lo=Math.floor(d.onTemp-6),hi=Math.ceil(d.offTemp+6),P=v=>Math.max(0,Math.min(100,(v-lo)/(hi-lo)*100))+'%';
 $('#tkOn').style.left=$('#son').style.left=P(d.onTemp);$('#tkOff').style.left=$('#soff').style.left=P(d.offTemp);
 $('#son').textContent='on '+d.onTemp+'°';$('#soff').textContent='off '+d.offTemp+'°';$('#slo').textContent=lo+'°';$('#shi').textContent=hi+'°';
 $('#knob').style.left=d.temp==null?'50%':P(d.temp);$('#knob').style.opacity=d.temp==null?.25:1;
 $('#sensor').textContent=d.sensorAgeS<0?'Waiting for the first sensor reading':d.sensorAgeS>60?'No sensor reading for '+ago(d.sensorAgeS)+' ('+d.sensorErrors+' errors)':'Sensor read '+ago(d.sensorAgeS)+' ago · 1-minute average';
 const on=d.heaterKnown&&d.heaterOn;$('#hicon').classList.toggle('on',on);
 $('#hstate').textContent=d.heaterMode==0?'Not switched':!d.heaterKnown?'Unknown':on?'On':'Off';
 let w=(why[d.reason]||(()=>d.reason||''))(d);if(d.testHoldS>0)w='Manual test · automatic control resumes in '+d.testHoldS+' s';else if(d.heaterMode&&d.heaterKnown&&d.wantOn!=d.heaterOn)w+=' · retrying';$('#hwhy').textContent=w;
 const via=['Monitor only, no plug configured','Via Smart Life plug (local)','Via Voice Monkey → Alexa'][d.heaterMode]||'';
 const lc=d.lastCmd?(d.lastCmd.startsWith('OK')?'last command OK':d.lastCmd)+(d.lastCmdTime?' · '+ago(Math.max(0,d.now-d.lastCmdTime))+' ago':''):'';
 $('#via').innerHTML=esc(via)+(lc?' · <span style="color:'+(d.lastCmd.startsWith('OK')?'inherit':'var(--bad)')+'">'+esc(lc)+'</span>':'');
 if(d.timeValid){const nc=nextChange(now,d);$('#snext').textContent=d.window?'Heating allowed':nc?'Next: '+when(nc,now):'Not scheduled';
  $('#sdesc').textContent=(d.window&&nc?'Until '+when(nc,now)+' · ':'')+schedText(d)+(d.enabled?'':' · frost guard off');weekStrip(now,d)}
 else{$('#snext').textContent='Waiting for time';$('#sdesc').textContent=schedText(d)}
 $('#boff').textContent=d.offTemp;$('#bon').classList.toggle('hide',!(d.boostLeftS>0));$('#bleft').textContent='Boosting · '+dur(d.boostLeftS/60)+' left';
 const today=dkey(now);$('#events').innerHTML=d.events.map(e=>{const m=e.match(/^(\d{4}-\d\d-\d\d) (\d\d:\d\d):\d\d\s+(.*)$/);let t='',x=e;
  if(m){x=m[3];t=m[1]==today?m[2]:DAY[new Date(m[1]+'T12:00').getDay()]+' '+m[2]}else{const u=e.match(/^(uptime \S+)\s+(.*)$/);if(u){t=u[1].replace('uptime ','+');x=u[2]}}
  const c=/FAIL/i.test(x)?'var(--bad)':/Heater ON/.test(x)?'var(--heat)':/Heater OFF/.test(x)?'var(--mute)':/Boot|WiFi|Time|SD/.test(x)?'var(--cool)':'var(--field)';
  return '<li><span class="t">'+esc(t)+'</span><span class="dot" style="background:'+c+'"></span><span>'+esc(x)+'</span></li>'}).join('')||'<li><span></span><span></span><span class="sub">Nothing yet</span></li>';
 const gb=d.sdTotalMB>1024?(d.sdTotalMB/1024).toFixed(1)+' GB':d.sdTotalMB+' MB';
 $('#sys').innerHTML=['Firmware '+d.fw,d.ip,'WiFi '+d.rssi+' dBm','Up '+ago(d.uptimeS),d.sd?'SD '+d.sdUsedMB+' MB of '+gb:'SD card not available'].map(x=>'<span>'+esc(x)+'</span>').join('');
 if(!hist.length||tab=='status')drawStats()}
function drawStats(){const end=Date.now(),p=hist.filter(x=>x.t>end-864e5),s=summary(p);$('#ran').textContent=s.n?(s.run?dur(s.run):'Not at all'):'–';$('#low').textContent=s.n?f1(s.lo)+' at '+clk(new Date(s.loT)):'–'}
async function loadHist(){try{const t=await (await fetch('/api/history')).text();hist=t.trim().split('\n').filter(Boolean).map(l=>{const a=l.split(',');return{t:+a[0]*1000,temp:+a[1],hum:+a[2],h:+a[3],w:+a[4]}});drawMain();drawStats()}catch(e){}}
function drawMain(){if(tab!='status')return;const end=S?S.now*1000:Date.now();chart($('#chart'),hist,end-864e5,end,{live:1,on:S&&S.onTemp,off:S&&S.offTemp,readout:$('#rd'),hint:'Hover or tap the chart for readings',label:'Temperature over the last 24 hours'})}
async function boost(m){try{await j('/api/boost?minutes='+m,{method:'POST'});toast(m?'Boost started for '+dur(m):'Boost cancelled');status()}catch(e){toast(e.message)}}

// ---------- logs ----------
let files=[],sel=null,sumQ=Promise.resolve();
function parseCsv(t){const pts=[],ev=[];t.split('\n').slice(1).forEach(l=>{l=l.trim();if(!l)return;const r=l.split(',');if(r.length<6)return;const tt=new Date(r[0].replace(' ','T')).getTime();
  if(r[1]!=='')pts.push({t:tt,temp:+r[1],hum:+r[2],h:+r[3],w:+r[4]});if(r[5])ev.push({t:r[0].slice(11,16),x:r.slice(5).join(',').trim()})});return{pts,ev}}
function dayLabel(n,long){const d=new Date(n.slice(0,10)+'T12:00'),now=S?new Date(S.now*1000):new Date(),y=new Date(now);y.setDate(y.getDate()-1);
 if(long)return DAYL[d.getDay()]+' '+d.getDate()+' '+MONL[d.getMonth()]+(d.getFullYear()!=now.getFullYear()?' '+d.getFullYear():'');
 return n.startsWith(dkey(now))?'Today':n.startsWith(dkey(y))?'Yesterday':DAY[d.getDay()]+' '+d.getDate()+' '+MON[d.getMonth()]}
const isToday=n=>S&&n.startsWith(dkey(new Date(S.now*1000)));
const kb=b=>b<1024?b+' B':b<1048576?Math.round(b/1024)+' KB':(b/1048576).toFixed(1)+' MB';
function sumKey(f){return 'st-sum:'+f.name+':'+f.size}
function daysList(){$('#days').innerHTML=files.map(f=>{const s=ls.get(sumKey(f));const rng=s&&s.n?f1(s.lo).slice(0,-1)+'–'+f1(s.hi):'',ht=s?(s.n?(s.run?'heater '+dur(s.run):'no heating'):'no readings'):'';
  return '<li><button type="button" data-n="'+esc(f.name)+'"'+(f.name==sel?' aria-current="true"':'')+'><b>'+esc(dayLabel(f.name))+'</b><small class="num" style="font-size:13px">'+rng+'</small><small>'+ht+'</small><small>'+kb(f.size)+'</small></button></li>'}).join('')||'<li class="sub" style="padding:10px 12px">No log files yet</li>';
 $$('#days button').forEach(b=>b.onclick=()=>openDay(b.dataset.n))}
async function loadLogs(){let d;try{d=await j('/api/logs')}catch(e){toast(e.message);return}
 files=d.files.filter(f=>/^\d{4}-\d\d-\d\d\.csv$/.test(f.name)).sort((a,b)=>b.name.localeCompare(a.name));
 if(S){$('#keep').textContent='kept '+S.retentionDays+' days';const pct=S.sdTotalMB?Math.max(1,S.sdUsedMB/S.sdTotalMB*100):0;$('#sdbar').style.width=pct+'%';
  $('#sdtxt').textContent=S.sd?'SD card · '+S.sdUsedMB+' MB of '+(S.sdTotalMB>1024?(S.sdTotalMB/1024).toFixed(1)+' GB':S.sdTotalMB+' MB')+' used':'SD card not available'}
 if(!sel||!files.find(f=>f.name==sel))sel=files[0]&&files[0].name;daysList();if(sel)openDay(sel);
 files.slice(0,14).forEach(f=>{if(!isToday(f.name)&&ls.get(sumKey(f)))return;sumQ=sumQ.then(async()=>{if(tab!='logs'||(ls.get(sumKey(f))&&!isToday(f.name)))return;
  try{const t=await (await fetch('/api/log?f='+encodeURIComponent(f.name))).text();ls.set(sumKey(f),summary(parseCsv(t).pts));daysList()}catch(e){}})})}
async function openDay(n){sel=n;daysList();const f=files.find(x=>x.name==n);$('#dtitle').textContent=dayLabel(n,1);$('#dfile').textContent=n+' · loading…';
 $('#ddl').href='/api/log?f='+encodeURIComponent(n)+'&download=1';$('#ddel').onclick=()=>del(n);
 let t;try{t=await (await fetch('/api/log?f='+encodeURIComponent(n))).text()}catch(e){$('#dfile').textContent=n+' · could not load';return}if(sel!=n)return;
 const {pts,ev}=parseCsv(t),s=summary(pts);if(f)ls.set(sumKey(f),s);daysList();
 $('#dfile').textContent=n+' · '+pts.length.toLocaleString()+' readings';
 const tile=(k,v,sub,c)=>'<div class="stat"><div class="k">'+k+'</div><div class="v"'+(c?' style="color:'+c+'"':'')+'>'+v+'</div><div class="k">'+sub+'</div></div>';
 $('#dstats').innerHTML=s.n?tile('Low',f1(s.lo),clk(new Date(s.loT)),'var(--cool)')+tile('High',f1(s.hi),clk(new Date(s.hiT)),'var(--heat)')+tile('Heater ran',s.run?dur(s.run).replace(' min',' m'):'0 m',s.cyc+(s.cyc==1?' cycle':' cycles'))+tile('Humidity',Math.round(s.hl)+'–'+Math.round(s.hh)+'%','range'):'<div class="sub">No readings this day</div>';
 const d0=new Date(n.slice(0,10)+'T00:00:00').getTime(),d1=new Date(d0);d1.setDate(d1.getDate()+1);
 chart($('#dchart'),pts,d0,+d1,{labelH:6,on:S&&S.onTemp,off:S&&S.offTemp,readout:$('#drd'),hint:'',label:'Temperature on '+dayLabel(n,1)});$('#drd').textContent='';
 $('#devents').innerHTML=ev.map(e=>'<li style="grid-template-columns:64px 0 minmax(0,1fr)"><span class="t">'+esc(e.t)+'</span><span></span><span>'+esc(e.x)+'</span></li>').join('')||'<li><span></span><span></span><span class="sub">No events</span></li>'}
async function del(n){if(!confirm('Delete the log for '+dayLabel(n,1)+'? This cannot be undone.'))return;try{await j('/api/log?f='+encodeURIComponent(n),{method:'DELETE'});toast('Deleted '+n);sel=null;loadLogs()}catch(e){toast(e.message)}}

// ---------- settings ----------
const F=$('#sf'),NUM=['theme','ledLevel','onTemp','offTemp','tempOffset','tuyaDp','logIntervalS','retentionDays','heaterMode'];let base=null;
function vals(){const o={};for(const el of F.elements){if(!el.name)continue;if(el.type=='radio'){if(el.checked)o[el.name]=+el.value;continue}
 o[el.name]=el.type=='checkbox'?el.checked:el.type=='time'?(el.value?(+el.value.slice(0,2))*60+(+el.value.slice(3,5)):0):NUM.includes(el.name)?+el.value:el.value}return o}
function fill(d){for(const[k,v]of Object.entries(d)){const els=F.elements[k];if(!els)continue;
 if(els instanceof RadioNodeList&&els[0]&&els[0].type=='radio'){[...els].forEach(r=>r.checked=+r.value==v);continue}const el=els;
 if(el.type=='checkbox')el.checked=!!v;else if(el.type=='time')el.value=hm(v);else if(k=='logIntervalS'){if(![...el.options].some(o=>+o.value==v))el.add(new Option(v+' seconds',v));el.value=v}else el.value=v}
 base=vals();ui()}
function dirtyN(){if(!base)return 0;const v=vals();return Object.keys(v).filter(k=>v[k]!==base[k]).length}
function ui(){const m=(vals().heaterMode)|0;if(base)theme(vals().theme);$('#m1').classList.toggle('hide',m!=1);$('#m2').classList.toggle('hide',m!=2);$('#tst').classList.toggle('hide',m==0);
 $('#wpw').classList.toggle('warn',!F.elements.webPass.value);
 if(base){const hc=F.elements.hostname.value.toLowerCase()!=base.hostname;$('#hostnote').textContent=hc?'Saving restarts the device at the new address, with a new certificate (your browser will warn again).':'Letters, digits and hyphens.';$('#hostnote').style.color=hc?'var(--heat)':''}const n=dirtyN();$('#bar').classList.toggle('hide',!n);$('#dirty').textContent=n+(n==1?' unsaved change':' unsaved changes')}
F.addEventListener('input',ui);F.addEventListener('change',ui);
addEventListener('beforeunload',e=>{if(dirtyN()){e.preventDefault();e.returnValue=''}});
async function loadSettings(){try{fill(await j('/api/settings'))}catch(e){toast(e.message)}
 if($('#fp').textContent=='…')j('/api/cert').then(c=>$('#fp').textContent=c.sha256).catch(()=>{})}
function discard(){loadSettings();toast('Changes discarded')}
async function save(e){e.preventDefault();const o=vals();if(o.offTemp<=o.onTemp){toast('The off temperature must be higher than the on temperature');return}
 o.hostname=o.hostname.toLowerCase();const hostChanged=base&&o.hostname!=base.hostname;
 try{const d=await j('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(o)});fill(d);brand(d.title);status();
  if(hostChanged){toast('Saved. Restarting as '+o.hostname+'.local…');setTimeout(()=>location.href='https://'+o.hostname+'.local/#settings',8000)}
  else toast(d.netChanged?'Saved. Reconnecting WiFi…':'Settings saved')}catch(err){toast(err.message)}}
async function test(c){if(dirtyN()){toast('Save your changes first, then test');return}const r=$('#tres');r.className='res';r.textContent='Sending…';
 try{const d=await j('/api/heater?cmd='+c,{method:'POST'});r.className='res '+(d.ok?'ok':'bad');r.textContent=(d.ok?'✓ Accepted':'✗ '+d.result)+' · '+clk(new Date())+' · automatic control resumes in 1 min'}catch(e){r.className='res bad';r.textContent=e.message}}
function waitBack(msg){const el=$('#fwres');el.className='res';let n=0;const t0=Date.now();const tick=async()=>{n++;el.textContent=msg+' ('+Math.round((Date.now()-t0)/1000)+' s)';
 try{const d=await j('/api/status');if(d.uptimeS<120){el.textContent='Back online';setTimeout(()=>location.reload(),800);return}}catch(e){}if(n<60)setTimeout(tick,2500);else el.textContent='Device has not come back. Try reloading.'};setTimeout(tick,4000)}
async function reboot(){if(!confirm('Restart the device now?'))return;try{await j('/api/reboot',{method:'POST'});waitBack('Restarting…')}catch(e){toast(e.message)}}
$('#fw').onchange=()=>{const f=$('#fw').files[0];$('#fw').value='';if(!f)return;if(!confirm('Install '+f.name+' ('+kb(f.size)+')? The device restarts when it finishes.'))return;
 const el=$('#fwres');el.className='res';const x=new XMLHttpRequest();x.open('POST','/api/update');
 x.upload.onprogress=e=>{if(e.lengthComputable)el.textContent='Uploading '+Math.round(e.loaded/e.total*100)+'%'};
 x.onload=()=>{let d={};try{d=JSON.parse(x.responseText)}catch(e){}if(x.status==200){el.className='res ok';waitBack('Installed. Restarting…')}else{el.className='res bad';el.textContent='Update failed: '+(d.error||x.status)}};
 x.onerror=()=>{el.className='res bad';el.textContent='Upload failed (connection lost)'};x.send(f)};

// ---------- start ----------
const h0=location.hash.slice(1);status().then(()=>{loadHist();if(h0=='logs'||h0=='settings')show(h0)});
setInterval(()=>{if(!document.hidden)status()},5000);setInterval(()=>{if(!document.hidden)loadHist()},60000);
let rz;addEventListener('resize',()=>{clearTimeout(rz);rz=setTimeout(drawMain,150)});
</script></body></html>)HTML";
