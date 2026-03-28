#include "theme.h"
#include "config.h"
#include "img_loader.h"
#include <lvgl.h>
#include <WiFi.h>

/**
 * NFT Gallery Tab — Fetches and displays NFT artwork on ESP32.
 *
 * Layout (240x ~260 usable):
 *   ┌─────────────────────────┐
 *   │  NFT Gallery       1/5  │  title + counter
 *   ├─────────────────────────┤
 *   │                         │
 *   │     [NFT IMAGE]         │  160x160 centered
 *   │     (fetched over WiFi) │
 *   │                         │
 *   ├─────────────────────────┤
 *   │  NFT Name               │
 *   │  Collection · Chain     │
 *   ├──────────┬──────────────┤
 *   │  < Prev  │    Next >    │  touch nav buttons
 *   └──────────┴──────────────┘
 *
 * Images are loaded on-demand via img_loader (HTTP → RGB565/JPEG).
 * Only one image is in memory at a time to save heap.
 */

#define NFT_IMG_SIZE 160
#define NFT_MAX_ITEMS 20

struct NFTItem {
    char name[48];
    char collection[32];
    char chain[16];
    char imageUrl[128];
};

static lv_obj_t* lblTitle   = nullptr;
static lv_obj_t* lblCounter = nullptr;
static lv_obj_t* imgNft     = nullptr;
static lv_obj_t* lblName    = nullptr;
static lv_obj_t* lblMeta    = nullptr;
static lv_obj_t* lblStatus  = nullptr;
static lv_obj_t* btnPrev    = nullptr;
static lv_obj_t* btnNext    = nullptr;

static NFTItem   _nfts[NFT_MAX_ITEMS];
static int       _nftCount = 0;
static int       _nftIndex = 0;
static LoadedImage _currentImg = {};
static bool      _loading = false;

/* ── Fetch real NFTs from Crossmint via Grudge backend ── */
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

static void _fetch_crossmint_nfts() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[NFT] WiFi not connected, using fallback");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    /* Fetch from Grudge backend which proxies to Crossmint */
    String nftUrl = String("https://") + API_HOST + "/nfts";
    http.begin(client, nftUrl);
    http.setTimeout(10000);
    int code = http.GET();

    if (code != 200) {
        Serial.printf("[NFT] Fetch failed: HTTP %d\n", code);
        http.end();
        return;
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        Serial.println("[NFT] JSON parse failed");
        return;
    }

    JsonArray arr = doc["nfts"].as<JsonArray>();
    _nftCount = 0;
    for (JsonObject nft : arr) {
        if (_nftCount >= NFT_MAX_ITEMS) break;
        const char* name = nft["name"] | "Untitled";
        const char* image = nft["image"] | "";
        const char* chain = nft["chain"] | "polygon";
        const char* desc = nft["description"] | "";

        strncpy(_nfts[_nftCount].name, name, sizeof(_nfts[0].name) - 1);
        strncpy(_nfts[_nftCount].collection, "Grudge Studio", sizeof(_nfts[0].collection) - 1);
        strncpy(_nfts[_nftCount].chain, chain, sizeof(_nfts[0].chain) - 1);
        strncpy(_nfts[_nftCount].imageUrl, image, sizeof(_nfts[0].imageUrl) - 1);
        _nftCount++;
    }

    Serial.printf("[NFT] Loaded %d NFTs from Crossmint\n", _nftCount);
}

/* ── Update display for current NFT ──────────────────── */
static void _update_info() {
    if (_nftCount == 0) {
        lv_label_set_text(lblName, "No NFTs loaded");
        lv_label_set_text(lblMeta, "");
        lv_label_set_text(lblCounter, "0/0");
        return;
    }

    NFTItem& nft = _nfts[_nftIndex];
    lv_label_set_text(lblName, nft.name);
    lv_label_set_text_fmt(lblMeta, "%s  ·  %s", nft.collection, nft.chain);
    lv_label_set_text_fmt(lblCounter, "%d/%d", _nftIndex + 1, _nftCount);
}

/* ── Load NFT image (async-friendly, blocking fetch) ──── */
static void _load_current_image() {
    if (_nftCount == 0 || _loading) return;
    _loading = true;

    /* Free previous image */
    if (_currentImg.ok) {
        img_free(_currentImg);
        lv_img_set_src(imgNft, NULL);
    }

    lv_label_set_text(lblStatus, "Loading...");
    lv_timer_handler(); /* Force render so user sees "Loading..." */

    NFTItem& nft = _nfts[_nftIndex];

    /* Check heap before fetching */
    size_t heap = img_available_heap();
    if (heap < 60000) {
        lv_label_set_text(lblStatus, "Low memory");
        Serial.printf("[NFT] Low heap: %u — skipping fetch\n", heap);
        _loading = false;
        return;
    }

    _currentImg = img_fetch(nft.imageUrl, NFT_IMG_SIZE, NFT_IMG_SIZE);

    if (_currentImg.ok) {
        lv_img_set_src(imgNft, &_currentImg.dsc);
        lv_label_set_text(lblStatus, "");
        Serial.printf("[NFT] Loaded: %s (%ux%u, %u bytes, heap=%u)\n",
                      nft.name, _currentImg.width, _currentImg.height,
                      _currentImg.dataSize, ESP.getFreeHeap());
    } else {
        /* Show placeholder text instead of broken image */
        lv_img_set_src(imgNft, NULL);
        lv_label_set_text(lblStatus, _currentImg.error);
        Serial.printf("[NFT] Failed: %s — %s\n", nft.name, _currentImg.error);
    }

    _loading = false;
}

/* ── Navigation callbacks ─────────────────────────────── */
static void _prev_cb(lv_event_t*) {
    if (_nftCount == 0) return;
    _nftIndex = (_nftIndex - 1 + _nftCount) % _nftCount;
    _update_info();
    _load_current_image();
}

static void _next_cb(lv_event_t*) {
    if (_nftCount == 0) return;
    _nftIndex = (_nftIndex + 1) % _nftCount;
    _update_info();
    _load_current_image();
}

/* ── Public: Create NFT Tab ───────────────────────────── */
void ui_tab_nft_create(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_gap(parent, 4, 0);

    /* Header row: title + counter */
    lv_obj_t* headerRow = lv_obj_create(parent);
    lv_obj_set_size(headerRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(headerRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(headerRow, 0, 0);
    lv_obj_set_style_pad_all(headerRow, 0, 0);
    lv_obj_set_flex_flow(headerRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(headerRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lblTitle = lv_label_create(headerRow);
    lv_label_set_text(lblTitle, "NFT Gallery");
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(WCS_PURPLE), 0);
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_14, 0);

    lblCounter = lv_label_create(headerRow);
    lv_label_set_text(lblCounter, "0/0");
    lv_obj_set_style_text_color(lblCounter, lv_color_hex(WCS_TEXT_MUTED), 0);
    lv_obj_set_style_text_font(lblCounter, &lv_font_montserrat_12, 0);

    /* Image container — dark bg with border */
    lv_obj_t* imgBox = lv_obj_create(parent);
    lv_obj_set_size(imgBox, NFT_IMG_SIZE + 16, NFT_IMG_SIZE + 16);
    lv_obj_set_style_bg_color(imgBox, lv_color_hex(WCS_BG_CARD), 0);
    lv_obj_set_style_border_width(imgBox, 1, 0);
    lv_obj_set_style_border_color(imgBox, lv_color_hex(WCS_PURPLE), 0);
    lv_obj_set_style_radius(imgBox, 12, 0);
    lv_obj_set_style_pad_all(imgBox, 8, 0);
    lv_obj_align(imgBox, LV_ALIGN_TOP_MID, 0, 0);

    imgNft = lv_img_create(imgBox);
    lv_obj_center(imgNft);

    /* Status label (loading / error) */
    lblStatus = lv_label_create(imgBox);
    lv_label_set_text(lblStatus, "");
    lv_obj_set_style_text_color(lblStatus, lv_color_hex(WCS_TEXT_MUTED), 0);
    lv_obj_set_style_text_font(lblStatus, &lv_font_montserrat_12, 0);
    lv_obj_center(lblStatus);

    /* NFT info */
    lblName = lv_label_create(parent);
    lv_label_set_text(lblName, "—");
    lv_obj_set_style_text_color(lblName, lv_color_hex(WCS_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(lblName, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(lblName, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lblName, 220);

    lblMeta = lv_label_create(parent);
    lv_label_set_text(lblMeta, "");
    lv_obj_set_style_text_color(lblMeta, lv_color_hex(WCS_TEXT_MUTED), 0);
    lv_obj_set_style_text_font(lblMeta, &lv_font_montserrat_12, 0);

    /* Navigation buttons */
    lv_obj_t* navRow = lv_obj_create(parent);
    lv_obj_set_size(navRow, LV_PCT(100), 36);
    lv_obj_set_style_bg_opa(navRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(navRow, 0, 0);
    lv_obj_set_style_pad_all(navRow, 0, 0);
    lv_obj_set_flex_flow(navRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(navRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    btnPrev = lv_btn_create(navRow);
    lv_obj_set_size(btnPrev, 100, 32);
    lv_obj_set_style_bg_color(btnPrev, lv_color_hex(WCS_BUTTON_BG), 0);
    lv_obj_set_style_bg_color(btnPrev, lv_color_hex(WCS_PURPLE), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btnPrev, 8, 0);
    lv_obj_add_event_cb(btnPrev, _prev_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* pLbl = lv_label_create(btnPrev);
    lv_label_set_text(pLbl, LV_SYMBOL_LEFT " Prev");
    lv_obj_center(pLbl);

    btnNext = lv_btn_create(navRow);
    lv_obj_set_size(btnNext, 100, 32);
    lv_obj_set_style_bg_color(btnNext, lv_color_hex(WCS_BUTTON_BG), 0);
    lv_obj_set_style_bg_color(btnNext, lv_color_hex(WCS_PURPLE), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btnNext, 8, 0);
    lv_obj_add_event_cb(btnNext, _next_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* nLbl = lv_label_create(btnNext);
    lv_label_set_text(nLbl, "Next " LV_SYMBOL_RIGHT);
    lv_obj_center(nLbl);

    /* Fetch real NFTs from Crossmint via backend */
    _fetch_crossmint_nfts();
    _update_info();
    if (_nftCount == 0) {
        lv_label_set_text(lblStatus, "No NFTs — check connection");
    } else {
        lv_label_set_text(lblStatus, "Tap Next to view");
    }
}
