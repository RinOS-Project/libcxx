// SPDX-License-Identifier: MIT

#include "new_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void* rin_malloc(size_t size);
extern void* rin_aligned_alloc(size_t size, size_t alignment);
extern void rin_log(const char* message);
extern void rin_exit(int status) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

static rin_cxx_new_handler_fn rin_cxx_new_handler_state;

static int rin_cxx_valid_alignment(size_t alignment) {
    return alignment >= sizeof(void*) &&
           (alignment & (alignment - 1u)) == 0u;
}

rin_cxx_new_handler_fn rin_cxx_get_new_handler(void) {
    return __atomic_load_n(&rin_cxx_new_handler_state, __ATOMIC_SEQ_CST);
}

rin_cxx_new_handler_fn rin_cxx_set_new_handler(rin_cxx_new_handler_fn handler) {
    return __atomic_exchange_n(&rin_cxx_new_handler_state, handler,
                               __ATOMIC_SEQ_CST);
}

static void* rin_cxx_allocate(size_t size, size_t alignment,
                              int aligned, int nothrow) {
    if (size == 0u) size = 1u;

    /* An invalid align_val_t must not reach the platform allocator.  Apart
     * from avoiding provider-specific behavior, this keeps the nothrow
     * overload failure-atomic (no callback or handler is invoked). */
    if (aligned && !rin_cxx_valid_alignment(alignment)) {
        if (nothrow) return (void*)0;
        rin_log("[NEW] Invalid alignment\n");
        rin_exit(1);
        __builtin_trap();
    }

    for (;;) {
        void* allocation = aligned
            ? rin_aligned_alloc(size, alignment)
            : rin_malloc(size);
        rin_cxx_new_handler_fn handler;

        if (allocation) return allocation;

        handler = rin_cxx_get_new_handler();
        if (handler) {
            handler();
            continue;
        }
        if (nothrow) return (void*)0;

        rin_log("[NEW] Out of memory\n");
        rin_exit(1);
        __builtin_trap();
    }
}

void* rin_cxx_operator_new(size_t size) {
    return rin_cxx_allocate(size, 0u, 0, 0);
}

void* rin_cxx_operator_new_nothrow(size_t size) {
    return rin_cxx_allocate(size, 0u, 0, 1);
}

void* rin_cxx_operator_new_aligned(size_t size, size_t alignment) {
    return rin_cxx_allocate(size, alignment, 1, 0);
}

void* rin_cxx_operator_new_aligned_nothrow(size_t size, size_t alignment) {
    return rin_cxx_allocate(size, alignment, 1, 1);
}
