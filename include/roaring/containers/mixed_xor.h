/*
 * mixed_xor.h
 *
 */

#ifndef INCLUDE_CONTAINERS_MIXED_XOR_H_
#define INCLUDE_CONTAINERS_MIXED_XOR_H_

/* These functions appear to exclude cases where the
 * inputs have the same type and the output is guaranteed
 * to have the same type as the inputs.  Eg, bitset unions
 */

/*
 * Java implementation (as of May 2016) for array_run, run_run
 * and  bitset_run don't do anything different for inplace.
 * (They are not truly in place.)
 */

#include <roaring/containers/array.h>
#include <roaring/containers/bitset.h>
#include <roaring/containers/run.h>

//#include "containers.h"

#ifdef __cplusplus
extern "C" { namespace roaring { namespace internal {
#endif

/* NON-LAZY XOR FUNCTIONS
 *
 * These functions compute the xor of src_1 and src_2 and return the result.
 * result_type will be the type of the newly generated container.
 *
 * They are "non-lazy" because they *do* compact the result container to a
 * minimal size, and ensure the cardinality of bitsets has been precalculated.
 */

container_t *array_bitset_container_xor(
        const array_container_t *src_1, const bitset_container_t *src_2,
        uint8_t *result_type);

container_t *bitset_bitset_container_xor(
        const bitset_container_t *src_1, const bitset_container_t *src_2,
        uint8_t *result_type);

container_t *run_bitset_container_xor(
        const run_container_t *src_1, const bitset_container_t *src_2,
        uint8_t *result_type);

container_t *array_run_container_xor(
        const array_container_t *src_1, const run_container_t *src_2,
        uint8_t *result_type);

container_t *array_array_container_xor(
        const array_container_t *src_1, const array_container_t *src_2,
        uint8_t *result_type);

container_t *run_run_container_xor(
        const run_container_t *src_1, const run_container_t *src_2,
        uint8_t *result_type);


/* Compute the xor of src_1 and src_2 and write the result to
 * dst. It is allowed for src_2 to be dst.  This version does not
 * update the cardinality of dst (it is set to BITSET_UNKNOWN_CARDINALITY).
 */

void array_bitset_container_lazy_xor(const array_container_t *src_1,
                                     const bitset_container_t *src_2,
                                     bitset_container_t *dst);


/* lazy xor.  Dst is initialized and may be equal to src_2.
 *  Result is left as a bitset container, even if actual
 *  cardinality would dictate an array container.
 */

void run_bitset_container_lazy_xor(const run_container_t *src_1,
                                   const bitset_container_t *src_2,
                                   bitset_container_t *dst);


/* dst does not initially have a valid container.  Creates either
 * an array or a bitset container, indicated by return code.
 * A bitset container will not have a valid cardinality and the
 * container type might not be correct for the actual cardinality
 */

bool array_array_container_lazy_xor(
        const array_container_t *src_1, const array_container_t *src_2,
        container_t **dst);

/* Dst is a valid run container. (Can it be src_2? Let's say not.)
 * Leaves result as run container, even if other options are
 * smaller.
 */

void array_run_container_lazy_xor(const array_container_t *src_1,
                                  const run_container_t *src_2,
                                  run_container_t *dst);


/* INPLACE versions (initial implementation may not exploit all inplace
 * opportunities (if any...)
 *
 * Compute the xor of src_1 and src_2 and write the result to src_1.
 * type1 should be the correct type of src1 to start with.
 * The type may be modified.
 */

void bitset_array_container_ixor(
        container_t **c1, uint8_t *type1,
        const array_container_t *ac2);

void bitset_bitset_container_ixor(
        container_t **c1, uint8_t *type1,
        const bitset_container_t *bc2);

void array_bitset_container_ixor(
        container_t **c1, uint8_t *type1,
        const bitset_container_t *bc2);

void run_bitset_container_ixor(
        container_t **c1, uint8_t *type1,
        const bitset_container_t *bc2);

void bitset_run_container_ixor(
        container_t **c1, uint8_t *type1,
        const run_container_t *rc2);

void array_run_container_ixor(
        container_t **c1, uint8_t *type1,
        const run_container_t *rc2);

void run_array_container_ixor(
        container_t **c1, uint8_t *type1,
        const array_container_t *ac2);

void array_array_container_ixor(
        container_t **c1, uint8_t *type1,
        const array_container_t *ac2);

void run_run_container_ixor(
        container_t **c1, uint8_t *type1,
        const run_container_t *ac2);


#ifdef __cplusplus
} } }  // extern "C" { namespace roaring { namespace internal {
#endif

#endif
