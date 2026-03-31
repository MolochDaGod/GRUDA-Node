#include "ai_admin.h"
#include "config.h"

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

/**
 * AI Admin — Voice Command Router
 *
 * Local intents are pattern-matched and handled directly on the ESP32.
 * Anything not recognized locally is forwarded to the Legion AI backend
 * at api.grudge-studio.com/api/ai/voice-cmd for processing.
 */

static AIAdminContext*  _ctx = nullptr;
static AIAdminState*    _st = nullptr;

/* ── Helpers ──────────────────────────────────────── */

static bool _starts_with(const String& text, const char* prefix) {
    return text.startsWith(prefix);
}

static bool _contains(const String& text, const char* sub) {
    return text.indexOf(sub) >= 0;
}

/* Add an entry to the rolling transcript */
static void _add_transcript(const String& text, const String& response, bool isLocal) {
    if (!_st) return;

    uint8_t idx = _st->transcriptCount;
    if (idx >= AI_TRANSCRIPT_MAX) {
        /* Shift entries down — drop oldest */
        for (uint8_t i = 0; i < AI_TRANSCRIPT_MAX - 1; i++) {
            _st->transcript[i] = _st->transcript[i + 1];
        }
        idx = AI_TRANSCRIPT_MAX - 1;
    } else {
        _st->transcriptCount++;
    }

    _st->transcript[idx].text = text;
    _st->transcript[idx].response = response;
    _st->transcript[idx].isLocal = isLocal;
    _st->transcript[idx].timestamp = millis();
}

/* ── Local Command Handlers ───────────────────────── */

static String _handle_status() {
    if (!_ctx || !_ctx->node) return "Node context unavailable";

    String r = "Node: GRD-17 | WiFi: ";
    r += (WiFi.status() == WL_CONNECTED) ? "Connected" : "Offline";
    r += " | Block: " + String(_ctx->node->latestBlockHeight);
    r += " | Peers: " + String(_ctx->node->peerCount);
    r += " | Uptime: " + String(millis() / 60000) + "min";
    r += " | Heap: " + String(ESP.getFreeHeap() / 1024) + "KB";
    return r;
}

static String _handle_account() {
    if (!_ctx || !_ctx->account) return "Account not available";

    if (!account_is_logged_in(*_ctx->account)) return "Not logged in";

    String r = "Logged in as: ";
    r += account_get_display_name(*_ctx->account);
    r += " | ID: ";
    r += account_get_grudge_id(*_ctx->account);
    return r;
}

static String _handle_unread() {
    if (!_ctx || !_ctx->treaty) return "Treaty not connected";

    uint8_t count = treaty_unread_count(*_ctx->treaty);
    if (count == 0) return "No unread messages";

    String r = String(count) + " unread message" + (count > 1 ? "s" : "") + ":";

    /* List unread per channel */
    extern const GuildChannel GUILD_CHANNELS[];
    extern const int GUILD_CHANNEL_COUNT_ACTUAL;
    for (int i = 0; i < GUILD_CHANNEL_COUNT_ACTUAL; i++) {
        uint8_t cu = treaty_channel_unread(*_ctx->treaty, GUILD_CHANNELS[i].id);
        if (cu > 0) {
            r += " " + String(GUILD_CHANNELS[i].name) + "(" + String(cu) + ")";
        }
    }
    return r;
}

static String _handle_read_messages() {
    if (!_ctx || !_ctx->treaty) return "Treaty not connected";

    TreatyState& ts = *_ctx->treaty;
    if (ts.messageCount == 0) return "No messages";

    /* Read latest 5 messages */
    String r = "Latest messages:\n";
    uint8_t start = (ts.messageCount > 5) ? ts.messageCount - 5 : 0;
    for (uint8_t i = start; i < ts.messageCount; i++) {
        TreatyMessage& m = ts.messages[i];
        r += m.senderName + ": " + m.text + "\n";
        m.read = true;
    }
    return r;
}

static String _handle_send(const String& text) {
    if (!_ctx || !_ctx->treaty || !_ctx->wallet) return "Treaty not connected";

    /* Parse: "send <message> to <channel>" */
    int toIdx = text.lastIndexOf(" to ");
    if (toIdx < 0) {
        /* Default to node-chat */
        String msg = text.substring(5); /* skip "send " */
        msg.trim();
        if (msg.length() == 0) return "What should I send?";
        treaty_send_channel(CH_NODE_CHAT, msg, *_ctx->wallet);
        return "Sent to #node-chat: " + msg;
    }

    String msg = text.substring(5, toIdx);
    String channel = text.substring(toIdx + 4);
    msg.trim();
    channel.trim();
    channel.toLowerCase();

    /* Match channel name */
    const char* channelId = CH_NODE_CHAT;
    if (_contains(channel, "general"))        channelId = CH_GENERAL;
    else if (_contains(channel, "treaty"))    channelId = CH_TREATY_DEALS;
    else if (_contains(channel, "announce"))  channelId = CH_ANNOUNCEMENTS;
    else if (_contains(channel, "node"))      channelId = CH_NODE_CHAT;

    treaty_send_channel(channelId, msg, *_ctx->wallet);
    return "Sent to " + String(channelId) + ": " + msg;
}

/* ── Backend AI Forwarding ────────────────────────── */

static String _forward_to_backend(const String& text) {
    if (WiFi.status() != WL_CONNECTED) return "Offline — can't reach AI backend";

    if (!_ctx || !_ctx->account) return "No account for AI request";

    HTTPClient http;
    String url = String("https://") + API_HOST + AI_VOICE_CMD_PATH;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + account_get_token(*_ctx->account));
    http.setTimeout(AI_ADMIN_TIMEOUT_MS);

    JsonDocument doc;
    doc["grudgeId"] = account_get_grudge_id(*_ctx->account);
    doc["text"]     = text;
    doc["source"]   = "gruda-node-ble";
    doc["firmware"] = GRUDA_VERSION;

    String payload;
    serializeJson(doc, payload);

    int code = http.POST(payload);
    if (code != 200) {
        String errMsg = "AI backend error: HTTP " + String(code);
        http.end();
        Serial.printf("[AI] %s\n", errMsg.c_str());
        return errMsg;
    }

    String body = http.getString();
    http.end();

    /* Parse response */
    JsonDocument resp;
    if (deserializeJson(resp, body)) {
        return "AI response parse error";
    }

    String response = resp["response"].as<String>();
    const char* action = resp["action"];

    /* Handle backend-directed actions */
    if (action) {
        if (strcmp(action, "send_treaty") == 0) {
            String ch = resp["data"]["channel"].as<String>();
            String msg = resp["data"]["message"].as<String>();
            if (_ctx->wallet) treaty_send_channel(ch, msg, *_ctx->wallet);
        }
    }

    if (response.length() == 0) response = "No response from AI";
    return response;
}

/* ── Public API ───────────────────────────────────── */

void ai_admin_init(AIAdminState& state, AIAdminContext& ctx) {
    _st = &state;
    _ctx = &ctx;
    state.transcriptCount = 0;
    state.processing = false;
    Serial.println("[AI] Admin router initialized");
}

String ai_admin_process(AIAdminState& state, const String& text) {
    String lower = text;
    lower.toLowerCase();
    lower.trim();

    String response;
    bool isLocal = true;

    state.processing = true;

    /* ── Local intent matching ────────────────────── */

    /* Status / node info */
    if (lower == "status" || lower == "node status" ||
        _contains(lower, "how is the node") || _contains(lower, "node info")) {
        response = _handle_status();
    }
    /* Account info */
    else if (lower == "account" || lower == "who am i" ||
             _contains(lower, "my account") || _contains(lower, "check account")) {
        response = _handle_account();
    }
    /* Unread messages */
    else if (lower == "unread" || _contains(lower, "unread message") ||
             _contains(lower, "any messages")) {
        response = _handle_unread();
    }
    /* Read messages */
    else if (_contains(lower, "read message") || _contains(lower, "last message") ||
             _contains(lower, "show message")) {
        response = _handle_read_messages();
    }
    /* Send message */
    else if (_starts_with(lower, "send ")) {
        response = _handle_send(lower);
    }
    /* Help */
    else if (lower == "help" || lower == "commands" || _contains(lower, "what can you do")) {
        response = "Commands: status, account, unread, read messages, "
                   "send <msg> to <channel>, help. "
                   "Or ask me anything — I'll forward to the AI.";
    }
    /* Ping */
    else if (lower == "ping") {
        response = "Pong! Node is alive.";
    }
    /* ── Forward to backend AI ────────────────────── */
    else {
        isLocal = false;
        response = _forward_to_backend(text);
    }

    state.processing = false;

    /* Log to transcript */
    _add_transcript(text, response, isLocal);

    Serial.printf("[AI] %s → %s [%s]\n", text.c_str(), response.c_str(),
                  isLocal ? "local" : "backend");

    return response;
}

const TranscriptEntry* ai_admin_get_transcript(const AIAdminState& state,
                                               uint8_t& count) {
    count = state.transcriptCount;
    return state.transcript;
}
