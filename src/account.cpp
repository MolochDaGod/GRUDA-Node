#include "account.h"
#include "config.h"
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_random.h>

static Preferences acctPrefs;

/* ── NVS Keys ─────────────────────────────────────── */
static const char* NVS_ACCT_NS       = "gruda_acct";
static const char* NVS_ACCT_GID      = "grudge_id";
static const char* NVS_ACCT_NAME     = "disp_name";
static const char* NVS_ACCT_TOKEN    = "auth_token";
static const char* NVS_ACCT_EXPIRES  = "expires_at";

/* ── Pairing code character set ───────────────────── */
static const char PAIR_CHARS[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
static const int  PAIR_CHARS_LEN = sizeof(PAIR_CHARS) - 1;

/* ── Init ─────────────────────────────────────────── */
void account_init(GrudgeAccount& acct) {
    acct.grudgeId    = "";
    acct.displayName = "";
    acct.authToken   = "";
    acct.expiresAt   = 0;
    acct.loggedIn    = false;
    Serial.println("[ACCT] Account system initialized");
}

/* ── Resume from NVS ──────────────────────────────── */
bool account_resume(GrudgeAccount& acct) {
    acctPrefs.begin(NVS_ACCT_NS, true); /* read-only */

    String gid   = acctPrefs.getString(NVS_ACCT_GID, "");
    String name  = acctPrefs.getString(NVS_ACCT_NAME, "");
    String token = acctPrefs.getString(NVS_ACCT_TOKEN, "");
    uint32_t exp = acctPrefs.getUInt(NVS_ACCT_EXPIRES, 0);

    acctPrefs.end();

    if (gid.length() == 0 || token.length() == 0) {
        Serial.println("[ACCT] No saved session found");
        return false;
    }

    /* Basic expiry check — if we have NTP time we can be precise,
       otherwise we trust the saved session until the backend rejects it */
    acct.grudgeId    = gid;
    acct.displayName = name;
    acct.authToken   = token;
    acct.expiresAt   = exp;
    acct.loggedIn    = true;

    Serial.printf("[ACCT] Resumed session: %s (%s)\n",
                  acct.displayName.c_str(), acct.grudgeId.c_str());
    return true;
}

/* ── Save session to NVS ──────────────────────────── */
static void _save_session(const GrudgeAccount& acct) {
    acctPrefs.begin(NVS_ACCT_NS, false);
    acctPrefs.putString(NVS_ACCT_GID,     acct.grudgeId);
    acctPrefs.putString(NVS_ACCT_NAME,    acct.displayName);
    acctPrefs.putString(NVS_ACCT_TOKEN,   acct.authToken);
    acctPrefs.putUInt(NVS_ACCT_EXPIRES,   acct.expiresAt);
    acctPrefs.end();
    Serial.println("[ACCT] Session saved to NVS");
}

/* ── Generate Pairing Code ────────────────────────── */
void account_generate_pairing_code(DevicePairingCode& pc) {
    for (int i = 0; i < DEVICE_PAIRING_CODE_LEN; i++) {
        uint32_t r = esp_random() % PAIR_CHARS_LEN;
        pc.code[i] = PAIR_CHARS[r];
    }
    pc.code[DEVICE_PAIRING_CODE_LEN] = '\0';
    pc.createdAt = millis();
    pc.active = true;
    Serial.printf("[ACCT] Pairing code: %s\n", pc.code);
}

/* ── Poll Backend for Auth ────────────────────────── */
bool account_poll_auth(GrudgeAccount& acct, const DevicePairingCode& pc) {
    if (!pc.active) return false;

    HTTPClient http;
    String url = String("https://") + API_HOST + DEVICE_AUTH_POLL_PATH +
                 "?code=" + String(pc.code);

    http.begin(url);
    http.addHeader("Accept", "application/json");
    http.setTimeout(5000);

    int code = http.GET();
    if (code != 200) {
        http.end();
        return false; /* Not yet claimed, or error — keep polling */
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[ACCT] Poll parse error: %s\n", err.c_str());
        return false;
    }

    /* Check if backend returned a valid session */
    const char* status = doc["status"];
    if (!status || strcmp(status, "authorized") != 0) {
        return false; /* Still pending */
    }

    acct.grudgeId    = doc["grudgeId"].as<String>();
    acct.displayName = doc["displayName"].as<String>();
    acct.authToken   = doc["token"].as<String>();
    acct.expiresAt   = doc["expiresAt"] | 0;
    acct.loggedIn    = true;

    _save_session(acct);

    Serial.printf("[ACCT] Logged in as %s (%s)\n",
                  acct.displayName.c_str(), acct.grudgeId.c_str());
    return true;
}

/* ── Logout ───────────────────────────────────────── */
void account_logout(GrudgeAccount& acct) {
    /* Notify backend (best-effort) */
    if (acct.authToken.length() > 0) {
        HTTPClient http;
        String url = String("https://") + API_HOST + "/device/auth/logout";
        http.begin(url);
        http.addHeader("Authorization", "Bearer " + acct.authToken);
        http.POST("");
        http.end();
    }

    /* Wipe NVS */
    acctPrefs.begin(NVS_ACCT_NS, false);
    acctPrefs.clear();
    acctPrefs.end();

    /* Reset state */
    acct.grudgeId    = "";
    acct.displayName = "";
    acct.authToken   = "";
    acct.expiresAt   = 0;
    acct.loggedIn    = false;

    Serial.println("[ACCT] Logged out — session wiped");
}

/* ── Queries ──────────────────────────────────────── */

bool account_is_logged_in(const GrudgeAccount& acct) {
    return acct.loggedIn && acct.authToken.length() > 0;
}

const char* account_get_token(const GrudgeAccount& acct) {
    return acct.authToken.c_str();
}

const char* account_get_display_name(const GrudgeAccount& acct) {
    return acct.displayName.length() > 0
           ? acct.displayName.c_str()
           : "Guest";
}

const char* account_get_grudge_id(const GrudgeAccount& acct) {
    return acct.grudgeId.c_str();
}

bool account_session_expired(const GrudgeAccount& acct) {
    if (acct.expiresAt == 0) return false; /* No expiry set — trust it */
    /* Without NTP we can't do a real check; the backend will reject
       expired tokens and the device will fall back to login screen */
    return false;
}

/* ── Provision new account directly from device ────── */
bool account_provision(GrudgeAccount& acct, const String& displayName,
                       const String& deviceUUID) {
    HTTPClient http;
    String url = String("https://") + API_HOST + DEVICE_PROVISION_PATH;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(8000);

    JsonDocument doc;
    doc["deviceUUID"]       = deviceUUID;
    doc["displayName"]      = displayName;
    doc["hardwareType"]     = "ESP32-GRD17";
    doc["firmwareVersion"]  = GRUDA_VERSION;

    String payload;
    serializeJson(doc, payload);

    int code = http.POST(payload);
    if (code != 200 && code != 201) {
        Serial.printf("[ACCT] Provision failed: HTTP %d\n", code);
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();

    JsonDocument resp;
    if (deserializeJson(resp, body)) {
        Serial.println("[ACCT] Provision response parse error");
        return false;
    }

    acct.grudgeId    = resp["grudgeId"].as<String>();
    acct.displayName = resp["displayName"].as<String>();
    acct.authToken   = resp["token"].as<String>();
    acct.expiresAt   = resp["expiresAt"] | 0;
    acct.loggedIn    = true;

    _save_session(acct);

    Serial.printf("[ACCT] Provisioned new account: %s (%s)\n",
                  acct.displayName.c_str(), acct.grudgeId.c_str());
    return true;
}

/* ── Factory Reset ────────────────────────────────── */
void account_factory_reset() {
    /* Wipe account session */
    Preferences p;
    p.begin(NVS_ACCT_NS, false);
    p.clear();
    p.end();

    /* Wipe WiFi credentials */
    p.begin(NVS_WIFI_NS, false);
    p.clear();
    p.end();

    /* Wipe device identity */
    p.begin(NVS_NAMESPACE, false);
    p.clear();
    p.end();

    Serial.println("[ACCT] Factory reset — all NVS data wiped");
}
