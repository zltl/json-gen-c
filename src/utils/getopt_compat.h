/**
 * @file getopt_compat.h
 * @brief Portable getopt_long implementation for systems without <getopt.h>.
 *
 * On POSIX systems, this header simply includes the system <getopt.h>.
 * On Windows (MSVC), it provides a compatible implementation.
 *
 * Based on public domain / BSD implementations.
 */

#ifndef GETOPT_COMPAT_H
#define GETOPT_COMPAT_H

#ifdef _WIN32

#ifdef __cplusplus
extern "C" {
#endif

extern char *optarg;
extern int optind, opterr, optopt;

struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};

#define no_argument       0
#define required_argument 1
#define optional_argument 2

/**
 * @brief Parse command-line options (long options, single-dash prefix).
 *
 * Compatible subset of POSIX getopt_long_only().
 */
int getopt_long_only(int argc, char *const argv[], const char *optstring,
                     const struct option *longopts, int *longindex);

#ifdef __cplusplus
}
#endif

#else /* POSIX */

#include <getopt.h>

#endif /* _WIN32 */

#endif /* GETOPT_COMPAT_H */
