#include <lvgl.h>
#include "config.h"
#include "treaty.h"
#include "theme.h"
#include "wallet.h"

/* ── External refs (set by main via ui_tab_treaty_set_context) ── */
static const GrudaWallet* _wallet = nullptr;
static TreatyState*       _state  = nullptr;

/* ── UI Elements ─────────────────────────────────────── */
static lv_obj_t* channelList = nullptr;  /* channel sidebar */
static lv_obj_t* msgScroll   = nullptr;  /* message bubble container */
static lv_obj_t* dotConn     = nullptr;  /* connection indicator */
static lv_obj_t* taCompose   = nullptr;  /* compose text area */
static lv_obj_t* keyboard    = nullptr;  /* on-screen keyboard */
static lv_obj_t* mainArea    = nullptr;  /* right side (messages + compose) */
static lv_obj_t* lblChanName = nullptr;  /* active channel name banner */
static lv_obj_t* chanBtns[GUILD_CHANNEL_COUNT] = {};  /* for highlighting active */

/* Short display names for sidebar */
static const char* CHAN_SHORT[] = {
    "#nod", "#gen", "#ann", "#trd", "#rul", "#sts", "\xE2\x99\xABlby", "\xE2\x99\xABwar"
};

/* ── Faction color helper ────────────────────────────── */
static uint32_t _faction_color(const String& faction) {
    if (faction == "Crusade") return WCS_ORANGE;
    if (faction == "Legion")  return WCS_RED;
    if (faction == "Fabled")  return WCS_BLUE;
    return WCS_TEXT_SECONDARY;
}

/* ── Update active channel highlight ─────────────────── */
static void _highlight_active_channel() {
    for (int i = 0; i < GUILD_CHANNEL_COUNT_ACTUAL && i < GUILD_CHANNEL_COUNT; i++) {
        if (!chanBtns[i]) continue;
        bool active = (_state && _state->activeChannel == String(GUILD_CHANNELS[i].id));
        lv_obj_set_style_border_side(chanBtns[i], active ? LV_BORDER_SIDE_LEFT : LV_BORDER_SIDE_NONE, 0);
        lv_obj_set_style_border_width(chanBtns[i], active ? 3 : 0, 0);
        lv_obj_set_style_border_color(chanBtns[i], lv_color_hex(WCS_ORANGE), 0);
        lv_obj_set_style_bg_color(chanBtns[i], lv_color_hex(active ? WCS_BG_INPUT : WCS_BG_SURFACE), 0);
    }
    /* Update channel name banner */
    if (lblChanName && _state) {
        for (int i = 0; i < GUILD_CHANNEL_COUNT_ACTUAL; i++) {
            if (_state->activeChannel == String(GUILD_CHANNELS[i].id)) {
                lv_label_set_text(lblChanName, GUILD_CHANNELS[i].name);
                break;
            }
        }
    }
}

/* ── Channel select callback ─────────────────────────── */
static void _channel_select_cb(lv_event_t* e) {
    if (!_state) return;
    int idx = (int)(uintptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < GUILD_CHANNEL_COUNT_ACTUAL) {
        _state->activeChannel = String(GUILD_CHANNELS[idx].id);
        _highlight_active_channel();
        Serial.printf("[TREATY] Switched to %s\n", GUILD_CHANNELS[idx].name);
    }
}

/* ── Quick-reply callback ────────────────────────────── */
static void _quick_reply_cb(lv_event_t* e) {
    if (!_wallet || !_state) return;
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    treaty_send_channel(_state->activeChannel, String(TREATY_QUICK_REPLIES[idx]), *_wallet);
}

/* ── Send button callback ────────────────────────────── */
static void _send_cb(lv_event_t* e) {
    (void)e;
    if (!taCompose || !_wallet || !_state) return;
    const char* txt = lv_textarea_get_text(taCompose);
    if (!txt || txt[0] == '\0') return;
    treaty_send_channel(_state->activeChannel, String(txt), *_wallet);
    lv_textarea_set_text(taCompose, "");
}

/* ── Keyboard show/hide on textarea focus ───────────── */
static void _ta_focus_cb(lv_event_t* e) {
    if (!keyboard) return;
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(keyboard, taCompose);
        lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        /* Shrink message area to make room for keyboard */
        if (msgScroll) lv_obj_set_height(msgScroll, 60);
    } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(keyboard, nullptr);
        /* Restore message area */
        if (msgScroll) lv_obj_set_height(msgScroll, 130);
    }
}

/* ── Create ──────────────────────────────────────────── */
void ui_tab_treaty_create(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(parent, 2, 0);
    lv_obj_set_style_pad_gap(parent, 2, 0);

    /* ── Left: Channel sidebar (56px wide) ─────────── */
    channelList = lv_obj_create(parent);
    lv_obj_set_size(channelList, 56, LV_PCT(100));
    lv_obj_set_flex_flow(channelList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(channelList, 2, 0);
    lv_obj_set_style_pad_row(channelList, 1, 0);
    lv_obj_set_style_bg_color(channelList, lv_color_hex(WCS_BG_CARD), 0);
    lv_obj_set_style_border_width(channelList, 0, 0);
    lv_obj_set_style_radius(channelList, 6, 0);
    lv_obj_set_scroll_dir(channelList, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(channelList, LV_SCROLLBAR_MODE_OFF);

    /* Connection dot at top of sidebar */
    dotConn = lv_obj_create(channelList);
    lv_obj_set_size(dotConn, 8, 8);
    lv_obj_set_style_radius(dotConn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dotConn, lv_color_hex(WCS_RED), 0);
    lv_obj_set_style_bg_opa(dotConn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dotConn, 0, 0);
    lv_obj_set_align(dotConn, LV_ALIGN_CENTER);

    /* Channel buttons with readable short names */
    for (int i = 0; i < GUILD_CHANNEL_COUNT_ACTUAL && i < GUILD_CHANNEL_COUNT; i++) {
        const GuildChannel& ch = GUILD_CHANNELS[i];

        lv_obj_t* btn = lv_btn_create(channelList);
        lv_obj_set_size(btn, 52, 22);
        lv_obj_set_style_bg_color(btn, lv_color_hex(WCS_BG_SURFACE), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(WCS_ORANGE_DARK), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_pad_all(btn, 1, 0);
        lv_obj_add_event_cb(btn, _channel_select_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        chanBtns[i] = btn;

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, CHAN_SHORT[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(
            ch.type == CHAN_VOICE ? WCS_GREEN :
            ch.readOnly ? WCS_TEXT_MUTED : WCS_TEXT_PRIMARY), 0);
        lv_obj_center(lbl);
    }

    /* Set initial active highlight */
    _highlight_active_channel();

    /* ── Right: Main area (messages + quick + compose) ─ */
    mainArea = lv_obj_create(parent);
    lv_obj_set_flex_grow(mainArea, 1);
    lv_obj_set_height(mainArea, LV_PCT(100));
    lv_obj_set_flex_flow(mainArea, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(mainArea, 2, 0);
    lv_obj_set_style_pad_row(mainArea, 2, 0);
    lv_obj_set_style_bg_opa(mainArea, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mainArea, 0, 0);

    /* Active channel name banner (scrolling marquee for long names) */
    lblChanName = lv_label_create(mainArea);
    lv_label_set_text(lblChanName, "# node-chat");
    lv_label_set_long_mode(lblChanName, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(lblChanName, LV_PCT(100));
    lv_obj_set_style_text_color(lblChanName, lv_color_hex(WCS_ORANGE), 0);
    lv_obj_set_style_text_font(lblChanName, &lv_font_montserrat_12, 0);
    lv_obj_set_style_anim_speed(lblChanName, 30, 0); /* px/sec scroll speed */

    /* Message scroll area */
    msgScroll = lv_obj_create(mainArea);
    lv_obj_set_size(msgScroll, LV_PCT(100), 116);
    lv_obj_set_flex_flow(msgScroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(msgScroll, 3, 0);
    lv_obj_set_style_pad_row(msgScroll, 2, 0);
    lv_obj_set_style_bg_color(msgScroll, lv_color_hex(WCS_BG_INPUT), 0);
    lv_obj_set_style_border_width(msgScroll, 1, 0);
    lv_obj_set_style_border_color(msgScroll, lv_color_hex(WCS_BG_SURFACE), 0);
    lv_obj_set_style_radius(msgScroll, 6, 0);
    lv_obj_set_scroll_dir(msgScroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(msgScroll, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_t* empty = lv_label_create(msgScroll);
    lv_label_set_text(empty, "# node-chat\nType or tap a quick reply");
    lv_obj_set_style_text_color(empty, lv_color_hex(WCS_TEXT_MUTED), 0);
    lv_obj_set_style_text_font(empty, &lv_font_montserrat_12, 0);
    lv_obj_set_width(empty, LV_PCT(100));

    /* Quick-reply bar */
    lv_obj_t* qrBar = lv_obj_create(mainArea);
    lv_obj_set_size(qrBar, LV_PCT(100), 24);
    lv_obj_set_flex_flow(qrBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(qrBar, 1, 0);
    lv_obj_set_style_pad_gap(qrBar, 3, 0);
    lv_obj_set_style_bg_opa(qrBar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(qrBar, 0, 0);
    lv_obj_set_scroll_dir(qrBar, LV_DIR_HOR);

    for (int i = 0; i < TREATY_QUICK_REPLIES_COUNT; i++) {
        lv_obj_t* btn = lv_btn_create(qrBar);
        lv_obj_set_height(btn, 20);
        lv_obj_set_style_min_width(btn, 40, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(WCS_BG_CARD), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(WCS_ORANGE_DARK), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(WCS_BG_SURFACE), 0);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_set_style_pad_hor(btn, 6, 0);
        lv_obj_set_style_pad_ver(btn, 1, 0);
        lv_obj_add_event_cb(btn, _quick_reply_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, TREATY_QUICK_REPLIES[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(WCS_BUTTON_TEXT), 0);
        lv_obj_center(lbl);
    }

    /* Compose bar */
    lv_obj_t* composeBar = lv_obj_create(mainArea);
    lv_obj_set_size(composeBar, LV_PCT(100), 30);
    lv_obj_set_flex_flow(composeBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(composeBar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(composeBar, lv_color_hex(WCS_BG_CARD), 0);
    lv_obj_set_style_border_width(composeBar, 0, 0);
    lv_obj_set_style_radius(composeBar, 6, 0);
    lv_obj_set_style_pad_all(composeBar, 2, 0);
    lv_obj_set_style_pad_gap(composeBar, 3, 0);

    taCompose = lv_textarea_create(composeBar);
    lv_obj_set_flex_grow(taCompose, 1);
    lv_obj_set_height(taCompose, 26);
    lv_textarea_set_one_line(taCompose, true);
    lv_textarea_set_placeholder_text(taCompose, "Message...");
    lv_obj_set_style_bg_color(taCompose, lv_color_hex(WCS_BG_INPUT), 0);
    lv_obj_set_style_border_width(taCompose, 1, 0);
    lv_obj_set_style_border_color(taCompose, lv_color_hex(WCS_BG_SURFACE), 0);
    lv_obj_set_style_border_color(taCompose, lv_color_hex(WCS_ORANGE), LV_STATE_FOCUSED);
    lv_obj_set_style_radius(taCompose, 4, 0);
    lv_obj_set_style_text_color(taCompose, lv_color_hex(WCS_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(taCompose, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(taCompose, _ta_focus_cb, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(taCompose, _ta_focus_cb, LV_EVENT_DEFOCUSED, nullptr);
    lv_obj_add_event_cb(taCompose, _ta_focus_cb, LV_EVENT_READY, nullptr);

    lv_obj_t* sendBtn = lv_btn_create(composeBar);
    lv_obj_set_size(sendBtn, 30, 26);
    lv_obj_set_style_bg_color(sendBtn, lv_color_hex(WCS_ORANGE), 0);
    lv_obj_set_style_bg_color(sendBtn, lv_color_hex(WCS_ORANGE_DARK), LV_STATE_PRESSED);
    lv_obj_set_style_radius(sendBtn, 4, 0);
    lv_obj_add_event_cb(sendBtn, _send_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* sendIcon = lv_label_create(sendBtn);
    lv_label_set_text(sendIcon, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(sendIcon, lv_color_hex(WCS_TEXT_PRIMARY), 0);
    lv_obj_center(sendIcon);

    /* ── On-screen keyboard (hidden until textarea focused) ── */
    keyboard = lv_keyboard_create(lv_scr_act());
    lv_obj_set_size(keyboard, SCREEN_W, 130);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(WCS_BG_CARD), 0);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(WCS_BG_SURFACE), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(WCS_ORANGE_DARK), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(keyboard, lv_color_hex(WCS_TEXT_PRIMARY), LV_PART_ITEMS);
    lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_border_width(keyboard, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(keyboard, lv_color_hex(WCS_BG_INPUT), LV_PART_ITEMS);
    lv_obj_set_style_radius(keyboard, 4, LV_PART_ITEMS);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

/* ── Set context (called from main before create) ──── */
void ui_tab_treaty_set_context(TreatyState* state, const GrudaWallet* wallet) {
    _state  = state;
    _wallet = wallet;
}

/* ── Refresh messages ────────────────────────────────── */
void ui_tab_treaty_refresh(const TreatyState& state) {
    /* Update connection dot */
    if (dotConn) {
        lv_obj_set_style_bg_color(dotConn,
            lv_color_hex(state.connected ? WCS_GREEN : WCS_RED), 0);
    }

    /* Rebuild message bubbles for active channel */
    if (!msgScroll) return;
    lv_obj_clean(msgScroll);

    if (state.messageCount == 0) {
        lv_obj_t* empty = lv_label_create(msgScroll);
        lv_label_set_text(empty, "No messages yet\nSend a treaty to begin");
        lv_obj_set_style_text_color(empty, lv_color_hex(WCS_TEXT_MUTED), 0);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(empty, LV_PCT(100));
        return;
    }

    /* Show last 10 messages for the active channel (oldest first) */
    String activeCh = state.activeChannel;
    int shown = 0;
    for (int i = state.messageCount - 1; i >= 0 && shown < 10; i--) {
        if (state.messages[i].channelId == activeCh ||
            state.messages[i].channelId.length() == 0) shown++;
    }
    /* Now render forward from the start of those shown messages */
    int skip = 0;
    for (int i = 0; i < state.messageCount; i++) {
        const TreatyMessage& m = state.messages[i];
        if (m.channelId != activeCh && m.channelId.length() > 0) continue;

        /* Bubble container */
        lv_obj_t* bubble = lv_obj_create(msgScroll);
        lv_obj_set_size(bubble, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(bubble, lv_color_hex(WCS_BG_CARD), 0);
        lv_obj_set_style_border_width(bubble, 0, 0);
        lv_obj_set_style_radius(bubble, 8, 0);
        lv_obj_set_style_pad_all(bubble, 4, 0);
        lv_obj_set_style_pad_row(bubble, 1, 0);
        lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);

        /* Sender name in faction color */
        lv_obj_t* name = lv_label_create(bubble);
        lv_label_set_text(name, m.senderName.c_str());
        lv_obj_set_style_text_color(name, lv_color_hex(_faction_color(m.faction)), 0);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);

        /* Message text */
        lv_obj_t* txt = lv_label_create(bubble);
        lv_label_set_text(txt, m.text.c_str());
        lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(txt, LV_PCT(100));
        lv_obj_set_style_text_color(txt,
            lv_color_hex(m.read ? WCS_TEXT_SECONDARY : WCS_TEXT_PRIMARY), 0);
        lv_obj_set_style_text_font(txt, &lv_font_montserrat_12, 0);
    }

    /* Auto-scroll to bottom */
    lv_obj_scroll_to_y(msgScroll, LV_COORD_MAX, LV_ANIM_OFF);
}
