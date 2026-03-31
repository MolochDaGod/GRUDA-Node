#ifndef GRUDA_VOICE_BT_H
#define GRUDA_VOICE_BT_H

/**
 * GRUDA Voice BLE Service
 *
 * NimBLE GATT server that accepts transcribed text from the mobile app
 * and sends AI responses back. Uses Nordic UART Service (NUS) UUIDs
 * for broad compatibility with BLE terminal apps.
 *
 * Flow:
 *   Phone mic → STT → BLE write (RX char) → ESP32
 *   ESP32 → AI response → BLE notify (TX char) → Phone TTS
 *
 * Created by RacAlvin The Pirate King for GRUDGE STUDIO
 */

#include <Arduino.h>

/* BLE connection state */
struct VoiceBTState {
    bool    initialized;
    bool    clientConnected;
    String  clientName;       /* BLE client device name if available */
    String  lastCommand;      /* last text received from phone */
    String  lastResponse;     /* last AI response sent back */
    uint32_t messageCount;    /* total messages received */
};

/* Callback type — called when text arrives from phone via BLE */
typedef void (*VoiceBTCallback)(const String& text);

/* ── Lifecycle ─────────────────────────────────────── */

/* Initialize BLE GATT server. suffix = last chars of grudgeId for device name */
void voice_bt_init(VoiceBTState& state, const String& suffix);

/* Set callback for incoming text (transcribed voice from phone) */
void voice_bt_set_callback(VoiceBTCallback cb);

/* Start BLE advertising (call after init) */
void voice_bt_start_advertising();

/* Stop BLE advertising */
void voice_bt_stop_advertising();

/* ── Messaging ─────────────────────────────────────── */

/* Send text back to phone via BLE notify (AI response, status, etc.) */
bool voice_bt_send(const String& text);

/* Send a JSON-formatted response with type tag */
bool voice_bt_send_json(const char* type, const String& text);

/* ── Queries ───────────────────────────────────────── */

bool voice_bt_is_connected();

#endif /* GRUDA_VOICE_BT_H */
