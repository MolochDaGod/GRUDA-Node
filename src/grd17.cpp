#include "grd17.h"
#include "config.h"
#include "mbedtls/sha256.h"
#include <ArduinoJson.h>
#include <cstring>

/**
 * GRD-17 GRUDACHAIN Node — ESP32 Implementation
 * Port of blockchain/blockchain.js from GrudgeDaDev/gruda_grd-17
 * Keweebec2 algorithm: SHA-256 block hashing + merkle root
 * Created by RacAlvin The Pirate King for GRUDGE STUDIO
 */

/* ── Internal SHA-256 helper ──────────────────────── */
static void _sha256(const uint8_t* in, size_t len, uint8_t out[32]) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, in, len);
    mbedtls_sha256_finish(&ctx, out);
    mbedtls_sha256_free(&ctx);
}

static void _bytes_to_hex(const uint8_t bytes[32], char hex[65]) {
    for (int i = 0; i < 32; i++) {
        sprintf(hex + i * 2, "%02x", bytes[i]);
    }
    hex[64] = '\0';
}

/* ── Keweebec2 Block Hash ─────────────────────────── */
void grd17_calculate_block_hash(const GrudaBlock& block, char outHash[65]) {
    JsonDocument doc;
    doc["index"]        = block.index;
    doc["timestamp"]    = block.timestamp;
    doc["previousHash"] = block.previousHash;
    doc["merkleRoot"]   = block.merkleRoot;
    doc["nonce"]        = block.nonce;

    String blockString;
    serializeJson(doc, blockString);

    uint8_t hash[32];
    _sha256((const uint8_t*)blockString.c_str(), blockString.length(), hash);
    _bytes_to_hex(hash, outHash);
}

/* ── Merkle Root ──────────────────────────────────── */
void grd17_calculate_merkle_root(const char** txHashes, uint16_t txCount, char outRoot[65]) {
    if (txCount == 0) {
        uint8_t hash[32];
        _sha256((const uint8_t*)"", 0, hash);
        _bytes_to_hex(hash, outRoot);
        return;
    }

    String level[64];
    uint16_t levelCount = txCount > 64 ? 64 : txCount;
    for (uint16_t i = 0; i < levelCount; i++) {
        level[i] = String(txHashes[i]);
    }

    while (levelCount > 1) {
        String nextLevel[64];
        uint16_t nextCount = 0;
        for (uint16_t i = 0; i < levelCount; i += 2) {
            String left = level[i];
            String right = (i + 1 < levelCount) ? level[i + 1] : left;
            String combined = left + right;
            uint8_t hash[32];
            _sha256((const uint8_t*)combined.c_str(), combined.length(), hash);
            char hex[65];
            _bytes_to_hex(hash, hex);
            nextLevel[nextCount++] = String(hex);
        }
        for (uint16_t i = 0; i < nextCount; i++) level[i] = nextLevel[i];
        levelCount = nextCount;
    }

    strncpy(outRoot, level[0].c_str(), 64);
    outRoot[64] = '\0';
}

/* ── Block Verification ───────────────────────────── */
bool grd17_verify_block(const GrudaBlock& block) {
    char computed[65];
    grd17_calculate_block_hash(block, computed);
    return strcmp(computed, block.hash) == 0;
}

bool grd17_verify_chain_link(const GrudaBlock& current, const GrudaBlock& previous) {
    return strcmp(current.previousHash, previous.hash) == 0;
}

/* ── Block Signing (matches GrudachainCore.signBlock) ── */
void grd17_sign_block(const char* data, const uint8_t privateKey[32], char outSig[65]) {
    /* Hash the data */
    uint8_t dataHash[32];
    _sha256((const uint8_t*)data, strlen(data), dataHash);

    /* HMAC-like: hash( privateKey || dataHash ) */
    uint8_t combined[64];
    memcpy(combined, privateKey, 32);
    memcpy(combined + 32, dataHash, 32);

    uint8_t sig[32];
    _sha256(combined, 64, sig);
    _bytes_to_hex(sig, outSig);
}

/* ── Validator Selection (PoS) ────────────────────── */
float grd17_calculate_voting_power(uint32_t stake, uint32_t totalStake) {
    if (totalStake == 0) return 0.0f;
    return (float)stake / (float)totalStake;
}

void grd17_init_validators(GRD17NodeState& state) {
    struct { const char* id; uint32_t stake; float commission; } defaults[] = {
        { GRD17_VALIDATOR_ID,  1000000, 0.05f },
        { GRD27_VALIDATOR_ID,   800000, 0.03f },
        { ALE_VALIDATOR_ID,     600000, 0.04f },
        { DANGRD_VALIDATOR_ID,  500000, 0.06f },
    };

    state.totalStake = 0;
    state.validatorCount = 4;

    for (int i = 0; i < 4; i++) {
        GrudaValidator& v = state.validators[i];
        strncpy(v.id, defaults[i].id, 31); v.id[31] = '\0';
        v.stake       = defaults[i].stake;
        v.commission  = defaults[i].commission;
        v.active      = true;
        v.performance = 1.0f;
        state.totalStake += v.stake;
    }

    for (int i = 0; i < 4; i++) {
        state.validators[i].votingPower =
            grd17_calculate_voting_power(state.validators[i].stake, state.totalStake);
    }
}

const char* grd17_select_validator(const GRD17NodeState& state) {
    float totalWeight = 0;
    for (int i = 0; i < state.validatorCount; i++) {
        if (state.validators[i].active) totalWeight += state.validators[i].votingPower;
    }
    float r = (float)random(10000) / 10000.0f * totalWeight;
    for (int i = 0; i < state.validatorCount; i++) {
        if (!state.validators[i].active) continue;
        r -= state.validators[i].votingPower;
        if (r <= 0) return state.validators[i].id;
    }
    return state.validators[0].id;
}

/* ── Node Lifecycle ───────────────────────────────── */
void grd17_init(GRD17NodeState& state, const char* validatorId) {
    strncpy(state.networkId, GRUDACHAIN_NETWORK_ID, 40); state.networkId[40] = '\0';
    strncpy(state.thisNodeValidator, validatorId, 31); state.thisNodeValidator[31] = '\0';
    state.running = false;
    state.latestBlockHeight = 0;
    state.latestBlockTime = 0;
    state.catchingUp = true;
    state.peerCount = 0;
    grd17_init_validators(state);

    Serial.println("[GRD-17] ═══════════════════════════════════════");
    Serial.printf("[GRD-17] Network: %s\n", state.networkId);
    Serial.printf("[GRD-17] Consensus: %s\n", GRUDACHAIN_CONSENSUS);
    Serial.printf("[GRD-17] Block Time: %dms | Reward: %.1f GRUDA\n",
                  GRUDACHAIN_BLOCK_TIME, GRUDACHAIN_BLOCK_REWARD);
    Serial.printf("[GRD-17] Validator: %s | Stake: %u\n",
                  state.thisNodeValidator, state.totalStake);
    Serial.println("[GRD-17] ═══════════════════════════════════════");
}

String grd17_get_status_json(const GRD17NodeState& state) {
    JsonDocument doc;
    doc["networkId"]  = state.networkId;
    doc["isRunning"]  = state.running;
    doc["validators"] = state.validatorCount;
    doc["consensus"]  = GRUDACHAIN_CONSENSUS;
    doc["blockTime"]  = GRUDACHAIN_BLOCK_TIME;
    doc["totalStake"] = state.totalStake;
    doc["chainId"]    = GRUDACHAIN_CHAIN_ID;

    JsonObject sync = doc["syncInfo"].to<JsonObject>();
    sync["latestBlockHeight"] = state.latestBlockHeight;
    sync["latestBlockTime"]   = state.latestBlockTime;
    sync["catchingUp"]        = state.catchingUp;

    JsonObject node = doc["thisNode"].to<JsonObject>();
    node["validator"] = state.thisNodeValidator;
    node["peers"]     = state.peerCount;
    node["platform"]  = "ESP32-GRD17";
    node["firmware"]  = GRUDA_VERSION;

    String out;
    serializeJson(doc, out);
    return out;
}

void grd17_hash_account_data(const uint8_t* data, size_t len, char outHash[65]) {
    uint8_t hash[32];
    _sha256(data, len, hash);
    _bytes_to_hex(hash, outHash);
}
