#include "theme.h"
#include "wallet.h"
#include <lvgl.h>

static lv_obj_t *lblPubkey = nullptr;
static lv_obj_t *lblBalance = nullptr;

void ui_tab_wallet_create(lv_obj_t *parent) {
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(parent, 10, 0);
  lv_obj_set_style_pad_gap(parent, 8, 0);

  /* Title */
  lv_obj_t *t = lv_label_create(parent);
  lv_label_set_text(t, "GRUDA Wallet");
  lv_obj_set_style_text_color(t, lv_color_hex(WCS_GREEN), 0);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);

  /* Public key */
  lblPubkey = lv_label_create(parent);
  lv_label_set_long_mode(lblPubkey, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lblPubkey, 220);
  lv_label_set_text(lblPubkey, "Loading...");
  lv_obj_set_style_text_color(lblPubkey, lv_color_hex(WCS_TEXT_SECONDARY), 0);
  lv_obj_set_style_text_font(lblPubkey, &lv_font_montserrat_12, 0);

  /* Balance row: text-only amount */
  lv_obj_t *balRow = lv_obj_create(parent);
  lv_obj_set_size(balRow, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(balRow, lv_color_hex(WCS_BG_CARD), 0);
  lv_obj_set_style_border_width(balRow, 0, 0);
  lv_obj_set_style_radius(balRow, 12, 0);
  lv_obj_set_style_pad_all(balRow, 10, 0);
  lv_obj_set_flex_flow(balRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(balRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  /* Balance amount */
  lblBalance = lv_label_create(balRow);
  lv_label_set_text(lblBalance, "0.0000 GRUDA");
  lv_obj_set_style_text_color(lblBalance, lv_color_hex(WCS_TEXT_PRIMARY), 0);
  lv_obj_set_style_text_font(lblBalance, &lv_font_montserrat_20, 0);
  lv_obj_set_style_pad_left(lblBalance, 8, 0);

  /* Provider badge */
  lv_obj_t *badge = lv_label_create(parent);
  lv_label_set_text(badge, "grudge-wallet | Solana");
  lv_obj_set_style_text_color(badge, lv_color_hex(WCS_TEXT_MUTED), 0);
  lv_obj_set_style_text_font(badge, &lv_font_montserrat_12, 0);
}

/* Called from main loop to refresh wallet display */
void ui_tab_wallet_update(const GrudaWallet &w, float balance) {
  if (lblPubkey && w.initialized) {
    String shortKey =
        w.publicKeyHex.substring(0, 16) + "..." + w.publicKeyHex.substring(112);
    lv_label_set_text(lblPubkey, shortKey.c_str());
  }
  if (lblBalance) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.4f GRUDA", balance);
    lv_label_set_text(lblBalance, buf);
  }
}
