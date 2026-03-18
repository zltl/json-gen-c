/*
 * lsp_jsonrpc.h - JSON-RPC 2.0 message framing for LSP
 *
 * Handles Content-Length header-based framing on stdin/stdout
 * and lightweight JSON object access for LSP messages.
 */

#ifndef LSP_JSONRPC_H
#define LSP_JSONRPC_H

#include "utils/sstr.h"
#include <stdio.h>

/* Simple JSON value representation for LSP messages. */
enum lsp_json_type {
    LSP_JSON_NULL,
    LSP_JSON_BOOL,
    LSP_JSON_NUMBER,
    LSP_JSON_STRING,
    LSP_JSON_ARRAY,
    LSP_JSON_OBJECT
};

struct lsp_json_value {
    enum lsp_json_type type;
    union {
        int bool_val;
        long number_val;
        sstr_t string_val;
        struct {
            struct lsp_json_value *items;
            int count;
        } array;
        struct {
            char **keys;
            struct lsp_json_value *values;
            int count;
        } object;
    } u;
};

/**
 * @brief Read one JSON-RPC message from stdin.
 * Reads Content-Length header, then the JSON body.
 * @return Allocated sstr_t with JSON body, or NULL on EOF/error.
 */
sstr_t lsp_jsonrpc_read(FILE *in);

/**
 * @brief Write a JSON-RPC message to stdout.
 * Writes Content-Length header followed by the JSON body.
 */
void lsp_jsonrpc_write(FILE *out, sstr_t json_body);

/**
 * @brief Parse a JSON string into an lsp_json_value tree.
 * @return 0 on success, -1 on parse error.
 */
int lsp_json_parse(const char *json, int len, struct lsp_json_value *out);

/**
 * @brief Free an lsp_json_value tree recursively.
 */
void lsp_json_free(struct lsp_json_value *val);

/**
 * @brief Look up a key in a JSON object value.
 * @return Pointer to the value, or NULL if not found or val is not an object.
 */
struct lsp_json_value *lsp_json_get(struct lsp_json_value *val, const char *key);

/**
 * @brief Get a string value from a JSON value.
 * @return The C string, or NULL if val is not a string.
 */
const char *lsp_json_string(struct lsp_json_value *val);

/**
 * @brief Get an integer value from a JSON value.
 * @return The integer, or 0 if val is not a number.
 */
long lsp_json_number(struct lsp_json_value *val);

/**
 * @brief Build a JSON-RPC response message.
 * @param id Request ID (number).
 * @param result_json JSON string for the "result" field (already formatted).
 * @return Allocated sstr_t with the full JSON-RPC response.
 */
sstr_t lsp_jsonrpc_response(long id, const char *result_json);

/**
 * @brief Build a JSON-RPC notification message.
 * @param method The method name.
 * @param params_json JSON string for the "params" field (already formatted).
 * @return Allocated sstr_t with the full JSON-RPC notification.
 */
sstr_t lsp_jsonrpc_notification(const char *method, const char *params_json);

/**
 * @brief Escape a string for JSON output.
 * @param s The string to escape.
 * @param out The sstr_t to append the escaped string to (with surrounding quotes).
 */
void lsp_json_escape_string(const char *s, sstr_t out);

#endif /* LSP_JSONRPC_H */
