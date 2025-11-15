/**
 * @file utils/io.h
 * @brief File operation helper functions for reading and writing files.
 * 
 * This module provides convenient wrapper functions for file I/O operations,
 * integrated with the sstr_t string type for efficient string handling.
 */

#ifndef UTILS_IO_H_
#define UTILS_IO_H_

#include "utils/sstr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read entire file content into an sstr_t string
 * 
 * Opens the specified file in binary mode, reads all its content,
 * and stores it in the provided sstr_t buffer. The buffer is cleared
 * before reading. This function is useful for loading configuration
 * files, templates, or any text-based data.
 * 
 * @param filename Path to the file to read (null-terminated string)
 * @param content  Output sstr_t buffer to store file content
 * @return 0 on success, -1 on failure (file not found, read error, etc.)
 * 
 * @note The content buffer must be initialized before calling this function
 * @note The entire file is loaded into memory, so be cautious with large files
 */
extern int read_file(const char* filename, sstr_t content);

/**
 * @brief Write sstr_t content to a file
 * 
 * Opens the specified file in binary write mode and writes the entire
 * content of the sstr_t string to it. If the file exists, it will be
 * overwritten. If it doesn't exist, it will be created.
 * 
 * @param filename Path to the output file (null-terminated string)
 * @param content  sstr_t string containing data to write
 * @return 0 on success, -1 on failure (permission denied, disk full, etc.)
 * 
 * @note This function writes in binary mode to preserve exact byte content
 * @note The file will be created if it doesn't exist
 */
extern int write_file(const char* filename, sstr_t content);

#ifdef __cplusplus
}
#endif

#endif  // UTILS_IO_H_
