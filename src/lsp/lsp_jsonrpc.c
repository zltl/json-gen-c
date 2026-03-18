/*
 * lsp_jsonrpc.c - JSON-RPC 2.0 message framing and lightweight JSON for LSP
 */

#define _POSIX_C_SOURCE 200809L

#include "lsp/lsp_jsonrpc.h"
#include "utils/compat.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- JSON-RPC framing (Content-Length header) ---- */

sstr_t lsp_jsonrpc_read(FILE *in)
{
    /* Read headers until blank line. */
    int content_length = -1;
    char line[4096];

    while (fgets(line, sizeof(line), in)) {
        /* Strip trailing \r\n. */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n'))
            line[--len] = '\0';

        if (len == 0)
            break; /* blank line = end of headers */

        if (strncmp(line, "Content-Length:", 15) == 0) {
            content_length = atoi(line + 15);
        }
        /* Ignore other headers (Content-Type, etc.). */
    }

    if (content_length <= 0)
        return NULL;

    char *buf = (char *)malloc((size_t)content_length + 1);
    if (!buf)
        return NULL;

    size_t total = 0;
    while ((int)total < content_length) {
        size_t n = fread(buf + total, 1, (size_t)content_length - total, in);
        if (n == 0) {
            free(buf);
            return NULL;
        }
        total += n;
    }
    buf[content_length] = '\0';

    sstr_t result = sstr_new();
    sstr_append_of(result, buf, (size_t)content_length);
    free(buf);
    return result;
}

void lsp_jsonrpc_write(FILE *out, sstr_t json_body)
{
    size_t len = sstr_length(json_body);
    fprintf(out, "Content-Length: %zu\r\n\r\n", len);
    fwrite(sstr_cstr(json_body), 1, len, out);
    fflush(out);
}

/* ---- Lightweight JSON parser ---- */

struct json_parser_ctx {
    const char *buf;
    int len;
    int pos;
};

static void skip_ws(struct json_parser_ctx *c)
{
    while (c->pos < c->len && isspace((unsigned char)c->buf[c->pos]))
        c->pos++;
}

static int parse_value(struct json_parser_ctx *c, struct lsp_json_value *out);

static int parse_string_raw(struct json_parser_ctx *c, sstr_t out)
{
    if (c->pos >= c->len || c->buf[c->pos] != '"')
        return -1;
    c->pos++; /* skip opening quote */

    while (c->pos < c->len) {
        char ch = c->buf[c->pos];
        if (ch == '"') {
            c->pos++;
            return 0;
        }
        if (ch == '\\') {
            c->pos++;
            if (c->pos >= c->len)
                return -1;
            char esc = c->buf[c->pos++];
            switch (esc) {
            case '"':  sstr_append_of(out, "\"", 1); break;
            case '\\': sstr_append_of(out, "\\", 1); break;
            case '/':  sstr_append_of(out, "/", 1);  break;
            case 'b':  sstr_append_of(out, "\b", 1); break;
            case 'f':  sstr_append_of(out, "\f", 1); break;
            case 'n':  sstr_append_of(out, "\n", 1); break;
            case 'r':  sstr_append_of(out, "\r", 1); break;
            case 't':  sstr_append_of(out, "\t", 1); break;
            case 'u': {
                /* \uXXXX — pass through as UTF-8 approximation */
                if (c->pos + 4 > c->len)
                    return -1;
                unsigned int cp = 0;
                for (int i = 0; i < 4; i++) {
                    char h = c->buf[c->pos++];
                    cp <<= 4;
                    if (h >= '0' && h <= '9')      cp |= (unsigned)(h - '0');
                    else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                    else return -1;
                }
                /* Encode as UTF-8. */
                if (cp < 0x80) {
                    char b = (char)cp;
                    sstr_append_of(out, &b, 1);
                } else if (cp < 0x800) {
                    char b[2] = {(char)(0xC0 | (cp >> 6)),
                                 (char)(0x80 | (cp & 0x3F))};
                    sstr_append_of(out, b, 2);
                } else {
                    char b[3] = {(char)(0xE0 | (cp >> 12)),
                                 (char)(0x80 | ((cp >> 6) & 0x3F)),
                                 (char)(0x80 | (cp & 0x3F))};
                    sstr_append_of(out, b, 3);
                }
                break;
            }
            default: return -1;
            }
        } else {
            sstr_append_of(out, &ch, 1);
            c->pos++;
        }
    }
    return -1; /* unterminated string */
}

static int parse_string(struct json_parser_ctx *c, struct lsp_json_value *out)
{
    sstr_t s = sstr_new();
    if (parse_string_raw(c, s) < 0) {
        sstr_free(s);
        return -1;
    }
    out->type = LSP_JSON_STRING;
    out->u.string_val = s;
    return 0;
}

static int parse_number(struct json_parser_ctx *c, struct lsp_json_value *out)
{
    int start = c->pos;
    if (c->pos < c->len && c->buf[c->pos] == '-')
        c->pos++;
    while (c->pos < c->len && isdigit((unsigned char)c->buf[c->pos]))
        c->pos++;
    /* Skip fractional and exponent parts (we only need integer IDs). */
    if (c->pos < c->len && c->buf[c->pos] == '.') {
        c->pos++;
        while (c->pos < c->len && isdigit((unsigned char)c->buf[c->pos]))
            c->pos++;
    }
    if (c->pos < c->len && (c->buf[c->pos] == 'e' || c->buf[c->pos] == 'E')) {
        c->pos++;
        if (c->pos < c->len && (c->buf[c->pos] == '+' || c->buf[c->pos] == '-'))
            c->pos++;
        while (c->pos < c->len && isdigit((unsigned char)c->buf[c->pos]))
            c->pos++;
    }
    if (c->pos == start)
        return -1;
    char tmp[64];
    int nlen = c->pos - start;
    if (nlen >= (int)sizeof(tmp))
        nlen = (int)sizeof(tmp) - 1;
    memcpy(tmp, c->buf + start, (size_t)nlen);
    tmp[nlen] = '\0';
    out->type = LSP_JSON_NUMBER;
    out->u.number_val = atol(tmp);
    return 0;
}

static int parse_array(struct json_parser_ctx *c, struct lsp_json_value *out)
{
    c->pos++; /* skip '[' */
    int cap = 8, count = 0;
    struct lsp_json_value *items = (struct lsp_json_value *)malloc(
        sizeof(struct lsp_json_value) * (size_t)cap);

    skip_ws(c);
    if (c->pos < c->len && c->buf[c->pos] == ']') {
        c->pos++;
        out->type = LSP_JSON_ARRAY;
        out->u.array.items = items;
        out->u.array.count = 0;
        return 0;
    }

    for (;;) {
        if (count >= cap) {
            cap *= 2;
            items = (struct lsp_json_value *)realloc(
                items, sizeof(struct lsp_json_value) * (size_t)cap);
        }
        if (parse_value(c, &items[count]) < 0) {
            /* Free already-parsed items. */
            for (int i = 0; i < count; i++)
                lsp_json_free(&items[i]);
            free(items);
            return -1;
        }
        count++;
        skip_ws(c);
        if (c->pos < c->len && c->buf[c->pos] == ',') {
            c->pos++;
            skip_ws(c);
            continue;
        }
        if (c->pos < c->len && c->buf[c->pos] == ']') {
            c->pos++;
            break;
        }
        for (int i = 0; i < count; i++)
            lsp_json_free(&items[i]);
        free(items);
        return -1;
    }

    out->type = LSP_JSON_ARRAY;
    out->u.array.items = items;
    out->u.array.count = count;
    return 0;
}

static int parse_object(struct json_parser_ctx *c, struct lsp_json_value *out)
{
    c->pos++; /* skip '{' */
    int cap = 8, count = 0;
    char **keys = (char **)malloc(sizeof(char *) * (size_t)cap);
    struct lsp_json_value *values = (struct lsp_json_value *)malloc(
        sizeof(struct lsp_json_value) * (size_t)cap);

    skip_ws(c);
    if (c->pos < c->len && c->buf[c->pos] == '}') {
        c->pos++;
        out->type = LSP_JSON_OBJECT;
        out->u.object.keys = keys;
        out->u.object.values = values;
        out->u.object.count = 0;
        return 0;
    }

    for (;;) {
        skip_ws(c);
        if (count >= cap) {
            cap *= 2;
            keys = (char **)realloc(keys, sizeof(char *) * (size_t)cap);
            values = (struct lsp_json_value *)realloc(
                values, sizeof(struct lsp_json_value) * (size_t)cap);
        }

        /* Parse key. */
        sstr_t key_s = sstr_new();
        if (parse_string_raw(c, key_s) < 0) {
            sstr_free(key_s);
            goto fail;
        }
        keys[count] = compat_strdup(sstr_cstr(key_s));
        sstr_free(key_s);

        skip_ws(c);
        if (c->pos >= c->len || c->buf[c->pos] != ':')
            goto fail;
        c->pos++;

        if (parse_value(c, &values[count]) < 0)
            goto fail;
        count++;

        skip_ws(c);
        if (c->pos < c->len && c->buf[c->pos] == ',') {
            c->pos++;
            continue;
        }
        if (c->pos < c->len && c->buf[c->pos] == '}') {
            c->pos++;
            break;
        }
        goto fail;
    }

    out->type = LSP_JSON_OBJECT;
    out->u.object.keys = keys;
    out->u.object.values = values;
    out->u.object.count = count;
    return 0;

fail:
    for (int i = 0; i < count; i++) {
        free(keys[i]);
        lsp_json_free(&values[i]);
    }
    free(keys);
    free(values);
    return -1;
}

static int parse_value(struct json_parser_ctx *c, struct lsp_json_value *out)
{
    skip_ws(c);
    if (c->pos >= c->len)
        return -1;

    char ch = c->buf[c->pos];
    if (ch == '"')
        return parse_string(c, out);
    if (ch == '{')
        return parse_object(c, out);
    if (ch == '[')
        return parse_array(c, out);
    if (ch == '-' || isdigit((unsigned char)ch))
        return parse_number(c, out);
    if (c->len - c->pos >= 4 && strncmp(c->buf + c->pos, "true", 4) == 0) {
        c->pos += 4;
        out->type = LSP_JSON_BOOL;
        out->u.bool_val = 1;
        return 0;
    }
    if (c->len - c->pos >= 5 && strncmp(c->buf + c->pos, "false", 5) == 0) {
        c->pos += 5;
        out->type = LSP_JSON_BOOL;
        out->u.bool_val = 0;
        return 0;
    }
    if (c->len - c->pos >= 4 && strncmp(c->buf + c->pos, "null", 4) == 0) {
        c->pos += 4;
        out->type = LSP_JSON_NULL;
        return 0;
    }
    return -1;
}

/* ---- Public JSON API ---- */

int lsp_json_parse(const char *json, int len, struct lsp_json_value *out)
{
    struct json_parser_ctx c = {json, len, 0};
    memset(out, 0, sizeof(*out));
    return parse_value(&c, out);
}

void lsp_json_free(struct lsp_json_value *val)
{
    if (!val)
        return;
    switch (val->type) {
    case LSP_JSON_STRING:
        sstr_free(val->u.string_val);
        val->u.string_val = NULL;
        break;
    case LSP_JSON_ARRAY:
        for (int i = 0; i < val->u.array.count; i++)
            lsp_json_free(&val->u.array.items[i]);
        free(val->u.array.items);
        val->u.array.items = NULL;
        val->u.array.count = 0;
        break;
    case LSP_JSON_OBJECT:
        for (int i = 0; i < val->u.object.count; i++) {
            free(val->u.object.keys[i]);
            lsp_json_free(&val->u.object.values[i]);
        }
        free(val->u.object.keys);
        free(val->u.object.values);
        val->u.object.keys = NULL;
        val->u.object.values = NULL;
        val->u.object.count = 0;
        break;
    default:
        break;
    }
    val->type = LSP_JSON_NULL;
}

struct lsp_json_value *lsp_json_get(struct lsp_json_value *val, const char *key)
{
    if (!val || val->type != LSP_JSON_OBJECT)
        return NULL;
    for (int i = 0; i < val->u.object.count; i++) {
        if (strcmp(val->u.object.keys[i], key) == 0)
            return &val->u.object.values[i];
    }
    return NULL;
}

const char *lsp_json_string(struct lsp_json_value *val)
{
    if (!val || val->type != LSP_JSON_STRING)
        return NULL;
    return sstr_cstr(val->u.string_val);
}

long lsp_json_number(struct lsp_json_value *val)
{
    if (!val || val->type != LSP_JSON_NUMBER)
        return 0;
    return val->u.number_val;
}

/* ---- JSON string escaping ---- */

void lsp_json_escape_string(const char *s, sstr_t out)
{
    sstr_append_of(out, "\"", 1);
    if (s) {
        for (const char *p = s; *p; p++) {
            switch (*p) {
            case '"':  sstr_append_of(out, "\\\"", 2); break;
            case '\\': sstr_append_of(out, "\\\\", 2); break;
            case '\b': sstr_append_of(out, "\\b", 2);  break;
            case '\f': sstr_append_of(out, "\\f", 2);  break;
            case '\n': sstr_append_of(out, "\\n", 2);  break;
            case '\r': sstr_append_of(out, "\\r", 2);  break;
            case '\t': sstr_append_of(out, "\\t", 2);  break;
            default:
                if ((unsigned char)*p < 0x20) {
                    char esc[7];
                    snprintf(esc, sizeof(esc), "\\u%04x", (unsigned char)*p);
                    sstr_append_of(out, esc, 6);
                } else {
                    sstr_append_of(out, p, 1);
                }
                break;
            }
        }
    }
    sstr_append_of(out, "\"", 1);
}

/* ---- JSON-RPC message builders ---- */

sstr_t lsp_jsonrpc_response(long id, const char *result_json)
{
    sstr_t msg = sstr_new();
    sstr_append_cstr(msg, "{\"jsonrpc\":\"2.0\",\"id\":");
    sstr_append_long_str(msg, id);
    sstr_append_cstr(msg, ",\"result\":");
    sstr_append_cstr(msg, result_json);
    sstr_append_of(msg, "}", 1);
    return msg;
}

sstr_t lsp_jsonrpc_notification(const char *method, const char *params_json)
{
    sstr_t msg = sstr_new();
    sstr_append_cstr(msg, "{\"jsonrpc\":\"2.0\",\"method\":");
    lsp_json_escape_string(method, msg);
    sstr_append_cstr(msg, ",\"params\":");
    sstr_append_cstr(msg, params_json);
    sstr_append_of(msg, "}", 1);
    return msg;
}
