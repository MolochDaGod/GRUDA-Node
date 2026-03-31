#ifndef GRUDA_ACCOUNT_H
#define GRUDA_ACCOUNT_H

/**
 * Grudge Account — Device Auth Module
 * Allows any Grudge user to log in on this device via pairing code.
 * Session stored in NVS; device wallet remains separate (signing key).
 * Created by RacAlvin The Pirate King for GRUDGE STUDIO
 */

#include <Arduino.h>

/* ── Account State ────────────────────────────────── */
struct GrudgeAccount {
    String grudgeId;       /* e.g. "GID-a1b2c3d4" */
    String displayName;    /* e.g. "RacAlvin" */
    String authToken;      /* session bearer token */
    uint32_t expiresAt;    /* unix timestamp when session expires */
    bool   loggedIn;
};

/* ── Pairing Code ─────────────────────────────────── */
struct DevicePairingCode {
    char   code[7];        /* 6-char alphanumeric + null */
    uint32_t createdAt;    /* millis() when generated */
    bool   active;
};

/* ── Lifecycle ────────────────────────────────────── */

/* Initialize the account system (call once at startup) */
void account_init(GrudgeAccount& acct);

/* Check NVS for a saved session. Returns true if a valid session was resumed. */
bool account_resume(GrudgeAccount& acct);

/* Generate a new pairing code for the login screen to display */
void account_generate_pairing_code(DevicePairingCode& pc);

/* Poll the backend to check if the pairing code has been claimed.
   Returns true if auth succeeded and acct is now populated. */
bool account_poll_auth(GrudgeAccount& acct, const DevicePairingCode& pc);

/* Log out — wipe session from NVS, reset acct state */
void account_logout(GrudgeAccount& acct);

/* ── Queries ──────────────────────────────────────── */

bool        account_is_logged_in(const GrudgeAccount& acct);
const char* account_get_token(const GrudgeAccount& acct);
const char* account_get_display_name(const GrudgeAccount& acct);
const char* account_get_grudge_id(const GrudgeAccount& acct);

/* Check if session has expired (compare against NTP or millis offset) */
bool account_session_expired(const GrudgeAccount& acct);

/**
 * Provision a new Grudge account directly from the device.
 * Creates a guest puter ID + Grudge ID via backend.
 * Returns true if account was created and acct is populated.
 */
bool account_provision(GrudgeAccount& acct, const String& displayName,
                       const String& deviceUUID);

/* Full factory reset — wipe all account + device NVS data */
void account_factory_reset();

#endif /* GRUDA_ACCOUNT_H */
