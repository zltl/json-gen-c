/**
 * @file getopt_compat.c
 * @brief Portable getopt_long_only implementation for Windows.
 *
 * This file is only compiled on Windows where <getopt.h> is not available.
 * It provides a minimal but functional getopt_long_only() compatible with
 * the POSIX interface used by json-gen-c.
 *
 * Based on public domain implementations. No copyright claimed.
 */

#ifdef _WIN32

#include "utils/getopt_compat.h"

#include <stdio.h>
#include <string.h>

char *optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = '?';

static int optpos = 0;  /* position within current argv element */

/* Try to match a long option. Returns val on match, '?' on error, -1 on no match. */
static int match_long(int argc, char *const argv[], const struct option *longopts,
                      int *longindex, const char *arg) {
    int i;
    int match = -1;
    int ambig = 0;
    size_t arglen;
    const char *eq = strchr(arg, '=');

    arglen = eq ? (size_t)(eq - arg) : strlen(arg);

    for (i = 0; longopts[i].name; i++) {
        if (strncmp(arg, longopts[i].name, arglen) == 0) {
            if (strlen(longopts[i].name) == arglen) {
                /* Exact match */
                match = i;
                ambig = 0;
                break;
            }
            if (match >= 0) {
                ambig = 1;
            } else {
                match = i;
            }
        }
    }

    if (ambig) {
        if (opterr)
            fprintf(stderr, "%s: option '%s' is ambiguous\n", argv[0], argv[optind]);
        optind++;
        return '?';
    }

    if (match < 0)
        return -1;

    optind++;
    if (eq) {
        if (longopts[match].has_arg == no_argument) {
            if (opterr)
                fprintf(stderr, "%s: option '--%s' doesn't allow an argument\n",
                        argv[0], longopts[match].name);
            return '?';
        }
        optarg = (char *)(eq + 1);
    } else if (longopts[match].has_arg == required_argument) {
        if (optind >= argc) {
            if (opterr)
                fprintf(stderr, "%s: option '--%s' requires an argument\n",
                        argv[0], longopts[match].name);
            return '?';
        }
        optarg = argv[optind++];
    }

    if (longindex)
        *longindex = match;

    if (longopts[match].flag) {
        *longopts[match].flag = longopts[match].val;
        return 0;
    }
    return longopts[match].val;
}

int getopt_long_only(int argc, char *const argv[], const char *optstring,
                     const struct option *longopts, int *longindex) {
    const char *cur;

    optarg = NULL;

    if (optind >= argc || argv[optind] == NULL)
        return -1;

    cur = argv[optind];

    /* Not an option */
    if (cur[0] != '-' || cur[1] == '\0')
        return -1;

    /* "--" stops processing */
    if (cur[1] == '-' && cur[2] == '\0') {
        optind++;
        return -1;
    }

    /* Try long option first (single dash for getopt_long_only) */
    if (longopts) {
        const char *longarg = cur + 1;
        if (cur[1] == '-')
            longarg = cur + 2;

        int ret = match_long(argc, argv, longopts, longindex, longarg);
        if (ret != -1)
            return ret;
    }

    /* Fall back to short option */
    if (optpos == 0)
        optpos = 1;

    optopt = cur[optpos];
    {
        const char *p = strchr(optstring, optopt);
        if (!p || optopt == ':') {
            if (opterr)
                fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], optopt);
            optpos++;
            if (cur[optpos] == '\0') {
                optind++;
                optpos = 0;
            }
            return '?';
        }

        if (p[1] == ':') {
            /* Option takes an argument */
            if (cur[optpos + 1] != '\0') {
                optarg = (char *)(cur + optpos + 1);
            } else {
                optind++;
                if (optind >= argc) {
                    if (opterr)
                        fprintf(stderr, "%s: option requires an argument -- '%c'\n",
                                argv[0], optopt);
                    optpos = 0;
                    return optstring[0] == ':' ? ':' : '?';
                }
                optarg = argv[optind];
            }
            optind++;
            optpos = 0;
            return optopt;
        }

        optpos++;
        if (cur[optpos] == '\0') {
            optind++;
            optpos = 0;
        }
        return optopt;
    }
}

#endif /* _WIN32 */
