/**
 * @file sstr.c
 * @brief Implementation of the sstr.h header file.
 */

#include "sstr.h"

#include <assert.h>
#include <ctype.h>
#include <malloc.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STR struct sstr_s
#define SSTR(s) ((STR*)(s))

#define STR_PTR(s)                                                        \
    ((SSTR(s))->type == SSTR_TYPE_SHORT                                   \
         ? (SSTR(s))->un.short_str                                        \
         : (SSTR(s)->type == SSTR_TYPE_LONG ? (SSTR(s)->un.long_str.data) \
                                            : (SSTR(s)->un.ref_str.data)))

static void char_to_hex(unsigned char c, unsigned char* buf, int cap) {
    static unsigned char hex[] = "0123456789abcdef";
    static unsigned char HEX[] = "0123456789ABCDEF";

    if (cap) {
        buf[0] = HEX[((c >> 4) & 0x0f)];
        buf[1] = HEX[(c & 0x0f)];
    } else {
        buf[0] = hex[((c >> 4) & 0x0f)];
        buf[1] = hex[(c & 0x0f)];
    }
}

sstr_t sstr_new() {
    STR* s = (STR*)malloc(sizeof(STR));
    memset(s, 0, sizeof(STR));
    return s;
}

void sstr_free(sstr_t s) {
    if (s == NULL) {
        return;
    }
    STR* ss = (STR*)s;
    if (ss->type == SSTR_TYPE_LONG) {
        free(ss->un.long_str.data);
    }
    free(s);
}

sstr_t sstr_of(const void* data, size_t length) {
    STR* s = (STR*)sstr_new();
    if (length <= SHORT_STR_CAPACITY) {
        memcpy(s->un.short_str, data, length);
        s->un.short_str[length] = '\0';
        s->type = SSTR_TYPE_SHORT;
    } else {
        s->un.long_str.data = (char*)malloc(length + 1);
        memcpy(s->un.long_str.data, data, length);
        s->un.long_str.capacity = length;
        s->un.long_str.data[length] = '\0';
        s->type = SSTR_TYPE_LONG;
    }
    s->length = length;
    return s;
}

sstr_t sstr_ref(const void* data, size_t length) {
    STR* s = (STR*)sstr_new();
    s->un.ref_str.data = (char*)data;
    s->length = length;
    s->type = SSTR_TYPE_REF;
    return s;
}

sstr_t sstr(const char* cstr) { return sstr_of(cstr, strlen(cstr)); }

char* sstr_cstr(sstr_t s) { return STR_PTR(s); }

int sstr_compare(sstr_t a, sstr_t b) {
    if (a == NULL && b == NULL) {
        return 0;
    }
    if (a == NULL) {
        return -1;
    }
    if (b == NULL) {
        return 1;
    }
    size_t alen = sstr_length(a), blen = sstr_length(b);
    size_t minlen = alen;
    if (minlen > blen) {
        minlen = blen;
    }

    int c = memcmp(STR_PTR(a), STR_PTR(b), minlen);
    if (c != 0) {
        return c;
    }
    return alen > blen;
}

int sstr_compare_c(sstr_t a, const char* b) {
    size_t alen = sstr_length(a), blen = strlen(b);
    size_t minlen = alen;
    if (minlen > blen) {
        minlen = blen;
    }

    int c = memcmp(STR_PTR(a), b, minlen);
    if (c != 0) {
        return c;
    }
    return alen > blen;
}

void sstr_append_zero(sstr_t s, size_t length) {
    STR* ss = (STR*)s;

    assert(ss->type != SSTR_TYPE_REF);

    if (ss->type == SSTR_TYPE_SHORT) {
        if (ss->length + length <= SHORT_STR_CAPACITY) {
            memset(ss->un.short_str + ss->length, 0, length + 1);
            ss->length += length;
            return;
        } else {
            char* ldata =
                (char*)malloc(length + ss->length + CAP_ADD_DELTA + 1);
            memcpy(ldata, ss->un.short_str, ss->length);
            memset(ldata + ss->length, 0, length + 1);
            ss->un.long_str.data = ldata;
            ss->un.long_str.capacity = length + ss->length + CAP_ADD_DELTA;
            ss->length += length;
            ss->type = SSTR_TYPE_LONG;
            return;
        }
    } else {
        if (ss->un.long_str.capacity - ss->length > length) {
            memset(ss->un.long_str.data + ss->length, 0, length + 1);
            ss->length += length;
            return;
        } else {
            ss->un.long_str.data = (char*)realloc(
                STR_PTR(s), length + ss->length + CAP_ADD_DELTA + 1);
            ss->un.long_str.capacity = length + ss->length + CAP_ADD_DELTA + 1;
            memset(ss->un.long_str.data + ss->length, 0, length + 1);
            ss->length += length;
            return;
        }
    }
}

void sstr_append_of(sstr_t s, const void* data, size_t length) {
    size_t oldlen = sstr_length(s);
    sstr_append_zero(s, length);
    memcpy(STR_PTR(s) + oldlen, data, length);
    STR_PTR(s)[sstr_length(s)] = '\0';
}

void sstr_append(sstr_t dst, sstr_t src) {
    sstr_append_of(dst, STR_PTR(src), sstr_length(src));
}

void sstr_append_cstr(sstr_t dst, const char* src) {
    sstr_append_of(dst, src, strlen(src));
}

sstr_t sstr_dup(sstr_t s) { return sstr_of(STR_PTR(s), sstr_length(s)); }

sstr_t sstr_substr(sstr_t s, size_t index, size_t len) {
    size_t minlen = len;
    size_t str_len = sstr_length(s);
    if (index > str_len) {
        return sstr_new();
    }
    if (index + minlen > str_len) {
        minlen = str_len - index;
    }
    return sstr_of(STR_PTR(s) + index, minlen);
}

void sstr_clear(sstr_t s) {
    STR* ss = (STR*)s;

    switch (ss->type) {
        case SSTR_TYPE_REF:
            ss->length = 0;
            ss->un.ref_str.data = NULL;
            break;
        case SSTR_TYPE_SHORT:
            ss->length = 0;
            ss->un.short_str[0] = 0;
            break;
        case SSTR_TYPE_LONG:
            ss->length = 0;
            free(ss->un.long_str.data);
            ss->un.long_str.data = NULL;
            ss->un.long_str.capacity = 0;
            break;
    }
}

static unsigned char* sstr_sprintf_num(unsigned char* buf, unsigned char* last,
                                       uint64_t ui64, unsigned char zero,
                                       unsigned int hexadecimal,
                                       unsigned width);

sstr_t sstr_printf(const char* fmt, ...) {
    va_list args;
    sstr_t res;

    va_start(args, fmt);
    res = sstr_vslprintf(fmt, args);
    va_end(args);
    return res;
}

sstr_t sstr_printf_append(sstr_t buf, const char* fmt, ...) {
    va_list args;
    sstr_t res;

    va_start(args, fmt);
    res = sstr_vslprintf_append(buf, fmt, args);
    va_end(args);
    return res;
}

sstr_t sstr_vslprintf(const char* fmt, va_list args) {
    sstr_t res = sstr_new();
    sstr_vslprintf_append(res, fmt, args);
    return res;
}

sstr_t sstr_vslprintf_append(sstr_t buf, const char* fmt, va_list args) {
    unsigned char *p, zero;
    int d;
    double f;
    size_t slen;
    size_t i;
    int64_t i64;
    uint64_t ui64, frac, scale;
    unsigned int width, sign, hex, frac_width, frac_width_set, n;
    STR* S;
    /* a default d after %..x/u  */
    int df_d;
    unsigned char tmp[100];
    unsigned char* ptmp;

    while (*fmt) {
        if (*fmt == '%') {
            i64 = 0;
            ui64 = 0;

            zero = (unsigned char)((*++fmt == '0') ? '0' : ' ');
            width = 0;
            sign = 1;
            hex = 0;
            frac_width = 6;
            frac_width_set = 0;
            slen = (size_t)-1;

            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt++ - '0');
            }

            df_d = 0;
            for (;;) {
                switch (*fmt) {
                    case 'u':
                        sign = 0;
                        fmt++;
                        df_d = 1;
                        continue;

                    case 'X':
                        hex = 2;
                        sign = 0;
                        fmt++;
                        df_d = 1;
                        continue;

                    case 'x':
                        hex = 1;
                        sign = 0;
                        fmt++;
                        df_d = 1;
                        continue;

                    case '.':
                        fmt++;
                        frac_width = 0;
                        while (*fmt >= '0' && *fmt <= '9') {
                            frac_width = frac_width * 10 + (*fmt++ - '0');
                            frac_width_set = 1;
                        }

                        break;

                    case '*':
                        slen = va_arg(args, size_t);
                        fmt++;
                        continue;

                    default:
                        break;
                }

                break;
            }

            switch (*fmt) {
                case 'S':
                    S = va_arg(args, STR*);
                    if (S == NULL) {
                        p = (unsigned char*)"NULL";
                        sstr_append_of(buf, p, 4);
                    } else if (hex == 0) {
                        sstr_append(buf, S);
                    } else if (hex) {
                        p = (unsigned char*)STR_PTR(S);
                        slen = sstr_length(S);
                        for (i = 0; i < slen; ++i) {
                            char_to_hex(p[i], tmp, hex == 2);
                            sstr_append_of(buf, tmp, 2);
                        }
                    }

                    fmt++;

                    continue;

                case 's':
                    p = va_arg(args, unsigned char*);

                    if (p == NULL) {
                        p = (unsigned char*)"NULL";
                    }

                    if (slen == (size_t)-1) {
                        sstr_append_of(buf, p, strlen((char*)p));
                    } else {
                        sstr_append_of(buf, p, slen);
                    }

                    fmt++;

                    continue;

                case 'T':
                    i64 = (int64_t)va_arg(args, time_t);
                    sign = 1;
                    df_d = 0;
                    break;

                case 'z':
                    if (sign) {
                        i64 = (int64_t)va_arg(args, long);
                    } else {
                        ui64 = (uint64_t)va_arg(args, unsigned long);
                    }
                    df_d = 0;
                    break;

                case 'd':
                    if (sign) {
                        i64 = (int64_t)va_arg(args, int);
                    } else {
                        ui64 = (uint64_t)va_arg(args, unsigned int);
                    }
                    df_d = 0;
                    break;

                case 'l':
                    if (sign) {
                        i64 = (int64_t)va_arg(args, long);
                    } else {
                        ui64 = (uint64_t)va_arg(args, unsigned long);
                    }
                    df_d = 0;
                    break;

                case 'D':
                    if (sign) {
                        i64 = (int64_t)va_arg(args, int32_t);
                    } else {
                        ui64 = (uint64_t)va_arg(args, uint32_t);
                    }
                    df_d = 0;
                    break;

                case 'L':
                    if (sign) {
                        i64 = va_arg(args, int64_t);
                    } else {
                        ui64 = va_arg(args, uint64_t);
                    }
                    df_d = 0;
                    break;

                case 'f':
                    f = va_arg(args, double);

                    if (f < 0) {
                        sstr_append_of(buf, "-", 1);
                        f = -f;
                    }

                    ui64 = (int64_t)f;
                    frac = 0;

                    if (frac_width) {
                        scale = 1;
                        for (n = frac_width; n; n--) {
                            scale *= 10;
                        }

                        frac = (uint64_t)((f - (double)ui64) * scale + 0.5);

                        if (frac == scale) {
                            ui64++;
                            frac = 0;
                        }
                    }

                    ptmp = sstr_sprintf_num(tmp, tmp + sizeof(tmp), ui64, zero,
                                            0, width);
                    sstr_append_of(buf, tmp, ptmp - tmp);

                    if (frac_width) {
                        sstr_append_of(buf, ".", 1);
                        ptmp = sstr_sprintf_num(tmp, tmp + sizeof(tmp), frac,
                                                '0', 0, frac_width);
                        if (frac_width_set == 0) {
                            while (ptmp > tmp && *(ptmp - 1) == '0') {
                                ptmp--;
                            }
                        }
                        sstr_append_of(buf, tmp, ptmp - tmp);
                    }

                    fmt++;

                    continue;

                case 'p':
                    ui64 = (uintptr_t)va_arg(args, void*);
                    hex = 2;
                    sign = 0;
                    zero = '0';
                    width = 2 * sizeof(void*);
                    break;

                case 'c':
                    d = va_arg(args, int);
                    sstr_append_of(buf, (unsigned char*)&d, 1);
                    fmt++;

                    continue;

                case 'Z':
                    sstr_append_of(buf, (unsigned char*)"\0", 1);
                    fmt++;

                    continue;

                case 'N':
                    sstr_append_of(buf, (unsigned char*)"\n", 1);
                    fmt++;

                    continue;

                case '%':
                    sstr_append_of(buf, (unsigned char*)"%", 1);
                    fmt++;

                    continue;

                default:
                    if (df_d) {
                        if (sign) {
                            i64 = (int64_t)va_arg(args, int);
                        } else {
                            ui64 = (uint64_t)va_arg(args, unsigned int);
                        }
                        break;
                    }
                    if (*fmt) sstr_append_of(buf, fmt++, 1);

                    continue;
            }

            if (sign) {
                if (i64 < 0) {
                    sstr_append_of(buf, "-", 1);
                    ui64 = (uint64_t)-i64;

                } else {
                    ui64 = (uint64_t)i64;
                }
            }

            ptmp = sstr_sprintf_num(tmp, tmp + sizeof(tmp), ui64, zero, hex,
                                    width);
            sstr_append_of(buf, tmp, ptmp - tmp);

            if (df_d && *fmt) {  // %xabc not %xd, move a to buf
                sstr_append_of(buf, fmt++, 1);
            } else if (*fmt) {
                fmt++;
            }

        } else {
            ptmp = (unsigned char*)fmt;
            while (*fmt && (*fmt) != '%') {
                fmt++;
            }
            sstr_append_of(buf, ptmp, (unsigned char*)fmt - ptmp);
        }
    }

    return buf;
}

#define SSTR_INT32_LEN (sizeof("-2147483648") - 1)
#define SSTR_INT64_LEN (sizeof("-9223372036854775808") - 1)

#define SSTR_MAX_UINT32_VALUE (uint32_t)0xffffffff
#define SSTR_MAX_INT32_VALUE (uint32_t)0x7fffffff

static unsigned char* sstr_sprintf_num(unsigned char* buf, unsigned char* last,
                                       uint64_t ui64, unsigned char zero,
                                       unsigned int hexadecimal,
                                       unsigned width) {
    unsigned char *p, temp[SSTR_INT64_LEN + 1];
    size_t len;
    uint32_t ui32;
    static unsigned char hex[] = "0123456789abcdef";
    static unsigned char HEX[] = "0123456789ABCDEF";

    p = temp + SSTR_INT64_LEN;

    if (hexadecimal == 0) {
        if (ui64 <= (uint64_t)SSTR_MAX_UINT32_VALUE) {
            ui32 = (uint32_t)ui64;

            do {
                *--p = (unsigned char)(ui32 % 10 + '0');
            } while (ui32 /= 10);

        } else {
            do {
                *--p = (unsigned char)(ui64 % 10 + '0');
            } while (ui64 /= 10);
        }

    } else if (hexadecimal == 1) {
        do {
            *--p = hex[(uint32_t)(ui64 & 0xf)];
        } while (ui64 >>= 4);

    } else { /* hexadecimal == 2 */

        do {
            *--p = HEX[(uint32_t)(ui64 & 0xf)];
        } while (ui64 >>= 4);
    }

    /* zero or space padding */

    len = (temp + SSTR_INT64_LEN) - p;

    while (len++ < width && buf < last) {
        *buf++ = zero;
    }

    /* number safe copy */

    len = (temp + SSTR_INT64_LEN) - p;

    if (buf + len > last) {
        len = last - buf;
    }

    memcpy(buf, p, len);
    buf += len;
    return buf;
}

const char* sstr_version() {
    static const char* const version = "1.1.1";
    return version;
}

void sstr_append_int_str(sstr_t s, int i) {
    unsigned char buf[SSTR_INT32_LEN + 1];
    unsigned char* p = buf + SSTR_INT32_LEN;
    uint32_t ui32;
    int negative = 0;

    if (i < 0) {
        negative = 1;
        ui32 = (uint32_t)-i;
    } else {
        ui32 = (uint32_t)i;
    }

    do {
        *--p = (unsigned char)(ui32 % 10 + '0');
    } while (ui32 /= 10);

    if (negative) {
        sstr_append_of(s, "-", 1);
    }
    sstr_append_of(s, p, buf + SSTR_INT32_LEN - p);
}

int sstr_parse_long(sstr_t s, long* v) {
    size_t i = 0;
    int negative = 0;
    *v = 0;
    unsigned char* p = (unsigned char*)STR_PTR(s);
    for (i = 0; i < sstr_length(s); ++i) {
        if (isspace(p[i])) {
            continue;
        }
        if (p[i] == '-') {
            negative = 1;
        }
    }
    for (; i < sstr_length(s); ++i) {
        if (isdigit(p[i])) {
            *v = *v * 10 + p[i] - '0';
        }
    }

    if (negative) {
        *v = -*v;
    }

    return i;
}

int sstr_parse_int(sstr_t* s, int* v) {
    long vr;
    int r = sstr_parse_long(s, &vr);
    *v = (int)vr;
    return r;
}

void sstr_append_long_str(sstr_t s, long l) {
    unsigned char buf[SSTR_INT64_LEN + 1];
    unsigned char* p = buf + SSTR_INT64_LEN;
    uint64_t ui64;
    int negative = 0;
    if (l < 0) {
        negative = 1;
        ui64 = (uint64_t)-l;
    } else {
        ui64 = (uint64_t)l;
    }

    do {
        *--p = (unsigned char)(ui64 % 10 + '0');
    } while (ui64 /= 10);
    if (negative) {
        sstr_append_of(s, "-", 1);
    }
    sstr_append_of(s, p, buf + SSTR_INT64_LEN - p);
}

void sstr_append_float_str(sstr_t s, float f, int precission) {
    sstr_append_double_str(s, (double)f, precission);
}

// #define MAX_DOUBLE_LEN (sizeof("-1.7976931348623157E+308") - 1)
void sstr_append_double_str(sstr_t s, double f, int precision) {
    unsigned char buf[SSTR_INT64_LEN + 1];
    unsigned char* p = buf + SSTR_INT64_LEN;
    uint64_t ui64;
    int negative = 0;
    double f2;  // fractional part

    // int part
    if (f < 0) {
        negative = 1;
        ui64 = (uint64_t)-f;
        f2 = -f - ui64;
    } else {
        ui64 = (uint64_t)f;
        f2 = f - ui64;
    }

    do {
        *--p = (unsigned char)(ui64 % 10 + '0');
    } while (ui64 /= 10);
    if (negative) {
        sstr_append_of(s, "-", 1);
    }
    sstr_append_of(s, p, buf + SSTR_INT64_LEN - p);

    // float part
    if (f2 > 0 && f2 > 1e-6) {
        sstr_append_of(s, ".", 1);
        p = buf;
        do {
            f2 *= 10;
            *p++ = (unsigned char)(f2 + '0');
            f2 -= (long)f2;
        } while (f2 > 1e-6 && f2 > 0.0 && p < buf + precision);

        sstr_append_of(s, buf, p - buf);
    }
}

int sstr_parse_double(sstr_t s, double* v) {
    size_t i = 0;
    int negative = 0;
    *v = 0;
    unsigned char* p = (unsigned char*)STR_PTR(s);
    for (i = 0; i < sstr_length(s); ++i) {
        if (isspace(p[i])) {
            continue;
        }
        if (p[i] == '-') {
            negative = 1;
        }
    }
    for (; i < sstr_length(s); ++i) {
        if (isdigit(p[i])) {
            *v = *v * 10 + p[i] - '0';
        }
    }
    if (p[i] == '.') i++;

    double v2 = 0, d2 = 10;
    for (; i < sstr_length(s); ++i) {
        if (isdigit(p[i])) {
            v2 += (p[i] - '0') / d2;
            d2 *= 10;
        }
    }
    *v += v2;

    if (negative) {
        *v = -*v;
    }

    return i;
}

int sstr_json_escape_string_append(sstr_t out, sstr_t in) {
    if (in == NULL) {
        return 0;
    }
    size_t i = 0;
    unsigned char* data = (unsigned char*)sstr_cstr(in);
    size_t in_len = sstr_length(in);
    for (i = 0; i < in_len; ++i) {
        if (data[i] <= 31 || data[i] == '\"' || data[i] == '\\') {
            // character needs to be escaped
            sstr_append_of(out, "\\", 1);
            switch (data[i]) {
                case '\\':
                    sstr_append_of(out, "\\", 1);
                    break;
                case '\"':
                    sstr_append_of(out, "\"", 1);
                    break;
                case '\b':
                    sstr_append_of(out, "b", 1);
                    break;
                case '\f':
                    sstr_append_of(out, "f", 1);
                    break;
                case '\n':
                    sstr_append_of(out, "n", 1);
                    break;
                case '\r':
                    sstr_append_of(out, "r", 1);
                    break;
                case '\t':
                    sstr_append_of(out, "t", 1);
                    break;
                default: {
                    // escape and print as unicode codepoint
                    char tmp[7] = {0};
                    sprintf(tmp, "u%04x", *(data + i));
                    sstr_append_cstr(out, tmp);
                }
            }
        } else {
            size_t j;
            for (j = i + 1; j < in_len; ++j) {
                if (data[j] <= 31 || data[j] == '\"' || data[j] == '\\') {
                    break;
                }
            }
            sstr_append_of(out, data + i, j - i);
            i += j - i - 1;
        }
    }
    return 0;
}

void sstr_append_of_if(sstr_t s, const void* data, size_t length, int cond) {
    if (cond) {
        size_t oldlen = sstr_length(s);
        sstr_append_zero(s, length);
        memcpy(STR_PTR(s) + oldlen, data, length);
        STR_PTR(s)[sstr_length(s)] = '\0';
    }
}

void sstr_append_indent(sstr_t s, size_t indent) {
    if (indent == 0) {
        return;
    }
    size_t cur_len = sstr_length(s);
    sstr_append_zero(s, indent);
    for (size_t i = 0; i < indent; i++) {
        STR_PTR(s)[cur_len + i] = ' ';
    }
}
