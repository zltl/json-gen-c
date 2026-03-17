/**
 * @file cbor_codec.c
 * @brief CBOR (RFC 8949) binary codec runtime — embedded into generated code.
 *
 * This file is NOT compiled directly; it is converted to a C byte array
 * via xxd and appended to the generated cbor.gen.c output.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Suppress unused-function warnings: not every schema uses every type. */
#if defined(__GNUC__) || defined(__clang__)
  #define CB_UNUSED __attribute__((unused))
#elif defined(_MSC_VER)
  #define CB_UNUSED
#else
  #define CB_UNUSED
#endif

/* ── Byte-order helpers (big-endian on the wire) ────────────────────── */

static inline CB_UNUSED void cb_store_be16(unsigned char *p, uint16_t v) {
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)(v);
}

static inline CB_UNUSED void cb_store_be32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)(v);
}

static inline CB_UNUSED void cb_store_be64(unsigned char *p, uint64_t v) {
    p[0] = (unsigned char)(v >> 56);
    p[1] = (unsigned char)(v >> 48);
    p[2] = (unsigned char)(v >> 40);
    p[3] = (unsigned char)(v >> 32);
    p[4] = (unsigned char)(v >> 24);
    p[5] = (unsigned char)(v >> 16);
    p[6] = (unsigned char)(v >> 8);
    p[7] = (unsigned char)(v);
}

static inline CB_UNUSED uint16_t cb_load_be16(const unsigned char *p) {
    return (uint16_t)((uint16_t)p[0] << 8 | (uint16_t)p[1]);
}

static inline CB_UNUSED uint32_t cb_load_be32(const unsigned char *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
           (uint32_t)p[2] << 8  | (uint32_t)p[3];
}

static inline CB_UNUSED uint64_t cb_load_be64(const unsigned char *p) {
    return (uint64_t)p[0] << 56 | (uint64_t)p[1] << 48 |
           (uint64_t)p[2] << 40 | (uint64_t)p[3] << 32 |
           (uint64_t)p[4] << 24 | (uint64_t)p[5] << 16 |
           (uint64_t)p[6] << 8  | (uint64_t)p[7];
}

/* ── Pack: encode a major-type + argument ───────────────────────────── */

static CB_UNUSED void cb_encode_head(sstr_t out, int major, uint64_t val) {
    unsigned char hdr = (unsigned char)(major << 5);
    if (val <= 23) {
        unsigned char b = hdr | (unsigned char)val;
        sstr_append_of(out, (const char *)&b, 1);
    } else if (val <= 0xff) {
        unsigned char buf[2] = { (unsigned char)(hdr | CB_AI_1BYTE), (unsigned char)val };
        sstr_append_of(out, (const char *)buf, 2);
    } else if (val <= 0xffff) {
        unsigned char buf[3];
        buf[0] = hdr | CB_AI_2BYTE;
        cb_store_be16(buf + 1, (uint16_t)val);
        sstr_append_of(out, (const char *)buf, 3);
    } else if (val <= 0xffffffffULL) {
        unsigned char buf[5];
        buf[0] = hdr | CB_AI_4BYTE;
        cb_store_be32(buf + 1, (uint32_t)val);
        sstr_append_of(out, (const char *)buf, 5);
    } else {
        unsigned char buf[9];
        buf[0] = hdr | CB_AI_8BYTE;
        cb_store_be64(buf + 1, val);
        sstr_append_of(out, (const char *)buf, 9);
    }
}

/* ── Pack: typed convenience functions ──────────────────────────────── */

static CB_UNUSED void cb_pack_nil(sstr_t out) {
    unsigned char b = CB_NULL;
    sstr_append_of(out, (const char *)&b, 1);
}

static CB_UNUSED void cb_pack_bool(sstr_t out, int v) {
    unsigned char b = v ? CB_TRUE : CB_FALSE;
    sstr_append_of(out, (const char *)&b, 1);
}

static CB_UNUSED void cb_pack_uint(sstr_t out, uint64_t v) {
    cb_encode_head(out, CB_MAJOR_UINT, v);
}

static CB_UNUSED void cb_pack_int(sstr_t out, int64_t v) {
    if (v >= 0) {
        cb_encode_head(out, CB_MAJOR_UINT, (uint64_t)v);
    } else {
        /* CBOR negative: major 1, value = -(v+1) = -1-v */
        cb_encode_head(out, CB_MAJOR_NINT, (uint64_t)(-(v + 1)));
    }
}

static CB_UNUSED void cb_pack_float(sstr_t out, float v) {
    unsigned char buf[5];
    uint32_t u;
    buf[0] = CB_FLOAT32;
    memcpy(&u, &v, 4);
    cb_store_be32(buf + 1, u);
    sstr_append_of(out, (const char *)buf, 5);
}

static CB_UNUSED void cb_pack_double(sstr_t out, double v) {
    unsigned char buf[9];
    uint64_t u;
    buf[0] = CB_FLOAT64;
    memcpy(&u, &v, 8);
    cb_store_be64(buf + 1, u);
    sstr_append_of(out, (const char *)buf, 9);
}

static CB_UNUSED void cb_pack_str(sstr_t out, const char *s, uint32_t len) {
    cb_encode_head(out, CB_MAJOR_TSTR, (uint64_t)len);
    if (len > 0) sstr_append_of(out, s, len);
}

static CB_UNUSED void cb_pack_sstr(sstr_t out, sstr_t s) {
    cb_pack_str(out, sstr_cstr(s), (uint32_t)sstr_length(s));
}

static CB_UNUSED void cb_pack_array_header(sstr_t out, uint32_t count) {
    cb_encode_head(out, CB_MAJOR_ARRAY, (uint64_t)count);
}

static CB_UNUSED void cb_pack_map_header(sstr_t out, uint32_t count) {
    cb_encode_head(out, CB_MAJOR_MAP, (uint64_t)count);
}

/* ── Unpack: core reader helpers ────────────────────────────────────── */

static CB_UNUSED int cb_reader_init(struct cb_reader *r,
                                     const unsigned char *data, size_t len) {
    if (!data && len > 0) return -1;
    r->data = data;
    r->len = len;
    r->pos = 0;
    return 0;
}

static CB_UNUSED int cb_peek(struct cb_reader *r) {
    if (r->pos >= r->len) return -1;
    return r->data[r->pos];
}

static CB_UNUSED int cb_read_bytes(struct cb_reader *r, size_t n,
                                    const unsigned char **out) {
    if (r->pos + n > r->len) return -1;
    *out = r->data + r->pos;
    r->pos += n;
    return 0;
}

/* Decode the initial byte and return the argument value.
 * Sets *major_out to the major type (0-7).
 * Returns -1 on error, 0 on success. */
static CB_UNUSED int cb_decode_head(struct cb_reader *r, int *major_out,
                                     uint64_t *val_out) {
    if (r->pos >= r->len) return -1;
    unsigned char ib = r->data[r->pos++];
    int major = (ib >> 5) & 0x07;
    int ai = ib & 0x1f;
    *major_out = major;

    if (ai <= 23) {
        *val_out = (uint64_t)ai;
    } else if (ai == CB_AI_1BYTE) {
        if (r->pos + 1 > r->len) return -1;
        *val_out = r->data[r->pos++];
    } else if (ai == CB_AI_2BYTE) {
        if (r->pos + 2 > r->len) return -1;
        *val_out = cb_load_be16(r->data + r->pos);
        r->pos += 2;
    } else if (ai == CB_AI_4BYTE) {
        if (r->pos + 4 > r->len) return -1;
        *val_out = cb_load_be32(r->data + r->pos);
        r->pos += 4;
    } else if (ai == CB_AI_8BYTE) {
        if (r->pos + 8 > r->len) return -1;
        *val_out = cb_load_be64(r->data + r->pos);
        r->pos += 8;
    } else {
        return -1;  /* indefinite length (31) or reserved (28-30) */
    }
    return 0;
}

/* ── Unpack: typed convenience functions ────────────────────────────── */

static CB_UNUSED int cb_unpack_nil(struct cb_reader *r) {
    if (r->pos >= r->len || r->data[r->pos] != CB_NULL) return -1;
    r->pos++;
    return 0;
}

static CB_UNUSED int cb_unpack_bool(struct cb_reader *r, int *out) {
    if (r->pos >= r->len) return -1;
    unsigned char b = r->data[r->pos];
    if (b == CB_TRUE) { *out = 1; r->pos++; return 0; }
    if (b == CB_FALSE) { *out = 0; r->pos++; return 0; }
    return -1;
}

static CB_UNUSED int cb_unpack_uint64(struct cb_reader *r, uint64_t *out) {
    int major;
    uint64_t val;
    size_t save = r->pos;
    if (cb_decode_head(r, &major, &val) < 0) return -1;
    if (major == CB_MAJOR_UINT) {
        *out = val;
        return 0;
    }
    /* Also accept negative zero (major 1, val 0 is -1 — not uint, reject) */
    r->pos = save;
    return -1;
}

static CB_UNUSED int cb_unpack_int64(struct cb_reader *r, int64_t *out) {
    int major;
    uint64_t val;
    size_t save = r->pos;
    if (cb_decode_head(r, &major, &val) < 0) return -1;

    if (major == CB_MAJOR_UINT) {
        if (val > (uint64_t)INT64_MAX) { r->pos = save; return -1; }
        *out = (int64_t)val;
        return 0;
    }
    if (major == CB_MAJOR_NINT) {
        /* CBOR negative: value = -1 - val */
        if (val > (uint64_t)(-(INT64_MIN + 1))) { r->pos = save; return -1; }
        *out = -1 - (int64_t)val;
        return 0;
    }
    r->pos = save;
    return -1;
}

static CB_UNUSED int cb_unpack_float(struct cb_reader *r, float *out) {
    if (r->pos >= r->len) return -1;
    unsigned char ib = r->data[r->pos];
    if (ib == CB_FLOAT32) {
        r->pos++;
        if (r->pos + 4 > r->len) return -1;
        uint32_t u = cb_load_be32(r->data + r->pos);
        r->pos += 4;
        memcpy(out, &u, 4);
        return 0;
    }
    if (ib == CB_FLOAT64) {
        double d;
        r->pos++;
        if (r->pos + 8 > r->len) return -1;
        uint64_t u = cb_load_be64(r->data + r->pos);
        r->pos += 8;
        memcpy(&d, &u, 8);
        *out = (float)d;
        return 0;
    }
    return -1;
}

static CB_UNUSED int cb_unpack_double(struct cb_reader *r, double *out) {
    if (r->pos >= r->len) return -1;
    unsigned char ib = r->data[r->pos];
    if (ib == CB_FLOAT64) {
        r->pos++;
        if (r->pos + 8 > r->len) return -1;
        uint64_t u = cb_load_be64(r->data + r->pos);
        r->pos += 8;
        memcpy(out, &u, 8);
        return 0;
    }
    if (ib == CB_FLOAT32) {
        float f;
        r->pos++;
        if (r->pos + 4 > r->len) return -1;
        uint32_t u = cb_load_be32(r->data + r->pos);
        r->pos += 4;
        memcpy(&f, &u, 4);
        *out = (double)f;
        return 0;
    }
    /* Also accept integers as double */
    int major;
    uint64_t val;
    size_t save = r->pos;
    if (cb_decode_head(r, &major, &val) < 0) return -1;
    if (major == CB_MAJOR_UINT) { *out = (double)val; return 0; }
    if (major == CB_MAJOR_NINT) { *out = -1.0 - (double)val; return 0; }
    r->pos = save;
    return -1;
}

static CB_UNUSED int cb_unpack_str(struct cb_reader *r,
                                    const char **out, uint32_t *out_len) {
    int major;
    uint64_t val;
    size_t save = r->pos;
    if (cb_decode_head(r, &major, &val) < 0) return -1;
    if (major != CB_MAJOR_TSTR) { r->pos = save; return -1; }
    if (val > (uint64_t)(r->len - r->pos)) { r->pos = save; return -1; }
    *out = (const char *)(r->data + r->pos);
    *out_len = (uint32_t)val;
    r->pos += (size_t)val;
    return 0;
}

static CB_UNUSED int cb_unpack_array_header(struct cb_reader *r,
                                             uint32_t *count) {
    int major;
    uint64_t val;
    size_t save = r->pos;
    if (cb_decode_head(r, &major, &val) < 0) return -1;
    if (major != CB_MAJOR_ARRAY) { r->pos = save; return -1; }
    *count = (uint32_t)val;
    return 0;
}

static CB_UNUSED int cb_unpack_map_header(struct cb_reader *r,
                                           uint32_t *count) {
    int major;
    uint64_t val;
    size_t save = r->pos;
    if (cb_decode_head(r, &major, &val) < 0) return -1;
    if (major != CB_MAJOR_MAP) { r->pos = save; return -1; }
    *count = (uint32_t)val;
    return 0;
}

/* ── Skip: recursively skip one CBOR data item ─────────────────────── */

static CB_UNUSED int cb_unpack_skip(struct cb_reader *r) {
    int major;
    uint64_t val;
    if (cb_decode_head(r, &major, &val) < 0) return -1;

    switch (major) {
    case CB_MAJOR_UINT:
    case CB_MAJOR_NINT:
        return 0;  /* argument already consumed */

    case CB_MAJOR_BSTR:
    case CB_MAJOR_TSTR:
        if (r->pos + val > r->len) return -1;
        r->pos += (size_t)val;
        return 0;

    case CB_MAJOR_ARRAY:
        for (uint64_t i = 0; i < val; i++) {
            if (cb_unpack_skip(r) < 0) return -1;
        }
        return 0;

    case CB_MAJOR_MAP:
        for (uint64_t i = 0; i < val; i++) {
            if (cb_unpack_skip(r) < 0) return -1;  /* key */
            if (cb_unpack_skip(r) < 0) return -1;  /* value */
        }
        return 0;

    case CB_MAJOR_TAG:
        /* Skip the tagged content */
        return cb_unpack_skip(r);

    case CB_MAJOR_SIMPLE: {
        /* ai already decoded; check for float payloads */
        unsigned char ai = (unsigned char)(val);
        if (ai <= 23) return 0;  /* simple value in ai */
        /* ai == 24 means 1-byte simple value (already consumed via cb_decode_head) */
        /* ai == 25,26,27 means float16/32/64 — payload already consumed in decode_head as val.
         * However, decode_head reads the argument as integer bytes, not the float payload.
         * For major 7 with ai 25/26/27, the payload was consumed by decode_head. */
        (void)ai;
        return 0;
    }

    default:
        return -1;
    }
}

/* ── Generic numeric helpers (for flexible type coercion) ───────────── */

static CB_UNUSED int cb_unpack_number_as_int64(struct cb_reader *r,
                                                int64_t *out) {
    if (r->pos >= r->len) return -1;
    unsigned char ib = r->data[r->pos];
    /* Float coercion */
    if (ib == CB_FLOAT32 || ib == CB_FLOAT64) {
        double d;
        if (cb_unpack_double(r, &d) < 0) return -1;
        *out = (int64_t)d;
        return 0;
    }
    return cb_unpack_int64(r, out);
}

static CB_UNUSED int cb_unpack_number_as_uint64(struct cb_reader *r,
                                                 uint64_t *out) {
    if (r->pos >= r->len) return -1;
    unsigned char ib = r->data[r->pos];
    if (ib == CB_FLOAT32 || ib == CB_FLOAT64) {
        double d;
        if (cb_unpack_double(r, &d) < 0) return -1;
        *out = (uint64_t)d;
        return 0;
    }
    return cb_unpack_uint64(r, out);
}

static CB_UNUSED int cb_unpack_number_as_double(struct cb_reader *r,
                                                 double *out) {
    return cb_unpack_double(r, out);
}

/* End of embedded runtime. */
