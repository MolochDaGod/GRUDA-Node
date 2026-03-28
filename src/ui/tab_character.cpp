#include "theme.h"
#include "config.h"
#include "img_loader.h"
#include <lvgl.h>
#include <WiFi.h>

/**
 * Character Profile Tab — Grudge Warlord Race Card
 *
 * Fetches the player's race card image from the backend and
 * displays it full-width on the 240x320 device screen.
 * Shows race name, class, level, and equipment slot indicators.
 *
 * Races: Undead, Barbarian, Dwarf, Elf, Human, Orc
 */

#define CHAR_IMG_W 200
#define CHAR_IMG_H 200

static lv_obj_t* lblRace     = nullptr;
static lv_obj_t* lblClass    = nullptr;
static lv_obj_t* lblLevel    = nullptr;
static lv_obj_t* imgChar     = nullptr;
static lv_obj_t* lblStatus   = nullptr;

static LoadedImage _charImg  = {};
static char _currentRace[16] = "human";
static char _currentClass[16] = "Warrior";
static int  _currentLevel    = 1;

/* ── Fetch character card from backend ────────────── */
static void _load_race_card() {
    if (WiFi.status() != WL_CONNECTED) {
        if (lblStatus) lv_label_set_text(lblStatus, "No WiFi");
        return;
    }

    /* Free previous image */
    if (_charImg.ok) {
        img_free(_charImg);
        if (imgChar) lv_img_set_src(imgChar, NULL);
    }

    if (lblStatus) {
        lv_label_set_text(lblStatus, "Loading...");
        lv_timer_handler();
    }

    /* Fetch from Grudge backend */
    String url = String("https://") + API_HOST + "/character/" + _currentRace + "/image";
    _charImg = img_fetch(url.c_str(), CHAR_IMG_W, CHAR_IMG_H);

    if (_charImg.ok && imgChar) {
        lv_img_set_src(imgChar, &_charImg.dsc);
        if (lblStatus) lv_label_set_text(lblStatus, "");
        Serial.printf("[CHAR] Loaded race card: %s (%ux%u)\n",
                      _currentRace, _charImg.width, _charImg.height);
    } else {
        if (lblStatus) lv_label_set_text(lblStatus, _charImg.error);
        Serial.printf("[CHAR] Failed: %s\n", _charImg.error);
    }
}

/* ── Cycle race on tap ────────────────────────────── */
static const char* RACES[] = { "undead", "barbarian", "dwarf", "elf", "human", "orc" };
static int _raceIdx = 4; /* default: human */

static void _next_race_cb(lv_event_t*) {
    _raceIdx = (_raceIdx + 1) % 6;
    strncpy(_currentRace, RACES[_raceIdx], sizeof(_currentRace) - 1);
    if (lblRace) {
        char buf[16];
        strncpy(buf, _currentRace, sizeof(buf));
        buf[0] = buf[0] - 32; /* capitalize */
        lv_label_set_text(lblRace, buf);
    }
    _load_race_card();
}

/* ── Public: Create Character Tab ─────────────────── */
void ui_tab_character_create(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 6, 0);
    lv_obj_set_style_pad_gap(parent, 4, 0);

    /* Header: GRUDGE WARLORD */
    lv_obj_t* header = lv_label_create(parent);
    lv_label_set_text(header, "GRUDGE WARLORD");
    lv_obj_set_style_text_color(header, lv_color_hex(WCS_ORANGE), 0);
    lv_obj_set_style_text_font(header, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_letter_space(header, 2, 0);

    /* Race name */
    lblRace = lv_label_create(parent);
    lv_label_set_text(lblRace, "Human");
    lv_obj_set_style_text_color(lblRace, lv_color_hex(WCS_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(lblRace, &lv_font_montserrat_16, 0);

    /* Character image container */
    lv_obj_t* imgBox = lv_obj_create(parent);
    lv_obj_set_size(imgBox, CHAR_IMG_W + 8, CHAR_IMG_H + 8);
    lv_obj_set_style_bg_color(imgBox, lv_color_hex(WCS_BG_CARD), 0);
    lv_obj_set_style_border_width(imgBox, 1, 0);
    lv_obj_set_style_border_color(imgBox, lv_color_hex(WCS_ORANGE_DARK), 0);
    lv_obj_set_style_radius(imgBox, 8, 0);
    lv_obj_set_style_pad_all(imgBox, 4, 0);
    lv_obj_set_style_shadow_width(imgBox, 0, 0);

    imgChar = lv_img_create(imgBox);
    lv_obj_center(imgChar);

    /* Status / loading text */
    lblStatus = lv_label_create(imgBox);
    lv_label_set_text(lblStatus, "Tap to browse races");
    lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_TEXT_MUTED), 0);
    lv_obj_set_style_text_font(lblStatus, &lv_font_montserrat_12, 0);
    lv_obj_center(lblStatus);

    /* Tap image box to cycle races */
    lv_obj_add_flag(imgBox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(imgBox, _next_race_cb, LV_EVENT_CLICKED, nullptr);

    /* Class + Level row */
    lv_obj_t* infoRow = lv_obj_create(parent);
    lv_obj_set_size(infoRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(infoRow, lv_color_hex(WCS_BG_CARD), 0);
    lv_obj_set_style_border_width(infoRow, 0, 0);
    lv_obj_set_style_radius(infoRow, 8, 0);
    lv_obj_set_style_pad_all(infoRow, 8, 0);
    lv_obj_set_flex_flow(infoRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(infoRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lblClass = lv_label_create(infoRow);
    lv_label_set_text(lblClass, "Warrior");
    lv_obj_set_style_text_color(lblClass, lv_color_hex(WCS_GOLD_LIGHT), 0);
    lv_obj_set_style_text_font(lblClass, &lv_font_montserrat_14, 0);

    lblLevel = lv_label_create(infoRow);
    lv_label_set_text(lblLevel, "Lv 1");
    lv_obj_set_style_text_color(lblLevel, lv_color_hex(WCS_GREEN), 0);
    lv_obj_set_style_text_font(lblLevel, &lv_font_montserrat_14, 0);
}

/* ── Update from backend data ─────────────────────── */
void ui_tab_character_update(const char* race, const char* cls, int level) {
    if (race && strlen(race) > 0) {
        strncpy(_currentRace, race, sizeof(_currentRace) - 1);
        if (lblRace) {
            char buf[16];
            strncpy(buf, race, sizeof(buf));
            if (buf[0] >= 'a') buf[0] -= 32;
            lv_label_set_text(lblRace, buf);
        }
    }
    if (cls && lblClass) lv_label_set_text(lblClass, cls);
    if (lblLevel) lv_label_set_text_fmt(lblLevel, "Lv %d", level);
}
