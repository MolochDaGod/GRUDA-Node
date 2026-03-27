#include "device_api.h"
#include "config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>

static Preferences devPrefs;
static String _deviceToken;

String device_get_saved_token() {
    devPrefs.begin(NVS_NAMESPACE, true);
    String token = devPrefs.getString(NVS_KEY_DEVTOKEN, "");
    devPrefs.end();
    return token;
}

static void _save_token(const String& token) {
    devPrefs.begin(NVS_NAMESPACE, false);
    devPrefs.putString(NVS_KEY_DEVTOKEN, token);
    devPrefs.end();
    _deviceToken = token;
}

bool device_register(const GrudaWallet& wallet, const String& grudgeId) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    String url = String("https://") + ID_HOST + DEVICE_REGISTER_PATH;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(8000);

    JsonDocument doc;
    doc["publicKey"]       = wallet.publicKeyHex;
    doc["firmwareVersion"] = GRUDA_VERSION;
    doc["hardwareType"]    = "ESP32-GRD17";
    doc["deviceName"]      = "GRUDA Node";
    if (grudgeId.length() > 0) {
        doc["grudgeId"] = grudgeId;
    }

    String payload;
    serializeJson(doc, payload);

    int code = http.POST(payload);
    if (code != 200 && code != 201) {
        Serial.printf("[DEVICE] Register failed: HTTP %d\n", code);
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();

    JsonDocument resp;
    if (deserializeJson(resp, body)) {
        Serial.println("[DEVICE] Register response parse error");
        return false;
    }

    String token = resp["token"].as<String>();
    if (token.length() > 0) {
        _save_token(token);
        Serial.printf("[DEVICE] Registered! Token: %s...\n", token.substring(0, 16).c_str());
    }

    bool isNew = resp["isNew"] | false;
    Serial.printf("[DEVICE] %s on backend\n", isNew ? "NEW device" : "Re-registered");
    return true;
}

bool device_heartbeat(const GrudaWallet& wallet, uint32_t blockHeight,
                      uint32_t uptime, uint8_t peers) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    String url = String("https://") + ID_HOST + DEVICE_HEARTBEAT_PATH;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);

    /* Use device token if we have one */
    if (_deviceToken.length() > 0) {
        http.addHeader("Authorization", "Bearer " + _deviceToken);
    }

    JsonDocument doc;
    doc["publicKey"]   = wallet.publicKeyHex;
    doc["blockHeight"] = blockHeight;
    doc["uptime"]      = uptime;
    doc["peers"]       = peers;
    doc["status"]      = "online";

    String payload;
    serializeJson(doc, payload);

    int code = http.POST(payload);
    http.end();

    if (code != 200) {
        Serial.printf("[DEVICE] Heartbeat failed: HTTP %d\n", code);
        return false;
    }
    return true;
}
