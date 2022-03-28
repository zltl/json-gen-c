/**
 * @file struct/struct_parse.h
 * @brief Parse struct definitions. Support scalar types(int, long, float,
 * double, sstr_t), arrays, structs.
 */

#ifndef STRUCT_PARSE_H_
#define STRUCT_PARSE_H_

#include <stddef.h>

#include "utils/hash_map.h"
#include "utils/sstr.h"

#ifdef __cplusplus
extern "C" {
#endif

// field type id
// !NOTE: MUST SAME AS IN gencode/codes/json_parse.h

#define FIELD_TYPE_INT 0
#define FIELD_TYPE_LONG 1
#define FIELD_TYPE_FLOAT 2
#define FIELD_TYPE_DOUBLE 3
#define FIELD_TYPE_SSTR 4
#define FIELD_TYPE_STRUCT 6
#define FIELD_TYPE_BOOL 7

#define TYPE_NAME_INT "int"
#define TYPE_NAME_BOOL "bool"
#define TYPE_NAME_SSTR "sstr_t"
#define TYPE_NAME_LONG "long"
#define TYPE_NAME_FLOAT "float"
#define TYPE_NAME_DOUBLE "double"

/**
 * @brief We use a hash map to store parsed structs, and use the struct name
 * as the key. This is the size of the hash map's bucket.
 */
#define STRUCT_MAP_BUCKET_SIZE 4096

/**
 * @brief structure to store field list of parsed structs. A struct may have
 * multiple field, and each field may be an array, or a struct. We use a single
 * linked list to store the fields of a struct.
 */
struct struct_field {
    // field name
    sstr_t name;
    // field type id
    int type;
    // 1 if the field is an array, 0 otherwise
    int is_array;
    // the name of field type
    sstr_t type_name;
    // linked list pointer to next field, NULL if this is the last field
    struct struct_field* next;
};

/**
 * @brief structure to store parsed structs. A struct may have multiple fields,
 * we put fields in a linked list.
 */
struct struct_container {
    // struct name
    sstr_t name;
    // field list
    struct struct_field* fields;
};

/**
 * @brief position of string
 */
struct pos {
    int line;
    int col;
    long offset;
};

/**
 * @brief parser context
 *
 */
struct struct_parser {
    // struct name --> struct_field list
    struct hash_map* struct_map;
    // position of string to be parsed
    struct pos pos;
};

// token types of struct definitions, return by next_token()

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

/**
 * @brief token
 *
 */
struct struct_token {
    int type;
    sstr_t txt;
};

/**
 * @brief create and init a struct_parser instance.
 *
 * @return struct struct_parser* if success, NULL otherwise.
 */
struct struct_parser* struct_parser_new();

/**
 * @brief free a struct_parser instance.
 *
 * @param parser struct struct_parser*
 */
void struct_parser_free(struct struct_parser* parser);

/**
 * @brief parse a struct definition file, and store the parsed structs in
 * struct_parser.
 *
 * @param parser context of parser.
 * @param content content of the file.
 * @return int 0 if success, -1 otherwise.
 */
int struct_parser_parse(struct struct_parser* parser, sstr_t content);

#ifdef __cplusplus
}
#endif

#endif  // STRUCT_PARSE_H_
