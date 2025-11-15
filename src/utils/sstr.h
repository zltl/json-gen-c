/**
 * @file sstr.h
 * @brief sstr_t are objects that represent sequences of characters.
 * @details The standard C style string is a sequence of characters terminated
 * by a null character, which is easy to cause buffer overflow. And it's
 * annoying to pass pointer and length of string to every whare.
 *
 * The sequence of characters just like a string in C++, inside the sstr_t
 * struct, it also has a null character at the end, so that we can use
 * sstr_t as a C style string.
 *
 * sstr_t contains a pointer to char sequence and its length, solves the
 * security problems of standard C string. With functions bundle with sstr_t,
 * you can easily manipulate the string just like standard C string, but in a
 * safer way:
 *
 *     sstr_t stotal = sstr_new();
 *     sstr_t s1 = sstr("hello");
 *     sstr_t s2 = sstr("world");
 *     sstr_append(stotal, s1);
 *     sstr_append_of(stotal, " ", 1);
 *     sstr_append(stotal, s2);
 *     sstr_free(s1);
 *     sstr_free(s2);
 *
 *     sstr_t result = sstr_printf("stotal=%S, c-str=%s, int=%d, long=%ld",
 *         stotal, stotal, 123, (long)456);
 *
 *     puts(sstr_cstr(result));
 *
 *     sstr_free(result);
 *     sstr_free(stotal);
 */

#ifndef SSTR_H_
#define SSTR_H_

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHORT_STR_CAPACITY 25
#define CAP_ADD_DELTA 256

/**
 * @brief Internal structure for sstr_t implementation
 * 
 * Uses three storage strategies based on string length:
 * - SHORT: Up to 25 chars stored inline (no heap allocation)
 * - LONG: Heap-allocated buffer with capacity tracking
 * - REF: Zero-copy reference to external data
 * 
 * The length field must be first to enable the sstr_length() macro.
 */
struct sstr_s {
    size_t length;  /**< String length in bytes (MUST BE FIRST) */
    char type;      /**< Storage type: SHORT, LONG, or REF */
    union {
        /** SHORT type: inline storage for strings up to 25 bytes */
        char short_str[SHORT_STR_CAPACITY + 1];
        
        /** LONG type: heap-allocated buffer with capacity tracking */
        struct {
            size_t capacity;  /**< Allocated buffer size */
            char* data;       /**< Pointer to heap-allocated buffer */
        } long_str;
        
        /** REF type: zero-copy reference to external data */
        struct {
            char* data;  /**< Pointer to external buffer (not owned) */
        } ref_str;
    } un;
};

/** String stored inline (up to 25 chars) */
#define SSTR_TYPE_SHORT 0
/** String stored in heap-allocated buffer */
#define SSTR_TYPE_LONG 1
/** String references external data (zero-copy) */
#define SSTR_TYPE_REF 2

/**
 * @brief sstr_t are objects that represent sequences of characters.
 */
typedef void* sstr_t;

/**
 * @brief Create an empty sstr_t.
 *
 * @return sstr_t
 */
extern sstr_t sstr_new(void);

/**
 * @brief delete a sstr_t.
 *
 * @param s sstr_t instance to delete.
 */
extern void sstr_free(sstr_t s);

/**
 * @brief Create a sstr_t from \a data with \a length bytes.
 * @details The \a data is copied to the new sstr_t, so you can free \a data
 * after calling this function.
 *
 * @param data data to copy to the result sstr_t.
 * @param length length of \a data.
 * @return sstr_t containing data copied from \a data.
 */
extern sstr_t sstr_of(const void* data, size_t length);

/**
 * @brief Create a sstr_t from data with length bytes. The data is not
 * copied, but have a pointer to data.
 *
 * @param data data of the result sstr_t.
 * @param length length of \a data.
 * @return sstr_t
 * @note The result sstr_t does not own data, but have a pointer to data. It is
 * a reference, not a copy.
 * @note You cannot append a sstr_ref() result.
 */
extern sstr_t sstr_ref(const void* data, size_t length);

/**
 * @brief Create a sstr_t from C-style (NULL-terminated) string \a str.
 * @details The \a cstr is copied to the new sstr_t, so you can free \a cstr
 * after calling this function.
 *
 * @param cstr C-style string to copy to the result sstr_t.
 * @return sstr_t containing \a data copied from cstr.
 */
extern sstr_t sstr(const char* cstr);

/**
 * @brief Return C-style string representation of \a s.
 * @details This function return a pointer to the internal C-style string, it
 * has a null-terminal character at the end. So you can use it as a C-style
 * string. The returned pointer is valid until
 * sstr_free()/sstr_append()/sstr_append_of() or any functions that may modify
 * the contents of sstr_t is called.
 *
 * @param s sstr_t instance to convert to C-style string.
 * @return char* C-style string representation of \a s.
 * @note The returned string is reused by \a s, do not free it yourself.
 */
extern char* sstr_cstr(sstr_t s);

/**
 * @brief Return the length of \a s, in terms of bytes.
 * @details This is the number of actual bytes that conform the contents of the
 * sstr_t, which is not necessarily equal to its storage capacity.
 *
 * Note that sstr_t objects handle bytes without knowledge of the encoding that
 * may eventually be used to encode the characters it contains. Therefore, the
 * value returned may not correspond to the actual number of encoded characters
 * in sequences of multi-byte or variable-length characters (such as UTF-8).
 *
 * @param s sstr_t instance to get length of.
 * @return size_t The number of bytes of \a s.
 */
#define sstr_length(s) ((struct sstr_s*)s)->length

/**
 * @brief Compare \a a and \a b
 *        return 0 if equal, <0 if \a a < \a b, >0 if \a a > \a b.
 *
 * @param a sstr_t to be compared.
 * @param b sstr_t to be compared to.
 * @return int the compare result.
 * @returns 0 They compare equal.
 * @returns <0 Either the value of the first character that does not match is
 * lower in the compared string, or all compared characters match but the
 * compared string is shorter.
 * @returns >0 Either the value of the first character that does not match is
 * greater in the compared string, or all compared characters match but the
 * compared string is longer.
 * @note This function is case sensitive.
 */
extern int sstr_compare(sstr_t a, sstr_t b);

/**
 * @brief compare sstr_t \a a and \a c-style string b
 * @details just like sstr_compare, but compare \a a and \a c-style string b.
 *
 * @return int
 */
extern int sstr_compare_c(sstr_t a, const char* b);

/**
 * @brief Append zero bytes to string (for buffer allocation)
 * 
 * Extends the string by appending the specified number of null bytes.
 * Automatically handles storage type transitions and capacity management.
 * Useful for pre-allocating buffer space before reading data.
 *
 * @param s Destination sstr_t to extend
 * @param length Number of null bytes to append
 * @note Will convert SHORT to LONG type if needed
 * @note Cannot be called on REF type strings
 */
extern void sstr_append_zero(sstr_t s, size_t length);

/**
 * @brief Append raw data to string
 * 
 * Extends the string by appending arbitrary byte data. The most flexible
 * append function, works with any data including embedded nulls. Handles
 * storage type transitions automatically.
 *
 * @param s Destination sstr_t to extend
 * @param data Pointer to data to append (can contain null bytes)
 * @param length Number of bytes to append from data
 * @note Automatically manages capacity and storage type
 * @note Cannot be called on REF type strings
 */
extern void sstr_append_of(sstr_t s, const void* data, size_t length);

/**
 * @brief Append one sstr_t to another
 * 
 * Concatenates two sstr_t strings efficiently. Automatically handles
 * all storage type combinations and capacity management.
 *
 * @param dst Destination sstr_t to extend
 * @param src Source sstr_t to append
 * @note dst is modified, src is unchanged
 * @note dst cannot be REF type
 */
extern void sstr_append(sstr_t dst, sstr_t src);

/**
 * @brief Append C string to sstr_t
 * 
 * Convenience function to append a null-terminated C string. Calculates
 * length automatically and appends the string data.
 *
 * @param dst Destination sstr_t to extend
 * @param src Source C string (null-terminated)
 * @note More efficient than creating a temporary sstr_t
 * @note dst cannot be REF type
 */
extern void sstr_append_cstr(sstr_t dst, const char* src);

/**
 * @brief Duplicate \a s and return.
 *
 * @param s sstr_t to duplicate.
 * @return sstr_t  duplicate of \a s.
 */
extern sstr_t sstr_dup(sstr_t s);

/**
 * @brief Get substring of \a s starting at \a index with \a length bytes.
 *
 * @param s sstr_t instance to get substring of.
 * @param index index of the first byte of the substring.
 * @param len number of bytes of the substring.
 * @return sstr_t substring of \a s. if \a index is out of range, return an
 * empty string.
 */
extern sstr_t sstr_substr(sstr_t s, size_t index, size_t len);

/**
 * @brief Clear string content to empty
 * 
 * Resets the string to empty (length 0) without deallocating memory.
 * The string can be reused efficiently after clearing. For SHORT and
 * LONG types, sets length to 0 and adds null terminator.
 *
 * @param s sstr_t instance to clear
 * @note Does not free allocated memory, only resets length
 * @note Does nothing for REF type strings
 * @note After clearing, the string is reusable
 */
extern void sstr_clear(sstr_t s);

/**
 * @brief Printf-style formatting with extended format specifiers
 * 
 * Creates a formatted string using printf-like syntax with additional
 * format specifiers for sstr_t and other types. This is the va_list
 * variant used by sstr_printf() and sstr_printf_append().
 *
 * Supported format specifiers:
 *   - %[0][width]T              time_t value
 *   - %[0][width][u][x|X]z      ssize_t/size_t
 *   - %[0][width][u][x|X]d      int/unsigned int
 *   - %[0][width][u][x|X]l      long
 *   - %[0][width][u][x|X]D      int32_t/uint32_t
 *   - %[0][width][u][x|X]L      int64_t/uint64_t
 *   - %[0][width][.width]f      double (max %18.15f)
 *   - %p                        void * pointer
 *   - %[x|X]S                   sstr_t (x/X for hexadecimal)
 *   - %s                        null-terminated C string
 *   - %*s                       length and string pointer
 *   - %Z                        null character '\0'
 *   - %N                        newline '\n'
 *   - %c                        single character
 *   - %%                        literal % character
 *
 * Reserved:
 *   - %C                        wide character (wchar_t)
 *
 * @param fmt Format string with specifiers
 * @param args va_list of arguments matching format specifiers
 * @return New sstr_t containing formatted result
 * @note If %u/%x/%X used, trailing 'd' can be omitted
 */
extern sstr_t sstr_vslprintf(const char* fmt, va_list args);

/**
 * @brief Append formatted text to existing string (va_list version)
 * 
 * Like sstr_vslprintf but appends to an existing string instead of
 * creating a new one. More efficient when building strings incrementally.
 * Uses the same extended format specifiers as sstr_vslprintf.
 *
 * @param buf Existing sstr_t to append formatted text to
 * @param fmt Format string with specifiers
 * @param args va_list of arguments matching format specifiers
 * @return The same buf pointer (for method chaining)
 * @note buf cannot be REF type
 * @note Returns NULL on formatting error
 */
extern sstr_t sstr_vslprintf_append(sstr_t buf, const char* fmt, va_list args);

/**
 * @brief Create formatted string (printf-style with extensions)
 * 
 * Creates a new sstr_t by formatting the provided arguments according
 * to the format string. Supports all standard printf formats plus
 * extended specifiers like %S for sstr_t.
 *
 * @param fmt Format string (supports extended specifiers)
 * @param ... Variable arguments matching format specifiers
 * @return New sstr_t containing formatted result
 * @note Caller must free the returned sstr_t with sstr_free()
 */
extern sstr_t sstr_printf(const char* fmt, ...);

/**
 * @brief Same as sstr_printf(), but but print to \a buf instead of create a new
 * one.
 *
 * @param buf buffer to print to.
 * @param fmt format string.
 * @param ... arguments.
 * @return sstr_t the result string.
 */
extern sstr_t sstr_printf_append(sstr_t buf, const char* fmt, ...);

/// convert sstr <-> int,long,float,double

extern void sstr_append_int_str(sstr_t s, int i);
extern int sstr_parse_long(sstr_t s, long* v);
extern int sstr_parse_int(sstr_t* s, int* v);
extern void sstr_append_long_str(sstr_t s, long l);
extern void sstr_append_float_str(sstr_t s, float f, int precission);
extern void sstr_append_double_str(sstr_t s, double f, int precision);
extern int sstr_parse_double(sstr_t s, double* v);

/**
 * @brief Append if cond is true, otherwise do nothing.
 *
 * @param s the sstr_t to append to.
 * @param data data to append.
 * @param length length of \a data.
 * @param cond condition
 */
extern void sstr_append_of_if(sstr_t s, const void* data, size_t length, int cond);
/**
 * @brief Append C style string if cond is true, otherwise do nothing.
 * @param dst destination sstr_t to append to.
 * @param src source C-style string to append
 * @param cond condition
 */
#define sstr_append_cstr_if(dst, src, cond) \
    sstr_append_of_if(dst, src, strlen(src), cond)

// escape string to json string format
extern int sstr_json_escape_string_append(sstr_t out, sstr_t in);

/**
 * @brief append spaces at the end of the sstr_t.
 *
 * @param s the sstr_t to append spaces to.
 * @param indent numbers of spaces to append.
 */
extern void sstr_append_indent(sstr_t s, size_t indent);

/**
 * @brief return version string.
 *
 * @return const char* static version string.
 */
extern const char* sstr_version(void);

#ifdef __cplusplus
}
#endif

#endif /* SSTR_H_  */
