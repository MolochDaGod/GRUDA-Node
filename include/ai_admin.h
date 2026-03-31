#ifndef GRUDA_AI_ADMIN_H
#define GRUDA_AI_ADMIN_H

/**
 * AI Admin — Voice Command Router
 *
 * Parses transcribed text from BLE voice input into actionable intents.
 * Local commands (status, read messages, switch tabs) are handled on-device.
 * Complex queries are forwarded to the Legion AI at ai.grudge-studio.com.
 *
 * Created by RacAlvin The Pirate King for GRUDGE STUDIO
 */

#include <Arduino.h>
#include "account.h"
#include "treaty.h"
#include "wallet.h"
#include "grd17.h"

/* Context struct passed to the AI router for answering queries */
struct AIAdminContext {
    GrudgeAccount*   account;
    GrudaWallet*     wallet;
    TreatyState*     treaty;
    GRD17NodeState*  node;
};

/* Transcript entry for the voice log */
struct TranscriptEntry {
    String  text;
    String  response;
    bool    isLocal;      /* true = handled on-device, false = sent to backend */
    uint32_t timestamp;
};

/* AI Admin state */
struct AIAdminState {
    TranscriptEntry transcript[8]; /* AI_TRANSCRIPT_MAX */
    uint8_t         transcriptCount;
    bool            processing;     /* true while waiting for backend response */
};

/* ── Lifecycle ─────────────────────────────────────── */

/* Initialize AI admin with references to device state */
void ai_admin_init(AIAdminState& state, AIAdminContext& ctx);

/* ── Command Processing ────────────────────────────── */

/**
 * Process a voice command string.
 * Returns the response text (displayed on LVGL + sent back via BLE).
 * Handles local intents immediately; forwards unknowns to backend.
 */
String ai_admin_process(AIAdminState& state, const String& text);

/* Get the latest transcript entries */
const TranscriptEntry* ai_admin_get_transcript(const AIAdminState& state,
                                               uint8_t& count);

#endif /* GRUDA_AI_ADMIN_H */
