#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <lvgl.h>

#include "account.h"
#include "alerts.h"
#include "boot_splash.h"
#include "compress.h"
#include "config.h"
#include "discord.h"
#include "display.h"
#include "grd17.h"
#include "screensaver.h"
#include "theme.h"
#include "treaty.h"
#include "ui_shell.h"
#include "voting.h"
#include "device_api.h"
#include "grudgeos.h"
#include "img_loader.h"
#include "wallet.h"

/* Login screen (src/ui/screen_login.cpp) */
extern void login_screen_show(GrudgeAccount &acct);
extern void login_screen_destroy();

/* ── Global State ─────────────────────────────────────── */
static GrudgeAccount account;
static GrudaWallet wallet;
static GRD17NodeState nodeState;
static TreatyState treatyState;
static VotingState votingState;
static AlertState alertState;
static GrudgeOSState grudgeosState;

static unsigned long lastTick = 0;
static unsigned long lastUISync = 0;
static unsigned long lastNodeBroadcast = 0;
static bool mainUIReady = false;

/* ── GrudaChain WebSocket (for node state broadcast) ─ */
static WebSocketsClient wsChain;
static bool chainConnected = false;

/* ── Touch Diagnostic Mode ───────────────────── */
bool touchDiagMode = false;
static String serialCmdBuf;

/* ── GrudaChain WS event handler ──────────────────── */
static void _chain_ws_event(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("[CHAIN] Connected to GRUDACHAIN");
      chainConnected = true;
      {
      /* Authenticate with Grudge backend (walletless — device UUID only) */
        String auth = "{\"event\":\"auth\",\"grudgeId\":\"";
        auth += account_get_grudge_id(account);
        auth += "\",\"token\":\"";
        auth += account_get_token(account);
        auth += "\",\"deviceId\":\"";
        auth += wallet.deviceUUID;
        auth += "\",\"ns\":\"";
        auth += WS_NS_CHAIN;
        auth += "\",\"platform\":\"ESP32-GRD17\",\"firmware\":\"";
        auth += GRUDA_VERSION;
        auth += "\"}";
        wsChain.sendTXT(auth);
      }
      break;
    case WStype_DISCONNECTED:
      Serial.println("[CHAIN] Disconnected");
      chainConnected = false;
      break;
    case WStype_TEXT:
      Serial.printf("[CHAIN] Received: %s\n", payload);
      break;
    default:
      break;
  }
}

/* ── WiFi ─────────────────────────────────────────── */
struct WifiCredential {
  const char *ssid;
  const char *pass;
};

static bool wifi_credential_valid(const char *ssid, const char *pass) {
  if (!ssid || !pass || ssid[0] == '\0')
    return false;
  String ssidStr = String(ssid);
  String passStr = String(pass);
  if (ssidStr == "YOUR_WIFI_SSID" || passStr == "YOUR_WIFI_PASS")
    return false;
  return true;
}

static void wifi_connect() {
  static const WifiCredential creds[] = {{WIFI_SSID, WIFI_PASS},
                                         {WIFI_SSID_2, WIFI_PASS_2},
                                         {WIFI_SSID_3, WIFI_PASS_3}};

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  int bestCred = -1;
  int bestRssi = -127;
  int n = WiFi.scanNetworks();

  if (n > 0) {
    for (size_t i = 0; i < (sizeof(creds) / sizeof(creds[0])); i++) {
      if (!wifi_credential_valid(creds[i].ssid, creds[i].pass))
        continue;
      for (int j = 0; j < n; j++) {
        if (WiFi.SSID(j) == creds[i].ssid) {
          int rssi = WiFi.RSSI(j);
          if (rssi > bestRssi) {
            bestRssi = rssi;
            bestCred = (int)i;
          }
        }
      }
    }
  }
  WiFi.scanDelete();

  if (bestCred < 0 && wifi_credential_valid(creds[0].ssid, creds[0].pass)) {
    bestCred = 0;
  }

  if (bestCred < 0) {
    Serial.println("[WIFI] No configured networks found; running offline");
    return;
  }

  Serial.printf("[WIFI] Connecting to %s\n", creds[bestCred].ssid);
  WiFi.begin(creds[bestCred].ssid, creds[bestCred].pass);
  uint8_t tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(250);
    tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WIFI] Connected. IP: %s\n",
                  WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[WIFI] Failed — running offline");
  }
}

/* ── LVGL Tick (called from timer ISR or loop) ────── */
static void lv_tick_task(void *) { lv_tick_inc(LV_TICK_PERIOD_MS); }

/* ── Called by login screen on successful auth ───── */
static void _start_main_ui() {
  /* Initialize device identity (UUID, no crypto keys) */
  if (!wallet.initialized) wallet_init(wallet);
  grd17_init(nodeState, GRD17_VALIDATOR_ID);
  treaty_init(treatyState);
  voting_init(votingState);
  alerts_init(alertState);

  /* Connect WebSockets with real Grudge Account auth token */
  if (WiFi.status() == WL_CONNECTED) {
    String token = String(account_get_token(account));
    treaty_connect(account.grudgeId, token);
    alerts_connect(account.grudgeId, token);

    /* Connect GrudaChain node broadcast socket */
    wsChain.beginSSL(WS_HOST, WS_PORT, WS_NS_CHAIN);
    wsChain.onEvent(_chain_ws_event);
    wsChain.setReconnectInterval(RECONNECT_INTERVAL_MS);

    /* Connect to GrudgeOS as device agent */
    grudgeos_init(grudgeosState);
    grudgeos_connect(grudgeosState, account.grudgeId,
                     String(account_get_token(account)),
                     wallet.publicKeyHex);

    nodeState.running = true;
  }

  /* Set treaty context before building UI */
  extern void ui_tab_treaty_set_context(TreatyState*, const GrudaWallet*);
  ui_tab_treaty_set_context(&treatyState, &wallet);

  /* Build main UI */
  ui_shell_set_account(&account);
  ui_shell_create();
  screensaver_reset_timer();

  ui_shell_update_tabs(wallet, nodeState, treatyState, votingState, alertState,
                       0.0f);

  /* Show logged-in display name instead of raw pubkey */
  ui_shell_set_grudge_id(account_get_display_name(account));

  mainUIReady = true;
  Serial.printf("[MAIN] Main UI ready — logged in as %s\n",
                account_get_display_name(account));

  /* Register device with id.grudge-studio.com */
  device_register(wallet, account.grudgeId);

  /* Post boot message to #node-chat */
  treaty_send_channel(CH_NODE_CHAT, "DevNode came online", wallet);

  /* Announce boot to Discord */
  String bootMsg = String("⚡ **GRUDA Node Online**\n");
  bootMsg += "Node: " + String(account_get_display_name(account)) + "\n";
  bootMsg += "Network: " + String(GRUDACHAIN_NETWORK_ID) + "\n";
  bootMsg += "Firmware: v" + String(GRUDA_VERSION);
  if (WiFi.status() == WL_CONNECTED) {
    bootMsg += "\nIP: " + WiFi.localIP().toString();
  }
  discord_post_embed("Node Boot", bootMsg, 0x00FF88);
}

/* Global callback for login screen success */
void on_login_success() { _start_main_ui(); }

/* ── Setup ────────────────────────────────────────── */
void setup() {
  Serial.begin(115200);
  Serial.println("\n═══════════════════════════════════════");
  Serial.println("  GRUDA Node v" GRUDA_VERSION);
  Serial.println("  Created by RacAlvin The Pirate King");
  Serial.println("  GRUDGE STUDIO — Grudge Account Device");
  Serial.println("═══════════════════════════════════════\n");

  /* Display + LVGL */
  display_init();

  /* LVGL tick timer */
  const esp_timer_create_args_t tickArgs = {.callback = lv_tick_task,
                                            .name = "lv_tick"};
  esp_timer_handle_t tickTimer;
  esp_timer_create(&tickArgs, &tickTimer);
  esp_timer_start_periodic(tickTimer, LV_TICK_PERIOD_MS * 1000);

  /* ── Boot Splash (Grudge logo + gold spinner) ──── */
  boot_splash_show();
  lv_timer_handler();

  boot_splash_set_progress(30, "Connecting WiFi...");
  lv_timer_handler();

  /* WiFi (needed before auth polling) */
  wifi_connect();

  boot_splash_set_progress(60, "Checking account...");
  lv_timer_handler();

  /* ── Grudge Account Auth Gate ────────────────── */
  account_init(account);

#if DEV_MODE
  /* DEV MODE: skip Web3Auth, use device UUID as identity */
  Serial.println("[MAIN] *** DEV_MODE active — skipping auth gate ***");
  wallet_init(wallet);
  account.grudgeId    = "DEV-" + wallet.deviceUUID.substring(0, 8);
  account.displayName = "DevNode";
  account.authToken   = "dev-local";
  account.loggedIn    = true;

  boot_splash_set_progress(100, "Dev Mode");
  lv_timer_handler();
  delay(400);
  boot_splash_hide();
  _start_main_ui();
#else
  if (account_resume(account)) {
    /* Valid session found in NVS — go straight to main UI */
    boot_splash_set_progress(100, "Welcome back!");
    lv_timer_handler();
    delay(400);
    boot_splash_hide();
    _start_main_ui();
  } else {
    /* No session — show login screen with pairing code */
    boot_splash_set_progress(100, "Please log in");
    lv_timer_handler();
    delay(400);
    boot_splash_hide();
    login_screen_show(account);
    /* Main UI will be created by on_login_success() callback */
  }
#endif

  Serial.println("[MAIN] Setup complete");
}

/* ── Loop ─────────────────────────────────────────── */
void loop() {
  /* LVGL handler — always runs (login screen or main UI) */
  lv_timer_handler();

  /* If main UI isn't ready yet, we're on the login screen.
     LVGL timers handle the polling; just yield. */
  if (!mainUIReady) {
    delay(5);
    return;
  }

  /* Screensaver check */
  if (screensaver_is_active()) {
    delay(50);
    return;
  }
  screensaver_check();

  /* Serial command processing (touch diagnostics) */
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      serialCmdBuf.trim();
      if (serialCmdBuf == "TOUCH_DIAG") {
        touchDiagMode = !touchDiagMode;
        Serial.printf("[DIAG] Touch diagnostic mode: %s\n",
                      touchDiagMode ? "ON" : "OFF");
        Serial.println("[DIAG] Format: rawX, rawY, rawZ | mappedX, mappedY");
      } else if (serialCmdBuf == "STATUS") {
        Serial.printf("[STATUS] WiFi: %s | Chain: %s | Treaty: %s\n",
                      WiFi.status() == WL_CONNECTED ? "OK" : "OFF",
                      chainConnected ? "OK" : "OFF",
                      treatyState.connected ? "OK" : "OFF");
        Serial.printf("[STATUS] Account: %s | Uptime: %lus | Block: %u\n",
                      account_get_display_name(account),
                      millis() / 1000, nodeState.latestBlockHeight);
      } else if (serialCmdBuf == "DISCORD") {
        Serial.println("[DISCORD] Sending test message...");
        bool ok = discord_post_embed("Device Test",
            "GRUDA Node is alive and posting from hardware.", 0xFF6600);
        Serial.printf("[DISCORD] %s\n", ok ? "Sent!" : "Failed");
      } else if (serialCmdBuf == "LOGOUT") {
        Serial.println("[MAIN] Logout requested via serial");
        account_logout(account);
        mainUIReady = false;
        ESP.restart();
      }
      serialCmdBuf = "";
    } else {
      serialCmdBuf += c;
    }
  }

  /* Network loops */
  treaty_loop();
  alerts_loop();
  wsChain.loop();
  grudgeos_loop(grudgeosState);

  /* UI status updates (every 2s) */
  if (millis() - lastUISync > 2000) {
    lastUISync = millis();

    bool wifiOk = WiFi.status() == WL_CONNECTED;
    ui_shell_set_wifi(wifiOk, wifiOk ? WiFi.RSSI() : 0);
    ui_shell_set_uptime(millis() / 1000);
    ui_shell_update_tabs(wallet, nodeState, treatyState, votingState,
                         alertState, 0.0f);
    screensaver_reset_timer();
  }

  /* Node state broadcast (every 30s, compressed) */
  if (millis() - lastNodeBroadcast > HASH_BROADCAST_INTERVAL_MS) {
    lastNodeBroadcast = millis();

    if (nodeState.running) {
      String status = grd17_get_status_json(nodeState);
      CompressResult cr = gruda_compress_json(status);
      if (cr.ok) {
        Serial.printf("[GRD-17] Broadcast: %u -> %u bytes (%.0f%%)\n",
                      cr.originalSize, cr.compressedSize, cr.ratio * 100);
        if (chainConnected) {
          wsChain.sendBIN(cr.data, cr.compressedSize);
        }
        gruda_compress_free(cr);
      }

      /* Heartbeat to backend */
      device_heartbeat(wallet, nodeState.latestBlockHeight,
                       millis() / 1000, nodeState.peerCount);

      /* GrudgeOS telemetry */
      grudgeos_send_status(grudgeosState, nodeState.latestBlockHeight,
                           millis() / 1000, nodeState.peerCount,
                           ESP.getFreeHeap());
    }
  }

  delay(5); /* yield to RTOS */
}
