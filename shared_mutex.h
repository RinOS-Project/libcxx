/*
 * RinOS C++ <shared_mutex> ✿
 * 共有ミューテックス (C++14/17)
 */

#ifndef RINCXX_SHARED_MUTEX_H
#define RINCXX_SHARED_MUTEX_H

#include "rincxx.h"

/* shared_timed_mutex starts in C++14; shared_mutex itself is C++17. */
#if defined(__cplusplus) && __cplusplus >= 201402L

#include "chrono.h"
#include "mutex.h"

namespace std {

namespace detail {

template<class Clock, class Duration>
inline int shared_timed_lock_wait(pthread_rwlock_t* rwlock, bool writer,
    const chrono::time_point<Clock, Duration>& timeout_time) {
    constexpr long long recheck_nanoseconds = 10000000LL;
    for (;;) {
        const auto now = Clock::now();
        if (!(now < timeout_time)) return ETIMEDOUT;
        long long remaining_nanoseconds = 0;
        if (!mutex_duration_to_nanoseconds(timeout_time - now,
                                           remaining_nanoseconds))
            return ETIMEDOUT;
        if (remaining_nanoseconds > recheck_nanoseconds)
            remaining_nanoseconds = recheck_nanoseconds;
        struct timespec deadline = {};
        if (!mutex_realtime_deadline(remaining_nanoseconds, deadline))
            return ETIMEDOUT;
        const int result = writer
            ? pthread_rwlock_timedwrlock(rwlock, &deadline)
            : pthread_rwlock_timedrdlock(rwlock, &deadline);
        if (result != ETIMEDOUT) return result;
    }
}

template<class Clock>
struct shared_timed_lock_strategy {
    template<class Duration>
    static int write(pthread_rwlock_t* rwlock,
                     const chrono::time_point<Clock, Duration>& timeout_time) {
        return shared_timed_lock_wait(rwlock, true, timeout_time);
    }

    template<class Duration>
    static int read(pthread_rwlock_t* rwlock,
                    const chrono::time_point<Clock, Duration>& timeout_time) {
        return shared_timed_lock_wait(rwlock, false, timeout_time);
    }
};

template<>
struct shared_timed_lock_strategy<chrono::steady_clock> {
    template<class Duration>
    static int write(pthread_rwlock_t* rwlock,
                     const chrono::time_point<chrono::steady_clock, Duration>& timeout_time) {
return shared_timed_lock_wait(rwlock, true, timeout_time);
    }

    template<class Duration>
    static int read(pthread_rwlock_t* rwlock,
                    const chrono::time_point<chrono::steady_clock, Duration>& timeout_time) {
return shared_timed_lock_wait(rwlock, false, timeout_time);
    }
};

template<>
struct shared_timed_lock_strategy<chrono::system_clock> {
    template<class Duration>
    static int write(pthread_rwlock_t* rwlock,
                     const chrono::time_point<chrono::system_clock, Duration>& timeout_time) {
        struct timespec deadline = {};
        if (!mutex_epoch_timespec_deadline(timeout_time.time_since_epoch(),
                                           deadline))
            return ETIMEDOUT;
        return pthread_rwlock_timedwrlock(rwlock, &deadline);
    }

    template<class Duration>
    static int read(pthread_rwlock_t* rwlock,
                    const chrono::time_point<chrono::system_clock, Duration>& timeout_time) {
        struct timespec deadline = {};
        if (!mutex_epoch_timespec_deadline(timeout_time.time_since_epoch(),
                                           deadline))
            return ETIMEDOUT;
        return pthread_rwlock_timedrdlock(rwlock, &deadline);
    }
};

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * shared_mutex (C++17)
 * ═══════════════════════════════════════════════════════════════*/

class shared_mutex {
    pthread_rwlock_t rwlock_;

public:
    using native_handle_type = pthread_rwlock_t*;

    shared_mutex() {
        const int result = pthread_rwlock_init(&rwlock_, nullptr);
        if (result != 0) {
            detail::mutex_operation_failed(result,
                                           "shared_mutex construction");
        }
    }

    ~shared_mutex() {
        pthread_rwlock_destroy(&rwlock_);
    }

    /* コピー・ムーブ禁止 */
    shared_mutex(const shared_mutex&) = delete;
    shared_mutex& operator=(const shared_mutex&) = delete;

    /* 排他ロック */
    void lock() {
        const int result = pthread_rwlock_wrlock(&rwlock_);
        if (result != 0) {
            detail::mutex_operation_failed(result, "shared_mutex lock");
        }
    }

    bool try_lock() noexcept {
        return pthread_rwlock_trywrlock(&rwlock_) == 0;
    }

    void unlock() noexcept {
        pthread_rwlock_unlock(&rwlock_);
    }

    /* 共有ロック */
    void lock_shared() {
        const int result = pthread_rwlock_rdlock(&rwlock_);
        if (result != 0) {
            detail::mutex_operation_failed(result,
                                           "shared_mutex lock_shared");
        }
    }

    bool try_lock_shared() noexcept {
        return pthread_rwlock_tryrdlock(&rwlock_) == 0;
    }

    void unlock_shared() noexcept {
        pthread_rwlock_unlock(&rwlock_);
    }

    native_handle_type native_handle() noexcept {
        return &rwlock_;
    }
};

/* ═══════════════════════════════════════════════════════════════
 * shared_timed_mutex (C++14)
 * ═══════════════════════════════════════════════════════════════*/

class shared_timed_mutex {
    shared_mutex mutex_;

public:
    using native_handle_type = shared_mutex::native_handle_type;

    shared_timed_mutex() = default;
    ~shared_timed_mutex() = default;

    shared_timed_mutex(const shared_timed_mutex&) = delete;
    shared_timed_mutex& operator=(const shared_timed_mutex&) = delete;

    /* 排他ロック */
    void lock() {
        mutex_.lock();
    }

    bool try_lock() noexcept {
        return mutex_.try_lock();
    }

    native_handle_type native_handle() noexcept {
        return mutex_.native_handle();
    }

    template<typename Rep, typename Period>
    bool try_lock_for(const chrono::duration<Rep, Period>& rel_time) {
        long long relative_nanoseconds = 0;
        if (!detail::mutex_duration_to_nanoseconds(
                rel_time, relative_nanoseconds))
            return try_lock();
        const long long now = chrono::steady_clock::now()
                                 .time_since_epoch().count();
        return try_lock_until(detail::mutex_steady_deadline_from_now(
            now, relative_nanoseconds));
    }

    template<typename Clock, typename Duration>
    bool try_lock_until(const chrono::time_point<Clock, Duration>& timeout_time) {
        if (try_lock()) return true;
        const int result = detail::shared_timed_lock_strategy<Clock>::write(
            mutex_.native_handle(), timeout_time);
        if (result == 0) return true;
        if (result == ETIMEDOUT) return false;
        detail::mutex_operation_failed(result,
                                       "shared_timed_mutex try_lock_until");
    }

    void unlock() noexcept {
        mutex_.unlock();
    }

    /* 共有ロック */
    void lock_shared() {
        mutex_.lock_shared();
    }

    bool try_lock_shared() noexcept {
        return mutex_.try_lock_shared();
    }

    template<typename Rep, typename Period>
    bool try_lock_shared_for(const chrono::duration<Rep, Period>& rel_time) {
        long long relative_nanoseconds = 0;
        if (!detail::mutex_duration_to_nanoseconds(
                rel_time, relative_nanoseconds))
            return try_lock_shared();
        const long long now = chrono::steady_clock::now()
                                 .time_since_epoch().count();
        return try_lock_shared_until(detail::mutex_steady_deadline_from_now(
            now, relative_nanoseconds));
    }

    template<typename Clock, typename Duration>
    bool try_lock_shared_until(const chrono::time_point<Clock, Duration>& timeout_time) {
        if (try_lock_shared()) return true;
        const int result = detail::shared_timed_lock_strategy<Clock>::read(
            mutex_.native_handle(), timeout_time);
        if (result == 0) return true;
        if (result == ETIMEDOUT) return false;
        detail::mutex_operation_failed(
            result, "shared_timed_mutex try_lock_shared_until");
    }

    void unlock_shared() noexcept {
        mutex_.unlock_shared();
    }
};

/* ═══════════════════════════════════════════════════════════════
 * shared_lock (C++14)
 * ═══════════════════════════════════════════════════════════════*/

template<typename Mutex>
class shared_lock {
    Mutex* mutex_;
    bool owns_;

    void validate_lock_attempt() {
        if (!mutex_) {
            detail::mutex_operation_failed(EINVAL,
                                           "shared_lock has no mutex");
        }
        if (owns_) {
            detail::mutex_operation_failed(EDEADLK,
                                           "shared_lock already owns mutex");
        }
    }

    void validate_unlock() {
        if (!mutex_ || !owns_) {
            detail::mutex_operation_failed(EPERM,
                                           "shared_lock does not own mutex");
        }
    }

public:
    using mutex_type = Mutex;

    /* コンストラクタ */
    shared_lock() noexcept : mutex_(nullptr), owns_(false) {}

    explicit shared_lock(Mutex& m) : mutex_(&m), owns_(true) {
        mutex_->lock_shared();
    }

    shared_lock(Mutex& m, defer_lock_t) noexcept : mutex_(&m), owns_(false) {}
    shared_lock(Mutex& m, try_to_lock_t) : mutex_(&m), owns_(mutex_->try_lock_shared()) {}
    shared_lock(Mutex& m, adopt_lock_t) noexcept : mutex_(&m), owns_(true) {}

    template<typename Rep, typename Period>
    shared_lock(Mutex& m, const chrono::duration<Rep, Period>& rel_time)
        : mutex_(&m), owns_(m.try_lock_shared_for(rel_time)) {}

    template<typename Clock, typename Duration>
    shared_lock(Mutex& m, const chrono::time_point<Clock, Duration>& timeout_time)
        : mutex_(&m), owns_(m.try_lock_shared_until(timeout_time)) {}

    /* デストラクタ */
    ~shared_lock() {
        if (owns_) {
            mutex_->unlock_shared();
        }
    }

    /* ムーブ */
    shared_lock(shared_lock&& other) noexcept
        : mutex_(other.mutex_), owns_(other.owns_) {
        other.mutex_ = nullptr;
        other.owns_ = false;
    }

    shared_lock& operator=(shared_lock&& other) noexcept {
        if (owns_) {
            mutex_->unlock_shared();
        }
        mutex_ = other.mutex_;
        owns_ = other.owns_;
        other.mutex_ = nullptr;
        other.owns_ = false;
        return *this;
    }

    /* コピー禁止 */
    shared_lock(const shared_lock&) = delete;
    shared_lock& operator=(const shared_lock&) = delete;

    /* ロック操作 */
    void lock() {
        validate_lock_attempt();
        mutex_->lock_shared();
        owns_ = true;
    }

    bool try_lock() {
        validate_lock_attempt();
        owns_ = mutex_->try_lock_shared();
        return owns_;
    }

    template<typename Rep, typename Period>
    bool try_lock_for(const chrono::duration<Rep, Period>& rel_time) {
        validate_lock_attempt();
        owns_ = mutex_->try_lock_shared_for(rel_time);
        return owns_;
    }

    template<typename Clock, typename Duration>
    bool try_lock_until(const chrono::time_point<Clock, Duration>& timeout_time) {
        validate_lock_attempt();
        owns_ = mutex_->try_lock_shared_until(timeout_time);
        return owns_;
    }

    void unlock() {
        validate_unlock();
        mutex_->unlock_shared();
        owns_ = false;
    }

    /* swap */
    void swap(shared_lock& other) noexcept {
        std::swap(mutex_, other.mutex_);
        std::swap(owns_, other.owns_);
    }

    /* release */
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

template<typename Mutex>
void swap(shared_lock<Mutex>& lhs, shared_lock<Mutex>& rhs) noexcept {
    lhs.swap(rhs);
}

} /* namespace std */

#endif /* defined(__cplusplus) && __cplusplus >= 201402L */
#endif /* RINCXX_SHARED_MUTEX_H */
