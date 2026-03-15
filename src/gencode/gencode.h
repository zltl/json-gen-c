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

#ifdef __cplusplus
}
#endif

#endif  // GENCODE_H_
