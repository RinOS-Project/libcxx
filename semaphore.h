/*
 * RinOS C++ <semaphore> ✿
 * セマフォ (C++20)
 */

#ifndef RINCXX_SEMAPHORE_H
#define RINCXX_SEMAPHORE_H

#include "rincxx.h"
#include "version.h"

/* C++20 semaphore declarations and their futex dependencies are not
 * available to C++11--17 consumers. */
#if defined(__cplusplus) && __cplusplus >= 202002L

#include "atomic.h"
#include "chrono.h"
#include "limits.h"
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && \
    __STDC_HOSTED__ && defined(SYS_OPEN)
#pragma push_macro("SYS_OPEN")
#undef SYS_OPEN
#define RINCXX_SEMAPHORE_RESTORE_HOST_SYS_OPEN 1
#endif
#include "../libc/sys/syscall.h"
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && __STDC_HOSTED__
#ifdef RINCXX_SEMAPHORE_RESTORE_HOST_SYS_OPEN
#undef SYS_OPEN
#pragma pop_macro("SYS_OPEN")
#undef RINCXX_SEMAPHORE_RESTORE_HOST_SYS_OPEN
#else
/* Do not leak the Rin target-only uppercase alias into a hosted CRT. */
#undef SYS_OPEN
#endif
#endif
#include "../libc/linux/futex.h"

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * counting_semaphore
 * ═══════════════════════════════════════════════════════════════*/

template<ptrdiff_t LeastMaxValue = numeric_limits<ptrdiff_t>::max()>
class counting_semaphore {
    static_assert(LeastMaxValue >= 0, "LeastMaxValue must be non-negative");
    static_assert(sizeof(unsigned int) == 4,
                  "RinOS futex words must be 32 bits");

    atomic_ptrdiff_t counter_;
    alignas(int) mutable unsigned int wake_word_;

    unsigned int* wake_address() const noexcept {
        return const_cast<unsigned int*>(&wake_word_);
    }

    void publish_wake() noexcept {
        unsigned int generation =
            __atomic_load_n(&wake_word_, __ATOMIC_RELAXED);
        while (generation != ~0U &&
               !__atomic_compare_exchange_n(&wake_word_, &generation,
                                            generation + 1U, false,
                                            __ATOMIC_RELEASE,
                                            __ATOMIC_RELAXED)) {
        }
        syscall(SYS_futex, wake_address(), FUTEX_WAKE_PRIVATE,
                0x7fffffff, NULL, NULL, 0);
    }

public:
    static constexpr ptrdiff_t max() noexcept {
        return LeastMaxValue;
    }

    /* コンストラクタ */
    constexpr explicit counting_semaphore(ptrdiff_t desired) noexcept
        : counter_(desired), wake_word_(0) {}

    /* コピー・ムーブ禁止 */
    counting_semaphore(const counting_semaphore&) = delete;
    counting_semaphore& operator=(const counting_semaphore&) = delete;

    /* release (V操作) — futex WAKE */
    void release(ptrdiff_t update = 1) noexcept {
        counter_.fetch_add(update, memory_order_release);
        publish_wake();
    }

    /* acquire (P操作) — futexブロッキング */
    void acquire() noexcept {
        while (true) {
            ptrdiff_t old = counter_.load(memory_order_acquire);
            if (old > 0) {
                if (counter_.compare_exchange_weak(old, old - 1,
                                                   memory_order_acq_rel,
                                                   memory_order_relaxed)) {
                    return;
                }
            } else {
                const unsigned int generation =
                    __atomic_load_n(&wake_word_, __ATOMIC_ACQUIRE);
                if (counter_.load(memory_order_acquire) == 0) {
                    if (generation == ~0U) {
                        (void)syscall(SYS_sched_yield);
                    } else {
                        syscall(SYS_futex, wake_address(), FUTEX_WAIT_PRIVATE,
                                generation, NULL, NULL, 0);
                    }
                }
            }
        }
    }

    /* try_acquire */
    bool try_acquire() noexcept {
        ptrdiff_t old = counter_.load(memory_order_acquire);
        while (old > 0) {
            if (counter_.compare_exchange_weak(old, old - 1,
                                               memory_order_acquire,
                                               memory_order_relaxed)) {
                return true;
            }
        }
        return false;
    }

    /* try_acquire_for */
    template<typename Rep, typename Period>
    bool try_acquire_for(const chrono::duration<Rep, Period>& rel_time) {
        return try_acquire_until(chrono::steady_clock::now() + rel_time);
    }

    /* try_acquire_until */
    template<typename Clock, typename Duration>
    bool try_acquire_until(const chrono::time_point<Clock, Duration>& abs_time) {
        while (Clock::now() < abs_time) {
            if (try_acquire()) {
                return true;
            }
            (void)syscall(SYS_sched_yield);
        }
        return try_acquire();
    }
};

/* ═══════════════════════════════════════════════════════════════
 * binary_semaphore
 * ═══════════════════════════════════════════════════════════════*/

using binary_semaphore = counting_semaphore<1>;

} /* namespace std */

#endif /* defined(__cplusplus) && __cplusplus >= 202002L */
#endif /* RINCXX_SEMAPHORE_H */
