#pragma once

// Komplettes HTML als PROGMEM-String
static const char HTML_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="de" data-theme="auto">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Nerd-Display – Einstellungen</title>
<style>
/* ===== Design Tokens ===== */
:root {
  --bg: #f6f7fb;
  --card: #ffffff;
  --text: #0f1222;
  --muted: #687085;
  --border: #e8eaf1;
  --primary: #4f46e5;
  --primary-contrast: #ffffff;
  --accent: #06b6d4;

  --shadow: 0 10px 25px rgba(16,24,40,.06);
  --radius: 14px;
  --pad: 14px;
}

/* Dark Scheme (automatisch) */
@media (prefers-color-scheme: dark) {
  :root {
    --bg: #0b0f19;
    --card: #121827;
    --text: #e5e7eb;
    --muted: #9aa4b2;
    --border: #1f2937;
    --shadow: 0 10px 25px rgba(0,0,0,.35);
  }
}

/* Manual override via [data-theme] */
html[data-theme="dark"] {
  --bg: #0b0f19; --card:#121827; --text:#e5e7eb; --muted:#9aa4b2; --border:#1f2937; --shadow:0 10px 25px rgba(0,0,0,.35);
}
html[data-theme="light"] {
  --bg: #f6f7fb; --card:#ffffff; --text:#0f1222; --muted:#687085; --border:#e8eaf1; --shadow:0 10px 25px rgba(16,24,40,.06);
}

/* ===== Base ===== */
*{box-sizing:border-box}
html,body{margin:0}
body{font:16px/1.55 system-ui, -apple-system, Segoe UI, Roboto, Helvetica, Arial, sans-serif; background:var(--bg); color:var(--text);}

/* ===== Layout ===== */
.container{max-width:980px;margin:28px auto;padding:0 16px}
.header {
  display:flex; align-items:center; justify-content:space-between;
  margin-bottom:16px;
}
.brand { display:flex; gap:12px; align-items:center; }
.brand h1{font-size:22px; margin:0}
.badges { display:flex; gap:8px; flex-wrap:wrap; }

.badge {
  display:inline-flex; align-items:center; gap:6px;
  padding:6px 10px; border:1px solid var(--border); border-radius:999px;
  color: var(--muted); background: color-mix(in oklab, var(--card) 92%, transparent);
  font-size:12px;
}
.badge code { font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; font-size:12px; opacity:.9; }

.theme-toggle {
  display:inline-flex; align-items:center; gap:8px;
  border:1px solid var(--border); background:var(--card); border-radius:999px; padding:8px 12px;
  cursor:pointer; user-select:none;
}
.theme-toggle svg{width:18px;height:18px}

.card{
  background:var(--card); border:1px solid var(--border); border-radius:var(--radius);
  box-shadow:var(--shadow); padding:20px; margin:18px 0;
}
.card h2{font-size:18px; margin:0 0 12px}
.card p.small{color:var(--muted); margin:6px 0 0}

.grid{display:grid; gap:12px}
@media (min-width:720px){ .grid.cols-2{grid-template-columns:1fr 1fr} }

/* ===== Form ===== */
label{display:block; font-weight:600; margin-top:6px}
input,select,button{
  font-size:15px;
}
input,select{
  width:100%; padding:12px 12px; border:1px solid var(--border); border-radius:12px; background:transparent; color:var(--text);
}
input::placeholder{color:var(--muted)}
.field-row{display:flex; gap:12px; align-items:center}

.actions{margin-top:14px; display:flex; gap:10px; flex-wrap:wrap}
.btn{
  appearance:none; border:0; border-radius:12px; padding:12px 16px; cursor:pointer; font-weight:600;
}
.btn.primary{ background:var(--primary); color:var(--primary-contrast); }
.btn.secondary{ background:transparent; border:1px solid var(--border); color:var(--text); }
.btn.warn{ background:#fee2e2; color:#991b1b; border:1px solid #fecaca; }

/* Copy button */
.copy {
  display:inline-flex; align-items:center; gap:8px; padding:8px 10px; border:1px dashed var(--border); border-radius:10px; cursor:pointer;
}

/* Toast */
.toast {
  position:fixed; left:50%; bottom:24px; transform:translateX(-50%);
  background:var(--card); color:var(--text); border:1px solid var(--border); border-radius:12px; padding:10px 14px; box-shadow:var(--shadow);
  opacity:0; pointer-events:none; transition:opacity .2s ease, transform .2s ease;
}
.toast.show{opacity:1; transform:translateX(-50%) translateY(-4px);}

/* Danger block (fix: lesbar in hell & dunkel) */
.danger {
  border-radius: var(--radius);
  padding: 20px;
  border: 1px solid #ffd6d6;
  background: #ffefef;
  color: #7a0f0f;
}
@media (prefers-color-scheme: dark) {
  .danger {
    background: #3a0e0e;
    border: 1px solid #5c1a1a;
    color: #f8d0d0;
  }
}
html[data-theme="dark"] .danger {
  background: #3a0e0e;
  border: 1px solid #5c1a1a;
  color: #f8d0d0;
}
</style>
</head>
<body>
  <div class="container">

    <div class="header">
      <div class="brand">
        <svg width="28" height="28" viewBox="0 0 24 24" fill="none"><circle cx="12" cy="12" r="10" stroke="currentColor" opacity=".25"/><path d="M5 15h8l1.2 2H7.2L5 15Zm2-6h10l2 3H9l-2-3Z" fill="currentColor" opacity=".6"/></svg>
        <h1>Nerd-Display</h1>
      </div>
      <div class="theme-toggle" id="themeBtn" title="Theme umschalten">
        <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M12 3a9 9 0 0 0 0 18 9 9 0 0 1 0-18Z" stroke="currentColor" stroke-width="1.5"/></svg>
        <span id="themeLabel">Auto</span>
      </div>
    </div>

    <div class="card">
      <div class="badges">
        <span class="badge">ID: <code>%%ID%%</code></span>
        <span class="badge">BaseTopic: <code id="btText">%%BT%%</code></span>
        <span class="badge">IP: <code>%%IP%%</code></span>
        <span class="badge">mDNS: <code>%%MDNS%%</code></span>
      </div>
      <div style="margin-top:10px">
        <button class="copy" id="copyBt" type="button" title="BaseTopic kopieren">BaseTopic kopieren</button>
        <span class="small" style="color:var(--muted); margin-left:8px">Lauftext, Helligkeit & Effekte werden ausschließlich per MQTT gesetzt.</span>
      </div>
    </div>

    <div class="card">
      <h2>MQTT</h2>
      <form method="POST" action="/save">
        <div class="grid cols-2">
          <label>Host/IP
            <input name="mqttHost" value="%%mqttHost%%" placeholder="192.168.1.10">
          </label>
          <label>Port
            <input name="mqttPort" type="number" min="1" max="65535" value="%%mqttPort%%">
          </label>
          <label>User
            <input name="mqttUser" value="%%mqttUser%%" placeholder="">
          </label>
          <label>Password
            <input name="mqttPass" type="password" value="%%mqttPass%%" placeholder="">
          </label>
          <label>Base Topic (ohne Slash am Ende)
            <input name="mqttBase" value="%%mqttBase%%" placeholder="zebra/nerd-display">
          </label>
        </div>

        <h2 style="margin-top:18px">Netzwerk</h2>
        <div class="grid cols-2">
          <label>mDNS-Name (ohne „nerd-display-“)
            <input name="mdnsName" value="%%mdnsName%%" placeholder="zebra">
          </label>
          <div>
            <p class="small">Aufruf: <code>nerd-display-%%mdnsName%%.local</code></p>
          </div>
        </div>

        <h2 style="margin-top:18px">Display</h2>
        <div class="grid cols-2">
          <label>Anzahl MAX7219-Module in Reihe
            <input name="displayCount" type="number" min="%%displayMin%%" max="%%displayMax%%" value="%%displayCount%%">
          </label>
          <div>
            <p class="small">Wertebereich: %%displayMin%% bis %%displayMax%%. Änderung wird nach dem Speichern aktiv.</p>
          </div>
        </div>

        <div class="actions">
          <button class="btn primary" type="submit">Speichern</button>
          <a class="btn secondary" href="/">Neu laden</a>
        </div>
      </form>
    </div>

    <div class="card danger">
      <h2>Werksreset</h2>
      <p class="small">Löscht WLAN & Einstellungen. Danach startet der AP erneut.</p>
      <form method="POST" action="/factory_reset">
        <div class="actions">
          <button class="btn warn" type="submit">Werksreset jetzt ausführen</button>
        </div>
      </form>
    </div>
  </div>

  <div class="toast" id="toast">Kopiert ✓</div>

<script>
/* ===== Theme Toggle with localStorage ===== */
(function(){
  const root = document.documentElement;
  const btn = document.getElementById('themeBtn');
  const label = document.getElementById('themeLabel');

  function apply(mode){
    if(mode === 'auto'){ root.setAttribute('data-theme','auto'); label.textContent = 'Auto'; }
    else if(mode === 'light'){ root.setAttribute('data-theme','light'); label.textContent = 'Hell'; }
    else { root.setAttribute('data-theme','dark'); label.textContent = 'Dunkel'; }
    localStorage.setItem('nd_theme', mode);
  }

  const saved = localStorage.getItem('nd_theme') || 'auto';
  apply(saved);

  btn.addEventListener('click', ()=>{
    const cur = localStorage.getItem('nd_theme') || 'auto';
    const next = cur === 'auto' ? 'dark' : (cur === 'dark' ? 'light' : 'auto');
    apply(next);
  });
})();

/* ===== Copy BaseTopic ===== */
(function(){
  const btn = document.getElementById('copyBt');
  const text = document.getElementById('btText');
  const toast = document.getElementById('toast');
  btn.addEventListener('click', async ()=>{
    try {
      await navigator.clipboard.writeText(text.textContent.trim());
      toast.classList.add('show');
      setTimeout(()=>toast.classList.remove('show'), 1300);
    } catch(e) {}
  });
})();
</script>
</body>
</html>
)HTML";
