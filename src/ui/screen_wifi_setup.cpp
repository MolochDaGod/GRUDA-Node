#include "config.h"
#include "theme.h"
#include <lvgl.h>
#include <WiFi.h>
#include <Preferences.h>

/**
 * WiFi Setup Screen — self-contained network picker.
 * Scans for available networks, shows a scrollable list with RSSI bars,
 * lets the user enter a password via on-screen keyboard, and connects.
 * Saves credentials to NVS for future boots.
 *
 * Created by RacAlvin The Pirate King for GRUDGE STUDIO
 */

/* Callback when WiFi is connected — main.cpp provides this */
extern void on_wifi_connected();

static lv_obj_t* wifiScreen = nullptr;
static lv_obj_t* networkList = nullptr;
static lv_obj_t* lblStatus = nullptr;
static lv_obj_t* passPanel = nullptr;
static lv_obj_t* passArea = nullptr;
static lv_obj_t* keyboard = nullptr;
static lv_obj_t* spinner = nullptr;

static String _selectedSSID;

/* ── NVS WiFi helpers ─────────────────────────────── */
static void _save_wifi_creds(const String& ssid, const String& pass) {
    Preferences p;
    p.begin(NVS_WIFI_NS, false);
    uint8_t count = p.getUChar(NVS_WIFI_COUNT, 0);

    /* Check if this SSID already saved — update password */
    for (uint8_t i = 0; i < count && i < NVS_WIFI_MAX; i++) {
        String key = "ssid_" + String(i);
        String saved = p.getString(key.c_str(), "");
        if (saved == ssid) {
            String pkey = "pass_" + String(i);
            p.putString(pkey.c_str(), pass);
            p.putUChar(NVS_WIFI_LAST, i);
            p.end();
            Serial.printf("[WIFI] Updated password for slot %d: %s\n", i, ssid.c_str());
            return;
        }
    }

    /* Save to next slot (wrap around) */
    uint8_t idx = count < NVS_WIFI_MAX ? count : 0;
    String skey = "ssid_" + String(idx);
    String pkey = "pass_" + String(idx);
    p.putString(skey.c_str(), ssid);
    p.putString(pkey.c_str(), pass);
    if (count < NVS_WIFI_MAX) p.putUChar(NVS_WIFI_COUNT, count + 1);
    p.putUChar(NVS_WIFI_LAST, idx);
    p.end();
    Serial.printf("[WIFI] Saved to slot %d: %s\n", idx, ssid.c_str());
}

bool wifi_load_saved_creds(String& ssid, String& pass) {
    Preferences p;
    p.begin(NVS_WIFI_NS, true);
    uint8_t count = p.getUChar(NVS_WIFI_COUNT, 0);
    if (count == 0) { p.end(); return false; }

    uint8_t last = p.getUChar(NVS_WIFI_LAST, 0);
    String skey = "ssid_" + String(last);
    String pkey = "pass_" + String(last);
    ssid = p.getString(skey.c_str(), "");
    pass = p.getString(pkey.c_str(), "");
    p.end();
    return ssid.length() > 0;
}

bool wifi_try_saved_networks() {
    Preferences p;
    p.begin(NVS_WIFI_NS, true);
    uint8_t count = p.getUChar(NVS_WIFI_COUNT, 0);
    if (count == 0) { p.end(); return false; }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);

    /* Try last-connected first, then others */
    uint8_t last = p.getUChar(NVS_WIFI_LAST, 0);
    uint8_t order[NVS_WIFI_MAX];
    order[0] = last;
    uint8_t oi = 1;
    for (uint8_t i = 0; i < count && i < NVS_WIFI_MAX; i++) {
        if (i != last && oi < NVS_WIFI_MAX) order[oi++] = i;
    }

    for (uint8_t j = 0; j < count && j < NVS_WIFI_MAX; j++) {
        uint8_t idx = order[j];
        String skey = "ssid_" + String(idx);
        String pkey = "pass_" + String(idx);
        String ssid = p.getString(skey.c_str(), "");
        String pass = p.getString(pkey.c_str(), "");
        if (ssid.length() == 0) continue;

        Serial.printf("[WIFI] Trying saved: %s (slot %d)\n", ssid.c_str(), idx);
        WiFi.begin(ssid.c_str(), pass.c_str());
        uint8_t tries = 0;
        while (WiFi.status() != WL_CONNECTED && tries < WIFI_CONNECT_TIMEOUT) {
            delay(250);
            tries++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            p.end();
            Serial.printf("[WIFI] Connected to %s — IP: %s\n",
                          ssid.c_str(), WiFi.localIP().toString().c_str());
            return true;
        }
        WiFi.disconnect();
    }
    p.end();
    Serial.println("[WIFI] No saved network connected");
    return false;
}

/* ── RSSI to bars string ──────────────────────────── */
static const char* _rssi_bars(int rssi) {
    if (rssi > -50) return LV_SYMBOL_WIFI "+++";
    if (rssi > -65) return LV_SYMBOL_WIFI "++ ";
    if (rssi > -80) return LV_SYMBOL_WIFI "+  ";
    return LV_SYMBOL_WIFI "   ";
}

/* ── Connect attempt (runs from password submit) ──── */
static void _try_connect(const String& ssid, const String& pass) {
    if (lblStatus) {
        lv_label_set_text(lblStatus, "Connecting...");
        lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_YELLOW), 0);
    }

    /* Show spinner */
    if (spinner) lv_obj_clear_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    lv_timer_handler(); /* force render */

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    uint8_t tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < WIFI_CONNECT_TIMEOUT) {
        delay(250);
        tries++;
        if (tries % 4 == 0) lv_timer_handler(); /* keep UI alive */
    }

    if (spinner) lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);

    if (WiFi.status() == WL_CONNECTED) {
        _save_wifi_creds(ssid, pass);

        if (lblStatus) {
            lv_label_set_text_fmt(lblStatus, "Connected! IP: %s",
                                  WiFi.localIP().toString().c_str());
            lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_GREEN), 0);
        }
        lv_timer_handler();
        delay(600);

        /* Destroy WiFi screen and proceed */
        if (wifiScreen) { lv_obj_del(wifiScreen); wifiScreen = nullptr; }
        on_wifi_connected();
    } else {
        WiFi.disconnect();
        if (lblStatus) {
            lv_label_set_text(lblStatus, "Failed! Tap to retry.");
            lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_RED), 0);
        }
    }
}

/* ── Password submit callback ─────────────────────── */
static void _pass_submit_cb(lv_event_t* e) {
    (void)e;
    if (!passArea) return;
    String pass = String(lv_textarea_get_text(passArea));
    /* Hide password panel */
    if (passPanel) lv_obj_add_flag(passPanel, LV_OBJ_FLAG_HIDDEN);
    _try_connect(_selectedSSID, pass);
}

/* ── Keyboard event (OK pressed) ──────────────────── */
static void _kb_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        _pass_submit_cb(e);
    } else if (code == LV_EVENT_CANCEL) {
        if (passPanel) lv_obj_add_flag(passPanel, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ── Network list item clicked ────────────────────── */
static void _network_clicked_cb(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    uint32_t idx = lv_obj_get_index(btn);

    /* Get SSID from stored scan */
    _selectedSSID = WiFi.SSID(idx);
    Serial.printf("[WIFI] Selected: %s\n", _selectedSSID.c_str());

    /* Check if network is open */
    wifi_auth_mode_t auth = WiFi.encryptionType(idx);
    if (auth == WIFI_AUTH_OPEN) {
        _try_connect(_selectedSSID, "");
        return;
    }

    /* Show password input panel */
    if (passPanel) {
        lv_obj_clear_flag(passPanel, LV_OBJ_FLAG_HIDDEN);
        if (passArea) lv_textarea_set_text(passArea, "");
    }
}

/* ── Skip button (offline mode) ──────────────────── */
static void _skip_cb(lv_event_t* e) {
    (void)e;
    Serial.println("[WIFI] Skipped — running offline");
    if (wifiScreen) { lv_obj_del(wifiScreen); wifiScreen = nullptr; }
    on_wifi_connected(); /* proceed even without WiFi */
}

/* ── Refresh scan ─────────────────────────────────── */
static void _scan_and_populate() {
    if (lblStatus) {
        lv_label_set_text(lblStatus, "Scanning...");
        lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_YELLOW), 0);
    }
    lv_timer_handler();

    int n = WiFi.scanNetworks();
    if (networkList) lv_obj_clean(networkList);

    if (n <= 0) {
        if (lblStatus) {
            lv_label_set_text(lblStatus, "No networks found. Tap Scan.");
            lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_RED), 0);
        }
        return;
    }

    for (int i = 0; i < n && i < 12; i++) {
        lv_obj_t* btn = lv_btn_create(networkList);
        lv_obj_set_size(btn, LV_PCT(100), 32);
        lv_obj_set_style_bg_color(btn, lv_color_hex(WCS_BG_CARD), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(WCS_BUTTON_ACTIVE), LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_pad_hor(btn, 8, 0);
        lv_obj_add_event_cb(btn, _network_clicked_cb, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* lbl = lv_label_create(btn);
        char buf[48];
        snprintf(buf, sizeof(buf), "%s  %s", _rssi_bars(WiFi.RSSI(i)),
                 WiFi.SSID(i).c_str());
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(WCS_TEXT_PRIMARY), 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    }

    if (lblStatus) {
        lv_label_set_text_fmt(lblStatus, "%d networks found", n);
        lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_GREEN), 0);
    }
    /* Keep scan results alive for SSID/auth lookup */
}

static void _scan_cb(lv_event_t* e) {
    (void)e;
    WiFi.scanDelete();
    _scan_and_populate();
}

/* ── Public: Show WiFi Setup Screen ───────────────── */
void wifi_setup_show() {
    WiFi.mode(WIFI_STA);

    wifiScreen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(wifiScreen, SCREEN_W, SCREEN_H);
    lv_obj_align(wifiScreen, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(wifiScreen, lv_color_hex(WCS_BG_DARK), 0);
    lv_obj_set_style_bg_opa(wifiScreen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wifiScreen, 0, 0);
    lv_obj_set_style_radius(wifiScreen, 0, 0);
    lv_obj_set_style_pad_all(wifiScreen, 8, 0);
    lv_obj_set_flex_flow(wifiScreen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(wifiScreen, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(wifiScreen, 4, 0);

    /* Title */
    lv_obj_t* title = lv_label_create(wifiScreen);
    lv_label_set_text(title, LV_SYMBOL_WIFI " WiFi Setup");
    lv_obj_set_style_text_color(title, lv_color_hex(WCS_ORANGE), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    /* Status */
    lblStatus = lv_label_create(wifiScreen);
    lv_label_set_text(lblStatus, "Scanning...");
    lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(lblStatus, &lv_font_montserrat_12, 0);

    /* Spinner (hidden until connecting) */
    spinner = lv_spinner_create(wifiScreen, 1000, 60);
    lv_obj_set_size(spinner, 24, 24);
    lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);

    /* Network list — scrollable */
    networkList = lv_obj_create(wifiScreen);
    lv_obj_set_size(networkList, LV_PCT(100), 140);
    lv_obj_set_style_bg_color(networkList, lv_color_hex(WCS_BG_SURFACE), 0);
    lv_obj_set_style_border_width(networkList, 1, 0);
    lv_obj_set_style_border_color(networkList, lv_color_hex(WCS_ORANGE_DARK), 0);
    lv_obj_set_style_radius(networkList, 8, 0);
    lv_obj_set_style_pad_all(networkList, 4, 0);
    lv_obj_set_style_pad_gap(networkList, 3, 0);
    lv_obj_set_flex_flow(networkList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(networkList, LV_DIR_VER);
    lv_obj_set_style_shadow_width(networkList, 0, 0);

    /* Button row: Scan | Skip */
    lv_obj_t* btnRow = lv_obj_create(wifiScreen);
    lv_obj_set_size(btnRow, LV_PCT(100), 36);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    lv_obj_set_style_shadow_width(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* scanBtn = lv_btn_create(btnRow);
    lv_obj_set_size(scanBtn, 100, 30);
    lv_obj_set_style_bg_color(scanBtn, lv_color_hex(WCS_BUTTON_BG), 0);
    lv_obj_set_style_bg_color(scanBtn, lv_color_hex(WCS_BUTTON_ACTIVE), LV_STATE_PRESSED);
    lv_obj_set_style_radius(scanBtn, 6, 0);
    lv_obj_set_style_border_width(scanBtn, 0, 0);
    lv_obj_add_event_cb(scanBtn, _scan_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* scanLbl = lv_label_create(scanBtn);
    lv_label_set_text(scanLbl, "Scan");
    lv_obj_center(scanLbl);

    lv_obj_t* skipBtn = lv_btn_create(btnRow);
    lv_obj_set_size(skipBtn, 100, 30);
    lv_obj_set_style_bg_color(skipBtn, lv_color_hex(WCS_BUTTON_BG), 0);
    lv_obj_set_style_radius(skipBtn, 6, 0);
    lv_obj_set_style_border_width(skipBtn, 0, 0);
    lv_obj_add_event_cb(skipBtn, _skip_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* skipLbl = lv_label_create(skipBtn);
    lv_label_set_text(skipLbl, "Skip");
    lv_obj_center(skipLbl);

    /* Password input panel (hidden until network selected) */
    passPanel = lv_obj_create(wifiScreen);
    lv_obj_set_size(passPanel, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(passPanel, lv_color_hex(WCS_BG_CARD), 0);
    lv_obj_set_style_border_width(passPanel, 1, 0);
    lv_obj_set_style_border_color(passPanel, lv_color_hex(WCS_ORANGE), 0);
    lv_obj_set_style_radius(passPanel, 8, 0);
    lv_obj_set_style_pad_all(passPanel, 6, 0);
    lv_obj_set_flex_flow(passPanel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_shadow_width(passPanel, 0, 0);
    lv_obj_add_flag(passPanel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* passLbl = lv_label_create(passPanel);
    lv_label_set_text(passLbl, "Password:");
    lv_obj_set_style_text_color(passLbl, lv_color_hex(WCS_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(passLbl, &lv_font_montserrat_12, 0);

    passArea = lv_textarea_create(passPanel);
    lv_textarea_set_one_line(passArea, true);
    lv_textarea_set_password_mode(passArea, true);
    lv_textarea_set_placeholder_text(passArea, "WiFi password");
    lv_obj_set_width(passArea, LV_PCT(100));
    lv_obj_set_style_bg_color(passArea, lv_color_hex(WCS_BG_INPUT), 0);
    lv_obj_set_style_text_color(passArea, lv_color_hex(WCS_TEXT_PRIMARY), 0);

    keyboard = lv_keyboard_create(wifiScreen);
    lv_keyboard_set_textarea(keyboard, passArea);
    lv_obj_set_size(keyboard, LV_PCT(100), 120);
    lv_obj_add_event_cb(keyboard, _kb_event_cb, LV_EVENT_ALL, nullptr);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);

    /* Show keyboard when passArea is focused */
    lv_obj_add_event_cb(passArea, [](lv_event_t* e) {
        if (keyboard) lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(passArea, [](lv_event_t* e) {
        if (keyboard) lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_DEFOCUSED, nullptr);

    /* Initial scan */
    _scan_and_populate();

    Serial.println("[WIFI] WiFi setup screen shown");
}

void wifi_setup_destroy() {
    WiFi.scanDelete();
    if (wifiScreen) {
        lv_obj_del(wifiScreen);
        wifiScreen = nullptr;
    }
}
