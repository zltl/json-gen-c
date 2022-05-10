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

struct sstr_s {
    size_t length;  // MUST FIRST, see sstr_length at sstr.h
    char type;
    union {
        // short string store datas in short_str
        char short_str[SHORT_STR_CAPACITY + 1];
        // long string allocate a buffer and store datas in long_str
        struct {
            size_t capacity;
            char* data;
        } long_str;
        // reference to a memory buffer
        struct {
            char* data;
        } ref_str;
    } un;
};

#define SSTR_TYPE_SHORT 0
#define SSTR_TYPE_LONG 1
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
sstr_t sstr_new();

/**
 * @brief delete a sstr_t.
 *
 * @param s sstr_t instance to delete.
 */
void sstr_free(sstr_t s);

/**
 * @brief Create a sstr_t from \a data with \a length bytes.
 * @details The \a data is copied to the new sstr_t, so you can free \a data
 * after calling this function.
 *
 * @param data data to copy to the result sstr_t.
 * @param length length of \a data.
 * @return sstr_t containing data copied from \a data.
 */
sstr_t sstr_of(const void* data, size_t length);

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
sstr_t sstr_ref(const void* data, size_t length);

/**
 * @brief Create a sstr_t from C-style (NULL-terminated) string \a str.
 * @details The \a cstr is copied to the new sstr_t, so you can free \a cstr
 * after calling this function.
 *
 * @param cstr C-style string to copy to the result sstr_t.
 * @return sstr_t containing \a data copied from cstr.
 */
sstr_t sstr(const char* cstr);

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
char* sstr_cstr(sstr_t s);

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
int sstr_compare(sstr_t a, sstr_t b);

/**
 * @brief compare sstr_t \a a and \a c-style string b
 * @details just like sstr_compare, but compare \a a and \a c-style string b.
 *
 * @return int
 */
int sstr_compare_c(sstr_t a, const char* b);

/**
 * @brief Extends the sstr_t by appending additional '\0' characters at the end
 * of its current value.
 *
 * @param s destination sstr_t.
 * @param length length of '\0' to append.
 */
void sstr_append_zero(sstr_t s, size_t length);

/**
 * @brief Extends the sstr_t by appending additional characters in \a data with
 * length of \a length at the end of its current value .
 *
 * @param s destination sstr_t.
 * @param data data to append.
 * @param length length of \a data.
 */
void sstr_append_of(sstr_t s, const void* data, size_t length);

/**
 * @brief Extends the sstr_t by appending additional characters contained in \a
 * src.
 *
 * @param dst destination sstr_t.
 * @param src source sstr_t.
 */
void sstr_append(sstr_t dst, sstr_t src);

/**
 * @brief Extends the sstr_t by appending additional characters contained in \a
 * src.
 *
 * @param dst destination sstr_t.
 * @param src source C-style string.
 */
void sstr_append_cstr(sstr_t dst, const char* src);

/**
 * @brief Duplicate \a s and return.
 *
 * @param s sstr_t to duplicate.
 * @return sstr_t  duplicate of \a s.
 */
sstr_t sstr_dup(sstr_t s);

/**
 * @brief Get substring of \a s starting at \a index with \a length bytes.
 *
 * @param s sstr_t instance to get substring of.
 * @param index index of the first byte of the substring.
 * @param len number of bytes of the substring.
 * @return sstr_t substring of \a s. if \a index is out of range, return an
 * empty string.
 */
sstr_t sstr_substr(sstr_t s, size_t index, size_t len);

/**
 * @brief clear the sstr_t. After this call, the sstr_t is empty.
 *
 * @param s sstr_t instance to clear.
 */
void sstr_clear(sstr_t s);

/**
 * @brief Printf implement.
 *
 * supported formats:
 *
 *   - %[0][width]T              time_t
 *   - %[0][width][u][x|X]z      ssize_t/size_t
 *   - %[0][width][u][x|X]d      int/u_int
 *   - %[0][width][u][x|X]l      long
 *   - %[0][width][u][x|X]D      int32_t/uint32_t
 *   - %[0][width][u][x|X]L      int64_t/uint64_t
 *   - %[0][width][.width]f      double, max valid number fits to %18.15f
 *   - %p                        void *
 *   - %[x|X]S                   sstr_t, if x, print as hexadecimal
 *   - %s                        null-terminated string
 *   - %*s                       length and string
 *   - %Z                        '\0'
 *   - %N                        '\n'
 *   - %c                        char
 *   - %%                        %
 *
 *  reserved:
 *   - %C                        wchar
 *
 *  if %u/%x/%X, tailing d can be ignore
 */
sstr_t sstr_vslprintf(const char* fmt, va_list args);

/**
 * @brief Same as sstr_vslprintf, but print to \a buf instead of create a new
 * one.
 *
 * @param buf result sstr_t to print to.
 * @param fmt format string.
 * @param args arguments.
 * @return sstr_t the result string.
 */
sstr_t sstr_vslprintf_append(sstr_t buf, const char* fmt, va_list args);

/**
 * @brief printf implement.
 *
 * @param fmt format, like C printf()
 * @param ... arguments, like C printf()
 * @return sstr_t result string.
 */
sstr_t sstr_printf(const char* fmt, ...);

/**
 * @brief Same as sstr_printf(), but but print to \a buf instead of create a new
 * one.
 *
 * @param buf buffer to print to.
 * @param fmt format string.
 * @param ... arguments.
 * @return sstr_t the result string.
 */
sstr_t sstr_printf_append(sstr_t buf, const char* fmt, ...);

/// convert sstr <-> int,long,float,double

void sstr_append_int_str(sstr_t s, int i);
int sstr_parse_long(sstr_t s, long* v);
int sstr_parse_int(sstr_t* s, int* v);
void sstr_append_long_str(sstr_t s, long l);
void sstr_append_float_str(sstr_t s, float f, int precission);
void sstr_append_double_str(sstr_t s, double f, int precision);
int sstr_parse_double(sstr_t s, double* v);

/**
 * @brief Append if cond is true, otherwise do nothing.
 *
 * @param s the sstr_t to append to.
 * @param data data to append.
 * @param length length of \a data.
 * @param cond condition
 */
void sstr_append_of_if(sstr_t s, const void* data, size_t length, int cond);
/**
 * @brief Append C style string if cond is true, otherwise do nothing.
 * @param dst destination sstr_t to append to.
 * @param src source C-style string to append
 * @param cond condition
 */
#define sstr_append_cstr_if(dst, src, cond) \
    sstr_append_of_if(dst, src, strlen(src), cond)

// escape string to json string format
int sstr_json_escape_string_append(sstr_t out, sstr_t in);

/**
 * @brief append spaces at the end of the sstr_t.
 *
 * @param s the sstr_t to append spaces to.
 * @param indent numbers of spaces to append.
 */
void sstr_append_indent(sstr_t s, size_t indent);

/**
 * @brief return version string.
 *
 * @return const char* static version string.
 */
const char* sstr_version();

#ifdef __cplusplus
}
#endif

#endif /* SSTR_H_  */
