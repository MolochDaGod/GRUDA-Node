#include "treaty.h"
#include "config.h"
#include "discord.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

static WebSocketsClient wsTreaty;
static TreatyState* _state = nullptr;
static String _grudgeId;
static String _authToken;
static bool _shouldReconnect = false;
static unsigned long _lastReconnectAttempt = 0;

const char* TREATY_QUICK_REPLIES[TREATY_QUICK_REPLIES_COUNT] = {
    "GG",
    "On my way",
    "Need backup",
    "Treaty accepted",
    "Denied",
    "At base"
};

/* ── Default Guild Channel List ───────────────────── */
const GuildChannel GUILD_CHANNELS[] = {
    { CH_NODE_CHAT,     "# node-chat",     CHAN_TEXT,  false },
    { CH_GENERAL,       "# general",       CHAN_TEXT,  false },
    { CH_ANNOUNCEMENTS, "# announcements", CHAN_TEXT,  true  },
    { CH_TREATY_DEALS,  "# treaty-deals",  CHAN_TEXT,  false },
    { CH_RULES,         "# rules",         CHAN_INFO,  true  },
    { CH_NODE_STATUS,   "# node-status",   CHAN_INFO,  true  },
    { CH_VC_LOBBY,      "♫ vc-lobby",       CHAN_VOICE, false },
    { CH_VC_WAR_ROOM,   "♫ vc-war-room",    CHAN_VOICE, false },
};
const int GUILD_CHANNEL_COUNT_ACTUAL = sizeof(GUILD_CHANNELS) / sizeof(GUILD_CHANNELS[0]);

/* ── WebSocket event handler ──────────────────────── */
static void _ws_event(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            Serial.println("[TREATY] Connected");
            if (_state) _state->connected = true;
            /* Authenticate with Grudge backend */
            {
                JsonDocument doc;
                doc["event"] = "auth";
                doc["grudgeId"] = _grudgeId;
                doc["token"] = _authToken;
                doc["ns"] = WS_NS_TREATY;
                String out;
                serializeJson(doc, out);
                wsTreaty.sendTXT(out);
            }
            break;

        case WStype_DISCONNECTED:
            Serial.println("[TREATY] Disconnected");
            if (_state) _state->connected = false;
            _shouldReconnect = true;
            break;

        case WStype_TEXT:
            /* Parse incoming DM */
            {
                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, payload, length);
                if (err) {
                    Serial.printf("[TREATY] JSON parse error: %s\n", err.c_str());
                    break;
                }
                const char* event = doc["event"];
                if (event && (strcmp(event, "dm") == 0 || strcmp(event, "channel_msg") == 0)) {
                    if (_state && _state->messageCount < TREATY_MAX_MESSAGES) {
                        TreatyMessage& msg = _state->messages[_state->messageCount];
                        msg.senderId   = doc["from"]["grudgeId"].as<String>();
                        msg.senderName = doc["from"]["name"].as<String>();
                        msg.faction    = doc["from"]["faction"].as<String>();
                        msg.channelId  = doc["channel"] | "";
                        msg.text       = doc["text"].as<String>();
                        msg.timestamp  = doc["ts"] | (uint32_t)millis();
                        msg.read       = false;
                        _state->messageCount++;
                        Serial.printf("[TREATY] %s> %s: %s\n",
                                      msg.channelId.c_str(),
                                      msg.senderName.c_str(), msg.text.c_str());
                    }
                }
            }
            break;

        default:
            break;
    }
}

/* ── Public API ───────────────────────────────────── */

void treaty_init(TreatyState& state) {
    _state = &state;
    state.messageCount = 0;
    state.connected = false;
    state.activeConversation = "";
    state.activeChannel = CH_NODE_CHAT; /* default channel */
}

void treaty_connect(const String& grudgeId, const String& authToken) {
    _grudgeId = grudgeId;
    _authToken = authToken;
    wsTreaty.beginSSL(WS_HOST, WS_PORT, WS_NS_TREATY);
    wsTreaty.onEvent(_ws_event);
    wsTreaty.setReconnectInterval(RECONNECT_INTERVAL_MS);
    Serial.println("[TREATY] Connecting...");
}

void treaty_disconnect() {
    wsTreaty.disconnect();
    if (_state) _state->connected = false;
}

void treaty_loop() {
    wsTreaty.loop();

    /* Auto-reconnect */
    if (_shouldReconnect && millis() - _lastReconnectAttempt > RECONNECT_INTERVAL_MS) {
        _lastReconnectAttempt = millis();
        _shouldReconnect = false;
        treaty_connect(_grudgeId, _authToken);
    }
}

bool treaty_send_dm(const String& recipientId, const String& text,
                    const GrudaWallet& wallet) {
    if (!_state || !_state->connected) return false;

    /* Build message payload */
    JsonDocument doc;
    doc["event"] = "dm";
    doc["to"] = recipientId;
    doc["text"] = text;
    doc["from"] = _grudgeId;

    /* Walletless: auth token authenticates the message, no device signing */
    doc["authToken"] = _authToken;

    String out;
    serializeJson(doc, out);
    wsTreaty.sendTXT(out);
    return true;
}

bool treaty_send_quick(const String& recipientId, uint8_t quickIndex,
                       const GrudaWallet& wallet) {
    if (quickIndex >= TREATY_QUICK_REPLIES_COUNT) return false;
    return treaty_send_dm(recipientId, String(TREATY_QUICK_REPLIES[quickIndex]), wallet);
}

/* ── Send to a guild channel ───────────────────────── */
bool treaty_send_channel(const String& channelId, const String& text,
                         const GrudaWallet& wallet) {
    if (!_state || !_state->connected) {
        /* Offline — still mirror node-chat to Discord */
        if (channelId == CH_NODE_CHAT) {
            discord_post_embed("NODES TALK", "**" + String(_grudgeId.substring(0, 8)) + "**: " + text, 0xFF6600);
        }
        return false;
    }

    JsonDocument doc;
    doc["event"]   = "channel_msg";
    doc["guild"]   = GUILD_ID;
    doc["channel"] = channelId;
    doc["text"]    = text;
    doc["from"]    = _grudgeId;

    /* Walletless: auth token authenticates, no device signing */
    doc["authToken"] = _authToken;

    String out;
    serializeJson(doc, out);
    wsTreaty.sendTXT(out);

    /* Mirror #node-chat to Discord as NODES TALK */
    if (channelId == CH_NODE_CHAT) {
        discord_post_embed("NODES TALK", "**" + String(_grudgeId.substring(0, 8)) + "**: " + text, 0xFF6600);
    }

    return true;
}

uint8_t treaty_unread_count(const TreatyState& state) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < state.messageCount; i++) {
        if (!state.messages[i].read) count++;
    }
    return count;
}

uint8_t treaty_channel_unread(const TreatyState& state, const String& channelId) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < state.messageCount; i++) {
        if (!state.messages[i].read && state.messages[i].channelId == channelId) count++;
    }
    return count;
}
