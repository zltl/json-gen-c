/**
 * @file error_codes.c
 * @brief Implementation of error code utilities
 */

#include "error_codes.h"

const char* json_gen_error_string(json_gen_error_t error_code) {
    switch (error_code) {
        case JSON_GEN_SUCCESS:
            return "Success";
        case JSON_GEN_ERROR_GENERAL:
            return "General error";
        case JSON_GEN_ERROR_MEMORY:
            return "Memory allocation error";
        case JSON_GEN_ERROR_FILE_IO:
            return "File I/O error";
        case JSON_GEN_ERROR_PARSE:
            return "Parsing error";
        case JSON_GEN_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case JSON_GEN_ERROR_BUFFER_OVERFLOW:
            return "Buffer overflow";
        case JSON_GEN_ERROR_FORMAT:
            return "Format error";
        case JSON_GEN_ERROR_NOT_FOUND:
            return "Resource not found";
        case JSON_GEN_ERROR_THREAD_SAFETY:
            return "Thread safety violation";
        case JSON_GEN_ERROR_BOUNDS:
            return "Boundary condition violation";
        default:
            return "Unknown error";
    }
}
