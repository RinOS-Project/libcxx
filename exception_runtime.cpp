/* SPDX-License-Identifier: MIT */
/* Out-of-line standard exception ABI anchors for the freestanding libcxx. */

#include "exception.h"

namespace std {

/*
 * bad_alloc is thrown by <new>.  Making its destructor the key function
 * gives the Itanium ABI one owning translation unit for its vtable and RTTI
 * instead of leaving _ZTISt9bad_alloc as a dynamic import from the host C++
 * runtime.  The class layout remains the standard single-inheritance
 * exception layout.
 */
bad_alloc::~bad_alloc() noexcept = default;

} /* namespace std */
