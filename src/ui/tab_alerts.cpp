#include <lvgl.h>
#include "alerts.h"
#include "theme.h"

static lv_obj_t* alertList = nullptr;

static const char* _alert_icon(AlertType t) {
    switch (t) {
        case ALERT_CREW_INVITE:   return LV_SYMBOL_PLUS;
        case ALERT_PVP_CHALLENGE: return LV_SYMBOL_CHARGE;
        case ALERT_FACTION_EVENT: return LV_SYMBOL_BELL;
        case ALERT_GOLD_TX:       return LV_SYMBOL_DOWNLOAD;
        case ALERT_GOULDSTONE:    return LV_SYMBOL_IMAGE;
        case ALERT_MISSION:       return LV_SYMBOL_RIGHT;
        case ALERT_CHAIN_VOTE:    return LV_SYMBOL_OK;
        default:                  return LV_SYMBOL_WARNING;
    }
}

void ui_tab_alerts_create(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 6, 0);
    lv_obj_set_style_pad_row(parent, 4, 0);

    lv_obj_t* t = lv_label_create(parent);
    lv_label_set_text(t, LV_SYMBOL_BELL " Alerts");
    lv_obj_set_style_text_color(t, lv_color_hex(WCS_ORANGE), 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);

    alertList = lv_list_create(parent);
    lv_obj_set_size(alertList, 224, 200);
    lv_obj_set_style_bg_color(alertList, lv_color_hex(WCS_BG_INPUT), 0);
    lv_obj_set_style_border_width(alertList, 1, 0);
    lv_obj_set_style_border_color(alertList, lv_color_hex(WCS_BG_SURFACE), 0);
    lv_obj_set_style_radius(alertList, 8, 0);
    lv_obj_set_style_pad_all(alertList, 4, 0);
}

void ui_tab_alerts_refresh(const AlertState& state) {
    if (!alertList) return;
    lv_obj_clean(alertList);
    for (int i = state.alertCount - 1; i >= 0 && i > (int)state.alertCount - 15; i--) {
        const Alert& a = state.alerts[i];
        lv_obj_t* btn = lv_list_add_btn(alertList, _alert_icon(a.type), a.title.c_str());
        lv_obj_set_style_text_color(btn, a.read ? lv_color_hex(WCS_TEXT_MUTED) : lv_color_hex(WCS_TEXT_PRIMARY), 0);
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_12, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(WCS_BG_SURFACE), 0);
        lv_obj_set_style_radius(btn, 6, 0);
    }
}
