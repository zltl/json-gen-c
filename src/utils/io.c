/**
 * @file utils/io.c
 * @brief Implementation of file I/O helper functions.
 * 
 * Provides simple and efficient file reading and writing operations
 * integrated with the sstr_t string type for seamless string handling.
 */

#include "utils/io.h"

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Read entire file content into an sstr_t string
 * 
 * Implementation details:
 * 1. Opens file in binary read mode
 * 2. Seeks to end to determine file size
 * 3. Allocates buffer space in sstr_t
 * 4. Reads entire file in one operation
 * 5. Closes file and returns
 * 
 * @param filename Path to input file
 * @param content Output sstr_t buffer (will be cleared first)
 * @return 0 on success, -1 if file cannot be opened
 */
int read_file(const char* filename, sstr_t content) {
    FILE* f = fopen(filename, "rb");
    if (f == NULL) {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    sstr_clear(content);
    sstr_append_zero(content, fsize);
    fread(sstr_cstr(content), 1, fsize, f);
    fclose(f);
    return 0;
}

/**
 * @brief Write sstr_t content to a file
 * 
 * Implementation details:
 * 1. Opens file in binary write mode (creates or truncates)
 * 2. Writes entire sstr_t content in one operation
 * 3. Closes file and returns
 * 
 * @param filename Path to output file
 * @param content sstr_t string to write
 * @return 0 on success, -1 if file cannot be opened for writing
 */
int write_file(const char* filename, sstr_t content) {
    FILE* f = fopen(filename, "wb");
    if (f == NULL) {
        return -1;
    }
    fwrite(sstr_cstr(content), 1, sstr_length(content), f);
    fclose(f);
    return 0;
}
