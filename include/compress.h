#ifndef GRUDA_COMPRESS_H
#define GRUDA_COMPRESS_H

#include <Arduino.h>

/**
 * GRD-17 Compression — Node-to-Node Message Format
 *
 * Uses deflate (zlib/miniz) — the common denominator across:
 *   - ESP32 hardware nodes (miniz in ROM)
 *   - Computer legion nodes (Node.js zlib)
 *   - Master nodes (Node.js zlib)
 *   - Browser clients (DecompressionStream)
 *
 * Compatible with grd17CompressionService.ts 'deflate' algorithm.
 *
 * Wire format: [1-byte algo flag][compressed payload]
 *   algo: 0x01 = deflate (default for ESP32)
 *         0x02 = gzip
 *         0x03 = brotli (not supported on ESP32, decode-only via backend)
 */

#define COMPRESS_ALGO_DEFLATE  0x01
#define COMPRESS_ALGO_GZIP     0x02
#define COMPRESS_ALGO_BROTLI   0x03

struct CompressResult {
    uint8_t* data;
    size_t   compressedSize;
    size_t   originalSize;
    float    ratio;       /* 0.0-1.0, lower = better compression */
    uint8_t  algorithm;
    bool     ok;
};

/* Compress data using deflate (ESP32 miniz) */
CompressResult gruda_compress(const uint8_t* input, size_t inputLen);

/* Decompress deflate data */
CompressResult gruda_decompress(const uint8_t* input, size_t inputLen, size_t expectedLen);

/* Free a CompressResult's data buffer */
void gruda_compress_free(CompressResult& r);

/* Compress a JSON string for WebSocket transmission */
CompressResult gruda_compress_json(const String& json);

/* Decompress and return as String */
String gruda_decompress_to_string(const uint8_t* input, size_t inputLen, size_t expectedLen);

#endif /* GRUDA_COMPRESS_H */
