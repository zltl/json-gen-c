/**
 * @file diag.h
 * @brief Diagnostic engine for clang-style error reporting.
 *
 * Provides structured diagnostics with:
 * - clang-style format: filename:line:col: severity: message
 * - Source code snippet with caret indicator
 * - Severity levels (error, warning, note)
 * - ANSI color output (auto-detected via isatty)
 */

#ifndef DIAG_H_
#define DIAG_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Diagnostic severity levels */
enum diag_severity {
    DIAG_ERROR,
    DIAG_WARNING,
    DIAG_NOTE
};

/** A single diagnostic entry */
struct diag_entry {
    enum diag_severity severity;
    int line;
    int col;
    char *message;
};

/** Diagnostic engine — accumulates diagnostics for later printing */
struct diag_engine {
    const char *filename;
    const char *source;        /* source content (not owned) */
    long source_len;
    struct diag_entry *entries;
    int count;
    int capacity;
    int error_count;
    int warning_count;
    int max_errors;            /* stop collecting after this many errors */
};

/**
 * @brief Create a new diagnostic engine.
 * @param filename Source filename (not owned, must outlive engine)
 * @param source Source content string (not owned)
 * @param source_len Length of source content
 * @return Pointer to new engine, or NULL on allocation failure
 */
struct diag_engine *diag_engine_new(const char *filename, const char *source,
                                    long source_len);

/**
 * @brief Free a diagnostic engine and all its entries.
 */
void diag_engine_free(struct diag_engine *engine);

/**
 * @brief Emit a diagnostic message.
 * @param engine Diagnostic engine
 * @param severity Severity level
 * @param line 1-based line number
 * @param col 1-based column number
 * @param fmt printf-style format string
 */
void diag_emit(struct diag_engine *engine, enum diag_severity severity,
               int line, int col, const char *fmt, ...)
    __attribute__((format(printf, 5, 6)));

/**
 * @brief Print all accumulated diagnostics to a stream.
 * Uses ANSI colors if stream is a terminal.
 */
void diag_print_all(struct diag_engine *engine, FILE *stream);

/**
 * @brief Check if any errors were emitted.
 */
int diag_has_errors(struct diag_engine *engine);

/**
 * @brief Get the number of errors emitted.
 */
int diag_error_count(struct diag_engine *engine);

#ifdef __cplusplus
}
#endif

#endif /* DIAG_H_ */
