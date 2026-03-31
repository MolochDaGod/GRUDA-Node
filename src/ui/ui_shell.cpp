#include "ui_shell.h"
#include "account.h"
#include "ai_admin.h"
#include "img_account_btn.h"
#include "theme.h"
#include "voice_bt.h"
#include <lvgl.h>

/* Account reference — set by main.cpp via ui_shell_set_account() */
static GrudgeAccount *_shellAcct = nullptr;

void ui_shell_set_account(GrudgeAccount *acct) { _shellAcct = acct; }

/* Tab content builders (defined in their own files) */
extern void ui_tab_wallet_create(lv_obj_t *parent);
extern void ui_tab_node_create(lv_obj_t *parent);
extern void ui_tab_treaty_create(lv_obj_t *parent);
extern void ui_tab_treaty_set_context(TreatyState *state, const GrudaWallet *wallet);
extern void ui_tab_vote_create(lv_obj_t *parent);
extern void ui_tab_alerts_create(lv_obj_t *parent);
extern void ui_tab_nft_create(lv_obj_t *parent);
extern void ui_tab_character_create(lv_obj_t *parent);
extern void ui_tab_voice_create(lv_obj_t *parent);
extern void ui_tab_wallet_update(const GrudaWallet &w, float balance);
extern void ui_tab_node_update(const GRD17NodeState &s);
extern void ui_tab_treaty_refresh(const TreatyState &state);
extern void ui_tab_alerts_refresh(const AlertState &state);
extern void ui_tab_voice_refresh(const VoiceBTState &vbt, const AIAdminState &ai);

#define HEADER_H 28  /* compact status strip */

static lv_obj_t *statusBar = nullptr;
static lv_obj_t *lblWifi = nullptr;
static lv_obj_t *lblBle = nullptr;
static lv_obj_t *lblId = nullptr;
static lv_obj_t *lblUptime = nullptr;
static lv_obj_t *accountPanel = nullptr;

static void _close_account_panel(lv_event_t *) {
  if (accountPanel) {
    lv_obj_del(accountPanel);
    accountPanel = nullptr;
  }
}

/* Logout callback — wipes session and restarts */
static void _logout_cb(lv_event_t *) {
  _close_account_panel(nullptr);
  if (_shellAcct) {
    account_logout(*_shellAcct);
  }
  ESP.restart();
}

static void _open_account_panel(lv_event_t *) {
  if (accountPanel) {
    _close_account_panel(nullptr);
    return;
  }

  accountPanel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(accountPanel, SCREEN_W - 24, 220);
  lv_obj_center(accountPanel);
  lv_obj_set_style_bg_color(accountPanel, lv_color_hex(WCS_OVERLAY_BG), 0);
  lv_obj_set_style_border_width(accountPanel, 1, 0);
  lv_obj_set_style_border_color(accountPanel, lv_color_hex(WCS_ORANGE_DARK), 0);
  lv_obj_set_style_radius(accountPanel, 12, 0);
  lv_obj_set_style_pad_all(accountPanel, 12, 0);
  lv_obj_set_style_shadow_width(accountPanel, 0, 0);
  lv_obj_set_style_text_color(accountPanel, lv_color_hex(WCS_BUTTON_TEXT), 0);
  lv_obj_set_flex_flow(accountPanel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(accountPanel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);

  /* Title */
  lv_obj_t *title = lv_label_create(accountPanel);
  lv_label_set_text(title, "Grudge Account");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(WCS_ORANGE), 0);

  /* Logged-in user */
  lv_obj_t *userLine = lv_label_create(accountPanel);
  if (_shellAcct && account_is_logged_in(*_shellAcct)) {
    lv_label_set_text_fmt(userLine, LV_SYMBOL_HOME " %s",
                          account_get_display_name(*_shellAcct));
  } else {
    lv_label_set_text(userLine, LV_SYMBOL_HOME " Not logged in");
  }
  lv_obj_set_style_text_color(userLine, lv_color_hex(WCS_TEXT_PRIMARY), 0);

  /* Grudge ID */
  lv_obj_t *idLine = lv_label_create(accountPanel);
  if (_shellAcct && account_is_logged_in(*_shellAcct)) {
    lv_label_set_text_fmt(idLine, "ID: %s",
                          account_get_grudge_id(*_shellAcct));
  } else {
    lv_label_set_text(idLine, "ID: --");
  }
  lv_obj_set_style_text_color(idLine, lv_color_hex(WCS_TEXT_SECONDARY), 0);
  lv_obj_set_style_text_font(idLine, &lv_font_montserrat_12, 0);

  /* WiFi */
  lv_obj_t *wifiLine = lv_label_create(accountPanel);
  lv_label_set_text_fmt(wifiLine, "WiFi: %s",
                        lblWifi ? lv_label_get_text(lblWifi) : "--");
  lv_obj_set_style_text_color(wifiLine, lv_color_hex(WCS_TEXT_SECONDARY), 0);
  lv_obj_set_style_text_font(wifiLine, &lv_font_montserrat_12, 0);

  /* Node */
  lv_obj_t *nodeLine = lv_label_create(accountPanel);
  lv_label_set_text_fmt(nodeLine, "Node: %s",
                        lblId ? lv_label_get_text(lblId) : "GRD-17");
  lv_obj_set_style_text_color(nodeLine, lv_color_hex(WCS_TEXT_SECONDARY), 0);
  lv_obj_set_style_text_font(nodeLine, &lv_font_montserrat_12, 0);

  /* Button row */
  lv_obj_t *btnRow = lv_obj_create(accountPanel);
  lv_obj_set_size(btnRow, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btnRow, 0, 0);
  lv_obj_set_style_pad_all(btnRow, 0, 0);
  lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  /* Close button */
  lv_obj_t *closeBtn = lv_btn_create(btnRow);
  lv_obj_set_size(closeBtn, 88, 34);
  lv_obj_set_style_radius(closeBtn, 8, 0);
  lv_obj_set_style_bg_color(closeBtn, lv_color_hex(WCS_BUTTON_ACTIVE), 0);
  lv_obj_add_event_cb(closeBtn, _close_account_panel, LV_EVENT_CLICKED,
                      nullptr);
  lv_obj_t *closeLbl = lv_label_create(closeBtn);
  lv_label_set_text(closeLbl, "Close");
  lv_obj_center(closeLbl);

  /* Logout button */
  lv_obj_t *logoutBtn = lv_btn_create(btnRow);
  lv_obj_set_size(logoutBtn, 88, 34);
  lv_obj_set_style_radius(logoutBtn, 8, 0);
  lv_obj_set_style_bg_color(logoutBtn, lv_color_hex(WCS_RED), 0);
  lv_obj_add_event_cb(logoutBtn, _logout_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *logoutLbl = lv_label_create(logoutBtn);
  lv_label_set_text(logoutLbl, "Log Out");
  lv_obj_center(logoutLbl);
}

#define COMPACT_HEADER_H 28

/* ── Compact Status Bar (WiFi | NodeID | Uptime | Acct) ── */
static void _create_status_bar(lv_obj_t *parent) {
  statusBar = lv_obj_create(parent);
  lv_obj_set_size(statusBar, SCREEN_W, COMPACT_HEADER_H);
  lv_obj_align(statusBar, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_border_width(statusBar, 0, 0);
  lv_obj_set_style_pad_left(statusBar, 6, 0);
  lv_obj_set_style_pad_right(statusBar, 6, 0);
  lv_obj_set_style_pad_top(statusBar, 4, 0);
  lv_obj_set_style_pad_bottom(statusBar, 4, 0);
  lv_obj_set_style_radius(statusBar, 0, 0);
  lv_obj_set_style_bg_color(statusBar, lv_color_hex(WCS_STATUSBAR_BG), 0);
  lv_obj_set_style_bg_opa(statusBar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(statusBar, 1, 0);
  lv_obj_set_style_border_side(statusBar, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_color(statusBar, lv_color_hex(WCS_ORANGE_DARK), 0);

  /* WiFi — left */
  lblWifi = lv_label_create(statusBar);
  lv_label_set_text(lblWifi, LV_SYMBOL_WIFI " --");
  lv_obj_set_style_text_color(lblWifi, lv_color_hex(WCS_STATUSBAR_TEXT), 0);
  lv_obj_set_style_text_font(lblWifi, &lv_font_montserrat_12, 0);
  lv_obj_align(lblWifi, LV_ALIGN_LEFT_MID, 0, 0);

  /* BLE — next to WiFi */
  lblBle = lv_label_create(statusBar);
  lv_label_set_text(lblBle, LV_SYMBOL_BLUETOOTH);
  lv_obj_set_style_text_color(lblBle, lv_color_hex(WCS_TEXT_MUTED), 0);
  lv_obj_set_style_text_font(lblBle, &lv_font_montserrat_12, 0);
  lv_obj_align(lblBle, LV_ALIGN_LEFT_MID, 50, 0);

  /* Node ID — center */
  lblId = lv_label_create(statusBar);
  lv_label_set_text(lblId, "GRD-17");
  lv_obj_set_style_text_color(lblId, lv_color_hex(WCS_ORANGE), 0);
  lv_obj_set_style_text_font(lblId, &lv_font_montserrat_12, 0);
  lv_obj_align(lblId, LV_ALIGN_CENTER, 0, 0);

  /* Uptime — right of center */
  lblUptime = lv_label_create(statusBar);
  lv_label_set_text(lblUptime, "0:00");
  lv_obj_set_style_text_color(lblUptime, lv_color_hex(WCS_STATUSBAR_TEXT), 0);
  lv_obj_set_style_text_font(lblUptime, &lv_font_montserrat_12, 0);
  lv_obj_align(lblUptime, LV_ALIGN_RIGHT_MID, -30, 0);

  /* Account button — far right */
  lv_obj_t *accBtn = lv_btn_create(statusBar);
  lv_obj_set_size(accBtn, 22, 22);
  lv_obj_align(accBtn, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_style_bg_color(accBtn, lv_color_hex(WCS_BUTTON_BG), 0);
  lv_obj_set_style_bg_color(accBtn, lv_color_hex(WCS_BUTTON_ACTIVE),
                            LV_STATE_PRESSED);
  lv_obj_set_style_border_width(accBtn, 0, 0);
  lv_obj_set_style_radius(accBtn, 6, 0);
  lv_obj_set_style_pad_all(accBtn, 1, 0);
  lv_obj_set_style_shadow_width(accBtn, 0, 0);
  lv_obj_t *accImg = lv_img_create(accBtn);
  lv_img_set_src(accImg, &account_btn_dsc);
  lv_obj_center(accImg);
  lv_obj_add_event_cb(accBtn, _open_account_panel, LV_EVENT_CLICKED, nullptr);
}

/* ── Build UI ──────────────────────────────────── */
void ui_shell_create() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(WCS_BG_DARK), 0);

  _create_status_bar(scr);

  /* Tabview below the Grudge header */
  lv_obj_t *tv = lv_tabview_create(scr, LV_DIR_BOTTOM, 36);
  lv_obj_set_size(tv, SCREEN_W, SCREEN_H - HEADER_H);
  lv_obj_align(tv, LV_ALIGN_TOP_LEFT, 0, HEADER_H);
  lv_obj_set_style_bg_color(tv, lv_color_hex(WCS_BG_DARK), 0);
  lv_obj_set_style_border_width(tv, 0, 0);

  /* Tab bar styling — touch-friendly icon buttons */
  lv_obj_t *tabBtns = lv_tabview_get_tab_btns(tv);
  lv_obj_set_height(tabBtns, 52);
  lv_obj_set_style_bg_color(tabBtns, lv_color_hex(WCS_BG_SURFACE), 0);
  lv_obj_set_style_pad_all(tabBtns, 6, 0);
  lv_obj_set_style_pad_gap(tabBtns, 8, 0);
  lv_obj_set_style_text_color(tabBtns, lv_color_hex(WCS_TEXT_MUTED), 0);
  lv_obj_set_style_text_color(tabBtns, lv_color_hex(WCS_BUTTON_TEXT),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(tabBtns, lv_color_hex(WCS_BG_SURFACE),
                            LV_PART_ITEMS);
  lv_obj_set_style_bg_color(tabBtns, lv_color_hex(WCS_BUTTON_ACTIVE),
                            LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_radius(tabBtns, 10, LV_PART_ITEMS);
  lv_obj_set_style_border_width(tabBtns, 0, LV_PART_ITEMS);
  lv_obj_set_style_pad_ver(tabBtns, 10, LV_PART_ITEMS);
  lv_obj_set_style_pad_hor(tabBtns, 10, LV_PART_ITEMS);
  lv_obj_set_style_text_font(tabBtns, &lv_font_montserrat_16, 0);

  /* Create tabs: Character | NFT | Treaty | Voice | Alerts | Settings */
  lv_obj_t *tabChar     = lv_tabview_add_tab(tv, LV_SYMBOL_HOME);
  lv_obj_t *tabNft      = lv_tabview_add_tab(tv, LV_SYMBOL_IMAGE);
  lv_obj_t *tabTreaty   = lv_tabview_add_tab(tv, LV_SYMBOL_ENVELOPE);
  lv_obj_t *tabVoice    = lv_tabview_add_tab(tv, LV_SYMBOL_AUDIO);
  lv_obj_t *tabAlerts   = lv_tabview_add_tab(tv, LV_SYMBOL_BELL);
  lv_obj_t *tabSettings = lv_tabview_add_tab(tv, LV_SYMBOL_SETTINGS);

  /* Populate each tab */
  ui_tab_character_create(tabChar);  /* Grudge Warlord race card */
  ui_tab_nft_create(tabNft);
  ui_tab_treaty_create(tabTreaty);
  ui_tab_voice_create(tabVoice);
  ui_tab_alerts_create(tabAlerts);
  ui_tab_node_create(tabSettings);
}

/* ── Update helpers (called from loop) ────────────── */
void ui_shell_set_wifi(bool connected, int8_t rssi) {
  if (!lblWifi)
    return;
  if (connected) {
    lv_label_set_text_fmt(lblWifi, LV_SYMBOL_WIFI " %ddB", rssi);
    lv_obj_set_style_text_color(lblWifi, lv_color_hex(0x00ff88), 0);
  } else {
    lv_label_set_text(lblWifi, LV_SYMBOL_WIFI " --");
    lv_obj_set_style_text_color(lblWifi, lv_color_hex(0xff4444), 0);
  }
}

void ui_shell_set_grudge_id(const char *id) {
  if (lblId)
    lv_label_set_text(lblId, id);
}

void ui_shell_set_uptime(uint32_t seconds) {
  if (!lblUptime)
    return;
  uint32_t h = seconds / 3600;
  uint32_t m = (seconds % 3600) / 60;
  lv_label_set_text_fmt(lblUptime, "%u:%02u", h, m);
}

void ui_shell_set_ble(bool connected) {
  if (!lblBle) return;
  if (connected) {
    lv_obj_set_style_text_color(lblBle, lv_color_hex(WCS_BLUE), 0);
  } else {
    lv_obj_set_style_text_color(lblBle, lv_color_hex(WCS_TEXT_MUTED), 0);
  }
}

void ui_shell_update_tabs(const GrudaWallet &wallet,
                          const GRD17NodeState &nodeState,
                          const TreatyState &treatyState,
                          const VotingState &votingState,
                          const AlertState &alertState,
                          const VoiceBTState &voiceState,
                          const AIAdminState &aiState,
                          float balance) {
  ui_tab_wallet_update(wallet, balance);
  ui_tab_node_update(nodeState);
  ui_tab_treaty_refresh(treatyState);
  ui_tab_alerts_refresh(alertState);
  ui_tab_voice_refresh(voiceState, aiState);
}
