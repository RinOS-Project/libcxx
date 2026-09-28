// SPDX-License-Identifier: MIT

#ifndef RIN_LIBCXX_NEW_RUNTIME_H
#define RIN_LIBCXX_NEW_RUNTIME_H

#include "../libc/stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*rin_cxx_new_handler_fn)(void);

rin_cxx_new_handler_fn rin_cxx_get_new_handler(void);
rin_cxx_new_handler_fn rin_cxx_set_new_handler(rin_cxx_new_handler_fn handler);

void* rin_cxx_operator_new(size_t size);
void* rin_cxx_operator_new_nothrow(size_t size);
void* rin_cxx_operator_new_aligned(size_t size, size_t alignment);
void* rin_cxx_operator_new_aligned_nothrow(size_t size, size_t alignment);

#ifdef __cplusplus
}
#endif

#endif
