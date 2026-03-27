#ifndef GRUDA_ALERTS_H
#define GRUDA_ALERTS_H

#include <Arduino.h>

#define MAX_ALERTS 30

enum AlertType : uint8_t {
    ALERT_CREW_INVITE    = 1,
    ALERT_PVP_CHALLENGE  = 2,
    ALERT_FACTION_EVENT  = 3,
    ALERT_GOLD_TX        = 4,
    ALERT_GOULDSTONE     = 5,
    ALERT_MISSION        = 6,
    ALERT_SYSTEM         = 7,
    ALERT_CHAIN_VOTE     = 8,
};

struct Alert {
    AlertType type;
    String    title;
    String    body;
    uint32_t  timestamp;
    bool      read;
};

struct AlertState {
    Alert   alerts[MAX_ALERTS];
    uint8_t alertCount;
    bool    connected;
};

/* Initialize alerts system */
void alerts_init(AlertState& state);

/* Connect to alerts WebSocket namespace */
void alerts_connect(const String& grudgeId, const String& authToken);

/* Disconnect */
void alerts_disconnect();

/* Process incoming data (call in loop) */
void alerts_loop();

/* Get unread alert count */
uint8_t alerts_unread_count(const AlertState& state);

/* Mark all as read */
void alerts_mark_all_read(AlertState& state);

#endif /* GRUDA_ALERTS_H */
