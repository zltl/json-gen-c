/**
 * @file compat.h
 * @brief Cross-platform compatibility abstractions for Windows and POSIX.
 *
 * Provides unified APIs for:
 * - Mutex (pthread on POSIX, CRITICAL_SECTION on Windows)
 * - isatty / fileno
 * - strdup
 * - Path separator detection
 */

#ifndef COMPAT_H
#define COMPAT_H

#ifdef _WIN32

/* ---- Windows ---- */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>

typedef CRITICAL_SECTION compat_mutex_t;

static inline int compat_mutex_init(compat_mutex_t *m) {
    InitializeCriticalSection(m);
    return 0;
}
static inline void compat_mutex_lock(compat_mutex_t *m) {
    EnterCriticalSection(m);
}
static inline void compat_mutex_unlock(compat_mutex_t *m) {
    LeaveCriticalSection(m);
}
static inline void compat_mutex_destroy(compat_mutex_t *m) {
    DeleteCriticalSection(m);
}

#define compat_isatty(fd)   _isatty(fd)
#define compat_fileno(f)    _fileno(f)
#define compat_strdup(s)    _strdup(s)

#else

/* ---- POSIX ---- */
#include <pthread.h>
#include <unistd.h>

typedef pthread_mutex_t compat_mutex_t;

static inline int compat_mutex_init(compat_mutex_t *m) {
    return pthread_mutex_init(m, NULL);
}
static inline void compat_mutex_lock(compat_mutex_t *m) {
    pthread_mutex_lock(m);
}
static inline void compat_mutex_unlock(compat_mutex_t *m) {
    pthread_mutex_unlock(m);
}
static inline void compat_mutex_destroy(compat_mutex_t *m) {
    pthread_mutex_destroy(m);
}

#define compat_isatty(fd)   isatty(fd)
#define compat_fileno(f)    fileno(f)
#define compat_strdup(s)    strdup(s)

#endif /* _WIN32 */

/* Path separator detection: true for '/' on all platforms, also '\' on Windows */
static inline int compat_is_path_sep(char c) {
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

#endif /* COMPAT_H */
