#include "boot_splash.h"
#include "img_grudge_logo.h"
#include "theme.h"
#include <lvgl.h>

static lv_obj_t *splashScreen = nullptr;
static lv_obj_t *lblLoading = nullptr;
static lv_obj_t *barProgress = nullptr;
static lv_obj_t *imgIntro = nullptr;
static lv_timer_t *introAnim = nullptr;
static uint8_t introFrame = 0;
static uint8_t bootProgress = 0;

/* ── Intro animation callback (unused for static test card) ─ */
static void _intro_anim_cb(lv_timer_t *) {
  /* No-op for color test — will be restored for production splash */
}

static void _draw_intro(lv_obj_t *parent) {
  imgIntro = lv_img_create(parent);
  lv_img_set_src(imgIntro, &grudge_logo_dsc);
  lv_obj_align(imgIntro, LV_ALIGN_CENTER, 0, -30);
}

/* ── Public API ───────────────────────────────────── */

void boot_splash_show() {
  splashScreen = lv_obj_create(lv_scr_act());
  lv_obj_set_size(splashScreen, SCREEN_W, SCREEN_H);
  lv_obj_align(splashScreen, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(splashScreen, lv_color_hex(WCS_BG_DARK), 0);
  lv_obj_set_style_border_width(splashScreen, 0, 0);
  lv_obj_set_style_radius(splashScreen, 0, 0);

  _draw_intro(splashScreen);

  /* Loading text */
  lblLoading = lv_label_create(splashScreen);
  lv_label_set_text(lblLoading, "Initializing GRD-17...");
  lv_obj_set_style_text_font(lblLoading, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lblLoading, lv_color_hex(WCS_TEXT_SECONDARY), 0);
  lv_obj_set_style_bg_color(lblLoading, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(lblLoading, LV_OPA_40, 0);
  lv_obj_set_style_pad_all(lblLoading, 4, 0);
  lv_obj_set_style_radius(lblLoading, 6, 0);
  lv_obj_align(lblLoading, LV_ALIGN_BOTTOM_MID, 0, -54);

  /* Progress bar */
  barProgress = lv_bar_create(splashScreen);
  lv_obj_set_size(barProgress, 196, 8);
  lv_obj_align(barProgress, LV_ALIGN_BOTTOM_MID, 0, -32);
  lv_bar_set_range(barProgress, 0, 100);
  lv_bar_set_value(barProgress, 0, LV_ANIM_ON);
  lv_obj_set_style_bg_color(barProgress, lv_color_hex(WCS_BG_SURFACE), 0);
  lv_obj_set_style_bg_color(barProgress, lv_color_hex(WCS_BUTTON_ACTIVE),
                            LV_PART_INDICATOR);
  lv_obj_set_style_radius(barProgress, 8, 0);
  lv_obj_set_style_radius(barProgress, 8, LV_PART_INDICATOR);

  /* Version footer */
  lv_obj_t *ver = lv_label_create(splashScreen);
  lv_label_set_text(ver, "v" GRUDA_VERSION " | RacAlvin The Pirate King");
  lv_obj_set_style_text_font(ver, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(ver, lv_color_hex(WCS_TEXT_MUTED), 0);
  lv_obj_align(ver, LV_ALIGN_BOTTOM_MID, 0, -10);

  bootProgress = 0;
}

void boot_splash_set_progress(uint8_t pct, const char *msg) {
  bootProgress = pct;
  if (barProgress)
    lv_bar_set_value(barProgress, pct, LV_ANIM_ON);
  if (lblLoading && msg)
    lv_label_set_text(lblLoading, msg);
}

void boot_splash_hide() {
  if (introAnim) {
    lv_timer_del(introAnim);
    introAnim = nullptr;
  }
  if (splashScreen) {
    lv_obj_del(splashScreen);
    splashScreen = nullptr;
  }
}

bool boot_splash_active() { return splashScreen != nullptr; }
