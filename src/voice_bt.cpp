#include "voice_bt.h"
#include "config.h"

#include <NimBLEDevice.h>
#include <ArduinoJson.h>

/**
 * BLE GATT server using NimBLE (lightweight Bluetooth LE stack).
 * Implements Nordic UART Service (NUS) pattern:
 *   - RX characteristic: phone writes transcribed text here
 *   - TX characteristic: ESP32 notifies AI responses back
 */

static VoiceBTState*    _state = nullptr;
static VoiceBTCallback  _callback = nullptr;
static NimBLEServer*    _server = nullptr;
static NimBLECharacteristic* _txChar = nullptr;
static NimBLECharacteristic* _rxChar = nullptr;
static String           _deviceName;

/* ── Server Callbacks ─────────────────────────────── */
class GrudaServerCB : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
        Serial.printf("[BLE] Client connected: %s\n",
                      NimBLEAddress(desc->peer_ota_addr).toString().c_str());
        if (_state) {
            _state->clientConnected = true;
            _state->clientName = NimBLEAddress(desc->peer_ota_addr).toString().c_str();
        }
        /* Allow multiple connections — keep advertising */
        NimBLEDevice::startAdvertising();
    }

    void onDisconnect(NimBLEServer* pServer) override {
        Serial.println("[BLE] Client disconnected");
        if (_state) {
            _state->clientConnected = false;
            _state->clientName = "";
        }
        NimBLEDevice::startAdvertising();
    }
};

/* ── RX Characteristic Callback (phone → ESP32) ──── */
class GrudaRxCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar) override {
        std::string val = pChar->getValue();
        if (val.empty()) return;

        String text = String(val.c_str());
        text.trim();
        if (text.length() == 0) return;

        Serial.printf("[BLE] RX: %s\n", text.c_str());

        if (_state) {
            _state->lastCommand = text;
            _state->messageCount++;
        }

        /* Fire callback to AI admin router */
        if (_callback) {
            _callback(text);
        }
    }
};

/* ── Public API ───────────────────────────────────── */

void voice_bt_init(VoiceBTState& state, const String& suffix) {
    _state = &state;
    state.initialized = false;
    state.clientConnected = false;
    state.clientName = "";
    state.lastCommand = "";
    state.lastResponse = "";
    state.messageCount = 0;

    /* Build device name: GRUDA-Node-<suffix> */
    _deviceName = String(BLE_DEVICE_NAME_PREFIX) + suffix;

    Serial.printf("[BLE] Initializing as '%s'\n", _deviceName.c_str());

    /* Init NimBLE */
    NimBLEDevice::init(_deviceName.c_str());
    NimBLEDevice::setMTU(BLE_MTU);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /* max range */

    /* Create GATT server */
    _server = NimBLEDevice::createServer();
    _server->setCallbacks(new GrudaServerCB());

    /* Create service (NUS UUIDs) */
    NimBLEService* svc = _server->createService(BLE_SERVICE_UUID);

    /* TX characteristic — ESP32 → Phone (notify) */
    _txChar = svc->createCharacteristic(
        BLE_CHAR_TX_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    /* RX characteristic — Phone → ESP32 (write) */
    _rxChar = svc->createCharacteristic(
        BLE_CHAR_RX_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    _rxChar->setCallbacks(new GrudaRxCB());

    /* Start service */
    svc->start();

    state.initialized = true;
    Serial.println("[BLE] GATT server ready");
}

void voice_bt_set_callback(VoiceBTCallback cb) {
    _callback = cb;
}

void voice_bt_start_advertising() {
    if (!_server) return;

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SERVICE_UUID);
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06); /* connection interval hint */
    adv->setMaxPreferred(0x12);
    adv->start();

    Serial.printf("[BLE] Advertising as '%s'\n", _deviceName.c_str());
}

void voice_bt_stop_advertising() {
    NimBLEDevice::getAdvertising()->stop();
    Serial.println("[BLE] Advertising stopped");
}

bool voice_bt_send(const String& text) {
    if (!_txChar || !_state || !_state->clientConnected) return false;

    /* Chunk if needed (BLE MTU limit) */
    size_t len = text.length();
    size_t maxChunk = BLE_MAX_MSG_LEN;

    if (len <= maxChunk) {
        _txChar->setValue((const uint8_t*)text.c_str(), len);
        _txChar->notify();
    } else {
        /* Send in chunks for long responses */
        for (size_t i = 0; i < len; i += maxChunk) {
            size_t chunk = min(maxChunk, len - i);
            _txChar->setValue((const uint8_t*)(text.c_str() + i), chunk);
            _txChar->notify();
            delay(20); /* small gap between chunks */
        }
    }

    if (_state) _state->lastResponse = text;
    return true;
}

bool voice_bt_send_json(const char* type, const String& text) {
    JsonDocument doc;
    doc["type"] = type;
    doc["text"] = text;
    doc["ts"]   = millis();

    String out;
    serializeJson(doc, out);
    return voice_bt_send(out);
}

bool voice_bt_is_connected() {
    return _state && _state->clientConnected;
}
