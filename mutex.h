/*
 * RinOS C++ <mutex> ✿
 * 相互排除 - pthread/futexベース実装
 */

#ifndef RINCXX_MUTEX_H
#define RINCXX_MUTEX_H

#include "rincxx.h"
#include "__pthread.h"
#include "system_error.h"
#include "chrono.h"
#if defined(RIN_FREESTANDING)
#include "../libc/sys/syscall.h"
#include "../libc/linux/futex.h"
#define RIN_MUTEX_HAS_LINUX_FUTEX 1
#elif defined(RIN_HOST_LINUX_FUTEX_TEST) && defined(__has_include)
#if __has_include(<sys/syscall.h>) && __has_include(<linux/futex.h>) && \
    __has_include(<unistd.h>)
#include <sys/syscall.h>
#include <linux/futex.h>
#include <unistd.h>
#define RIN_MUTEX_HAS_LINUX_FUTEX 1
#endif
#endif

namespace std {

namespace detail {

[[noreturn]] inline void mutex_operation_failed(int error,
                                                 const char* operation) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw system_error(error, system_category(), operation);
#else
    (void)error;
    (void)operation;
    terminate();
#endif
}

template<class Rep, class Period>
inline bool mutex_duration_to_nanoseconds(
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

inline chrono::steady_clock::time_point mutex_steady_deadline_from_now(
    long long now, long long relative_nanoseconds) noexcept {
    const long long maximum = numeric_limits<long long>::max();
    if (relative_nanoseconds <= 0)
        return chrono::steady_clock::time_point(chrono::nanoseconds(now));
    if (now > maximum - relative_nanoseconds)
        return chrono::steady_clock::time_point(chrono::nanoseconds(maximum));
    return chrono::steady_clock::time_point(
        chrono::nanoseconds(now + relative_nanoseconds));
}

inline bool mutex_realtime_deadline(long long relative_nanoseconds,
                                    struct timespec& deadline) {
    constexpr long long billion = 1000000000LL;
    if (relative_nanoseconds <= 0) return false;
    struct timespec now = {};
    if (::clock_gettime(CLOCK_REALTIME, &now) != 0) {
        const int error = errno != 0 ? errno : EIO;
        mutex_operation_failed(error, "timed mutex realtime clock");
    }
    if (now.tv_nsec < 0 || now.tv_nsec >= billion) {
        mutex_operation_failed(EIO, "timed mutex invalid realtime clock");
    }

    using seconds_type = decltype(now.tv_sec);
    const long long maximum_seconds = static_cast<long long>(
        numeric_limits<seconds_type>::max());
    const long long delta_seconds = relative_nanoseconds / billion;
    const long long delta_nanoseconds = relative_nanoseconds % billion;
    const long long current_seconds = static_cast<long long>(now.tv_sec);
    if (delta_seconds > maximum_seconds ||
        current_seconds > maximum_seconds - delta_seconds) {
        deadline.tv_sec = numeric_limits<seconds_type>::max();
        deadline.tv_nsec = static_cast<long>(billion - 1);
        return true;
    }

    deadline.tv_sec = static_cast<seconds_type>(
        current_seconds + delta_seconds);
    long long nanoseconds = static_cast<long long>(now.tv_nsec) +
                            delta_nanoseconds;
    if (nanoseconds >= billion) {
        if (deadline.tv_sec == numeric_limits<seconds_type>::max()) {
            deadline.tv_nsec = static_cast<long>(billion - 1);
            return true;
        }
        ++deadline.tv_sec;
        nanoseconds -= billion;
    }
    deadline.tv_nsec = static_cast<long>(nanoseconds);
    return true;
}

template<class Clock, class Duration>
inline bool mutex_timepoint_realtime_deadline(
    const chrono::time_point<Clock, Duration>& timeout_time,
    struct timespec& deadline) {
    const auto now = Clock::now();
    if (!(now < timeout_time)) return false;
    long long relative_nanoseconds = 0;
    if (!mutex_duration_to_nanoseconds(timeout_time - now,
                                       relative_nanoseconds))
        return false;
    return mutex_realtime_deadline(relative_nanoseconds, deadline);
}

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * mutex - pthread_mutex_t wrapper, or a directly exercised futex owner
 * ═══════════════════════════════════════════════════════════════*/

#if defined(RIN_HOST_LINUX_FUTEX_TEST)

namespace __detail {

inline int* mutex_futex_address(volatile int* state) noexcept {
    return (int*)state;
}

inline void mutex_futex_wait(volatile int* state) noexcept {
    (void)syscall(SYS_futex, mutex_futex_address(state), FUTEX_WAIT_PRIVATE, 2,
                  nullptr, nullptr, 0);
}

inline void mutex_futex_wake(volatile int* state) noexcept {
    (void)syscall(SYS_futex, mutex_futex_address(state), FUTEX_WAKE_PRIVATE, 1,
                  nullptr, nullptr, 0);
}

} /* namespace __detail */

class mutex {
    /* 0 = unlocked, 1 = uncontended owner, 2 = contended owner. */
    volatile int state_;

public:
    mutex() noexcept : state_(0) {}

    mutex(const mutex&) = delete;
    mutex& operator=(const mutex&) = delete;

    void lock() {
        int expected = 0;
        if (__atomic_compare_exchange_n(&state_, &expected, 1, false,
                                        __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED)) {
            return;
        }

        for (;;) {
            expected = 0;
            if (__atomic_compare_exchange_n(&state_, &expected, 2, false,
                                            __ATOMIC_ACQUIRE,
                                            __ATOMIC_RELAXED)) {
                return;
            }
            if (expected == 1) {
                expected = __atomic_exchange_n(&state_, 2,
                                               __ATOMIC_ACQUIRE);
                if (expected == 0) return;
            }
            __detail::mutex_futex_wait(&state_);
        }
    }

    bool try_lock() noexcept {
        int expected = 0;
        return __atomic_compare_exchange_n(&state_, &expected, 1, false,
                                           __ATOMIC_ACQUIRE,
                                           __ATOMIC_RELAXED);
    }

    void unlock() noexcept {
        const int previous = __atomic_exchange_n(&state_, 0,
                                                  __ATOMIC_RELEASE);
        if (previous == 2) __detail::mutex_futex_wake(&state_);
    }
};

#else

class mutex {
    pthread_mutex_t mtx_;

public:
    mutex() noexcept : mtx_(PTHREAD_MUTEX_INITIALIZER) {}

    mutex(const mutex&) = delete;
    mutex& operator=(const mutex&) = delete;

    void lock() {
        const int result = pthread_mutex_lock(&mtx_);
        if (result != 0) {
            detail::mutex_operation_failed(result, "mutex lock");
        }
    }

    bool try_lock() noexcept {
        return pthread_mutex_trylock(&mtx_) == 0;
    }

    void unlock() noexcept {
        pthread_mutex_unlock(&mtx_);
    }

    using native_handle_type = pthread_mutex_t*;
    native_handle_type native_handle() noexcept { return &mtx_; }
};

#endif

/* ═══════════════════════════════════════════════════════════════
 * recursive_mutex - pthread_mutex_tラッパー (RECURSIVE)
 * ═══════════════════════════════════════════════════════════════*/

class recursive_mutex {
    pthread_mutex_t mtx_;

public:
    recursive_mutex() {
        pthread_mutexattr_t attr;
        int result = pthread_mutexattr_init(&attr);
        if (result != 0) {
            detail::mutex_operation_failed(result,
                                           "recursive_mutex attributes");
        }
        result = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        if (result != 0) {
            pthread_mutexattr_destroy(&attr);
            detail::mutex_operation_failed(result,
                                           "recursive_mutex attributes");
        }
        result = pthread_mutex_init(&mtx_, &attr);
        pthread_mutexattr_destroy(&attr);
        if (result != 0) {
            detail::mutex_operation_failed(result, "recursive_mutex construction");
        }
    }

    ~recursive_mutex() {
        pthread_mutex_destroy(&mtx_);
    }

    recursive_mutex(const recursive_mutex&) = delete;
    recursive_mutex& operator=(const recursive_mutex&) = delete;

    using native_handle_type = pthread_mutex_t*;
    native_handle_type native_handle() noexcept { return &mtx_; }

    void lock() {
        const int result = pthread_mutex_lock(&mtx_);
        if (result != 0) {
            detail::mutex_operation_failed(result, "recursive_mutex lock");
        }
    }

    bool try_lock() noexcept {
        return pthread_mutex_trylock(&mtx_) == 0;
    }

    void unlock() noexcept {
        pthread_mutex_unlock(&mtx_);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * timed_mutex - deadline waits backed by the sleepable pthread mutex owner
 * ═══════════════════════════════════════════════════════════════*/

class timed_mutex {
    pthread_mutex_t mtx_;

public:
    timed_mutex() noexcept : mtx_(PTHREAD_MUTEX_INITIALIZER) {}

    timed_mutex(const timed_mutex&) = delete;
    timed_mutex& operator=(const timed_mutex&) = delete;

    void lock() {
        const int result = pthread_mutex_lock(&mtx_);
        if (result != 0)
            detail::mutex_operation_failed(result, "timed_mutex lock");
    }

    bool try_lock() noexcept {
        return pthread_mutex_trylock(&mtx_) == 0;
    }

    template<class Clock, class Duration>
    bool try_lock_until(
        const chrono::time_point<Clock, Duration>& timeout_time) {
        if (try_lock()) return true;
        struct timespec deadline = {};
        if (!detail::mutex_timepoint_realtime_deadline(timeout_time,
                                                        deadline))
            return false;
        const int result = pthread_mutex_timedlock(&mtx_, &deadline);
        if (result == 0) return true;
        if (result == ETIMEDOUT) return false;
        detail::mutex_operation_failed(result, "timed_mutex timed lock");
    }

    template<class Rep, class Period>
    bool try_lock_for(const chrono::duration<Rep, Period>& relative_time) {
        long long relative_nanoseconds = 0;
        if (!detail::mutex_duration_to_nanoseconds(
                relative_time, relative_nanoseconds))
            return try_lock();
        const long long now = chrono::steady_clock::now()
                                  .time_since_epoch().count();
        return try_lock_until(detail::mutex_steady_deadline_from_now(
            now, relative_nanoseconds));
    }

    void unlock() noexcept {
        (void)pthread_mutex_unlock(&mtx_);
    }

    using native_handle_type = pthread_mutex_t*;
    native_handle_type native_handle() noexcept { return &mtx_; }
};

class recursive_timed_mutex {
    pthread_mutex_t mtx_;

public:
    recursive_timed_mutex() {
        pthread_mutexattr_t attr;
        int result = pthread_mutexattr_init(&attr);
        if (result != 0)
            detail::mutex_operation_failed(result,
                                           "recursive_timed_mutex attributes");
        result = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        if (result != 0) {
            pthread_mutexattr_destroy(&attr);
            detail::mutex_operation_failed(result,
                                           "recursive_timed_mutex attributes");
        }
        result = pthread_mutex_init(&mtx_, &attr);
        pthread_mutexattr_destroy(&attr);
        if (result != 0)
            detail::mutex_operation_failed(result,
                                           "recursive_timed_mutex construction");
    }

    ~recursive_timed_mutex() { (void)pthread_mutex_destroy(&mtx_); }

    recursive_timed_mutex(const recursive_timed_mutex&) = delete;
    recursive_timed_mutex& operator=(const recursive_timed_mutex&) = delete;

    using native_handle_type = pthread_mutex_t*;
    native_handle_type native_handle() noexcept { return &mtx_; }

    void lock() {
        const int result = pthread_mutex_lock(&mtx_);
        if (result != 0)
            detail::mutex_operation_failed(result,
                                           "recursive_timed_mutex lock");
    }

    bool try_lock() noexcept {
        return pthread_mutex_trylock(&mtx_) == 0;
    }

    template<class Clock, class Duration>
    bool try_lock_until(
        const chrono::time_point<Clock, Duration>& timeout_time) {
        if (try_lock()) return true;
        struct timespec deadline = {};
        if (!detail::mutex_timepoint_realtime_deadline(timeout_time,
                                                        deadline))
            return false;
        const int result = pthread_mutex_timedlock(&mtx_, &deadline);
        if (result == 0) return true;
        if (result == ETIMEDOUT) return false;
        detail::mutex_operation_failed(result,
                                       "recursive_timed_mutex timed lock");
    }

    template<class Rep, class Period>
    bool try_lock_for(const chrono::duration<Rep, Period>& relative_time) {
        long long relative_nanoseconds = 0;
        if (!detail::mutex_duration_to_nanoseconds(
                relative_time, relative_nanoseconds))
            return try_lock();
        const long long now = chrono::steady_clock::now()
                                  .time_since_epoch().count();
        return try_lock_until(detail::mutex_steady_deadline_from_now(
            now, relative_nanoseconds));
    }

    void unlock() noexcept {
        (void)pthread_mutex_unlock(&mtx_);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * ロックタグ (must be before lock_guard)
 * ═══════════════════════════════════════════════════════════════*/

struct defer_lock_t { explicit defer_lock_t() = default; };
struct try_to_lock_t { explicit try_to_lock_t() = default; };
struct adopt_lock_t { explicit adopt_lock_t() = default; };

static constexpr defer_lock_t defer_lock{};
static constexpr try_to_lock_t try_to_lock{};
static constexpr adopt_lock_t adopt_lock{};

/* ═══════════════════════════════════════════════════════════════
 * lock_guard - RAIIロックガード
 * ═══════════════════════════════════════════════════════════════*/

template<class Mutex>
class lock_guard {
    Mutex& mutex_;

public:
    using mutex_type = Mutex;

    explicit lock_guard(Mutex& m) : mutex_(m) {
        mutex_.lock();
    }

    lock_guard(Mutex& m, adopt_lock_t) noexcept : mutex_(m) {
        /* 既にロック済み */
    }

    ~lock_guard() {
        mutex_.unlock();
    }

    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;
};

/* ═══════════════════════════════════════════════════════════════
 * unique_lock - 移動可能なロック
 * ═══════════════════════════════════════════════════════════════*/

template<class Mutex>
class unique_lock {
    Mutex* mutex_;
    bool owns_;

public:
    using mutex_type = Mutex;

    /* コンストラクタ */
    unique_lock() noexcept : mutex_(nullptr), owns_(false) {}

    explicit unique_lock(Mutex& m) : mutex_(&m), owns_(true) {
        mutex_->lock();
    }

    unique_lock(Mutex& m, defer_lock_t) noexcept
        : mutex_(&m), owns_(false) {}

    unique_lock(Mutex& m, try_to_lock_t)
        : mutex_(&m), owns_(mutex_->try_lock()) {}

    unique_lock(Mutex& m, adopt_lock_t) noexcept
        : mutex_(&m), owns_(true) {}

    /* ムーブ */
    unique_lock(unique_lock&& other) noexcept
        : mutex_(other.mutex_), owns_(other.owns_) {
        other.mutex_ = nullptr;
        other.owns_ = false;
    }

    unique_lock& operator=(unique_lock&& other) noexcept {
        if (owns_) mutex_->unlock();
        mutex_ = other.mutex_;
        owns_ = other.owns_;
        other.mutex_ = nullptr;
        other.owns_ = false;
        return *this;
    }

    /* コピー禁止 */
    unique_lock(const unique_lock&) = delete;
    unique_lock& operator=(const unique_lock&) = delete;

    /* デストラクタ */
    ~unique_lock() {
        if (owns_) mutex_->unlock();
    }

    /* ロック操作 */
    void lock() {
        if (!mutex_) {
            detail::mutex_operation_failed(EINVAL,
                                           "unique_lock has no mutex");
        }
        if (owns_) {
            detail::mutex_operation_failed(EDEADLK,
                                           "unique_lock already owns mutex");
        }
        mutex_->lock();
        owns_ = true;
    }

    bool try_lock() {
        if (!mutex_) {
            detail::mutex_operation_failed(EINVAL,
                                           "unique_lock has no mutex");
        }
        if (owns_) {
            detail::mutex_operation_failed(EDEADLK,
                                           "unique_lock already owns mutex");
        }
        owns_ = mutex_->try_lock();
        return owns_;
    }

    template<class Rep, class Period>
    bool try_lock_for(const chrono::duration<Rep, Period>& relative_time) {
        if (!mutex_) {
            detail::mutex_operation_failed(EINVAL,
                                           "unique_lock has no mutex");
        }
        if (owns_) {
            detail::mutex_operation_failed(EDEADLK,
                                           "unique_lock already owns mutex");
        }
        owns_ = mutex_->try_lock_for(relative_time);
        return owns_;
    }

    template<class Clock, class Duration>
    bool try_lock_until(
        const chrono::time_point<Clock, Duration>& timeout_time) {
        if (!mutex_) {
            detail::mutex_operation_failed(EINVAL,
                                           "unique_lock has no mutex");
        }
        if (owns_) {
            detail::mutex_operation_failed(EDEADLK,
                                           "unique_lock already owns mutex");
        }
        owns_ = mutex_->try_lock_until(timeout_time);
        return owns_;
    }

    void unlock() {
        if (!mutex_ || !owns_) {
            detail::mutex_operation_failed(EPERM,
                                           "unique_lock does not own mutex");
        }
        mutex_->unlock();
        owns_ = false;
    }

    /* 交換 */
    void swap(unique_lock& other) noexcept {
        std::swap(mutex_, other.mutex_);
        std::swap(owns_, other.owns_);
    }

    /* リリース */
    Mutex* release() noexcept {
        Mutex* m = mutex_;
        mutex_ = nullptr;
        owns_ = false;
        return m;
    }

    /* 状態 */
    bool owns_lock() const noexcept { return owns_; }
    explicit operator bool() const noexcept { return owns_; }
    Mutex* mutex() const noexcept { return mutex_; }
};

template<class Mutex>
void swap(unique_lock<Mutex>& lhs, unique_lock<Mutex>& rhs) noexcept {
    lhs.swap(rhs);
}

namespace detail {

/* std::try_lock's recursive form rolls back every mutex acquired before a
 * failed try_lock.  The index is relative to the remaining argument pack. */
template<class Mutex>
int try_lock_one(Mutex& mutex) {
    return mutex.try_lock() ? -1 : 0;
}

template<class Mutex>
int try_lock_many(Mutex& mutex) {
    return try_lock_one(mutex);
}

template<class First, class Second, class... Rest>
int try_lock_many(First& first, Second& second, Rest&... rest) {
    if (!first.try_lock()) return 0;

    int result;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    try {
        result = try_lock_many(second, rest...);
    } catch (...) {
        first.unlock();
        throw;
    }
#else
    result = try_lock_many(second, rest...);
#endif

    if (result >= 0) {
        first.unlock();
        return result + 1;
    }
    return -1;
}

inline void mutex_lock_backoff() noexcept {
    /* Yield the execution pipeline after a failed all-or-nothing attempt. */
#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    __asm__ volatile("pause" ::: "memory");
#elif defined(__GNUC__) || defined(__clang__)
    __asm__ volatile("" ::: "memory");
#else
    /* The lock/unlock retry remains correct without an architecture hint. */
#endif
}

template<class First, class Second, class... Rest>
class scoped_lock_acquire {
public:
    scoped_lock_acquire() noexcept = default;

    explicit scoped_lock_acquire(First& first, Second& second,
                                 Rest&... rest) {
        std::lock(first, second, rest...);
    }
};

} /* namespace detail */

/* Return the zero-based index of the first mutex which could not be acquired,
 * or -1 after acquiring every mutex.  The recursive helper releases its
 * prefix on both ordinary failure and exception unwinding. */
template<class First, class Second, class... Rest>
int try_lock(First& first, Second& second, Rest&... rest) {
    return detail::try_lock_many(first, second, rest...);
}

/* Acquire the first mutex blocking, then try the remainder as a transaction.
 * On failure all locks are released before retrying.  A caller never blocks
 * on one mutex while retaining another, which breaks circular wait between
 * invocations that pass the same mutexes in different orders. */
template<class First, class Second, class... Rest>
void lock(First& first, Second& second, Rest&... rest) {
    for (;;) {
        first.lock();
        int result;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try {
            result = detail::try_lock_many(second, rest...);
        } catch (...) {
            first.unlock();
            throw;
        }
#else
        result = detail::try_lock_many(second, rest...);
#endif
        if (result < 0) return;
        first.unlock();
        detail::mutex_lock_backoff();
    }
}

/* ═══════════════════════════════════════════════════════════════
 * scoped_lock - 複数ミューテックスのロック (C++17)
 * ═══════════════════════════════════════════════════════════════*/

template<class... Mutexes>
class scoped_lock;

/* 0個のミューテックス */
template<>
class scoped_lock<> {
public:
    explicit scoped_lock() = default;
    explicit scoped_lock(adopt_lock_t) noexcept {}
    ~scoped_lock() = default;

    scoped_lock(const scoped_lock&) = delete;
    scoped_lock& operator=(const scoped_lock&) = delete;
};

/* 1個のミューテックス */
template<class Mutex>
class scoped_lock<Mutex> {
    Mutex& mutex_;

public:
    using mutex_type = Mutex;

    explicit scoped_lock(Mutex& m) : mutex_(m) {
        mutex_.lock();
    }

    explicit scoped_lock(adopt_lock_t, Mutex& m) noexcept : mutex_(m) {}

    ~scoped_lock() {
        mutex_.unlock();
    }

    scoped_lock(const scoped_lock&) = delete;
    scoped_lock& operator=(const scoped_lock&) = delete;
};

/* 複数ミューテックス */
template<class Mutex1, class Mutex2, class... Mutexes>
class scoped_lock<Mutex1, Mutex2, Mutexes...> {
    detail::scoped_lock_acquire<Mutex1, Mutex2, Mutexes...> acquire_;
    Mutex1& mutex1_;
    scoped_lock<Mutex2, Mutexes...> rest_;

public:
    explicit scoped_lock(Mutex1& m1, Mutex2& m2, Mutexes&... rest)
        : acquire_(m1, m2, rest...), mutex1_(m1),
          rest_(adopt_lock, m2, rest...) {
    }

    explicit scoped_lock(adopt_lock_t, Mutex1& m1, Mutex2& m2,
                         Mutexes&... rest) noexcept
        : acquire_(), mutex1_(m1), rest_(adopt_lock, m2, rest...) {
    }

    ~scoped_lock() {
        mutex1_.unlock();
    }

    scoped_lock(const scoped_lock&) = delete;
    scoped_lock& operator=(const scoped_lock&) = delete;
};

/* ═══════════════════════════════════════════════════════════════
 * once_flag と call_once — futexベース
 * ═══════════════════════════════════════════════════════════════*/

class once_flag {
    volatile int state_;  /* 0: not called, 1: in progress, 2: done */

    template<class Callable, class... Args>
    friend void call_once(once_flag& flag, Callable&& f, Args&&... args);

public:
    constexpr once_flag() noexcept : state_(0) {}

    once_flag(const once_flag&) = delete;
    once_flag& operator=(const once_flag&) = delete;
};

namespace __detail {

inline void once_wake(volatile int* state) noexcept {
#if defined(RIN_MUTEX_HAS_LINUX_FUTEX)
    (void)syscall(SYS_futex, state, FUTEX_WAKE_PRIVATE, 0x7fffffff,
                  nullptr, nullptr, 0);
#else
    (void)state;
#endif
}

inline void once_wait(volatile int* state) noexcept {
#if defined(RIN_MUTEX_HAS_LINUX_FUTEX)
    (void)syscall(SYS_futex, state, FUTEX_WAIT_PRIVATE, 1, nullptr, nullptr, 0);
#else
    __asm__ volatile("pause" ::: "memory");
    (void)state;
#endif
}

} /* namespace __detail */

template<class Callable, class... Args>
void call_once(once_flag& flag, Callable&& f, Args&&... args) {
    for (;;) {
        int state = __atomic_load_n(&flag.state_, __ATOMIC_ACQUIRE);

        if (state == 2) return;
        if (state != 0 ||
            __sync_val_compare_and_swap(&flag.state_, 0, 1) != 0) {
            /*
             * A caller which observed an in-progress initializer must retry
             * the state transition after its futex wake.  In particular, an
             * initializer is allowed to throw and reset state_ to zero; just
             * waiting for state_ == 2 would otherwise spin forever and no
             * caller would run the required retry.
             */
            __detail::once_wait(&flag.state_);
            continue;
        }

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try {
            f(forward<Args>(args)...);
        } catch (...) {
            /* A throwing initializer must allow a later caller to retry. */
            __atomic_store_n(&flag.state_, 0, __ATOMIC_RELEASE);
            __detail::once_wake(&flag.state_);
            throw;
        }
#else
        f(forward<Args>(args)...);
#endif
        __atomic_store_n(&flag.state_, 2, __ATOMIC_RELEASE);
        __detail::once_wake(&flag.state_);
        return;
    }
}

} /* namespace std */

#undef RIN_MUTEX_HAS_LINUX_FUTEX

#endif /* RINCXX_MUTEX_H */
