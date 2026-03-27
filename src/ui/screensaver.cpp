#include "screensaver.h"
#include "theme.h"
#include <lvgl.h>

static lv_obj_t *ssScreen = nullptr;
static lv_obj_t *lblGruda = nullptr;
static unsigned long lastActivity = 0;
static bool ssActive = false;

void screensaver_show() {
  if (ssActive)
    return;

  ssScreen = lv_obj_create(lv_scr_act());
  lv_obj_set_size(ssScreen, SCREEN_W, SCREEN_H);
  lv_obj_align(ssScreen, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(ssScreen, lv_color_hex(WCS_BG_DARK), 0);
  lv_obj_set_style_border_width(ssScreen, 0, 0);
  lv_obj_set_style_radius(ssScreen, 0, 0);

  /* GRUDA text */
  lblGruda = lv_label_create(ssScreen);
  lv_label_set_text(lblGruda, "GRUDA");
  lv_obj_set_style_text_font(lblGruda, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lblGruda, lv_color_hex(WCS_GOLD_LIGHT), 0);
  lv_obj_set_style_text_letter_space(lblGruda, 8, 0);
  lv_obj_align(lblGruda, LV_ALIGN_CENTER, 0, 50);

  /* Touch hint */
  lv_obj_t *hint = lv_label_create(ssScreen);
  lv_label_set_text(hint, "Touch to wake");
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(hint, lv_color_hex(WCS_TEXT_MUTED), 0);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -15);

  ssActive = true;
}

void screensaver_hide() {
  if (!ssActive)
    return;
  if (ssScreen) {
    lv_obj_del(ssScreen);
    ssScreen = nullptr;
  }
  ssActive = false;
}

bool screensaver_is_active() { return ssActive; }

void screensaver_reset_timer() { lastActivity = millis(); }

void screensaver_check(uint32_t idleTimeoutMs) {
  if (ssActive)
    return;
  if (millis() - lastActivity > idleTimeoutMs) {
    screensaver_show();
  }
}
