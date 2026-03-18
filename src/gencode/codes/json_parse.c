#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sstr.h"
#include "utils/error_codes.h"

/* Fast character classification lookup table (replaces locale-aware
   isspace/isdigit in hot loops). Bit 0 = whitespace, Bit 1 = digit. */
static const unsigned char json_char_class_[256] = {
    /* 0x00-0x08 */ 0,0,0,0,0,0,0,0,0,
    /* 0x09 \t  */ 1,
    /* 0x0A \n  */ 1,
    /* 0x0B \v  */ 1,
    /* 0x0C \f  */ 1,
    /* 0x0D \r  */ 1,
    /* 0x0E-0x1F */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* 0x20 ' ' */ 1,
    /* 0x21-0x2F */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* 0x30 '0' - 0x39 '9' */ 2,2,2,2,2,2,2,2,2,2,
    /* 0x3A-0xFF: all zero (remaining 198 entries) */
};
#define JSON_IS_SPACE(c) (json_char_class_[(unsigned char)(c)] & 1)
#define JSON_IS_DIGIT(c) (json_char_class_[(unsigned char)(c)] & 2)

/* Allocator indirection — users may override before including generated code.
 *
 * Compile-time: define JGENC_MALLOC/JGENC_REALLOC/JGENC_FREE before including
 * the generated .c file, e.g. -DJGENC_MALLOC=my_malloc.
 *
 * Run-time: call json_gen_c_set_alloc() to redirect allocations without
 * recompiling.  When custom macros are defined the runtime API is disabled.
 */
#ifndef JGENC_MALLOC
static void* (*jgenc_malloc_fn_)(size_t)                        = NULL;
static void* (*jgenc_realloc_fn_)(void*, size_t)                = NULL;
static void  (*jgenc_free_fn_)(void*)                           = NULL;

static inline void* jgenc_malloc_dispatch_(size_t sz) {
    return jgenc_malloc_fn_ ? jgenc_malloc_fn_(sz) : malloc(sz);
}
static inline void* jgenc_realloc_dispatch_(void* p, size_t sz) {
    return jgenc_realloc_fn_ ? jgenc_realloc_fn_(p, sz) : realloc(p, sz);
}
static inline void jgenc_free_dispatch_(void* p) {
    if (jgenc_free_fn_) jgenc_free_fn_(p); else free(p);
}

#define JGENC_MALLOC(sz)      jgenc_malloc_dispatch_(sz)
#define JGENC_REALLOC(p, sz)  jgenc_realloc_dispatch_((p), (sz))
#define JGENC_FREE(p)         jgenc_free_dispatch_(p)

/**
 * @brief Override the memory allocator used by generated JSON code at runtime.
 *
 * Pass NULL for any parameter to keep the default (stdlib) implementation.
 * This function is NOT thread-safe — call it once during program
 * initialisation, before any marshal / unmarshal calls.
 */
void json_gen_c_set_alloc(void* (*malloc_fn)(size_t),
                           void* (*realloc_fn)(void*, size_t),
                           void  (*free_fn)(void*)) {
    jgenc_malloc_fn_  = malloc_fn;
    jgenc_realloc_fn_ = realloc_fn;
    jgenc_free_fn_    = free_fn;
}
#else
/* Custom compile-time macros — runtime API disabled */
#endif
#ifndef JGENC_REALLOC
#define JGENC_REALLOC(p, sz) realloc((p), (sz))
#endif
#ifndef JGENC_FREE
#define JGENC_FREE(p) free(p)
#endif

/* ---------------------------------------------------------------
 * Inline fast-path helpers for sstr operations.
 *
 * The sstr library lives in a separate translation unit, so none of
 * its tiny accessor functions (sstr_cstr, sstr_clear, sstr_append_of)
 * can be inlined by the compiler.  The struct layout is defined in
 * sstr.h, so we can build fast-path macros here that the compiler
 * will inline at every call-site in the generated runtime.
 *
 * These eliminate the function-call overhead that profiling showed
 * accounts for ~27% of total unmarshal time (sstr_cstr 8.9%,
 * sstr_append_zero 8.5%, ptoken 7.2%, sstr_append_of 6.1%,
 * sstr_clear 4.1%).
 * --------------------------------------------------------------- */

/* Inline sstr_cstr: return pointer to the raw char data. */
#define SSTR_I_(s)  ((struct sstr_s*)(s))
#define SSTR_CSTR_(s) \
    (SSTR_I_(s)->type == SSTR_TYPE_SHORT \
        ? SSTR_I_(s)->un.short_str \
        : SSTR_I_(s)->un.long_str.data)

/* Inline sstr_clear for the hot path (token buffer is always SHORT or LONG). */
static inline void sstr_clear_fast_(sstr_t s) {
    struct sstr_s* ss = (struct sstr_s*)s;
    if (ss->type == SSTR_TYPE_SHORT) {
        ss->length = 0;
        ss->un.short_str[0] = '\0';
    } else if (ss->type == SSTR_TYPE_LONG) {
        /* Keep the allocated buffer; just reset length. */
        ss->length = 0;
        ss->un.long_str.data[0] = '\0';
    } else {
        sstr_clear(s);
    }
}

/* Inline sstr_append_of: fast path for SHORT string with capacity. */
static inline void sstr_append_of_fast_(sstr_t s, const void* data, size_t len) {
    struct sstr_s* ss = (struct sstr_s*)s;
    if (ss->type == SSTR_TYPE_SHORT && ss->length + len <= SHORT_STR_CAPACITY) {
        memcpy(ss->un.short_str + ss->length, data, len);
        ss->length += len;
        ss->un.short_str[ss->length] = '\0';
        return;
    }
    if (ss->type == SSTR_TYPE_LONG &&
        ss->un.long_str.capacity - ss->length > len) {
        memcpy(ss->un.long_str.data + ss->length, data, len);
        ss->length += len;
        ss->un.long_str.data[ss->length] = '\0';
        return;
    }
    sstr_append_of(s, data, len);
}

/*
  hash map to describe structs like:

    hash(structname, fieldname) --> index of json_field_offset_item
                                      |
                                -------
                                |
                                v
    json_field_offset_item:  [] [index of item] [] [] [] []
                                       |
                                       -----
                                           |
                                           v
    json_field_offset_item: [item] [item] [item] item
                                          |:
                                            {
                                                offset, type_size ...
                                            }

    json parser need this information to get the location of each field,
    then store value to the right location.
*/
extern struct json_field_offset_item json_field_offset_item[];
extern int json_entry_hash_size;
extern int json_entry_hash[];

static unsigned int hash_s(const char* data, size_t n, unsigned int seed) {
    // unsigned int seed = 0xbc9f1d34;
    // Similar to murmur hash
    const unsigned int m = 0xc6a4a793;
    const unsigned int r = 24;
    const char* limit = data + n;
    assert(n <= (size_t)UINT_MAX);
    unsigned int h = seed ^ ((unsigned int)n * m);

    // Pick up four bytes at a time
    while (data + 4 <= limit) {
        unsigned int w = *(unsigned int*)(data);
        data += 4;
        h += w;
        h *= m;
        h ^= (h >> 16);
    }

    // Pick up remaining bytes
    switch (limit - data) {
        case 3:
            h += (unsigned char)(data[2]) << 16;
            /* fall through */
        case 2:
            h += (unsigned char)(data[1]) << 8;
            /* fall through */
        case 1:
            h += (unsigned char)(data[0]);
            h *= m;
            h ^= (h >> r);
            break;
    }
    return h;
}

static unsigned int hash_2s_c(const char* key1, const char* key2) {
    unsigned int h = 0xbc9f1d34;
    h = hash_s(key1, strlen(key1), h);
    h = hash_s("#", 1, h);
    h = hash_s(key2, strlen(key2), h);
    return h;
}

/* Pre-hashed variant: caller supplies the partial hash that already includes
   the struct_name portion, so we only hash the "#" separator + field name. */
static unsigned int hash_2s_prehash(unsigned int st_hash, const char* key2,
                                     size_t key2_len) {
    unsigned int h = st_hash;
    h = hash_s("#", 1, h);
    h = hash_s(key2, key2_len, h);
    return h;
}

// find the index of json_field_offset_item by structname and field name
static struct json_field_offset_item* json_field_offset_item_find(
    const char* st, const char* field) {
    unsigned int h = hash_2s_c(st, field) % json_entry_hash_size;
    int id = json_entry_hash[h];
    if (id < 0) {
        return NULL;
    }

    do {
        struct json_field_offset_item* item = &json_field_offset_item[id];
        if (strcmp(st, item->struct_name) == 0 &&
            strcmp(field, item->field_name) == 0) {
            return item;
        }
        h++;
        if ((int)h >= json_entry_hash_size) {
            h = 0;
        }
        id = json_entry_hash[h];
        if (id < 0) {
            return NULL;
        }
    } while (1);

    return NULL;
}

/* Lookup using pre-hashed struct name, avoids recomputing struct_name hash on
   every field.  field_len must equal strlen(field). */
static struct json_field_offset_item* json_field_offset_item_find_ph(
    unsigned int st_hash, const char* st, const char* field,
    size_t field_len) {
    unsigned int h =
        hash_2s_prehash(st_hash, field, field_len) % json_entry_hash_size;
    int id = json_entry_hash[h];
    if (id < 0) {
        return NULL;
    }

    do {
        struct json_field_offset_item* item = &json_field_offset_item[id];
        if (strcmp(st, item->struct_name) == 0 &&
            strcmp(field, item->field_name) == 0) {
            return item;
        }
        h++;
        if ((int)h >= json_entry_hash_size) {
            h = 0;
        }
        id = json_entry_hash[h];
        if (id < 0) {
            return NULL;
        }
    } while (1);

    return NULL;
}

/// print tokens, for debug
static char* ptoken(int type, sstr_t txt) {
    switch (type) {
        case JSON_TOKEN_LEFT_BRACKET:
            return "[";
        case JSON_TOKEN_RIGHT_BRACKET:
            return "]";
        case JSON_TOKEN_LEFT_BRACE:
            return "{";
        case JSON_TOKEN_RIGHT_BRACE:
            return "}";
        case JSON_TOKEN_COMMA:
            return ",";
        case JSON_TOKEN_COLON:
            return ":";
        case JSON_TOKEN_TRUE:
            return "true";
        case JSON_TOKEN_FALSE:
            return "false";
        case JSON_TOKEN_NULL:
            return "null";
        case JSON_TOKEN_STRING:
        case JSON_TOKEN_INT:
        case JSON_TOKEN_FLOAT:
            return SSTR_CSTR_(txt);
        case JSON_TOKEN_EOF:
            return "-EOF-";
        case JSON_ERROR:
            return "-ERROR-";
        default:
            return "-UNKOWN-";
    }
    return "";
}

// print error messages, for debugs
#define PERROR(pos, msg, ...) \
    sstr_printf("line %d col %d: error: " msg, pos->line, pos->col, ##__VA_ARGS__)

static int json_next_token_(sstr_t content, struct json_pos* pos, sstr_t txt);

static int json_next_token(sstr_t content, struct json_pos* pos, sstr_t txt) {
    int tk = json_next_token_(content, pos, txt);
#ifdef JSON_DEBUG
    printf("TOKEN>%s /line %d col %d\n", ptoken(tk, txt), pos->line, pos->col);
#endif
    return tk;
}

/* parse 4 digit hexadecimal number */
static unsigned int parse_hex4(const unsigned char* const input) {
    unsigned int h = 0;
    size_t i = 0;

    for (i = 0; i < 4; i++) {
        /* parse digit */
        if ((input[i] >= '0') && (input[i] <= '9')) {
            h += (unsigned int)input[i] - '0';
        } else if ((input[i] >= 'A') && (input[i] <= 'F')) {
            h += (unsigned int)10 + input[i] - 'A';
        } else if ((input[i] >= 'a') && (input[i] <= 'f')) {
            h += (unsigned int)10 + input[i] - 'a';
        } else /* invalid */
        {
            return 0;
        }

        if (i < 3) {
            /* shift left to make place for the next nibble */
            h = h << 4;
        }
    }

    return h;
}

// uXXXX [\uxxxx]
static int utf16_literal_to_utf8(sstr_t content, struct json_pos* pos,
                                 sstr_t txt) {
    char* data = SSTR_CSTR_(content);
    long i = pos->offset;
    long len = sstr_length(content);
    if (i + 5 >= len) {
        sstr_clear(txt);
        sstr_append_cstr(txt,
                         "expected escape UTF-16 sequence, "
                         "but reached end of json string");
        return JSON_ERROR;
    }
    i++;
    unsigned int first_code = parse_hex4((const unsigned char*)&data[i]);
    unsigned int codepoint = 0;
    i += 4;
    pos->col += 5;
    pos->offset += 5;
    /* check that the code is valid */
    if (((first_code >= 0xDC00) && (first_code <= 0xDFFF))) {
        sstr_clear(txt);
        sstr_append_cstr(txt,
                         "expected escape UTF-16 sequence, but found invalid");
        return JSON_ERROR;
    }
    // UTF16 surrogate pair
    if ((first_code >= 0xD800) && (first_code <= 0xDBFF)) {
        unsigned int second_code;

        if (i + 6 >= len) {
            sstr_clear(txt);
            sstr_append_cstr(txt, "UTF16 surrogate pair expected, but EOF");
            return JSON_ERROR;
        }
        if ((data[i] != '\\') || (data[i + 1] != 'u')) {
            sstr_clear(txt);
            sstr_append_cstr(
                txt, "UTF16 surrogate pair expected, but not found \\uXXXX");
            return JSON_ERROR;
        }
        second_code = parse_hex4((const unsigned char*)&data[i + 2]);
        /* check that the code is valid */
        if ((second_code < 0xDC00) || (second_code > 0xDFFF)) {
            sstr_clear(txt);
            sstr_append_cstr(
                txt, "expected escape UTF-16 second_code, but found invalid");
            return JSON_ERROR;
        }
        /* calculate the unicode codepoint from the surrogate pair */
        codepoint =
            0x10000 + (((first_code & 0x3FF) << 10) | (second_code & 0x3FF));
        pos->col += 6;
        pos->offset += 6;
    } else {
        codepoint = first_code;
    }

    int utf8_length = 0;
    unsigned char first_byte_mark = 0;
    /* encode as UTF-8
     * takes at maximum 4 bytes to encode:
     * 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    if (codepoint < 0x80) {
        /* normal ascii, encoding 0xxxxxxx */
        utf8_length = 1;
    } else if (codepoint < 0x800) {
        /* two bytes, encoding 110xxxxx 10xxxxxx */
        utf8_length = 2;
        first_byte_mark = 0xC0; /* 11000000 */
    } else if (codepoint < 0x10000) {
        /* three bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx */
        utf8_length = 3;
        first_byte_mark = 0xE0; /* 11100000 */
    } else if (codepoint <= 0x10FFFF) {
        /* four bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx 10xxxxxx */
        utf8_length = 4;
        first_byte_mark = 0xF0; /* 11110000 */
    } else {
        /* invalid unicode codepoint */
        sstr_clear(txt);
        sstr_append_cstr(txt,
                         "invalid unicode codepoint, cannot convert to utf8");
        return JSON_ERROR;
    }
    int utf8_position;
    unsigned char output_pointer[4];
    for (utf8_position = (unsigned char)(utf8_length - 1); utf8_position > 0;
         utf8_position--) {
        /* 10xxxxxx */
        output_pointer[utf8_position] =
            (unsigned char)((codepoint | 0x80) & 0xBF);
        codepoint >>= 6;
    }
    /* encode first byte */
    if (utf8_length > 1) {
        (output_pointer)[0] =
            (unsigned char)((codepoint | first_byte_mark) & 0xFF);
    } else {
        (output_pointer)[0] = (unsigned char)(codepoint & 0x7F);
    }
    sstr_append_of(txt, output_pointer, utf8_length);
    return 0;
}

/**
 * @brief Parse a JSON string token with proper bounds checking
 * @param content JSON content string
 * @param pos Current parsing position
 * @param txt Output string buffer
 * @return Token type or JSON_ERROR on failure
 */
static int json_parse_string_token(sstr_t content, struct json_pos* pos,
                                   sstr_t txt) {
    long len = sstr_length(content);
    long i;
    char* data = SSTR_CSTR_(content);
    
    // Validate input parameters
    if (data == NULL || txt == NULL || pos == NULL) {
        return JSON_ERROR;
    }
    
    i = pos->offset;

    // Validate bounds before accessing data[i]
    if (i >= len) {
        sstr_clear(txt);
        sstr_t e = PERROR(pos, "unexpected end of input when expecting string");
        sstr_append(txt, e);
        sstr_free(e);
        return JSON_ERROR;
    }

    // data[i] should be '"' - validate this
    if (data[i] != '"') {
        sstr_clear(txt);
        sstr_t e = PERROR(pos, "expected '\"' at start of string, got '%c'", data[i]);
        sstr_append(txt, e);
        sstr_free(e);
        return JSON_ERROR;
    }
    
    // Move past opening quote
    i++;
    pos->col++;

    sstr_clear_fast_(txt);
    while (i < len && data[i] != '"') {
        if (data[i] == '\\') {
            // Handle escape sequence with proper bounds checking
            if (i + 1 >= len) {
                sstr_clear(txt);
                sstr_t e = PERROR(pos,
                                  "expected escape sequence, but reached "
                                  "end of json string");
                sstr_append(txt, e);
                sstr_free(e);
                return JSON_ERROR;
            }
            i++;
            pos->col++;

            switch (data[i]) {
                case 'b':
                    sstr_append_of_fast_(txt, "\b", 1);
                    i++;
                    pos->col++;
                    break;
                case 'f':
                    sstr_append_of_fast_(txt, "\f", 1);
                    i++;
                    pos->col++;
                    break;
                case 'n':
                    sstr_append_of_fast_(txt, "\n", 1);
                    i++;
                    pos->col++;
                    break;
                case 'r':
                    sstr_append_of_fast_(txt, "\r", 1);
                    i++;
                    pos->col++;
                    break;
                case 't':
                    sstr_append_of_fast_(txt, "\t", 1);
                    i++;
                    pos->col++;
                    break;
                case '\"':
                    sstr_append_of_fast_(txt, "\"", 1);
                    i++;
                    pos->col++;
                    break;
                case '\\':
                    sstr_append_of_fast_(txt, "\\", 1);
                    i++;
                    pos->col++;
                    break;
                case '/':
                    sstr_append_of_fast_(txt, "/", 1);
                    i++;
                    pos->col++;
                    break;
                /* UTF-16 literal */
                case 'u': {
                    pos->offset = i;
                    sstr_t tmp = sstr_new();
                    int r = utf16_literal_to_utf8(content, pos, tmp);
                    if (r != 0) {
                        sstr_clear(txt);
                        sstr_append(txt, tmp);
                        sstr_free(tmp);
                        return JSON_ERROR;
                    }
                    sstr_append(txt, tmp);
                    i = pos->offset;
                    break;
                }
                default: {
                    sstr_clear(txt);
                    sstr_t e =
                        PERROR(pos, "unknown escape sequence '\\%s'", data[i]);
                    sstr_append(txt, e);
                    sstr_free(e);
                    return JSON_ERROR;
                }
            }
        } else {
            int j = i;
            while (j < len && data[j] != '"' && data[j] != '\\') {
                j++;
            }
            sstr_append_of_fast_(txt, data + i, j - i);
            pos->col += j - i;
            i = j;
        }
    }
    if (data[i] != '\"') {
        sstr_clear(txt);
        sstr_t e = PERROR(pos, "expected '\"', but got '%s'", data[i]);
        sstr_append(txt, e);
        sstr_free(e);
        return JSON_ERROR;
    }
    pos->offset = i + 1;
    pos->col++;

    return JSON_TOKEN_STRING;
}

static inline int json_skip_space_comments(sstr_t content, struct json_pos* pos) {
    long len = sstr_length(content);
    long i = pos->offset;
    char* data = SSTR_CSTR_(content);

    int skiped = 0;

    do {
        skiped = 0;
        // trim spaces
        while (i < len && JSON_IS_SPACE(data[i])) {
            i++;
            pos->col++;
            if (data[i] == '\n') {  // new line
                pos->line++;
                pos->col = 0;
            }
            pos->offset = i;
            skiped = 1;
        }
        // skip one line comment
        if (i + 1 < len && data[i] == '/' && data[i + 1] == '/') {
            i += 2;
            while (i < len && data[i] != '\n') {
                i++;
            }
            pos->col = 0;
            pos->line++;
            pos->offset = i;
            skiped = 1;
        }
        // skip multiple line comments
        if (i + 1 < len && data[i] == '/' && data[i + 1] == '*') {
            i += 2;
            pos->col += 2;
            while (i + 1 < len && data[i] != '*' && data[i + 1] != '/') {
                i++;
                pos->col++;
                if (data[i] == '\n') {  // new line
                    pos->line++;
                    pos->col = 0;
                }
            }
            i += 2;
            pos->col += 2;
            pos->offset = i;
            skiped = 1;
        }
    } while (skiped);

    return 0;
}

/*
JSON_TOKEN_STRING = "([^"] | \")*"
JSON_TOKEN_INT = [0-9]+
JSON_TOKEN_FLOAT = [0-9]*\.[0-9]+
JSON_TOKEN_TRUE = true
JSON_TOKEN_FALSE = false
JSON_TOKEN_NULL = null
JSON_TOKEN_COMMA = ,
JSON_TOKEN_COLON = :
JSON_TOKEN_LEFT_BRACE = {
JSON_TOKEN_RIGHT_BRACE = }
JSON_TOKEN_LEFT_BRACKET = [
JSON_TOKEN_RIGHT_BRACKET = ]
*/
#include <stdio.h>
static int json_next_token_(sstr_t content, struct json_pos* pos, sstr_t txt) {
    long len = sstr_length(content);
    long i = pos->offset;
    char* data = SSTR_CSTR_(content);

    sstr_clear_fast_(txt);

    if (i >= len) {
        return JSON_TOKEN_EOF;
    }
    json_skip_space_comments(content, pos);
    i = pos->offset;
    if (i >= len) {
        return JSON_TOKEN_EOF;
    }

    int ch = data[i];
    switch (ch) {
        case '\"':
            return json_parse_string_token(content, pos, txt);
        case '[':
        case ']':
        case '{':
        case '}':
        case ':':
        case ',': {
            i++;
            pos->col++;
            pos->offset = i;
            return ch;
        }
        default:  // int, float, bool, null
            break;
    }
    // parse number
    int tk = JSON_TOKEN_INT;
    sstr_clear_fast_(txt);
    int start_pos = i;
    if (JSON_IS_DIGIT(ch) || ch == '-' || ch == '.') {
        if (ch != '.') {
            i++;
            pos->col++;
            while (i < len && JSON_IS_DIGIT(data[i])) {
                i++;
                pos->col++;
            }
        }
        if (i < len && data[i] == '.') {
            tk = JSON_TOKEN_FLOAT;
            i++;
            pos->col++;
            while (i < len && JSON_IS_DIGIT(data[i])) {
                i++;
                pos->col++;
            }
        }
        // Handle scientific notation (e/E followed by optional +/- and digits)
        if (i < len && (data[i] == 'e' || data[i] == 'E')) {
            tk = JSON_TOKEN_FLOAT;
            i++;
            pos->col++;
            if (i < len && (data[i] == '+' || data[i] == '-')) {
                i++;
                pos->col++;
            }
            while (i < len && JSON_IS_DIGIT(data[i])) {
                i++;
                pos->col++;
            }
        }
        sstr_append_of_fast_(txt, data + start_pos, i - start_pos);
        pos->offset = i;
        return tk;
    }
    // parse true, false, null — detect by first char + fixed length
    long keyword_start = i;
    if (ch == 't' && i + 4 <= len && memcmp(data + i, "true", 4) == 0 &&
        (i + 4 >= len || !isalpha((unsigned char)data[i + 4]))) {
        i += 4;
        pos->col += 4;
        pos->offset = i;
        sstr_append_of_fast_(txt, "true", 4);
        return JSON_TOKEN_TRUE;
    }
    if (ch == 'f' && i + 5 <= len && memcmp(data + i, "false", 5) == 0 &&
        (i + 5 >= len || !isalpha((unsigned char)data[i + 5]))) {
        i += 5;
        pos->col += 5;
        pos->offset = i;
        sstr_append_of_fast_(txt, "false", 5);
        return JSON_TOKEN_FALSE;
    }
    if (ch == 'n' && i + 4 <= len && memcmp(data + i, "null", 4) == 0 &&
        (i + 4 >= len || !isalpha((unsigned char)data[i + 4]))) {
        i += 4;
        pos->col += 4;
        pos->offset = i;
        sstr_append_of_fast_(txt, "null", 4);
        return JSON_TOKEN_NULL;
    }
    // Unknown identifier — scan and report error
    while (i < len && isalpha(data[i])) {
        i++;
        pos->col++;
    }
    sstr_append_of_fast_(txt, data + keyword_start, i - keyword_start);
    pos->offset = i;

    sstr_t e = PERROR(pos, "unexpected identify %s", SSTR_CSTR_(txt));
    sstr_append(txt, e);
    sstr_free(e);
    return JSON_ERROR;
}

// parse integer, set integer value to *val, put error string to txt.
// parse bool true to 1, and false to 0.
// ============================================================
// Signed integer unmarshal macro (handles all signed int types)
// ============================================================
#define DEFINE_UNMARSHAL_SCALAR_SIGNED(TYPE, CONV_TYPE, CONV_FN, MIN_VAL, MAX_VAL) \
static int json_unmarshal_scalar_##TYPE(sstr_t content, struct json_pos* pos,  \
                                        TYPE* val, sstr_t txt) {              \
    int tk = json_next_token(content, pos, txt);                               \
    if (tk == JSON_TOKEN_FALSE) {                                              \
        *val = 0;                                                              \
    } else if (tk == JSON_TOKEN_TRUE) {                                        \
        *val = 1;                                                              \
    } else if (tk != JSON_TOKEN_INT) {                                         \
        sstr_t e = PERROR(pos, "expected integer but got '%s'",                \
                          ptoken(tk, txt));                                     \
        sstr_append(txt, e);                                                   \
        sstr_free(e);                                                          \
        return tk;                                                             \
    } else {                                                                   \
        char* endptr;                                                          \
        CONV_TYPE temp_val = CONV_FN(SSTR_CSTR_(txt), &endptr, 10);           \
        if (*endptr != '\0' || temp_val > (MAX_VAL) || temp_val < (MIN_VAL)) { \
            sstr_t e = PERROR(pos, #TYPE " value out of range: '%s'",          \
                              SSTR_CSTR_(txt));                                \
            sstr_append(txt, e);                                               \
            sstr_free(e);                                                      \
            return JSON_ERROR;                                                 \
        }                                                                      \
        *val = (TYPE)temp_val;                                                 \
    }                                                                          \
    return 0;                                                                  \
}

DEFINE_UNMARSHAL_SCALAR_SIGNED(int, long, strtol, INT_MIN, INT_MAX)
DEFINE_UNMARSHAL_SCALAR_SIGNED(long, long, strtol, LONG_MIN, LONG_MAX)
DEFINE_UNMARSHAL_SCALAR_SIGNED(int8_t, long, strtol, INT8_MIN, INT8_MAX)
DEFINE_UNMARSHAL_SCALAR_SIGNED(int16_t, long, strtol, INT16_MIN, INT16_MAX)
DEFINE_UNMARSHAL_SCALAR_SIGNED(int32_t, long, strtol, INT32_MIN, INT32_MAX)
DEFINE_UNMARSHAL_SCALAR_SIGNED(int64_t, long long, strtoll, INT64_MIN, INT64_MAX)

// ============================================================
// Floating-point unmarshal macro
// ============================================================
#define DEFINE_UNMARSHAL_SCALAR_FLOAT(TYPE, CONV_FN)                           \
static int json_unmarshal_scalar_##TYPE(sstr_t content, struct json_pos* pos,  \
                                        TYPE* val, sstr_t txt) {              \
    int tk = json_next_token(content, pos, txt);                               \
    if (tk != JSON_TOKEN_FLOAT && tk != JSON_TOKEN_INT) {                      \
        sstr_t e = PERROR(pos, "expected floating number but got '%s'",        \
                          ptoken(tk, txt));                                     \
        sstr_append(txt, e);                                                   \
        sstr_free(e);                                                          \
        return tk;                                                             \
    } else {                                                                   \
        char* endptr;                                                          \
        TYPE temp_val = CONV_FN(SSTR_CSTR_(txt), &endptr);                    \
        if (*endptr != '\0') {                                                 \
            sstr_t e = PERROR(pos, #TYPE " format invalid: '%s'",              \
                              SSTR_CSTR_(txt));                                \
            sstr_append(txt, e);                                               \
            sstr_free(e);                                                      \
            return JSON_ERROR;                                                 \
        }                                                                      \
        *val = temp_val;                                                       \
    }                                                                          \
    return 0;                                                                  \
}

DEFINE_UNMARSHAL_SCALAR_FLOAT(float, strtof)
DEFINE_UNMARSHAL_SCALAR_FLOAT(double, strtod)

// parse string value, create an sstr_t instance and set it to *val, put error
// string to txt if error occur.
static int json_unmarshal_scalar_sstr_t(sstr_t content, struct json_pos* pos,
                                        sstr_t* val, sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk == JSON_TOKEN_NULL) {
        return 0;
    } else if (tk != JSON_TOKEN_STRING) {
        sstr_t e = PERROR(pos, "expected string but got '%s'", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return tk;
    } else {
        *val = sstr_dup(txt);
    }
    return 0;
}

// parse enum value: read a JSON string and look up the corresponding int index
// in the enum_strings array. Falls back to parsing as int if not a string.
static int json_unmarshal_scalar_enum(sstr_t content, struct json_pos* pos,
                                      int* val, const char** enum_strings,
                                      int enum_count, sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk == JSON_TOKEN_STRING) {
        const char* s = SSTR_CSTR_(txt);
        for (int i = 0; i < enum_count; i++) {
            if (strcmp(s, enum_strings[i]) == 0) {
                *val = i;
                return 0;
            }
        }
        sstr_t e = PERROR(pos, "unknown enum value '%s'", s);
        sstr_append(txt, e);
        sstr_free(e);
        return -1;
    } else if (tk == JSON_TOKEN_INT) {
        char* endptr;
        long temp_val = strtol(SSTR_CSTR_(txt), &endptr, 10);
        if (*endptr != '\0' || temp_val > INT_MAX || temp_val < INT_MIN) {
            sstr_t e = PERROR(pos, "enum integer value out of range: '%s'", SSTR_CSTR_(txt));
            sstr_append(txt, e);
            sstr_free(e);
            return JSON_ERROR;
        }
        *val = (int)temp_val;
        return 0;
    } else {
        sstr_t e = PERROR(pos, "expected string or integer for enum but got '%s'", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return tk;
    }
}

// ============================================================
// Unsigned integer unmarshal macro (handles all unsigned int types)
// ============================================================
#define DEFINE_UNMARSHAL_SCALAR_UNSIGNED(TYPE, CONV_TYPE, CONV_FN, MAX_VAL)     \
static int json_unmarshal_scalar_##TYPE(sstr_t content, struct json_pos* pos,  \
                                        TYPE* val, sstr_t txt) {              \
    int tk = json_next_token(content, pos, txt);                               \
    if (tk == JSON_TOKEN_FALSE) {                                              \
        *val = 0;                                                              \
    } else if (tk == JSON_TOKEN_TRUE) {                                        \
        *val = 1;                                                              \
    } else if (tk != JSON_TOKEN_INT) {                                         \
        sstr_t e = PERROR(pos, "expected integer but got '%s'",                \
                          ptoken(tk, txt));                                     \
        sstr_append(txt, e);                                                   \
        sstr_free(e);                                                          \
        return tk;                                                             \
    } else {                                                                   \
        const char* s = SSTR_CSTR_(txt);                                      \
        while (*s == ' ') s++;                                                 \
        if (*s == '-') {                                                       \
            sstr_t e = PERROR(pos, #TYPE " cannot be negative: '%s'",          \
                              SSTR_CSTR_(txt));                                \
            sstr_append(txt, e);                                               \
            sstr_free(e);                                                      \
            return JSON_ERROR;                                                 \
        }                                                                      \
        char* endptr;                                                          \
        CONV_TYPE temp_val = CONV_FN(SSTR_CSTR_(txt), &endptr, 10);           \
        if (*endptr != '\0' || temp_val > (MAX_VAL)) {                         \
            sstr_t e = PERROR(pos, #TYPE " value out of range: '%s'",          \
                              SSTR_CSTR_(txt));                                \
            sstr_append(txt, e);                                               \
            sstr_free(e);                                                      \
            return JSON_ERROR;                                                 \
        }                                                                      \
        *val = (TYPE)temp_val;                                                 \
    }                                                                          \
    return 0;                                                                  \
}

DEFINE_UNMARSHAL_SCALAR_UNSIGNED(uint8_t, unsigned long, strtoul, UINT8_MAX)
DEFINE_UNMARSHAL_SCALAR_UNSIGNED(uint16_t, unsigned long, strtoul, UINT16_MAX)
DEFINE_UNMARSHAL_SCALAR_UNSIGNED(uint32_t, unsigned long, strtoul, UINT32_MAX)
DEFINE_UNMARSHAL_SCALAR_UNSIGNED(uint64_t, unsigned long long, strtoull, UINT64_MAX)

// unmarshal array of enum values
static int json_unmarshal_array_internal_enum(sstr_t content,
                                              struct json_pos* pos,
                                              int** ptr, int* ptrlen,
                                              const char** enum_strings,
                                              int enum_count,
                                              sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk == JSON_TOKEN_NULL) {
        *ptr = NULL;
        *ptrlen = 0;
        return 0;
    }
    if (tk != JSON_TOKEN_LEFT_BRACKET) {
        sstr_t e = PERROR(pos, "expected '[' but got '%s'", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return -1;
    }
    int cap = 4;
    int len = 0;
    int* arr = (int*)JGENC_MALLOC(sizeof(int) * cap);
    if (arr == NULL) return -1;
    while (1) {
        // peek for ']' or value
        struct json_pos saved = *pos;
        sstr_t temp = sstr_new();
        tk = json_next_token(content, pos, temp);
        sstr_free(temp);
        if (tk == JSON_TOKEN_RIGHT_BRACKET) {
            break;
        }
        if (tk == JSON_TOKEN_COMMA) {
            continue;
        }
        // restore position to re-read the token in scalar_enum
        *pos = saved;
        if (len >= cap) {
            cap *= 2;
            int* new_arr = (int*)JGENC_REALLOC(arr, sizeof(int) * cap);
            if (new_arr == NULL) { JGENC_FREE(arr); return -1; }
            arr = new_arr;
        }
        int r = json_unmarshal_scalar_enum(content, pos, &arr[len], enum_strings, enum_count, txt);
        if (r != 0) {
            JGENC_FREE(arr);
            return r;
        }
        len++;
    }
    *ptr = arr;
    *ptrlen = len;
    return 0;
}

// ignore value, just skip it.
// call this function if parse a keyname that does not exists
// in field list of a struct.
static int json_unmarshal_ignore_value(sstr_t content, struct json_pos* pos,
                                       sstr_t txt) {
    int brace = 0;
    int bracket = 0;
    while (1) {
        int tk = json_next_token(content, pos, txt);
        if (tk == JSON_TOKEN_EOF) {
            sstr_t e = PERROR(pos, "unexpected EOF");
            sstr_append(txt, e);
            sstr_free(e);
            return -1;
        }
        if (tk == JSON_TOKEN_LEFT_BRACE) {
            brace++;
        } else if (tk == JSON_TOKEN_RIGHT_BRACE) {
            brace--;
        } else if (tk == JSON_TOKEN_LEFT_BRACKET) {
            bracket++;
        } else if (tk == JSON_TOKEN_RIGHT_BRACKET) {
            bracket--;
        }
        if (brace == 0 && bracket == 0) {
            break;
        }
    }
    return 0;
}

static int json_field_is_selected(const struct json_parse_param* param,
                                  const struct json_field_offset_item* fi) {
    int word_index;
    if (param->field_mask == NULL) {
        return 1;
    }
    if (fi->field_index < 0) {
        return 0;
    }
    word_index = fi->field_index / 64;
    if (word_index >= param->field_mask_word_count) {
        return 0;
    }
    return (param->field_mask[word_index] &
            (UINT64_C(1) << (fi->field_index % 64))) != 0;
}

static void json_clear_struct_value(void* instance_ptr, const char* struct_name);

static const struct json_nested_mask* json_find_nested_mask(
        const struct json_parse_param* param, int field_index) {
    int i;
    for (i = 0; i < param->nested_mask_count; i++) {
        if (param->nested_masks[i].field_index == field_index) {
            return &param->nested_masks[i];
        }
    }
    return NULL;
}

static struct json_field_offset_item* json_array_length_field(
    const struct json_field_offset_item* fi) {
    /* Build "field_name_len" on the stack to avoid sstr_t allocation. */
    size_t name_len = strlen(fi->field_name);
    size_t buf_len = name_len + 4; /* "_len" */
    char buf[256];
    if (buf_len >= sizeof(buf)) {
        /* Fallback for very long names — should never happen in practice. */
        sstr_t s = sstr(fi->field_name);
        sstr_append_cstr(s, "_len");
        struct json_field_offset_item* r =
            json_field_offset_item_find(fi->struct_name, sstr_cstr(s));
        sstr_free(s);
        return r;
    }
    memcpy(buf, fi->field_name, name_len);
    memcpy(buf + name_len, "_len", 5); /* includes '\0' */
    return json_field_offset_item_find(fi->struct_name, buf);
}

static void json_clear_map_value(void* value_ptr,
                                 const struct json_field_offset_item* fi) {
    switch (fi->map_value_type) {
        case FIELD_TYPE_SSTR:
            sstr_free(*(sstr_t*)value_ptr);
            *(sstr_t*)value_ptr = NULL;
            break;
        case FIELD_TYPE_STRUCT:
            json_clear_struct_value(value_ptr, fi->field_type_name);
            break;
        default:
            break;
    }
}

static void json_clear_map_container(void* map_ptr,
                                     const struct json_field_offset_item* fi) {
    char* entries = *(char**)map_ptr;
    int len = *(int*)((char*)map_ptr + sizeof(void*));
    int i;

    for (i = 0; i < len; i++) {
        char* entry = entries + (size_t)i * fi->map_entry_size;
        sstr_free(*(sstr_t*)entry);
        json_clear_map_value(entry + fi->map_value_offset, fi);
    }
    JGENC_FREE(entries);
    *(char**)map_ptr = NULL;
    *(int*)((char*)map_ptr + sizeof(void*)) = 0;
}

static void json_clear_oneof_value(void* value_ptr,
                                   const struct json_field_offset_item* fi) {
    int tag;

    if (fi->oneof_variant_structs != NULL && fi->enum_count > 0) {
        tag = *(int*)((char*)value_ptr + fi->oneof_tag_offset);
        if (tag >= 0 && tag < fi->enum_count) {
            json_clear_struct_value((char*)value_ptr + fi->oneof_value_offset,
                                    fi->oneof_variant_structs[tag]);
        }
    }
    memset(value_ptr, 0, (size_t)fi->type_size);
}

static void json_clear_field_value(void* instance_ptr,
                                   const struct json_field_offset_item* fi) {
    char* field_ptr = (char*)instance_ptr + fi->offset;
    int i;

    if (fi->field_type == FIELD_TYPE_MAP) {
        if (fi->is_array) {
            struct json_field_offset_item* len_fi = json_array_length_field(fi);
            int len = len_fi ? *(int*)((char*)instance_ptr + len_fi->offset) : 0;
            char* arr = *(char**)field_ptr;
            for (i = 0; i < len; i++) {
                json_clear_map_container(arr + (size_t)i * fi->type_size, fi);
            }
            JGENC_FREE(arr);
            *(char**)field_ptr = NULL;
            if (len_fi) {
                *(int*)((char*)instance_ptr + len_fi->offset) = 0;
            }
        } else {
            json_clear_map_container(field_ptr, fi);
        }
        if (fi->has_field_offset >= 0) {
            *(bool*)((char*)instance_ptr + fi->has_field_offset) = false;
        }
        return;
    }

    if (fi->is_array && fi->array_size > 0) {
        if (fi->field_type == FIELD_TYPE_SSTR) {
            for (i = 0; i < fi->array_size; i++) {
                sstr_free(((sstr_t*)field_ptr)[i]);
                ((sstr_t*)field_ptr)[i] = NULL;
            }
        } else if (fi->field_type == FIELD_TYPE_STRUCT) {
            for (i = 0; i < fi->array_size; i++) {
                json_clear_struct_value(field_ptr + (size_t)i * fi->type_size,
                                        fi->field_type_name);
            }
        } else if (fi->field_type == FIELD_TYPE_ONEOF) {
            for (i = 0; i < fi->array_size; i++) {
                json_clear_oneof_value(field_ptr + (size_t)i * fi->type_size, fi);
            }
        } else if (fi->field_type == FIELD_TYPE_FLOAT) {
            for (i = 0; i < fi->array_size; i++) {
                ((float*)field_ptr)[i] = 0.0f;
            }
        } else if (fi->field_type == FIELD_TYPE_DOUBLE) {
            for (i = 0; i < fi->array_size; i++) {
                ((double*)field_ptr)[i] = 0.0;
            }
        } else {
            memset(field_ptr, 0, (size_t)fi->type_size * (size_t)fi->array_size);
        }
        if (fi->has_field_offset >= 0) {
            *(bool*)((char*)instance_ptr + fi->has_field_offset) = false;
        }
        return;
    }

    if (fi->is_array) {
        struct json_field_offset_item* len_fi = json_array_length_field(fi);
        int len = len_fi ? *(int*)((char*)instance_ptr + len_fi->offset) : 0;
        char* arr = *(char**)field_ptr;

        if (fi->field_type == FIELD_TYPE_SSTR) {
            for (i = 0; i < len; i++) {
                sstr_free(((sstr_t*)arr)[i]);
            }
        } else if (fi->field_type == FIELD_TYPE_STRUCT) {
            for (i = 0; i < len; i++) {
                json_clear_struct_value(arr + (size_t)i * fi->type_size,
                                        fi->field_type_name);
            }
        } else if (fi->field_type == FIELD_TYPE_ONEOF) {
            for (i = 0; i < len; i++) {
                json_clear_oneof_value(arr + (size_t)i * fi->type_size, fi);
            }
        }

        JGENC_FREE(arr);
        *(char**)field_ptr = NULL;
        if (len_fi) {
            *(int*)((char*)instance_ptr + len_fi->offset) = 0;
        }
        if (fi->has_field_offset >= 0) {
            *(bool*)((char*)instance_ptr + fi->has_field_offset) = false;
        }
        return;
    }

    switch (fi->field_type) {
        case FIELD_TYPE_FLOAT:
            *(float*)field_ptr = 0.0f;
            break;
        case FIELD_TYPE_DOUBLE:
            *(double*)field_ptr = 0.0;
            break;
        case FIELD_TYPE_SSTR:
            sstr_free(*(sstr_t*)field_ptr);
            *(sstr_t*)field_ptr = NULL;
            break;
        case FIELD_TYPE_STRUCT:
            json_clear_struct_value(field_ptr, fi->field_type_name);
            break;
        case FIELD_TYPE_ONEOF:
            json_clear_oneof_value(field_ptr, fi);
            break;
        default:
            memset(field_ptr, 0, (size_t)fi->type_size);
            break;
    }
    if (fi->has_field_offset >= 0) {
        *(bool*)((char*)instance_ptr + fi->has_field_offset) = false;
    }
}

static void json_clear_struct_value(void* instance_ptr, const char* struct_name) {
    int i;
    for (i = 0; json_field_offset_item[i].field_name != NULL; i++) {
        struct json_field_offset_item* fi = &json_field_offset_item[i];
        if (fi->field_index < 0) {
            continue;
        }
        if (strcmp(fi->struct_name, struct_name) != 0) {
            continue;
        }
        json_clear_field_value(instance_ptr, fi);
    }
}

/**
 * @brief Scan a JSON object for a specific string-valued key without consuming
 *        the object.  Used by oneof (tagged union) unmarshal to discover the
 *        discriminator value before dispatching to the correct variant.
 *
 * On entry *pos must point just past the opening '{' of the object.
 * On return *pos is restored to that same location regardless of success or
 * failure.
 *
 * @param content   Full JSON input string.
 * @param pos       Current parse position (saved/restored).
 * @param tag_field The JSON key name to look for (e.g. "type").
 * @param out_value On success, receives the string value of the tag field.
 *                  Caller must free with sstr_free().
 * @param txt       Error / debug message buffer.
 * @return 0 on success, -1 on error (tag key not found or parse error).
 */
static int json_scan_tag_value(sstr_t content, struct json_pos* pos,
                               const char* tag_field, sstr_t out_value,
                               sstr_t txt) {
    struct json_pos saved = *pos;
    int found = 0;

    while (1) {
        sstr_t key_txt = sstr_new();
        int tk = json_next_token(content, pos, key_txt);
        if (tk == JSON_TOKEN_RIGHT_BRACE || tk == JSON_TOKEN_EOF ||
            tk == JSON_ERROR) {
            sstr_free(key_txt);
            break;
        }
        if (tk == JSON_TOKEN_COMMA) {
            sstr_free(key_txt);
            continue;
        }
        if (tk != JSON_TOKEN_STRING) {
            sstr_free(key_txt);
            break;
        }

        /* Consume colon. */
        sstr_t colon_txt = sstr_new();
        int colon_tk = json_next_token(content, pos, colon_txt);
        sstr_free(colon_txt);
        if (colon_tk != JSON_TOKEN_COLON) {
            sstr_free(key_txt);
            break;
        }

        if (strcmp(sstr_cstr(key_txt), tag_field) == 0) {
            /* Next token must be a string. */
            sstr_t val_txt = sstr_new();
            int val_tk = json_next_token(content, pos, val_txt);
            if (val_tk == JSON_TOKEN_STRING) {
                sstr_append(out_value, val_txt);
                found = 1;
            }
            sstr_free(val_txt);
            sstr_free(key_txt);
            break;
        }
        sstr_free(key_txt);
        /* Skip the value we don't care about. */
        json_unmarshal_ignore_value(content, pos, txt);
    }

    *pos = saved;
    if (!found) {
        sstr_t e = PERROR(pos, "oneof: tag field \"%s\" not found", tag_field);
        sstr_append(txt, e);
        sstr_free(e);
        return -1;
    }
    return 0;
}

/**
 * @brief Generic unmarshal for a oneof (tagged union).
 *
 * Two-pass algorithm:
 *   1. Save pos, read '{', scan for tag field, restore pos.
 *   2. Based on tag string, set the tag enum, then call
 *      json_unmarshal_struct_internal() targeting the variant union member.
 *      That function reads '{' again from the restored position and parses
 *      the whole object — the tag field is simply skipped as an unknown field
 *      by the variant struct's offset table.
 *
 * @param content              Full JSON input.
 * @param pos                  Current parse position (before the '{').
 * @param instance             Pointer to the oneof struct instance.
 * @param tag_field            JSON key name used as discriminator (e.g. "type").
 * @param variant_names        Array of tag string values (one per variant).
 * @param variant_struct_names Array of C struct names (one per variant).
 * @param variant_count        Number of variants.
 * @param tag_offset           offsetof(struct Oneof, tag).
 * @param value_offset         offsetof(struct Oneof, value).
 * @param txt                  Error buffer.
 * @return 0 on success, -1 on error.
 */
static int json_unmarshal_oneof_internal(sstr_t content, struct json_pos* pos,
                                         void* instance,
                                         const char* tag_field,
                                         const char** variant_names,
                                         const char** variant_struct_names,
                                         int variant_count,
                                         int tag_offset, int value_offset,
                                         int depth, sstr_t txt) {
    /* Save position before '{' for pass 2. */
    struct json_pos saved = *pos;

    /* Pass 1: consume '{', scan for tag, restore position. */
    sstr_t tmp = sstr_new();
    int tk = json_next_token(content, pos, tmp);
    sstr_free(tmp);
    if (tk == JSON_TOKEN_EOF) {
        return 0;
    }
    if (tk == JSON_ERROR) {
        return -1;
    }
    if (tk != JSON_TOKEN_LEFT_BRACE) {
        sstr_t e = PERROR(pos, "oneof: expected '{' but got token %d", tk);
        sstr_append(txt, e);
        sstr_free(e);
        return -1;
    }

    /* pos is now past '{'. Scan for the tag field. */
    sstr_t tag_value = sstr_new();
    int r = json_scan_tag_value(content, pos, tag_field, tag_value, txt);
    if (r < 0) {
        sstr_free(tag_value);
        return -1;
    }

    /* Match tag value to a variant. */
    int matched = -1;
    int i;
    for (i = 0; i < variant_count; i++) {
        if (strcmp(sstr_cstr(tag_value), variant_names[i]) == 0) {
            matched = i;
            break;
        }
    }
    if (matched < 0) {
        sstr_t e = PERROR(pos, "oneof: unknown tag value \"%s\"",
                          sstr_cstr(tag_value));
        sstr_append(txt, e);
        sstr_free(e);
        sstr_free(tag_value);
        return -1;
    }
    sstr_free(tag_value);

    /* Set the tag enum value. */
    *(int*)((char*)instance + tag_offset) = matched;

    /* Pass 2: restore to before '{' and unmarshal as the variant struct.
     * json_unmarshal_struct_internal will read '{' and parse all fields.
     * The tag field (e.g. "type") will be looked up in the variant struct's
     * offset table and not found, so it gets harmlessly skipped. */
    *pos = saved;

    struct json_parse_param sub;
    sub.instance_ptr = (char*)instance + value_offset;
    sub.in_array = 0;
    sub.in_struct = 1;
    sub.depth = depth + 1;
    sub.struct_name = variant_struct_names[matched];
    sub.field_name = "";
    sub.field_mask = NULL;
    sub.field_mask_word_count = 0;
    sub.nested_masks = NULL;
    sub.nested_mask_count = 0;
    r = json_unmarshal_struct_internal(content, pos, &sub, txt);
    if (r < 0) {
        return -1;
    }
    return 0;
}

/**
 * @brief Unmarshal a JSON array of oneof (tagged union) elements.
 *
 * Parses '[', then for each element calls json_unmarshal_oneof_internal.
 * Grows the array dynamically using JGENC_REALLOC.
 *
 * @param content              Full JSON input.
 * @param pos                  Current parse position (before '[').
 * @param arr_pp               Pointer to the array pointer (output).
 * @param ptrlen               Pointer to the array length (output).
 * @param element_size         sizeof(struct OneofType).
 * @param tag_field            JSON discriminator key name.
 * @param variant_names        Array of variant tag strings.
 * @param variant_struct_names Array of variant struct names.
 * @param variant_count        Number of variants.
 * @param tag_offset           offsetof(struct Oneof, tag).
 * @param value_offset         offsetof(struct Oneof, value).
 * @param txt                  Error buffer.
 * @return 0 on success, -1 on error.
 */
static int json_unmarshal_array_internal_oneof(
    sstr_t content, struct json_pos* pos,
    void** arr_pp, int* ptrlen, int element_size,
    const char* tag_field,
    const char** variant_names,
    const char** variant_struct_names,
    int variant_count,
    int tag_offset, int value_offset,
    int depth, sstr_t txt) {

    int tk = json_next_token(content, pos, txt);
    if (tk == JSON_TOKEN_NULL) {
        return 0;
    }
    if (tk != JSON_TOKEN_LEFT_BRACKET) {
        sstr_t e = PERROR(pos, "expected '[' but got %s", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return -1;
    }

    char* arr = NULL;
    int len = 0;
    int cap = 0;

    while (1) {
        /* Peek for ']' */
        struct json_pos peek = *pos;
        sstr_t peek_txt = sstr_new();
        int peek_tk = json_next_token(content, &peek, peek_txt);
        sstr_free(peek_txt);
        if (peek_tk == JSON_TOKEN_RIGHT_BRACKET) {
            *pos = peek;
            break;
        }

        /* Grow array if needed. */
        if (len >= cap) {
            cap = cap == 0 ? 4 : cap * 2;
            arr = (char*)JGENC_REALLOC(arr, (size_t)cap * element_size);
            if (!arr) return -1;
        }
        memset(arr + len * element_size, 0, (size_t)element_size);

        int r = json_unmarshal_oneof_internal(
            content, pos,
            arr + len * element_size,
            tag_field, variant_names, variant_struct_names,
            variant_count, tag_offset, value_offset, depth, txt);
        if (r < 0) {
            JGENC_FREE(arr);
            *arr_pp = NULL;
            *ptrlen = 0;
            return -1;
        }
        len++;

        /* Expect ',' or ']'. */
        peek = *pos;
        sstr_t sep_txt = sstr_new();
        peek_tk = json_next_token(content, &peek, sep_txt);
        sstr_free(sep_txt);
        if (peek_tk == JSON_TOKEN_RIGHT_BRACKET) {
            *pos = peek;
            break;
        }
        if (peek_tk == JSON_TOKEN_COMMA) {
            *pos = peek;
            continue;
        }
        if (peek_tk == JSON_ERROR || peek_tk == JSON_TOKEN_EOF) {
            JGENC_FREE(arr);
            *arr_pp = NULL;
            *ptrlen = 0;
            return -1;
        }
    }

    /* Shrink to fit. */
    if (len > 0 && len < cap) {
        arr = (char*)JGENC_REALLOC(arr, (size_t)len * element_size);
    }
    *arr_pp = arr;
    *ptrlen = len;
    return 0;
}

// ============================================================
// Array unmarshal macro (handles all scalar array types)
// ============================================================
#define DEFINE_UNMARSHAL_ARRAY_INTERNAL(TYPE)                                   \
static int json_unmarshal_array_internal_##TYPE(sstr_t content,                \
                                                struct json_pos* pos,          \
                                                TYPE** ptr, int* ptrlen,       \
                                                sstr_t txt) {                  \
    int tk = json_next_token(content, pos, txt);                               \
    if (tk != JSON_TOKEN_LEFT_BRACKET) {                                       \
        sstr_t e = PERROR(pos, "expected '[' but got %s", ptoken(tk, txt));    \
        sstr_append(txt, e);                                                   \
        sstr_free(e);                                                          \
        return -1;                                                             \
    }                                                                          \
    int cap_ = 0;                                                              \
    while (1) {                                                                \
        TYPE res = 0;                                                          \
        int r = json_unmarshal_scalar_##TYPE(content, pos, &res, txt);         \
        if (r == JSON_TOKEN_RIGHT_BRACKET) {                                   \
            return 0;                                                          \
        }                                                                      \
        if (r < 0) {                                                           \
            return r;                                                          \
        }                                                                      \
        if (*ptrlen >= cap_) {                                                 \
            cap_ = cap_ == 0 ? 4 : cap_ * 2;                                  \
            TYPE* np_ = (TYPE*)JGENC_REALLOC(*ptr, cap_ * sizeof(TYPE));       \
            if (!np_) return -1;                                               \
            *ptr = np_;                                                        \
        }                                                                      \
        (*ptr)[*ptrlen] = res;                                                 \
        *ptrlen = *ptrlen + 1;                                                 \
        int tk2 = json_next_token(content, pos, txt);                          \
        if (tk2 == JSON_TOKEN_RIGHT_BRACKET) {                                 \
            return 0;                                                          \
        }                                                                      \
        if (tk2 == JSON_TOKEN_COMMA) {                                         \
            continue;                                                          \
        }                                                                      \
        if (tk2 == JSON_ERROR) {                                               \
            return -1;                                                         \
        }                                                                      \
        if (tk2 == JSON_TOKEN_EOF) {                                           \
            sstr_t e = PERROR(pos, "parsing array, each EOF");                 \
            sstr_append(txt, e);                                               \
            sstr_free(e);                                                      \
            return -1;                                                         \
        }                                                                      \
    }                                                                          \
    return 0;                                                                  \
}

DEFINE_UNMARSHAL_ARRAY_INTERNAL(int)
DEFINE_UNMARSHAL_ARRAY_INTERNAL(long)
DEFINE_UNMARSHAL_ARRAY_INTERNAL(float)
DEFINE_UNMARSHAL_ARRAY_INTERNAL(double)

static int json_unmarshal_array_internal_sstr_t(sstr_t content,
                                                struct json_pos* pos,
                                                sstr_t** ptr, int* ptrlen,
                                                sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk != JSON_TOKEN_LEFT_BRACKET) {
        sstr_t e = PERROR(pos, "expected '[' but got %s", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return -1;
    }

    int cap_ = 0;
    while (1) {
        sstr_t res = NULL;
        int r = json_unmarshal_scalar_sstr_t(content, pos, &res, txt);
        if (r == JSON_TOKEN_RIGHT_BRACKET) {
            return 0;
        }
        if (r < 0) {
            return r;
        }
        if (*ptrlen >= cap_) {
            cap_ = cap_ == 0 ? 4 : cap_ * 2;
            sstr_t* np_ = (sstr_t*)JGENC_REALLOC(*ptr, cap_ * sizeof(sstr_t));
            if (!np_) return -1;
            *ptr = np_;
        }
        (*ptr)[*ptrlen] = res;
        *ptrlen = *ptrlen + 1;
        int tk = json_next_token(content, pos, txt);
        if (tk == JSON_TOKEN_RIGHT_BRACKET) {
            return 0;
        }
        if (tk == JSON_TOKEN_COMMA) {
            continue;
        }
        if (tk == JSON_ERROR) {
            return -1;
        }
        if (tk == JSON_TOKEN_EOF) {
            sstr_t e = PERROR(pos, "parsing array, each EOF");
            sstr_append(txt, e);
            sstr_free(e);
            return -1;
        }
    }
    return 0;
}

DEFINE_UNMARSHAL_ARRAY_INTERNAL(int8_t)
DEFINE_UNMARSHAL_ARRAY_INTERNAL(int16_t)
DEFINE_UNMARSHAL_ARRAY_INTERNAL(int32_t)
DEFINE_UNMARSHAL_ARRAY_INTERNAL(int64_t)
DEFINE_UNMARSHAL_ARRAY_INTERNAL(uint8_t)
DEFINE_UNMARSHAL_ARRAY_INTERNAL(uint16_t)
DEFINE_UNMARSHAL_ARRAY_INTERNAL(uint32_t)
DEFINE_UNMARSHAL_ARRAY_INTERNAL(uint64_t)

int json_unmarshal_array_int(sstr_t content, int** ptr, int* len) {
    struct json_pos pos;
    pos.line = 0;
    pos.col = 0;
    pos.offset = 0;
    sstr_t txt = sstr_new();
    int r = json_unmarshal_array_internal_int(content, &pos, ptr, len, txt);
    if (r != 0) {
#ifdef JSON_DEBUG
        printf("ERROR: %s\n", sstr_cstr(txt));
#endif
        JGENC_FREE(*ptr);
        *ptr = NULL;
        *len = 0;
    }
    sstr_free(txt);
    return r;
}

int json_unmarshal_array_long(sstr_t content, long** ptr, int* len) {
    struct json_pos pos;
    pos.line = 0;
    pos.col = 0;
    pos.offset = 0;
    sstr_t txt = sstr_new();
    int r = json_unmarshal_array_internal_long(content, &pos, ptr, len, txt);

    if (r != 0) {
#ifdef JSON_DEBUG
        printf("ERROR: %s\n", sstr_cstr(txt));
#endif
        JGENC_FREE(*ptr);
        *ptr = NULL;
        *len = 0;
    }

    sstr_free(txt);
    return r;
}

int json_unmarshal_array_float(sstr_t content, float** ptr, int* len) {
    struct json_pos pos;
    pos.line = 0;
    pos.col = 0;
    pos.offset = 0;
    sstr_t txt = sstr_new();
    int r = json_unmarshal_array_internal_float(content, &pos, ptr, len, txt);
    if (r != 0) {
#ifdef JSON_DEBUG
        printf("ERROR: %s\n", sstr_cstr(txt));
#endif
        JGENC_FREE(*ptr);
        *ptr = NULL;
        *len = 0;
    }
    sstr_free(txt);
    return r;
}

int json_unmarshal_array_double(sstr_t content, double** ptr, int* len) {
    struct json_pos pos;
    pos.line = 0;
    pos.col = 0;
    pos.offset = 0;
    sstr_t txt = sstr_new();
    int r = json_unmarshal_array_internal_double(content, &pos, ptr, len, txt);
    if (r != 0) {
#ifdef JSON_DEBUG
        printf("ERROR: %s\n", sstr_cstr(txt));
#endif
        JGENC_FREE(*ptr);
        *ptr = NULL;
        *len = 0;
    }
    sstr_free(txt);
    return r;
}

int json_unmarshal_array_sstr_t(sstr_t content, sstr_t** ptr, int* len) {
    struct json_pos pos;
    pos.line = 0;
    pos.col = 0;
    pos.offset = 0;
    sstr_t txt = sstr_new();
    int r = json_unmarshal_array_internal_sstr_t(content, &pos, ptr, len, txt);
    if (r != 0) {
#ifdef JSON_DEBUG
        printf("ERROR: %s\n", sstr_cstr(txt));
#endif
        int i;
        for (i = 0; i < *len; ++i) {
            sstr_free((*ptr)[i]);
        }
        JGENC_FREE(*ptr);
        *ptr = NULL;
        *len = 0;
    }
    sstr_free(txt);
    return r;
}

#define DEFINE_UNMARSHAL_ARRAY_PUBLIC(TYPE)                                     \
int json_unmarshal_array_##TYPE(sstr_t content, TYPE** ptr, int* len) {        \
    struct json_pos pos;                                                        \
    pos.line = 0;                                                              \
    pos.col = 0;                                                               \
    pos.offset = 0;                                                            \
    sstr_t txt = sstr_new();                                                   \
    int r = json_unmarshal_array_internal_##TYPE(content, &pos, ptr, len,      \
                                                  txt);                        \
    if (r != 0) {                                                              \
        JGENC_FREE(*ptr);                                                            \
        *ptr = NULL;                                                           \
        *len = 0;                                                              \
    }                                                                          \
    sstr_free(txt);                                                            \
    return r;                                                                  \
}

DEFINE_UNMARSHAL_ARRAY_PUBLIC(int8_t)
DEFINE_UNMARSHAL_ARRAY_PUBLIC(int16_t)
DEFINE_UNMARSHAL_ARRAY_PUBLIC(int32_t)
DEFINE_UNMARSHAL_ARRAY_PUBLIC(int64_t)
DEFINE_UNMARSHAL_ARRAY_PUBLIC(uint8_t)
DEFINE_UNMARSHAL_ARRAY_PUBLIC(uint16_t)
DEFINE_UNMARSHAL_ARRAY_PUBLIC(uint32_t)
DEFINE_UNMARSHAL_ARRAY_PUBLIC(uint64_t)

static int json_unmarshal_struct_internal(sstr_t content, struct json_pos* pos,
                                          struct json_parse_param* param,
                                          sstr_t txt);

/**
 * @brief Parse JSON array with proper memory management
 * @param content JSON content string
 * @param pos Current parsing position
 * @param param Parse parameters
 * @param len Output array length
 * @param txt Error message buffer
 * @return 0 on success, negative on error
 */
static int json_unmarshal_array_internal(sstr_t content, struct json_pos* pos,
                                         struct json_parse_param* param,
                                         int* len, sstr_t txt) {
    *len = 0;
    struct json_field_offset_item* field =
        json_field_offset_item_find(param->struct_name, "");
    if (field == NULL) {
        sstr_t e = PERROR(pos, "struct %s not found", param->struct_name);
        sstr_append(txt, e);
        sstr_free(e);
        return JSON_GEN_ERROR_NOT_FOUND;
    }
#ifdef JSON_DEBUG
    printf("array find field struct %s, size %d\n", field->struct_name,
           field->type_size);
#endif

    int tk = json_next_token(content, pos, txt);
    if (tk != JSON_TOKEN_LEFT_BRACKET) {
        sstr_t e = PERROR(pos, "expected '[' but got %s", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return JSON_GEN_ERROR_PARSE;
    }

    // Check for empty array immediately after '['
    // We need to peek ahead to see if the next token is ']'
    struct json_pos peek_pos = *pos;  // Save current position
    tk = json_next_token(content, &peek_pos, txt);
    if (tk == JSON_TOKEN_RIGHT_BRACKET) {
        // Empty array case - update position and return
        *pos = peek_pos;
        return JSON_GEN_SUCCESS;
    }
    // If not empty, continue with normal parsing using original position

    int cap_ = 0;
    while (1) {
        void* ptr = JGENC_MALLOC(field->type_size);
        if (ptr == NULL) {
            sstr_t e = PERROR(pos, "memory allocation failed for array element");
            sstr_append(txt, e);
            sstr_free(e);
            return JSON_GEN_ERROR_MEMORY;
        }
        
        memset(ptr, 0, field->type_size);
        struct json_parse_param sub_param;
        sub_param.instance_ptr = ptr;
        sub_param.in_array = 1;
        sub_param.in_struct = 0;
        sub_param.depth = param->depth + 1;
        sub_param.struct_name = param->struct_name;
        sub_param.field_name = param->field_name;
        sub_param.field_mask = NULL;
        sub_param.field_mask_word_count = 0;
        sub_param.nested_masks = NULL;
        sub_param.nested_mask_count = 0;

        int r = json_unmarshal_struct_internal(content, pos, &sub_param, txt);
        
        // Handle parsing failure - free allocated memory before returning
        if (r < 0) {
            JGENC_FREE(ptr);
            return r;
        }
        
        // Grow array buffer with capacity doubling
        if (*len >= cap_) {
            cap_ = cap_ == 0 ? 4 : cap_ * 2;
            void* pptr = JGENC_REALLOC(*(void**)param->instance_ptr,
                                 cap_ * field->type_size);
            if (pptr == NULL) {
                JGENC_FREE(ptr);
                sstr_t e = PERROR(pos, "memory reallocation failed for array");
                sstr_append(txt, e);
                sstr_free(e);
                return JSON_GEN_ERROR_MEMORY;
            }
            *(void**)param->instance_ptr = pptr;
        }
        
        // Copy element to array and update pointer
        void* arr_base = *(void**)param->instance_ptr;
        memcpy(arr_base + (*len * field->type_size), ptr, field->type_size);
        JGENC_FREE(ptr);
        *len = *len + 1;

        if (r == 1) {
            return JSON_GEN_SUCCESS;  // finished
        }

        int tk = json_next_token(content, pos, txt);
        if (tk == JSON_TOKEN_RIGHT_BRACKET) {
            return JSON_GEN_SUCCESS;
        }
        if (tk == JSON_TOKEN_COMMA) {
            continue;
        }
        if (tk == JSON_ERROR) {
            return JSON_GEN_ERROR_PARSE;
        }
        if (tk == JSON_TOKEN_EOF) {
            sstr_t e = PERROR(pos, "parsing array, reached EOF unexpectedly");
            sstr_append(txt, e);
            sstr_free(e);
            return JSON_GEN_ERROR_PARSE;
        }
    }
    return JSON_GEN_SUCCESS;
}

// Unmarshal a single JSON object into a map container.
// map_ptr points to the beginning of {entries_ptr, len} pair.
// entry_size is sizeof(json_map_entry_<type>).
// value_type is the FIELD_TYPE_* constant for the value.
static int json_unmarshal_map_object(sstr_t content, struct json_pos* pos,
                                     void* map_ptr,
                                     int entry_size, int value_type,
                                     const char* value_type_name,
                                     const char** enum_strings, int enum_count,
                                     int depth, sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk == JSON_TOKEN_NULL) {
        return 0;
    }
    if (tk != JSON_TOKEN_LEFT_BRACE) {
        sstr_t e = PERROR(pos, "expected '{' for map but got '%s'",
                          ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return -1;
    }

    char** entries_pp = (char**)map_ptr;
    int* len_p = (int*)((char*)map_ptr + sizeof(void*));
    int cap = *len_p;
    char* entries = *entries_pp;

    while (1) {
        tk = json_next_token(content, pos, txt);
        if (tk == JSON_TOKEN_RIGHT_BRACE) {
            break;
        }
        if (tk == JSON_TOKEN_COMMA) {
            continue;
        }
        if (tk == JSON_TOKEN_EOF || tk == JSON_ERROR) {
            return -1;
        }
        if (tk != JSON_TOKEN_STRING) {
            sstr_t e = PERROR(pos, "expected string key in map but got '%s'",
                              ptoken(tk, txt));
            sstr_append(txt, e);
            sstr_free(e);
            return -1;
        }

        // Save the key
        sstr_t key = sstr_dup(txt);

        // Expect colon
        tk = json_next_token(content, pos, txt);
        if (tk != JSON_TOKEN_COLON) {
            sstr_t e = PERROR(pos, "expected ':' in map but got '%s'",
                              ptoken(tk, txt));
            sstr_append(txt, e);
            sstr_free(e);
            sstr_free(key);
            return -1;
        }

        // Grow entries array
        int idx = *len_p;
        if (idx >= cap) {
            cap = cap == 0 ? 4 : cap * 2;
            entries = (char*)JGENC_REALLOC(entries, (size_t)cap * entry_size);
            if (!entries) {
                sstr_free(key);
                return -1;
            }
            *entries_pp = entries;
        }

        char* entry = entries + (size_t)idx * entry_size;
        // Store key at offset 0 of entry
        *(sstr_t*)entry = key;
        // Value starts after the key (after sstr_t = sizeof(void*))
        void* val_ptr = entry + sizeof(sstr_t);

        int r = 0;
        switch (value_type) {
            case FIELD_TYPE_INT:
            case FIELD_TYPE_BOOL:
                r = json_unmarshal_scalar_int(content, pos, (int*)val_ptr, txt);
                break;
            case FIELD_TYPE_LONG:
                r = json_unmarshal_scalar_long(content, pos, (long*)val_ptr,
                                               txt);
                break;
            case FIELD_TYPE_INT8:
                r = json_unmarshal_scalar_int8_t(content, pos, (int8_t*)val_ptr, txt);
                break;
            case FIELD_TYPE_INT16:
                r = json_unmarshal_scalar_int16_t(content, pos, (int16_t*)val_ptr, txt);
                break;
            case FIELD_TYPE_INT32:
                r = json_unmarshal_scalar_int32_t(content, pos, (int32_t*)val_ptr, txt);
                break;
            case FIELD_TYPE_INT64:
                r = json_unmarshal_scalar_int64_t(content, pos, (int64_t*)val_ptr, txt);
                break;
            case FIELD_TYPE_UINT8:
                r = json_unmarshal_scalar_uint8_t(content, pos, (uint8_t*)val_ptr, txt);
                break;
            case FIELD_TYPE_UINT16:
                r = json_unmarshal_scalar_uint16_t(content, pos, (uint16_t*)val_ptr, txt);
                break;
            case FIELD_TYPE_UINT32:
                r = json_unmarshal_scalar_uint32_t(content, pos, (uint32_t*)val_ptr, txt);
                break;
            case FIELD_TYPE_UINT64:
                r = json_unmarshal_scalar_uint64_t(content, pos, (uint64_t*)val_ptr, txt);
                break;
            case FIELD_TYPE_FLOAT:
                r = json_unmarshal_scalar_float(content, pos, (float*)val_ptr,
                                                txt);
                break;
            case FIELD_TYPE_DOUBLE:
                r = json_unmarshal_scalar_double(content, pos, (double*)val_ptr,
                                                 txt);
                break;
            case FIELD_TYPE_SSTR: {
                sstr_t s = NULL;
                r = json_unmarshal_scalar_sstr_t(content, pos, &s, txt);
                *(sstr_t*)val_ptr = s;
                break;
            }
            case FIELD_TYPE_ENUM:
                r = json_unmarshal_scalar_enum(content, pos, (int*)val_ptr,
                                               enum_strings, enum_count, txt);
                break;
            case FIELD_TYPE_STRUCT: {
                struct json_parse_param sub;
                sub.instance_ptr = val_ptr;
                sub.in_array = 0;
                sub.in_struct = 1;
                sub.depth = depth + 1;
                sub.struct_name = value_type_name;
                sub.field_name = "";
                sub.field_mask = NULL;
                sub.field_mask_word_count = 0;
                sub.nested_masks = NULL;
                sub.nested_mask_count = 0;
                r = json_unmarshal_struct_internal(content, pos, &sub, txt);
                break;
            }
            default:
                r = -1;
                break;
        }
        if (r < 0) {
            return r;
        }
        (*len_p)++;
    }

    // Shrink to fit
    if (*len_p > 0 && *len_p < cap) {
        entries = (char*)JGENC_REALLOC(entries, (size_t)(*len_p) * entry_size);
        if (entries) {
            *entries_pp = entries;
        }
    }
    return 0;
}

static int json_unmarshal_struct_internal(sstr_t content, struct json_pos* pos,
                                          struct json_parse_param* param,
                                          sstr_t txt) {
    // Check recursion depth
    if (param->depth > JSON_MAX_DEPTH) {
        sstr_t e = PERROR(pos, "maximum JSON nesting depth (%d) exceeded", JSON_MAX_DEPTH);
        sstr_append(txt, e);
        sstr_free(e);
        return -1;
    }

    // '{'
    int tk = json_next_token(content, pos, txt);
    if (tk == JSON_TOKEN_EOF) {
        if (!param->in_array) {
            sstr_t e = PERROR(pos, "expected '{' but got empty input");
            sstr_append(txt, e);
            sstr_free(e);
            return -1;
        }
        return 0;
    }
    if (tk == JSON_ERROR) {
        return -1;
    }
    if (param->in_array && tk == JSON_TOKEN_RIGHT_BRACKET) {
        return 1;
    }

    if (tk != JSON_TOKEN_LEFT_BRACE) {
        sstr_t e = PERROR(pos, "expected '{' but got '%s'", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return -1;
    }

    // Pre-compute hash prefix for struct_name (avoids re-hashing per field)
    unsigned int st_hash_ = hash_s(param->struct_name, strlen(param->struct_name),
                                   0xbc9f1d34);

    // fields
    while (1) {
        tk = json_next_token(content, pos, txt);
        if (tk == JSON_ERROR) {
            return -1;
        }
        if (tk == JSON_TOKEN_EOF) {
            sstr_t e = PERROR(pos, "expected '}' but reach end of file");
            sstr_append(txt, e);
            sstr_free(e);
            return -1;
        }
        if (tk == JSON_TOKEN_RIGHT_BRACE) {
            break;
        }
        if (tk == JSON_TOKEN_COMMA) {
            // Peek ahead for trailing comma — skip whitespace/comments,
            // check raw char instead of allocating sstr_t + full token parse.
            struct json_pos peek = *pos;
            json_skip_space_comments(content, &peek);
            if (peek.offset < (long)sstr_length(content) &&
                SSTR_CSTR_(content)[peek.offset] == '}') {
                sstr_t e = PERROR(pos, "trailing comma not allowed before '}'");
                sstr_append(txt, e);
                sstr_free(e);
                return -1;
            }
            continue;
        }

        // field_name
        if (tk != JSON_TOKEN_STRING) {
            sstr_t e = PERROR(pos, "expected field_name string but got '%s'",
                              ptoken(tk, txt));
            sstr_append(txt, e);
            sstr_free(e);
            return -1;
        }

        struct json_field_offset_item* fi =
            json_field_offset_item_find_ph(st_hash_, param->struct_name,
                                           SSTR_CSTR_(txt), sstr_length(txt));
        if (fi == NULL) {
#if JSON_DEBUG
            printf("json_field_offset_item_find NULL, ignoring...\n");
#endif
            // Consume the expected colon before skipping the value.
            tk = json_next_token(content, pos, txt);
            if (tk != JSON_TOKEN_COLON) {
                sstr_t e =
                    PERROR(pos, "expected ':' but got '%s'", ptoken(tk, txt));
                sstr_append(txt, e);
                sstr_free(e);
                return -1;
            }
            if (json_unmarshal_ignore_value(content, pos, txt) != 0) {
                return -1;
            }
            continue;
        }
#if JSON_DEBUG
        printf("field found: %s->%s %s is_array: %d\n", (fi->struct_name),
               (fi->field_name), fi->field_type_name, fi->is_array);
#endif
        tk = json_next_token(content, pos, txt);
        if (tk != JSON_TOKEN_COLON) {
            sstr_t e =
                PERROR(pos, "expected ':' but got '%s'", ptoken(tk, txt));
            sstr_append(txt, e);
            sstr_free(e);
            return -1;
        }
        if (!json_field_is_selected(param, fi)) {
            if (json_unmarshal_ignore_value(content, pos, txt) != 0) {
                return -1;
            }
            continue;
        }
        if (param->field_mask != NULL) {
            json_clear_field_value(param->instance_ptr, fi);
        }

        // Handle nullable fields: accept JSON null
        if (fi->is_nullable) {
            struct json_pos peek = *pos;
            sstr_t peek_txt = sstr_new();
            int peek_tk = json_next_token(content, &peek, peek_txt);
            sstr_free(peek_txt);
            if (peek_tk == JSON_TOKEN_NULL) {
                *pos = peek;
                // has_field remains false from init
                continue;
            }
        }

        if (fi->field_type == FIELD_TYPE_MAP) {
            if (fi->is_array) {
                // array of maps: "field": [{...}, {...}, ...]
                sstr_t field_len_name = sstr(fi->field_name);
                sstr_append_cstr(field_len_name, "_len");
                struct json_field_offset_item* len_fi =
                    json_field_offset_item_find(param->struct_name,
                                                sstr_cstr(field_len_name));
                sstr_free(field_len_name);
                if (len_fi == NULL) {
                    return -1;
                }

                int tk2 = json_next_token(content, pos, txt);
                if (tk2 == JSON_TOKEN_NULL) {
                    continue;
                }
                if (tk2 != JSON_TOKEN_LEFT_BRACKET) {
                    sstr_t e = PERROR(pos, "expected '[' for map array but got '%s'",
                                      ptoken(tk2, txt));
                    sstr_append(txt, e);
                    sstr_free(e);
                    return -1;
                }

                // pointer to the map array pointer
                char** arr_pp = (char**)((char*)param->instance_ptr + fi->offset);
                char* arr = *arr_pp;
                int arr_len = 0;
                int arr_cap = 0;

                while (1) {
                    // peek for ] or ,
                    struct json_pos peek = *pos;
                    sstr_t peek_txt = sstr_new();
                    tk2 = json_next_token(content, &peek, peek_txt);
                    sstr_free(peek_txt);
                    if (tk2 == JSON_TOKEN_RIGHT_BRACKET) {
                        *pos = peek;
                        break;
                    }

                    // grow array
                    if (arr_len >= arr_cap) {
                        arr_cap = arr_cap == 0 ? 4 : arr_cap * 2;
                        arr = (char*)JGENC_REALLOC(arr, (size_t)arr_cap * fi->type_size);
                        if (!arr) return -1;
                        *arr_pp = arr;
                    }

                    // Init the new map container: entries=NULL, len=0
                    char* map_ptr = arr + (size_t)arr_len * fi->type_size;
                    *(void**)map_ptr = NULL;
                    *(int*)(map_ptr + sizeof(void*)) = 0;

                    int r = json_unmarshal_map_object(
                        content, pos, map_ptr,
                        fi->map_entry_size, fi->map_value_type,
                        fi->field_type_name,
                        fi->enum_strings, fi->enum_count,
                        param->depth + 1, txt);
                    if (r < 0) return r;
                    arr_len++;

                    tk2 = json_next_token(content, pos, txt);
                    if (tk2 == JSON_TOKEN_RIGHT_BRACKET) {
                        break;
                    }
                    if (tk2 == JSON_TOKEN_COMMA) {
                        continue;
                    }
                    return -1;
                }

                // Shrink to fit
                if (arr_len > 0 && arr_len < arr_cap) {
                    arr = (char*)JGENC_REALLOC(arr, (size_t)arr_len * fi->type_size);
                    if (arr) *arr_pp = arr;
                }
                *(int*)((char*)param->instance_ptr + len_fi->offset) = arr_len;
            } else {
                // scalar map: "field": {...}
                void* map_ptr = (char*)param->instance_ptr + fi->offset;
                int r = json_unmarshal_map_object(
                    content, pos, map_ptr,
                    fi->map_entry_size, fi->map_value_type,
                    fi->field_type_name,
                    fi->enum_strings, fi->enum_count,
                    param->depth + 1, txt);
                if (r < 0) return r;
            }
            if (fi->has_field_offset >= 0) {
                *(bool*)((char*)param->instance_ptr + fi->has_field_offset) = true;
            }
            continue;
        }

        if (fi->is_array && fi->array_size > 0) {
            // fixed-size array: parse directly into inline buffer
            int count = 0;
            int max_size = fi->array_size;
            void* base = (char*)param->instance_ptr + fi->offset;

            int tk2 = json_next_token(content, pos, txt);
            if (tk2 != JSON_TOKEN_LEFT_BRACKET) {
                sstr_t e = PERROR(pos, "expected '[' but got '%s'",
                                  ptoken(tk2, txt));
                sstr_append(txt, e);
                sstr_free(e);
                return -1;
            }

            // peek for empty array
            struct json_pos peek = *pos;
            sstr_t peek_txt = sstr_new();
            tk2 = json_next_token(content, &peek, peek_txt);
            sstr_free(peek_txt);
            if (tk2 == JSON_TOKEN_RIGHT_BRACKET) {
                *pos = peek;
                continue;
            }

            while (1) {
                if (count >= max_size) {
                    sstr_t e = PERROR(pos, "fixed-size array overflow: "
                                      "max %d elements for field '%s'",
                                      max_size, fi->field_name);
                    sstr_append(txt, e);
                    sstr_free(e);
                    return -1;
                }
                int r;
                switch (fi->field_type) {
                    case FIELD_TYPE_INT:
                    case FIELD_TYPE_BOOL:
                        r = json_unmarshal_scalar_int(content, pos,
                            &((int*)base)[count], txt);
                        break;
                    case FIELD_TYPE_LONG:
                        r = json_unmarshal_scalar_long(content, pos,
                            &((long*)base)[count], txt);
                        break;
                    case FIELD_TYPE_INT8:
                        r = json_unmarshal_scalar_int8_t(content, pos,
                            &((int8_t*)base)[count], txt);
                        break;
                    case FIELD_TYPE_INT16:
                        r = json_unmarshal_scalar_int16_t(content, pos,
                            &((int16_t*)base)[count], txt);
                        break;
                    case FIELD_TYPE_INT32:
                        r = json_unmarshal_scalar_int32_t(content, pos,
                            &((int32_t*)base)[count], txt);
                        break;
                    case FIELD_TYPE_INT64:
                        r = json_unmarshal_scalar_int64_t(content, pos,
                            &((int64_t*)base)[count], txt);
                        break;
                    case FIELD_TYPE_UINT8:
                        r = json_unmarshal_scalar_uint8_t(content, pos,
                            &((uint8_t*)base)[count], txt);
                        break;
                    case FIELD_TYPE_UINT16:
                        r = json_unmarshal_scalar_uint16_t(content, pos,
                            &((uint16_t*)base)[count], txt);
                        break;
                    case FIELD_TYPE_UINT32:
                        r = json_unmarshal_scalar_uint32_t(content, pos,
                            &((uint32_t*)base)[count], txt);
                        break;
                    case FIELD_TYPE_UINT64:
                        r = json_unmarshal_scalar_uint64_t(content, pos,
                            &((uint64_t*)base)[count], txt);
                        break;
                    case FIELD_TYPE_FLOAT:
                        r = json_unmarshal_scalar_float(content, pos,
                            &((float*)base)[count], txt);
                        break;
                    case FIELD_TYPE_DOUBLE:
                        r = json_unmarshal_scalar_double(content, pos,
                            &((double*)base)[count], txt);
                        break;
                    case FIELD_TYPE_SSTR: {
                        sstr_t s = NULL;
                        r = json_unmarshal_scalar_sstr_t(content, pos, &s, txt);
                        ((sstr_t*)base)[count] = s;
                        break;
                    }
                    case FIELD_TYPE_ENUM:
                        r = json_unmarshal_scalar_enum(content, pos,
                            &((int*)base)[count],
                            fi->enum_strings, fi->enum_count, txt);
                        break;
                    case FIELD_TYPE_STRUCT: {
                        struct json_parse_param sub;
                        sub.instance_ptr =
                            (char*)base + count * fi->type_size;
                        sub.in_array = 1;
                        sub.in_struct = 0;
                        sub.depth = param->depth + 1;
                        sub.struct_name = fi->field_type_name;
                        sub.field_name = fi->field_name;
                        sub.field_mask = NULL;
                        sub.field_mask_word_count = 0;
                        sub.nested_masks = NULL;
                        sub.nested_mask_count = 0;
                        r = json_unmarshal_struct_internal(content, pos,
                                                           &sub, txt);
                        break;
                    }
                    case FIELD_TYPE_ONEOF:
                        r = json_unmarshal_oneof_internal(
                            content, pos,
                            (char*)base + count * fi->type_size,
                            fi->oneof_tag_field,
                            fi->enum_strings,
                            fi->oneof_variant_structs,
                            fi->enum_count,
                            fi->oneof_tag_offset,
                            fi->oneof_value_offset,
                            param->depth + 1, txt);
                        break;
                    default:
                        r = -1;
                        break;
                }
                if (r == JSON_TOKEN_RIGHT_BRACKET) {
                    break;
                }
                if (r < 0) {
                    return r;
                }
                count++;

                tk2 = json_next_token(content, pos, txt);
                if (tk2 == JSON_TOKEN_RIGHT_BRACKET) {
                    break;
                }
                if (tk2 == JSON_TOKEN_COMMA) {
                    continue;
                }
                if (tk2 == JSON_ERROR || tk2 == JSON_TOKEN_EOF) {
                    return -1;
                }
            }
            if (fi->has_field_offset >= 0) {
                *(bool*)((char*)param->instance_ptr + fi->has_field_offset) = true;
            }
            continue;
        }

        if (fi->is_array) {
            sstr_t field_len_name = sstr(fi->field_name);
            sstr_append_cstr(field_len_name, "_len");
            struct json_field_offset_item* len_fi = json_field_offset_item_find(
                param->struct_name, sstr_cstr(field_len_name));
            sstr_free(field_len_name);
            if (len_fi == NULL) {
                sstr_t e = PERROR(pos, "field %s not found",
                                  sstr_cstr(field_len_name));
                sstr_append(txt, e);
                sstr_free(e);
                return -1;
            }
            int len = 0;

            switch (fi->field_type) {
                case FIELD_TYPE_STRUCT: {
                    struct json_parse_param ar_param;
                    ar_param.instance_ptr = fi->offset + param->instance_ptr;
                    ar_param.in_array = 1;
                    ar_param.in_struct = 0;
                    ar_param.depth = param->depth;
                    ar_param.struct_name = fi->field_type_name;
                    ar_param.field_name = fi->field_name;
                    ar_param.field_mask = NULL;
                    ar_param.field_mask_word_count = 0;
                    ar_param.nested_masks = NULL;
                    ar_param.nested_mask_count = 0;
                    int r = json_unmarshal_array_internal(content, pos,
                                                          &ar_param, &len, txt);
                    if (r < 0) {
                        return r;
                    }

                    break;
                }
                case FIELD_TYPE_INT:
                case FIELD_TYPE_BOOL:
                    json_unmarshal_array_internal_int(
                        content, pos, fi->offset + param->instance_ptr, &len,
                        txt);
                    break;
                case FIELD_TYPE_LONG:
                    json_unmarshal_array_internal_long(
                        content, pos, fi->offset + param->instance_ptr, &len,
                        txt);
                    break;
                case FIELD_TYPE_INT8:
                    json_unmarshal_array_internal_int8_t(
                        content, pos, fi->offset + param->instance_ptr, &len,
                        txt);
                    break;
                case FIELD_TYPE_INT16:
                    json_unmarshal_array_internal_int16_t(
                        content, pos, fi->offset + param->instance_ptr, &len,
                        txt);
                    break;
                case FIELD_TYPE_INT32:
                    json_unmarshal_array_internal_int32_t(
                        content, pos, fi->offset + param->instance_ptr, &len,
                        txt);
                    break;
                case FIELD_TYPE_INT64:
                    json_unmarshal_array_internal_int64_t(
                        content, pos, fi->offset + param->instance_ptr, &len,
                        txt);
                    break;
                case FIELD_TYPE_UINT8:
                    json_unmarshal_array_internal_uint8_t(
                        content, pos, fi->offset + param->instance_ptr, &len,
                        txt);
                    break;
                case FIELD_TYPE_UINT16:
                    json_unmarshal_array_internal_uint16_t(
                        content, pos, fi->offset + param->instance_ptr, &len,
                        txt);
                    break;
                case FIELD_TYPE_UINT32:
                    json_unmarshal_array_internal_uint32_t(
                        content, pos, fi->offset + param->instance_ptr, &len,
                        txt);
                    break;
                case FIELD_TYPE_UINT64:
                    json_unmarshal_array_internal_uint64_t(
                        content, pos, fi->offset + param->instance_ptr, &len,
                        txt);
                    break;
                case FIELD_TYPE_FLOAT:
                    json_unmarshal_array_internal_float(
                        content, pos, fi->offset + param->instance_ptr, &len,
                        txt);
                    break;
                case FIELD_TYPE_DOUBLE:
                    json_unmarshal_array_internal_double(
                        content, pos, fi->offset + param->instance_ptr, &len,
                        txt);
                    break;
                case FIELD_TYPE_SSTR:
                    json_unmarshal_array_internal_sstr_t(
                        content, pos, fi->offset + param->instance_ptr, &len,
                        txt);
                    break;
                case FIELD_TYPE_ENUM:
                    json_unmarshal_array_internal_enum(
                        content, pos,
                        (int**)(fi->offset + param->instance_ptr), &len,
                        fi->enum_strings, fi->enum_count, txt);
                    break;
                case FIELD_TYPE_ONEOF: {
                    int r2 = json_unmarshal_array_internal_oneof(
                        content, pos,
                        (void**)(fi->offset + param->instance_ptr), &len,
                        fi->type_size,
                        fi->oneof_tag_field,
                        fi->enum_strings,
                        fi->oneof_variant_structs,
                        fi->enum_count,
                        fi->oneof_tag_offset,
                        fi->oneof_value_offset,
                        param->depth + 1, txt);
                    if (r2 < 0) return -1;
                    break;
                }
                default: {
                    sstr_t e = PERROR(pos, "unsupported field type %d",
                                      fi->field_type);
                    sstr_append(txt, e);
                    sstr_free(e);
                    return -1;
                }
            }
            *(int*)(param->instance_ptr + len_fi->offset) = len;

            if (fi->has_field_offset >= 0) {
                *(bool*)((char*)param->instance_ptr + fi->has_field_offset) = true;
            }
            continue;
        }

        int r;
        // field value
        switch (fi->field_type) {
            case FIELD_TYPE_INT:
            case FIELD_TYPE_BOOL:
                r = json_unmarshal_scalar_int(
                    content, pos,
                    (int*)((char*)param->instance_ptr + fi->offset), txt);
                if (r != 0) {
                    return r;
                }
                break;
            case FIELD_TYPE_LONG:
                r = json_unmarshal_scalar_long(
                    content, pos,
                    (long*)((char*)param->instance_ptr + fi->offset), txt);
                if (r != 0) {
                    return r;
                }
                break;
            case FIELD_TYPE_INT8:
                r = json_unmarshal_scalar_int8_t(
                    content, pos,
                    (int8_t*)((char*)param->instance_ptr + fi->offset), txt);
                if (r != 0) {
                    return r;
                }
                break;
            case FIELD_TYPE_INT16:
                r = json_unmarshal_scalar_int16_t(
                    content, pos,
                    (int16_t*)((char*)param->instance_ptr + fi->offset), txt);
                if (r != 0) {
                    return r;
                }
                break;
            case FIELD_TYPE_INT32:
                r = json_unmarshal_scalar_int32_t(
                    content, pos,
                    (int32_t*)((char*)param->instance_ptr + fi->offset), txt);
                if (r != 0) {
                    return r;
                }
                break;
            case FIELD_TYPE_INT64:
                r = json_unmarshal_scalar_int64_t(
                    content, pos,
                    (int64_t*)((char*)param->instance_ptr + fi->offset), txt);
                if (r != 0) {
                    return r;
                }
                break;
            case FIELD_TYPE_UINT8:
                r = json_unmarshal_scalar_uint8_t(
                    content, pos,
                    (uint8_t*)((char*)param->instance_ptr + fi->offset), txt);
                if (r != 0) {
                    return r;
                }
                break;
            case FIELD_TYPE_UINT16:
                r = json_unmarshal_scalar_uint16_t(
                    content, pos,
                    (uint16_t*)((char*)param->instance_ptr + fi->offset), txt);
                if (r != 0) {
                    return r;
                }
                break;
            case FIELD_TYPE_UINT32:
                r = json_unmarshal_scalar_uint32_t(
                    content, pos,
                    (uint32_t*)((char*)param->instance_ptr + fi->offset), txt);
                if (r != 0) {
                    return r;
                }
                break;
            case FIELD_TYPE_UINT64:
                r = json_unmarshal_scalar_uint64_t(
                    content, pos,
                    (uint64_t*)((char*)param->instance_ptr + fi->offset), txt);
                if (r != 0) {
                    return r;
                }
                break;

            case FIELD_TYPE_FLOAT:
                r = json_unmarshal_scalar_float(
                    content, pos,
                    (float*)((char*)param->instance_ptr + fi->offset), txt);
                if (r != 0) {
                    return r;
                }
                break;
            case FIELD_TYPE_DOUBLE:
                r = json_unmarshal_scalar_double(
                    content, pos,
                    (double*)((char*)param->instance_ptr + fi->offset), txt);
                if (r != 0) {
                    return r;
                }
                break;
            case FIELD_TYPE_SSTR: {
                sstr_t s = NULL;
                r = json_unmarshal_scalar_sstr_t(content, pos, &s, txt);
                *(sstr_t*)((char*)param->instance_ptr + fi->offset) = (void*)s;
                if (r != 0) {
                    return r;
                }
                break;
            }

            case FIELD_TYPE_STRUCT: {
                const struct json_nested_mask* nm =
                    json_find_nested_mask(param, fi->field_index);
                struct json_parse_param sub_param;
                sub_param.instance_ptr = param->instance_ptr + fi->offset;
                sub_param.in_array = 0;
                sub_param.in_struct = 1;
                sub_param.depth = param->depth + 1;
                sub_param.struct_name = fi->field_type_name;
                sub_param.field_name = fi->field_name;
                if (nm) {
                    sub_param.field_mask = nm->mask;
                    sub_param.field_mask_word_count = nm->mask_word_count;
                    sub_param.nested_masks = nm->sub_masks;
                    sub_param.nested_mask_count = nm->sub_mask_count;
                } else {
                    sub_param.field_mask = NULL;
                    sub_param.field_mask_word_count = 0;
                    sub_param.nested_masks = NULL;
                    sub_param.nested_mask_count = 0;
                }
                tk = json_unmarshal_struct_internal(content, pos, &sub_param,
                                                    txt);
                if (tk == -1) {
                    return -1;
                }
            } break;

            case FIELD_TYPE_ENUM:
                r = json_unmarshal_scalar_enum(
                    content, pos,
                    (int*)((char*)param->instance_ptr + fi->offset),
                    fi->enum_strings, fi->enum_count, txt);
                if (r != 0) {
                    return r;
                }
                break;

            case FIELD_TYPE_ONEOF: {
                r = json_unmarshal_oneof_internal(
                    content, pos,
                    (char*)param->instance_ptr + fi->offset,
                    fi->oneof_tag_field,
                    fi->enum_strings,
                    fi->oneof_variant_structs,
                    fi->enum_count,
                    fi->oneof_tag_offset,
                    fi->oneof_value_offset,
                    param->depth + 1, txt);
                if (r < 0) {
                    return -1;
                }
            } break;
        }
        if (fi->has_field_offset >= 0) {
            *(bool*)((char*)param->instance_ptr + fi->has_field_offset) = true;
        }
    }
    //
    return 0;
}
