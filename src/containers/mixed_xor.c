/*
 * mixed_xor.c
 */

#include <assert.h>
#include <string.h>

#include <roaring/bitset_util.h>
#include <roaring/containers/containers.h>
#include <roaring/containers/convert.h>
#include <roaring/containers/mixed_xor.h>
#include <roaring/containers/perfparameters.h>

#ifdef __cplusplus
extern "C" { namespace roaring { namespace internal {
#endif

/* Compute the xor of src_1 and src_2.
 * result_type is the type of the newly generated return container.
 */
container_t *array_bitset_container_xor(
    const array_container_t *src_1, const bitset_container_t *src_2,
    uint8_t *result_type
){
    bitset_container_t *new_bc = bitset_container_create();
    bitset_container_copy(src_2, new_bc);
    new_bc->cardinality = (int32_t)bitset_flip_list_withcard(
                                        new_bc->array, new_bc->cardinality,
                                        src_1->array, src_1->cardinality);

    if (new_bc->cardinality <= DEFAULT_MAX_SIZE) {  // compact into array
        array_container_t *new_ac = array_container_from_bitset(new_bc);
        bitset_container_free(new_bc);
        *result_type = ARRAY_CONTAINER_TYPE;
        return new_ac;
    }

    *result_type = BITSET_CONTAINER_TYPE;
    return new_bc;
}

/* Compute the xor of src_1 and src_2 and write the result to
 * dst. It is allowed for src_2 to be dst.  This version does not
 * update the cardinality of dst (it is set to BITSET_UNKNOWN_CARDINALITY).
 */

void array_bitset_container_lazy_xor(const array_container_t *src_1,
                                     const bitset_container_t *src_2,
                                     bitset_container_t *dst) {
    if (src_2 != dst) bitset_container_copy(src_2, dst);
    bitset_flip_list(dst->array, src_1->array, src_1->cardinality);
    dst->cardinality = BITSET_UNKNOWN_CARDINALITY;
}

/* Compute the xor of src_1 and src_2 and return it.
 * result_type is the type of the newly generated container.
 */

container_t *run_bitset_container_xor(
    const run_container_t *src_1, const bitset_container_t *src_2,
    uint8_t *result_type
){
    bitset_container_t *new_bc = bitset_container_create();

    bitset_container_copy(src_2, new_bc);
    for (int32_t rlepos = 0; rlepos < src_1->n_runs; ++rlepos) {
        rle16_t rle = src_1->runs[rlepos];
        bitset_flip_range(new_bc->array, rle.value,
                          rle.value + rle.length + UINT32_C(1));
    }
    new_bc->cardinality = bitset_container_compute_cardinality(new_bc);

    if (new_bc->cardinality <= DEFAULT_MAX_SIZE) {
        array_container_t *new_ac = array_container_from_bitset(new_bc);
        bitset_container_free(new_bc);
        *result_type = ARRAY_CONTAINER_TYPE;
        return new_ac;
    }

    *result_type = BITSET_CONTAINER_TYPE;
    return new_bc;
}

/* lazy xor.  Dst is initialized and may be equal to src_2.
 *  Result is left as a bitset container, even if actual
 *  cardinality would dictate an array container.
 */

void run_bitset_container_lazy_xor(const run_container_t *src_1,
                                   const bitset_container_t *src_2,
                                   bitset_container_t *dst) {
    if (src_2 != dst) bitset_container_copy(src_2, dst);
    for (int32_t rlepos = 0; rlepos < src_1->n_runs; ++rlepos) {
        rle16_t rle = src_1->runs[rlepos];
        bitset_flip_range(dst->array, rle.value,
                          rle.value + rle.length + UINT32_C(1));
    }
    dst->cardinality = BITSET_UNKNOWN_CARDINALITY;
}

/* dst does not indicate a valid container initially.  Eventually it
 * can become any kind of container.
 */

container_t *array_run_container_xor(
    const array_container_t *ac1, const run_container_t *rc2,
    uint8_t *result_type
){
    // semi following Java XOR implementation as of May 2016
    // the C OR implementation works quite differently and can return a run
    // container
    // TODO could optimize for full run containers.

    // use of lazy following Java impl.
    const int arbitrary_threshold = 32;
    if (ac1->cardinality < arbitrary_threshold) {
        run_container_t *new_rc = run_container_create();
        array_run_container_lazy_xor(ac1, rc2, new_rc);  // keeps runs
        return convert_run_to_efficient_container_and_free(new_rc, result_type);
    }

    int card = run_container_cardinality(rc2);
    if (card <= DEFAULT_MAX_SIZE) {
        // Java implementation works with the array, xoring the run elements via
        // iterator
        array_container_t *temp = array_container_from_run(rc2);
        container_t *ans = array_array_container_xor(temp, ac1, result_type);
        array_container_free(temp);
        return ans;
    }
    
    // guess that it will end up as a bitset
    container_t *ans = bitset_container_from_run(rc2);
    *result_type = BITSET_CONTAINER_TYPE;
    bitset_array_container_ixor(&ans, result_type, ac1);
    return ans;
}

/* Dst is a valid run container. (Can it be src_2? Let's say not.)
 * Leaves result as run container, even if other options are
 * smaller.
 */

void array_run_container_lazy_xor(const array_container_t *src_1,
                                  const run_container_t *src_2,
                                  run_container_t *dst) {
    run_container_grow(dst, src_1->cardinality + src_2->n_runs, false);
    int32_t rlepos = 0;
    int32_t arraypos = 0;
    dst->n_runs = 0;

    while ((rlepos < src_2->n_runs) && (arraypos < src_1->cardinality)) {
        if (src_2->runs[rlepos].value <= src_1->array[arraypos]) {
            run_container_smart_append_exclusive(dst, src_2->runs[rlepos].value,
                                                 src_2->runs[rlepos].length);
            rlepos++;
        } else {
            run_container_smart_append_exclusive(dst, src_1->array[arraypos],
                                                 0);
            arraypos++;
        }
    }
    while (arraypos < src_1->cardinality) {
        run_container_smart_append_exclusive(dst, src_1->array[arraypos], 0);
        arraypos++;
    }
    while (rlepos < src_2->n_runs) {
        run_container_smart_append_exclusive(dst, src_2->runs[rlepos].value,
                                             src_2->runs[rlepos].length);
        rlepos++;
    }
}

/* dst does not indicate a valid container initially.  Eventually it
 * can become any kind of container.
 */

container_t *run_run_container_xor(
    const run_container_t *src_1, const run_container_t *src_2,
    uint8_t *result_type
){
    run_container_t *new_rc = run_container_create();
    run_container_xor(src_1, src_2, new_rc);
    return convert_run_to_efficient_container_and_free(new_rc, result_type);
}

/*
 * Java implementation (as of May 2016) for array_run, run_run
 * and  bitset_run don't do anything different for inplace.
 * Could adopt the mixed_union.c approach instead (ie, using
 * smart_append_exclusive)
 *
 */

container_t *array_array_container_xor(
    const array_container_t *src_1, const array_container_t *src_2,
    uint8_t *result_type
){
    int totalCardinality =
        src_1->cardinality + src_2->cardinality;  // upper bound
    if (totalCardinality <= DEFAULT_MAX_SIZE) {
        array_container_t *new_ac =
                array_container_create_given_capacity(totalCardinality);
        array_container_xor(src_1, src_2, new_ac);
        return new_ac;
    }
    bitset_container_t *new_bc = bitset_container_from_array(src_1);
    new_bc->cardinality = (uint32_t)bitset_flip_list_withcard(
                                        new_bc->array, new_bc->cardinality,
                                        src_2->array, src_2->cardinality);

    if (new_bc->cardinality <= DEFAULT_MAX_SIZE) {  // need to convert!
        array_container_t *new_ac = array_container_from_bitset(new_bc);
        bitset_container_free(new_bc);
        *result_type = ARRAY_CONTAINER_TYPE;
        return new_ac;
    }

    *result_type = BITSET_CONTAINER_TYPE;
    return new_bc;
}

bool array_array_container_lazy_xor(
    const array_container_t *src_1, const array_container_t *src_2,
    container_t **dst
){
    int totalCardinality = src_1->cardinality + src_2->cardinality;
    // upper bound, but probably poor estimate for xor
    if (totalCardinality <= ARRAY_LAZY_LOWERBOUND) {
        *dst = array_container_create_given_capacity(totalCardinality);
        if (*dst != NULL)
            array_container_xor(src_1, src_2, CAST_array(*dst));
        return false;  // not a bitset
    }
    *dst = bitset_container_from_array(src_1);
    bool returnval = true;  // expect a bitset (maybe, for XOR??)
    if (*dst != NULL) {
        bitset_container_t *ourbitset = CAST_bitset(*dst);
        bitset_flip_list(ourbitset->array, src_2->array, src_2->cardinality);
        ourbitset->cardinality = BITSET_UNKNOWN_CARDINALITY;
    }
    return returnval;
}

/* Compute the xor of src_1 and src_2 and write the result to
 * dst (which has no container initially). Return value is
 * "dst is a bitset"
 */

container_t *bitset_bitset_container_xor(
    const bitset_container_t *src_1, const bitset_container_t *src_2,
    uint8_t *result_type
){
    bitset_container_t *new_bc = bitset_container_create();
    int card = bitset_container_xor(src_1, src_2, new_bc);
    if (card <= DEFAULT_MAX_SIZE) {
        array_container_t *new_ac = array_container_from_bitset(new_bc);
        bitset_container_free(new_bc);
        *result_type = ARRAY_CONTAINER_TYPE;
        return new_ac;
    } else {
        *result_type = BITSET_CONTAINER_TYPE;
        return new_bc;
    }
}

/* Compute the xor of src_1 and src_2 and write the result to
 * dst (which has no container initially).  It will modify src_1
 * to be dst if the result is a bitset.  Otherwise, it will
 * free src_1 and dst will be a new array container.  In both
 * cases, the caller is responsible for deallocating dst.
 * Returns true iff dst is a bitset  */

void bitset_array_container_ixor(
    container_t **c1, uint8_t *type1,
    const array_container_t *ac2
){
    assert(*type1 == BITSET_CONTAINER_TYPE);
    bitset_container_t *bc1 = *movable_CAST_bitset(c1);

    bc1->cardinality = (uint32_t)bitset_flip_list_withcard(
                                        bc1->array, bc1->cardinality,
                                        ac2->array, ac2->cardinality);

    if (bc1->cardinality > DEFAULT_MAX_SIZE) {  // array won't be smaller
        assert(*type1 == BITSET_CONTAINER_TYPE);
        return;
    }

    *c1 = array_container_from_bitset(bc1);
    bitset_container_free(bc1);
}

/* a bunch of in-place, some of which may not *really* be inplace.
 * TODO: write actual inplace routine if efficiency warrants it
 * Anything inplace with a bitset is a good candidate
 */

void bitset_bitset_container_ixor(
    container_t **c1, uint8_t *type1,
    const bitset_container_t *bc2
){
    assert(*type1 == BITSET_CONTAINER_TYPE);
    bitset_container_t *bc1 = *movable_CAST_bitset(c1);
    *c1 = bitset_bitset_container_xor(bc1, bc2, type1);
    bitset_container_free(bc1);
}

void array_bitset_container_ixor(
    container_t **c1, uint8_t *type1,
    const bitset_container_t *bc2
){
    assert(*type1 == ARRAY_CONTAINER_TYPE);
    array_container_t *ac1 = *movable_CAST_array(c1);
    *c1 = array_bitset_container_xor(ac1, bc2, type1);
    array_container_free(ac1);
}

void run_bitset_container_ixor(
    container_t **c1, uint8_t *type1,
    const bitset_container_t *bc2
){
    assert(*type1 == RUN_CONTAINER_TYPE);
    run_container_t *rc1 = *movable_CAST_run(c1);
    *c1 = run_bitset_container_xor(rc1, bc2, type1);
    run_container_free(rc1);
}

void bitset_run_container_ixor(
    container_t **c1, uint8_t *type1,
    const run_container_t *rc2
){
    assert(*type1 == BITSET_CONTAINER_TYPE);
    bitset_container_t *bc1 = *movable_CAST_bitset(c1);
    *c1 = run_bitset_container_xor(rc2, bc1, type1);
    bitset_container_free(bc1);
}

void array_run_container_ixor(
    container_t **c1, uint8_t *type1,
    const run_container_t *rc2
){
    assert(*type1 == ARRAY_CONTAINER_TYPE);
    array_container_t *ac1 = *movable_CAST_array(c1);
    *c1 = array_run_container_xor(ac1, rc2, type1);
    array_container_free(ac1);
}

void run_array_container_ixor(
    container_t **c1, uint8_t *type1,
    const array_container_t *ac2
){
    assert(*type1 == RUN_CONTAINER_TYPE);
    run_container_t *rc1 = *movable_CAST_run(c1);
    *c1 = array_run_container_xor(ac2, rc1, type1);
    run_container_free(rc1);
}

void array_array_container_ixor(
    container_t **c1, uint8_t *type1,
    const array_container_t *ac2
){
    assert(*type1 == ARRAY_CONTAINER_TYPE);
    array_container_t *ac1 = *movable_CAST_array(c1);
    *c1 = array_array_container_xor(ac1, ac2, type1);
    array_container_free(ac1);
}

void run_run_container_ixor(
    container_t **c1, uint8_t *type1,
    const run_container_t *rc2
){
    assert(*type1 == RUN_CONTAINER_TYPE);
    run_container_t *rc1 = *movable_CAST_run(c1);
    *c1 = run_run_container_xor(rc1, rc2, type1);
    run_container_free(rc1);
}


#ifdef __cplusplus
} } }  // extern "C" { namespace roaring { namespace internal {
#endif
