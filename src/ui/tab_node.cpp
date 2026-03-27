#include <lvgl.h>
#include "grd17.h"
#include "theme.h"

static lv_obj_t* lblBlock   = nullptr;
static lv_obj_t* lblPeers   = nullptr;
static lv_obj_t* lblStake   = nullptr;
static lv_obj_t* lblStatus  = nullptr;

void ui_tab_node_create(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_row(parent, 6, 0);

    /* Title */
    lv_obj_t* t = lv_label_create(parent);
    lv_label_set_text(t, "GRD-17 Node");
    lv_obj_set_style_text_color(t, lv_color_hex(WCS_ORANGE), 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);

    /* Status card */
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_hex(WCS_BG_CARD), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 4, 0);

    lblStatus = lv_label_create(card);
    lv_label_set_text(lblStatus, "Syncing...");
    lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_YELLOW), 0);
    lv_obj_set_style_text_font(lblStatus, &lv_font_montserrat_14, 0);

    lblBlock = lv_label_create(card);
    lv_label_set_text(lblBlock, "Block: 0");
    lv_obj_set_style_text_color(lblBlock, lv_color_hex(WCS_TEXT_PRIMARY), 0);

    lblPeers = lv_label_create(card);
    lv_label_set_text(lblPeers, "Peers: 0");
    lv_obj_set_style_text_color(lblPeers, lv_color_hex(WCS_TEXT_SECONDARY), 0);

    lblStake = lv_label_create(card);
    lv_label_set_text(lblStake, "Stake: 0");
    lv_obj_set_style_text_color(lblStake, lv_color_hex(WCS_TEXT_SECONDARY), 0);

    /* Network ID */
    lv_obj_t* net = lv_label_create(card);
    lv_label_set_text(net, GRUDACHAIN_NETWORK_ID);
    lv_obj_set_style_text_color(net, lv_color_hex(WCS_TEXT_MUTED), 0);
    lv_obj_set_style_text_font(net, &lv_font_montserrat_12, 0);
}

void ui_tab_node_update(const GRD17NodeState& s) {
    if (lblBlock)  lv_label_set_text_fmt(lblBlock, "Block: %u", s.latestBlockHeight);
    if (lblPeers)  lv_label_set_text_fmt(lblPeers, "Peers: %u", s.peerCount);
    if (lblStake)  lv_label_set_text_fmt(lblStake, "Stake: %u GRUDA", s.totalStake);
    if (lblStatus) {
        if (s.running && !s.catchingUp) {
            lv_label_set_text(lblStatus, LV_SYMBOL_OK " Active");
            lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_GREEN), 0);
        } else if (s.running) {
            lv_label_set_text(lblStatus, LV_SYMBOL_REFRESH " Syncing...");
            lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_YELLOW), 0);
        } else {
            lv_label_set_text(lblStatus, LV_SYMBOL_CLOSE " Offline");
            lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_RED), 0);
        }
    }
}
