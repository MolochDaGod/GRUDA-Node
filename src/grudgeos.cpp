#include "grudgeos.h"
#include "config.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

/**
 * GrudgeOS Device Agent — connects ESP32 to the GrudgeOS desktop
 * at puter-monitor-ai.onrender.com as a live hardware agent.
 *
 * Protocol: JSON over WebSocket
 *   → { "event": "agent:register", "agent": {...} }
 *   → { "event": "agent:heartbeat", "data": {...} }
 *   → { "event": "agent:status", "data": {...} }
 *   ← { "event": "command", "action": "reboot"|"config"|... }
 */

#define GRUDGEOS_HOST "puter-monitor-ai.onrender.com"
#define GRUDGEOS_PORT 443
#define GRUDGEOS_PATH "/ws"
#define GRUDGEOS_HEARTBEAT_MS 30000
#define GRUDGEOS_RECONNECT_MS 15000

static WebSocketsClient _wsOS;
static GrudgeOSState*   _osState = nullptr;
static String           _grudgeId;
static String           _authToken;
static String           _deviceKey;

/* ── Send JSON helper ─────────────────────────────── */
static void _send_json(JsonDocument& doc) {
    String msg;
    serializeJson(doc, msg);
    _wsOS.sendTXT(msg);
    if (_osState) _osState->messageCount++;
}

/* ── Register device as agent ─────────────────────── */
static void _register_agent() {
    if (!_osState) return;

    JsonDocument doc;
    doc["event"] = "agent:register";

    JsonObject agent = doc["agent"].to<JsonObject>();
    agent["type"]      = "device";
    agent["platform"]  = "ESP32-GRD17";
    agent["firmware"]  = GRUDA_VERSION;
    agent["grudgeId"]  = _grudgeId;
    agent["deviceKey"] = _deviceKey;
    agent["name"]      = "GRUDA Node";
    agent["capabilities"][0] = "chain-validator";
    agent["capabilities"][1] = "nft-display";
    agent["capabilities"][2] = "treaty-relay";
    agent["capabilities"][3] = "wallet-signer";
    agent["heap"]      = ESP.getFreeHeap();
    agent["chipModel"] = ESP.getChipModel();
    agent["cpuFreq"]   = ESP.getCpuFreqMHz();

    _send_json(doc);
    Serial.println("[GRUDGEOS] Agent registration sent");
}

/* ── Handle incoming commands ─────────────────────── */
static void _handle_message(const char* payload) {
    JsonDocument doc;
    if (deserializeJson(doc, payload)) return;

    const char* event = doc["event"];
    if (!event) return;

    if (strcmp(event, "agent:registered") == 0) {
        if (_osState) {
            _osState->registered = true;
            _osState->agentId = doc["agentId"].as<const char*>();
            _osState->sessionId = doc["sessionId"].as<const char*>();
        }
        Serial.printf("[GRUDGEOS] Registered as agent: %s\n",
                      _osState->agentId.c_str());
    }
    else if (strcmp(event, "command") == 0) {
        const char* action = doc["action"];
        if (!action) return;

        if (strcmp(action, "reboot") == 0) {
            Serial.println("[GRUDGEOS] Reboot command received");
            delay(500);
            ESP.restart();
        }
        else if (strcmp(action, "status") == 0) {
            /* Remote status request — send back heap + uptime */
            grudgeos_send_status(*_osState,
                0, millis() / 1000, 0, ESP.getFreeHeap());
        }
        else {
            Serial.printf("[GRUDGEOS] Unknown command: %s\n", action);
        }
    }
    else if (strcmp(event, "ping") == 0) {
        JsonDocument pong;
        pong["event"] = "pong";
        _send_json(pong);
    }
}

/* ── WebSocket event handler ──────────────────────── */
static void _ws_event(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            Serial.println("[GRUDGEOS] Connected to GrudgeOS");
            if (_osState) _osState->connected = true;
            _register_agent();
            break;

        case WStype_DISCONNECTED:
            Serial.println("[GRUDGEOS] Disconnected");
            if (_osState) {
                _osState->connected = false;
                _osState->registered = false;
            }
            break;

        case WStype_TEXT:
            _handle_message((const char*)payload);
            break;

        default:
            break;
    }
}

/* ── Public API ───────────────────────────────────── */

void grudgeos_init(GrudgeOSState& state) {
    state.connected = false;
    state.registered = false;
    state.agentId = "";
    state.sessionId = "";
    state.lastHeartbeat = 0;
    state.messageCount = 0;
}

void grudgeos_connect(GrudgeOSState& state, const String& grudgeId,
                      const String& authToken, const String& deviceKey) {
    _osState = &state;
    _grudgeId = grudgeId;
    _authToken = authToken;
    _deviceKey = deviceKey;

    _wsOS.beginSSL(GRUDGEOS_HOST, GRUDGEOS_PORT, GRUDGEOS_PATH);
    _wsOS.onEvent(_ws_event);
    _wsOS.setReconnectInterval(GRUDGEOS_RECONNECT_MS);

    /* Auth header for initial handshake */
    String authHeader = "Authorization: Bearer " + authToken;
    _wsOS.setExtraHeaders(authHeader.c_str());

    Serial.printf("[GRUDGEOS] Connecting to %s...\n", GRUDGEOS_HOST);
}

void grudgeos_disconnect(GrudgeOSState& state) {
    _wsOS.disconnect();
    state.connected = false;
    state.registered = false;
}

void grudgeos_loop(GrudgeOSState& state) {
    _wsOS.loop();

    /* Auto-heartbeat every 30s when registered */
    if (state.registered && (millis() - state.lastHeartbeat > GRUDGEOS_HEARTBEAT_MS)) {
        state.lastHeartbeat = millis();
        grudgeos_send_status(state, 0, millis() / 1000, 0, ESP.getFreeHeap());
    }
}

void grudgeos_send_status(GrudgeOSState& state,
                          uint32_t blockHeight, uint32_t uptime,
                          uint8_t peers, uint32_t freeHeap) {
    if (!state.connected) return;

    JsonDocument doc;
    doc["event"] = "agent:status";
    JsonObject data = doc["data"].to<JsonObject>();
    data["blockHeight"] = blockHeight;
    data["uptime"]      = uptime;
    data["peers"]       = peers;
    data["freeHeap"]    = freeHeap;
    data["firmware"]    = GRUDA_VERSION;

    _send_json(doc);
}

void grudgeos_send_event(GrudgeOSState& state,
                         const char* eventType, const String& payload) {
    if (!state.connected) return;

    JsonDocument doc;
    doc["event"] = "agent:event";
    doc["type"]  = eventType;
    doc["data"]  = payload;

    _send_json(doc);
}
