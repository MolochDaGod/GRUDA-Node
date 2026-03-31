#ifndef GRUDA_CONFIG_H
#define GRUDA_CONFIG_H

#include <Arduino.h> /* uint8_t, uint32_t, String, millis, etc. */

/* ── Secrets (loaded from config_secrets.h — never commit that file) ─ */
#if __has_include("config_secrets.h")
  #include "config_secrets.h"
#endif

/* ── WiFi — override via config_secrets.h or build flags ── */
#ifndef WIFI_SSID
  #define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
  #define WIFI_PASS ""
#endif
#ifndef WIFI_SSID_2
  #define WIFI_SSID_2 ""
#endif
#ifndef WIFI_PASS_2
  #define WIFI_PASS_2 ""
#endif
#ifndef WIFI_SSID_3
  #define WIFI_SSID_3 ""
#endif
#ifndef WIFI_PASS_3
  #define WIFI_PASS_3 ""
#endif

/* ── Discord Webhook — override via config_secrets.h ── */
#ifndef DISCORD_WEBHOOK_HOST
  #define DISCORD_WEBHOOK_HOST "discord.com"
#endif
#ifndef DISCORD_WEBHOOK_PATH
  #define DISCORD_WEBHOOK_PATH ""
#endif

/* ── Dev Mode ──────────────────────────────────── */
/* Set to 1 to skip Grudge Account auth and boot straight to main UI.
   Uses wallet pubkey as identity. Set to 0 for production. */
#ifndef DEV_MODE
  #define DEV_MODE 0
#endif

/* ── Grudge Backend ─────────────────────────────── */
#ifndef API_HOST
  #define API_HOST "api.grudge-studio.com"
#endif
#ifndef API_PORT
  #define API_PORT 443
#endif
#ifndef WS_HOST
  #define WS_HOST "ws.grudge-studio.com"
#endif
#ifndef WS_PORT
  #define WS_PORT 443
#endif
#ifndef ID_HOST
  #define ID_HOST "id.grudge-studio.com"
#endif

/* ── WebSocket Namespaces ─────────────────────────── */
#define WS_NS_TREATY "/treaty"
#define WS_NS_CHAIN "/grudachain"
#define WS_NS_ALERTS "/alerts"

/* ── GRD-17 Hashing ───────────────────────────────── */
#define GRD17_ROUNDS 17
#define GRD17_SALT_PREFIX "GRUDACHAIN"

/* ── Display ──────────────────────────────────────── */
#define SCREEN_W 240
#define SCREEN_H 320
#define STATUS_BAR_H 30

/* ── Touch (XPT2046 on separate SPI bus — CYD board) ─ */
/* CYD touch SPI pins (VSPI instance, separate from display HSPI) */
#define TOUCH_SPI_CLK   25
#define TOUCH_SPI_MISO  39
#define TOUCH_SPI_MOSI  32
#define TOUCH_SPI_CS    33
#define TOUCH_SPI_IRQ   36

/* Touch mapping — raw XPT2046 ADC range to screen pixels.
   Adjust these if touch is offset. Use TOUCH_DIAG serial command. */
#define TOUCH_RAW_X_MIN  200
#define TOUCH_RAW_X_MAX  3800
#define TOUCH_RAW_Y_MIN  200
#define TOUCH_RAW_Y_MAX  3800

/* Smoothing */
#define TOUCH_SMOOTH_ALPHA_NUM 3
#define TOUCH_SMOOTH_ALPHA_DEN 4

/* ── NVS Keys (walletless — no private keys stored) ── */
#define NVS_NAMESPACE "gruda"
#define NVS_KEY_GRUDGEID "grudge_id"
#define NVS_KEY_NODEID "node_id"

/* ── NVS WiFi Storage ──────────────────────────── */
#define NVS_WIFI_NS       "gruda_wifi"
#define NVS_WIFI_MAX      3            /* max saved networks */
#define NVS_WIFI_COUNT    "wifi_count"
#define NVS_WIFI_LAST     "last_idx"

/* ── Device Auth ────────────────────────────── */
#define DEVICE_AUTH_POLL_PATH "/device/auth/poll"
#define DEVICE_PAIRING_CODE_LEN 6
#define AUTH_POLL_INTERVAL_MS 3000       /* poll backend every 3s during login */
#define PAIRING_CODE_EXPIRE_MS 300000   /* regenerate code after 5 min */

/* ── Device Registration API (id.grudge-studio.com) ── */
#define DEVICE_REGISTER_PATH   "/api/devices/register"
#define DEVICE_HEARTBEAT_PATH  "/api/devices/heartbeat"
#define DEVICE_ME_PATH         "/api/devices/me"
#define DEVICE_PROVISION_PATH  "/api/devices/provision"
#define NVS_KEY_DEVTOKEN       "dev_token"

/* ── Treaty Guild: Grudge Studio (default) ───────── */
#define GUILD_NAME          "Grudge Studio"
#define GUILD_ID            "grudge-studio-default"

/* ── Token Purchase Links ────────────────────────── */
#define GBUX_PURCHASE_URL "https://raydium.io/launchpad/token/?mint=55TpSoMNxbfsNJ9U1dQoo9H3dRtDmjBZVMcKqvU2nray"
#define GBUX_MINT_ADDRESS "55TpSoMNxbfsNJ9U1dQoo9H3dRtDmjBZVMcKqvU2nray"

/* Channel IDs */
#define CH_NODE_CHAT        "node-chat"
#define CH_GENERAL          "general"
#define CH_ANNOUNCEMENTS    "announcements"
#define CH_TREATY_DEALS     "treaty-deals"
#define CH_RULES            "rules"
#define CH_NODE_STATUS      "node-status"
#define CH_VC_LOBBY         "vc-lobby"
#define CH_VC_WAR_ROOM      "vc-war-room"

/* Channel count */
#define GUILD_CHANNEL_COUNT 8

/* ── BLE Voice Service ────────────────────────────── */
/* Custom 128-bit UUIDs for the GRUDA Voice BLE service */
#define BLE_SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_CHAR_RX_UUID        "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  /* phone → ESP32 */
#define BLE_CHAR_TX_UUID        "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  /* ESP32 → phone */
#define BLE_DEVICE_NAME_PREFIX  "GRUDA-Node-"
#define BLE_MTU                 256
#define BLE_MAX_MSG_LEN         240   /* max text message over BLE */

/* ── AI Admin ─────────────────────────────────────── */
#define AI_VOICE_CMD_PATH       "/api/ai/voice-cmd"
#define AI_ADMIN_TIMEOUT_MS     8000
#define AI_RESPONSE_MAX_LEN     256   /* max chars displayed on LVGL */
#define AI_TRANSCRIPT_MAX       8     /* max entries in voice transcript log */

/* ── Timing ─────────────────────────────────────── */
#define HASH_BROADCAST_INTERVAL_MS 30000 /* broadcast hash state every 30s */
#define ALERT_POLL_INTERVAL_MS 5000
#define RECONNECT_INTERVAL_MS 10000
#define HEAP_CHECK_INTERVAL_MS 60000    /* log heap stats every 60s */
#define LOW_HEAP_THRESHOLD     20480    /* 20KB — trigger cleanup below this */
#define WIFI_CONNECT_TIMEOUT   40       /* 40 * 250ms = 10s */

#endif /* GRUDA_CONFIG_H */
