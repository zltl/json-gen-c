/**
 * @file utils/io.h
 * @brief File operation helper functions.
 */

#ifndef UTILS_IO_H_
#define UTILS_IO_H_

#include "utils/sstr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read the entire contents of a file into a string.
 *
 * @param filename path to the file to read.
 * @param content  output sstr_t that receives the file contents.
 * @return 0 on success, non-zero on error (e.g. file not found).
 */
extern int read_file(const char* filename, sstr_t content);

/**
 * @brief Write a string to a file, replacing any existing contents.
 *
 * @param filename path to the file to write.
 * @param content  sstr_t containing the data to write.
 * @return 0 on success, non-zero on error.
 */
extern int write_file(const char* filename, sstr_t content);

#ifdef __cplusplus
}
#endif

#endif  // UTILS_IO_H_
