#include "theme.h"
#include "wallet.h"
#include <lvgl.h>

static lv_obj_t *lblDeviceId = nullptr;
static lv_obj_t *lblBalance = nullptr;
static lv_obj_t *lblGbux = nullptr;
static lv_obj_t *lblHeap = nullptr;

void ui_tab_wallet_create(lv_obj_t *parent) {
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(parent, 8, 0);
  lv_obj_set_style_pad_gap(parent, 6, 0);

  /* Title */
  lv_obj_t *t = lv_label_create(parent);
  lv_label_set_text(t, LV_SYMBOL_HOME " GRUDA Account");
  lv_obj_set_style_text_color(t, lv_color_hex(WCS_ORANGE), 0);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);

  /* Device ID (UUID, not a wallet key) */
  lblDeviceId = lv_label_create(parent);
  lv_label_set_long_mode(lblDeviceId, LV_LABEL_LONG_DOT);
  lv_obj_set_width(lblDeviceId, 220);
  lv_label_set_text(lblDeviceId, "Loading...");
  lv_obj_set_style_text_color(lblDeviceId, lv_color_hex(WCS_TEXT_SECONDARY), 0);
  lv_obj_set_style_text_font(lblDeviceId, &lv_font_montserrat_12, 0);

  /* GRUDA balance card */
  lv_obj_t *grudaCard = lv_obj_create(parent);
  lv_obj_set_size(grudaCard, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(grudaCard, lv_color_hex(WCS_BG_CARD), 0);
  lv_obj_set_style_border_width(grudaCard, 1, 0);
  lv_obj_set_style_border_color(grudaCard, lv_color_hex(WCS_GOLD_DARK), 0);
  lv_obj_set_style_radius(grudaCard, 10, 0);
  lv_obj_set_style_pad_all(grudaCard, 10, 0);
  lv_obj_set_flex_flow(grudaCard, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(grudaCard, 4, 0);

  lv_obj_t *grudaLabel = lv_label_create(grudaCard);
  lv_label_set_text(grudaLabel, "GRUDA");
  lv_obj_set_style_text_color(grudaLabel, lv_color_hex(WCS_GOLD_LIGHT), 0);
  lv_obj_set_style_text_font(grudaLabel, &lv_font_montserrat_12, 0);

  lblBalance = lv_label_create(grudaCard);
  lv_label_set_text(lblBalance, "0.0000");
  lv_obj_set_style_text_color(lblBalance, lv_color_hex(WCS_TEXT_PRIMARY), 0);
  lv_obj_set_style_text_font(lblBalance, &lv_font_montserrat_16, 0);

  /* GBUX balance card */
  lv_obj_t *gbuxCard = lv_obj_create(parent);
  lv_obj_set_size(gbuxCard, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(gbuxCard, lv_color_hex(WCS_BG_CARD), 0);
  lv_obj_set_style_border_width(gbuxCard, 1, 0);
  lv_obj_set_style_border_color(gbuxCard, lv_color_hex(WCS_BLUE), 0);
  lv_obj_set_style_radius(gbuxCard, 10, 0);
  lv_obj_set_style_pad_all(gbuxCard, 10, 0);
  lv_obj_set_flex_flow(gbuxCard, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(gbuxCard, 4, 0);

  lv_obj_t *gbuxLabel = lv_label_create(gbuxCard);
  lv_label_set_text(gbuxLabel, "GBUX  \xE2\x80\xA2  Solana");
  lv_obj_set_style_text_color(gbuxLabel, lv_color_hex(WCS_BLUE), 0);
  lv_obj_set_style_text_font(gbuxLabel, &lv_font_montserrat_12, 0);

  lblGbux = lv_label_create(gbuxCard);
  lv_label_set_text(lblGbux, "0.0000");
  lv_obj_set_style_text_color(lblGbux, lv_color_hex(WCS_TEXT_PRIMARY), 0);
  lv_obj_set_style_text_font(lblGbux, &lv_font_montserrat_16, 0);

  /* System info */
  lblHeap = lv_label_create(parent);
  lv_label_set_text(lblHeap, "Heap: --");
  lv_obj_set_style_text_color(lblHeap, lv_color_hex(WCS_TEXT_MUTED), 0);
  lv_obj_set_style_text_font(lblHeap, &lv_font_montserrat_12, 0);

  /* Auth badge */
  lv_obj_t *badge = lv_label_create(parent);
  lv_label_set_text(badge, "Web3Auth  \xE2\x80\xA2  Server-Routed");
  lv_obj_set_style_text_color(badge, lv_color_hex(WCS_TEXT_MUTED), 0);
  lv_obj_set_style_text_font(badge, &lv_font_montserrat_12, 0);
}

void ui_tab_wallet_update(const GrudaWallet &w, float balance) {
  if (lblDeviceId && w.initialized) {
    lv_label_set_text_fmt(lblDeviceId, "Device: %s", w.deviceUUID.c_str());
  }
  if (lblBalance) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.4f", balance);
    lv_label_set_text(lblBalance, buf);
  }
  if (lblHeap) {
    lv_label_set_text_fmt(lblHeap, "Heap: %uKB free", ESP.getFreeHeap() / 1024);
  }
}
