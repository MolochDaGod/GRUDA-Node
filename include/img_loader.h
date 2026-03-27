#ifndef GRUDA_IMG_LOADER_H
#define GRUDA_IMG_LOADER_H

/**
 * Runtime Image Loader — WiFi HTTP fetch + decode for ESP32
 *
 * Supports two formats from the Grudge backend:
 *   1. Raw RGB565 binary (pre-rendered server-side, fastest path)
 *   2. JPEG decode on-device using TJpgDec (for NFTs, dynamic content)
 *
 * Images are fetched over HTTPS, decoded into heap, and wrapped in
 * lv_img_dsc_t for direct LVGL display. Caller must free with img_free().
 *
 * Designed for cheap ESP32 hardware — minimal RAM, no PSRAM required.
 * Maximum practical image: 240x240 RGB565 = 115KB (fits in heap).
 *
 * Created by RacAlvin The Pirate King for GRUDGE STUDIO
 */

#include <Arduino.h>
#include <lvgl.h>

/* Image format returned by backend */
enum ImgFormat : uint8_t {
    IMG_FMT_RGB565  = 1, /* Raw RGB565 binary (2 bytes/pixel, byte-swapped) */
    IMG_FMT_JPEG    = 2, /* JPEG (decoded on-device via TJpgDec) */
    IMG_FMT_UNKNOWN = 0,
};

/* Loaded image result */
struct LoadedImage {
    lv_img_dsc_t  dsc;      /* LVGL image descriptor (ready to use) */
    uint8_t*      data;     /* Heap-allocated pixel buffer */
    size_t        dataSize; /* Size of data in bytes */
    uint16_t      width;
    uint16_t      height;
    bool          ok;       /* true if load succeeded */
    char          error[64]; /* error message if !ok */
};

/* ── Fetch + Decode ────────────────────────────────── */

/**
 * Fetch an image from a URL and decode it for LVGL display.
 * Supports raw RGB565 (Content-Type: application/octet-stream) and
 * JPEG (Content-Type: image/jpeg).
 *
 * @param url       Full HTTPS URL to image
 * @param maxW      Maximum width (image will be rejected if wider)
 * @param maxH      Maximum height (image will be rejected if taller)
 * @return          LoadedImage — check .ok before using .dsc
 */
LoadedImage img_fetch(const char* url, uint16_t maxW = 240, uint16_t maxH = 240);

/**
 * Fetch a pre-rendered RGB565 image from the Grudge object storage.
 * URL format: https://api.grudge-studio.com/objects/{key}.rgb565
 * Header X-Img-Width and X-Img-Height provide dimensions.
 */
LoadedImage img_fetch_rgb565(const char* url, uint16_t width, uint16_t height);

/**
 * Decode a JPEG buffer already in memory into RGB565 for LVGL.
 */
LoadedImage img_decode_jpeg(const uint8_t* jpegData, size_t jpegLen,
                            uint16_t maxW = 240, uint16_t maxH = 240);

/* ── Memory ────────────────────────────────────────── */

/**
 * Free a LoadedImage's pixel buffer. Safe to call on failed loads.
 */
void img_free(LoadedImage& img);

/* ── Utility ───────────────────────────────────────── */

/**
 * Get free heap available for image loading.
 */
size_t img_available_heap();

#endif /* GRUDA_IMG_LOADER_H */
