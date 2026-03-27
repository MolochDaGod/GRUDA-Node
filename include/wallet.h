#ifndef GRUDA_WALLET_H
#define GRUDA_WALLET_H

/**
 * Device Identity — Walletless Architecture
 *
 * The GRUDA Node does NOT store private keys or do on-device signing.
 * User identity comes from Web3Auth via the Grudge Account session.
 * All signing/transactions are server-routed through api.grudge-studio.com.
 *
 * This module provides only a stable device UUID for hardware identification.
 * The UUID is generated once and stored in NVS — it is NOT a wallet key.
 */

#include <Arduino.h>

/* Device identity (no private keys, no signing) */
struct GrudaWallet {
    String  deviceUUID;    /* Random UUID for hardware identification */
    String  publicKeyHex;  /* Alias for deviceUUID (API compat) */
    bool    initialized;
};

/* Initialize device identity — loads or generates UUID from NVS */
bool wallet_init(GrudaWallet& w);

/* Get device UUID as hex string */
String wallet_pubkey_hex(const GrudaWallet& w);

/* Wipe device identity from NVS (factory reset) */
void wallet_wipe();

#endif /* GRUDA_WALLET_H */
