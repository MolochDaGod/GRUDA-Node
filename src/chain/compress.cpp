#include "compress.h"
#include <string.h>

/*
 * Real deflate compression using ESP32 ROM miniz low-level API.
 * The high-level zlib wrappers are disabled in ROM, but tdefl/tinfl
 * compressor/decompressor work fine on raw deflate streams.
 * Compatible with Node.js zlib.deflateRawSync / inflateRawSync
 * and browser DecompressionStream('deflate-raw').
 */
#include <rom/miniz.h>

/* ── Compress (real deflate via tdefl) ───────────────── */
CompressResult gruda_compress(const uint8_t* input, size_t inputLen) {
    CompressResult r = { nullptr, 0, inputLen, 1.0f, COMPRESS_ALGO_DEFLATE, false };
    if (!input || inputLen == 0) return r;

    /* Worst-case output size + 1 byte algo prefix */
    size_t maxOut = inputLen + 128 + (inputLen >> 7);
    uint8_t* buf = (uint8_t*)malloc(maxOut + 1);
    if (!buf) {
        Serial.println("[COMPRESS] malloc failed");
        return r;
    }
    buf[0] = COMPRESS_ALGO_DEFLATE;

    /* Use tdefl — tdefl_compress_mem_to_mem returns output size or 0 on fail */
    size_t outLen = tdefl_compress_mem_to_mem(
        buf + 1, maxOut, input, inputLen,
        TDEFL_DEFAULT_MAX_PROBES /* ~level 6 */
    );

    if (outLen > 0) {
        r.data = buf;
        r.compressedSize = outLen + 1; /* +1 for algo prefix */
        r.ratio = (float)r.compressedSize / (float)inputLen;
        r.ok = true;
    } else {
        /* Compression failed — fallback to pass-through */
        Serial.println("[COMPRESS] tdefl failed, using raw");
        memcpy(buf + 1, input, inputLen);
        r.data = buf;
        r.compressedSize = inputLen + 1;
        r.ratio = 1.0f;
        r.ok = true;
    }
    return r;
}

/* ── Decompress (real inflate via tinfl) ─────────────── */
CompressResult gruda_decompress(const uint8_t* input, size_t inputLen, size_t expectedLen) {
    CompressResult r = { nullptr, 0, 0, 0, 0, false };
    if (inputLen < 2) return r;
    if (input[0] != COMPRESS_ALGO_DEFLATE) {
        Serial.printf("[COMPRESS] Unsupported algo: 0x%02x\n", input[0]);
        return r;
    }

    /* Use expectedLen hint or default 4x expansion */
    size_t outCap = expectedLen > 0 ? expectedLen : (inputLen * 4);
    uint8_t* buf = (uint8_t*)malloc(outCap + 1);
    if (!buf) return r;

    /* tinfl_decompress_mem_to_mem returns output size or TINFL_DECOMPRESS_MEM_TO_MEM_FAILED */
    size_t outLen = tinfl_decompress_mem_to_mem(
        buf, outCap, input + 1, inputLen - 1, 0
    );

    if (outLen != TINFL_DECOMPRESS_MEM_TO_MEM_FAILED && outLen > 0) {
        buf[outLen] = '\0';
        r.data = buf;
        r.compressedSize = inputLen;
        r.originalSize = outLen;
        r.algorithm = COMPRESS_ALGO_DEFLATE;
        r.ok = true;
    } else {
        Serial.printf("[COMPRESS] tinfl failed (outLen=%u)\n", (unsigned)outLen);
        free(buf);
    }
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
