#include "compress.h"
#include <string.h>

/**
 * NOTE: Real deflate compression deferred — ESP32 ROM miniz disables its
 * zlib API. Using pass-through (uncompressed) for now; the broadcast call
 * in main.cpp is still TODO. Replace with esp32/rom/miniz tdefl/tinfl
 * low-level API or bundle standalone miniz to enable real compression.
 */

/* ── Compress (pass-through stub) ───────────────────── */
CompressResult gruda_compress(const uint8_t* input, size_t inputLen) {
    CompressResult r = { nullptr, 0, inputLen, 1.0f, COMPRESS_ALGO_DEFLATE, false };
    /* +1 byte for algo flag prefix */
    uint8_t* buf = (uint8_t*)malloc(inputLen + 1);
    if (!buf) return r;
    buf[0] = COMPRESS_ALGO_DEFLATE;
    memcpy(buf + 1, input, inputLen);
    r.data           = buf;
    r.compressedSize = inputLen + 1;
    r.ratio          = 1.0f;
    r.ok             = true;
    return r;
}

/* ── Decompress (pass-through stub) ───────────────────── */
CompressResult gruda_decompress(const uint8_t* input, size_t inputLen, size_t expectedLen) {
    CompressResult r = { nullptr, 0, 0, 0, 0, false };
    if (inputLen < 2) return r;
    if (input[0] != COMPRESS_ALGO_DEFLATE) {
        Serial.printf("[COMPRESS] Unsupported algo: 0x%02x\n", input[0]);
        return r;
    }
    size_t dataLen = inputLen - 1;
    uint8_t* buf = (uint8_t*)malloc(dataLen + 1);
    if (!buf) return r;
    memcpy(buf, input + 1, dataLen);
    buf[dataLen] = '\0';
    r.data           = buf;
    r.compressedSize = inputLen;
    r.originalSize   = dataLen;
    r.algorithm      = COMPRESS_ALGO_DEFLATE;
    r.ok             = true;
    return r;
}

/* ── Free ─────────────────────────────────────────── */
void gruda_compress_free(CompressResult& r) {
    if (r.data) {
        free(r.data);
        r.data = nullptr;
    }
}

/* ── JSON helpers ─────────────────────────────────── */
CompressResult gruda_compress_json(const String& json) {
    return gruda_compress((const uint8_t*)json.c_str(), json.length());
}

String gruda_decompress_to_string(const uint8_t* input, size_t inputLen, size_t expectedLen) {
    CompressResult r = gruda_decompress(input, inputLen, expectedLen);
    if (!r.ok) return "";
    String s((const char*)r.data, r.originalSize);
    gruda_compress_free(r);
    return s;
}
