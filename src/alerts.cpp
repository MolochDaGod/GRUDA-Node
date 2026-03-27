#include "alerts.h"
#include "config.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

static WebSocketsClient wsAlerts;
static AlertState* _aState = nullptr;
static String _aGrudgeId;
static String _aAuthToken;

/* ── Map event string to AlertType ────────────────── */
static AlertType _parseType(const char* t) {
    if (!t) return ALERT_SYSTEM;
    if (strcmp(t, "crew_invite") == 0)    return ALERT_CREW_INVITE;
    if (strcmp(t, "pvp_challenge") == 0)  return ALERT_PVP_CHALLENGE;
    if (strcmp(t, "faction_event") == 0)  return ALERT_FACTION_EVENT;
    if (strcmp(t, "gold_tx") == 0)        return ALERT_GOLD_TX;
    if (strcmp(t, "gouldstone") == 0)     return ALERT_GOULDSTONE;
    if (strcmp(t, "mission") == 0)        return ALERT_MISSION;
    if (strcmp(t, "chain_vote") == 0)     return ALERT_CHAIN_VOTE;
    return ALERT_SYSTEM;
}

/* ── WebSocket event handler ──────────────────────── */
static void _ws_event(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            Serial.println("[ALERTS] Connected");
            if (_aState) _aState->connected = true;
            {
                JsonDocument doc;
                doc["event"] = "auth";
                doc["grudgeId"] = _aGrudgeId;
                doc["token"] = _aAuthToken;
                doc["ns"] = WS_NS_ALERTS;
                String out;
                serializeJson(doc, out);
                wsAlerts.sendTXT(out);
            }
            break;

        case WStype_DISCONNECTED:
            Serial.println("[ALERTS] Disconnected");
            if (_aState) _aState->connected = false;
            break;

        case WStype_TEXT:
            {
                JsonDocument doc;
                if (deserializeJson(doc, payload, length)) break;

                const char* event = doc["event"];
                if (event && strcmp(event, "alert") == 0) {
                    if (_aState && _aState->alertCount < MAX_ALERTS) {
                        Alert& a = _aState->alerts[_aState->alertCount];
                        a.type      = _parseType(doc["alertType"]);
                        a.title     = doc["title"].as<String>();
                        a.body      = doc["body"].as<String>();
                        a.timestamp = doc["ts"] | (uint32_t)millis();
                        a.read      = false;
                        _aState->alertCount++;
                        Serial.printf("[ALERTS] New: %s\n", a.title.c_str());
                    }
                }
            }
            break;

        default:
            break;
    }
}

/* ── Public API ───────────────────────────────────── */

void alerts_init(AlertState& state) {
    _aState = &state;
    state.alertCount = 0;
    state.connected = false;
}

void alerts_connect(const String& grudgeId, const String& authToken) {
    _aGrudgeId = grudgeId;
    _aAuthToken = authToken;
    wsAlerts.beginSSL(WS_HOST, WS_PORT, WS_NS_ALERTS);
    wsAlerts.onEvent(_ws_event);
    wsAlerts.setReconnectInterval(RECONNECT_INTERVAL_MS);
}

void alerts_disconnect() {
    wsAlerts.disconnect();
    if (_aState) _aState->connected = false;
}

void alerts_loop() {
    wsAlerts.loop();
}

uint8_t alerts_unread_count(const AlertState& state) {
    uint8_t c = 0;
    for (uint8_t i = 0; i < state.alertCount; i++) {
        if (!state.alerts[i].read) c++;
    }
    return c;
}

void alerts_mark_all_read(AlertState& state) {
    for (uint8_t i = 0; i < state.alertCount; i++) {
        state.alerts[i].read = true;
    }
}
