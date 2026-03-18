/*
 * lsp_server.c - LSP server implementation for json-gen-c schema files
 *
 * Provides real-time diagnostics, completions, and hover info
 * for .json-gen-c schema files via Language Server Protocol.
 */

#define _POSIX_C_SOURCE 200809L

#include "lsp/lsp_server.h"
#include "lsp/lsp_jsonrpc.h"
#include "struct/struct_parse.h"
#include "utils/compat.h"
#include "utils/diag.h"
#include "utils/sstr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Document store (single open document for simplicity) ---- */

struct lsp_document {
    char *uri;
    sstr_t content;
};

struct lsp_state {
    struct lsp_document doc;
    int initialized;
    int shutdown_requested;
    int should_exit;
};

static void lsp_state_init(struct lsp_state *state)
{
    memset(state, 0, sizeof(*state));
}

static void lsp_state_cleanup(struct lsp_state *state)
{
    free(state->doc.uri);
    state->doc.uri = NULL;
    if (state->doc.content) {
        sstr_free(state->doc.content);
        state->doc.content = NULL;
    }
}

/* ---- URI to file path conversion ---- */

static char *uri_to_path(const char *uri)
{
    if (strncmp(uri, "file://", 7) == 0)
        return compat_strdup(uri + 7);
    return compat_strdup(uri);
}

/* ---- Diagnostic publishing ---- */

static int lsp_severity_from_diag(enum diag_severity sev)
{
    switch (sev) {
    case DIAG_ERROR:   return 1;
    case DIAG_WARNING: return 2;
    case DIAG_NOTE:    return 3;
    }
    return 1;
}

static void publish_diagnostics(FILE *out, const char *uri,
                                struct diag_engine *diag)
{
    sstr_t params = sstr_new();
    sstr_append_cstr(params, "{\"uri\":");
    lsp_json_escape_string(uri, params);
    sstr_append_cstr(params, ",\"diagnostics\":[");

    int count = diag ? diag->count : 0;
    for (int i = 0; i < count; i++) {
        struct diag_entry *e = &diag->entries[i];
        if (i > 0)
            sstr_append_of(params, ",", 1);

        int line = (e->line > 0) ? e->line - 1 : 0;
        int col = (e->col > 0) ? e->col - 1 : 0;

        sstr_append_cstr(params, "{\"range\":{\"start\":{\"line\":");
        sstr_append_int_str(params, line);
        sstr_append_cstr(params, ",\"character\":");
        sstr_append_int_str(params, col);
        sstr_append_cstr(params, "},\"end\":{\"line\":");
        sstr_append_int_str(params, line);
        sstr_append_cstr(params, ",\"character\":");
        sstr_append_int_str(params, col + 1);
        sstr_append_cstr(params, "}},\"severity\":");
        sstr_append_int_str(params, lsp_severity_from_diag(e->severity));
        sstr_append_cstr(params, ",\"source\":\"json-gen-c\",\"message\":");
        lsp_json_escape_string(e->message, params);
        sstr_append_of(params, "}", 1);
    }

    sstr_append_cstr(params, "]}");

    sstr_t msg = lsp_jsonrpc_notification(
        "textDocument/publishDiagnostics", sstr_cstr(params));
    lsp_jsonrpc_write(out, msg);

    sstr_free(params);
    sstr_free(msg);
}

/* ---- Schema validation ---- */

static void validate_document(FILE *out, struct lsp_state *state)
{
    if (!state->doc.uri || !state->doc.content)
        return;

    /* Create parser and pre-set diag engine to suppress auto-printing. */
    struct struct_parser *parser = struct_parser_new();
    if (!parser) {
        publish_diagnostics(out, state->doc.uri, NULL);
        return;
    }

    char *filepath = uri_to_path(state->doc.uri);
    parser->name = compat_strdup(filepath ? filepath : "<buffer>");
    parser->diag = diag_engine_new(
        parser->name,
        sstr_cstr(state->doc.content),
        (long)sstr_length(state->doc.content));
    free(filepath);

    if (!parser->diag) {
        struct_parser_free(parser);
        publish_diagnostics(out, state->doc.uri, NULL);
        return;
    }

    /* Parse (errors collected in parser->diag, not printed). */
    struct_parser_parse(parser, state->doc.content);

    /* Run semantic validation (suppress its stderr printing). */
    struct_parser_validate_to(parser, NULL);

    /* Publish collected diagnostics. */
    publish_diagnostics(out, state->doc.uri, parser->diag);

    struct_parser_free(parser);
}

/* ---- Completion items ---- */

static const char *keyword_completions[] = {
    "struct", "enum", "oneof", "optional", "nullable",
    "map", "#include", NULL
};

static const char *type_completions[] = {
    "int", "long", "float", "double", "bool", "sstr_t",
    "int8_t", "int16_t", "int32_t", "int64_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t", NULL
};

static const char *annotation_completions[] = {
    "@json", "@tag", "@deprecated", NULL
};

static sstr_t build_completion_response(long id)
{
    sstr_t result = sstr_new();
    sstr_append_of(result, "[", 1);

    int first = 1;

    /* Keywords (kind=14 = Keyword) */
    for (int i = 0; keyword_completions[i]; i++) {
        if (!first) sstr_append_of(result, ",", 1);
        first = 0;
        sstr_append_cstr(result, "{\"label\":");
        lsp_json_escape_string(keyword_completions[i], result);
        sstr_append_cstr(result, ",\"kind\":14}");
    }

    /* Types (kind=21 = TypeParameter) */
    for (int i = 0; type_completions[i]; i++) {
        if (!first) sstr_append_of(result, ",", 1);
        first = 0;
        sstr_append_cstr(result, "{\"label\":");
        lsp_json_escape_string(type_completions[i], result);
        sstr_append_cstr(result, ",\"kind\":21}");
    }

    /* Annotations (kind=15 = Snippet) */
    for (int i = 0; annotation_completions[i]; i++) {
        if (!first) sstr_append_of(result, ",", 1);
        first = 0;
        sstr_append_cstr(result, "{\"label\":");
        lsp_json_escape_string(annotation_completions[i], result);
        sstr_append_cstr(result, ",\"kind\":15}");
    }

    sstr_append_of(result, "]", 1);

    sstr_t resp = lsp_jsonrpc_response(id, sstr_cstr(result));
    sstr_free(result);
    return resp;
}

/* ---- Hover support ---- */

static sstr_t build_hover_response(long id, struct lsp_state *state,
                                   struct lsp_json_value *params)
{
    (void)state;
    (void)params;
    /* TODO: implement type-aware hover by resolving word at cursor
     * against parsed struct/enum/oneof names. For now, return null. */
    return lsp_jsonrpc_response(id, "null");
}

/* ---- Initialize response ---- */

static sstr_t build_initialize_response(long id)
{
    const char *caps =
        "{"
        "\"capabilities\":{"
            "\"textDocumentSync\":{"
                "\"openClose\":true,"
                "\"change\":1,"  /* Full sync */
                "\"save\":{\"includeText\":true}"
            "},"
            "\"completionProvider\":{"
                "\"triggerCharacters\":[\"@\",\"#\",\"<\"]"
            "},"
            "\"hoverProvider\":true"
        "},"
        "\"serverInfo\":{"
            "\"name\":\"json-gen-c-lsp\","
            "\"version\":\"0.9.0\""
        "}"
        "}";
    return lsp_jsonrpc_response(id, caps);
}

/* ---- Request dispatch ---- */

static void handle_message(FILE *out, struct lsp_state *state,
                           struct lsp_json_value *msg)
{
    const char *method = lsp_json_string(lsp_json_get(msg, "method"));
    struct lsp_json_value *id_val = lsp_json_get(msg, "id");
    long id = id_val ? lsp_json_number(id_val) : -1;
    struct lsp_json_value *params = lsp_json_get(msg, "params");

    if (!method)
        return;

    if (strcmp(method, "initialize") == 0) {
        sstr_t resp = build_initialize_response(id);
        lsp_jsonrpc_write(out, resp);
        sstr_free(resp);
        state->initialized = 1;
        return;
    }

    if (strcmp(method, "initialized") == 0) {
        /* Client acknowledges init — nothing to do. */
        return;
    }

    if (strcmp(method, "shutdown") == 0) {
        sstr_t resp = lsp_jsonrpc_response(id, "null");
        lsp_jsonrpc_write(out, resp);
        sstr_free(resp);
        state->shutdown_requested = 1;
        return;
    }

    if (strcmp(method, "exit") == 0) {
        state->should_exit = 1;
        return;
    }

    /* ---- Text document notifications ---- */

    if (strcmp(method, "textDocument/didOpen") == 0 && params) {
        struct lsp_json_value *td = lsp_json_get(params, "textDocument");
        if (td) {
            const char *uri = lsp_json_string(lsp_json_get(td, "uri"));
            const char *text = lsp_json_string(lsp_json_get(td, "text"));
            if (uri && text) {
                free(state->doc.uri);
                state->doc.uri = compat_strdup(uri);
                if (state->doc.content)
                    sstr_free(state->doc.content);
                state->doc.content = sstr(text);
                validate_document(out, state);
            }
        }
        return;
    }

    if (strcmp(method, "textDocument/didChange") == 0 && params) {
        struct lsp_json_value *changes = lsp_json_get(params, "contentChanges");
        if (changes && changes->type == LSP_JSON_ARRAY && changes->u.array.count > 0) {
            /* Full sync: last change has complete content. */
            struct lsp_json_value *last = &changes->u.array.items[changes->u.array.count - 1];
            const char *text = lsp_json_string(lsp_json_get(last, "text"));
            if (text) {
                if (state->doc.content)
                    sstr_free(state->doc.content);
                state->doc.content = sstr(text);
                validate_document(out, state);
            }
        }
        return;
    }

    if (strcmp(method, "textDocument/didSave") == 0 && params) {
        const char *text = lsp_json_string(lsp_json_get(params, "text"));
        if (text) {
            if (state->doc.content)
                sstr_free(state->doc.content);
            state->doc.content = sstr(text);
        }
        validate_document(out, state);
        return;
    }

    if (strcmp(method, "textDocument/didClose") == 0 && params) {
        struct lsp_json_value *td = lsp_json_get(params, "textDocument");
        if (td) {
            const char *uri = lsp_json_string(lsp_json_get(td, "uri"));
            if (uri && state->doc.uri && strcmp(uri, state->doc.uri) == 0) {
                /* Clear diagnostics for closed document. */
                publish_diagnostics(out, uri, NULL);
                free(state->doc.uri);
                state->doc.uri = NULL;
                if (state->doc.content) {
                    sstr_free(state->doc.content);
                    state->doc.content = NULL;
                }
            }
        }
        return;
    }

    /* ---- Request: completion ---- */

    if (strcmp(method, "textDocument/completion") == 0 && id >= 0) {
        sstr_t resp = build_completion_response(id);
        lsp_jsonrpc_write(out, resp);
        sstr_free(resp);
        return;
    }

    /* ---- Request: hover ---- */

    if (strcmp(method, "textDocument/hover") == 0 && id >= 0) {
        sstr_t resp = build_hover_response(id, state, params);
        lsp_jsonrpc_write(out, resp);
        sstr_free(resp);
        return;
    }

    /* ---- Unknown request: respond with method-not-found ---- */

    if (id >= 0) {
        sstr_t body = sstr_new();
        sstr_append_cstr(body, "{\"jsonrpc\":\"2.0\",\"id\":");
        sstr_append_long_str(body, id);
        sstr_append_cstr(body,
            ",\"error\":{\"code\":-32601,\"message\":\"Method not found\"}}");
        lsp_jsonrpc_write(out, body);
        sstr_free(body);
    }
}

/* ---- Main server loop ---- */

int lsp_server_run(void)
{
    struct lsp_state state;
    lsp_state_init(&state);

    FILE *in = stdin;
    FILE *out = stdout;

    while (!state.should_exit) {
        sstr_t msg_str = lsp_jsonrpc_read(in);
        if (!msg_str)
            break; /* EOF */

        struct lsp_json_value msg;
        if (lsp_json_parse(sstr_cstr(msg_str), (int)sstr_length(msg_str), &msg) == 0) {
            handle_message(out, &state, &msg);
            lsp_json_free(&msg);
        }
        sstr_free(msg_str);
    }

    lsp_state_cleanup(&state);
    /* Per LSP spec: exit code 0 if shutdown was requested, 1 otherwise. */
    return state.shutdown_requested ? 0 : 1;
}
