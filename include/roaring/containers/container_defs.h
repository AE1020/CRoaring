/*
 * container_defs.h
 *
 * Unlike containers.h (which is a file aggregating all the container includes,
 * like array.h, bitset.h, and run.h) this is a file included BY those headers
 * to do things like define the container base class `container_t`.
 */

#ifndef INCLUDE_CONTAINERS_CONTAINER_DEFS_H_
#define INCLUDE_CONTAINERS_CONTAINER_DEFS_H_

// container_defs.h is the first file that isn't part of the public API, so
// it's where we want to put configuration information before the single
// inclusion of needful.h for the internal code.
//
#define NEEDFUL_CAST_SHORTHANDS 1
#include "../needful.h"

// The preferences are a separate file to separate out tweakable parameters
#include <roaring/containers/perfparameters.h>

// helper macros which gloss C++ vs. C difference for void pointer downcast
// (and makes the code look nicer)
//
#define roaring_typed_malloc(T) ((T*)roaring_malloc(sizeof(T)))
#define roaring_typed_malloc_n(T, n) ((T*)roaring_malloc(sizeof(T) * (n)))
#define roaring_typed_realloc_n(T, p, n) \
    ((T*)roaring_realloc((p), sizeof(T) * (n)))

#ifdef __cplusplus
namespace roaring {
namespace internal {  // No extern "C" (contains template)
#endif

/*
 * Since roaring_array_t's definition is not opaque, the container type is
 * part of the API.  If it's not going to be `void*` then it needs a name, and
 * expectations are to prefix C library-exported names with `roaring_` etc.
 *
 * Rather than force the whole codebase to use the name `roaring_container_t`,
 * the few API appearances use the macro ROARING_CONTAINER_T.  Those includes
 * are prior to containers.h, so make a short private alias of `container_t`.
 * Then undefine the awkward macro so it's not used any more than it has to be.
 */
typedef ROARING_CONTAINER_T container_t;

/*
 * See ROARING_CONTAINER_T for notes on using container_t as a base class.
 * This macro helps make the following pattern look nicer:
 *
 *     #ifdef __cplusplus
 *     struct roaring_array_s : public container_t {
 *     #else
 *     struct roaring_array_s {
 *     #endif
 *         int32_t cardinality;
 *         int32_t capacity;
 *         uint16_t *array;
 *     }
 */
#if __cplusplus
#define STRUCT_CONTAINER(name) struct name : public container_t /* { ... } */
#else
#define STRUCT_CONTAINER(name) struct name /* { ... } */
#endif

#ifdef __cplusplus
}
}  // namespace roaring { namespace internal {
#endif

#endif /* INCLUDE_CONTAINERS_CONTAINER_DEFS_H_ */
