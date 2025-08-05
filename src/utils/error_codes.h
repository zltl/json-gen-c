/**
 * @file error_codes.h
 * @brief Unified error code definitions for json-gen-c
 */

#ifndef ERROR_CODES_H
#define ERROR_CODES_H

/**
 * @brief Error codes for json-gen-c operations
 */
typedef enum {
    JSON_GEN_SUCCESS = 0,           /**< Operation completed successfully */
    JSON_GEN_ERROR_GENERAL = -1,    /**< General error */
    JSON_GEN_ERROR_MEMORY = -2,     /**< Memory allocation error */
    JSON_GEN_ERROR_FILE_IO = -3,    /**< File I/O error */
    JSON_GEN_ERROR_PARSE = -4,      /**< Parsing error */
    JSON_GEN_ERROR_INVALID_PARAM = -5, /**< Invalid parameter */
    JSON_GEN_ERROR_BUFFER_OVERFLOW = -6, /**< Buffer overflow */
    JSON_GEN_ERROR_FORMAT = -7,     /**< Format error */
    JSON_GEN_ERROR_NOT_FOUND = -8,  /**< Resource not found */
    JSON_GEN_ERROR_THREAD_SAFETY = -9, /**< Thread safety violation */
    JSON_GEN_ERROR_BOUNDS = -10     /**< Boundary condition violation */
} json_gen_error_t;

/**
 * @brief Get error message string for error code
 * @param error_code The error code
 * @return Human-readable error message
 */
const char* json_gen_error_string(json_gen_error_t error_code);

#endif /* ERROR_CODES_H */
