#include "img_loader.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

/*
 * JPEG decoding: Using Bodmer's TJpg_Decoder (already in TFT_eSPI ecosystem).
 * If not available, we fall back to RGB565-only mode (backend pre-renders).
 * The ESP32 can decode a 240x240 JPEG in ~200ms — fine for NFT gallery.
 */
#if __has_include(<TJpg_Decoder.h>)
  #include <TJpg_Decoder.h>
  #define HAS_TJPGD 1
#else
  #define HAS_TJPGD 0
#endif

/* ── Shared decode state for TJpgDec callback ─────── */
#if HAS_TJPGD
static uint16_t* _jpgOutBuf = nullptr;
static uint16_t  _jpgOutW = 0;
static uint16_t  _jpgOutH = 0;

static bool _jpg_output_cb(int16_t x, int16_t y, uint16_t w, uint16_t h,
                           uint16_t* bitmap) {
    if (!_jpgOutBuf) return false;
    for (uint16_t row = 0; row < h; row++) {
        uint32_t srcOff = row * w;
        uint32_t dstOff = ((uint32_t)(y + row) * _jpgOutW) + x;
        if ((y + row) >= _jpgOutH) break;
        uint16_t copyW = (x + w > _jpgOutW) ? (_jpgOutW - x) : w;
        memcpy(&_jpgOutBuf[dstOff], &bitmap[srcOff], copyW * 2);
    }
    return true;
}
#endif

/* ── Internal: build lv_img_dsc_t from raw pixel data ── */
static void _fill_dsc(LoadedImage& img) {
    img.dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    img.dsc.header.always_zero = 0;
    img.dsc.header.reserved = 0;
    img.dsc.header.w = img.width;
    img.dsc.header.h = img.height;
    img.dsc.data_size = img.dataSize;
    img.dsc.data = img.data;
}

static LoadedImage _make_error(const char* msg) {
    LoadedImage img = {};
    img.ok = false;
    strncpy(img.error, msg, sizeof(img.error) - 1);
    return img;
}

/* ── Fetch raw RGB565 binary ──────────────────────── */
LoadedImage img_fetch_rgb565(const char* url, uint16_t width, uint16_t height) {
    size_t expected = (size_t)width * height * 2;
    if (expected > img_available_heap() * 0.7) {
        return _make_error("Image too large for heap");
    }

    WiFiClientSecure client;
    client.setInsecure(); /* Skip cert validation for speed on cheap hw */
    HTTPClient http;

    if (!http.begin(client, url)) {
        return _make_error("HTTP begin failed");
    }

    int code = http.GET();
    if (code != 200) {
        http.end();
        LoadedImage err = _make_error("HTTP error");
        snprintf(err.error, sizeof(err.error), "HTTP %d", code);
        return err;
    }

    int len = http.getSize();
    if (len > 0 && (size_t)len != expected) {
        http.end();
        return _make_error("Size mismatch");
    }

    uint8_t* buf = (uint8_t*)malloc(expected);
    if (!buf) {
        http.end();
        return _make_error("malloc failed");
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t read = 0;
    unsigned long start = millis();
    while (read < expected && (millis() - start) < 15000) {
        if (stream->available()) {
            size_t chunk = stream->readBytes(buf + read, expected - read);
            read += chunk;
        } else {
            delay(1);
        }
    }
    http.end();

    if (read != expected) {
        free(buf);
        return _make_error("Incomplete download");
    }

    LoadedImage img = {};
    img.data = buf;
    img.dataSize = expected;
    img.width = width;
    img.height = height;
    img.ok = true;
    _fill_dsc(img);

    Serial.printf("[IMG] RGB565 loaded: %ux%u (%u bytes, %lums)\n",
                  width, height, expected, millis() - start);
    return img;
}

/* ── Decode JPEG from memory ──────────────────────── */
LoadedImage img_decode_jpeg(const uint8_t* jpegData, size_t jpegLen,
                            uint16_t maxW, uint16_t maxH) {
#if HAS_TJPGD
    /* Get JPEG dimensions first */
    uint16_t jw = 0, jh = 0;
    TJpgDec.getFsJpgSize(&jw, &jh);  /* We'll use array method below */

    TJpgDec.setJpgScale(1);
    TJpgDec.setCallback(_jpg_output_cb);

    /* Allocate output buffer (max = maxW * maxH) */
    size_t outSize = (size_t)maxW * maxH * 2;
    if (outSize > img_available_heap() * 0.7) {
        return _make_error("JPEG too large for heap");
    }

    uint16_t* outBuf = (uint16_t*)malloc(outSize);
    if (!outBuf) return _make_error("malloc failed for JPEG");
    memset(outBuf, 0, outSize);

    _jpgOutBuf = outBuf;
    _jpgOutW = maxW;
    _jpgOutH = maxH;

    unsigned long start = millis();
    JRESULT res = TJpgDec.drawJpg(0, 0, jpegData, jpegLen);
    _jpgOutBuf = nullptr;

    if (res != JDR_OK) {
        free(outBuf);
        return _make_error("JPEG decode failed");
    }

    LoadedImage img = {};
    img.data = (uint8_t*)outBuf;
    img.dataSize = outSize;
    img.width = maxW;
    img.height = maxH;
    img.ok = true;
    _fill_dsc(img);

    Serial.printf("[IMG] JPEG decoded: %ux%u (%lums)\n",
                  maxW, maxH, millis() - start);
    return img;
#else
    return _make_error("No JPEG decoder (TJpgDec not available)");
#endif
}

/* ── Auto-detect fetch (RGB565 or JPEG by Content-Type) ── */
LoadedImage img_fetch(const char* url, uint16_t maxW, uint16_t maxH) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    if (!http.begin(client, url)) {
        return _make_error("HTTP begin failed");
    }

    /* Request with Accept header for content negotiation */
    http.addHeader("Accept", "application/octet-stream, image/jpeg");
    int code = http.GET();
    if (code != 200) {
        http.end();
        LoadedImage err = _make_error("HTTP error");
        snprintf(err.error, sizeof(err.error), "HTTP %d", code);
        return err;
    }

    String ct = http.header("Content-Type");
    int len = http.getSize();

    /* Check for dimension headers from Grudge backend */
    uint16_t imgW = maxW, imgH = maxH;
    String hdrW = http.header("X-Img-Width");
    String hdrH = http.header("X-Img-Height");
    if (hdrW.length() > 0) imgW = hdrW.toInt();
    if (hdrH.length() > 0) imgH = hdrH.toInt();

    if (imgW > maxW || imgH > maxH) {
        http.end();
        return _make_error("Image exceeds max dimensions");
    }

    /* Read entire body into temp buffer */
    if (len <= 0 || (size_t)len > img_available_heap() * 0.6) {
        http.end();
        return _make_error("Body too large or unknown size");
    }

    uint8_t* body = (uint8_t*)malloc(len);
    if (!body) {
        http.end();
        return _make_error("malloc failed");
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t read = 0;
    unsigned long start = millis();
    while (read < (size_t)len && (millis() - start) < 20000) {
        if (stream->available()) {
            size_t chunk = stream->readBytes(body + read, len - read);
            read += chunk;
        } else {
            delay(1);
        }
    }
    http.end();

    if (read != (size_t)len) {
        free(body);
        return _make_error("Incomplete download");
    }

    /* Route by content type */
    if (ct.indexOf("octet-stream") >= 0 || ct.indexOf("rgb565") >= 0) {
        /* Raw RGB565 — body IS the pixel data */
        size_t expected = (size_t)imgW * imgH * 2;
        if (read != expected) {
            free(body);
            return _make_error("RGB565 size mismatch");
        }
        LoadedImage img = {};
        img.data = body; /* Transfer ownership */
        img.dataSize = read;
        img.width = imgW;
        img.height = imgH;
        img.ok = true;
        _fill_dsc(img);
        Serial.printf("[IMG] RGB565 fetched: %ux%u (%lums)\n",
                      imgW, imgH, millis() - start);
        return img;
    }
    else if (ct.indexOf("jpeg") >= 0 || ct.indexOf("jpg") >= 0) {
        /* JPEG — decode on device */
        LoadedImage img = img_decode_jpeg(body, read, imgW, imgH);
        free(body); /* JPEG source no longer needed */
        return img;
    }
    else {
        free(body);
        return _make_error("Unsupported Content-Type");
    }
}

/* ── Free ─────────────────────────────────────────── */
void img_free(LoadedImage& img) {
    if (img.data) {
        free(img.data);
        img.data = nullptr;
    }
    img.ok = false;
}

/* ── Utility ──────────────────────────────────────── */
size_t img_available_heap() {
    return ESP.getFreeHeap();
}
