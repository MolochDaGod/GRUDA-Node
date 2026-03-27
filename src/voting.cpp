#include "voting.h"
#include "config.h"
#include "discord.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

void voting_init(VotingState& state) {
    state.proposalCount = 0;
    state.selectedIndex = -1;
    state.loading = false;
}

/* ── Fetch proposals ──────────────────────────────── */
bool voting_fetch_proposals(VotingState& state, const String& authToken) {
    state.loading = true;

    HTTPClient http;
    String url = String("https://") + API_HOST + "/grudachain/proposals";
    http.begin(url);
    http.addHeader("Authorization", "Bearer " + authToken);
    http.addHeader("Accept", "application/json");

    int code = http.GET();
    if (code != 200) {
        Serial.printf("[VOTE] Fetch failed: HTTP %d\n", code);
        http.end();
        state.loading = false;
        return false;
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[VOTE] JSON parse error: %s\n", err.c_str());
        state.loading = false;
        return false;
    }

    JsonArray arr = doc["proposals"].as<JsonArray>();
    state.proposalCount = 0;
    for (JsonObject p : arr) {
        if (state.proposalCount >= MAX_PROPOSALS) break;
        Proposal& prop = state.proposals[state.proposalCount];
        prop.id           = p["id"].as<String>();
        prop.title        = p["title"].as<String>();
        prop.description  = p["description"].as<String>();
        prop.yesCount     = p["votes"]["yes"] | 0;
        prop.noCount      = p["votes"]["no"] | 0;
        prop.abstainCount = p["votes"]["abstain"] | 0;
        prop.expiresAt    = p["expiresAt"] | 0;
        prop.voted        = p["myVote"].is<int>();
        prop.myVote       = prop.voted ? (VoteChoice)(p["myVote"].as<int>()) : VOTE_ABSTAIN;
        state.proposalCount++;
    }

    state.selectedIndex = state.proposalCount > 0 ? 0 : -1;
    state.loading = false;
    Serial.printf("[VOTE] Loaded %d proposals\n", state.proposalCount);
    return true;
}

/* ── Submit vote ──────────────────────────────────── */
bool voting_submit(const Proposal& proposal, VoteChoice choice,
                   const GrudaWallet& wallet, const String& authToken) {
    /* Build vote payload */
    JsonDocument doc;
    doc["proposalId"] = proposal.id;
    doc["vote"] = (int)choice;
    doc["deviceId"] = wallet.deviceUUID;

    /* Walletless: auth token authenticates the vote, no device signing */
    String signedPayload;
    serializeJson(doc, signedPayload);

    /* POST to backend */
    HTTPClient http;
    String url = String("https://") + API_HOST + "/grudachain/vote";
    http.begin(url);
    http.addHeader("Authorization", "Bearer " + authToken);
    http.addHeader("Content-Type", "application/json");

    int code = http.POST(signedPayload);
    bool ok = (code == 200 || code == 201);
    if (!ok) {
        Serial.printf("[VOTE] Submit failed: HTTP %d\n", code);
    } else {
        Serial.printf("[VOTE] Vote submitted: proposal=%s choice=%d\n",
                      proposal.id.c_str(), (int)choice);
        /* Announce vote to Discord */
        const char* choiceStr[] = { "Yes", "No", "Abstain" };
        String voteMsg = String("✅ **Vote Cast**\n");
        voteMsg += "Proposal: " + proposal.title + "\n";
        voteMsg += "Choice: " + String(choiceStr[(int)choice]);
        discord_post_embed("Chain Vote", voteMsg, 0xAA44FF);
    }
    http.end();
    return ok;
}
