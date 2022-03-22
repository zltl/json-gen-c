/**
 * @file json_parse.h
 * @brief parse json string.
 */

#ifndef JSON_PARSE_H
#define JSON_PARSE_H

#include "sstr.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#define FIELD_TYPE_INT 0
#define FIELD_TYPE_LONG 1
#define FIELD_TYPE_FLOAT 2
#define FIELD_TYPE_DOUBLE 3
#define FIELD_TYPE_SSTR 4
#define FIELD_TYPE_STRUCT 6

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

int unmarshal_struct_internal(sstr_t content, struct json_pos* pos,
                              struct json_parse_param* param, sstr_t txt);

#ifdef __cplusplus
}
#endif

#endif  // JSON_PARSE_H
