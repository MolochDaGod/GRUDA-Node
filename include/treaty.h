#ifndef GRUDA_TREATY_H
#define GRUDA_TREATY_H

#include <Arduino.h>
#include "wallet.h"

#define TREATY_MAX_MESSAGES 16
#define TREATY_QUICK_REPLIES_COUNT 6

/* Channel types */
enum ChannelType : uint8_t {
    CHAN_TEXT  = 0,
    CHAN_VOICE = 1,
    CHAN_INFO  = 2,
};

struct GuildChannel {
    const char* id;
    const char* name;       /* display name with # prefix */
    ChannelType type;
    bool        readOnly;   /* true = announcements, rules, node-status */
};

struct TreatyMessage {
    String   senderId;
    String   senderName;
    String   faction;       /* "Crusade" | "Legion" | "Fabled" */
    String   channelId;     /* which channel this message belongs to */
    String   text;
    uint32_t timestamp;
    bool     read;
};

struct TreatyState {
    TreatyMessage messages[TREATY_MAX_MESSAGES];
    uint8_t messageCount;
    bool    connected;
    String  activeConversation;   /* Grudge ID of current DM partner */
    String  activeChannel;        /* Currently selected channel ID */
};

/* Default guild channel list */
extern const GuildChannel GUILD_CHANNELS[];
extern const int GUILD_CHANNEL_COUNT_ACTUAL;

/* Quick reply presets */
extern const char* TREATY_QUICK_REPLIES[TREATY_QUICK_REPLIES_COUNT];

/* Initialize Treaty DM system */
void treaty_init(TreatyState& state);

/* Connect WebSocket to Treaty namespace */
void treaty_connect(const String& grudgeId, const String& authToken);

/* Disconnect */
void treaty_disconnect();

/* Process incoming WebSocket data (call in loop) */
void treaty_loop();

/* Send a DM (signed with wallet key for authenticity) */
bool treaty_send_dm(const String& recipientId, const String& text,
                    const GrudaWallet& wallet);

/* Send to a guild channel */
bool treaty_send_channel(const String& channelId, const String& text,
                         const GrudaWallet& wallet);

/* Send a quick reply by index */
bool treaty_send_quick(const String& recipientId, uint8_t quickIndex,
                       const GrudaWallet& wallet);

/* Get unread message count (optionally filtered by channel) */
uint8_t treaty_unread_count(const TreatyState& state);
uint8_t treaty_channel_unread(const TreatyState& state, const String& channelId);

#endif /* GRUDA_TREATY_H */
