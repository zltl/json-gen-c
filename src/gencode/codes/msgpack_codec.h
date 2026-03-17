/**
 * @file msgpack_codec.h
 * @brief MessagePack binary codec runtime — embedded into generated code.
 *
 * This header is NOT compiled directly; it is embedded via xxd into the
 * generated msgpack.gen.c output, providing the runtime pack/unpack engine.
 */

#ifndef MSGPACK_CODEC_H_
#define MSGPACK_CODEC_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ── MessagePack format constants ───────────────────────────────────── */

/* Positive fixint: 0x00 – 0x7f */
#define MP_FIXINT_MAX    0x7f

/* Negative fixint: 0xe0 – 0xff  (values -32 .. -1) */
#define MP_NEG_FIXINT    0xe0

/* Nil, booleans */
#define MP_NIL           0xc0
#define MP_FALSE         0xc2
#define MP_TRUE          0xc3

/* Floats */
#define MP_FLOAT32       0xca
#define MP_FLOAT64       0xcb

/* Unsigned integers */
#define MP_UINT8         0xcc
#define MP_UINT16        0xcd
#define MP_UINT32        0xce
#define MP_UINT64        0xcf

/* Signed integers */
#define MP_INT8          0xd0
#define MP_INT16         0xd1
#define MP_INT32         0xd2
#define MP_INT64         0xd3

/* Strings */
#define MP_FIXSTR        0xa0  /* 101xxxxx, len in low 5 bits */
#define MP_FIXSTR_MASK   0x1f
#define MP_STR8          0xd9
#define MP_STR16         0xda
#define MP_STR32         0xdb

/* Arrays */
#define MP_FIXARRAY      0x90  /* 1001xxxx, len in low 4 bits */
#define MP_FIXARRAY_MASK 0x0f
#define MP_ARRAY16       0xdc
#define MP_ARRAY32       0xdd

/* Maps */
#define MP_FIXMAP        0x80  /* 1000xxxx, len in low 4 bits */
#define MP_FIXMAP_MASK   0x0f
#define MP_MAP16         0xde
#define MP_MAP32         0xdf

/* ── Read cursor ────────────────────────────────────────────────────── */

struct mp_reader {
    const unsigned char *data;
    size_t len;
    size_t pos;
};

/* ── Field offset item (shared with JSON — same layout) ─────────────  */
/* Reused from the generated offset map; declared in gencode output.    */

/* ── Pack (encode) API ──────────────────────────────────────────────── */

static void mp_pack_nil(sstr_t out);
static void mp_pack_bool(sstr_t out, int v);
static void mp_pack_int(sstr_t out, int64_t v);
static void mp_pack_uint(sstr_t out, uint64_t v);
static void mp_pack_float(sstr_t out, float v);
static void mp_pack_double(sstr_t out, double v);
static void mp_pack_str(sstr_t out, const char *s, uint32_t len);
static void mp_pack_sstr(sstr_t out, sstr_t s);
static void mp_pack_array_header(sstr_t out, uint32_t count);
static void mp_pack_map_header(sstr_t out, uint32_t count);

/* ── Unpack (decode) API ────────────────────────────────────────────── */

static int mp_reader_init(struct mp_reader *r, const unsigned char *data, size_t len);
static int mp_peek(struct mp_reader *r);

static int mp_unpack_nil(struct mp_reader *r);
static int mp_unpack_bool(struct mp_reader *r, int *out);
static int mp_unpack_int64(struct mp_reader *r, int64_t *out);
static int mp_unpack_uint64(struct mp_reader *r, uint64_t *out);
static int mp_unpack_float(struct mp_reader *r, float *out);
static int mp_unpack_double(struct mp_reader *r, double *out);
static int mp_unpack_str(struct mp_reader *r, const char **out, uint32_t *out_len);
static int mp_unpack_array_header(struct mp_reader *r, uint32_t *count);
static int mp_unpack_map_header(struct mp_reader *r, uint32_t *count);

/* Skip one value (any type, recursively for containers). */
static int mp_unpack_skip(struct mp_reader *r);

/* Generic numeric unpack — reads any int/uint/float into an int64_t. */
static int mp_unpack_number_as_int64(struct mp_reader *r, int64_t *out);
static int mp_unpack_number_as_uint64(struct mp_reader *r, uint64_t *out);
static int mp_unpack_number_as_double(struct mp_reader *r, double *out);

#endif /* MSGPACK_CODEC_H_ */
