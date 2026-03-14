/**
 * @file diag.c
 * @brief Diagnostic engine implementation.
 */

#define _POSIX_C_SOURCE 200809L

#include "utils/diag.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DIAG_INITIAL_CAPACITY 16
#define DIAG_MAX_ERRORS_DEFAULT 20
#define DIAG_MSG_BUF_SIZE 1024

/* ANSI color codes */
#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_RED     "\033[1;31m"
#define ANSI_MAGENTA "\033[1;35m"
#define ANSI_CYAN    "\033[1;36m"
#define ANSI_GREEN   "\033[1;32m"

struct diag_engine *diag_engine_new(const char *filename, const char *source,
                                    long source_len) {
    struct diag_engine *engine =
        (struct diag_engine *)calloc(1, sizeof(struct diag_engine));
    if (engine == NULL) {
        return NULL;
    }
    engine->filename = filename;
    engine->source = source;
    engine->source_len = source_len;
    engine->entries = (struct diag_entry *)calloc(
        DIAG_INITIAL_CAPACITY, sizeof(struct diag_entry));
    if (engine->entries == NULL) {
        free(engine);
        return NULL;
    }
    engine->capacity = DIAG_INITIAL_CAPACITY;
    engine->count = 0;
    engine->error_count = 0;
    engine->warning_count = 0;
    engine->max_errors = DIAG_MAX_ERRORS_DEFAULT;
    return engine;
}

void diag_engine_free(struct diag_engine *engine) {
    if (engine == NULL) return;
    for (int i = 0; i < engine->count; i++) {
        free(engine->entries[i].message);
    }
    free(engine->entries);
    free(engine);
}

void diag_emit(struct diag_engine *engine, enum diag_severity severity,
               int line, int col, const char *fmt, ...) {
    if (engine == NULL) return;

    /* Stop collecting after max_errors errors */
    if (severity == DIAG_ERROR && engine->error_count >= engine->max_errors) {
        return;
    }

    /* Grow entries array if needed */
    if (engine->count >= engine->capacity) {
        int new_cap = engine->capacity * 2;
        struct diag_entry *new_entries = (struct diag_entry *)realloc(
            engine->entries, (size_t)new_cap * sizeof(struct diag_entry));
        if (new_entries == NULL) {
            return;  /* silently drop diagnostic on OOM */
        }
        engine->entries = new_entries;
        engine->capacity = new_cap;
    }

    /* Format message */
    char buf[DIAG_MSG_BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    struct diag_entry *entry = &engine->entries[engine->count];
    entry->severity = severity;
    entry->line = line;
    entry->col = col;
    entry->message = strdup(buf);
    if (entry->message == NULL) {
        return;
    }
    engine->count++;

    if (severity == DIAG_ERROR) {
        engine->error_count++;
    } else if (severity == DIAG_WARNING) {
        engine->warning_count++;
    }
}

/**
 * @brief Find the start and end offsets of a given 1-based line in source.
 * @return 0 on success, -1 if line not found
 */
static int find_source_line(const char *source, long source_len, int line,
                            long *out_start, long *out_end) {
    if (source == NULL || line < 1) return -1;
    int cur_line = 1;
    long start = 0;
    for (long i = 0; i <= source_len; i++) {
        if (cur_line == line) {
            start = i;
            /* Find end of this line */
            long end = i;
            while (end < source_len && source[end] != '\n' &&
                   source[end] != '\r') {
                end++;
            }
            *out_start = start;
            *out_end = end;
            return 0;
        }
        if (i < source_len && source[i] == '\n') {
            cur_line++;
        } else if (i < source_len && source[i] == '\r') {
            cur_line++;
            if (i + 1 < source_len && source[i + 1] == '\n') {
                i++;  /* skip \r\n */
            }
        }
    }
    return -1;
}

static const char *severity_str(enum diag_severity sev) {
    switch (sev) {
        case DIAG_ERROR:   return "error";
        case DIAG_WARNING: return "warning";
        case DIAG_NOTE:    return "note";
    }
    return "error";
}

static const char *severity_color(enum diag_severity sev) {
    switch (sev) {
        case DIAG_ERROR:   return ANSI_RED;
        case DIAG_WARNING: return ANSI_MAGENTA;
        case DIAG_NOTE:    return ANSI_CYAN;
    }
    return ANSI_RED;
}

void diag_print_all(struct diag_engine *engine, FILE *stream) {
    if (engine == NULL || engine->count == 0) return;

    int use_color = isatty(fileno(stream));

    for (int i = 0; i < engine->count; i++) {
        struct diag_entry *e = &engine->entries[i];

        /* Line 1: filename:line:col: severity: message */
        if (use_color) {
            fprintf(stream, "%s%s:%d:%d: %s%s: %s%s%s\n",
                    ANSI_BOLD, engine->filename, e->line, e->col,
                    severity_color(e->severity), severity_str(e->severity),
                    ANSI_RESET ANSI_BOLD, e->message, ANSI_RESET);
        } else {
            fprintf(stream, "%s:%d:%d: %s: %s\n",
                    engine->filename, e->line, e->col,
                    severity_str(e->severity), e->message);
        }

        /* Line 2+3: source snippet + caret */
        long line_start, line_end;
        if (find_source_line(engine->source, engine->source_len, e->line,
                             &line_start, &line_end) == 0) {
            /* Print source line with leading spaces preserved */
            long line_len = line_end - line_start;
            fprintf(stream, " %.*s\n", (int)line_len,
                    engine->source + line_start);

            /* Caret line: spaces + ^ */
            int caret_pos = e->col;
            if (caret_pos < 1) caret_pos = 1;
            /* Account for tabs in the source line for alignment */
            fprintf(stream, " ");
            for (int c = 0; c < caret_pos - 1 && c < (int)line_len; c++) {
                char ch = engine->source[line_start + c];
                if (ch == '\t') {
                    fprintf(stream, "\t");
                } else {
                    fprintf(stream, " ");
                }
            }
            if (use_color) {
                fprintf(stream, "%s^%s\n", ANSI_GREEN, ANSI_RESET);
            } else {
                fprintf(stream, "^\n");
            }
        }
    }

    /* Summary line */
    if (engine->error_count > 0 || engine->warning_count > 0) {
        fprintf(stream, "%d error%s",
                engine->error_count, engine->error_count != 1 ? "s" : "");
        if (engine->warning_count > 0) {
            fprintf(stream, " and %d warning%s",
                    engine->warning_count,
                    engine->warning_count != 1 ? "s" : "");
        }
        fprintf(stream, " generated.\n");
    }
}

int diag_has_errors(struct diag_engine *engine) {
    if (engine == NULL) return 0;
    return engine->error_count > 0;
}

int diag_error_count(struct diag_engine *engine) {
    if (engine == NULL) return 0;
    return engine->error_count;
}
