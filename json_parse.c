#include "json_parse.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "example.h"
#include "utils/sstr.h"

extern struct field_offset_item field_offset_item[];
extern int entry_hash_size;
extern int entry_hash[];

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
inline static unsigned int hash_2s(sstr_t key1, sstr_t key2) {
    unsigned int res = 0xbc9f1d34;
    sstr_t tmp = sstr_dup(key1);
    sstr_append_of(tmp, "#", 1);
    sstr_append(tmp, key2);
    res = hash_s(sstr_cstr(tmp), sstr_length(tmp), res);
    sstr_free(tmp);
    return res;
}

inline static unsigned int hash_2s_c(const char* key1, const char* key2) {
    unsigned int res = 0xbc9f1d34;
    sstr_t tmp = sstr(key1);
    sstr_append_of(tmp, "#", 1);
    sstr_append_cstr(tmp, key2);
    res = hash_s(sstr_cstr(tmp), sstr_length(tmp), res);
    sstr_free(tmp);
    return res;
}

struct field_offset_item* field_offset_item_find(const char* st,
                                                 const char* field) {
    unsigned int h = hash_2s_c(st, field) % entry_hash_size;
    int id = entry_hash[h];
    if (id < 0) {
        return NULL;
    }

    do {
        struct field_offset_item* item = &field_offset_item[id];
        if (strcmp(st, item->struct_name) == 0 &&
            strcmp(field, item->field_name) == 0) {
            return item;
        }
        h++;
        if ((int)h >= entry_hash_size) {
            h = 0;
        }
        id = entry_hash[h];
        if (id < 0) {
            return NULL;
        }
    } while (1);

    return NULL;
}

static char* ptoken(int type, sstr_t txt) {
    switch (type) {
        case JSON_TOKEN_QUOTE:
            return "\"";
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
        case JSON_TOKEN_BOOL_TRUE:
            return "true";
        case JSON_TOKEN_BOOL_FALSE:
            return "false";
        case JSON_TOKEN_NULL:
            return "null";
        case JSON_TOKEN_IDENTIFY:
        case JSON_TOKEN_STRING:
        case JSON_TOKEN_INTEGER:
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

#define PERROR(pos, msg, ...) \
    sstr_printf("line %d col %d: " msg, pos->line, pos->col, ##__VA_ARGS__)

static int json_next_token_(sstr_t content, struct json_pos* pos, sstr_t txt);

static int json_next_token(sstr_t content, struct json_pos* pos, sstr_t txt) {
    int tk = json_next_token_(content, pos, txt);
    printf("TOKEN>%s /line %d col %d\n", ptoken(tk, txt), pos->line, pos->col);
    return tk;
}

static int json_next_token_(sstr_t content, struct json_pos* pos, sstr_t txt) {
    long len = sstr_length(content);
    long i = pos->offset;
    char* data = sstr_cstr(content);

    sstr_clear(txt);

    if (i >= len) {
        return JSON_TOKEN_EOF;
    }

    while (i < len) {
        // trim spaces
        while (i < len && (data[i] == ' ' || data[i] == '\t' ||
                           data[i] == '\r' || data[i] == '\n')) {
            i++;
            pos->col++;
            if (data[i] == '\n') {
                pos->line++;
                pos->col = 0;
            }
        }

        // skip one line comment
        if (i + 1 < len && data[i] == '/' && data[i + 1] == '/') {
            i += 2;
            while (i < len && data[i] != '\n') {
                i++;
                pos->col++;
            }
            pos->col = 0;
            pos->line++;
            pos->offset = i;
            continue;
        }
        // multiple line comment
        else if (i + 1 < len && data[i] == '/' && data[i + 1] == '*') {
            i += 2;
            while (i + 1 < len && data[i] != '*' && data[i + 1] != '/') {
                i++;
                pos->col++;
                if (data[i] == '\n') {
                    pos->line++;
                    pos->col = 0;
                }
            }
            i += 2;
            pos->offset = i;
            continue;
        }

        // parse tokens
        switch (data[i]) {
            case '\"':  // string
                i++;
                pos->col++;
                long start_pos = i;
                while (i < len && data[i] != '\"') {
                    if (data[i] == '\\' && i + 1 < len && data[i + 1] == '\"') {
                        i++;
                        pos->col++;
                    }
                    i++;
                    pos->col++;
                }
                if (data[i] != '\"') {
                    sstr_t e =
                        PERROR(pos, "expected \" but reach end of string");
                    sstr_append(txt, e);
                    sstr_free(e);
                    pos->offset = i;
                    return JSON_ERROR;
                }
                i++;
                pos->col++;
                sstr_append_of(txt, data + start_pos, i - start_pos - 1);
                pos->offset = i;
                return JSON_TOKEN_STRING;
            case '[':
                i++;
                pos->col++;
                pos->offset = i;
                return JSON_TOKEN_LEFT_BRACKET;
            case '{':
                i++;
                pos->col++;
                pos->offset = i;
                return JSON_TOKEN_LEFT_BRACE;
            case ']':
                i++;
                pos->col++;
                pos->offset = i;
                return JSON_TOKEN_RIGHT_BRACKET;
            case '}':
                i++;
                pos->col++;
                pos->offset = i;
                return JSON_TOKEN_RIGHT_BRACE;
            case ':':
                i++;
                pos->col++;
                pos->offset = i;
                return JSON_TOKEN_COLON;
            case ',':
                i++;
                pos->col++;
                pos->offset = i;
                return JSON_TOKEN_COMMA;
            default:
                pos->offset = i;
                break;  //  identify, integer, float, bool, null
        }               //                  ^
        //                                  |
        //                                 ---
        int tk = JSON_TOKEN_INTEGER;
        int start_pos = i;
        while (i < len &&
               (isalnum(data[i]) || data[i] == '_' || data[i] == '.')) {
            if (data[i] == '.' && tk == JSON_TOKEN_INTEGER) {
                tk = JSON_TOKEN_FLOAT;
            }
            if (!isdigit(data[i]) && data[i] != '.') {
                tk = JSON_TOKEN_IDENTIFY;
            }
            i++;
            pos->col++;
        }
        if (tk == JSON_TOKEN_INTEGER || tk == JSON_TOKEN_FLOAT) {
            sstr_append_of(txt, data + start_pos, i - start_pos);
            pos->offset = i;
            return JSON_TOKEN_INTEGER;
        }
        if (sstr_compare_c(txt, "true") == 0) {
            tk = JSON_TOKEN_BOOL_TRUE;
        } else if (sstr_compare_c(txt, "false") == 0) {
            tk = JSON_TOKEN_BOOL_FALSE;
        } else if (sstr_compare_c(txt, "null") == 0) {
            tk = JSON_TOKEN_NULL;
        } else {
            sstr_t e = PERROR(pos, "unexpected identify %s", sstr_cstr(txt));
            sstr_append(txt, e);
            sstr_free(e);
            pos->offset = i;
            return JSON_ERROR;
        }
        pos->offset = i;
        return tk;
    }
    pos->offset = i;
    return JSON_TOKEN_EOF;
}

int unmarshal_array_interval(sstr_t content, struct json_pos* pos,
                             struct json_parse_param* param, sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk != JSON_TOKEN_LEFT_BRACKET) {
        sstr_t e = PERROR(pos, "expected '[' but reach '%s'", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return JSON_ERROR;
    }
    struct field_offset_item* fi =
        field_offset_item_find(param->struct_name, param->field_name);
    if (fi == NULL) {
        sstr_t e = PERROR(pos, "field %s not found", param->field_name);
        sstr_append(txt, e);
        sstr_free(e);
        return JSON_ERROR;
    }
    while (1) {
        tk = json_next_token(content, pos, txt);
        if (tk == JSON_TOKEN_RIGHT_BRACKET) {
            break;
        }
        if (tk == JSON_TOKEN_COMMA) {
            continue;
        }
        if (fi->field_type == FIELD_TYPE_STRUCT) {
            // TODO:
        }
    }
    return 0;
}

int unmarshal_scalar_int(sstr_t content, struct json_pos* pos, int* val,
                         sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk == JSON_TOKEN_BOOL_FALSE) {
        *val = 0;
    } else if (tk == JSON_TOKEN_BOOL_TRUE) {
        *val = 1;
    } else if (tk != JSON_TOKEN_INTEGER) {
        sstr_t e =
            PERROR(pos, "expected integer but got '%s'", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return tk;
    } else {
        *val = atoi(sstr_cstr(txt));
    }
    return 0;
}

int unmarshal_scalar_long(sstr_t content, struct json_pos* pos, long* val,
                          sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk != JSON_TOKEN_INTEGER) {
        sstr_t e =
            PERROR(pos, "expected integer but got '%s'", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return tk;
    } else {
        *val = atol(sstr_cstr(txt));
    }
    return 0;
}

int unmarshal_scalar_float(sstr_t content, struct json_pos* pos, float* val,
                           sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk != JSON_TOKEN_FLOAT && tk != JSON_TOKEN_INTEGER) {
        sstr_t e = PERROR(pos, "expected floating number but got '%s'",
                          ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return tk;
    } else {
        *val = atof(sstr_cstr(txt));
    }
    return 0;
}

int unmarshal_scalar_double(sstr_t content, struct json_pos* pos, double* val,
                            sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk != JSON_TOKEN_FLOAT && tk != JSON_TOKEN_INTEGER) {
        sstr_t e = PERROR(pos, "expected floating number but got '%s'",
                          ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return tk;
    } else {
        *val = atof(sstr_cstr(txt));
    }
    return 0;
}

int unmarshal_scalar_sstr_t(sstr_t content, struct json_pos* pos, sstr_t val,
                            sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk == JSON_TOKEN_NULL) {
        return 0;
    } else if (tk != JSON_TOKEN_STRING) {
        sstr_t e = PERROR(pos, "expected string but got '%s'", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return tk;
    } else {
        sstr_append(val, txt);
    }
    return 0;
}

static int unmarshal_ignore_value(sstr_t content, struct json_pos* pos,
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

#define DEF_UNMARSHAL_ARRAY_TYPE(type)                                   \
    int unmarshal_array_##type(sstr_t content, struct json_pos* pos,     \
                               type** ptr, int* ptrlen, sstr_t txt) {    \
        int tk = json_next_token(content, pos, txt);                     \
        if (tk != JSON_TOKEN_LEFT_BRACKET) {                             \
            sstr_t e =                                                   \
                PERROR(pos, "expected '[' but got %s", ptoken(tk, txt)); \
            sstr_append(txt, e);                                         \
            sstr_free(e);                                                \
        }                                                                \
        while (1) {                                                      \
            type res = 0;                                                \
            int r = unmarshal_scalar_##type(content, pos, &res, txt);    \
            if (r == JSON_TOKEN_RIGHT_BRACKET) {                         \
                return 0;                                                \
            }                                                            \
            if (r < 0) {                                                 \
                return r;                                                \
            }                                                            \
            *ptr = (type*)realloc(*ptr, (*ptrlen + 1) * sizeof(type));   \
            (*ptr)[*ptrlen] = res;                                       \
            *ptrlen = *ptrlen + 1;                                       \
            int tk = json_next_token(content, pos, txt);                 \
            if (tk == JSON_TOKEN_RIGHT_BRACKET) {                        \
                return 0;                                                \
            }                                                            \
            if (tk == JSON_TOKEN_COMMA) {                                \
                continue;                                                \
            }                                                            \
            if (tk == JSON_ERROR) {                                      \
                return -1;                                               \
            }                                                            \
            if (tk == JSON_TOKEN_EOF) {                                  \
                sstr_t e = PERROR(pos, "parsing array, each EOF");       \
                sstr_append(txt, e);                                     \
                sstr_free(e);                                            \
                return -1;                                               \
            }                                                            \
        }                                                                \
    }

DEF_UNMARSHAL_ARRAY_TYPE(int)
DEF_UNMARSHAL_ARRAY_TYPE(long)
DEF_UNMARSHAL_ARRAY_TYPE(float)
DEF_UNMARSHAL_ARRAY_TYPE(double)
int unmarshal_array_sstr_t(sstr_t content, struct json_pos* pos, sstr_t** ptr,
                           int* ptrlen, sstr_t txt) {
    int tk = json_next_token(content, pos, txt);
    if (tk != JSON_TOKEN_LEFT_BRACKET) {
        sstr_t e = PERROR(pos, "expected '[' but got %s", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return -1;
    }

    while (1) {
        sstr_t res = sstr_new();
        int r = unmarshal_scalar_sstr_t(content, pos, &res, txt);
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

int unmarshal_struct_internal(sstr_t content, struct json_pos* pos,
                              struct json_parse_param* param, sstr_t txt);

int unmarshal_array_struct_internal(sstr_t content, struct json_pos* pos,
                                    struct json_parse_param* param, int* len,
                                    sstr_t txt) {
    *len = 0;
    struct field_offset_item* field =
        field_offset_item_find(param->struct_name, "");
    if (field == NULL) {
        sstr_t e = PERROR(pos, "struct %s not found", param->struct_name);
        sstr_append(txt, e);
        sstr_free(e);
        return -1;
    }
    printf("array find field struct %s, size %d\n", field->struct_name,
           field->type_size);

    int tk = json_next_token(content, pos, txt);
    if (tk != JSON_TOKEN_LEFT_BRACKET) {
        sstr_t e = PERROR(pos, "expected '[' but got %s", ptoken(tk, txt));
        sstr_append(txt, e);
        sstr_free(e);
        return -1;
    }

    while (1) {
        void* ptr = malloc(field->type_size);
        struct json_parse_param sub_param;
        sub_param.instance_ptr = ptr;
        sub_param.in_array = 1;
        sub_param.in_struct = 0;
        sub_param.struct_name = param->struct_name;
        sub_param.field_name = param->field_name;

        int r = unmarshal_struct_internal(content, pos, &sub_param, txt);
        if (r < 0) {
            free(ptr);
            return r;
        }
        if (r == 1) {
            free(ptr);
            return 0;  // finished
        }

        void* pptr = realloc(*(void**)param->instance_ptr,
                             (*len + 1) * field->type_size);
        memcpy(pptr + (*len * field->type_size), ptr, field->type_size);
        free(ptr);
        *(void**)param->instance_ptr = pptr;
        *len = *len + 1;

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

int unmarshal_struct_internal(sstr_t content, struct json_pos* pos,
                              struct json_parse_param* param, sstr_t txt) {
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

        struct field_offset_item* fi =
            field_offset_item_find(param->struct_name, sstr_cstr(txt));
        if (fi == NULL) {
            printf("field_offset_item_find NULL, ignoring...\n");
            unmarshal_ignore_value(content, pos, txt);
            continue;
        }

        printf("field found: %s->%s %s is_array: %d\n", (fi->struct_name),
               (fi->field_name), fi->field_type_name, fi->is_array);

        tk = json_next_token(content, pos, txt);
        if (tk != JSON_TOKEN_COLON) {
            sstr_t e =
                PERROR(pos, "expected ':' but got '%s'", ptoken(tk, txt));
            sstr_append(txt, e);
            sstr_free(e);
            return -1;
        }

        // TODO: get field items
        if (fi->is_array) {
            sstr_t field_len_name = sstr(fi->field_name);
            sstr_append_cstr(field_len_name, "_len");
            struct field_offset_item* len_fi = field_offset_item_find(
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
                    int r = unmarshal_array_struct_internal(
                        content, pos, &ar_param, &len, txt);
                    if (r < 0) {
                        return r;
                    }

                    break;
                }
                case FIELD_TYPE_INT:
                    unmarshal_array_int(content, pos,
                                        fi->offset + param->instance_ptr, &len,
                                        txt);
                    break;
                case FIELD_TYPE_LONG:
                    printf("parsing long array..........\n");
                    unmarshal_array_long(content, pos,
                                         fi->offset + param->instance_ptr, &len,
                                         txt);
                    break;
                case FIELD_TYPE_FLOAT:
                    unmarshal_array_float(content, pos,
                                          fi->offset + param->instance_ptr,
                                          &len, txt);
                    break;
                case FIELD_TYPE_DOUBLE:
                    unmarshal_array_double(content, pos,
                                           fi->offset + param->instance_ptr,
                                           &len, txt);
                    break;
                case FIELD_TYPE_SSTR:
                    unmarshal_array_sstr_t(content, pos,
                                           fi->offset + param->instance_ptr,
                                           &len, txt);
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
                r = unmarshal_scalar_int(
                    content, pos,
                    (int*)((char*)param->instance_ptr + fi->offset), txt);
                if (r != 0) {
                    return r;
                }
                break;
            case FIELD_TYPE_LONG:
                r = unmarshal_scalar_long(
                    content, pos,
                    (long*)((char*)param->instance_ptr + fi->offset), txt);
                if (r != 0) {
                    return r;
                }
                break;

            case FIELD_TYPE_FLOAT:
                r = unmarshal_scalar_float(
                    content, pos,
                    (float*)((char*)param->instance_ptr + fi->offset), txt);
                if (r != 0) {
                    return r;
                }
                break;
            case FIELD_TYPE_DOUBLE:
                r = unmarshal_scalar_double(
                    content, pos,
                    (double*)((char*)param->instance_ptr + fi->offset), txt);
                if (r != 0) {
                    return r;
                }
                break;
            case FIELD_TYPE_SSTR: {
                sstr_t s = sstr_new();
                r = unmarshal_scalar_sstr_t(content, pos, s, txt);
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
                tk = unmarshal_struct_internal(content, pos, &sub_param, txt);
                if (tk == -1) {
                    return -1;
                }
            } break;
        }
    }
    //
    return 0;
}
