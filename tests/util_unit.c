/*
 * util_unit.c
 *
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "test.h"

#include <roaring/bitset_util.h>
#include <roaring/misc/configreport.h>

#ifdef __cplusplus  // stronger type checking errors if C built in C++ mode
using namespace roaring::internal;
using namespace roaring::misc;
#endif

DEFINE_TEST(setandextract_uint16) {
    const unsigned int bitset_size = 1 << 16;
    const unsigned int bitset_size_in_words =
        bitset_size / (sizeof(uint64_t) * 8);

    for (unsigned int offset = 1; offset < bitset_size; offset++) {
        const unsigned int valsize = bitset_size / offset;
        uint16_t* vals = typed_malloc_n(uint16_t, valsize);
        uint64_t* bitset =
            typed_calloc_n(uint64_t, bitset_size_in_words);
        for (unsigned int k = 0; k < valsize; ++k) {
            vals[k] = (uint16_t)(k * offset);
        }

        bitset_set_list(bitset, vals, valsize);
        uint16_t* newvals = typed_malloc_n(uint16_t, valsize);
        bitset_extract_setbits_uint16(bitset, bitset_size_in_words, newvals, 0);

        for (unsigned int k = 0; k < valsize; ++k) {
            assert_int_equal(newvals[k], vals[k]);
        }

        free(vals);
        free(newvals);
        free(bitset);
    }
}

DEFINE_TEST(setandextract_uint32) {
    const unsigned int bitset_size = 1 << 16;
    const unsigned int bitset_size_in_words =
        bitset_size / (sizeof(uint64_t) * 8);

    for (unsigned int offset = 1; offset < bitset_size; offset++) {
        const unsigned int valsize = bitset_size / offset;
        uint16_t* vals = typed_malloc_n(uint16_t, valsize);
        uint64_t* bitset =
            typed_calloc_n(uint64_t, bitset_size_in_words);

        for (unsigned int k = 0; k < valsize; ++k) {
            vals[k] = (uint16_t)(k * offset);
        }

        bitset_set_list(bitset, vals, valsize);
        uint32_t* newvals = typed_malloc_n(uint32_t, valsize);
        bitset_extract_setbits(bitset, bitset_size_in_words, newvals, 0);

        for (unsigned int k = 0; k < valsize; ++k) {
            assert_int_equal(newvals[k], vals[k]);
        }

        free(vals);
        free(newvals);
        free(bitset);
    }
}

int main() {
    tellmeall();

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(setandextract_uint16),
        cmocka_unit_test(setandextract_uint32),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
