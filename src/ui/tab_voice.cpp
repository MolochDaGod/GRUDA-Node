#include "ai_admin.h"
#include "config.h"
#include "theme.h"
#include "voice_bt.h"
#include <lvgl.h>

/**
 * Voice Tab — shows BLE connection status, AI transcript log,
 * and quick-action buttons for common voice commands.
 */

static lv_obj_t* lblBleStatus = nullptr;
static lv_obj_t* lblLastCmd   = nullptr;
static lv_obj_t* logPanel     = nullptr;
static lv_obj_t* lblProcessing = nullptr;

/* External references set by main.cpp */
static VoiceBTState*  _vbtState = nullptr;
static AIAdminState*  _aiState  = nullptr;

void ui_tab_voice_set_context(VoiceBTState* vbt, AIAdminState* ai) {
    _vbtState = vbt;
    _aiState  = ai;
}

/* ── Quick command buttons callback ──────────────── */
static void _quick_cmd_cb(lv_event_t* e) {
    const char* cmd = (const char*)lv_event_get_user_data(e);
    if (!cmd) return;

    /* Simulate a voice command through the same pipeline */
    extern void _on_voice_text(const String& text);
    _on_voice_text(String(cmd));
}

/* ── Create the Voice tab content ────────────────── */
void ui_tab_voice_create(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(WCS_BG_DARK), 0);
    lv_obj_set_style_pad_all(parent, 6, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    /* BLE Status line */
    lblBleStatus = lv_label_create(parent);
    lv_label_set_text(lblBleStatus, LV_SYMBOL_BLUETOOTH " BLE: Waiting...");
    lv_obj_set_style_text_color(lblBleStatus, lv_color_hex(WCS_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(lblBleStatus, &lv_font_montserrat_12, 0);

    /* Processing indicator */
    lblProcessing = lv_label_create(parent);
    lv_label_set_text(lblProcessing, "");
    lv_obj_set_style_text_color(lblProcessing, lv_color_hex(WCS_YELLOW), 0);
    lv_obj_set_style_text_font(lblProcessing, &lv_font_montserrat_12, 0);

    /* Last command */
    lblLastCmd = lv_label_create(parent);
    lv_label_set_text(lblLastCmd, "Say something...");
    lv_obj_set_style_text_color(lblLastCmd, lv_color_hex(WCS_ORANGE), 0);
    lv_obj_set_style_text_font(lblLastCmd, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lblLastCmd, LV_PCT(100));
    lv_label_set_long_mode(lblLastCmd, LV_LABEL_LONG_WRAP);

    /* Transcript log — scrollable container */
    logPanel = lv_obj_create(parent);
    lv_obj_set_size(logPanel, LV_PCT(100), 110);
    lv_obj_set_style_bg_color(logPanel, lv_color_hex(WCS_BG_CARD), 0);
    lv_obj_set_style_border_width(logPanel, 1, 0);
    lv_obj_set_style_border_color(logPanel, lv_color_hex(WCS_ORANGE_DARK), 0);
    lv_obj_set_style_radius(logPanel, 8, 0);
    lv_obj_set_style_pad_all(logPanel, 4, 0);
    lv_obj_set_flex_flow(logPanel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(logPanel, LV_DIR_VER);
    lv_obj_set_style_shadow_width(logPanel, 0, 0);

    /* Quick command buttons */
    lv_obj_t* btnRow = lv_obj_create(parent);
    lv_obj_set_size(btnRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    lv_obj_set_style_pad_gap(btnRow, 4, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_shadow_width(btnRow, 0, 0);

    static const char* quickCmds[] = {"Status", "Unread", "Help", "Ping"};
    static const char* quickVals[] = {"status", "unread", "help", "ping"};

    for (int i = 0; i < 4; i++) {
        lv_obj_t* btn = lv_btn_create(btnRow);
        lv_obj_set_size(btn, LV_SIZE_CONTENT, 28);
        lv_obj_set_style_bg_color(btn, lv_color_hex(WCS_BUTTON_BG), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(WCS_BUTTON_ACTIVE),
                                  LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_pad_hor(btn, 10, 0);
        lv_obj_set_style_pad_ver(btn, 4, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_add_event_cb(btn, _quick_cmd_cb, LV_EVENT_CLICKED,
                            (void*)quickVals[i]);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, quickCmds[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(WCS_BUTTON_TEXT), 0);
        lv_obj_center(lbl);
    }
}

/* ── Refresh the voice tab from current state ─────── */
void ui_tab_voice_refresh(const VoiceBTState& vbt, const AIAdminState& ai) {
    /* BLE status */
    if (lblBleStatus) {
        if (vbt.clientConnected) {
            lv_label_set_text_fmt(lblBleStatus,
                LV_SYMBOL_BLUETOOTH " Connected (%u msgs)", vbt.messageCount);
            lv_obj_set_style_text_color(lblBleStatus,
                lv_color_hex(WCS_GREEN), 0);
        } else if (vbt.initialized) {
            lv_label_set_text(lblBleStatus,
                LV_SYMBOL_BLUETOOTH " Advertising...");
            lv_obj_set_style_text_color(lblBleStatus,
                lv_color_hex(WCS_BLUE), 0);
        } else {
            lv_label_set_text(lblBleStatus,
                LV_SYMBOL_BLUETOOTH " BLE Off");
            lv_obj_set_style_text_color(lblBleStatus,
                lv_color_hex(WCS_TEXT_MUTED), 0);
        }
    }

    /* Processing indicator */
    if (lblProcessing) {
        lv_label_set_text(lblProcessing,
            ai.processing ? "Processing..." : "");
    }

    /* Last command display */
    if (lblLastCmd && vbt.lastCommand.length() > 0) {
        String display = "> " + vbt.lastCommand;
        if (display.length() > 60) display = display.substring(0, 57) + "...";
        lv_label_set_text(lblLastCmd, display.c_str());
    }

    /* Rebuild transcript log */
    if (logPanel) {
        lv_obj_clean(logPanel);

        uint8_t count;
        const TranscriptEntry* entries = ai_admin_get_transcript(ai, count);

        /* Show latest entries (bottom = newest) */
        uint8_t start = (count > 6) ? count - 6 : 0;
        for (uint8_t i = start; i < count; i++) {
            /* User text */
            lv_obj_t* cmdLbl = lv_label_create(logPanel);
            String cmdTxt = "> " + entries[i].text;
            if (cmdTxt.length() > 40) cmdTxt = cmdTxt.substring(0, 37) + "...";
            lv_label_set_text(cmdLbl, cmdTxt.c_str());
            lv_obj_set_style_text_color(cmdLbl, lv_color_hex(WCS_ORANGE_LIGHT), 0);
            lv_obj_set_style_text_font(cmdLbl, &lv_font_montserrat_12, 0);
            lv_obj_set_width(cmdLbl, LV_PCT(100));

            /* AI response */
            lv_obj_t* respLbl = lv_label_create(logPanel);
            String respTxt = entries[i].response;
            if (respTxt.length() > 80) respTxt = respTxt.substring(0, 77) + "...";
            lv_label_set_text(respLbl, respTxt.c_str());
            lv_obj_set_style_text_color(respLbl,
                lv_color_hex(entries[i].isLocal ? WCS_GREEN : WCS_BLUE), 0);
            lv_obj_set_style_text_font(respLbl, &lv_font_montserrat_12, 0);
            lv_obj_set_width(respLbl, LV_PCT(100));
            lv_label_set_long_mode(respLbl, LV_LABEL_LONG_WRAP);
        }

        /* Auto-scroll to bottom */
        lv_obj_scroll_to_y(logPanel, LV_COORD_MAX, LV_ANIM_ON);
    }
}
