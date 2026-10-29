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
#include "__futex_wait_queue.h"

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
    mutable detail::__futex_wait_queue waiters_;

public:
    static constexpr ptrdiff_t max() noexcept {
        return LeastMaxValue;
    }

    /* コンストラクタ */
    constexpr explicit counting_semaphore(ptrdiff_t desired) noexcept
        : counter_(desired), waiters_() {}

    /* コピー・ムーブ禁止 */
    counting_semaphore(const counting_semaphore&) = delete;
    counting_semaphore& operator=(const counting_semaphore&) = delete;

    /* release (V操作) — futex WAKE */
    void release(ptrdiff_t update = 1) noexcept {
        counter_.fetch_add(update, memory_order_release);
        if (update > 0) {
            waiters_.lock();
            waiters_.notify_n_locked(update);
            waiters_.unlock();
        }
    }

    /* acquire (P操作) — futexブロッキング */
    void acquire() noexcept {
        if (try_acquire()) return;
        detail::__futex_waiter waiter;
        for (;;) {
            waiters_.lock();
            if (try_acquire()) {
                waiters_.remove_locked(waiter);
                waiters_.unlock();
                return;
            }
            waiters_.enqueue_locked(waiter);
            waiters_.unlock();

            (void)syscall(SYS_futex, &waiter.notified, FUTEX_WAIT_PRIVATE,
                          0, nullptr, nullptr, 0);

            waiters_.lock();
            if (__atomic_load_n(&waiter.notified, __ATOMIC_ACQUIRE) != 0) {
                if (try_acquire()) {
                    waiters_.unlock();
                    return;
                }
                waiters_.enqueue_locked(waiter);
            }
            waiters_.unlock();
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
        detail::__futex_wait_registration registration(waiters_);
        detail::__futex_waiter& waiter = registration.waiter();
        for (;;) {
            const bool before_deadline = Clock::now() < abs_time;
            waiters_.lock();
            if (try_acquire()) {
                waiters_.remove_locked(waiter);
                waiters_.unlock();
                return true;
            }
            if (!before_deadline) {
                waiters_.remove_locked(waiter);
                waiters_.unlock();
                return try_acquire();
            }
            waiters_.enqueue_locked(waiter);
            waiters_.unlock();

            struct timespec deadline = {};
            int operation = FUTEX_WAIT_BITSET_PRIVATE;
            if (!detail::semaphore_deadline_strategy<Clock>::make(
                    abs_time, deadline, operation)) {
                waiters_.lock();
                if (try_acquire()) {
                    waiters_.remove_locked(waiter);
                    waiters_.unlock();
                    return true;
                }
                waiters_.remove_locked(waiter);
                waiters_.unlock();
                return try_acquire();
            }

            (void)syscall(SYS_futex, &waiter.notified, operation,
                          0, &deadline, nullptr, FUTEX_BITSET_MATCH_ANY);
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
