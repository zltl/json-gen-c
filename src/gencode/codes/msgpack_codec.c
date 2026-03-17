/**
 * @file msgpack_codec.c
 * @brief MessagePack binary codec runtime — embedded into generated code.
 *
 * This file is NOT compiled directly; it is converted to a C byte array
 * via xxd and appended to the generated msgpack.gen.c output.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Suppress unused-function warnings: not every schema uses every type. */
#if defined(__GNUC__) || defined(__clang__)
  #define MP_UNUSED __attribute__((unused))
#elif defined(_MSC_VER)
  #define MP_UNUSED
#else
  #define MP_UNUSED
#endif

/* ── Byte-order helpers (big-endian on the wire) ────────────────────── */

static inline MP_UNUSED void mp_store_be16(unsigned char *p, uint16_t v) {
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)(v);
}

static inline MP_UNUSED void mp_store_be32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)(v);
}

static inline MP_UNUSED void mp_store_be64(unsigned char *p, uint64_t v) {
    p[0] = (unsigned char)(v >> 56);
    p[1] = (unsigned char)(v >> 48);
    p[2] = (unsigned char)(v >> 40);
    p[3] = (unsigned char)(v >> 32);
    p[4] = (unsigned char)(v >> 24);
    p[5] = (unsigned char)(v >> 16);
    p[6] = (unsigned char)(v >> 8);
    p[7] = (unsigned char)(v);
}

static inline MP_UNUSED uint16_t mp_load_be16(const unsigned char *p) {
    return (uint16_t)((uint16_t)p[0] << 8 | (uint16_t)p[1]);
}

static inline MP_UNUSED uint32_t mp_load_be32(const unsigned char *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
           (uint32_t)p[2] << 8  | (uint32_t)p[3];
}

static inline MP_UNUSED uint64_t mp_load_be64(const unsigned char *p) {
    return (uint64_t)p[0] << 56 | (uint64_t)p[1] << 48 |
           (uint64_t)p[2] << 40 | (uint64_t)p[3] << 32 |
           (uint64_t)p[4] << 24 | (uint64_t)p[5] << 16 |
           (uint64_t)p[6] << 8  | (uint64_t)p[7];
}

/* ── Pack (encode) ──────────────────────────────────────────────────── */

static MP_UNUSED void mp_pack_nil(sstr_t out) {
    unsigned char b = MP_NIL;
    sstr_append_of(out, (const char *)&b, 1);
}

static MP_UNUSED void mp_pack_bool(sstr_t out, int v) {
    unsigned char b = v ? MP_TRUE : MP_FALSE;
    sstr_append_of(out, (const char *)&b, 1);
}

static MP_UNUSED void mp_pack_uint(sstr_t out, uint64_t v) {
    unsigned char buf[9];
    if (v <= MP_FIXINT_MAX) {
        buf[0] = (unsigned char)v;
        sstr_append_of(out, (const char *)buf, 1);
    } else if (v <= 0xff) {
        buf[0] = MP_UINT8;
        buf[1] = (unsigned char)v;
        sstr_append_of(out, (const char *)buf, 2);
    } else if (v <= 0xffff) {
        buf[0] = MP_UINT16;
        mp_store_be16(buf + 1, (uint16_t)v);
        sstr_append_of(out, (const char *)buf, 3);
    } else if (v <= 0xffffffffULL) {
        buf[0] = MP_UINT32;
        mp_store_be32(buf + 1, (uint32_t)v);
        sstr_append_of(out, (const char *)buf, 5);
    } else {
        buf[0] = MP_UINT64;
        mp_store_be64(buf + 1, v);
        sstr_append_of(out, (const char *)buf, 9);
    }
}

static MP_UNUSED void mp_pack_int(sstr_t out, int64_t v) {
    if (v >= 0) {
        mp_pack_uint(out, (uint64_t)v);
        return;
    }
    unsigned char buf[9];
    if (v >= -32) {
        buf[0] = (unsigned char)(v & 0xff);  /* negative fixint */
        sstr_append_of(out, (const char *)buf, 1);
    } else if (v >= -128) {
        buf[0] = MP_INT8;
        buf[1] = (unsigned char)(int8_t)v;
        sstr_append_of(out, (const char *)buf, 2);
    } else if (v >= -32768) {
        buf[0] = MP_INT16;
        mp_store_be16(buf + 1, (uint16_t)(int16_t)v);
        sstr_append_of(out, (const char *)buf, 3);
    } else if (v >= -2147483648LL) {
        buf[0] = MP_INT32;
        mp_store_be32(buf + 1, (uint32_t)(int32_t)v);
        sstr_append_of(out, (const char *)buf, 5);
    } else {
        buf[0] = MP_INT64;
        mp_store_be64(buf + 1, (uint64_t)v);
        sstr_append_of(out, (const char *)buf, 9);
    }
}

static MP_UNUSED void mp_pack_float(sstr_t out, float v) {
    unsigned char buf[5];
    uint32_t u;
    buf[0] = MP_FLOAT32;
    memcpy(&u, &v, 4);
    mp_store_be32(buf + 1, u);
    sstr_append_of(out, (const char *)buf, 5);
}

static MP_UNUSED void mp_pack_double(sstr_t out, double v) {
    unsigned char buf[9];
    uint64_t u;
    buf[0] = MP_FLOAT64;
    memcpy(&u, &v, 8);
    mp_store_be64(buf + 1, u);
    sstr_append_of(out, (const char *)buf, 9);
}

static MP_UNUSED void mp_pack_str(sstr_t out, const char *s, uint32_t len) {
    unsigned char buf[5];
    if (len <= MP_FIXSTR_MASK) {
        buf[0] = (unsigned char)(MP_FIXSTR | len);
        sstr_append_of(out, (const char *)buf, 1);
    } else if (len <= 0xff) {
        buf[0] = MP_STR8;
        buf[1] = (unsigned char)len;
        sstr_append_of(out, (const char *)buf, 2);
    } else if (len <= 0xffff) {
        buf[0] = MP_STR16;
        mp_store_be16(buf + 1, (uint16_t)len);
        sstr_append_of(out, (const char *)buf, 3);
    } else {
        buf[0] = MP_STR32;
        mp_store_be32(buf + 1, len);
        sstr_append_of(out, (const char *)buf, 5);
    }
    if (len > 0) {
        sstr_append_of(out, s, len);
    }
}

static MP_UNUSED void mp_pack_sstr(sstr_t out, sstr_t s) {
    mp_pack_str(out, sstr_cstr(s), (uint32_t)sstr_length(s));
}

static MP_UNUSED void mp_pack_array_header(sstr_t out, uint32_t count) {
    unsigned char buf[5];
    if (count <= MP_FIXARRAY_MASK) {
        buf[0] = (unsigned char)(MP_FIXARRAY | count);
        sstr_append_of(out, (const char *)buf, 1);
    } else if (count <= 0xffff) {
        buf[0] = MP_ARRAY16;
        mp_store_be16(buf + 1, (uint16_t)count);
        sstr_append_of(out, (const char *)buf, 3);
    } else {
        buf[0] = MP_ARRAY32;
        mp_store_be32(buf + 1, count);
        sstr_append_of(out, (const char *)buf, 5);
    }
}

static MP_UNUSED void mp_pack_map_header(sstr_t out, uint32_t count) {
    unsigned char buf[5];
    if (count <= MP_FIXMAP_MASK) {
        buf[0] = (unsigned char)(MP_FIXMAP | count);
        sstr_append_of(out, (const char *)buf, 1);
    } else if (count <= 0xffff) {
        buf[0] = MP_MAP16;
        mp_store_be16(buf + 1, (uint16_t)count);
        sstr_append_of(out, (const char *)buf, 3);
    } else {
        buf[0] = MP_MAP32;
        mp_store_be32(buf + 1, count);
        sstr_append_of(out, (const char *)buf, 5);
    }
}

/* ── Unpack (decode) ────────────────────────────────────────────────── */

static MP_UNUSED int mp_reader_init(struct mp_reader *r, const unsigned char *data, size_t len) {
    r->data = data;
    r->len = len;
    r->pos = 0;
    return 0;
}

static MP_UNUSED int mp_peek(struct mp_reader *r) {
    if (r->pos >= r->len) return -1;
    return r->data[r->pos];
}

static MP_UNUSED int mp_read_bytes(struct mp_reader *r, size_t n, const unsigned char **out) {
    if (r->pos + n > r->len) return -1;
    *out = r->data + r->pos;
    r->pos += n;
    return 0;
}

static MP_UNUSED int mp_read1(struct mp_reader *r, unsigned char *out) {
    if (r->pos >= r->len) return -1;
    *out = r->data[r->pos++];
    return 0;
}

static MP_UNUSED int mp_unpack_nil(struct mp_reader *r) {
    unsigned char b;
    if (mp_read1(r, &b) < 0 || b != MP_NIL) return -1;
    return 0;
}

static MP_UNUSED int mp_unpack_bool(struct mp_reader *r, int *out) {
    unsigned char b;
    if (mp_read1(r, &b) < 0) return -1;
    if (b == MP_TRUE)  { *out = 1; return 0; }
    if (b == MP_FALSE) { *out = 0; return 0; }
    return -1;
}

static MP_UNUSED int mp_unpack_int64(struct mp_reader *r, int64_t *out) {
    unsigned char b;
    const unsigned char *p;
    if (mp_read1(r, &b) < 0) return -1;

    /* positive fixint */
    if (b <= MP_FIXINT_MAX) {
        *out = (int64_t)b;
        return 0;
    }
    /* negative fixint */
    if (b >= MP_NEG_FIXINT) {
        *out = (int64_t)(int8_t)b;
        return 0;
    }
    switch (b) {
    case MP_UINT8:
        if (mp_read1(r, &b) < 0) return -1;
        *out = (int64_t)b;
        return 0;
    case MP_UINT16:
        if (mp_read_bytes(r, 2, &p) < 0) return -1;
        *out = (int64_t)mp_load_be16(p);
        return 0;
    case MP_UINT32:
        if (mp_read_bytes(r, 4, &p) < 0) return -1;
        *out = (int64_t)mp_load_be32(p);
        return 0;
    case MP_UINT64:
        if (mp_read_bytes(r, 8, &p) < 0) return -1;
        *out = (int64_t)mp_load_be64(p);
        return 0;
    case MP_INT8:
        if (mp_read1(r, &b) < 0) return -1;
        *out = (int64_t)(int8_t)b;
        return 0;
    case MP_INT16:
        if (mp_read_bytes(r, 2, &p) < 0) return -1;
        *out = (int64_t)(int16_t)mp_load_be16(p);
        return 0;
    case MP_INT32:
        if (mp_read_bytes(r, 4, &p) < 0) return -1;
        *out = (int64_t)(int32_t)mp_load_be32(p);
        return 0;
    case MP_INT64:
        if (mp_read_bytes(r, 8, &p) < 0) return -1;
        *out = (int64_t)mp_load_be64(p);
        return 0;
    default:
        return -1;
    }
}

static MP_UNUSED int mp_unpack_uint64(struct mp_reader *r, uint64_t *out) {
    unsigned char b;
    const unsigned char *p;
    if (mp_read1(r, &b) < 0) return -1;

    if (b <= MP_FIXINT_MAX) {
        *out = (uint64_t)b;
        return 0;
    }
    switch (b) {
    case MP_UINT8:
        if (mp_read1(r, &b) < 0) return -1;
        *out = (uint64_t)b;
        return 0;
    case MP_UINT16:
        if (mp_read_bytes(r, 2, &p) < 0) return -1;
        *out = (uint64_t)mp_load_be16(p);
        return 0;
    case MP_UINT32:
        if (mp_read_bytes(r, 4, &p) < 0) return -1;
        *out = (uint64_t)mp_load_be32(p);
        return 0;
    case MP_UINT64:
        if (mp_read_bytes(r, 8, &p) < 0) return -1;
        *out = mp_load_be64(p);
        return 0;
    /* Also accept signed types and cast */
    case MP_INT8:
        if (mp_read1(r, &b) < 0) return -1;
        *out = (uint64_t)(int8_t)b;
        return 0;
    case MP_INT16:
        if (mp_read_bytes(r, 2, &p) < 0) return -1;
        *out = (uint64_t)(int16_t)mp_load_be16(p);
        return 0;
    case MP_INT32:
        if (mp_read_bytes(r, 4, &p) < 0) return -1;
        *out = (uint64_t)(int32_t)mp_load_be32(p);
        return 0;
    case MP_INT64:
        if (mp_read_bytes(r, 8, &p) < 0) return -1;
        *out = (uint64_t)(int64_t)mp_load_be64(p);
        return 0;
    default:
        return -1;
    }
}

static MP_UNUSED int mp_unpack_float(struct mp_reader *r, float *out) {
    unsigned char b;
    const unsigned char *p;
    if (mp_read1(r, &b) < 0) return -1;
    if (b == MP_FLOAT32) {
        if (mp_read_bytes(r, 4, &p) < 0) return -1;
        uint32_t u = mp_load_be32(p);
        memcpy(out, &u, 4);
        return 0;
    }
    if (b == MP_FLOAT64) {
        if (mp_read_bytes(r, 8, &p) < 0) return -1;
        uint64_t u = mp_load_be64(p);
        double d;
        memcpy(&d, &u, 8);
        *out = (float)d;
        return 0;
    }
    return -1;
}

static MP_UNUSED int mp_unpack_double(struct mp_reader *r, double *out) {
    unsigned char b;
    const unsigned char *p;
    if (mp_read1(r, &b) < 0) return -1;
    if (b == MP_FLOAT64) {
        if (mp_read_bytes(r, 8, &p) < 0) return -1;
        uint64_t u = mp_load_be64(p);
        memcpy(out, &u, 8);
        return 0;
    }
    if (b == MP_FLOAT32) {
        if (mp_read_bytes(r, 4, &p) < 0) return -1;
        uint32_t u = mp_load_be32(p);
        float f;
        memcpy(&f, &u, 4);
        *out = (double)f;
        return 0;
    }
    /* Also accept integers */
    r->pos--;  /* put back the tag byte */
    {
        int64_t iv;
        if (mp_unpack_int64(r, &iv) == 0) {
            *out = (double)iv;
            return 0;
        }
    }
    return -1;
}

static MP_UNUSED int mp_unpack_str(struct mp_reader *r, const char **out, uint32_t *out_len) {
    unsigned char b;
    const unsigned char *p;
    if (mp_read1(r, &b) < 0) return -1;

    uint32_t len;
    if ((b & 0xe0) == MP_FIXSTR) {
        len = b & MP_FIXSTR_MASK;
    } else if (b == MP_STR8) {
        unsigned char lb;
        if (mp_read1(r, &lb) < 0) return -1;
        len = lb;
    } else if (b == MP_STR16) {
        if (mp_read_bytes(r, 2, &p) < 0) return -1;
        len = mp_load_be16(p);
    } else if (b == MP_STR32) {
        if (mp_read_bytes(r, 4, &p) < 0) return -1;
        len = mp_load_be32(p);
    } else {
        return -1;
    }
    if (mp_read_bytes(r, len, &p) < 0) return -1;
    *out = (const char *)p;
    *out_len = len;
    return 0;
}

static MP_UNUSED int mp_unpack_array_header(struct mp_reader *r, uint32_t *count) {
    unsigned char b;
    const unsigned char *p;
    if (mp_read1(r, &b) < 0) return -1;

    if ((b & 0xf0) == MP_FIXARRAY) {
        *count = b & MP_FIXARRAY_MASK;
        return 0;
    }
    if (b == MP_ARRAY16) {
        if (mp_read_bytes(r, 2, &p) < 0) return -1;
        *count = mp_load_be16(p);
        return 0;
    }
    if (b == MP_ARRAY32) {
        if (mp_read_bytes(r, 4, &p) < 0) return -1;
        *count = mp_load_be32(p);
        return 0;
    }
    return -1;
}

static MP_UNUSED int mp_unpack_map_header(struct mp_reader *r, uint32_t *count) {
    unsigned char b;
    const unsigned char *p;
    if (mp_read1(r, &b) < 0) return -1;

    if ((b & 0xf0) == MP_FIXMAP) {
        *count = b & MP_FIXMAP_MASK;
        return 0;
    }
    if (b == MP_MAP16) {
        if (mp_read_bytes(r, 2, &p) < 0) return -1;
        *count = mp_load_be16(p);
        return 0;
    }
    if (b == MP_MAP32) {
        if (mp_read_bytes(r, 4, &p) < 0) return -1;
        *count = mp_load_be32(p);
        return 0;
    }
    return -1;
}

static MP_UNUSED int mp_unpack_skip(struct mp_reader *r) {
    if (r->pos >= r->len) return -1;
    unsigned char b = r->data[r->pos];

    /* positive fixint / negative fixint */
    if (b <= MP_FIXINT_MAX || b >= MP_NEG_FIXINT) {
        r->pos++;
        return 0;
    }

    /* fixstr */
    if ((b & 0xe0) == MP_FIXSTR) {
        uint32_t len = b & MP_FIXSTR_MASK;
        r->pos++;
        if (r->pos + len > r->len) return -1;
        r->pos += len;
        return 0;
    }

    /* fixarray */
    if ((b & 0xf0) == MP_FIXARRAY) {
        uint32_t count = b & MP_FIXARRAY_MASK;
        r->pos++;
        for (uint32_t i = 0; i < count; i++) {
            if (mp_unpack_skip(r) < 0) return -1;
        }
        return 0;
    }

    /* fixmap */
    if ((b & 0xf0) == MP_FIXMAP) {
        uint32_t count = b & MP_FIXMAP_MASK;
        r->pos++;
        for (uint32_t i = 0; i < count; i++) {
            if (mp_unpack_skip(r) < 0) return -1;  /* key */
            if (mp_unpack_skip(r) < 0) return -1;  /* value */
        }
        return 0;
    }

    r->pos++;
    switch (b) {
    case MP_NIL:
    case MP_FALSE:
    case MP_TRUE:
        return 0;
    case MP_UINT8:
    case MP_INT8:
        if (r->pos + 1 > r->len) return -1;
        r->pos += 1;
        return 0;
    case MP_UINT16:
    case MP_INT16:
        if (r->pos + 2 > r->len) return -1;
        r->pos += 2;
        return 0;
    case MP_UINT32:
    case MP_INT32:
    case MP_FLOAT32:
        if (r->pos + 4 > r->len) return -1;
        r->pos += 4;
        return 0;
    case MP_UINT64:
    case MP_INT64:
    case MP_FLOAT64:
        if (r->pos + 8 > r->len) return -1;
        r->pos += 8;
        return 0;
    case MP_STR8: {
        if (r->pos + 1 > r->len) return -1;
        uint32_t len = r->data[r->pos]; r->pos++;
        if (r->pos + len > r->len) return -1;
        r->pos += len;
        return 0;
    }
    case MP_STR16: {
        if (r->pos + 2 > r->len) return -1;
        uint32_t len = mp_load_be16(r->data + r->pos); r->pos += 2;
        if (r->pos + len > r->len) return -1;
        r->pos += len;
        return 0;
    }
    case MP_STR32: {
        if (r->pos + 4 > r->len) return -1;
        uint32_t len = mp_load_be32(r->data + r->pos); r->pos += 4;
        if (r->pos + len > r->len) return -1;
        r->pos += len;
        return 0;
    }
    case MP_ARRAY16: {
        if (r->pos + 2 > r->len) return -1;
        uint32_t count = mp_load_be16(r->data + r->pos); r->pos += 2;
        for (uint32_t i = 0; i < count; i++) {
            if (mp_unpack_skip(r) < 0) return -1;
        }
        return 0;
    }
    case MP_ARRAY32: {
        if (r->pos + 4 > r->len) return -1;
        uint32_t count = mp_load_be32(r->data + r->pos); r->pos += 4;
        for (uint32_t i = 0; i < count; i++) {
            if (mp_unpack_skip(r) < 0) return -1;
        }
        return 0;
    }
    case MP_MAP16: {
        if (r->pos + 2 > r->len) return -1;
        uint32_t count = mp_load_be16(r->data + r->pos); r->pos += 2;
        for (uint32_t i = 0; i < count; i++) {
            if (mp_unpack_skip(r) < 0) return -1;
            if (mp_unpack_skip(r) < 0) return -1;
        }
        return 0;
    }
    case MP_MAP32: {
        if (r->pos + 4 > r->len) return -1;
        uint32_t count = mp_load_be32(r->data + r->pos); r->pos += 4;
        for (uint32_t i = 0; i < count; i++) {
            if (mp_unpack_skip(r) < 0) return -1;
            if (mp_unpack_skip(r) < 0) return -1;
        }
        return 0;
    }
    default:
        return -1;  /* unknown type */
    }
}

/* Generic numeric helpers */
static MP_UNUSED int mp_unpack_number_as_int64(struct mp_reader *r, int64_t *out) {
    unsigned char b = r->data[r->pos];
    if (b == MP_FLOAT32 || b == MP_FLOAT64) {
        double d;
        if (mp_unpack_double(r, &d) < 0) return -1;
        *out = (int64_t)d;
        return 0;
    }
    return mp_unpack_int64(r, out);
}

static MP_UNUSED int mp_unpack_number_as_uint64(struct mp_reader *r, uint64_t *out) {
    unsigned char b = r->data[r->pos];
    if (b == MP_FLOAT32 || b == MP_FLOAT64) {
        double d;
        if (mp_unpack_double(r, &d) < 0) return -1;
        *out = (uint64_t)d;
        return 0;
    }
    return mp_unpack_uint64(r, out);
}

static MP_UNUSED int mp_unpack_number_as_double(struct mp_reader *r, double *out) {
    return mp_unpack_double(r, out);
}

/* End of embedded runtime. */
