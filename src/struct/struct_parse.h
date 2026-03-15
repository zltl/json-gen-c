/**
 * @file struct/struct_parse.h
 * @brief Parse struct definitions. Support scalar types(int, long, float,
 * double, sstr_t), arrays, structs.
 */

#ifndef STRUCT_PARSE_H_
#define STRUCT_PARSE_H_

#include <stddef.h>

#include "utils/diag.h"
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
#define FIELD_TYPE_ENUM 5
#define FIELD_TYPE_STRUCT 6
#define FIELD_TYPE_BOOL 7
#define FIELD_TYPE_MAP 8
#define FIELD_TYPE_INT8 9
#define FIELD_TYPE_INT16 10
#define FIELD_TYPE_INT32 11
#define FIELD_TYPE_INT64 12
#define FIELD_TYPE_UINT8 13
#define FIELD_TYPE_UINT16 14
#define FIELD_TYPE_UINT32 15
#define FIELD_TYPE_UINT64 16
#define FIELD_TYPE_ONEOF 17

#define TYPE_NAME_INT "int"
#define TYPE_NAME_BOOL "bool"
#define TYPE_NAME_SSTR "sstr_t"
#define TYPE_NAME_LONG "long"
#define TYPE_NAME_FLOAT "float"
#define TYPE_NAME_DOUBLE "double"
#define TYPE_NAME_INT8 "int8_t"
#define TYPE_NAME_INT16 "int16_t"
#define TYPE_NAME_INT32 "int32_t"
#define TYPE_NAME_INT64 "int64_t"
#define TYPE_NAME_UINT8 "uint8_t"
#define TYPE_NAME_UINT16 "uint16_t"
#define TYPE_NAME_UINT32 "uint32_t"
#define TYPE_NAME_UINT64 "uint64_t"

/**
 * @brief We use a hash map to store parsed structs, and use the struct name
 * as the key. This is the size of the hash map's bucket.
 */
#define STRUCT_MAP_BUCKET_SIZE 4096

/**
 * @brief structure to store a single enum value (name-index pair).
 * Enum values are stored as a linked list.
 */
struct enum_value {
    sstr_t name;
    int index;
    struct enum_value* next;
};

/**
 * @brief structure to store a parsed enum definition.
 */
struct enum_container {
    sstr_t name;
    struct enum_value* values;
    int count;
    // source position where the enum name was defined
    int name_line;
    int name_col;
    // source filename (not owned)
    const char *filename;
};

/**
 * @brief structure to store a single variant of a oneof (tagged union).
 * Variants are stored as a linked list.
 */
struct oneof_variant {
    sstr_t name;              // variant name (used as tag value string)
    sstr_t struct_type_name;  // the struct type this variant maps to
    int index;                // 0-based variant index
    struct oneof_variant* next;
};

/**
 * @brief structure to store a parsed oneof (tagged union) definition.
 */
struct oneof_container {
    sstr_t name;
    sstr_t tag_field;          // JSON discriminator key (default: "type")
    struct oneof_variant* variants;
    int count;
    // source position where the oneof name was defined
    int name_line;
    int name_col;
    // source filename (not owned)
    const char *filename;
};

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
    // fixed-size array length (>0), 0 means dynamic array
    int array_size;
    // the name of field type
    sstr_t type_name;
    // for FIELD_TYPE_MAP: value type id
    int map_value_type;
    // for FIELD_TYPE_MAP: value type name
    sstr_t map_value_type_name;
    // 1 if field is optional (may be absent from JSON), 0 otherwise
    int is_optional;
    // 1 if field is nullable (JSON value may be null), 0 otherwise
    int is_nullable;
    // JSON key alias (NULL means use field name as-is)
    sstr_t json_name;
    // default value literal (NULL means no default)
    sstr_t default_value;
    // 1 if field has a declared default value
    int has_default;
    // source position where the field was defined
    int line;
    int col;
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
    // source position where the struct name was defined
    int name_line;
    int name_col;
    // source filename (not owned)
    const char *filename;
};

/**
 * @brief position of string
 */
struct pos {
    int line;
    int col;
    long offset;
};

// linked list node for tracking include stack (circular include detection)
struct include_node {
    const char* path;
    struct include_node* parent;
};

/**
 * @brief parser context
 *
 */
struct struct_parser {
    // struct name --> struct_field list
    struct hash_map* struct_map;
    // enum name --> enum_container
    struct hash_map* enum_map;
    // oneof name --> oneof_container
    struct hash_map* oneof_map;
    // position of string to be parsed
    struct pos pos;
    // name of parser
    char *name;
    // diagnostic engine (owned by top-level parser, shared with sub-parsers)
    struct diag_engine *diag;
    // include stack for circular include detection (linked list, not owned)
    struct include_node* include_stack;
};

// token types of struct definitions, return by next_token()

#define TOKEN_LEFT_BRACE '{'
#define TOKEN_RIGHT_BRACE '}'
#define TOKEN_LEFT_BRACKET '['
#define TOKEN_RIGHT_BRACKET ']'
#define TOKEN_SEMICOLON ';'
#define TOKEN_COMMA ','
#define TOKEN_SHARPE '#'
#define TOKEN_AT '@'
#define TOKEN_EQUAL '='
#define TOKEN_STRING 4
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

/**
 * @brief Validate parsed schema for semantic errors.
 *
 * Checks for: undefined type references, duplicate field names,
 * duplicate enum values, and C keyword usage.
 *
 * @param parser Parser with populated struct_map and enum_map.
 * @return 0 if no errors, -1 if validation errors found.
 */
int struct_parser_validate(struct struct_parser* parser);

#ifdef __cplusplus
}
#endif

#endif  // STRUCT_PARSE_H_
