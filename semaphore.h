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

namespace detail {

template<typename Rep, typename Period>
inline bool semaphore_duration_to_nanoseconds(
    const chrono::duration<Rep, Period>& value,
    long long& nanoseconds) noexcept {
    const Rep count = value.count();
    if (!(count > Rep(0))) {
        nanoseconds = 0;
        return false;
    }
    const long double scaled =
        static_cast<long double>(count) *
        static_cast<long double>(Period::num) * 1000000000.0L /
        static_cast<long double>(Period::den);
    if (!(scaled > 0.0L)) {
        nanoseconds = 0;
        return false;
    }
    const long double maximum = static_cast<long double>(
        numeric_limits<long long>::max());
    nanoseconds = scaled >= maximum
        ? numeric_limits<long long>::max()
        : static_cast<long long>(scaled);
    return nanoseconds > 0;
}

inline chrono::steady_clock::time_point semaphore_steady_deadline(
    long long now, long long relative_nanoseconds) noexcept {
    const long long maximum = numeric_limits<long long>::max();
    if (relative_nanoseconds <= 0)
        return chrono::steady_clock::time_point(chrono::nanoseconds(now));
    if (now > maximum - relative_nanoseconds)
        return chrono::steady_clock::time_point(chrono::nanoseconds(maximum));
    return chrono::steady_clock::time_point(
        chrono::nanoseconds(now + relative_nanoseconds));
}

template<typename Duration>
inline bool semaphore_epoch_timespec(
    const Duration& since_epoch, struct timespec& deadline) noexcept {
    const long double count = static_cast<long double>(since_epoch.count());
    if (!(count > 0.0L)) return false;
    const long double seconds = count *
        static_cast<long double>(Duration::period::num) /
        static_cast<long double>(Duration::period::den);
    if (!(seconds > 0.0L)) return false;
    using seconds_type = decltype(deadline.tv_sec);
    const long double maximum_seconds = static_cast<long double>(
        numeric_limits<seconds_type>::max());
    if (seconds >= maximum_seconds) {
        deadline.tv_sec = numeric_limits<seconds_type>::max();
        deadline.tv_nsec = 999999999L;
        return true;
    }
    const long long whole_seconds = static_cast<long long>(seconds);
    long long nanoseconds = static_cast<long long>(
        (seconds - static_cast<long double>(whole_seconds)) *
        1000000000.0L);
    if (nanoseconds < 0) nanoseconds = 0;
    if (nanoseconds >= 1000000000LL) {
        if (whole_seconds >= static_cast<long long>(
                numeric_limits<seconds_type>::max())) {
            deadline.tv_sec = numeric_limits<seconds_type>::max();
            deadline.tv_nsec = 999999999L;
            return true;
        }
        deadline.tv_sec = static_cast<seconds_type>(whole_seconds + 1LL);
        nanoseconds -= 1000000000LL;
    } else {
        deadline.tv_sec = static_cast<seconds_type>(whole_seconds);
    }
    deadline.tv_nsec = static_cast<long>(nanoseconds);
    return true;
}

template<typename Clock>
struct semaphore_deadline_strategy {
    template<typename Duration>
    static bool make(
        const chrono::time_point<Clock, Duration>& timeout_time,
        struct timespec& deadline, int& operation) noexcept {
        const auto now = Clock::now();
        if (!(now < timeout_time)) return false;
        long long relative_nanoseconds = 0;
        if (!semaphore_duration_to_nanoseconds(timeout_time - now,
                                               relative_nanoseconds))
            return false;
        /* A user-defined clock may advance, stop, or jump while the thread
         * sleeps.  Re-sample it regularly without returning to a busy loop. */
        constexpr long long clock_recheck_nanoseconds = 10000000LL;
        if (relative_nanoseconds > clock_recheck_nanoseconds)
            relative_nanoseconds = clock_recheck_nanoseconds;
        const long long monotonic_now = chrono::steady_clock::now()
            .time_since_epoch().count();
        const auto monotonic_deadline = semaphore_steady_deadline(
            monotonic_now, relative_nanoseconds);
        operation = FUTEX_WAIT_BITSET_PRIVATE;
        return semaphore_epoch_timespec(
            monotonic_deadline.time_since_epoch(), deadline);
    }
};

inline void semaphore_wait_after_generation_saturation(
    unsigned int* address, unsigned int generation) noexcept {
    /* The generation intentionally cannot wrap.  A bounded relative sleep
     * prevents a release racing the wait from being lost indefinitely. */
    struct timespec retry = {};
    retry.tv_nsec = 1000000L;
    syscall(SYS_futex, address, FUTEX_WAIT_PRIVATE, generation,
            &retry, NULL, 0);
}

template<>
struct semaphore_deadline_strategy<chrono::steady_clock> {
    template<typename Duration>
    static bool make(
        const chrono::time_point<chrono::steady_clock, Duration>& timeout_time,
        struct timespec& deadline, int& operation) noexcept {
        if (!(chrono::steady_clock::now() < timeout_time)) return false;
        operation = FUTEX_WAIT_BITSET_PRIVATE;
        return semaphore_epoch_timespec(timeout_time.time_since_epoch(),
                                        deadline);
    }
};

template<>
struct semaphore_deadline_strategy<chrono::system_clock> {
    template<typename Duration>
    static bool make(
        const chrono::time_point<chrono::system_clock, Duration>& timeout_time,
        struct timespec& deadline, int& operation) noexcept {
        if (!(chrono::system_clock::now() < timeout_time)) return false;
        operation = FUTEX_WAIT_BITSET_PRIVATE | FUTEX_CLOCK_REALTIME;
        return semaphore_epoch_timespec(timeout_time.time_since_epoch(),
                                        deadline);
    }
};

} /* namespace detail */

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
                        detail::semaphore_wait_after_generation_saturation(
                            wake_address(), generation);
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
        long long relative_nanoseconds = 0;
        if (!detail::semaphore_duration_to_nanoseconds(
                rel_time, relative_nanoseconds))
            return try_acquire();
        const long long now = chrono::steady_clock::now()
                                 .time_since_epoch().count();
        return try_acquire_until(detail::semaphore_steady_deadline(
            now, relative_nanoseconds));
    }

    /* try_acquire_until */
    template<typename Clock, typename Duration>
    bool try_acquire_until(const chrono::time_point<Clock, Duration>& abs_time) {
        if (try_acquire()) return true;
        for (;;) {
            if (!(Clock::now() < abs_time)) return try_acquire();
            const unsigned int generation =
                __atomic_load_n(&wake_word_, __ATOMIC_ACQUIRE);
            if (counter_.load(memory_order_acquire) != 0) continue;

            if (generation == ~0U) {
                detail::semaphore_wait_after_generation_saturation(
                    wake_address(), generation);
                continue;
            }

            struct timespec deadline = {};
            int operation = FUTEX_WAIT_BITSET_PRIVATE;
            if (!detail::semaphore_deadline_strategy<Clock>::make(
                    abs_time, deadline, operation))
                return try_acquire();
            if (counter_.load(memory_order_acquire) != 0) continue;
            syscall(SYS_futex, wake_address(), operation, generation,
                    &deadline, NULL, FUTEX_BITSET_MATCH_ANY);
        }
    }
};

/* ═══════════════════════════════════════════════════════════════
 * binary_semaphore
 * ═══════════════════════════════════════════════════════════════*/

using binary_semaphore = counting_semaphore<1>;

} /* namespace std */

#endif /* defined(__cplusplus) && __cplusplus >= 202002L */
#endif /* RINCXX_SEMAPHORE_H */
