#include <lvgl.h>
#include "voting.h"

static lv_obj_t* lblTitle = nullptr;
static lv_obj_t* lblDesc  = nullptr;
static lv_obj_t* lblTally = nullptr;

void ui_tab_vote_create(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_row(parent, 4, 0);

    lv_obj_t* t = lv_label_create(parent);
    lv_label_set_text(t, "GRUDACHAIN Vote");
    lv_obj_set_style_text_color(t, lv_color_hex(0x00ff88), 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);

    lblTitle = lv_label_create(parent);
    lv_label_set_text(lblTitle, "No proposals");
    lv_label_set_long_mode(lblTitle, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lblTitle, 220);
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(0xffffff), 0);

    lblDesc = lv_label_create(parent);
    lv_label_set_text(lblDesc, "");
    lv_label_set_long_mode(lblDesc, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lblDesc, 220);
    lv_obj_set_style_text_color(lblDesc, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(lblDesc, &lv_font_montserrat_12, 0);

    lblTally = lv_label_create(parent);
    lv_label_set_text(lblTally, "");
    lv_obj_set_style_text_color(lblTally, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(lblTally, &lv_font_montserrat_12, 0);

    /* Vote buttons */
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, 224, 40);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);

    const char* labels[] = { "YES", "NO", "SKIP" };
    uint32_t colors[] = { 0x00aa44, 0xcc2222, 0x666666 };
    for (int i = 0; i < 3; i++) {
        lv_obj_t* btn = lv_btn_create(row);
        lv_obj_set_size(btn, 68, 32);
        lv_obj_set_style_bg_color(btn, lv_color_hex(colors[i]), 0);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_center(lbl);
    }
}

void ui_tab_vote_show(const Proposal& p) {
    if (lblTitle) lv_label_set_text(lblTitle, p.title.c_str());
    if (lblDesc)  lv_label_set_text(lblDesc, p.description.c_str());
    if (lblTally) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Y:%u  N:%u  A:%u", p.yesCount, p.noCount, p.abstainCount);
        lv_label_set_text(lblTally, buf);
    }
}
