/**
 * @file struct_parse.c
 * @brief parse struct definition file.
 */
#include "struct/struct_parse.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/diag.h"
#include "utils/compat.h"
#include "utils/hash_map.h"
#include "utils/io.h"
#include "utils/sstr.h"
#include "utils/error_codes.h"

/*
    STRUCT_LIST = STRUCT STRUCT_LIST
    STRUCT = "struct" NAME "{" FIELD_LIST "}" | INCLUDE | empty
    FIELD_LIST = FIELD FIELD_LIST
    FIELDS = NAME NAME ";" | NAME NAME "[]" ";"
    INCLUDE = "#include <" NAME ">" | "#include \"" NAME "\""
    
    NAME = [a-zA-Z_][a-zA-Z0-9_]*
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
    field->array_size = 0;
    field->map_value_type = 0;
    field->map_value_type_name = NULL;
    field->is_optional = 0;
    field->is_nullable = 0;
    field->json_name = NULL;
    field->default_value = NULL;
    field->has_default = 0;
    field->is_deprecated = 0;
    field->line = 0;
    field->col = 0;
    return field;
}

static void struct_field_free(struct struct_field* field) {
    if (field) {
        sstr_free(field->name);
        sstr_free(field->type_name);
        sstr_free(field->json_name);
        sstr_free(field->default_value);
    }
    free(field);
}

static void struct_field_free_all(struct struct_field* field) {
    struct struct_field* tmp = NULL;
    while (field) {
        tmp = field->next;
        struct_field_free(field);
        field = tmp;
    }
}

static void struct_field_add(struct struct_field** head,
                             struct struct_field* field) {
    field->next = *head;
    *head = field;
}

static struct struct_container* struct_container_new() {
    struct struct_container* container =
        (struct struct_container*)malloc(sizeof(struct struct_container));
    if (container == NULL) {
        return NULL;
    }
    container->name = NULL;
    container->fields = NULL;
    container->name_line = 0;
    container->name_col = 0;
    container->filename = NULL;
    return container;
}

static void struct_container_free(struct struct_container* container) {
    if (container) {
        if (container->name) {
            sstr_free(container->name);
        }
        struct_field_free_all(container->fields);
        free(container);
    }
}

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

static void enum_value_free(struct enum_value* val) {
    while (val) {
        struct enum_value* next = val->next;
        sstr_free(val->name);
        free(val);
        val = next;
    }
}

static void enum_container_free(struct enum_container* ec) {
    if (ec) {
        sstr_free(ec->name);
        enum_value_free(ec->values);
        free(ec);
    }
}

static void enum_container_value_free(void* ptr) {
    enum_container_free((struct enum_container*)ptr);
}

static void oneof_variant_free(struct oneof_variant* v) {
    while (v) {
        struct oneof_variant* next = v->next;
        sstr_free(v->name);
        sstr_free(v->struct_type_name);
        free(v);
        v = next;
    }
}

static void oneof_container_free(struct oneof_container* oc) {
    if (oc) {
        sstr_free(oc->name);
        sstr_free(oc->tag_field);
        oneof_variant_free(oc->variants);
        free(oc);
    }
}

static void oneof_container_value_free(void* ptr) {
    oneof_container_free((struct oneof_container*)ptr);
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
    parser->enum_map =
        hash_map_new(STRUCT_MAP_BUCKET_SIZE, sstr_key_hash, sstr_key_cmp,
                     sstr_key_free, enum_container_value_free);
    if (parser->enum_map == NULL) {
        hash_map_free(parser->struct_map);
        free(parser);
        return NULL;
    }
    parser->oneof_map =
        hash_map_new(STRUCT_MAP_BUCKET_SIZE, sstr_key_hash, sstr_key_cmp,
                     sstr_key_free, oneof_container_value_free);
    if (parser->oneof_map == NULL) {
        hash_map_free(parser->enum_map);
        hash_map_free(parser->struct_map);
        free(parser);
        return NULL;
    }
    parser->pos.col = 0;
    parser->pos.line = 1;
    parser->pos.offset = 0;
    parser->name = NULL;
    parser->diag = NULL;
    parser->include_stack = NULL;

    return parser;
}

void struct_parser_free(struct struct_parser* parser) {
    if (parser) {
        hash_map_free(parser->struct_map);
        hash_map_free(parser->enum_map);
        hash_map_free(parser->oneof_map);
        if (parser->diag) {
            diag_engine_free(parser->diag);
            parser->diag = NULL;
        }
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
        if (parser->diag) {
            diag_emit(parser->diag, DIAG_ERROR, parser->pos.line,
                      parser->pos.col, "expected identifier");
        }
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
    if (parser->pos.offset >= (long)sstr_length(content)) {
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
            case '@':
                token->type = TOKEN_AT;
                parser->pos.offset = i + 1;
                return TOKEN_AT;
            case '=':
                token->type = TOKEN_EQUAL;
                parser->pos.offset = i + 1;
                return TOKEN_EQUAL;
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
            case ',':
                parser->pos.offset = i + 1;
                token->type = TOKEN_COMMA;
                return TOKEN_COMMA;
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
            case '-':
                // negative number literal: peek ahead for digit
                if (i + 1 < length && isdigit(data[i + 1])) {
                    long start_pos = i;
                    i++;  // skip '-'
                    parser->pos.col++;
                    token->type = TOKEN_INTEGER;
                    while (i < length && (isdigit(data[i]) || data[i] == '.')) {
                        if (data[i] == '.') {
                            token->type = TOKEN_FLOAT;
                        }
                        i++;
                        parser->pos.col++;
                    }
                    token->txt = sstr_of(data + start_pos, i - start_pos);
                    parser->pos.offset = i;
                    return token->type;
                }
                token->type = TOKEN_ERROR;
                parser->pos.offset = i + 1;
                return TOKEN_ERROR;
            default:
                break;
                // identifier
        }
        // get identifier or number
        long start_pos = i;
        parser->pos.offset = i;
        if (isdigit(data[i])) {
            // number literal (integer or float with '.')
            token->type = TOKEN_INTEGER;
            while (i < length && (isdigit(data[i]) || data[i] == '.')) {
                if (data[i] == '.') {
                    token->type = TOKEN_FLOAT;
                }
                i++;
                parser->pos.col++;
            }
            token->txt = sstr_of(data + start_pos, i - start_pos);
            parser->pos.offset = i;
            return token->type;
        }
        if (!isalpha(data[i]) && data[i] != '_') {
            token->type = TOKEN_ERROR;
            parser->pos.offset = i + 1;
            return TOKEN_ERROR;
        }
        token->type = TOKEN_IDENTIFY;
        parser->pos.col--;
        while (i < length && (isalnum(data[i]) || data[i] == '_')) {
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

static const char* token_type_str(struct struct_token* token) {
    switch (token->type) {
        case TOKEN_AT:
            return "@";
        case TOKEN_EQUAL:
            return "=";
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
        case TOKEN_COMMA:
            return ",";
        case TOKEN_EOF:
            return "--EOF--";
        case TOKEN_ERROR:
            return "--ERROR--";
    }
    return "--UNKNOWN--";
}

#define PERROR(parser, fmt, ...)                                          \
    diag_emit(parser->diag, DIAG_ERROR, parser->pos.line, parser->pos.col, \
              fmt, ##__VA_ARGS__)

enum top_level_item_kind {
    TOP_LEVEL_ITEM_INVALID = 0,
    TOP_LEVEL_ITEM_INCLUDE,
    TOP_LEVEL_ITEM_STRUCT,
    TOP_LEVEL_ITEM_ENUM,
    TOP_LEVEL_ITEM_ONEOF,
};

static enum top_level_item_kind top_level_item_kind_from_token(
    struct struct_token* token) {
    if (token->type == TOKEN_SHARPE) {
        return TOP_LEVEL_ITEM_INCLUDE;
    }
    if (token->type != TOKEN_IDENTIFY || token->txt == NULL) {
        return TOP_LEVEL_ITEM_INVALID;
    }
    if (sstr_compare_c(token->txt, "struct") == 0) {
        return TOP_LEVEL_ITEM_STRUCT;
    }
    if (sstr_compare_c(token->txt, "enum") == 0) {
        return TOP_LEVEL_ITEM_ENUM;
    }
    if (sstr_compare_c(token->txt, "oneof") == 0) {
        return TOP_LEVEL_ITEM_ONEOF;
    }
    return TOP_LEVEL_ITEM_INVALID;
}

/* Recovery helpers: skip tokens to synchronization points */
static void skip_to_semicolon(struct struct_parser* parser, sstr_t content,
                              struct struct_token* token) {
    while (token->type != TOKEN_SEMICOLON && token->type != TOKEN_EOF &&
           token->type != TOKEN_RIGHT_BRACE) {
        next_token(parser, content, token);
    }
}

static void skip_to_top_level(struct struct_parser* parser, sstr_t content,
                              struct struct_token* token) {
    /* Skip tokens until we find struct/enum/oneof/# or EOF */
    while (1) {
        int tk = next_token(parser, content, token);
        if (tk == TOKEN_EOF) return;
        if (top_level_item_kind_from_token(token) != TOP_LEVEL_ITEM_INVALID) {
            return;
        }
    }
}

// '#include "filename"'
/**
 * @brief Parse include directive with proper error handling and resource management
 * @param parser Current parser state
 * @param content Content being parsed
 * @param token Current token
 * @return 0 on success, negative on error
 */
static int parse_include_directive(struct struct_parser* parser, sstr_t content,
                                   struct struct_token* token) {
    // Validate input parameters
    if (parser == NULL || content == NULL || token == NULL) {
        return JSON_GEN_ERROR_INVALID_PARAM;
    }
    
    // 'include'
    int tk = next_token(parser, content, token);
    if (tk != TOKEN_IDENTIFY || sstr_compare_c(token->txt, "include") != 0) {
        PERROR(parser, "expected 'include' after '#', found '%s'",
               token_type_str(token));
        return JSON_GEN_ERROR_PARSE;
    }

    // '"filename"', or '<filename>'
    tk = next_token(parser, content, token);
    if (tk != TOKEN_STRING) {
        PERROR(parser, "expected string file name, found '%s'",
               token_type_str(token));
        return JSON_GEN_ERROR_PARSE;
    }

    // filename is relative to current file.
    // get the real path of the file.
    char* filename = sstr_cstr(token->txt);
    if (filename == NULL) {
        PERROR(parser, "invalid filename in include directive");
        return JSON_GEN_ERROR_INVALID_PARAM;
    }
    
    sstr_t file = sstr_new();
    if (file == NULL) {
        PERROR(parser, "memory allocation failed for file path");
        return JSON_GEN_ERROR_MEMORY;
    }
    
    int fname_len = strlen(parser->name);
    while (fname_len >= 0 && !compat_is_path_sep(parser->name[fname_len])) {
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

    // Check for circular includes
    for (struct include_node* node = parser->include_stack; node; node = node->parent) {
        if (strcmp(node->path, sstr_cstr(file)) == 0) {
            PERROR(parser, "circular include detected: \"%s\"", sstr_cstr(file));
            sstr_free(file);
            return JSON_GEN_ERROR_PARSE;
        }
    }

    // read the contents of the file
    sstr_t sub_content = sstr_new();
    if (sub_content == NULL) {
        PERROR(parser, "memory allocation failed for file content");
        sstr_free(file);
        return JSON_GEN_ERROR_MEMORY;
    }
    
    int r = read_file(sstr_cstr(file), sub_content);
    if (r != 0) {
        PERROR(parser, "include file \"%s\" not found", sstr_cstr(file));
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
    sub_parser.enum_map = parser->enum_map;
    sub_parser.oneof_map = parser->oneof_map;
    // Track include stack for circular detection
    struct include_node this_node;
    this_node.path = sstr_cstr(file);
    this_node.parent = parser->include_stack;
    sub_parser.include_stack = &this_node;
    // sub-parser gets its own diag engine for correct filename/source
    sub_parser.diag = diag_engine_new(sstr_cstr(file), sstr_cstr(sub_content),
                                      (long)sstr_length(sub_content));
    r = struct_parser_parse(&sub_parser, sub_content);
    // print sub-parser diagnostics immediately (sub-file context)
    if (sub_parser.diag) {
        diag_print_all(sub_parser.diag, stderr);
        if (diag_has_errors(sub_parser.diag)) {
            r = JSON_GEN_ERROR_PARSE;
        }
        diag_engine_free(sub_parser.diag);
        sub_parser.diag = NULL;
    }
    sstr_free(sub_content);
    sstr_free(file);

    return r;
}

static const char* top_level_item_name(enum top_level_item_kind kind) {
    switch (kind) {
        case TOP_LEVEL_ITEM_STRUCT:
            return "struct";
        case TOP_LEVEL_ITEM_ENUM:
            return "enum";
        case TOP_LEVEL_ITEM_ONEOF:
            return "oneof";
        default:
            return "item";
    }
}

static const char* top_level_item_defined_as_name(enum top_level_item_kind kind) {
    switch (kind) {
        case TOP_LEVEL_ITEM_STRUCT:
            return "a struct";
        case TOP_LEVEL_ITEM_ENUM:
            return "an enum";
        case TOP_LEVEL_ITEM_ONEOF:
            return "a oneof";
        default:
            return "an item";
    }
}

static struct hash_map* top_level_item_map(struct struct_parser* parser,
                                           enum top_level_item_kind kind) {
    switch (kind) {
        case TOP_LEVEL_ITEM_STRUCT:
            return parser->struct_map;
        case TOP_LEVEL_ITEM_ENUM:
            return parser->enum_map;
        case TOP_LEVEL_ITEM_ONEOF:
            return parser->oneof_map;
        default:
            return NULL;
    }
}

static int check_top_level_name_available(struct struct_parser* parser,
                                          sstr_t name,
                                          enum top_level_item_kind kind) {
    void* existing = NULL;
    struct hash_map* current_map = top_level_item_map(parser, kind);

    if (current_map != NULL &&
        hash_map_find(current_map, name, &existing) == HASH_MAP_OK) {
        PERROR(parser, "duplicate %s name '%s'",
               top_level_item_name(kind), sstr_cstr(name));
        return -1;
    }
    if (kind != TOP_LEVEL_ITEM_STRUCT &&
        hash_map_find(parser->struct_map, name, &existing) == HASH_MAP_OK) {
        PERROR(parser, "'%s' already defined as %s", sstr_cstr(name),
               top_level_item_defined_as_name(TOP_LEVEL_ITEM_STRUCT));
        return -1;
    }
    if (kind != TOP_LEVEL_ITEM_ENUM &&
        hash_map_find(parser->enum_map, name, &existing) == HASH_MAP_OK) {
        PERROR(parser, "'%s' already defined as %s", sstr_cstr(name),
               top_level_item_defined_as_name(TOP_LEVEL_ITEM_ENUM));
        return -1;
    }
    if (kind != TOP_LEVEL_ITEM_ONEOF &&
        hash_map_find(parser->oneof_map, name, &existing) == HASH_MAP_OK) {
        PERROR(parser, "'%s' already defined as %s", sstr_cstr(name),
               top_level_item_defined_as_name(TOP_LEVEL_ITEM_ONEOF));
        return -1;
    }
    return 0;
}

static int register_top_level_item(struct struct_parser* parser, sstr_t name,
                                   enum top_level_item_kind kind, void* item) {
    struct hash_map* map = top_level_item_map(parser, kind);
    if (map == NULL) {
        return -1;
    }
    if (check_top_level_name_available(parser, name, kind) != 0) {
        return -1;
    }
    hash_map_insert(map, sstr_dup(name), item);
    return 0;
}

// skip semicolons and commas, return the next meaningful token
static int next_meaningful_token(struct struct_parser* parser, sstr_t content,
                                 struct struct_token* token) {
    int tk = next_token(parser, content, token);
    while (tk == TOKEN_SEMICOLON || tk == TOKEN_COMMA) {
        tk = next_token(parser, content, token);
    }
    return tk;
}

/**
 * @brief Look up a primitive type name and return its FIELD_TYPE_* enum value.
 * @return The corresponding FIELD_TYPE_* value, or -1 if not a primitive type.
 */
static int lookup_primitive_type(sstr_t type_name) {
    static const struct {
        const char* name;
        int type_id;
    } type_table[] = {
        { TYPE_NAME_INT,    FIELD_TYPE_INT },
        { TYPE_NAME_BOOL,   FIELD_TYPE_BOOL },
        { TYPE_NAME_LONG,   FIELD_TYPE_LONG },
        { TYPE_NAME_FLOAT,  FIELD_TYPE_FLOAT },
        { TYPE_NAME_DOUBLE, FIELD_TYPE_DOUBLE },
        { TYPE_NAME_SSTR,   FIELD_TYPE_SSTR },
        { TYPE_NAME_INT8,   FIELD_TYPE_INT8 },
        { TYPE_NAME_INT16,  FIELD_TYPE_INT16 },
        { TYPE_NAME_INT32,  FIELD_TYPE_INT32 },
        { TYPE_NAME_INT64,  FIELD_TYPE_INT64 },
        { TYPE_NAME_UINT8,  FIELD_TYPE_UINT8 },
        { TYPE_NAME_UINT16, FIELD_TYPE_UINT16 },
        { TYPE_NAME_UINT32, FIELD_TYPE_UINT32 },
        { TYPE_NAME_UINT64, FIELD_TYPE_UINT64 },
    };
    for (int i = 0; i < (int)(sizeof(type_table) / sizeof(type_table[0])); i++) {
        if (sstr_compare_c(type_name, type_table[i].name) == 0) {
            return type_table[i].type_id;
        }
    }
    return -1;
}

/**
 * @brief Parse a map<key_type, value_type> field type.
 * Expects the tokenizer to have just consumed "map". Reads the <K,V> parameter.
 * @return 0 on success, -1 on error.
 */
static int parse_map_field(struct struct_parser* parser, sstr_t content,
                           struct struct_token* token, struct struct_field* field,
                           sstr_t type_name) {
    token->txt = NULL;
    // next token should be TOKEN_STRING from <...> (tokenizer treats <> like quotes)
    next_token(parser, content, token);
    if (token->type != TOKEN_STRING) {
        PERROR(parser, "expected '<key_type, value_type>' after 'map'");
        sstr_free(type_name);
        return -1;
    }
    // token->txt contains e.g. "sstr_t, int"
    // find the comma separator
    char* params = sstr_cstr(token->txt);
    char* comma = strchr(params, ',');
    if (comma == NULL) {
        PERROR(parser, "expected ',' in map type parameters");
        sstr_free(type_name);
        return -1;
    }
    // extract and trim key type
    while (*params == ' ') params++;
    char* key_end = comma;
    while (key_end > params && *(key_end - 1) == ' ') key_end--;
    // extract and trim value type
    char* val_start = comma + 1;
    while (*val_start == ' ') val_start++;
    char* val_end = params + sstr_length(token->txt);
    while (val_end > val_start && *(val_end - 1) == ' ') val_end--;

    // validate key type is sstr_t
    int key_len = (int)(key_end - params);
    if (key_len != 6 || strncmp(params, "sstr_t", 6) != 0) {
        PERROR(parser, "map key type must be 'sstr_t', got '%.*s'",
               key_len, params);
        sstr_free(type_name);
        return -1;
    }

    // determine value type
    int val_len = (int)(val_end - val_start);
    sstr_t val_type_name = sstr_of(val_start, val_len);
    int map_val_type = lookup_primitive_type(val_type_name);
    if (map_val_type < 0) {
        void* enum_val = NULL;
        if (hash_map_find(parser->enum_map, val_type_name, &enum_val) == HASH_MAP_OK) {
            map_val_type = FIELD_TYPE_ENUM;
        } else {
            map_val_type = FIELD_TYPE_STRUCT;
        }
    }

    field->type = FIELD_TYPE_MAP;
    field->map_value_type = map_val_type;
    if (map_val_type == FIELD_TYPE_BOOL) {
        field->map_value_type_name = sstr("int");
        sstr_free(val_type_name);
    } else {
        field->map_value_type_name = val_type_name;
    }
    // type_name for map fields: "map" (used for identification)
    field->type_name = type_name;
    sstr_free(token->txt);
    token->txt = NULL;
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

    // parse @json "alias" and @deprecated annotations (order-independent)
    while (tk == TOKEN_AT) {
        tk = next_token(parser, content, token);
        if (tk != TOKEN_IDENTIFY) {
            PERROR(parser, "expected annotation name after '@', found '%s'",
                   token_type_str(token));
            return -1;
        }
        if (sstr_compare_c(token->txt, "json") == 0) {
            sstr_free(token->txt);
            token->txt = NULL;
            tk = next_token(parser, content, token);
            if (tk != TOKEN_STRING) {
                PERROR(parser, "expected string after '@json', found '%s'",
                       token_type_str(token));
                return -1;
            }
            field->json_name = token->txt;
            token->txt = NULL;
        } else if (sstr_compare_c(token->txt, "deprecated") == 0) {
            field->is_deprecated = 1;
            sstr_free(token->txt);
            token->txt = NULL;
        } else {
            PERROR(parser, "unknown annotation '@%s'",
                   sstr_cstr(token->txt));
            return -1;
        }
        tk = next_token(parser, content, token);
    }

    // field type
    if (tk != TOKEN_IDENTIFY) {
        PERROR(parser, "expected type name, found '%s'",
               token_type_str(token));
        return -1;
    }

    if (sstr_length(token->txt) == 0) {
        PERROR(parser, "expected type name, found empty string");
        return -1;
    }

    // parse optional/nullable modifiers before type name
    while (sstr_compare_c(token->txt, "optional") == 0 ||
           sstr_compare_c(token->txt, "nullable") == 0) {
        if (sstr_compare_c(token->txt, "optional") == 0) {
            field->is_optional = 1;
        } else {
            field->is_nullable = 1;
        }
        sstr_free(token->txt);
        token->txt = NULL;
        tk = next_token(parser, content, token);
        if (tk != TOKEN_IDENTIFY) {
            PERROR(parser, "expected type name after modifier, found '%s'",
                   token_type_str(token));
            return -1;
        }
    }

    type_name = token->txt;

    // capture field source position (type token location)
    field->line = parser->pos.line;
    field->col = parser->pos.col;

    // field type MUST not be 'struct', it's a reserved keyword.
    if (sstr_compare_c(type_name, "struct") == 0) {
        PERROR(parser,
               "expected field type, found reserve keyword 'struct'");
        token->txt = NULL;
        sstr_free(type_name);
        return -1;
    }

    // handle map<key_type, value_type> syntax
    if (sstr_compare_c(type_name, "map") == 0) {
        type_id = FIELD_TYPE_MAP;
        if (parse_map_field(parser, content, token, field, type_name) < 0) {
            return -1;
        }
        goto parse_field_name;
    }

    // get type id of typename.
    type_id = lookup_primitive_type(type_name);
    if (type_id < 0) {
        void* enum_val = NULL;
        void* oneof_val = NULL;
        if (hash_map_find(parser->enum_map, type_name, &enum_val) == HASH_MAP_OK) {
            type_id = FIELD_TYPE_ENUM;
        } else if (hash_map_find(parser->oneof_map, type_name, &oneof_val) == HASH_MAP_OK) {
            type_id = FIELD_TYPE_ONEOF;
        } else {
            type_id = FIELD_TYPE_STRUCT;
        }
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
parse_field_name:
    tk = next_token(parser, content, token);
    if (tk != TOKEN_IDENTIFY) {
        PERROR(parser, "expected field name, found '%s'",
               token_type_str(token));
        return -1;
    }
    field->name = token->txt;
    token->txt = NULL;

    // field end or array []
    tk = next_token(parser, content, token);
    if (tk == TOKEN_LEFT_BRACKET) {
        field->is_array = 1;
        tk = next_token(parser, content, token);
        if (token->type == TOKEN_INTEGER) {
            // fixed-size array: parse the size
            long sz = strtol(sstr_cstr(token->txt), NULL, 10);
            if (sz <= 0) {
                PERROR(parser, "array size must be a positive integer");
                return -1;
            }
            field->array_size = (int)sz;
            tk = next_token(parser, content, token);
        }
        if (tk != TOKEN_RIGHT_BRACKET) {
            PERROR(parser, "expected ']', found '%s'",
                   token_type_str(token));
            return -1;
        }

        tk = next_token(parser, content, token);
    }
    // parse default value: = <literal>;
    if (tk == TOKEN_EQUAL) {
        if (field->is_array) {
            PERROR(parser, "default values are not supported for array fields");
            return -1;
        }
        if (field->type == FIELD_TYPE_MAP) {
            PERROR(parser, "default values are not supported for map fields");
            return -1;
        }
        if (field->type == FIELD_TYPE_STRUCT) {
            PERROR(parser, "default values are not supported for struct fields");
            return -1;
        }
        tk = next_token(parser, content, token);
        if (tk != TOKEN_INTEGER && tk != TOKEN_FLOAT &&
            tk != TOKEN_IDENTIFY && tk != TOKEN_STRING) {
            PERROR(parser, "expected default value after '=', found '%s'",
                   token_type_str(token));
            return -1;
        }
        field->default_value = token->txt;
        token->txt = NULL;
        field->has_default = 1;
        tk = next_token(parser, content, token);
    }
    if (tk != TOKEN_SEMICOLON) {
        PERROR(parser, "expected ';', found '%s'", token_type_str(token));
        return -1;
    }
    return 0;
}

// parse an enum body: name '{' value0, value1, ... '}'
// the 'enum' keyword must already be consumed by caller.
static int parse_enum_body(struct struct_parser* parser, sstr_t content,
                           struct struct_token* token) {
    // 'name'
    int tk = next_token(parser, content, token);
    if (tk != TOKEN_IDENTIFY) {
        PERROR(parser, "expected enum name, found '%s'",
               token_type_str(token));
        return -1;
    }
    sstr_t enum_name = token->txt;
    token->txt = NULL;
    int enum_name_line = parser->pos.line;
    int enum_name_col = parser->pos.col;

    // '{'
    tk = next_token(parser, content, token);
    if (tk != TOKEN_LEFT_BRACE) {
        PERROR(parser, "expected '{', found '%s'",
               token_type_str(token));
        sstr_free(enum_name);
        return -1;
    }

    struct enum_container* ec =
        (struct enum_container*)malloc(sizeof(struct enum_container));
    if (ec == NULL) {
        sstr_free(enum_name);
        return -1;
    }
    ec->name = enum_name;
    ec->values = NULL;
    ec->count = 0;
    ec->name_line = enum_name_line;
    ec->name_col = enum_name_col;
    ec->filename = parser->name;

    // parse enum values: [@deprecated] IDENT [, IDENT]* '}'
    int had_error = 0;
    while (1) {
        tk = next_meaningful_token(parser, content, token);
        if (tk == TOKEN_RIGHT_BRACE) {
            break;
        }
        if (tk == TOKEN_EOF) {
            PERROR(parser, "unexpected end of file in enum '%s'",
                   sstr_cstr(enum_name));
            enum_container_free(ec);
            return -1;
        }

        // parse optional @deprecated annotation on enum value
        int value_deprecated = 0;
        if (tk == TOKEN_AT) {
            tk = next_token(parser, content, token);
            if (tk != TOKEN_IDENTIFY ||
                sstr_compare_c(token->txt, "deprecated") != 0) {
                PERROR(parser,
                       "expected 'deprecated' after '@' in enum, found '%s'",
                       token_type_str(token));
                had_error = 1;
                while (token->type != TOKEN_COMMA &&
                       token->type != TOKEN_SEMICOLON &&
                       token->type != TOKEN_RIGHT_BRACE &&
                       token->type != TOKEN_EOF) {
                    next_token(parser, content, token);
                }
                if (token->type == TOKEN_RIGHT_BRACE) break;
                continue;
            }
            value_deprecated = 1;
            sstr_free(token->txt);
            token->txt = NULL;
            tk = next_token(parser, content, token);
        }

        if (tk != TOKEN_IDENTIFY) {
            PERROR(parser, "expected enum value name, found '%s'",
                   token_type_str(token));
            had_error = 1;
            /* skip to next comma, semicolon, or closing brace */
            while (token->type != TOKEN_COMMA &&
                   token->type != TOKEN_SEMICOLON &&
                   token->type != TOKEN_RIGHT_BRACE &&
                   token->type != TOKEN_EOF) {
                next_token(parser, content, token);
            }
            if (token->type == TOKEN_RIGHT_BRACE) break;
            continue;
        }
        struct enum_value* ev =
            (struct enum_value*)malloc(sizeof(struct enum_value));
        if (ev == NULL) {
            enum_container_free(ec);
            return -1;
        }
        ev->name = token->txt;
        token->txt = NULL;
        ev->index = ec->count;
        ev->is_deprecated = value_deprecated;
        ev->next = ec->values;
        ec->values = ev;
        ec->count++;

        // optional comma or semicolon separator
        tk = next_token(parser, content, token);
        if (tk == TOKEN_RIGHT_BRACE) {
            break;
        }
        if (tk != TOKEN_SEMICOLON && tk != TOKEN_COMMA) {
            PERROR(parser, "expected ',' or ';' or '}' in enum, found '%s'",
                   token_type_str(token));
            had_error = 1;
            /* skip to next comma, semicolon, or closing brace */
            while (token->type != TOKEN_COMMA &&
                   token->type != TOKEN_SEMICOLON &&
                   token->type != TOKEN_RIGHT_BRACE &&
                   token->type != TOKEN_EOF) {
                next_token(parser, content, token);
            }
            if (token->type == TOKEN_RIGHT_BRACE) break;
        }
    }

    // reverse the values list so they are in order
    struct enum_value* prev = NULL;
    struct enum_value* cur = ec->values;
    while (cur) {
        struct enum_value* next = cur->next;
        cur->next = prev;
        prev = cur;
        cur = next;
    }
    ec->values = prev;
    // re-index after reversing
    int idx = 0;
    for (struct enum_value* v = ec->values; v; v = v->next) {
        v->index = idx++;
    }

    if (register_top_level_item(parser, ec->name, TOP_LEVEL_ITEM_ENUM, ec) !=
        0) {
        enum_container_free(ec);
        return -1;
    }
    return had_error ? -1 : 0;
}

// parse a oneof body: name '{' [@tag "tagname"] (StructType variant_name ';')+ '}'
// the 'oneof' keyword must already be consumed by caller.
static int parse_oneof_body(struct struct_parser* parser, sstr_t content,
                             struct struct_token* token) {
    // 'name'
    int tk = next_token(parser, content, token);
    if (tk != TOKEN_IDENTIFY) {
        PERROR(parser, "expected oneof name, found '%s'",
               token_type_str(token));
        return -1;
    }
    sstr_t oneof_name = token->txt;
    token->txt = NULL;
    int oneof_name_line = parser->pos.line;
    int oneof_name_col = parser->pos.col;

    // '{'
    tk = next_token(parser, content, token);
    if (tk != TOKEN_LEFT_BRACE) {
        PERROR(parser, "expected '{', found '%s'",
               token_type_str(token));
        sstr_free(oneof_name);
        return -1;
    }

    struct oneof_container* oc =
        (struct oneof_container*)malloc(sizeof(struct oneof_container));
    if (oc == NULL) {
        sstr_free(oneof_name);
        return -1;
    }
    oc->name = oneof_name;
    oc->tag_field = sstr("type");  // default tag field name
    oc->variants = NULL;
    oc->count = 0;
    oc->name_line = oneof_name_line;
    oc->name_col = oneof_name_col;
    oc->filename = parser->name;

    int had_error = 0;
    while (1) {
        tk = next_meaningful_token(parser, content, token);
        if (tk == TOKEN_RIGHT_BRACE) {
            break;
        }
        if (tk == TOKEN_EOF) {
            PERROR(parser, "unexpected end of file in oneof '%s'",
                   sstr_cstr(oneof_name));
            oneof_container_free(oc);
            return -1;
        }

        // @tag "field_name" or @deprecated annotation
        int variant_deprecated = 0;
        if (tk == TOKEN_AT) {
            tk = next_token(parser, content, token);
            if (tk != TOKEN_IDENTIFY) {
                PERROR(parser, "expected annotation name after '@' in oneof, found '%s'",
                       token_type_str(token));
                had_error = 1;
                continue;
            }
            if (sstr_compare_c(token->txt, "tag") == 0) {
                sstr_free(token->txt);
                token->txt = NULL;
                tk = next_token(parser, content, token);
                if (tk != TOKEN_STRING) {
                    PERROR(parser, "expected string after '@tag' in oneof, found '%s'",
                           token_type_str(token));
                    had_error = 1;
                    continue;
                }
                sstr_free(oc->tag_field);
                oc->tag_field = token->txt;
                token->txt = NULL;
                continue;
            } else if (sstr_compare_c(token->txt, "deprecated") == 0) {
                variant_deprecated = 1;
                sstr_free(token->txt);
                token->txt = NULL;
                tk = next_token(parser, content, token);
            } else {
                PERROR(parser, "unknown annotation '@%s' in oneof",
                       sstr_cstr(token->txt));
                had_error = 1;
                continue;
            }
        }

        // expect: StructType variant_name ';'
        if (tk != TOKEN_IDENTIFY) {
            PERROR(parser, "expected struct type name in oneof, found '%s'",
                   token_type_str(token));
            had_error = 1;
            while (token->type != TOKEN_SEMICOLON &&
                   token->type != TOKEN_RIGHT_BRACE &&
                   token->type != TOKEN_EOF) {
                next_token(parser, content, token);
            }
            if (token->type == TOKEN_RIGHT_BRACE) break;
            continue;
        }
        sstr_t struct_type = token->txt;
        token->txt = NULL;

        tk = next_token(parser, content, token);
        if (tk != TOKEN_IDENTIFY) {
            PERROR(parser, "expected variant name in oneof, found '%s'",
                   token_type_str(token));
            sstr_free(struct_type);
            had_error = 1;
            while (token->type != TOKEN_SEMICOLON &&
                   token->type != TOKEN_RIGHT_BRACE &&
                   token->type != TOKEN_EOF) {
                next_token(parser, content, token);
            }
            if (token->type == TOKEN_RIGHT_BRACE) break;
            continue;
        }
        sstr_t variant_name = token->txt;
        token->txt = NULL;

        tk = next_token(parser, content, token);
        if (tk != TOKEN_SEMICOLON) {
            PERROR(parser, "expected ';' after variant declaration, found '%s'",
                   token_type_str(token));
            sstr_free(struct_type);
            sstr_free(variant_name);
            had_error = 1;
            while (token->type != TOKEN_SEMICOLON &&
                   token->type != TOKEN_RIGHT_BRACE &&
                   token->type != TOKEN_EOF) {
                next_token(parser, content, token);
            }
            if (token->type == TOKEN_RIGHT_BRACE) break;
            continue;
        }

        struct oneof_variant* v =
            (struct oneof_variant*)malloc(sizeof(struct oneof_variant));
        if (v == NULL) {
            sstr_free(struct_type);
            sstr_free(variant_name);
            oneof_container_free(oc);
            return -1;
        }
        v->name = variant_name;
        v->struct_type_name = struct_type;
        v->index = oc->count;
        v->is_deprecated = variant_deprecated;
        v->next = oc->variants;
        oc->variants = v;
        oc->count++;
    }

    if (oc->count == 0) {
        PERROR(parser, "oneof '%s' has no variants", sstr_cstr(oneof_name));
        oneof_container_free(oc);
        return -1;
    }

    // reverse the variants list so they are in order
    struct oneof_variant* prev = NULL;
    struct oneof_variant* cur = oc->variants;
    while (cur) {
        struct oneof_variant* next = cur->next;
        cur->next = prev;
        prev = cur;
        cur = next;
    }
    oc->variants = prev;
    // re-index after reversing
    int idx = 0;
    for (struct oneof_variant* v = oc->variants; v; v = v->next) {
        v->index = idx++;
    }

    if (register_top_level_item(parser, oc->name, TOP_LEVEL_ITEM_ONEOF, oc) !=
        0) {
        oneof_container_free(oc);
        return -1;
    }
    return had_error ? -1 : 0;
}

// parse a struct body: name '{' fields '}'
// the 'struct' keyword must already be consumed by caller.
static int parse_struct_body(struct struct_parser* parser, sstr_t content,
                             struct struct_token* token,
                             struct struct_container* sct) {
    // 'name'
    int tk = next_token(parser, content, token);
    if (tk == TOKEN_EOF) {
        return 0;
    }

    if (tk != TOKEN_IDENTIFY) {
        PERROR(parser, "expected struct name, found '%s'",
               token_type_str(token));
        return -1;
    }
    sstr_t struct_name = token->txt;
    sct->name = struct_name;
    sct->name_line = parser->pos.line;
    sct->name_col = parser->pos.col;
    sct->filename = parser->name;
    token->txt = NULL;

    // '{'
    tk = next_token(parser, content, token);
    if (tk != TOKEN_LEFT_BRACE) {
        PERROR(parser, "expected '{', found '%s'", token_type_str(token));
        return -1;
    }

    // struct fields
    int had_error = 0;
    while (token->type != TOKEN_RIGHT_BRACE && token->type != TOKEN_EOF &&
           token->type != TOKEN_ERROR) {
        // field
        struct struct_field* field = struct_field_new(NULL, 0, NULL);
        int r = struct_parse_field(parser, content, token, field);
        if (r != 0) {
            struct_field_free(field);
            had_error = 1;
            // recovery: skip to next semicolon or closing brace
            skip_to_semicolon(parser, content, token);
            continue;
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

    return had_error ? -1 : 0;
}

static int parse_struct_definition(struct struct_parser* parser, sstr_t content,
                                   struct struct_token* token) {
    struct struct_container* sct = struct_container_new();
    if (sct == NULL) {
        return JSON_GEN_ERROR_MEMORY;
    }

    int r = parse_struct_body(parser, content, token, sct);
    if (r != 0) {
        struct_container_free(sct);
        return r;
    }
    if (sct->name == NULL) {
        struct_container_free(sct);
        return 0;
    }

    // we insert fields into sct->fields at the head of the list, so reverse.
    struct_field_reverse(&sct->fields);
    if (register_top_level_item(parser, sct->name, TOP_LEVEL_ITEM_STRUCT, sct) !=
        0) {
        struct_container_free(sct);
        return -1;
    }
    return 0;
}

static int parse_top_level_item(struct struct_parser* parser, sstr_t content,
                                struct struct_token* token) {
    switch (top_level_item_kind_from_token(token)) {
        case TOP_LEVEL_ITEM_INCLUDE:
            return parse_include_directive(parser, content, token);
        case TOP_LEVEL_ITEM_STRUCT:
            return parse_struct_definition(parser, content, token);
        case TOP_LEVEL_ITEM_ENUM:
            return parse_enum_body(parser, content, token);
        case TOP_LEVEL_ITEM_ONEOF:
            return parse_oneof_body(parser, content, token);
        default:
            PERROR(parser,
                   "expected 'struct', 'enum', 'oneof' or '#include', found '%s'",
                   token_type_str(token));
            return JSON_GEN_ERROR_PARSE;
    }
}

// parse a struct definition file.
// return 0 if success, negative if error
// if success, the parser->struct_map will store all structs.
int struct_parser_parse(struct struct_parser* parser, sstr_t content) {
    struct struct_token token;
    token.txt = NULL;
    token.type = 0;
    int had_error = 0;

    // Create diag engine if not already set (top-level parse)
    int owns_diag = 0;
    if (parser->diag == NULL) {
        parser->diag = diag_engine_new(
            parser->name, sstr_cstr(content), (long)sstr_length(content));
        if (parser->diag == NULL) {
            fprintf(stderr, "fatal: failed to allocate diagnostic engine\n");
            return JSON_GEN_ERROR_MEMORY;
        }
        owns_diag = 1;
    }

    while (1) {
        int tk = next_meaningful_token(parser, content, &token);
        if (tk == TOKEN_EOF) {
            break;
        }

        if (top_level_item_kind_from_token(&token) == TOP_LEVEL_ITEM_INVALID) {
            PERROR(parser, "expected 'struct', 'enum', 'oneof' or '#include', found '%s'",
                   token_type_str(&token));
            had_error = 1;
            // recovery: skip to next top-level keyword
            skip_to_top_level(parser, content, &token);
            // Re-check what we landed on
            if (token.type == TOKEN_EOF) {
                break;
            }
        }

        int r = parse_top_level_item(parser, content, &token);
        if (r != 0) {
            // error already recorded; continue parsing
            had_error = 1;
            continue;
        }
    }
    token_clear(&token);

    // Print diagnostics and return error status
    if (owns_diag) {
        diag_print_all(parser->diag, stderr);
    }
    if (had_error || diag_has_errors(parser->diag)) {
        return JSON_GEN_ERROR_PARSE;
    }

    return 0;
}

/* ---- C11 keyword list for identifier validation ---- */
static const char* c_keywords[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "else", "extern", "for", "goto", "if", "inline", "register", "restrict",
    "return", "short", "signed", "sizeof", "static", "switch", "typedef",
    "union", "unsigned", "void", "volatile", "while",
    "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic",
    "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local",
    NULL
};

static int is_c_keyword(const char* name) {
    for (const char** kw = c_keywords; *kw; kw++) {
        if (strcmp(name, *kw) == 0) return 1;
    }
    return 0;
}

/* ---- Validation callbacks for hash_map_for_each ---- */

struct validate_ctx {
    struct diag_engine* diag;
    struct hash_map* struct_map;
    struct hash_map* enum_map;
    struct hash_map* oneof_map;
};

static void validate_struct_fields(void* key, void* value, void* ptr) {
    (void)key;
    struct struct_container* sct = (struct struct_container*)value;
    struct validate_ctx* ctx = (struct validate_ctx*)ptr;
    const char* sname = sstr_cstr(sct->name);

    /* Check for duplicate field names (O(n^2) — fine for typical structs) */
    for (struct struct_field* f = sct->fields; f; f = f->next) {
        for (struct struct_field* g = f->next; g; g = g->next) {
            if (sstr_compare(f->name, g->name) == 0) {
                diag_emit(ctx->diag, DIAG_ERROR, g->line, g->col,
                          "duplicate field name '%s' in struct '%s'",
                          sstr_cstr(f->name), sname);
            }
        }
    }

    /* Check for undefined type references */
    for (struct struct_field* f = sct->fields; f; f = f->next) {
        if (f->type == FIELD_TYPE_STRUCT) {
            void* found = NULL;
            if (hash_map_find(ctx->struct_map, f->type_name, &found) != HASH_MAP_OK) {
                diag_emit(ctx->diag, DIAG_ERROR, f->line, f->col,
                          "use of undefined type '%s' in struct '%s' field '%s'",
                          sstr_cstr(f->type_name), sname, sstr_cstr(f->name));
            }
        }
        if (f->type == FIELD_TYPE_ONEOF) {
            void* found = NULL;
            if (hash_map_find(ctx->oneof_map, f->type_name, &found) != HASH_MAP_OK) {
                diag_emit(ctx->diag, DIAG_ERROR, f->line, f->col,
                          "use of undefined oneof type '%s' in struct '%s' field '%s'",
                          sstr_cstr(f->type_name), sname, sstr_cstr(f->name));
            }
        }
        if (f->type == FIELD_TYPE_MAP && f->map_value_type == FIELD_TYPE_STRUCT) {
            void* found = NULL;
            if (hash_map_find(ctx->struct_map, f->map_value_type_name, &found) != HASH_MAP_OK) {
                diag_emit(ctx->diag, DIAG_ERROR, f->line, f->col,
                          "use of undefined type '%s' in struct '%s' field '%s'",
                          sstr_cstr(f->map_value_type_name), sname, sstr_cstr(f->name));
            }
        }
    }

    /* Check for C keyword usage in field names */
    for (struct struct_field* f = sct->fields; f; f = f->next) {
        if (f->name && is_c_keyword(sstr_cstr(f->name))) {
            diag_emit(ctx->diag, DIAG_WARNING, f->line, f->col,
                      "field name '%s' in struct '%s' is a C keyword",
                      sstr_cstr(f->name), sname);
        }
    }

    /* Check enum default values are valid enum constants */
    for (struct struct_field* f = sct->fields; f; f = f->next) {
        if (f->has_default && f->type == FIELD_TYPE_ENUM) {
            void* found = NULL;
            if (hash_map_find(ctx->enum_map, f->type_name, &found) == HASH_MAP_OK) {
                struct enum_container* ec = (struct enum_container*)found;
                int valid = 0;
                for (struct enum_value* v = ec->values; v; v = v->next) {
                    if (sstr_compare(f->default_value, v->name) == 0) {
                        valid = 1;
                        break;
                    }
                }
                if (!valid) {
                    diag_emit(ctx->diag, DIAG_ERROR, f->line, f->col,
                              "default value '%s' is not a valid constant of enum '%s'",
                              sstr_cstr(f->default_value), sstr_cstr(f->type_name));
                }
            }
        }
    }

    /* Check struct name against C keywords */
    if (sct->name && is_c_keyword(sname)) {
        diag_emit(ctx->diag, DIAG_WARNING, sct->name_line, sct->name_col,
                  "struct name '%s' is a C keyword", sname);
    }
}

static void validate_enum_values(void* key, void* value, void* ptr) {
    (void)key;
    struct enum_container* ec = (struct enum_container*)value;
    struct validate_ctx* ctx = (struct validate_ctx*)ptr;
    const char* ename = sstr_cstr(ec->name);

    /* Check for duplicate enum values (O(n^2) — fine for typical enums) */
    for (struct enum_value* v = ec->values; v; v = v->next) {
        for (struct enum_value* w = v->next; w; w = w->next) {
            if (sstr_compare(v->name, w->name) == 0) {
                diag_emit(ctx->diag, DIAG_ERROR, 0, 0,
                          "duplicate enum value '%s' in enum '%s'",
                          sstr_cstr(v->name), ename);
            }
        }
    }

    /* Check enum name against C keywords */
    if (ec->name && is_c_keyword(ename)) {
        diag_emit(ctx->diag, DIAG_WARNING, ec->name_line, ec->name_col,
                  "enum name '%s' is a C keyword", ename);
    }
}

static void validate_oneof_variants(void* key, void* value, void* ptr) {
    (void)key;
    struct oneof_container* oc = (struct oneof_container*)value;
    struct validate_ctx* ctx = (struct validate_ctx*)ptr;
    const char* oname = sstr_cstr(oc->name);

    /* Check that all variant struct types exist */
    for (struct oneof_variant* v = oc->variants; v; v = v->next) {
        void* found = NULL;
        if (hash_map_find(ctx->struct_map, v->struct_type_name, &found) != HASH_MAP_OK) {
            diag_emit(ctx->diag, DIAG_ERROR, oc->name_line, oc->name_col,
                      "variant '%s' references undefined struct '%s' in oneof '%s'",
                      sstr_cstr(v->name), sstr_cstr(v->struct_type_name), oname);
        }
    }

    /* Check for duplicate variant names */
    for (struct oneof_variant* v = oc->variants; v; v = v->next) {
        for (struct oneof_variant* w = v->next; w; w = w->next) {
            if (sstr_compare(v->name, w->name) == 0) {
                diag_emit(ctx->diag, DIAG_ERROR, oc->name_line, oc->name_col,
                          "duplicate variant name '%s' in oneof '%s'",
                          sstr_cstr(v->name), oname);
            }
        }
    }

    /* Check oneof name against C keywords */
    if (oc->name && is_c_keyword(oname)) {
        diag_emit(ctx->diag, DIAG_WARNING, oc->name_line, oc->name_col,
                  "oneof name '%s' is a C keyword", oname);
    }
}

int struct_parser_validate(struct struct_parser* parser) {
    if (parser == NULL) return 0;

    struct diag_engine* diag = diag_engine_new("<schema>", NULL, 0);
    if (diag == NULL) {
        fprintf(stderr, "fatal: failed to allocate validation diagnostic engine\n");
        return -1;
    }

    struct validate_ctx ctx;
    ctx.diag = diag;
    ctx.struct_map = parser->struct_map;
    ctx.enum_map = parser->enum_map;
    ctx.oneof_map = parser->oneof_map;

    hash_map_for_each(parser->struct_map, validate_struct_fields, &ctx);
    hash_map_for_each(parser->enum_map, validate_enum_values, &ctx);
    hash_map_for_each(parser->oneof_map, validate_oneof_variants, &ctx);

    int has_errors = diag_has_errors(diag);
    if (diag->count > 0) {
        diag_print_all(diag, stderr);
    }
    diag_engine_free(diag);

    return has_errors ? -1 : 0;
}
