/* SPDX-License-Identifier: MIT */
/*
 * Hosted/target pthread ownership boundary for Rin libcxx.
 */

#ifndef RINCXX_PTHREAD_H
#define RINCXX_PTHREAD_H

#if defined(__cplusplus) && !defined(RIN_FREESTANDING) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__
/* Do not overlay Rin's target pthread/time ABI on a compiler-provided
 * pthread runtime.  The scheduler declaration is used by stop_token and
 * this_thread::yield. */
#include <pthread.h>
#if defined(RIN_LIBC_PTHREAD_HEADER)
/* An include path containing libs/libc can resolve the angle-bracket
 * pthread.h above to Rin's hosted-compatible header.  Keep its scheduler
 * owner paired with that pthread ABI. */
#include "../libc/sched.h"
#define RIN_CXX_HAS_RIN_SCHED 1
#else
#include <sched.h>
#define RIN_CXX_HAS_RIN_SCHED 0
#endif
#else
#include "../libc/pthread.h"
#include "../libc/sched.h"
#define RIN_CXX_HAS_RIN_SCHED 1
#endif

#ifdef __cplusplus
namespace std {
namespace detail {

inline void rin_cxx_scheduler_yield() noexcept
{
#if RIN_CXX_HAS_RIN_SCHED
    rin_sched_yield();
#else
    (void)::sched_yield();
#endif
}

} /* namespace detail */
} /* namespace std */
#endif

#endif /* RINCXX_PTHREAD_H */
