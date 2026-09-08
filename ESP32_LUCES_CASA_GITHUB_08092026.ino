#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ================================================================
// CONFIGURACIÓN DE RED Y BROKER MQTT NUBE
// ================================================================
const char* WIFI_SSID     = "A35 de Leonardo";
const char* WIFI_PASSWORD = "123456789";

const char* MQTT_BROKER   = "broker.hivemq.com";
const int   MQTT_PORT     = 1883;

const char* TOPIC_CONTROL = "casa/leonardo/reles/control";
const char* TOPIC_ESTADO  = "casa/leonardo/reles/estado";

// Asignación de GPIOs para los 8 relés (LilyGO T-Relay / Estándar)
const uint8_t RELAY_PINS[8] = {5, 18, 19, 21, 12, 13, 32, 33};
bool relayStates[8] = {false, false, false, false, false, false, false, false};

WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ================================================================
// CÓDIGO INTERFAZ WEB HTML / CSS / JS EN MEMORIA FLASH (PROGMEM)
// ================================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Control de Luces ESP32</title>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/paho-mqtt/1.0.1/mqttws31.min.js"></script>
  <style>
    :root {
      --bg: #0b0f19;
      --card-bg: rgba(30, 41, 59, 0.7);
      --accent: #38bdf8;
      --text: #f8fafc;
      --text-muted: #94a3b8;
      --switch-on: #22c55e;
      --switch-off: #334155;
      --status-online: #22c55e;
      --status-offline: #ef4444;
      --danger: #f87171;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: system-ui, -apple-system, sans-serif; }
    body {
      background: radial-gradient(circle at top, #1e293b, var(--bg));
      color: var(--text);
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 24px 16px;
    }
    #pin-screen {
      position: fixed; top: 0; left: 0; right: 0; bottom: 0;
      background: rgba(11, 15, 25, 0.95);
      backdrop-filter: blur(16px);
      z-index: 999;
      display: flex; flex-direction: column; justify-content: center; align-items: center; padding: 20px;
    }
    .pin-container {
      background: var(--card-bg);
      border: 1px solid rgba(255, 255, 255, 0.1);
      border-radius: 24px; padding: 30px 24px; width: 100%; max-width: 320px; text-align: center;
      box-shadow: 0 20px 40px rgba(0,0,0,0.5);
    }
    .pin-title { font-size: 1.2rem; margin-bottom: 8px; color: var(--accent); }
    .pin-subtitle { font-size: 0.8rem; color: var(--text-muted); margin-bottom: 20px; }
    .pin-dots { display: flex; justify-content: center; gap: 16px; margin-bottom: 24px; }
    .dot-input { width: 16px; height: 16px; border-radius: 50%; border: 2px solid var(--accent); transition: all 0.2s; }
    .dot-input.filled { background: var(--accent); box-shadow: 0 0 10px var(--accent); }
    .keypad { display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; }
    .key-btn {
      background: rgba(255, 255, 255, 0.05); border: 1px solid rgba(255, 255, 255, 0.1);
      color: var(--text); font-size: 1.3rem; font-weight: 600; padding: 16px; border-radius: 16px;
      cursor: pointer; transition: all 0.1s ease;
    }
    .key-btn:active { transform: scale(0.92); background: rgba(56, 189, 248, 0.2); }
    .key-btn.clear { color: var(--danger); }
    .key-btn.enter { color: var(--switch-on); }
    .shake { animation: shake 0.4s cubic-bezier(.36,.07,.19,.97) both; }
    @keyframes shake {
      10%, 90% { transform: translate3d(-1px, 0, 0); }
      20%, 80% { transform: translate3d(2px, 0, 0); }
      30%, 50%, 70% { transform: translate3d(-4px, 0, 0); }
      40%, 60% { transform: translate3d(4px, 0, 0); }
    }
    .dashboard { width: 100%; max-width: 650px; display: none; }
    .header { text-align: center; margin-bottom: 16px; position: relative; }
    .header h1 { font-size: 1.6rem; color: var(--accent); }
    .connection-status {
      display: inline-flex; align-items: center; gap: 8px; font-size: 0.85rem;
      color: var(--text-muted); margin-top: 6px; background: rgba(0,0,0,0.2); padding: 6px 14px; border-radius: 20px;
    }
    .status-dot { width: 10px; height: 10px; border-radius: 50%; background-color: var(--status-offline); transition: background-color 0.3s; }
    .status-dot.connected { background-color: var(--status-online); }
    .btn-lock {
      position: absolute; right: 0; top: 0; background: rgba(255,255,255,0.08);
      border: 1px solid rgba(255,255,255,0.1); color: var(--text); padding: 8px 12px; border-radius: 10px; cursor: pointer; font-size: 0.8rem;
    }
    .master-actions { display: flex; gap: 12px; margin: 20px 0; }
    .btn-master {
      flex: 1; padding: 14px; font-weight: 700; border-radius: 12px; border: 1px solid rgba(255,255,255,0.1);
      cursor: pointer; transition: transform 0.1s ease;
    }
    .btn-master:active { transform: scale(0.96); }
    .btn-all-on { background: rgba(34, 197, 94, 0.2); color: #4ade80; border-color: rgba(34, 197, 94, 0.4); }
    .btn-all-off { background: rgba(239, 68, 68, 0.2); color: #f87171; border-color: rgba(239, 68, 68, 0.4); }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 16px; }
    .card {
      background: var(--card-bg); backdrop-filter: blur(12px); border: 1px solid rgba(255, 255, 255, 0.08);
      border-radius: 16px; padding: 18px 20px; display: flex; justify-content: space-between; align-items: center;
    }
    .channel-name { font-weight: 600; font-size: 1rem; }
    .channel-status { font-size: 0.75rem; color: var(--text-muted); margin-top: 2px; }
    .switch { position: relative; display: inline-block; width: 54px; height: 28px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: var(--switch-off); transition: .3s; border-radius: 34px; }
    .slider:before { position: absolute; content: ""; height: 22px; width: 22px; left: 3px; bottom: 3px; background-color: white; transition: .3s; border-radius: 50%; }
    input:checked + .slider { background-color: var(--switch-on); }
    input:checked + .slider:before { transform: translateX(26px); }
  </style>
</head>
<body>

  <div id="pin-screen">
    <div class="pin-container" id="pin-card">
      <div class="pin-title">🔒 Acceso Remoto</div>
      <div class="pin-subtitle" id="pin-msg">Ingresa el PIN de seguridad</div>
      <div class="pin-dots">
        <div class="dot-input"></div><div class="dot-input"></div><div class="dot-input"></div><div class="dot-input"></div>
      </div>
      <div class="keypad">
        <button class="key-btn" onclick="pressKey('1')">1</button>
        <button class="key-btn" onclick="pressKey('2')">2</button>
        <button class="key-btn" onclick="pressKey('3')">3</button>
        <button class="key-btn" onclick="pressKey('4')">4</button>
        <button class="key-btn" onclick="pressKey('5')">5</button>
        <button class="key-btn" onclick="pressKey('6')">6</button>
        <button class="key-btn" onclick="pressKey('7')">7</button>
        <button class="key-btn" onclick="pressKey('8')">8</button>
        <button class="key-btn" onclick="pressKey('9')">9</button>
        <button class="key-btn clear" onclick="clearPin()">C</button>
        <button class="key-btn" onclick="pressKey('0')">0</button>
        <button class="key-btn enter" onclick="checkPin()">✓</button>
      </div>
    </div>
  </div>

  <div class="dashboard" id="main-dashboard">
    <div class="header">
      <button class="btn-lock" onclick="lockApp()">🔒 Bloquear</button>
      <h1>Control de Luces ESP32</h1>
      <div class="connection-status">
        <span class="status-dot" id="status-dot"></span>
        <span id="status-text">Conectando...</span>
      </div>
    </div>
    <div class="master-actions">
      <button class="btn-master btn-all-on" onclick="setAll(true)">⚡ ENCENDER TODO</button>
      <button class="btn-master btn-all-off" onclick="setAll(false)">🌙 APAGAR TODO</button>
    </div>
    <div class="grid" id="relay-grid"></div>
  </div>

  <script>
    const PIN_CORRECTO = "1234";
    let currentPin = "";
    const MQTT_BROKER = "broker.hivemq.com";
    const MQTT_PORT = 8884; 
    const TOPIC_CONTROL = "casa/leonardo/reles/control";
    const TOPIC_ESTADO = "casa/leonardo/reles/estado";
    let client = null;

    function pressKey(num) {
      if (currentPin.length < 4) {
        currentPin += num;
        updateDots();
        if (currentPin.length === 4) setTimeout(checkPin, 150);
      }
    }
    function clearPin() {
      currentPin = "";
      updateDots();
      document.getElementById('pin-msg').innerText = "Ingresa el PIN de seguridad";
      document.getElementById('pin-msg').style.color = "var(--text-muted)";
    }
    function updateDots() {
      const dots = document.querySelectorAll('.dot-input');
      dots.forEach((dot, index) => {
        if (index < currentPin.length) dot.classList.add('filled');
        else dot.classList.remove('filled');
      });
    }
    function checkPin() {
      if (currentPin === PIN_CORRECTO) {
        document.getElementById('pin-screen').style.display = 'none';
        document.getElementById('main-dashboard').style.display = 'block';
        if (!client) iniciarMQTT();
      } else {
        const card = document.getElementById('pin-card');
        card.classList.add('shake');
        document.getElementById('pin-msg').innerText = "¡PIN Incorrecto!";
        document.getElementById('pin-msg').style.color = "var(--danger)";
        setTimeout(() => { card.classList.remove('shake'); clearPin(); }, 500);
      }
    }
    function lockApp() {
      clearPin();
      document.getElementById('main-dashboard').style.display = 'none';
      document.getElementById('pin-screen').style.display = 'flex';
    }

    const grid = document.getElementById('relay-grid');
    for (let i = 1; i <= 8; i++) {
      grid.innerHTML += `
        <div class="card">
          <div>
            <div class="channel-name">Luz ${i}</div>
            <div class="channel-status" id="st-${i}">Desconectado</div>
          </div>
          <label class="switch">
            <input type="checkbox" id="sw-${i}" onchange="toggleRelay(${i})">
            <span class="slider"></span>
          </label>
        </div>`;
    }

    function iniciarMQTT() {
      const clientId = "WebClient_" + Math.random().toString(16).substr(2, 8);
      client = new Paho.MQTT.Client(MQTT_BROKER, MQTT_PORT, "/mqtt", clientId);

      client.onConnectionLost = () => {
        document.getElementById('status-dot').classList.remove('connected');
        document.getElementById('status-text').innerText = "Reconectando...";
        setTimeout(conectar, 3000);
      };

      client.onMessageArrived = (message) => {
        try {
          const data = JSON.parse(message.payloadString);
          for (let i = 1; i <= 8; i++) {
            const key = 'r' + i;
            if (data.hasOwnProperty(key)) {
              const isChecked = data[key] === 1;
              document.getElementById(`sw-${i}`).checked = isChecked;
              document.getElementById(`st-${i}`).innerText = isChecked ? "Encendido" : "Apagado";
            }
          }
        } catch (e) { console.error("Error JSON:", e); }
      };
      conectar();
    }

    function conectar() {
      client.connect({
        onSuccess: () => {
          document.getElementById('status-dot').classList.add('connected');
          document.getElementById('status-text').innerText = "Conectado globalmente";
          client.subscribe(TOPIC_ESTADO);
        },
        onFailure: () => setTimeout(conectar, 3000),
        useSSL: true
      });
    }

    function toggleRelay(id) {
      if (!client || !client.isConnected()) return;
      const isChecked = document.getElementById(`sw-${id}`).checked;
      const payload = JSON.stringify({ rele: id, estado: isChecked ? 1 : 0 });
      const message = new Paho.MQTT.Message(payload);
      message.destinationName = TOPIC_CONTROL;
      client.send(message);
    }

    function setAll(state) {
      if (!client || !client.isConnected()) return;
      const payload = JSON.stringify({ rele: 0, estado: state ? 1 : 0 });
      const message = new Paho.MQTT.Message(payload);
      message.destinationName = TOPIC_CONTROL;
      client.send(message);
    }
  </script>
</body>
</html>
)rawliteral";

// ================================================================
// FUNCIONES DE CONTROL DE RELÉS Y PUBLICACIÓN MQTT
// ================================================================
void publicarEstado() {
  StaticJsonDocument<256> doc;
  for (int i = 0; i < 8; i++) {
    String key = "r" + String(i + 1);
    doc[key] = relayStates[i] ? 1 : 0;
  }
  
  char buffer[256];
  serializeJson(doc, buffer);
  mqttClient.publish(TOPIC_ESTADO, buffer, true);
}

void callbackMQTT(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  
  if (error) return;

  int rele = doc["rele"];
  int estado = doc["estado"];

  if (rele >= 1 && rele <= 8) {
    int idx = rele - 1;
    relayStates[idx] = (estado == 1);
    digitalWrite(RELAY_PINS[idx], relayStates[idx] ? HIGH : LOW);
  } else if (rele == 0) {
    for (int i = 0; i < 8; i++) {
      relayStates[i] = (estado == 1);
      digitalWrite(RELAY_PINS[i], relayStates[i] ? HIGH : LOW);
    }
  }

  publicarEstado();
}

void reconectarMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando a broker MQTT...");
    String clientId = "ESP32_Control_" + String(random(0xffff), HEX);
    
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println(" OK!");
      mqttClient.subscribe(TOPIC_CONTROL);
      publicarEstado();
    } else {
      Serial.print(" Falló rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Reintentando en 5s...");
      delay(5000);
    }
  }
}

// ================================================================
// SETUP Y LOOP
// ================================================================
void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 8; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\n¡Wi-Fi Conectado!");
  Serial.print("Dirección IP local: ");
  Serial.println(WiFi.localIP());

  // Servidor Web para alojar la interfaz en la red local
  server.on("/", []() {
    server.send(200, "text/html", INDEX_HTML);
  });
  server.begin();

  // Cliente MQTT
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(callbackMQTT);
}

void loop() {
  server.handleClient();

  if (!mqttClient.connected()) {
    reconectarMQTT();
  }
  mqttClient.loop();
}