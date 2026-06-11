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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <roaring/memory.h>

// Typed test-only allocation helpers, used so test code can build cleanly
// as C++ without explicit casts.  Can be used with regular free().

#define typed_malloc(T) (T *)malloc(sizeof(T))
#define typed_malloc_n(T, n) (T *)malloc(sizeof(T) * (n))
#define typed_calloc(T) typed_calloc_n(T, 1)
#define typed_calloc_n(T, n) (T *)calloc((n), sizeof(T))

// Install a validating allocator in every test binary so CRoaring's
// hookable malloc/realloc/free paths are exercised instead of accidentally
// falling back to the system allocator.  It also catches when tests are
// doing things like roaring_bitmap_free() on ordinary malloc()'d memory.

typedef struct roaring_test_allocation_header_s {
    size_t size;
    void *raw;
    uintptr_t magic;
} test_alloc_header_t;

enum {
    ROARING_TEST_ALLOCATION_MAGIC = (uintptr_t)0x524f415254455354ull,
    ROARING_TEST_ALLOCATION_FREED = (uintptr_t)0x4652454544454144ull,
};

static size_t roaring_test_live_allocations = 0;

static inline void roaring_test_allocator_fail(const char *message,
                                               const void *ptr) {
    fprintf(stderr, "roaring test allocator error: %s (ptr=%p)\n", message,
            ptr);
    fflush(stderr);
    abort();
}

static inline test_alloc_header_t *roaring_test_allocation_header_from_ptr(
    void *ptr) {
    return *((test_alloc_header_t **)ptr - 1);
}

static inline void *roaring_test_malloc(size_t size) {
    size_t payload_size = size == 0 ? 1 : size;
    size_t total_size = sizeof(test_alloc_header_t) +
                        sizeof(test_alloc_header_t *) + payload_size;
    test_alloc_header_t *header = (test_alloc_header_t *)malloc(total_size);
    if (header == NULL) return NULL;
    header->size = size;
    header->raw = header;
    header->magic = ROARING_TEST_ALLOCATION_MAGIC;
    *((test_alloc_header_t **)(header + 1)) = header;
    roaring_test_live_allocations++;
    return (void *)((test_alloc_header_t **)(header + 1) + 1);
}

static inline void *roaring_test_calloc(size_t n_elements, size_t elem_size) {
    if (n_elements != 0 && elem_size > SIZE_MAX / n_elements) return NULL;
    size_t size = n_elements * elem_size;
    void *ptr = roaring_test_malloc(size);
    if (ptr != NULL) memset(ptr, 0, size == 0 ? 1 : size);
    return ptr;
}

static inline void roaring_test_free(void *ptr) {
    if (ptr == NULL) return;
    test_alloc_header_t *header = roaring_test_allocation_header_from_ptr(ptr);
    if (header->magic != ROARING_TEST_ALLOCATION_MAGIC) {
        roaring_test_allocator_fail("invalid allocation header in free()", ptr);
    }
    header->magic = ROARING_TEST_ALLOCATION_FREED;
    roaring_test_live_allocations--;
    free(header);
}

static inline void *roaring_test_realloc(void *ptr, size_t size) {
    if (ptr == NULL) return roaring_test_malloc(size);
    if (size == 0) {
        roaring_test_free(ptr);
        return NULL;
    }
    test_alloc_header_t *header = roaring_test_allocation_header_from_ptr(ptr);
    if (header->magic != ROARING_TEST_ALLOCATION_MAGIC) {
        roaring_test_allocator_fail("invalid allocation header in realloc()",
                                    ptr);
    }
    size_t old_size = header->size;
    void *new_ptr = roaring_test_malloc(size);
    if (new_ptr == NULL) return NULL;
    memcpy(new_ptr, ptr, old_size < size ? old_size : size);
    roaring_test_free(ptr);
    return new_ptr;
}

static inline void *roaring_test_aligned_malloc(size_t alignment, size_t size) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        fprintf(stderr, "roaring test allocator error: invalid alignment %zu\n",
                alignment);
        fflush(stderr);
        abort();
    }

    size_t payload_size = size == 0 ? 1 : size;
    size_t payload_alignment = alignment < sizeof(test_alloc_header_t *)
                                   ? sizeof(test_alloc_header_t *)
                                   : alignment;
    size_t allocation_size = sizeof(test_alloc_header_t) +
                             sizeof(test_alloc_header_t *) + payload_size +
                             payload_alignment - 1;
    char *raw = typed_malloc_n(char, allocation_size);
    if (raw == NULL) return NULL;

    test_alloc_header_t *header = (test_alloc_header_t *)raw;
    char *payload =
        raw + sizeof(test_alloc_header_t) + sizeof(test_alloc_header_t *);
    uintptr_t aligned = ((uintptr_t)payload + payload_alignment - 1) &
                        ~(uintptr_t)(payload_alignment - 1);
    payload = (char *)aligned;
    header->size = size;
    header->raw = raw;
    header->magic = ROARING_TEST_ALLOCATION_MAGIC;
    *((test_alloc_header_t **)(payload - sizeof(test_alloc_header_t *))) =
        header;
    roaring_test_live_allocations++;
    return (void *)payload;
}

static inline void roaring_test_aligned_free(void *ptr) {
    if (ptr == NULL) return;
    test_alloc_header_t *header = roaring_test_allocation_header_from_ptr(ptr);
    if (header->magic != ROARING_TEST_ALLOCATION_MAGIC) {
        roaring_test_allocator_fail("bad allocation header in aligned_free()",
                                    ptr);
    }
    header->magic = ROARING_TEST_ALLOCATION_FREED;
    roaring_test_live_allocations--;
    free(header->raw);
}

static inline void roaring_test_assert_memory_balanced(void) {
    if (roaring_test_live_allocations != 0) {
        fprintf(stderr,
                "test allocator error: %zu live allocations remain at exit\n",
                roaring_test_live_allocations);
        fflush(stderr);
        abort();
    }
}

static inline void roaring_test_install_memory_hooks(void) {
    static int installed = 0;
    if (installed) return;
    installed = 1;

    roaring_memory_t memory_hook;
    memset(&memory_hook, 0, sizeof(memory_hook));
    memory_hook.malloc = roaring_test_malloc;
    memory_hook.realloc = roaring_test_realloc;
    memory_hook.calloc = roaring_test_calloc;
    memory_hook.free = roaring_test_free;
    memory_hook.aligned_malloc = roaring_test_aligned_malloc;
    memory_hook.aligned_free = roaring_test_aligned_free;
    roaring_init_memory_hook(memory_hook);
    atexit(roaring_test_assert_memory_balanced);
}

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void roaring_test_memory_hook_constructor(void);
__declspec(allocate(".CRT$XCU")) static void (
    *roaring_test_memory_hook_constructor_ptr)(void) =
    roaring_test_memory_hook_constructor;
static void roaring_test_memory_hook_constructor(void) {
    roaring_test_install_memory_hooks();
}
#elif defined(__GNUC__) || defined(__clang__)
static void __attribute__((constructor))
roaring_test_memory_hook_constructor(void) {
    roaring_test_install_memory_hooks();
}
#endif

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
#define cmocka_fail_msg(msg, ...)                       \
    do {                                                \
        print_error("ERROR: " msg "\n", ##__VA_ARGS__); \
        cmocka_fail();                                  \
    } while (0)

#undef skip
#define cmocka_skip() _skip(__FILE__, __LINE__)

#define DESCRIBE_TEST fprintf(stderr, "--- %s\n", __func__)

#define assert_bitmap_validate(b)                                           \
    do {                                                                    \
        const char *internal_reason_buf = NULL;                             \
        if (!roaring_bitmap_internal_validate((b), &internal_reason_buf)) { \
            cmocka_fail_msg("internal validation failed: %s",               \
                            internal_reason_buf);                           \
        }                                                                   \
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
