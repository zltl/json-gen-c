/**
 * @file cbor_codec.h
 * @brief CBOR (RFC 8949) wire format constants and reader/writer declarations.
 *
 * This file is NOT compiled directly; it is converted to a C byte array
 * via xxd and embedded into the generated cbor.gen.c output.
 */

#ifndef CBOR_CODEC_H
#define CBOR_CODEC_H

#include <stdint.h>
#include <stddef.h>

/* ── CBOR major types (high 3 bits of initial byte) ─────────────────── */

#define CB_MAJOR_UINT    0  /* Major type 0: unsigned integer */
#define CB_MAJOR_NINT    1  /* Major type 1: negative integer */
#define CB_MAJOR_BSTR    2  /* Major type 2: byte string */
#define CB_MAJOR_TSTR    3  /* Major type 3: text string (UTF-8) */
#define CB_MAJOR_ARRAY   4  /* Major type 4: array */
#define CB_MAJOR_MAP     5  /* Major type 5: map */
#define CB_MAJOR_TAG     6  /* Major type 6: semantic tag */
#define CB_MAJOR_SIMPLE  7  /* Major type 7: simple/float */

/* Initial byte = (major << 5) | additional_info */
#define CB_INITIAL(major, ai)  (((major) << 5) | (ai))

/* ── Additional information values ──────────────────────────────────── */

#define CB_AI_1BYTE      24  /* Next 1 byte is the value/length */
#define CB_AI_2BYTE      25  /* Next 2 bytes */
#define CB_AI_4BYTE      26  /* Next 4 bytes */
#define CB_AI_8BYTE      27  /* Next 8 bytes */

/* ── Simple values (major type 7) ───────────────────────────────────── */

#define CB_FALSE         CB_INITIAL(7, 20)  /* 0xf4 */
#define CB_TRUE          CB_INITIAL(7, 21)  /* 0xf5 */
#define CB_NULL          CB_INITIAL(7, 22)  /* 0xf6 */
#define CB_UNDEFINED     CB_INITIAL(7, 23)  /* 0xf7 */

/* ── Float markers (major type 7) ───────────────────────────────────── */

#define CB_FLOAT16       CB_INITIAL(7, 25)  /* 0xf9 — half-precision */
#define CB_FLOAT32       CB_INITIAL(7, 26)  /* 0xfa */
#define CB_FLOAT64       CB_INITIAL(7, 27)  /* 0xfb */

/* ── Reader context ─────────────────────────────────────────────────── */

struct cb_reader {
    const unsigned char *data;
    size_t len;
    size_t pos;
};

#endif /* CBOR_CODEC_H */
