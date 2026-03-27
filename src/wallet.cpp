#include "wallet.h"
#include "config.h"
#include <Preferences.h>
#include <esp_random.h>

/**
 * Walletless Device Identity
 * No private keys, no signing, no micro-ecc.
 * Just a random UUID for hardware identification.
 * User identity = Web3Auth session via Grudge Account.
 */

static Preferences prefs;
static const char* NVS_KEY_UUID = "device_uuid";

/* ── Generate a random hex UUID ──────────────────── */
static String _generate_uuid() {
    uint8_t buf[16];
    esp_fill_random(buf, 16);
    /* Set version 4 (random) UUID bits */
    buf[6] = (buf[6] & 0x0F) | 0x40;
    buf[8] = (buf[8] & 0x3F) | 0x80;
    char hex[37];
    snprintf(hex, sizeof(hex),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             buf[0], buf[1], buf[2], buf[3],
             buf[4], buf[5], buf[6], buf[7],
             buf[8], buf[9], buf[10], buf[11],
             buf[12], buf[13], buf[14], buf[15]);
    return String(hex);
}

String wallet_pubkey_hex(const GrudaWallet& w) {
    return w.deviceUUID;
}

/* ── Init: load or generate device UUID ───────────── */
bool wallet_init(GrudaWallet& w) {
    prefs.begin(NVS_NAMESPACE, false);

    String uuid = prefs.getString(NVS_KEY_UUID, "");
    if (uuid.length() > 0) {
        w.deviceUUID = uuid;
        w.publicKeyHex = uuid;
        w.initialized = true;
        prefs.end();
        Serial.printf("[DEVICE] UUID loaded: %s\n", uuid.c_str());
        return true;
    }

    /* Generate new UUID */
    uuid = _generate_uuid();
    prefs.putString(NVS_KEY_UUID, uuid);
    prefs.end();

    w.deviceUUID = uuid;
    w.publicKeyHex = uuid;
    w.initialized = true;
    Serial.printf("[DEVICE] New UUID generated: %s\n", uuid.c_str());
    return true;
}

/* ── Factory reset ──────────────────────────────── */
void wallet_wipe() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    Serial.println("[DEVICE] Identity wiped from NVS");
}
