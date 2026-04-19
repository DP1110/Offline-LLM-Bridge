#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

// --- Access Point Config ---
const char* AP_SSID = "LLM-Bridge"; 
const char* AP_PASS = "llm12345";   

// --- Ollama Config ---
String laptopIP    = "192.168.4.2"; 
int    ollamaPort  = 11434;
String ollamaModel = "llama3";      

ESP8266WebServer server(80);

// --- Chat Web UI ---
const char* chatPage = R"rawliteral(
<!DOCTYPE html><html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Offline LLM Chat</title>
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:'Segoe UI',sans-serif;background:#0d0d1a;color:#e0e0e0;display:flex;flex-direction:column;align-items:center;padding:16px;min-height:100vh}
    .header{text-align:center;margin-bottom:16px}
    h1{color:#00d4ff;font-size:20px}
    .sub{color:#444;font-size:11px;margin-top:4px}
    .badges{display:flex;gap:8px;justify-content:center;margin-top:8px}
    .badge{padding:3px 12px;border-radius:20px;font-size:11px}
    .b1{background:#00ff8820;color:#00ff88}
    .b2{background:#ff880020;color:#ff8800}
    .chat{width:100%;max-width:600px;background:#13131f;border:1px solid #ffffff0d;border-radius:16px;padding:14px;height:420px;overflow-y:auto;display:flex;flex-direction:column;gap:12px;margin-bottom:14px}
    .msg{max-width:82%;padding:10px 14px;border-radius:12px;font-size:14px;line-height:1.6;word-wrap:break-word}
    .user{background:#00d4ff1a;border:1px solid #00d4ff33;color:#cceeff;align-self:flex-end;border-bottom-right-radius:4px}
    .ai{background:#1a1a2e;border:1px solid #ffffff0d;color:#e0e0e0;align-self:flex-start;border-bottom-left-radius:4px}
    .sys{background:transparent;color:#333;font-size:11px;align-self:center;border:none;text-align:center}
    .lbl{font-size:10px;font-weight:bold;text-transform:uppercase;letter-spacing:.5px;margin-bottom:4px;color:#666}
    .user .lbl{color:#00d4ff88}
    .ai .lbl{color:#00ff8888}
    .row{display:flex;width:100%;max-width:600px;gap:10px}
    textarea{flex:1;background:#13131f;border:1px solid #ffffff15;border-radius:10px;padding:11px 14px;color:#fff;font-size:14px;resize:none;height:48px;outline:none;font-family:inherit;transition:border .2s}
    textarea:focus{border-color:#00d4ff55}
    .btns{display:flex;flex-direction:column;gap:6px}
    button{background:linear-gradient(135deg,#00d4ff,#0055ff);color:#000;font-weight:bold;border:none;padding:0 18px;border-radius:8px;cursor:pointer;font-size:14px;height:48px;white-space:nowrap;transition:opacity .2s}
    button:hover{opacity:.85}
    button:disabled{opacity:.4;cursor:not-allowed}
    .btn-settings{background:linear-gradient(135deg,#ff8800,#ff4400);height:48px;padding:0 14px;font-size:13px}
    .spinner{display:inline-block;width:13px;height:13px;border:2px solid #ffffff33;border-top-color:#000;border-radius:50%;animation:spin .7s linear infinite;margin-right:5px;vertical-align:middle}
    @keyframes spin{to{transform:rotate(360deg)}}
    .modal{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:#000000bb;z-index:100;justify-content:center;align-items:center}
    .modal.open{display:flex}
    .modal-card{background:#13131f;border:1px solid #00d4ff22;border-radius:16px;padding:28px;width:320px}
    .modal-card h3{color:#ff8800;margin-bottom:16px;font-size:16px}
    label{font-size:12px;color:#888;display:block;margin-bottom:4px;margin-top:12px}
    input{width:100%;background:#0d0d1a;border:1px solid #ffffff15;border-radius:8px;padding:10px 12px;color:#fff;font-size:14px;outline:none}
    input:focus{border-color:#00d4ff55}
    .modal-btns{display:flex;gap:8px;margin-top:18px}
    .btn-save{background:linear-gradient(135deg,#00d4ff,#0055ff);color:#000;font-weight:bold;border:none;padding:10px;border-radius:8px;cursor:pointer;flex:1;font-size:14px}
    .btn-close{background:#1e1e30;color:#888;border:none;padding:10px;border-radius:8px;cursor:pointer;flex:1;font-size:14px}
  </style>
</head>
<body>
<div class="header">
  <h1>Offline LLM Chat</h1>
  <p class="sub">via ESP8266 Bridge - No Internet Required</p>
  <div class="badges">
    <span class="badge b1" id="connBadge">ESP8266 Online</span>
    <span class="badge b2" id="modelBadge">Checking...</span>
  </div>
</div>
<div class="chat" id="chatBox">
  <div class="msg sys">Private offline chat - connect both laptop & phone to <strong>LLM-Bridge</strong> WiFi</div>
</div>
<div class="row">
  <textarea id="msgInput" placeholder="Type your message... (Enter to send)"></textarea>
  <div class="btns">
    <button id="sendBtn" onclick="sendMsg()">Send</button>
    <button class="btn-settings" onclick="openSettings()">Config</button>
  </div>
</div>
<div class="modal" id="settingsModal">
  <div class="modal-card">
    <h3>Connection Settings</h3>
    <label>Laptop IP Address</label>
    <input type="text" id="ipInput" placeholder="192.168.4.2">
    <label>Ollama Model</label>
    <input type="text" id="modelInput" placeholder="llama3">
    <div class="modal-btns">
      <button class="btn-save" onclick="saveSettings()">Save</button>
      <button class="btn-close" onclick="closeSettings()">Close</button>
    </div>
  </div>
</div>
<script>
  const chatBox  = document.getElementById('chatBox');
  const msgInput = document.getElementById('msgInput');
  const sendBtn  = document.getElementById('sendBtn');
  msgInput.addEventListener('keydown', function(e) { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); sendMsg(); } });
  
  function addMsg(role, text) {
    const d = document.createElement('div');
    d.className = 'msg ' + role;
    if (role !== 'sys') {
      const lbl = document.createElement('div');
      lbl.className = 'lbl';
      lbl.textContent = role === 'user' ? 'You' : 'LLM';
      d.appendChild(lbl);
    }
    const c = document.createElement('div');
    c.textContent = text;
    d.appendChild(c);
    chatBox.appendChild(d);
    chatBox.scrollTop = chatBox.scrollHeight;
    return d;
  }
  function setLoading(on) {
    sendBtn.disabled = on;
    sendBtn.innerHTML = on ? '<span class="spinner"></span>Thinking...' : 'Send';
  }
  async function sendMsg() {
    const text = msgInput.value.trim();
    if (!text) return;
    addMsg('user', text);
    msgInput.value = "";
    setLoading(true);
    try {
      const res = await fetch('/chat', {
        method: 'POST',
        headers: {'Content-Type':'application/x-www-form-urlencoded'},
        body: 'msg=' + encodeURIComponent(text)
      });
      const reply = await res.text();
      addMsg('ai', reply);
    } catch(e) {
      addMsg('ai', 'Error: Cannot reach ESP8266.');
    }
    setLoading(false);
    msgInput.focus();
  }
  async function checkStatus() {
    try {
      const r = await fetch('/status');
      const d = await r.json();
      document.getElementById('modelBadge').textContent = 'Model: ' + d.model;
    } catch(e) {
      document.getElementById('modelBadge').textContent = 'LLM Offline';
    }
  }
  function openSettings()  { document.getElementById('settingsModal').classList.add('open'); }
  function closeSettings() { document.getElementById('settingsModal').classList.remove('open'); }
  async function saveSettings() {
    const ip    = document.getElementById('ipInput').value.trim();
    const model = document.getElementById('modelInput').value.trim();
    if (!ip || !model) return;
    await fetch('/config', {
      method: 'POST',
      headers: {'Content-Type':'application/x-www-form-urlencoded'},
      body: 'ip=' + encodeURIComponent(ip) + '&model=' + encodeURIComponent(model)
    });
    closeSettings();
    addMsg('sys', 'Config saved - IP: ' + ip + ' | Model: ' + model);
    checkStatus();
  }
  checkStatus();
  setInterval(checkStatus, 15000);
</script>
</body></html>
)rawliteral";

String askOllama(String prompt) {
  String url = "http://" + laptopIP + ":" + String(ollamaPort) + "/api/generate";
  WiFiClient client;
  HTTPClient http;
  
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(60000); 

  StaticJsonDocument<1024> req;
  req["model"]  = ollamaModel;
  req["prompt"] = prompt;
  req["stream"] = false;

  String payload;
  serializeJson(req, payload);

  int code = http.POST(payload);
  String reply = "Error: Cannot reach Ollama.";

  if (code == 200) {
    String raw = http.getString();
    StaticJsonDocument<4096> res;
    deserializeJson(res, raw);
    reply = res["response"].as<String>();
  }
  http.end();
  return reply;
}

void handleRoot() { server.send(200, "text/html", chatPage); }

void handleChat() {
  if (!server.hasArg("msg")) {
    server.send(400, "text/plain", "Missing message");
    return;
  }
  server.send(200, "text/plain", askOllama(server.arg("msg")));
}

void handleStatus() {
  String json = "{\"model\":\"" + ollamaModel + "\",\"ip\":\"" + laptopIP + "\",\"free_mem\":" + String(ESP.getFreeHeap()) + "}";
  server.send(200, "application/json", json);
}

void handleConfig() {
  if (server.hasArg("ip"))    laptopIP    = server.arg("ip");
  if (server.hasArg("model")) ollamaModel = server.arg("model");
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  WiFi.softAP(AP_SSID, AP_PASS);
  server.on("/",       HTTP_GET,  handleRoot);
  server.on("/chat",   HTTP_POST, handleChat);
  server.on("/status", HTTP_GET,  handleStatus);
  server.on("/config", HTTP_POST, handleConfig);
  server.begin();
}

void loop() {
  server.handleClient();
}