#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <lvgl.h>

#include "account.h"
#include "ai_admin.h"
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
#include "voice_bt.h"
#include "voting.h"
#include "device_api.h"
#include "grudgeos.h"
#include "img_loader.h"
#include "wallet.h"

/* Setup screens */
extern void wifi_setup_show();
extern void wifi_setup_destroy();
extern bool wifi_try_saved_networks();
extern void account_setup_show(GrudgeAccount &acct, GrudaWallet &wallet);
extern void account_setup_destroy();
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
static VoiceBTState voiceBTState;
static AIAdminState aiAdminState;
static AIAdminContext aiAdminCtx;

static unsigned long lastTick = 0;
static unsigned long lastUISync = 0;
static unsigned long lastNodeBroadcast = 0;
static unsigned long lastHeapCheck = 0;
static bool mainUIReady = false;

/* ── GrudaChain WebSocket (for node state broadcast) ─ */
static WebSocketsClient wsChain;
static bool chainConnected = false;

/* ── Touch Diagnostic Mode ───────────────── */
bool touchDiagMode = false;
static String serialCmdBuf;

/* ── Voice tab externs ────────────────────── */
extern void ui_tab_voice_set_context(VoiceBTState* vbt, AIAdminState* ai);
extern void ui_tab_voice_refresh(const VoiceBTState& vbt, const AIAdminState& ai);

/* ── BLE voice callback (called from BLE RX on any core) ── */
void _on_voice_text(const String& text) {
  String response = ai_admin_process(aiAdminState, text);

  /* Send response back to phone via BLE */
  voice_bt_send_json("ai_response", response);

  /* Announce to treaty #node-chat */
  String chatMsg = "[VOICE] " + text + " -> " + response;
  if (chatMsg.length() > 200) chatMsg = chatMsg.substring(0, 197) + "...";
  treaty_send_channel(CH_NODE_CHAT, chatMsg, wallet);
}

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

  /* Initialize AI Admin router */
  aiAdminCtx.account = &account;
  aiAdminCtx.wallet  = &wallet;
  aiAdminCtx.treaty  = &treatyState;
  aiAdminCtx.node    = &nodeState;
  ai_admin_init(aiAdminState, aiAdminCtx);

  /* Initialize BLE Voice service */
  String bleSuffix = String(account_get_grudge_id(account)).substring(0, 6);
  voice_bt_init(voiceBTState, bleSuffix);
  voice_bt_set_callback(_on_voice_text);
  voice_bt_start_advertising();

  /* Set voice tab context */
  ui_tab_voice_set_context(&voiceBTState, &aiAdminState);

  /* Build main UI */
  ui_shell_set_account(&account);
  ui_shell_create();
  screensaver_reset_timer();

  ui_shell_update_tabs(wallet, nodeState, treatyState, votingState, alertState,
                       voiceBTState, aiAdminState, 0.0f);

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

/* Global callback for login/account setup success */
void on_login_success() { _start_main_ui(); }

/* Callback from WiFi setup screen — WiFi connected, proceed to account check */
void on_wifi_connected() {
  account_init(account);
  if (account_resume(account)) {
    Serial.println("[MAIN] Session resumed after WiFi setup");
    _start_main_ui();
  } else {
    /* Show account setup (create / pair / reset) */
    account_setup_show(account, wallet);
  }
}

/* ── Heap monitoring + cleanup ────────────────────── */
static void _heap_check() {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t minHeap  = esp_get_minimum_free_heap_size();
  Serial.printf("[HEAP] Free: %uKB | Watermark: %uKB | LVGL: ",
                freeHeap / 1024, minHeap / 1024);

  lv_mem_monitor_t mon;
  lv_mem_monitor(&mon);
  Serial.printf("%u%% used, %u%% frag\n", mon.used_pct, mon.frag_pct);

  /* Low-heap cleanup */
  if (freeHeap < LOW_HEAP_THRESHOLD) {
    Serial.println("[HEAP] LOW — running cleanup");

    /* Trim old treaty messages */
    if (treatyState.messageCount > 10) {
      uint8_t drop = treatyState.messageCount - 10;
      for (uint8_t i = 0; i < treatyState.messageCount - drop; i++) {
        treatyState.messages[i] = treatyState.messages[i + drop];
      }
      treatyState.messageCount -= drop;
      Serial.printf("[HEAP] Dropped %u old treaty msgs\n", drop);
    }
  }
}

/* ── Setup ──────────────────────────────────────── */
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

  boot_splash_set_progress(20, "Checking WiFi...");
  lv_timer_handler();

#if DEV_MODE
  /* DEV MODE: skip all setup, use device UUID as identity */
  Serial.println("[MAIN] *** DEV_MODE active — skipping setup ***");
  wifi_connect();  /* legacy hardcoded WiFi for dev */
  wallet_init(wallet);
  account_init(account);
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
  /* ── Step 1: Try saved WiFi networks from NVS ────── */
  bool wifiOk = wifi_try_saved_networks();

  /* Also try hardcoded creds as fallback */
  if (!wifiOk) {
    boot_splash_set_progress(40, "Trying config WiFi...");
    lv_timer_handler();
    wifi_connect();
    wifiOk = (WiFi.status() == WL_CONNECTED);
  }

  if (wifiOk) {
    boot_splash_set_progress(60, "Checking account...");
    lv_timer_handler();

    /* ── Step 2: Check for saved account session ── */
    account_init(account);
    if (account_resume(account)) {
      boot_splash_set_progress(100, "Welcome back!");
      lv_timer_handler();
      delay(400);
      boot_splash_hide();
      _start_main_ui();
    } else {
      /* WiFi connected but no account — show account setup */
      boot_splash_set_progress(100, "Account Setup");
      lv_timer_handler();
      delay(400);
      boot_splash_hide();
      account_setup_show(account, wallet);
    }
  } else {
    /* No WiFi — show WiFi setup screen */
    boot_splash_set_progress(100, "WiFi Setup");
    lv_timer_handler();
    delay(400);
    boot_splash_hide();
    wifi_setup_show();
  }
#endif

  Serial.printf("[MAIN] Setup complete | Heap: %uKB\n", ESP.getFreeHeap() / 1024);
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
      } else if (serialCmdBuf == "WIPE") {
        Serial.println("[MAIN] FACTORY RESET via serial");
        account_factory_reset();
        delay(300);
        ESP.restart();
      } else if (serialCmdBuf == "HEAP") {
        _heap_check();
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
    ui_shell_set_ble(voiceBTState.clientConnected);
    ui_shell_update_tabs(wallet, nodeState, treatyState, votingState,
                         alertState, voiceBTState, aiAdminState, 0.0f);
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

  /* Heap monitoring (every 60s) */
  if (millis() - lastHeapCheck > HEAP_CHECK_INTERVAL_MS) {
    lastHeapCheck = millis();
    _heap_check();
  }

  delay(5); /* yield to RTOS */
}
