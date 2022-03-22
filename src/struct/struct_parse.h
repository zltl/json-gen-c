/**
 * @file struct/struct_parse.h
 * @brief Parse struct definitions. Support scalar types(int, long, float,
 * double), arrays, structs.
 */

#ifndef STRUCT_PARSE_H_
#define STRUCT_PARSE_H_

#include <stddef.h>

#include "utils/hash_map.h"
#include "utils/sstr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FIELD_TYPE_INT 0
#define FIELD_TYPE_LONG 1
#define FIELD_TYPE_FLOAT 2
#define FIELD_TYPE_DOUBLE 3
#define FIELD_TYPE_SSTR 4
#define FIELD_TYPE_STRUCT 6

#define TYPE_NAME_INT "int"
#define TYPE_NAME_SSTR "sstr_t"
#define TYPE_NAME_LONG "long"
#define TYPE_NAME_FLOAT "float"
#define TYPE_NAME_DOUBLE "double"

#define STRUCT_MAP_BUCKET_SIZE 128

struct struct_field {
    sstr_t name;
    int type;
    int is_array;
    sstr_t type_name;
    struct struct_field* next;
};

struct struct_container {
    sstr_t name;
    struct struct_field* fields;
};

struct pos {
    int line;
    int col;
    long offset;
};

struct struct_parser {
    // struct name --> struct_field list
    struct hash_map* struct_map;
    struct pos pos;
};

#define TOKEN_LEFT_BRACE '{'
#define TOKEN_RIGHT_BRACE '}'
#define TOKEN_LEFT_BRACKET '['
#define TOKEN_RIGHT_BRACKET ']'
#define TOKEN_SEMICOLON ';'
#define TOKEN_IDENTIFY 1
#define TOKEN_INTEGER 2
#define TOKEN_FLOAT 3
#define TOKEN_EOF 0
#define TOKEN_ERROR -1

struct struct_token {
    int type;
    sstr_t txt;
};

struct struct_parser* struct_parser_new();
void struct_parser_free(struct struct_parser* parser);
int struct_parser_parse(struct struct_parser* parser, sstr_t content);

#ifdef __cplusplus
}
#endif

#endif  // STRUCT_PARSE_H_
