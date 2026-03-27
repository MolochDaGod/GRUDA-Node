#ifndef GRUDA_WALLET_H
#define GRUDA_WALLET_H

#include <Arduino.h>

/* Wallet state */
struct GrudaWallet {
    uint8_t privateKey[32];
    uint8_t publicKey[64];     /* micro-ecc uses uncompressed 64-byte pubkey */
    String  publicKeyHex;
    bool    initialized;
};

/* Initialize wallet — loads from NVS or generates new keypair */
bool wallet_init(GrudaWallet& w);

/* Sign a message (returns 64-byte signature) */
bool wallet_sign(const GrudaWallet& w, const uint8_t* msg, size_t msgLen,
                 uint8_t sig[64]);

/* Verify a signature against a public key */
bool wallet_verify(const uint8_t pubkey[64], const uint8_t* msg, size_t msgLen,
                   const uint8_t sig[64]);

/* Get public key as hex string (for display / QR) */
String wallet_pubkey_hex(const GrudaWallet& w);

/* Wipe wallet from NVS (factory reset) */
void wallet_wipe();

#endif /* GRUDA_WALLET_H */
