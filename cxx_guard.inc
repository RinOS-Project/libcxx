/* SPDX-License-Identifier: MIT */
/*
 * Itanium C++ ABI local-static guard transaction.
 *
 * The first byte is the compiler-visible initialized flag.  The second byte
 * is an internal pending flag.  Remaining ABI bytes are never modified.
 * Define RIN_CXX_GUARD_WAIT() before inclusion to yield while contended.
 */

#ifndef RIN_CXX_GUARD_WAIT
#define RIN_CXX_GUARD_WAIT() ((void)0)
#define RIN_CXX_GUARD_WAIT_WAS_DEFAULT 1
#endif

#define RIN_CXX_GUARD_INITIALIZED 1u
#define RIN_CXX_GUARD_PENDING 1u
#define RIN_CXX_GUARD_SPINS_BEFORE_WAIT 1024u

static void rin_cxx_guard_pause(void) {
#if defined(__i386__) || defined(__x86_64__)
    __asm__ __volatile__("pause");
#else
    __atomic_signal_fence(__ATOMIC_SEQ_CST);
#endif
}

int __cxa_guard_acquire(long long* guard) {
    unsigned char* bytes = (unsigned char*)guard;
    unsigned int spins = 0u;

    if (__atomic_load_n(&bytes[0], __ATOMIC_ACQUIRE) != 0u) return 0;

    for (;;) {
        unsigned char expected = 0u;
        if (__atomic_compare_exchange_n(
                &bytes[1], &expected, RIN_CXX_GUARD_PENDING, 0,
                __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            if (__atomic_load_n(&bytes[0], __ATOMIC_ACQUIRE) != 0u) {
                __atomic_store_n(&bytes[1], 0u, __ATOMIC_RELEASE);
                return 0;
            }
            return 1;
        }

        while (__atomic_load_n(&bytes[1], __ATOMIC_ACQUIRE) != 0u) {
            rin_cxx_guard_pause();
            if (++spins == RIN_CXX_GUARD_SPINS_BEFORE_WAIT) {
                RIN_CXX_GUARD_WAIT();
                spins = 0u;
            }
        }
        if (__atomic_load_n(&bytes[0], __ATOMIC_ACQUIRE) != 0u) return 0;
    }
}

void __cxa_guard_release(long long* guard) {
    unsigned char* bytes = (unsigned char*)guard;
    __atomic_store_n(&bytes[0], RIN_CXX_GUARD_INITIALIZED, __ATOMIC_RELEASE);
    __atomic_store_n(&bytes[1], 0u, __ATOMIC_RELEASE);
}

void __cxa_guard_abort(long long* guard) {
    unsigned char* bytes = (unsigned char*)guard;
    __atomic_store_n(&bytes[1], 0u, __ATOMIC_RELEASE);
}

#undef RIN_CXX_GUARD_SPINS_BEFORE_WAIT
#undef RIN_CXX_GUARD_PENDING
#undef RIN_CXX_GUARD_INITIALIZED

#ifdef RIN_CXX_GUARD_WAIT_WAS_DEFAULT
#undef RIN_CXX_GUARD_WAIT_WAS_DEFAULT
#undef RIN_CXX_GUARD_WAIT
#endif
