#ifndef GRUDA_GRD17_H
#define GRUDA_GRD17_H

/**
 * GRD-17 GRUDACHAIN Node — ESP32
 * Keweebec2 consensus: PoS + SHA-256 block hashing + merkle root
 * Created by RacAlvin The Pirate King for GRUDGE STUDIO
 */

#include <Arduino.h>

/* ── Chain Constants ──────────────────────────────── */
#define GRUDACHAIN_NETWORK_ID    "GRUDACHAIN-MAINNET-17"
#define GRUDACHAIN_CHAIN_ID      "gruda-17"
#define GRUDACHAIN_CONSENSUS     "Keweebec2-PoS"
#define GRUDACHAIN_BLOCK_TIME    3000        /* ms per block target */
#define GRUDACHAIN_BLOCK_REWARD  17.0f       /* GRUDA per block */

/* ── Validator IDs ────────────────────────────────── */
#define GRD17_VALIDATOR_ID   "GRD-17-NODE-001"
#define GRD27_VALIDATOR_ID   "GRD-27-NODE-001"
#define ALE_VALIDATOR_ID     "ALE-VALIDATOR-001"
#define DANGRD_VALIDATOR_ID  "DANGRD-VAL-001"

/* ── Data Structures ──────────────────────────────── */

struct GrudaBlock {
    uint32_t index;
    uint64_t timestamp;
    char     previousHash[65];
    char     merkleRoot[65];
    uint32_t nonce;
    char     hash[65];
};

struct GrudaValidator {
    char     id[32];
    uint32_t stake;
    float    commission;
    bool     active;
    float    performance;
    float    votingPower;
};

#define GRD17_MAX_VALIDATORS 4

struct GRD17NodeState {
    char           networkId[41];
    char           thisNodeValidator[32];
    bool           running;
    uint32_t       latestBlockHeight;
    uint32_t       latestBlockTime;
    bool           catchingUp;
    uint8_t        peerCount;
    GrudaValidator validators[GRD17_MAX_VALIDATORS];
    int            validatorCount;
    uint32_t       totalStake;
};

/* ── API ──────────────────────────────────────────── */

/* Lifecycle */
void        grd17_init(GRD17NodeState& state, const char* validatorId);
void        grd17_init_validators(GRD17NodeState& state);
String      grd17_get_status_json(const GRD17NodeState& state);

/* Block operations */
void        grd17_calculate_block_hash(const GrudaBlock& block, char outHash[65]);
void        grd17_calculate_merkle_root(const char** txHashes, uint16_t txCount,
                                         char outRoot[65]);
bool        grd17_verify_block(const GrudaBlock& block);
bool        grd17_verify_chain_link(const GrudaBlock& current,
                                     const GrudaBlock& previous);

/* Signing / hashing */
void        grd17_sign_block(const char* data, const uint8_t privateKey[32], char outSig[65]);
void        grd17_hash_account_data(const uint8_t* data, size_t len, char outHash[65]);

/* Validator / PoS */
float       grd17_calculate_voting_power(uint32_t stake, uint32_t totalStake);
const char* grd17_select_validator(const GRD17NodeState& state);

#endif /* GRUDA_GRD17_H */
