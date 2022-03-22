/**
 * @file utils/io.h
 * @brief file operation helper functions.
 */

#ifndef UTILS_IO_H_
#define UTILS_IO_H_

#include "utils/sstr.h"

#ifdef __cplusplus
extern "C" {
#endif

int read_file(const char* filename, sstr_t content);
int write_file(const char* filename, sstr_t content);

#ifdef __cplusplus
}
#endif

#endif  // UTILS_IO_H_
