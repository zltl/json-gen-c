/**
 * @file struct_parse.c
 * @brief parse struct definition file.
 */
#include "struct/struct_parse.h"

#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "utils/hash_map.h"
#include "utils/io.h"
#include "utils/sstr.h"
#include "utils/error_codes.h"

/*
    Grammar for struct definition parsing:
    
    STRUCT_LIST = STRUCT STRUCT_LIST
    STRUCT = "struct" NAME "{" FIELD_LIST "}" | INCLUDE | empty
    FIELD_LIST = FIELD FIELD_LIST
    FIELDS = NAME NAME ";" | NAME NAME "[]" ";"
    INCLUDE = "#include <" NAME ">" | "#include \"" NAME "\""
    
    NAME = [a-zA-Z_][a-zA-Z0-9_]*
    
    This defines a simple grammar for C-style struct definitions with support
    for scalar types, arrays, nested structs, and include directives.
*/

/**
 * @brief Create a new struct field node
 * 
 * Allocates and initializes a struct_field node for a linked list of fields.
 * Used during parsing to build up the field list for a struct definition.
 * 
 * @param name      Field name (ownership transferred)
 * @param type      Field type ID (FIELD_TYPE_INT, etc.)
 * @param type_name Type name string (ownership transferred)
 * @return Newly allocated field, or NULL on allocation failure
 */
static struct struct_field* struct_field_new(sstr_t name, int type,
                                             sstr_t type_name) {
    struct struct_field* field =
        (struct struct_field*)malloc(sizeof(struct struct_field));
    if (field == NULL) {
        return NULL;
    }
    field->name = name;
    field->type = type;
    field->type_name = type_name;
    field->next = NULL;
    field->is_array = 0;
    return field;
}

/**
 * @brief Free a single struct field and its strings
 * 
 * Deallocates the field node and its associated name and type_name strings.
 * Safe to call with NULL field pointer.
 * 
 * @param field Field to free (can be NULL)
 */
static void struct_field_free(struct struct_field* field) {
    if (field) {
        sstr_free(field->name);
        sstr_free(field->type_name);
    }
    free(field);
}

/**
 * @brief Free all fields in a linked list
 * 
 * Traverses the field list and frees each node and its associated data.
 * Handles NULL safely by doing nothing.
 * 
 * @param field Head of field list to free (can be NULL)
 */
static void struct_field_free_all(struct struct_field* field) {
    struct struct_field* tmp = NULL;
    while (field) {
        tmp = field->next;
        struct_field_free(field);
        field = tmp;
    }
}

/**
 * @brief Add a field to the head of a field list
 * 
 * Inserts a field at the beginning of the linked list. This creates
 * a reverse order (last field added is first in list), which is later
 * reversed to restore original order.
 * 
 * @param head  Pointer to list head (modified)
 * @param field Field to add at the head
 */
static void struct_field_add(struct struct_field** head,
                             struct struct_field* field) {
    field->next = *head;
    *head = field;
}

/**
 * @brief Create a new struct container
 * 
 * Allocates a container to hold a parsed struct definition including
 * its name and list of fields. Used as values in the struct hash map.
 * 
 * @return New container, or NULL on allocation failure
 */
static struct struct_container* struct_container_new() {
    struct struct_container* container =
        (struct struct_container*)malloc(sizeof(struct struct_container));
    if (container == NULL) {
        return NULL;
    }
    container->name = NULL;
    container->fields = NULL;
    return container;
}

/**
 * @brief Free a struct container and all its contents
 * 
 * Deallocates the container, its name string, and all fields in its
 * field list. Safe to call with NULL container.
 * 
 * @param container Container to free (can be NULL)
 */
static void struct_container_free(struct struct_container* container) {
    if (container) {
        if (container->name) {
            sstr_free(container->name);
        }
        struct_field_free_all(container->fields);
        free(container);
    }
}

/**
 * @brief Reverse a linked list of struct fields
 * 
 * Reverses the field list to restore original declaration order.
 * During parsing, fields are added to the head for efficiency,
 * but we need them in the original order for code generation.
 * 
 * @param head Pointer to list head (modified to point to new head)
 */
static void struct_field_reverse(struct struct_field** head) {
    struct struct_field* prev = NULL;
    struct struct_field* cur = *head;
    struct struct_field* next = NULL;
    while (cur) {
        next = cur->next;
        cur->next = prev;
        prev = cur;
        cur = next;
    }
    *head = prev;
}

static void struct_container_value_free(void* container) {
    struct struct_container* c = (struct struct_container*)container;
    struct_container_free(c);
}

struct struct_parser* struct_parser_new() {
    struct struct_parser* parser =
        (struct struct_parser*)malloc(sizeof(struct struct_parser));
    if (parser == NULL) {
        return NULL;
    }
    parser->struct_map =
        hash_map_new(STRUCT_MAP_BUCKET_SIZE, sstr_key_hash, sstr_key_cmp,
                     sstr_key_free, struct_container_value_free);
    if (parser->struct_map == NULL) {
        free(parser);
        return NULL;
    }
    parser->pos.col = 0;
    parser->pos.line = 1;
    parser->pos.offset = 0;
    parser->name = NULL;

    return parser;
}

void struct_parser_free(struct struct_parser* parser) {
    if (parser) {
        hash_map_free(parser->struct_map);
        free(parser);
    }
}

static void token_clear(struct struct_token* token) {
    if (token) {
        if (token->txt) {
            sstr_free(token->txt);
        }
        token->txt = NULL;
    }
}

static void ptoken(struct struct_parser* parser, struct struct_token* token) {
    int tk = token->type;
    if (tk == TOKEN_ERROR) {
        fprintf(stderr, "error at line %d, col %d, expected identifier\n",
                parser->pos.line, parser->pos.col);
        return;
    }
#ifdef JSON_DEBUG
    if (tk == TOKEN_EOF) {
        printf("TOKEN>EOF, file=%s\n", parser->name);
        return;
    }

    if (tk == TOKEN_IDENTIFY || tk == TOKEN_INTEGER || tk == TOKEN_FLOAT) {
        printf("TOKEN>\'%s\', file=%s, line=%d, col=%d\n", parser->name,
               sstr_cstr(token->txt), parser->pos.line, parser->pos.col);
    } else {
        printf("TOKEN>\'%c\', file=%s, line=%d, col=%d\n", tk, parser->name,
               parser->pos.line, parser->pos.col);
    }
#endif  // JSON_DEUBG
}

static int next_token_(struct struct_parser* parser, sstr_t content,
                       struct struct_token* token) {
    long i = 0;
    if (parser->pos.offset >= (long)sstr_length(content) - 1) {
        parser->pos.offset++;
        token->type = TOKEN_EOF;
        return TOKEN_EOF;
    }
    char* data = sstr_cstr(content);
    long length = (long)sstr_length(content);
    for (i = parser->pos.offset; i < length; ++i) {
        parser->pos.offset = i;

        if (data[i] == '\n') {
            parser->pos.line++;
            parser->pos.col = 0;
        } else {
            parser->pos.col++;
        }
        switch (data[i]) {
            case '#':
                token->type = TOKEN_SHARPE;
                parser->pos.offset = i + 1;
                return TOKEN_SHARPE;
            case '\"': {
                token->type = TOKEN_STRING;
                parser->pos.offset = i + 1;
                int start_pos = parser->pos.offset;
                int end_pos = start_pos;
                for (; end_pos < length; ++end_pos) {
                    parser->pos.col++;
                    if (data[end_pos] == '\"') {
                        parser->pos.offset = end_pos + 1;
                        token->txt =
                            sstr_of(data + start_pos, end_pos - start_pos);
                        return TOKEN_STRING;
                    }
                }
                token->type = TOKEN_ERROR;
                parser->pos.offset = end_pos;
                return TOKEN_ERROR;
            }
            case '<': {
                token->type = TOKEN_STRING;
                parser->pos.offset = i + 1;
                int start_pos = parser->pos.offset;
                int end_pos = start_pos;
                for (; end_pos < length; ++end_pos) {
                    parser->pos.col++;
                    if (data[end_pos] == '>') {
                        parser->pos.offset++;
                        token->txt =
                            sstr_of(data + start_pos, end_pos - start_pos);
                        return TOKEN_STRING;
                    }
                }
                token->type = TOKEN_ERROR;
                parser->pos.offset = end_pos;
                return TOKEN_ERROR;
            }
            case '{':
                token->type = TOKEN_LEFT_BRACE;
                parser->pos.offset = i + 1;
                return TOKEN_LEFT_BRACE;
            case '}':
                token->type = TOKEN_RIGHT_BRACE;
                parser->pos.offset = i + 1;
                return TOKEN_RIGHT_BRACE;
            case '[':
                token->type = TOKEN_LEFT_BRACKET;
                parser->pos.offset = i + 1;
                return TOKEN_LEFT_BRACKET;
            case ']':
                token->type = TOKEN_RIGHT_BRACKET;
                parser->pos.offset = i + 1;
                return TOKEN_RIGHT_BRACKET;
            case ';':
                parser->pos.offset = i + 1;
                token->type = TOKEN_SEMICOLON;
                return TOKEN_SEMICOLON;
            case '/':
                if (i + 1 < length && data[i + 1] == '/') {
                    i += 2;  // skip //
                    // comment
                    while (i < length && data[i] != '\n') {
                        i++;
                    }
                    parser->pos.col = 0;
                    parser->pos.line++;
                    parser->pos.offset = i;
                    continue;
                } else if (i + 1 < length && data[i + 1] == '*') {
                    i += 2;  // skip /*
                    // comment
                    parser->pos.col += 1;
                    while (i + 1 < length &&
                           (data[i] != '*' || data[i + 1] != '/')) {
                        if (data[i] == '\n') {
                            parser->pos.line++;
                            parser->pos.col = 0;
                        } else {
                            parser->pos.col++;
                        }
                        i++;
                    }
                    i += 2;
                    parser->pos.col += 2;
                    parser->pos.offset = i;
                    continue;
                }
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                continue;
            default:
                break;
                // identifier
        }
        // get identifier or number
        long start_pos = i;
        token->type = TOKEN_INTEGER;
        parser->pos.offset = i;
        if (!isalnum(data[i]) && data[i] != '_') {
            token->type = TOKEN_ERROR;
            return TOKEN_ERROR;
        }
        parser->pos.col--;
        while (i < (long)sstr_length(content) &&
               (isalnum(data[i]) || data[i] == '_')) {
            if (token->type == TOKEN_INTEGER && data[i] == '.') {
                token->type = TOKEN_FLOAT;
            }
            if (isalpha(data[i])) {
                token->type = TOKEN_IDENTIFY;
            }
            i++;
            parser->pos.col++;
        }
        token->txt = sstr_of(data + start_pos, i - start_pos);
        parser->pos.offset = i;
        return TOKEN_IDENTIFY;
    }
    token->type = TOKEN_EOF;
    return TOKEN_EOF;
}

static int next_token(struct struct_parser* parser, sstr_t content,
                      struct struct_token* token) {
    token_clear(token);
    int tk = next_token_(parser, content, token);
    ptoken(parser, token);
    return tk;
}

static char* token_type_str(struct struct_token* token) {
    switch (token->type) {
        case TOKEN_SHARPE:
            return "#";
        case TOKEN_STRING:
            return sstr_cstr(token->txt);
        case TOKEN_IDENTIFY:
            return sstr_cstr(token->txt);
        case TOKEN_LEFT_BRACE:
            return "{";
        case TOKEN_RIGHT_BRACE:
            return "}";
        case TOKEN_LEFT_BRACKET:
            return "[";
        case TOKEN_RIGHT_BRACKET:
            return "]";
        case TOKEN_SEMICOLON:
            return ";";
        case TOKEN_EOF:
            return "--EOF--";
        case TOKEN_ERROR:
            return "--ERROR--";
    }
    return "--UNKNOWN--";
}

#define PERROR(parser, fmt, ...)                                    \
    fprintf(stderr, "file %s, line %d, col %d: " fmt, parser->name, \
            parser->pos.line, parser->pos.col, ##__VA_ARGS__)

// '#include "filename"'
/**
 * @brief Parse include directive with proper error handling and resource management
 * @param parser Current parser state
 * @param content Content being parsed
 * @param token Current token
 * @return 0 on success, negative on error
 */
static int struct_parse_include(struct struct_parser* parser, sstr_t content,
                                struct struct_token* token) {
    // Validate input parameters
    if (parser == NULL || content == NULL || token == NULL) {
        return JSON_GEN_ERROR_INVALID_PARAM;
    }
    
    // 'include'
    int tk = next_token(parser, content, token);
    if (tk != TOKEN_IDENTIFY || sstr_compare_c(token->txt, "include") != 0) {
        PERROR(parser, "expect #include, but found %s\n",
               token_type_str(token));
        return JSON_GEN_ERROR_PARSE;
    }

    // '"filename"', or '<filename>'
    tk = next_token(parser, content, token);
    if (tk != TOKEN_STRING) {
        PERROR(parser, "expect string file name, but found %s\n",
               token_type_str(token));
        return JSON_GEN_ERROR_PARSE;
    }

    // filename is relative to current file.
    // get the real path of the file.
    char* filename = sstr_cstr(token->txt);
    if (filename == NULL) {
        PERROR(parser, "invalid filename in include directive\n");
        return JSON_GEN_ERROR_INVALID_PARAM;
    }
    
    sstr_t file = sstr_new();
    if (file == NULL) {
        PERROR(parser, "memory allocation failed for file path\n");
        return JSON_GEN_ERROR_MEMORY;
    }
    
    int fname_len = strlen(parser->name);
    while (fname_len >= 0 && parser->name[fname_len] != '/') {
        fname_len--;
    }
    if (fname_len >= 0) {  // Fixed: was '!0' which is always true
        sstr_append_of(file, parser->name, fname_len + 1);  // include the '/'
    }
    sstr_append_cstr(file, filename);
    
    // Clean up token text
    if (token->txt) {
        sstr_free(token->txt);
        token->txt = NULL;
    }

    // read the contents of the file
    sstr_t sub_content = sstr_new();
    if (sub_content == NULL) {
        PERROR(parser, "memory allocation failed for file content\n");
        sstr_free(file);
        return JSON_GEN_ERROR_MEMORY;
    }
    
    int r = read_file(sstr_cstr(file), sub_content);
    if (r != 0) {
        PERROR(parser, "include file \"%s\" not found\n", sstr_cstr(file));
        sstr_free(sub_content);
        sstr_free(file);
        return JSON_GEN_ERROR_FILE_IO;
    }

    // then parse it.
    struct struct_parser sub_parser;
    sub_parser.name = sstr_cstr(file);
    sub_parser.pos.col = 0;
    sub_parser.pos.line = 1;
    sub_parser.pos.offset = 0;
    // put the structs into the upper parser->struct_map also.
    sub_parser.struct_map = parser->struct_map;
    // then we step into the sub file.
    r = struct_parser_parse(&sub_parser, sub_content);
    sstr_free(sub_content);
    sstr_free(file);

    return r;
}

// parse 'struct' or '#include ' statement
static int parse_keyword_struct_or_include(struct struct_parser* parser,
                                           sstr_t content,
                                           struct struct_token* token) {
    int tk = 0;

    tk = next_token(parser, content, token);
    while (tk == TOKEN_SEMICOLON) {
        tk = next_token(parser, content, token);
    }
    if (tk == TOKEN_EOF) {
        return 0;
    }

    if (tk == TOKEN_SHARPE) {
        // try parse '#include "filename"'
        // TODO: look wierd on this function, maybe need to be refactor
        return struct_parse_include(parser, content, token);
    }

    // else parse 'struct' keyword
    if (tk != TOKEN_IDENTIFY) {
        PERROR(parser, "expected \'struct\', found \'%s\'\n",
               token_type_str(token));
        return -1;
    }
    if (sstr_compare_c(token->txt, "struct") != 0) {
        PERROR(parser, "expected \'struct\', found \'%s\'\n",
               sstr_cstr(token->txt));
        return -1;
    }
    return 0;
}

static int struct_parse_field(struct struct_parser* parser, sstr_t content,
                              struct struct_token* token,
                              struct struct_field* field) {
    sstr_t type_name = NULL;
    int type_id = 0;

    // ignore ';'
    int tk = next_token(parser, content, token);
    while (tk == TOKEN_SEMICOLON) {
        tk = next_token(parser, content, token);
    }

    // return if end of struct
    if (tk == TOKEN_RIGHT_BRACE) {
        return 0;
    }

    // field type
    if (tk != TOKEN_IDENTIFY) {
        PERROR(parser, "expected type name, found \'%s\'\n",
               token_type_str(token));
        return -1;
    }

    if (sstr_length(token->txt) == 0) {
        PERROR(parser, "expected type name, found empty string\n");
        return -1;
    }
    type_name = token->txt;

    // field type MUST not be 'struct', it's a reserved keyword.
    if (sstr_compare_c(type_name, "struct") == 0) {
        PERROR(parser,
               "expected field type, found reserve keyworkd 'struct'\n");
        sstr_free(type_name);
        return -1;
    }

    // get type id of typename.
    if (sstr_compare_c(type_name, TYPE_NAME_INT) == 0) {
        type_id = FIELD_TYPE_INT;
    } else if (sstr_compare_c(type_name, TYPE_NAME_BOOL) == 0) {
        type_id = FIELD_TYPE_BOOL;
    } else if (sstr_compare_c(type_name, TYPE_NAME_LONG) == 0) {
        type_id = FIELD_TYPE_LONG;
    } else if (sstr_compare_c(type_name, TYPE_NAME_FLOAT) == 0) {
        type_id = FIELD_TYPE_FLOAT;
    } else if (sstr_compare_c(type_name, TYPE_NAME_DOUBLE) == 0) {
        type_id = FIELD_TYPE_DOUBLE;
    } else if (sstr_compare_c(type_name, TYPE_NAME_SSTR) == 0) {
        type_id = FIELD_TYPE_SSTR;
    } else {
        type_id = FIELD_TYPE_STRUCT;
    }
    field->type = type_id;
    // treat bool as int, because no 'bool' scalar type in C.
    if (type_id == FIELD_TYPE_BOOL) {
        field->type_name = sstr("int");
    } else {
        field->type_name = type_name;
        token->txt = NULL;
    }

    // field name
    tk = next_token(parser, content, token);
    if (tk != TOKEN_IDENTIFY) {
        PERROR(parser, "expected field name, found \'%s\'\n",
               token_type_str(token));
        sstr_free(type_name);
        return -1;
    }
    field->name = token->txt;
    token->txt = NULL;

    // field end or array []
    tk = next_token(parser, content, token);
    if (tk == TOKEN_LEFT_BRACKET) {
        field->is_array = 1;
        tk = next_token(parser, content, token);
        if (tk == TOKEN_INTEGER) {
            // ignore array size
            tk = next_token(parser, content, token);
        }
        if (tk != TOKEN_RIGHT_BRACKET) {
            PERROR(parser, "expected \']\', found \'%s\'\n",
                   token_type_str(token));
            return -1;
        }

        tk = next_token(parser, content, token);
    }
    if (tk != TOKEN_SEMICOLON) {
        PERROR(parser, "expected \';\', found \'%s\'\n", token_type_str(token));
        return -1;
    }
    return 0;
}

// parse a struct
// or #include statement
static int parse_struct(struct struct_parser* parser, sstr_t content,
                        struct struct_token* token,
                        struct struct_container* sct) {
    // 'struct', or '#include'
    int r = parse_keyword_struct_or_include(parser, content, token);
    if (r != 0) {
        return r;
    }
    if (token->type == TOKEN_EOF) {
        return 0;
    }

    // 'name'
    int tk = next_token(parser, content, token);
    if (tk == TOKEN_EOF) {
        return 0;
    }

    if (tk != TOKEN_IDENTIFY) {
        PERROR(parser, "expected struct name, found \'%s\'\n",
               token_type_str(token));
        return -1;
    }
    sstr_t struct_name = token->txt;
    sct->name = struct_name;
    token->txt = NULL;

    // '{'
    tk = next_token(parser, content, token);
    if (tk != TOKEN_LEFT_BRACE) {
        PERROR(parser, "expected \'{\', found \'%s\'\n", token_type_str(token));
        return -1;
    }

    // struct fields
    while (token->type != TOKEN_RIGHT_BRACE && token->type != TOKEN_EOF &&
           token->type != TOKEN_ERROR) {
        // field
        struct struct_field* field = struct_field_new(NULL, 0, NULL);
        r = struct_parse_field(parser, content, token, field);
        if (r != 0) {
            struct_field_free(field);
            return r;
        }
        if (token->type == TOKEN_RIGHT_BRACE) {
            struct_field_free(field);
            break;
        }
        struct_field_add(&sct->fields, field);
#ifdef JSON_DEBUG
        printf("%s: inserting field: field name=%s typename=%s\n",
               sstr_cstr(struct_name), sstr_cstr(field->name),
               sstr_cstr(field->type_name));
#endif
    }
    // fields until '}'

    return 0;
}

// parse a struct definition file.
// return 0 if success, -1 if error
// if success, the parser->struct_map will store all structs.
int struct_parser_parse(struct struct_parser* parser, sstr_t content) {
    struct struct_token token;
    token.txt = NULL;
    token.type = 0;
    token.txt = NULL;

    do {
        // start parsing 'struct aa{ type field; type field; };'

        struct struct_container* sct = struct_container_new();
        int r = parse_struct(parser, content, &token, sct);
        if (r != 0) {
            struct_container_free(sct);
            return r;
        }
        if (sct->name) {
            // we insert fields into sct->fields to the head of the list,
            // so the list is reversed.
            struct_field_reverse(&sct->fields);
            // than insert a struct into parser->struct_map;
            hash_map_insert(parser->struct_map, sstr_dup(sct->name), sct);
        } else {
            struct_container_free(sct);
        }
    } while (token.type != TOKEN_EOF);
    token_clear(&token);

    return 0;
}
