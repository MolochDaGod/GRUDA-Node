#include "account.h"
#include "config.h"
#include "img_grudge_logo.h"
#include "theme.h"
#include "wallet.h"
#include <lvgl.h>

/**
 * Account Setup Screen — self-contained account creation.
 * Three options:
 *   1) Create new Grudge account (enter display name → provision via backend)
 *   2) Pair with existing account (pairing code flow)
 *   3) Factory reset (wipe all NVS data)
 *
 * Created by RacAlvin The Pirate King for GRUDGE STUDIO
 */

/* Callbacks from main.cpp */
extern void on_login_success();

static GrudgeAccount* _acctRef = nullptr;
static GrudaWallet*   _walletRef = nullptr;

static lv_obj_t* setupScreen = nullptr;
static lv_obj_t* menuPanel = nullptr;
static lv_obj_t* createPanel = nullptr;
static lv_obj_t* pairPanel = nullptr;
static lv_obj_t* nameArea = nullptr;
static lv_obj_t* keyboard = nullptr;
static lv_obj_t* lblSetupStatus = nullptr;

/* Pairing code state (reused from screen_login.cpp pattern) */
static DevicePairingCode _pairingCode;
static lv_obj_t* lblPairCode = nullptr;
static lv_obj_t* lblPairStatus = nullptr;
static lv_timer_t* pairPollTimer = nullptr;

/* ── Show/hide panels ─────────────────────────────── */
static void _show_menu();
static void _show_create();
static void _show_pair();

static void _hide_all_panels() {
    if (menuPanel) lv_obj_add_flag(menuPanel, LV_OBJ_FLAG_HIDDEN);
    if (createPanel) lv_obj_add_flag(createPanel, LV_OBJ_FLAG_HIDDEN);
    if (pairPanel) lv_obj_add_flag(pairPanel, LV_OBJ_FLAG_HIDDEN);
    if (keyboard) lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

/* ── Transition to main UI after successful account ── */
static void _finish_setup() {
    if (pairPollTimer) { lv_timer_del(pairPollTimer); pairPollTimer = nullptr; }
    if (setupScreen) { lv_obj_del(setupScreen); setupScreen = nullptr; }
    on_login_success();
}

/* ── CREATE ACCOUNT flow ──────────────────────────── */
static void _create_submit_cb(lv_event_t* e) {
    (void)e;
    if (!nameArea || !_acctRef || !_walletRef) return;

    String name = String(lv_textarea_get_text(nameArea));
    name.trim();
    if (name.length() < 2) {
        if (lblSetupStatus) {
            lv_label_set_text(lblSetupStatus, "Name too short (min 2 chars)");
            lv_obj_set_style_text_color(lblSetupStatus, lv_color_hex(WCS_RED), 0);
        }
        return;
    }

    if (lblSetupStatus) {
        lv_label_set_text(lblSetupStatus, "Creating account...");
        lv_obj_set_style_text_color(lblSetupStatus, lv_color_hex(WCS_YELLOW), 0);
    }
    if (keyboard) lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_timer_handler();

    /* Ensure wallet UUID exists */
    if (!_walletRef->initialized) wallet_init(*_walletRef);

    bool ok = account_provision(*_acctRef, name, _walletRef->deviceUUID);

    if (ok) {
        if (lblSetupStatus) {
            lv_label_set_text_fmt(lblSetupStatus, "Welcome, %s!",
                                  account_get_display_name(*_acctRef));
            lv_obj_set_style_text_color(lblSetupStatus, lv_color_hex(WCS_GREEN), 0);
        }
        lv_timer_handler();
        delay(600);
        _finish_setup();
    } else {
        if (lblSetupStatus) {
            lv_label_set_text(lblSetupStatus, "Failed! Check WiFi and retry.");
            lv_obj_set_style_text_color(lblSetupStatus, lv_color_hex(WCS_RED), 0);
        }
    }
}

static void _create_kb_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) _create_submit_cb(e);
    else if (code == LV_EVENT_CANCEL) _show_menu();
}

/* ── PAIR WITH CODE flow ──────────────────────────── */
static void _pair_poll_cb(lv_timer_t* t) {
    (void)t;
    if (!_acctRef || !_pairingCode.active) return;

    if (lblPairStatus) {
        lv_label_set_text(lblPairStatus, "Checking...");
        lv_obj_set_style_text_color(lblPairStatus, lv_color_hex(WCS_YELLOW), 0);
    }

    bool ok = account_poll_auth(*_acctRef, _pairingCode);
    if (ok) {
        _pairingCode.active = false;
        if (pairPollTimer) { lv_timer_del(pairPollTimer); pairPollTimer = nullptr; }
        if (lblPairStatus) {
            lv_label_set_text(lblPairStatus, "Logged in!");
            lv_obj_set_style_text_color(lblPairStatus, lv_color_hex(WCS_GREEN), 0);
        }
        lv_timer_handler();
        delay(600);
        _finish_setup();
    } else {
        if (lblPairStatus) {
            lv_label_set_text(lblPairStatus, "Waiting for login...");
            lv_obj_set_style_text_color(lblPairStatus, lv_color_hex(WCS_TEXT_MUTED), 0);
        }
        /* Expire code after 5 min */
        if (_pairingCode.active &&
            (millis() - _pairingCode.createdAt) > PAIRING_CODE_EXPIRE_MS) {
            account_generate_pairing_code(_pairingCode);
            if (lblPairCode) lv_label_set_text(lblPairCode, _pairingCode.code);
        }
    }
}

/* ── FACTORY RESET ────────────────────────────────── */
static void _reset_confirm_cb(lv_event_t* e) {
    (void)e;
    account_factory_reset();
    delay(300);
    ESP.restart();
}

static void _reset_cb(lv_event_t* e) {
    (void)e;
    /* Show confirmation dialog */
    static const char* resetBtns[] = {"Reset", "Cancel", ""};
    lv_obj_t* mbox = lv_msgbox_create(lv_scr_act(), "Factory Reset",
        "This will erase all data.\nAre you sure?",
        resetBtns, false);
    lv_obj_center(mbox);
    lv_obj_set_style_bg_color(mbox, lv_color_hex(WCS_BG_CARD), 0);
    lv_obj_set_style_text_color(mbox, lv_color_hex(WCS_TEXT_PRIMARY), 0);

    lv_obj_add_event_cb(lv_msgbox_get_btns(mbox), [](lv_event_t* ev) {
        lv_obj_t* obj = lv_event_get_current_target(ev);
        uint16_t idx = lv_btnmatrix_get_selected_btn(obj);
        if (idx == 0) {
            _reset_confirm_cb(ev);
        } else {
            lv_msgbox_close(lv_obj_get_parent(obj));
        }
    }, LV_EVENT_VALUE_CHANGED, nullptr);
}

/* ── Panel builders ───────────────────────────────── */
static void _show_menu() {
    _hide_all_panels();
    if (menuPanel) lv_obj_clear_flag(menuPanel, LV_OBJ_FLAG_HIDDEN);
    if (pairPollTimer) { lv_timer_del(pairPollTimer); pairPollTimer = nullptr; }
}

static void _show_create() {
    _hide_all_panels();
    if (createPanel) lv_obj_clear_flag(createPanel, LV_OBJ_FLAG_HIDDEN);
    if (keyboard) {
        lv_keyboard_set_textarea(keyboard, nameArea);
        lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    if (nameArea) lv_textarea_set_text(nameArea, "");
    if (lblSetupStatus) lv_label_set_text(lblSetupStatus, "");
}

static void _show_pair() {
    _hide_all_panels();
    if (pairPanel) lv_obj_clear_flag(pairPanel, LV_OBJ_FLAG_HIDDEN);
    account_generate_pairing_code(_pairingCode);
    if (lblPairCode) lv_label_set_text(lblPairCode, _pairingCode.code);
    if (lblPairStatus) lv_label_set_text(lblPairStatus, "Waiting for login...");
    pairPollTimer = lv_timer_create(_pair_poll_cb, AUTH_POLL_INTERVAL_MS, nullptr);
}

static void _menu_create_cb(lv_event_t*) { _show_create(); }
static void _menu_pair_cb(lv_event_t*)   { _show_pair(); }
static void _back_cb(lv_event_t*)        { _show_menu(); }

/* ── Public: Show Account Setup Screen ────────────── */
void account_setup_show(GrudgeAccount& acct, GrudaWallet& wallet) {
    _acctRef = &acct;
    _walletRef = &wallet;

    setupScreen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(setupScreen, SCREEN_W, SCREEN_H);
    lv_obj_align(setupScreen, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(setupScreen, lv_color_hex(WCS_BG_DARK), 0);
    lv_obj_set_style_bg_opa(setupScreen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(setupScreen, 0, 0);
    lv_obj_set_style_radius(setupScreen, 0, 0);
    lv_obj_set_style_pad_all(setupScreen, 8, 0);
    lv_obj_set_flex_flow(setupScreen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(setupScreen, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(setupScreen, 6, 0);

    /* Logo */
    lv_obj_t* logo = lv_img_create(setupScreen);
    lv_img_set_src(logo, &grudge_logo_dsc);
    lv_img_set_zoom(logo, 140);

    /* Title */
    lv_obj_t* title = lv_label_create(setupScreen);
    lv_label_set_text(title, "Account Setup");
    lv_obj_set_style_text_color(title, lv_color_hex(WCS_ORANGE), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    /* ── Menu Panel (3 buttons) ──────────────────── */
    menuPanel = lv_obj_create(setupScreen);
    lv_obj_set_size(menuPanel, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(menuPanel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(menuPanel, 0, 0);
    lv_obj_set_style_pad_all(menuPanel, 0, 0);
    lv_obj_set_style_pad_gap(menuPanel, 8, 0);
    lv_obj_set_flex_flow(menuPanel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_shadow_width(menuPanel, 0, 0);

    auto mkBtn = [&](const char* text, lv_event_cb_t cb, uint32_t color) {
        lv_obj_t* btn = lv_btn_create(menuPanel);
        lv_obj_set_size(btn, LV_PCT(100), 42);
        lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);
    };

    mkBtn("Create New Account", _menu_create_cb, WCS_ORANGE);
    mkBtn("Pair with Code", _menu_pair_cb, WCS_BUTTON_BG);
    mkBtn("Factory Reset", _reset_cb, WCS_RED);

    /* ── Create Account Panel ────────────────────── */
    createPanel = lv_obj_create(setupScreen);
    lv_obj_set_size(createPanel, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(createPanel, lv_color_hex(WCS_BG_CARD), 0);
    lv_obj_set_style_border_width(createPanel, 1, 0);
    lv_obj_set_style_border_color(createPanel, lv_color_hex(WCS_ORANGE), 0);
    lv_obj_set_style_radius(createPanel, 8, 0);
    lv_obj_set_style_pad_all(createPanel, 8, 0);
    lv_obj_set_flex_flow(createPanel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(createPanel, 4, 0);
    lv_obj_set_style_shadow_width(createPanel, 0, 0);
    lv_obj_add_flag(createPanel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* nameLbl = lv_label_create(createPanel);
    lv_label_set_text(nameLbl, "Choose a display name:");
    lv_obj_set_style_text_color(nameLbl, lv_color_hex(WCS_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_12, 0);

    nameArea = lv_textarea_create(createPanel);
    lv_textarea_set_one_line(nameArea, true);
    lv_textarea_set_max_length(nameArea, 20);
    lv_textarea_set_placeholder_text(nameArea, "RacAlvin");
    lv_obj_set_width(nameArea, LV_PCT(100));
    lv_obj_set_style_bg_color(nameArea, lv_color_hex(WCS_BG_INPUT), 0);
    lv_obj_set_style_text_color(nameArea, lv_color_hex(WCS_TEXT_PRIMARY), 0);

    lblSetupStatus = lv_label_create(createPanel);
    lv_label_set_text(lblSetupStatus, "");
    lv_obj_set_style_text_font(lblSetupStatus, &lv_font_montserrat_12, 0);

    /* Back button */
    lv_obj_t* backBtn = lv_btn_create(createPanel);
    lv_obj_set_size(backBtn, 80, 28);
    lv_obj_set_style_bg_color(backBtn, lv_color_hex(WCS_BUTTON_BG), 0);
    lv_obj_set_style_radius(backBtn, 6, 0);
    lv_obj_set_style_border_width(backBtn, 0, 0);
    lv_obj_add_event_cb(backBtn, _back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* backLbl = lv_label_create(backBtn);
    lv_label_set_text(backLbl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(backLbl);

    /* ── Pair Panel ──────────────────────────────── */
    pairPanel = lv_obj_create(setupScreen);
    lv_obj_set_size(pairPanel, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(pairPanel, lv_color_hex(WCS_BG_CARD), 0);
    lv_obj_set_style_border_width(pairPanel, 1, 0);
    lv_obj_set_style_border_color(pairPanel, lv_color_hex(WCS_ORANGE), 0);
    lv_obj_set_style_radius(pairPanel, 8, 0);
    lv_obj_set_style_pad_all(pairPanel, 8, 0);
    lv_obj_set_flex_flow(pairPanel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(pairPanel, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(pairPanel, 4, 0);
    lv_obj_set_style_shadow_width(pairPanel, 0, 0);
    lv_obj_add_flag(pairPanel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* pairInst = lv_label_create(pairPanel);
    lv_label_set_text(pairInst, "Enter code at:");
    lv_obj_set_style_text_color(pairInst, lv_color_hex(WCS_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(pairInst, &lv_font_montserrat_12, 0);

    lv_obj_t* pairUrl = lv_label_create(pairPanel);
    lv_label_set_text(pairUrl, "id.grudge-studio.com/device");
    lv_obj_set_style_text_color(pairUrl, lv_color_hex(WCS_BLUE), 0);
    lv_obj_set_style_text_font(pairUrl, &lv_font_montserrat_12, 0);

    lblPairCode = lv_label_create(pairPanel);
    lv_label_set_text(lblPairCode, "------");
    lv_obj_set_style_text_font(lblPairCode, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblPairCode, lv_color_hex(WCS_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_letter_space(lblPairCode, 8, 0);

    lblPairStatus = lv_label_create(pairPanel);
    lv_label_set_text(lblPairStatus, "");
    lv_obj_set_style_text_font(lblPairStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblPairStatus, lv_color_hex(WCS_TEXT_MUTED), 0);

    lv_obj_t* pairBackBtn = lv_btn_create(pairPanel);
    lv_obj_set_size(pairBackBtn, 80, 28);
    lv_obj_set_style_bg_color(pairBackBtn, lv_color_hex(WCS_BUTTON_BG), 0);
    lv_obj_set_style_radius(pairBackBtn, 6, 0);
    lv_obj_set_style_border_width(pairBackBtn, 0, 0);
    lv_obj_add_event_cb(pairBackBtn, _back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* pairBackLbl = lv_label_create(pairBackBtn);
    lv_label_set_text(pairBackLbl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(pairBackLbl);

    /* ── Keyboard (shared, hidden by default) ──── */
    keyboard = lv_keyboard_create(setupScreen);
    lv_keyboard_set_textarea(keyboard, nameArea);
    lv_obj_set_size(keyboard, LV_PCT(100), 120);
    lv_obj_add_event_cb(keyboard, _create_kb_cb, LV_EVENT_ALL, nullptr);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);

    /* Show keyboard when nameArea focused */
    lv_obj_add_event_cb(nameArea, [](lv_event_t*) {
        if (keyboard) lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(nameArea, [](lv_event_t*) {
        if (keyboard) lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_DEFOCUSED, nullptr);

    /* Start on menu */
    _show_menu();

    Serial.println("[SETUP] Account setup screen shown");
}

void account_setup_destroy() {
    if (pairPollTimer) { lv_timer_del(pairPollTimer); pairPollTimer = nullptr; }
    if (setupScreen) { lv_obj_del(setupScreen); setupScreen = nullptr; }
}
