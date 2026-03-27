#ifndef GRUDA_GRUDGEOS_H
#define GRUDA_GRUDGEOS_H

/**
 * GrudgeOS Device Agent — ESP32 Node Registration
 *
 * Connects to the GrudgeOS desktop (puter-monitor-ai) as a live
 * device agent. The node appears in the "Active Agents" panel,
 * sends telemetry, and can receive commands (reboot, config update,
 * firmware OTA trigger, etc.)
 *
 * WebSocket protocol matches the GrudgeOS agent message format.
 */

#include <Arduino.h>

/* Connection state */
struct GrudgeOSState {
    bool    connected;
    bool    registered;
    String  agentId;
    String  sessionId;
    uint32_t lastHeartbeat;
    uint32_t messageCount;
};

/* ── Lifecycle ─────────────────────────────────────── */

/* Initialize GrudgeOS agent connection */
void grudgeos_init(GrudgeOSState& state);

/* Connect to GrudgeOS WebSocket (call after WiFi + account login) */
void grudgeos_connect(GrudgeOSState& state, const String& grudgeId,
                      const String& authToken, const String& deviceKey);

/* Disconnect cleanly */
void grudgeos_disconnect(GrudgeOSState& state);

/* Process WebSocket events (call in loop) */
void grudgeos_loop(GrudgeOSState& state);

/* ── Telemetry ─────────────────────────────────────── */

/* Send device status update to GrudgeOS */
void grudgeos_send_status(GrudgeOSState& state,
                          uint32_t blockHeight, uint32_t uptime,
                          uint8_t peers, uint32_t freeHeap);

/* Send arbitrary event to GrudgeOS activity log */
void grudgeos_send_event(GrudgeOSState& state,
                         const char* eventType, const String& payload);

#endif /* GRUDA_GRUDGEOS_H */
