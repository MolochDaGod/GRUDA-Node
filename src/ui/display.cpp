#include "display.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

/* Touch diagnostic flag — set from main.cpp serial command */
extern bool touchDiagMode;

static TFT_eSPI tft = TFT_eSPI();

/* ── Separate SPI bus for XPT2046 touch (CYD board) ── */
static SPIClass touchSPI(VSPI);
static XPT2046_Touchscreen ts(TOUCH_SPI_CS);

static lv_disp_draw_buf_t drawBuf;
/* Single 20-line draw buffer — ESP32 DRAM is tight with all modules.
   20 lines * 240px * 2 bytes = 9,600 bytes. */
#define DRAW_BUF_LINES 20
static lv_color_t buf1[SCREEN_W * DRAW_BUF_LINES];
static bool touchHasLast = false;
static uint16_t touchLastX = 0;
static uint16_t touchLastY = 0;

static inline uint16_t _clamp(int32_t v, int32_t lo, int32_t hi) {
  if (v < lo) return (uint16_t)lo;
  if (v > hi) return (uint16_t)hi;
  return (uint16_t)v;
}

/* ── LVGL flush callback ──────────────────────────── */
static void _flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                      lv_color_t *color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(drv);
}

/* ── LVGL touch read callback (XPT2046 on separate SPI) ─ */
static void _touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  (void)drv;

  bool pressed = ts.touched();
  data->state = pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;

  if (pressed) {
    TS_Point p = ts.getPoint();

    /* Map raw ADC (0–4095) to screen pixels */
    int32_t sx = (int32_t)(p.x - TOUCH_RAW_X_MIN) * (SCREEN_W - 1)
                 / (TOUCH_RAW_X_MAX - TOUCH_RAW_X_MIN);
    int32_t sy = (int32_t)(p.y - TOUCH_RAW_Y_MIN) * (SCREEN_H - 1)
                 / (TOUCH_RAW_Y_MAX - TOUCH_RAW_Y_MIN);

    uint16_t x = _clamp(sx, 0, SCREEN_W - 1);
    uint16_t y = _clamp(sy, 0, SCREEN_H - 1);

    /* Diagnostic output */
    if (touchDiagMode) {
      Serial.printf("[TOUCH] raw: %d, %d, z=%d | mapped: %u, %u\n",
                    p.x, p.y, p.z, x, y);
    }

    /* Smoothing */
    if (touchHasLast) {
      x = (uint16_t)((touchLastX * TOUCH_SMOOTH_ALPHA_NUM + x)
                     / TOUCH_SMOOTH_ALPHA_DEN);
      y = (uint16_t)((touchLastY * TOUCH_SMOOTH_ALPHA_NUM + y)
                     / TOUCH_SMOOTH_ALPHA_DEN);
    }

    touchLastX = x;
    touchLastY = y;
    touchHasLast = true;
    data->point.x = x;
    data->point.y = y;
  } else {
    touchHasLast = false;
  }
}

/* ── Public init ──────────────────────────────────── */
void display_init() {
  tft.init();
  tft.setRotation(0); /* portrait 240x320 */

  /* Gamma correction — reset to curve 1 for consistent color on clone panels */
  tft.writecommand(0x26); /* ILI9341_GAMMASET */
  tft.writedata(0x01);    /* Gamma curve 1 (default/sRGB) */

  tft.fillScreen(TFT_BLACK);

/* Backlight — use PWM for brightness control */
#ifdef TFT_BL
  ledcSetup(0, 5000, 8);     /* channel 0, 5kHz, 8-bit */
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, 255);          /* full brightness (0-255) */
#endif

  /* ── Init XPT2046 on separate VSPI bus (CYD board) ── */
  touchSPI.begin(TOUCH_SPI_CLK, TOUCH_SPI_MISO, TOUCH_SPI_MOSI, TOUCH_SPI_CS);
  ts.begin(touchSPI);
  ts.setRotation(0);
  Serial.println("[TOUCH] XPT2046 initialized on VSPI");

  /* LVGL init */
  lv_init();
  lv_disp_draw_buf_init(&drawBuf, buf1, NULL, SCREEN_W * DRAW_BUF_LINES);

  /* Display driver */
  static lv_disp_drv_t dispDrv;
  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = SCREEN_W;
  dispDrv.ver_res = SCREEN_H;
  dispDrv.flush_cb = _flush_cb;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_drv_register(&dispDrv);

  /* Input driver (touch) */
  static lv_indev_drv_t indevDrv;
  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = _touch_cb;
  lv_indev_drv_register(&indevDrv);

  Serial.println("[DISPLAY] TFT + LVGL initialized");
}
