/**
 * @file gencode.h
 * @author your name (you@domain.com)
 * @brief generate json manipulate codes.
 */

#ifndef GENCODE_H_
#define GENCODE_H_

#include "utils/hash_map.h"
#include "utils/sstr.h"

#ifdef __cplusplus
extern "C" {
#endif

// the output file names
#define OUTPUT_C_FILENAME "json.gen.c"
#define OUTPUT_H_FILENAME "json.gen.h"
#define OUTPUT_MSGPACK_C_FILENAME "msgpack.gen.c"
#define OUTPUT_MSGPACK_H_FILENAME "msgpack.gen.h"
#define OUTPUT_CBOR_C_FILENAME "cbor.gen.c"
#define OUTPUT_CBOR_H_FILENAME "cbor.gen.h"
#define OUTPUT_CPP_FILENAME "json_gen_c.gen.hpp"
#define OUTPUT_RUST_FILENAME "json_gen_c.gen.rs"
#define OUTPUT_GO_FILENAME "json_gen_c.gen.go"

/**
 * @brief generate json manipulate codes by struct_map, enum_map and oneof_map into source and header.
 *
 * @param struct_map the hash map that store all structs parsed from struct
 * defintion files.
 * @param enum_map the hash map that store all enums parsed from enum definitions.
 * @param oneof_map the hash map that store all oneof (tagged union) definitions.
 * @param source the output source code.
 * @param header the output header code.
 * @return int 0 if success, -1 if failed.
 */
extern int gencode_source(struct hash_map* struct_map, struct hash_map* enum_map,
                          struct hash_map* oneof_map, sstr_t source, sstr_t header);

/**
 * @brief generate MessagePack pack/unpack codes.
 *
 * @param struct_map the hash map that store all structs parsed from struct
 * defintion files.
 * @param enum_map the hash map that store all enums parsed from enum definitions.
 * @param oneof_map the hash map that store all oneof (tagged union) definitions.
 * @param source the output source code.
 * @param header the output header code.
 * @return int 0 if success, -1 if failed.
 */
extern int gencode_msgpack_source(struct hash_map* struct_map, struct hash_map* enum_map,
                                  struct hash_map* oneof_map, sstr_t source, sstr_t header);

/**
 * @brief generate CBOR (RFC 8949) pack/unpack codes.
 */
extern int gencode_cbor_source(struct hash_map* struct_map, struct hash_map* enum_map,
                               struct hash_map* oneof_map, sstr_t source, sstr_t header);

/**
 * @brief generate C++ wrapper header (.gen.hpp) that provides RAII classes
 * around the generated C structs.
 *
 * @param struct_map parsed struct definitions.
 * @param enum_map parsed enum definitions.
 * @param oneof_map parsed oneof (tagged union) definitions.
 * @param c_header_name filename of the generated C header to include.
 * @param format output format (0=json, 1=msgpack, 2=cbor) for marshal prefix.
 * @param output the output C++ header content.
 * @return 0 on success, -1 on failure.
 */
extern int gencode_cpp_wrapper(struct hash_map* struct_map, struct hash_map* enum_map,
                               struct hash_map* oneof_map, const char* c_header_name,
                               int format, sstr_t output);

/**
 * @brief generate a Rust module (.rs) with native serde-compatible structs
 * and enums.
 *
 * @param struct_map parsed struct definitions.
 * @param enum_map parsed enum definitions.
 * @param oneof_map parsed oneof (tagged union) definitions.
 * @param output the output Rust source content.
 * @return 0 on success, -1 on failure.
 */
extern int gencode_rust(struct hash_map* struct_map, struct hash_map* enum_map,
                        struct hash_map* oneof_map, sstr_t output);

/**
 * @brief generate a Go source file (.go) with native encoding/json-compatible
 * structs and enums.
 *
 * @param struct_map parsed struct definitions.
 * @param enum_map parsed enum definitions.
 * @param oneof_map parsed oneof (tagged union) definitions.
 * @param package_name Go package name to use.
 * @param output the output Go source content.
 * @return 0 on success, -1 on failure.
 */
extern int gencode_go(struct hash_map* struct_map, struct hash_map* enum_map,
                      struct hash_map* oneof_map, const char* package_name,
                      sstr_t output);

#ifdef __cplusplus
}
#endif

#endif  // GENCODE_H_
