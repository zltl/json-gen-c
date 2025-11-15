
/*
TOKEN_STRING = "([^"] | \")*"
TOKEN_INT = [0-9]+
TOKEN_FLOAT = [0-9]*\.[0-9]+
TOKEN_TRUE = true
TOKEN_FALSE = false
TOKEN_NULL = null
TOKEN_COMMA = ,
TOKEN_COLON = :
TOKEN_LEFT_BRACE = {
TOKEN_RIGHT_BRACE = }
TOKEN_LEFT_BRACKET = [
TOKEN_RIGHT_BRACKET = ]

---------------

JSON = STRUCT | ARRAY
STRUCT = { FIELD_LIST }
FIELD_LIST = FIELD | FIELD, FIELD_LIST | empty
FIELD = FIELD_KEY : FIELD_VALUE
FIELD_KEY = TOKEN_STRING
FIELD_VALUE = SCALAR_VALUE | STRUCT | ARRAY
SCALAR_VALUE = TOKEN_STRING | TOKEN_FLOAT | TOKEN_INT | TOKEN_BOOL | TOKEN_NULL
ELEMENT_LIST = FIELD_VALUE | FIELD_VALUE, ELEMENT_LIST | empty
ARRAY = [ ELEMENT_LIST ]
*/

enum json_token {
    JSON_ERROR = -1,
    JSON_TOKEN_EOF = 0,
    JSON_TOKEN_STRING,
    JSON_TOKEN_INT,
    JSON_TOKEN_FLOAT,
    JSON_TOKEN_TRUE,
    JSON_TOKEN_FALSE,
    JSON_TOKEN_NULL,
    JSON_TOKEN_COMMA = ',',          // ,
    JSON_TOKEN_COLON = ':',          // :
    JSON_TOKEN_LEFT_BRACE = '{',     // {
    JSON_TOKEN_RIGHT_BRACE = '}',    // }
    JSON_TOKEN_LEFT_BRACKET = '[',   // [
    JSON_TOKEN_RIGHT_BRACKET = ']',  // ]
};

// field type id
// !NOTE: MUST SAME AS IN src/struct/struct_parse.h
#define FIELD_TYPE_INT 0
#define FIELD_TYPE_LONG 1
#define FIELD_TYPE_FLOAT 2
#define FIELD_TYPE_DOUBLE 3
#define FIELD_TYPE_SSTR 4
#define FIELD_TYPE_STRUCT 6
#define FIELD_TYPE_BOOL 7

struct json_parse_param {
    void* instance_ptr;
    int in_array;
    int in_struct;
    const char* struct_name;
    const char* field_name;
};

struct json_pos {
    int line;
    int col;
    long offset;
};

static int json_unmarshal_struct_internal(sstr_t content, struct json_pos* pos,
                                          struct json_parse_param* param,
                                          sstr_t txt);
static int json_unmarshal_array_internal(sstr_t content, struct json_pos* pos,
                                         struct json_parse_param* param,
                                         int* len, sstr_t txt);
static int json_unmarshal_array_internal_sstr_t(sstr_t content,
                                                struct json_pos* pos,
                                                sstr_t** ptr, int* ptrlen,
                                                sstr_t txt);
static int json_unmarshal_array_internal_int(sstr_t content,
                                             struct json_pos* pos, int** ptr,
                                             int* ptrlen, sstr_t txt);
static int json_unmarshal_array_internal_long(sstr_t content,
                                              struct json_pos* pos, long** ptr,
                                              int* ptrlen, sstr_t txt);
static int json_unmarshal_array_internal_float(sstr_t content,
                                               struct json_pos* pos,
                                               float** ptr, int* ptrlen,
                                               sstr_t txt);
static int json_unmarshal_array_internal_double(sstr_t content,
                                                struct json_pos* pos,
                                                double** ptr, int* ptrlen,
                                                sstr_t txt);

int json_marshal_array_indent_int(int* obj, int len, int indent, int curindent,
                                  sstr_t out) {
    int i;
    sstr_append_of(out, "[", 1);
    sstr_append_of_if(out, "\n", 1, indent);
    curindent += indent;
    for (i = 0; i < len; i++) {
        sstr_append_indent(out, curindent);
        sstr_printf_append(out, "%d", obj[i]);
        if (i != len - 1) {
            sstr_append_of(out, ",", 1);
        }
        sstr_append_of_if(out, "\n", 1, indent);
    }
    curindent -= indent;
    sstr_append_indent(out, curindent);
    sstr_append_of(out, "]", 1);
    return 0;
}

int json_marshal_array_indent_long(long* obj, int len, int indent,
                                   int curindent, sstr_t out) {
    int i;
    sstr_append_of(out, "[", 1);
    sstr_append_of_if(out, "\n", 1, indent);
    curindent += indent;
    for (i = 0; i < len; i++) {
        sstr_append_indent(out, curindent);
        sstr_printf_append(out, "%l", obj[i]);
        if (i != len - 1) {
            sstr_append_of(out, ",", 1);
        }
        sstr_append_of_if(out, "\n", 1, indent);
    }
    curindent -= indent;
    sstr_append_indent(out, curindent);
    sstr_append_of(out, "]", 1);
    return 0;
}

int json_marshal_array_indent_float(float* obj, int len, int indent,
                                    int curindent, sstr_t out) {
    int i;
    sstr_append_of(out, "[", 1);
    sstr_append_of_if(out, "\n", 1, indent);
    curindent += indent;
    for (i = 0; i < len; i++) {
        sstr_append_indent(out, curindent);
        sstr_append_float_str(out, obj[i], -1);
        if (i != len - 1) {
            sstr_append_of(out, ",", 1);
        }
        sstr_append_of_if(out, "\n", 1, indent);
    }
    curindent -= indent;
    sstr_append_indent(out, curindent);
    sstr_append_of(out, "]", 1);
    return 0;
}

int json_marshal_array_indent_double(double* obj, int len, int indent,
                                     int curindent, sstr_t out) {
    int i;
    sstr_append_of(out, "[", 1);
    sstr_append_of_if(out, "\n", 1, indent);
    curindent += indent;
    for (i = 0; i < len; i++) {
        sstr_append_indent(out, curindent);
        sstr_append_double_str(out, obj[i], -1);
        if (i != len - 1) {
            sstr_append_of(out, ",", 1);
        }
        sstr_append_of_if(out, "\n", 1, indent);
    }
    curindent -= indent;
    sstr_append_indent(out, curindent);
    sstr_append_of(out, "]", 1);
    return 0;
}

int json_marshal_array_indent_sstr_t(sstr_t* obj, int len, int indent,
                                     int curindent, sstr_t out) {
    int i;
    sstr_append_of(out, "[", 1);
    sstr_append_of_if(out, "\n", 1, indent);
    curindent += indent;
    for (i = 0; i < len; i++) {
        sstr_append_indent(out, curindent);
        sstr_append_cstr(out, "\"");
        sstr_json_escape_string_append(out, obj[i]);
        sstr_append_cstr(out, "\"");
        if (i != len - 1) {
            sstr_append_of(out, ",", 1);
        }
        sstr_append_of_if(out, "\n", 1, indent);
    }
    curindent -= indent;
    sstr_append_indent(out, curindent);
    sstr_append_of(out, "]", 1);
    return 0;
}
