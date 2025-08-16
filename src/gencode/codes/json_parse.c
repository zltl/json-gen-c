
#include <ctype.h>
#include <limits.h>
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
    unsigned int res = 0xbc9f1d34;
    sstr_t tmp = sstr(key1);
    sstr_append_of(tmp, "#", 1);
    sstr_append_cstr(tmp, key2);
    res = hash_s(sstr_cstr(tmp), sstr_length(tmp), res);
    sstr_free(tmp);
    return res;
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
    unsigned int second_code = 0;
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
        i += 6;
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
    long i = pos->offset;
    char* data = sstr_cstr(content);
    
    // Validate input parameters
    if (data == NULL || txt == NULL || pos == NULL) {
        return JSON_ERROR;
    }
    
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
                default: {
                    sstr_t e = PERROR(pos, "unsupported field type %d",
                                      fi->field_type);
                    sstr_append(txt, e);
                    sstr_free(e);
                    return -1;
                }
            }
            *(int*)(param->instance_ptr + len_fi->offset) = len;

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
        }
    }
    //
    return 0;
}
