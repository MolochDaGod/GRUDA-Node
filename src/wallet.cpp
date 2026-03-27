#include "wallet.h"
#include "config.h"
#include <Preferences.h>
#include <uECC.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>

static Preferences prefs;

/* ── RNG callback for micro-ecc ───────────────────── */
static int _rng_cb(uint8_t* dest, unsigned size) {
    esp_fill_random(dest, size);
    return 1;
}

/* ── Hex helper ───────────────────────────────────── */
String wallet_pubkey_hex(const GrudaWallet& w) {
    char hex[129];
    for (int i = 0; i < 64; i++) {
        sprintf(hex + i * 2, "%02x", w.publicKey[i]);
    }
    hex[128] = '\0';
    return String(hex);
}

/* ── Init: load or generate ───────────────────────── */
bool wallet_init(GrudaWallet& w) {
    uECC_set_rng(_rng_cb);
    const struct uECC_Curve_t* curve = uECC_secp256k1();

    prefs.begin(NVS_NAMESPACE, false);

    /* Try to load existing keypair */
    size_t privLen = prefs.getBytes(NVS_KEY_PRIVKEY, w.privateKey, 32);
    size_t pubLen  = prefs.getBytes(NVS_KEY_PUBKEY, w.publicKey, 64);

    if (privLen == 32 && pubLen == 64) {
        Serial.println("[WALLET] Loaded keypair from NVS");
        w.publicKeyHex = wallet_pubkey_hex(w);
        w.initialized = true;
        prefs.end();
        return true;
    }

    /* Generate new keypair */
    Serial.println("[WALLET] Generating new keypair...");
    if (!uECC_make_key(w.publicKey, w.privateKey, curve)) {
        Serial.println("[WALLET] ERROR: Key generation failed");
        w.initialized = false;
        prefs.end();
        return false;
    }

    /* Persist to NVS */
    prefs.putBytes(NVS_KEY_PRIVKEY, w.privateKey, 32);
    prefs.putBytes(NVS_KEY_PUBKEY, w.publicKey, 64);
    prefs.end();

    w.publicKeyHex = wallet_pubkey_hex(w);
    w.initialized = true;
    Serial.printf("[WALLET] New keypair generated. Pub: %s\n", w.publicKeyHex.c_str());
    return true;
}

/* ── Sign ─────────────────────────────────────────── */
bool wallet_sign(const GrudaWallet& w, const uint8_t* msg, size_t msgLen,
                 uint8_t sig[64]) {
    if (!w.initialized) return false;

    /* Hash the message first (sign the hash, not raw data) */
    uint8_t hash[32];
    mbedtls_sha256(msg, msgLen, hash, 0);

    const struct uECC_Curve_t* curve = uECC_secp256k1();
    return uECC_sign(w.privateKey, hash, 32, sig, curve) == 1;
}

/* ── Verify ───────────────────────────────────────── */
bool wallet_verify(const uint8_t pubkey[64], const uint8_t* msg, size_t msgLen,
                   const uint8_t sig[64]) {
    uint8_t hash[32];
    mbedtls_sha256(msg, msgLen, hash, 0);

    const struct uECC_Curve_t* curve = uECC_secp256k1();
    return uECC_verify(pubkey, hash, 32, sig, curve) == 1;
}

/* ── Factory reset ────────────────────────────────── */
void wallet_wipe() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.remove(NVS_KEY_PRIVKEY);
    prefs.remove(NVS_KEY_PUBKEY);
    prefs.end();
    Serial.println("[WALLET] Keypair wiped from NVS");
}
