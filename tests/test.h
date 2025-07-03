//
// This test.h file is included by all the unit tests.
//
// It contains helpers for working with the cmocka unit tests.  Since that is
// a third party file, putting common macros and functions here is better
// than changing it.
//

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

/*
 * Typed test-only allocation helpers, used so test code can build cleanly
 * as C++ without explicit casts.  Can be used with regular free().
 */
#define typed_malloc(T)        (T*)malloc(sizeof(T))
#define typed_malloc_n(T, n)   (T*)malloc(sizeof(T) * (n))
#define typed_calloc(T)        typed_calloc_n(T, 1)
#define typed_calloc_n(T, n)   (T*)calloc((n), sizeof(T))

#ifdef __cplusplus
//
// It's generally not good to span a header file with `extern "C"`, but
// this is how the cpp_unit.cpp test was doing it.  cmocka.h apparently
// only has #ifdefs for extern "C" under MSC (?)
//
extern "C" {
#include <cmocka.h>
}
#else
#include <cmocka.h>
#endif

// Patch cmocka to not get the final say on high-value words that might be
// useful in the code being tested (fail, skip, fail_msg...)
//
// (test.h should be included before the Roaring headers, otherwise there
// would be errors of cmocka #define'ing fail over top of Roaring's fail())

#undef fail
#define cmocka_fail() _fail(__FILE__, __LINE__)

#undef fail_msg
#define cmocka_fail_msg(msg, ...) do { \
    print_error("ERROR: " msg "\n", ##__VA_ARGS__); \
    cmocka_fail(); \
} while (0)

#undef skip
#define cmocka_skip() _skip(__FILE__, __LINE__)


#define DESCRIBE_TEST fprintf(stderr, "--- %s\n", __func__)

#define assert_bitmap_validate(b)                                            \
    do {                                                                     \
        const char *internal_reason_buf = NULL;                              \
        if (!roaring_bitmap_internal_validate((b), &internal_reason_buf)) {  \
            cmocka_fail_msg("internal validation failed: %s",                \
                internal_reason_buf);                                        \
        }                                                                    \
    } while (0)

// The "cmocka" test functions are supposed to look like:
//
//      void test_function(void **state)
//
// Originally the C tests would declare it like:
//
//      void test_function()
//
// But in C++ that would not match the type, so the C++ tests declared as:
//
//      void test_function(void **)
//
// There's a problem if you're trying to write code that will compile in
// either C or C++, because it's not legal in C99 to not name a parameter...
// and if you give it a name, then there will be complaints that the paramter
// is not used.
//
// Disabling bad cast warnings in C++ defeats the point of compiling in C++,
// and knowing when parameters aren't referenced is useful even in tests.  So
// rather than disabling warnings, this defines a macro to declare the tests.
//
#ifdef __cplusplus
#define DEFINE_TEST(name) static void name(void **)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
// In C23, `void name()` declares a function taking no arguments, which is
// incompatible with cmocka's `void (*)(void **)`. C23 lets us name the
// parameter and mark it unused to keep the cmocka-compatible signature.
#define DEFINE_TEST(name) static void name([[maybe_unused]] void **state)
#else
#define DEFINE_TEST(name) static void name()
#endif
