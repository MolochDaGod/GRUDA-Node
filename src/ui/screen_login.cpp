#include "account.h"
#include "config.h"
#include "img_grudge_logo.h"
#include "theme.h"
#include <lvgl.h>

/**
 * Login Screen — Grudge Account Device Pairing
 * Shows logo + 6-char pairing code + instructions.
 * Polls backend every AUTH_POLL_INTERVAL_MS.
 * On success, calls the provided callback to transition to main UI.
 */

/* Forward declaration — main.cpp provides this after login */
extern void on_login_success();

static lv_obj_t *loginScreen   = nullptr;
static lv_obj_t *lblCode       = nullptr;
static lv_obj_t *lblStatus     = nullptr;
static lv_timer_t *pollTimer   = nullptr;

static GrudgeAccount *_acctRef = nullptr;
static DevicePairingCode _pairingCode;

/* ── Poll timer callback ──────────────────────────── */
static void _poll_timer_cb(lv_timer_t *timer) {
    (void)timer;
    if (!_acctRef || !_pairingCode.active) return;

    /* Update status indicator */
    lv_label_set_text(lblStatus, "Checking...");
    lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_YELLOW), 0);

    bool ok = account_poll_auth(*_acctRef, _pairingCode);

    if (ok) {
        /* Auth succeeded — stop polling */
        _pairingCode.active = false;
        if (pollTimer) {
            lv_timer_del(pollTimer);
            pollTimer = nullptr;
        }

        lv_label_set_text(lblStatus, "Logged in!");
        lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_GREEN), 0);

        /* Brief delay so user sees success, then transition */
        lv_timer_create(
            [](lv_timer_t *t) {
                lv_timer_del(t);
                /* Remove login screen */
                if (loginScreen) {
                    lv_obj_del(loginScreen);
                    loginScreen = nullptr;
                }
                on_login_success();
            },
            800, nullptr);
    } else {
        lv_label_set_text(lblStatus, "Waiting for login...");
        lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_TEXT_MUTED), 0);
    }

    /* Expire pairing code after 5 minutes — generate a new one */
    if (_pairingCode.active &&
        (millis() - _pairingCode.createdAt) > PAIRING_CODE_EXPIRE_MS) {
        account_generate_pairing_code(_pairingCode);
        lv_label_set_text(lblCode, _pairingCode.code);
    }
}

/* ── Regenerate button callback ───────────────────── */
static void _regen_cb(lv_event_t *e) {
    (void)e;
    account_generate_pairing_code(_pairingCode);
    lv_label_set_text(lblCode, _pairingCode.code);
    lv_label_set_text(lblStatus, "New code generated");
    lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_ORANGE), 0);
}

/* ── Public: Show Login Screen ────────────────────── */
void login_screen_show(GrudgeAccount &acct) {
    _acctRef = &acct;

    /* Generate initial pairing code */
    account_generate_pairing_code(_pairingCode);

    /* Full-screen container */
    loginScreen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(loginScreen, SCREEN_W, SCREEN_H);
    lv_obj_align(loginScreen, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(loginScreen, lv_color_hex(WCS_BG_DARK), 0);
    lv_obj_set_style_bg_opa(loginScreen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(loginScreen, 0, 0);
    lv_obj_set_style_radius(loginScreen, 0, 0);
    lv_obj_set_style_pad_all(loginScreen, 0, 0);
    lv_obj_set_flex_flow(loginScreen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(loginScreen, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(loginScreen, 6, 0);

    /* Grudge Logo */
    lv_obj_t *logo = lv_img_create(loginScreen);
    lv_img_set_src(logo, &grudge_logo_dsc);
    lv_img_set_zoom(logo, 180); /* scale down to fit */

    /* Title */
    lv_obj_t *title = lv_label_create(loginScreen);
    lv_label_set_text(title, "GRUDGE NODE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(WCS_ORANGE), 0);
    lv_obj_set_style_text_letter_space(title, 3, 0);

    /* Instruction */
    lv_obj_t *inst = lv_label_create(loginScreen);
    lv_label_set_text(inst, "Enter this code at");
    lv_obj_set_style_text_color(inst, lv_color_hex(WCS_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(inst, &lv_font_montserrat_12, 0);

    lv_obj_t *urlLbl = lv_label_create(loginScreen);
    lv_label_set_text(urlLbl, "id.grudge-studio.com/device");
    lv_obj_set_style_text_color(urlLbl, lv_color_hex(WCS_BLUE), 0);
    lv_obj_set_style_text_font(urlLbl, &lv_font_montserrat_14, 0);

    /* Pairing Code — large, prominent */
    lv_obj_t *codeBox = lv_obj_create(loginScreen);
    lv_obj_set_size(codeBox, 200, 52);
    lv_obj_set_style_bg_color(codeBox, lv_color_hex(WCS_BG_CARD), 0);
    lv_obj_set_style_border_width(codeBox, 2, 0);
    lv_obj_set_style_border_color(codeBox, lv_color_hex(WCS_ORANGE), 0);
    lv_obj_set_style_radius(codeBox, 12, 0);
    lv_obj_set_style_pad_all(codeBox, 0, 0);

    lblCode = lv_label_create(codeBox);
    lv_label_set_text(lblCode, _pairingCode.code);
    lv_obj_center(lblCode);
    lv_obj_set_style_text_font(lblCode, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lblCode, lv_color_hex(WCS_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_letter_space(lblCode, 8, 0);

    /* Status label */
    lblStatus = lv_label_create(loginScreen);
    lv_label_set_text(lblStatus, "Waiting for login...");
    lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_TEXT_MUTED), 0);
    lv_obj_set_style_text_font(lblStatus, &lv_font_montserrat_12, 0);

    /* Regenerate button */
    lv_obj_t *regenBtn = lv_btn_create(loginScreen);
    lv_obj_set_size(regenBtn, 140, 34);
    lv_obj_set_style_bg_color(regenBtn, lv_color_hex(WCS_BUTTON_BG), 0);
    lv_obj_set_style_bg_color(regenBtn, lv_color_hex(WCS_BUTTON_ACTIVE),
                              LV_STATE_PRESSED);
    lv_obj_set_style_border_width(regenBtn, 1, 0);
    lv_obj_set_style_border_color(regenBtn, lv_color_hex(WCS_ORANGE_DARK), 0);
    lv_obj_set_style_radius(regenBtn, 8, 0);
    lv_obj_add_event_cb(regenBtn, _regen_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *regenLbl = lv_label_create(regenBtn);
    lv_label_set_text(regenLbl, "New Code");
    lv_obj_center(regenLbl);
    lv_obj_set_style_text_font(regenLbl, &lv_font_montserrat_12, 0);

    /* Start polling timer */
    pollTimer = lv_timer_create(_poll_timer_cb, AUTH_POLL_INTERVAL_MS, nullptr);

    Serial.println("[LOGIN] Login screen shown");
}

/* ── Public: Destroy login screen (if still alive) ── */
void login_screen_destroy() {
    if (pollTimer) {
        lv_timer_del(pollTimer);
        pollTimer = nullptr;
    }
    if (loginScreen) {
        lv_obj_del(loginScreen);
        loginScreen = nullptr;
    }
}
