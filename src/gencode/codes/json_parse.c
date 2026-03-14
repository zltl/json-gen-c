
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sstr.h"
#include "utils/error_codes.h"

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
    unsigned int h = seed ^ (n * m);

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
            __attribute__((fallthrough));
        case 2:
            h += (unsigned char)(data[1]) << 8;
            __attribute__((fallthrough));
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
        // -1 means empty slot.
        // The size of hash array is twize of items number, so the array
        // should have many empty slots.
        // We step to next slot if conflict when construct the array, so
        // if we find empty slot, it means the key is not in the hash table
        // array.
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
            return sstr_cstr(txt);
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
    sstr_printf("line %d col %d: " msg, pos->line, pos->col, ##__VA_ARGS__)

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
    char* data = sstr_cstr(content);
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
    unsigned int first_code = parse_hex4((const unsigned char*)&data[i + 1]);
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
    char* data = sstr_cstr(content);
    
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

    sstr_clear(txt);
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
                    sstr_append_of(txt, "\b", 1);
                    i++;
                    pos->col++;
                    break;
                case 'f':
                    sstr_append_of(txt, "\f", 1);
                    i++;
                    pos->col++;
                    break;
                case 'n':
                    sstr_append_of(txt, "\n", 1);
                    i++;
                    pos->col++;
                    break;
                case 'r':
                    sstr_append_of(txt, "\r", 1);
                    i++;
                    pos->col++;
                    break;
                case 't':
                    sstr_append_of(txt, "\t", 1);
                    i++;
                    pos->col++;
                    break;
                case '\"':
                    sstr_append_of(txt, "\"", 1);
                    i++;
                    pos->col++;
                    break;
                case '\\':
                    sstr_append_of(txt, "\\", 1);
                    i++;
                    pos->col++;
                    break;
                case '/':
                    sstr_append_of(txt, "/", 1);
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
            sstr_append_of(txt, data + i, j - i);
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
    char* data = sstr_cstr(content);

    int skiped = 0;

    do {
        skiped = 0;
        // trim spaces
        while (i < len && isspace(data[i])) {
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
    char* data = sstr_cstr(content);

    sstr_clear(txt);

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
    sstr_clear(txt);
    int start_pos = i;
    if (isdigit(ch) || ch == '-' || ch == '.') {
        if (ch != '.') {
            i++;
            pos->col++;
            while (i < len && isdigit(data[i])) {
                i++;
                pos->col++;
            }
        }
        if (i < len && data[i] == '.') {
            tk = JSON_TOKEN_FLOAT;
            i++;
            pos->col++;
            while (i < len && isdigit(data[i])) {
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
            while (i < len && isdigit(data[i])) {
                i++;
                pos->col++;
            }
        }
        sstr_append_of(txt, data + start_pos, i - start_pos);
        pos->offset = i;
        return tk;
    }
    // parse true, false, null
    while (i < len && isalpha(data[i])) {
        i++;
        pos->col++;
    }
    sstr_append_of(txt, data + start_pos, i - start_pos);
    pos->offset = i;
    if (sstr_compare_c(txt, "true") == 0) {
        return JSON_TOKEN_TRUE;
    }
    if (sstr_compare_c(txt, "false") == 0) {
        return JSON_TOKEN_FALSE;
    }
    if (sstr_compare_c(txt, "null") == 0) {
        return JSON_TOKEN_NULL;
    }

    sstr_t e = PERROR(pos, "unexpected identify %s", sstr_cstr(txt));
    sstr_append(txt, e);
    sstr_free(e);
    return JSON_ERROR;
}

// parse integer, set integer value to *val, put error string to txt.
// parse bool true to 1, and false to 0.
static int json_unmarshal_scalar_int(sstr_t content, struct json_pos* pos,
                                     int* val, sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk == JSON_TOKEN_FALSE) {
        *val = 0;
    } else if (tk == JSON_TOKEN_TRUE) {
        *val = 1;
    } else if (tk != JSON_TOKEN_INT) {
        sstr_t e =
            PERROR(pos, "expected integer but got '%s'", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return tk;
    } else {
        char* endptr;
        long temp_val = strtol(sstr_cstr(txt), &endptr, 10);
        if (*endptr != '\0' || temp_val > INT_MAX || temp_val < INT_MIN) {
            sstr_t e = PERROR(pos, "integer value out of range: '%s'", sstr_cstr(txt));
            sstr_append(txt, e);
            sstr_free(e);
            return JSON_ERROR;
        }
        *val = (int)temp_val;
    }
    return 0;
}

// parse integer, set long integer value to *val, put integer string to txt.
static int json_unmarshal_scalar_long(sstr_t content, struct json_pos* pos,
                                      long* val, sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk == JSON_TOKEN_FALSE) {
        *val = 0;
    } else if (tk == JSON_TOKEN_TRUE) {
        *val = 1;
    } else if (tk != JSON_TOKEN_INT) {
        sstr_t e =
            PERROR(pos, "expected integer but got '%s'", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return tk;
    } else {
        char* endptr;
        long temp_val = strtol(sstr_cstr(txt), &endptr, 10);
        if (*endptr != '\0') {
            sstr_t e = PERROR(pos, "invalid long integer format: '%s'", sstr_cstr(txt));
            sstr_append(txt, e);
            sstr_free(e);
            return JSON_ERROR;
        }
        *val = temp_val;
    }
    return 0;
}

// parse float, set float value to *val, put error string to txt.
static int json_unmarshal_scalar_float(sstr_t content, struct json_pos* pos,
                                       float* val, sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk != JSON_TOKEN_FLOAT && tk != JSON_TOKEN_INT) {
        sstr_t e = PERROR(pos, "expected floating number but got '%s'",
                          ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return tk;
    } else {
        char* endptr;
        float temp_val = strtof(sstr_cstr(txt), &endptr);
        if (*endptr != '\0') {
            sstr_t e = PERROR(pos, "invalid float format: '%s'", sstr_cstr(txt));
            sstr_append(txt, e);
            sstr_free(e);
            return JSON_ERROR;
        }
        *val = temp_val;
    }
    return 0;
}

// parse double floating point number, set double value to *val, put error
// string to txt.
static int json_unmarshal_scalar_double(sstr_t content, struct json_pos* pos,
                                        double* val, sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk != JSON_TOKEN_FLOAT && tk != JSON_TOKEN_INT) {
        sstr_t e = PERROR(pos, "expected floating number but got '%s'",
                          ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return tk;
    } else {
        char* endptr;
        double temp_val = strtod(sstr_cstr(txt), &endptr);
        if (*endptr != '\0') {
            sstr_t e = PERROR(pos, "invalid double format: '%s'", sstr_cstr(txt));
            sstr_append(txt, e);
            sstr_free(e);
            return JSON_ERROR;
        }
        *val = temp_val;
    }
    return 0;
}

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
        const char* s = sstr_cstr(txt);
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
        long temp_val = strtol(sstr_cstr(txt), &endptr, 10);
        if (*endptr != '\0' || temp_val > INT_MAX || temp_val < INT_MIN) {
            sstr_t e = PERROR(pos, "enum integer value out of range: '%s'", sstr_cstr(txt));
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

// Macro for signed precise-width integer unmarshal (int8_t, int16_t, int32_t)
#define DEFINE_UNMARSHAL_SCALAR_SIGNED(TYPE, MIN_VAL, MAX_VAL)                 \
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
        long temp_val = strtol(sstr_cstr(txt), &endptr, 10);                   \
        if (*endptr != '\0' || temp_val > (MAX_VAL) || temp_val < (MIN_VAL)) { \
            sstr_t e = PERROR(pos, #TYPE " value out of range: '%s'",          \
                              sstr_cstr(txt));                                 \
            sstr_append(txt, e);                                               \
            sstr_free(e);                                                      \
            return JSON_ERROR;                                                 \
        }                                                                      \
        *val = (TYPE)temp_val;                                                 \
    }                                                                          \
    return 0;                                                                  \
}

DEFINE_UNMARSHAL_SCALAR_SIGNED(int8_t, INT8_MIN, INT8_MAX)
DEFINE_UNMARSHAL_SCALAR_SIGNED(int16_t, INT16_MIN, INT16_MAX)
DEFINE_UNMARSHAL_SCALAR_SIGNED(int32_t, INT32_MIN, INT32_MAX)

static int json_unmarshal_scalar_int64_t(sstr_t content, struct json_pos* pos,
                                         int64_t* val, sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk == JSON_TOKEN_FALSE) {
        *val = 0;
    } else if (tk == JSON_TOKEN_TRUE) {
        *val = 1;
    } else if (tk != JSON_TOKEN_INT) {
        sstr_t e = PERROR(pos, "expected integer but got '%s'",
                          ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return tk;
    } else {
        char* endptr;
        long long temp_val = strtoll(sstr_cstr(txt), &endptr, 10);
        if (*endptr != '\0' || temp_val > INT64_MAX || temp_val < INT64_MIN) {
            sstr_t e = PERROR(pos, "int64_t value out of range: '%s'",
                              sstr_cstr(txt));
            sstr_append(txt, e);
            sstr_free(e);
            return JSON_ERROR;
        }
        *val = (int64_t)temp_val;
    }
    return 0;
}

// Macro for unsigned precise-width integer unmarshal (uint8_t, uint16_t, uint32_t)
#define DEFINE_UNMARSHAL_SCALAR_UNSIGNED(TYPE, MAX_VAL)                         \
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
        const char* s = sstr_cstr(txt);                                        \
        while (*s == ' ') s++;                                                 \
        if (*s == '-') {                                                       \
            sstr_t e = PERROR(pos, #TYPE " cannot be negative: '%s'",          \
                              sstr_cstr(txt));                                 \
            sstr_append(txt, e);                                               \
            sstr_free(e);                                                      \
            return JSON_ERROR;                                                 \
        }                                                                      \
        char* endptr;                                                          \
        unsigned long temp_val = strtoul(sstr_cstr(txt), &endptr, 10);         \
        if (*endptr != '\0' || temp_val > (MAX_VAL)) {                         \
            sstr_t e = PERROR(pos, #TYPE " value out of range: '%s'",          \
                              sstr_cstr(txt));                                 \
            sstr_append(txt, e);                                               \
            sstr_free(e);                                                      \
            return JSON_ERROR;                                                 \
        }                                                                      \
        *val = (TYPE)temp_val;                                                 \
    }                                                                          \
    return 0;                                                                  \
}

DEFINE_UNMARSHAL_SCALAR_UNSIGNED(uint8_t, UINT8_MAX)
DEFINE_UNMARSHAL_SCALAR_UNSIGNED(uint16_t, UINT16_MAX)
DEFINE_UNMARSHAL_SCALAR_UNSIGNED(uint32_t, UINT32_MAX)

static int json_unmarshal_scalar_uint64_t(sstr_t content, struct json_pos* pos,
                                          uint64_t* val, sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk == JSON_TOKEN_FALSE) {
        *val = 0;
    } else if (tk == JSON_TOKEN_TRUE) {
        *val = 1;
    } else if (tk != JSON_TOKEN_INT) {
        sstr_t e = PERROR(pos, "expected integer but got '%s'",
                          ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return tk;
    } else {
        const char* s = sstr_cstr(txt);
        while (*s == ' ') s++;
        if (*s == '-') {
            sstr_t e = PERROR(pos, "uint64_t cannot be negative: '%s'",
                              sstr_cstr(txt));
            sstr_append(txt, e);
            sstr_free(e);
            return JSON_ERROR;
        }
        char* endptr;
        unsigned long long temp_val = strtoull(sstr_cstr(txt), &endptr, 10);
        if (*endptr != '\0' || temp_val > UINT64_MAX) {
            sstr_t e = PERROR(pos, "uint64_t value out of range: '%s'",
                              sstr_cstr(txt));
            sstr_append(txt, e);
            sstr_free(e);
            return JSON_ERROR;
        }
        *val = (uint64_t)temp_val;
    }
    return 0;
}

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
    int* arr = (int*)malloc(sizeof(int) * cap);
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
            int* new_arr = (int*)realloc(arr, sizeof(int) * cap);
            if (new_arr == NULL) { free(arr); return -1; }
            arr = new_arr;
        }
        int r = json_unmarshal_scalar_enum(content, pos, &arr[len], enum_strings, enum_count, txt);
        if (r != 0) {
            free(arr);
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

// parse array of string like
//    [1, 2, 3, 8, 3, 5]
static int json_unmarshal_array_internal_int(sstr_t content,
                                             struct json_pos* pos, int** ptr,
                                             int* ptrlen, sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk != JSON_TOKEN_LEFT_BRACKET) {
        sstr_t e = PERROR(pos, "expected '[' but got %s", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
    }
    while (1) {
        int res = 0;
        int r = json_unmarshal_scalar_int(content, pos, &res, txt);
        if (r == JSON_TOKEN_RIGHT_BRACKET) {
            return 0;
        }
        if (r < 0) {
            return r;
        }
        *ptr = (int*)realloc(*ptr, (*ptrlen + 1) * sizeof(int));
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
}

static int json_unmarshal_array_internal_long(sstr_t content,
                                              struct json_pos* pos, long** ptr,
                                              int* ptrlen, sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk != JSON_TOKEN_LEFT_BRACKET) {
        sstr_t e = PERROR(pos, "expected '[' but got %s", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
    }
    while (1) {
        long res = 0;
        int r = json_unmarshal_scalar_long(content, pos, &res, txt);
        if (r == JSON_TOKEN_RIGHT_BRACKET) {
            return 0;
        }
        if (r < 0) {
            return r;
        }
        *ptr = (long*)realloc(*ptr, (*ptrlen + 1) * sizeof(long));
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
}

static int json_unmarshal_array_internal_float(sstr_t content,
                                               struct json_pos* pos,
                                               float** ptr, int* ptrlen,
                                               sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk != JSON_TOKEN_LEFT_BRACKET) {
        sstr_t e = PERROR(pos, "expected '[' but got %s", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
    }
    while (1) {
        float res = 0;
        int r = json_unmarshal_scalar_float(content, pos, &res, txt);
        if (r == JSON_TOKEN_RIGHT_BRACKET) {
            return 0;
        }
        if (r < 0) {
            return r;
        }
        *ptr = (float*)realloc(*ptr, (*ptrlen + 1) * sizeof(float));
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
}

static int json_unmarshal_array_internal_double(sstr_t content,
                                                struct json_pos* pos,
                                                double** ptr, int* ptrlen,
                                                sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk != JSON_TOKEN_LEFT_BRACKET) {
        sstr_t e = PERROR(pos, "expected '[' but got %s", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
    }
    while (1) {
        double res = 0;
        int r = json_unmarshal_scalar_double(content, pos, &res, txt);
        if (r == JSON_TOKEN_RIGHT_BRACKET) {
            return 0;
        }
        if (r < 0) {
            return r;
        }
        *ptr = (double*)realloc(*ptr, (*ptrlen + 1) * sizeof(double));
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
}

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

    while (1) {
        sstr_t res = NULL;
        int r = json_unmarshal_scalar_sstr_t(content, pos, &res, txt);
        if (r == JSON_TOKEN_RIGHT_BRACKET) {
            return 0;
        }
        if (r < 0) {
            return r;
        }
        *ptr = (sstr_t*)realloc(*ptr, (*ptrlen + 1) * sizeof(sstr_t));
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
    while (1) {                                                                \
        TYPE res = 0;                                                          \
        int r = json_unmarshal_scalar_##TYPE(content, pos, &res, txt);         \
        if (r == JSON_TOKEN_RIGHT_BRACKET) {                                   \
            return 0;                                                          \
        }                                                                      \
        if (r < 0) {                                                           \
            return r;                                                          \
        }                                                                      \
        *ptr = (TYPE*)realloc(*ptr, (*ptrlen + 1) * sizeof(TYPE));             \
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
        free(*ptr);
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
        free(*ptr);
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
        free(*ptr);
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
        free(*ptr);
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
        free(*ptr);
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
        free(*ptr);                                                            \
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

    while (1) {
        void* ptr = malloc(field->type_size);
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
        sub_param.struct_name = param->struct_name;
        sub_param.field_name = param->field_name;

        int r = json_unmarshal_struct_internal(content, pos, &sub_param, txt);
        
        // Handle parsing failure - free allocated memory before returning
        if (r < 0) {
            free(ptr);
            return r;
        }
        
        // Reallocate array buffer with error checking
        void* pptr = realloc(*(void**)param->instance_ptr,
                             (*len + 1) * field->type_size);
        if (pptr == NULL) {
            free(ptr);
            sstr_t e = PERROR(pos, "memory reallocation failed for array");
            sstr_append(txt, e);
            sstr_free(e);
            return JSON_GEN_ERROR_MEMORY;
        }
        
        // Copy element to array and update pointer
        memcpy(pptr + (*len * field->type_size), ptr, field->type_size);
        free(ptr);
        *(void**)param->instance_ptr = pptr;
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
                                     sstr_t txt) {
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
            entries = (char*)realloc(entries, (size_t)cap * entry_size);
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
                sub.struct_name = value_type_name;
                sub.field_name = "";
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
        entries = (char*)realloc(entries, (size_t)(*len_p) * entry_size);
        if (entries) {
            *entries_pp = entries;
        }
    }
    return 0;
}

static int json_unmarshal_struct_internal(sstr_t content, struct json_pos* pos,
                                          struct json_parse_param* param,
                                          sstr_t txt) {
    // '{'
    int tk = json_next_token(content, pos, txt);
    if (tk == JSON_TOKEN_EOF) {
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
            // Look ahead to see if there's a trailing comma
            struct json_pos saved_pos = *pos;
            sstr_t temp_txt = sstr_new();
            int next_tk = json_next_token(content, pos, temp_txt);
            
            if (next_tk == JSON_TOKEN_RIGHT_BRACE) {
                // This is a trailing comma - not allowed in strict JSON
                sstr_t e = PERROR(pos, "trailing comma not allowed before '}'");
                sstr_append(txt, e);
                sstr_free(e);
                sstr_free(temp_txt);
                return -1;
            }
            
            // Restore position and continue parsing
            *pos = saved_pos;
            sstr_free(temp_txt);
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
            json_field_offset_item_find(param->struct_name, sstr_cstr(txt));
        if (fi == NULL) {
#if JSON_DEBUG
            printf("json_field_offset_item_find NULL, ignoring...\n");
#endif
            json_unmarshal_ignore_value(content, pos, txt);
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
                        arr = (char*)realloc(arr, (size_t)arr_cap * fi->type_size);
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
                        fi->enum_strings, fi->enum_count, txt);
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
                    arr = (char*)realloc(arr, (size_t)arr_len * fi->type_size);
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
                    fi->enum_strings, fi->enum_count, txt);
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
                        sub.struct_name = fi->field_type_name;
                        sub.field_name = fi->field_name;
                        r = json_unmarshal_struct_internal(content, pos,
                                                           &sub, txt);
                        break;
                    }
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
                    ar_param.struct_name = fi->field_type_name;
                    ar_param.field_name = fi->field_name;
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
                struct json_parse_param sub_param;
                sub_param.instance_ptr = param->instance_ptr + fi->offset;
                sub_param.in_array = 0;
                sub_param.in_struct = 1;
                sub_param.struct_name = fi->field_type_name;
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
        }
        if (fi->has_field_offset >= 0) {
            *(bool*)((char*)param->instance_ptr + fi->has_field_offset) = true;
        }
    }
    //
    return 0;
}
