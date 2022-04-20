
#define JSON_TOKEN_QUOTE '\"'
#define JSON_TOKEN_LEFT_BRACKET '['
#define JSON_TOKEN_RIGHT_BRACKET ']'
#define JSON_TOKEN_LEFT_BRACE '{'
#define JSON_TOKEN_RIGHT_BRACE '}'
#define JSON_TOKEN_COMMA ','
#define JSON_TOKEN_COLON ':'
#define JSON_TOKEN_FLOAT 1
#define JSON_TOKEN_BOOL_TRUE 2
#define JSON_TOKEN_BOOL_FALSE 3
#define JSON_TOKEN_NULL 4
#define JSON_TOKEN_IDENTIFY 5
#define JSON_TOKEN_STRING 6
#define JSON_TOKEN_INTEGER 7
#define JSON_TOKEN_EOF -2
#define JSON_ERROR -1

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
    char* struct_name;
    char* field_name;
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
        sstr_printf_append(out, "%f", (double)obj[i]);
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
        sstr_printf_append(out, "%f", obj[i]);
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
