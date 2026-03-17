/**
 * @file compat/compat_check.h
 * @brief Schema compatibility checker — compares two parsed schemas and
 *        reports safe vs. breaking changes.
 */

#ifndef COMPAT_CHECK_H_
#define COMPAT_CHECK_H_

#include "utils/hash_map.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compare two parsed schemas and print a compatibility report.
 *
 * Compares structs, enums, and oneofs between the old and new schemas.
 * Reports safe changes (additions, deprecations) and breaking changes
 * (removals, type changes).
 *
 * @param old_structs Hash map of old schema structs.
 * @param old_enums   Hash map of old schema enums.
 * @param old_oneofs  Hash map of old schema oneofs.
 * @param new_structs Hash map of new schema structs.
 * @param new_enums   Hash map of new schema enums.
 * @param new_oneofs  Hash map of new schema oneofs.
 * @return 0 if only safe changes, 1 if any breaking changes found.
 */
int compat_check(struct hash_map *old_structs, struct hash_map *old_enums,
                 struct hash_map *old_oneofs, struct hash_map *new_structs,
                 struct hash_map *new_enums, struct hash_map *new_oneofs);

#ifdef __cplusplus
}
#endif

#endif /* COMPAT_CHECK_H_ */
