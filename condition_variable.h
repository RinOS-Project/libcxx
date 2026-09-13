/*
 * RinOS C++ <condition_variable> ✿
 * 条件変数
 */

#ifndef RINCXX_CONDITION_VARIABLE_H
#define RINCXX_CONDITION_VARIABLE_H

#include "rincxx.h"
#include "mutex.h"
#include "chrono.h"
#include "system_error.h"
#include "__pthread.h"
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <errno.h>
#else
#include "../libc/errno.h"
#endif
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <stdlib.h>
#else
#include "../libc/stdlib.h"
#endif

namespace std {

namespace detail {

/*
 * `notify_all_at_thread_exit` keeps work until the caller thread exits.  A
 * raw condition_variable pointer in that work item becomes a use-after-free
 * when a condition variable is closed before the thread exits.  Keep a
 * bounded, generation-tagged registry of the native condition objects.  Each
 * entry also owns a gate mutex: waiters hold it across caller-lock release
 * and entry into pthread_cond_wait, while notifiers and destruction hold it
 * across signal/broadcast.  Destruction first closes the entry and drains it,
 * so a stale exit node can only become a no-op and no native wait can miss a
 * close wakeup immediately before entering pthread_cond_wait.
 */
struct __condition_variable_registry_slot {
    pthread_cond_t* cond;
    pthread_mutex_t gate;
    unsigned long long generation;
    unsigned int active_waiters;
    volatile unsigned char closing;
    unsigned char gate_initialized;
};

struct __condition_variable_registry {
    volatile unsigned int lock;
    __condition_variable_registry_slot slots[128];
};

inline __condition_variable_registry& __condition_variable_registry_state() {
    static __condition_variable_registry state = {};
    return state;
}

inline void __condition_variable_registry_lock(
    __condition_variable_registry& state) noexcept {
    while (__sync_lock_test_and_set(&state.lock, 1u) != 0u) {
        /* The registry is only held across a slot lookup or one pthread
         * broadcast.  A bounded pause keeps exit/destruction contention from
         * consuming the whole core. */
        __asm__ volatile("pause" ::: "memory");
    }
}

inline void __condition_variable_registry_unlock(
    __condition_variable_registry& state) noexcept {
    __sync_lock_release(&state.lock);
}

inline bool __condition_variable_registry_register(
    pthread_cond_t* cond, unsigned long long& generation) noexcept {
    __condition_variable_registry& state =
        __condition_variable_registry_state();
    __condition_variable_registry_lock(state);
    for (unsigned int index = 0; index < 128u; ++index) {
        __condition_variable_registry_slot& slot = state.slots[index];
        if (slot.cond != nullptr) continue;
        if (pthread_mutex_init(&slot.gate, nullptr) != 0) continue;
        unsigned long long next = slot.generation + 1u;
        if (next == 0u) next = 1u;
        slot.generation = next;
        slot.cond = cond;
        slot.active_waiters = 0u;
        __atomic_store_n(&slot.closing, 0u, __ATOMIC_RELEASE);
        slot.gate_initialized = 1u;
        generation = next;
        __condition_variable_registry_unlock(state);
        return true;
    }
    __condition_variable_registry_unlock(state);
    return false;
}

inline bool __condition_variable_registry_begin_wait(
    pthread_cond_t* cond, unsigned long long generation,
    pthread_mutex_t*& gate, volatile unsigned char*& closing) noexcept {
    gate = nullptr;
    closing = nullptr;
    if (!cond || generation == 0u) return false;
    __condition_variable_registry& state =
        __condition_variable_registry_state();
    __condition_variable_registry_lock(state);
    bool accepted = false;
    for (unsigned int index = 0; index < 128u; ++index) {
        __condition_variable_registry_slot& slot = state.slots[index];
        if (slot.cond != cond || slot.generation != generation) continue;
        if (__atomic_load_n(&slot.closing, __ATOMIC_ACQUIRE) == 0u &&
            slot.active_waiters != 0xffffffffu) {
            ++slot.active_waiters;
            gate = &slot.gate;
            closing = &slot.closing;
            accepted = true;
        }
        break;
    }
    __condition_variable_registry_unlock(state);
    return accepted;
}

inline void __condition_variable_registry_end_wait(
    pthread_cond_t* cond, unsigned long long generation) noexcept {
    if (!cond || generation == 0u) return;
    __condition_variable_registry& state =
        __condition_variable_registry_state();
    __condition_variable_registry_lock(state);
    for (unsigned int index = 0; index < 128u; ++index) {
        __condition_variable_registry_slot& slot = state.slots[index];
        if (slot.cond == cond && slot.generation == generation) {
            if (slot.active_waiters != 0u) --slot.active_waiters;
            break;
        }
    }
    __condition_variable_registry_unlock(state);
}

inline unsigned int __condition_variable_registry_active_waiters(
    pthread_cond_t* cond, unsigned long long generation) noexcept {
    if (!cond || generation == 0u) return 0u;
    __condition_variable_registry& state =
        __condition_variable_registry_state();
    __condition_variable_registry_lock(state);
    unsigned int result = 0u;
    for (unsigned int index = 0; index < 128u; ++index) {
        const __condition_variable_registry_slot& slot = state.slots[index];
        if (slot.cond == cond && slot.generation == generation) {
            result = slot.active_waiters;
            break;
        }
    }
    __condition_variable_registry_unlock(state);
    return result;
}

/* Mark a condition variable closed, wake every native waiter, and wait until
 * every waiter has returned from pthread_cond_wait.  The slot stays present
 * and closing while the drain runs, so a late begin_wait cannot touch a
 * condition variable whose storage is being reclaimed.  The gate lock makes
 * the broadcast atomic with respect to a waiter that is releasing its caller
 * mutex and about to enter pthread_cond_wait. */
inline void __condition_variable_registry_close(
    pthread_cond_t* cond, unsigned long long generation) noexcept {
    if (!cond || generation == 0u) return;
    __condition_variable_registry& state =
        __condition_variable_registry_state();
    pthread_mutex_t* gate = nullptr;
    bool found = false;
    __condition_variable_registry_lock(state);
    for (unsigned int index = 0; index < 128u; ++index) {
        __condition_variable_registry_slot& slot = state.slots[index];
        if (slot.cond == cond && slot.generation == generation) {
            __atomic_store_n(&slot.closing, 1u, __ATOMIC_RELEASE);
            gate = &slot.gate;
            found = true;
            break;
        }
    }
    __condition_variable_registry_unlock(state);
    if (!found) return;

    /* Do not retain the registry spinlock while acquiring the waiter gate.
     * A waiter that observes `closing` releases this gate before calling
     * end_wait(), which needs the registry lock; holding both here would
     * deadlock exactly in that close handoff.  `closing` prevents new waiters
     * from entering, and the slot remains published until the drain below. */
    const int gate_result = pthread_mutex_lock(gate);
    if (gate_result != 0) terminate();
    (void)pthread_cond_broadcast(cond);
    (void)pthread_mutex_unlock(gate);

    for (;;) {
        __condition_variable_registry_lock(state);
        bool drained = false;
        for (unsigned int index = 0; index < 128u; ++index) {
            __condition_variable_registry_slot& slot = state.slots[index];
            if (slot.cond != cond || slot.generation != generation) continue;
            if (slot.active_waiters == 0u) {
                if (slot.gate_initialized) {
                    (void)pthread_mutex_destroy(&slot.gate);
                    slot.gate_initialized = 0u;
                }
                slot.cond = nullptr;
                __atomic_store_n(&slot.closing, 0u, __ATOMIC_RELEASE);
                drained = true;
            }
            break;
        }
        __condition_variable_registry_unlock(state);
        if (drained) return;
        /* The native waiter's final handoff can legitimately take more than
         * one spin (it must reacquire the caller mutex before end_wait()).
         * Yield through the same scheduler owner used by this_thread::yield
         * instead of burning a CPU while the close-drain is in progress. */
        rin_cxx_scheduler_yield();
    }
}

inline void __condition_variable_registry_unregister(
    pthread_cond_t* cond, unsigned long long generation) noexcept {
    if (!cond || generation == 0u) return;
    __condition_variable_registry& state =
        __condition_variable_registry_state();
    __condition_variable_registry_lock(state);
    for (unsigned int index = 0; index < 128u; ++index) {
        __condition_variable_registry_slot& slot = state.slots[index];
        if (slot.cond == cond && slot.generation == generation) {
            if (slot.gate_initialized) {
                (void)pthread_mutex_destroy(&slot.gate);
                slot.gate_initialized = 0u;
            }
            slot.cond = nullptr;
            break;
        }
    }
    __condition_variable_registry_unlock(state);
}

inline bool __condition_variable_registry_notify(
    pthread_cond_t* cond, unsigned long long generation) noexcept {
    if (!cond || generation == 0u) return false;
    __condition_variable_registry& state =
        __condition_variable_registry_state();
    __condition_variable_registry_lock(state);
    bool found = false;
    for (unsigned int index = 0; index < 128u; ++index) {
        __condition_variable_registry_slot& slot = state.slots[index];
        if (slot.cond == cond && slot.generation == generation) {
            (void)pthread_mutex_lock(&slot.gate);
            (void)pthread_cond_broadcast(slot.cond);
            (void)pthread_mutex_unlock(&slot.gate);
            found = true;
            break;
        }
    }
    __condition_variable_registry_unlock(state);
    return found;
}

/* Serialize one-shot notifications with close/drain as well.  The public
 * notify methods used to call pthread_cond_signal directly, which left a
 * small window where a concurrent destructor could destroy the native condvar
 * while a notifier was still entering it.  Keeping the signal under the same
 * generation registry lock makes the native lifetime boundary identical for
 * notify_one and notify_all. */
inline bool __condition_variable_registry_signal(
    pthread_cond_t* cond, unsigned long long generation) noexcept {
    if (!cond || generation == 0u) return false;
    __condition_variable_registry& state =
        __condition_variable_registry_state();
    bool found = false;
    __condition_variable_registry_lock(state);
    for (unsigned int index = 0; index < 128u; ++index) {
        __condition_variable_registry_slot& slot = state.slots[index];
        if (slot.cond == cond && slot.generation == generation) {
            (void)pthread_mutex_lock(&slot.gate);
            (void)pthread_cond_signal(slot.cond);
            (void)pthread_mutex_unlock(&slot.gate);
            found = true;
            break;
        }
    }
    __condition_variable_registry_unlock(state);
    return found;
}

[[noreturn]] inline void condition_variable_operation_failed(
    int error, const char* operation) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw system_error(error, system_category(), operation);
#else
    (void)error;
    (void)operation;
    terminate();
#endif
}

/* condition_variable_any accepts arbitrary Lockable objects, so a generic
 * lock does not necessarily expose ownership state.  When the lock does
 * provide the standard unique_lock/shared_lock observers, validate that
 * state before any clock conversion or internal mutex mutation.  This keeps
 * an unowned observable lock from reaching unlock() as an undefined backend
 * operation while preserving compatibility with minimal custom Lockables. */
template<typename Lock, typename = void>
struct condition_variable_any_lock_state {
    static bool valid(const Lock&) noexcept { return true; }
};

template<typename Lock>
struct condition_variable_any_lock_state<
    Lock, void_t<decltype(declval<const Lock&>().owns_lock()),
                 decltype(declval<const Lock&>().mutex())>> {
    static bool valid(const Lock& lock) noexcept {
        return lock.mutex() != nullptr && lock.owns_lock();
    }
};

template<typename Lock>
inline void condition_variable_any_validate_lock(
    const Lock& lock, const char* operation) {
    if (!condition_variable_any_lock_state<Lock>::valid(lock)) {
        condition_variable_operation_failed(EINVAL, operation);
    }
}

/* Convert a positive relative nanosecond interval into the realtime
 * timespec required by pthread_cond_timedwait.  time_t is `long` in the
 * freestanding ABI, so adding a large duration directly can wrap on LLP64
 * targets.  Saturate an unrepresentable future deadline instead of exposing
 * a past deadline (which would turn a wait into an accidental busy timeout).
 */
inline bool condition_variable_make_realtime_deadline(
    const struct timespec& now, long long relative_nanoseconds,
    struct timespec& deadline) noexcept {
    constexpr long long billion = 1000000000LL;
    if (relative_nanoseconds <= 0 || now.tv_nsec < 0 ||
        now.tv_nsec >= billion) {
        return false;
    }

    const long long delta_seconds = relative_nanoseconds / billion;
    const long long delta_nanoseconds = relative_nanoseconds % billion;
    using timespec_seconds_type = decltype(now.tv_sec);
    const long long maximum_seconds = static_cast<long long>(
        numeric_limits<timespec_seconds_type>::max());
    const long long now_seconds = static_cast<long long>(now.tv_sec);

    if (delta_seconds > maximum_seconds ||
        now_seconds > maximum_seconds - delta_seconds) {
        deadline.tv_sec = numeric_limits<timespec_seconds_type>::max();
        deadline.tv_nsec = static_cast<long>(billion - 1);
        return true;
    }

    timespec_seconds_type seconds =
        static_cast<timespec_seconds_type>(now_seconds + delta_seconds);
    long long nanoseconds =
        static_cast<long long>(now.tv_nsec) + delta_nanoseconds;
    if (nanoseconds >= billion) {
        if (seconds == numeric_limits<timespec_seconds_type>::max()) {
            deadline.tv_sec = seconds;
            deadline.tv_nsec = static_cast<long>(billion - 1);
            return true;
        }
        ++seconds;
        nanoseconds -= billion;
    }

    deadline.tv_sec = seconds;
    deadline.tv_nsec = static_cast<long>(nanoseconds);
    return true;
}

/* Convert a relative wait duration without first forming
 * `steady_clock::now() + duration`.  The latter can wrap the signed
 * nanosecond representation before the timed-wait adapter gets a chance to
 * saturate it.  Integral durations use chrono's checked conversion; floating
 * durations are bounded through long double before narrowing. */
template<typename Rep, typename Period>
inline bool condition_variable_duration_to_nanoseconds(
    const chrono::duration<Rep, Period>& value,
    long long& nanoseconds) noexcept {
    const Rep count = value.count();
    if (!(count > Rep(0))) {
        nanoseconds = 0;
        return false;
    }

    if (is_integral<Rep>::value) {
        nanoseconds = chrono::duration_cast<chrono::nanoseconds>(value).count();
        return nanoseconds > 0;
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
    if (scaled >= maximum) {
        nanoseconds = numeric_limits<long long>::max();
        return true;
    }
    nanoseconds = static_cast<long long>(scaled);
    return nanoseconds > 0;
}

/* Subtract two already-saturated nanosecond epochs without forming an
 * overflowing signed expression.  `time_point - time_point` normally does
 * that subtraction in the common duration representation before the wait
 * adapter can apply its bound, so a far-future deadline paired with a
 * negative (but valid) clock sample could wrap into an apparent timeout. */
inline bool condition_variable_epoch_delta_nanoseconds(
    long long deadline, long long now, long long& relative) noexcept {
    if (deadline <= now) {
        relative = 0;
        return false;
    }
    const long long maximum = numeric_limits<long long>::max();
    if (now < 0 && deadline > maximum + now) {
        relative = maximum;
        return true;
    }
    relative = deadline - now;
    return relative > 0;
}

template<typename Clock, typename TimeoutDuration>
inline bool condition_variable_timepoint_delta_nanoseconds(
    const chrono::time_point<Clock, TimeoutDuration>& deadline,
    const typename Clock::time_point& now,
    long long& relative) noexcept {
    const long long deadline_ns = chrono::duration_cast<chrono::nanoseconds>(
        deadline.time_since_epoch()).count();
    const long long now_ns = chrono::duration_cast<chrono::nanoseconds>(
        now.time_since_epoch()).count();
    return condition_variable_epoch_delta_nanoseconds(
        deadline_ns, now_ns, relative);
}

inline chrono::steady_clock::time_point
condition_variable_steady_deadline_from_now(
    long long now, long long relative_nanoseconds) noexcept {
    const long long maximum = numeric_limits<long long>::max();
    if (relative_nanoseconds <= 0) {
        return chrono::steady_clock::time_point(
            chrono::nanoseconds(now));
    }
    /* `maximum - now` overflows when an injected/target monotonic clock
     * reports a negative sample.  Compare against the positive interval
     * instead; with relative_nanoseconds > 0 this subtraction is always
     * representable and the final addition is therefore bounded. */
    if (relative_nanoseconds > 0 &&
        now > maximum - relative_nanoseconds) {
        return chrono::steady_clock::time_point(
            chrono::nanoseconds(maximum));
    }
    return chrono::steady_clock::time_point(
        chrono::nanoseconds(now + relative_nanoseconds));
}

inline chrono::steady_clock::time_point
condition_variable_steady_deadline(long long relative_nanoseconds) noexcept {
    const long long now = chrono::steady_clock::now().time_since_epoch().count();
    return condition_variable_steady_deadline_from_now(
        now, relative_nanoseconds);
}

/* A condition_variable_any wait must reacquire its caller lock before it
 * returns or propagates a native-wait exception.  If a user Lock throws from
 * that relock, returning would violate the wait contract with an unowned
 * lock, so terminate at the defined synchronization failure boundary. */
template<typename Lock>
inline void condition_variable_any_relock_or_terminate(Lock& lock) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
        lock.lock();
    } catch (...) {
        terminate();
    }
#else
    lock.lock();
#endif
}

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * cv_status
 * ═══════════════════════════════════════════════════════════════*/

enum class cv_status {
    no_timeout,
    timeout
};

/* ═══════════════════════════════════════════════════════════════
 * condition_variable
 * ═══════════════════════════════════════════════════════════════*/

class condition_variable {
    pthread_cond_t cond_;
    unsigned long long registry_generation_;

    static void validate_wait_lock(unique_lock<mutex>& lock,
                                   const char* operation) {
        if (!lock.mutex() || !lock.owns_lock()) {
            detail::condition_variable_operation_failed(EINVAL, operation);
        }
    }

public:
    using native_handle_type = pthread_cond_t*;

    condition_variable() : registry_generation_(0u) {
        const int result = pthread_cond_init(&cond_, nullptr);
        if (result != 0) {
            detail::condition_variable_operation_failed(
                result, "condition_variable construction");
        }
        if (!detail::__condition_variable_registry_register(
                &cond_, registry_generation_)) {
            (void)pthread_cond_destroy(&cond_);
            detail::condition_variable_operation_failed(
                ENOMEM, "condition_variable registry capacity");
        }
    }

    ~condition_variable() {
        detail::__condition_variable_registry_close(
            &cond_, registry_generation_);
        (void)pthread_cond_destroy(&cond_);
    }

    /* コピー・ムーブ禁止 */
    condition_variable(const condition_variable&) = delete;
    condition_variable& operator=(const condition_variable&) = delete;

    /* notify_one */
    void notify_one() noexcept {
        (void)detail::__condition_variable_registry_signal(
            &cond_, registry_generation_);
    }

    /* notify_all */
    void notify_all() noexcept {
        (void)detail::__condition_variable_registry_notify(
            &cond_, registry_generation_);
    }

    /* wait — pthread_cond_wait経由 (futexブロッキング) */
    void wait(unique_lock<mutex>& lock) {
        validate_wait_lock(lock, "condition_variable wait lock state");
        pthread_mutex_t* gate = nullptr;
        volatile unsigned char* closing = nullptr;
        if (!detail::__condition_variable_registry_begin_wait(
                &cond_, registry_generation_, gate, closing)) {
            detail::condition_variable_operation_failed(
                EBUSY, "condition_variable wait closing");
        }
        pthread_mutex_t* pmtx = lock.mutex()->native_handle();
        const int gate_result = pthread_mutex_lock(gate);
        if (gate_result != 0) {
            detail::__condition_variable_registry_end_wait(
                &cond_, registry_generation_);
            detail::condition_variable_operation_failed(
                gate_result, "condition_variable wait gate");
        }
        if (__atomic_load_n(closing, __ATOMIC_ACQUIRE) != 0u) {
            (void)pthread_mutex_unlock(gate);
            detail::__condition_variable_registry_end_wait(
                &cond_, registry_generation_);
            /* The waiter claimed a slot before close, but had not yet
             * released the caller mutex.  Treat the close as a wakeup and
             * leave the caller lock untouched; the registry drain keeps the
             * condition storage alive until this handoff is complete. */
            return;
        }
        const int unlock_result = pthread_mutex_unlock(pmtx);
        if (unlock_result != 0) {
            (void)pthread_mutex_unlock(gate);
            detail::__condition_variable_registry_end_wait(
                &cond_, registry_generation_);
            detail::condition_variable_operation_failed(
                unlock_result, "condition_variable wait lock release");
        }
        const int result = pthread_cond_wait(&cond_, gate);
        const int gate_unlock_result = pthread_mutex_unlock(gate);
        const int relock_result = pthread_mutex_lock(pmtx);
        detail::__condition_variable_registry_end_wait(
            &cond_, registry_generation_);
        if (gate_unlock_result != 0) {
            detail::condition_variable_operation_failed(
                gate_unlock_result, "condition_variable wait gate release");
        }
        if (relock_result != 0) {
            detail::condition_variable_operation_failed(
                relock_result, "condition_variable wait lock reacquire");
        }
        if (result != 0) {
            detail::condition_variable_operation_failed(result,
                                                        "condition_variable wait");
        }
    }

    template<typename Predicate>
    void wait(unique_lock<mutex>& lock, Predicate pred) {
        validate_wait_lock(lock, "condition_variable wait lock state");
        while (!pred()) {
            wait(lock);
        }
    }

    /* wait_for */
    template<typename Rep, typename Period>
    cv_status wait_for(unique_lock<mutex>& lock,
                       const chrono::duration<Rep, Period>& rel_time) {
        validate_wait_lock(lock, "condition_variable wait_for lock state");
        long long relative_nanoseconds = 0;
        if (!detail::condition_variable_duration_to_nanoseconds(
                rel_time, relative_nanoseconds)) {
            return cv_status::timeout;
        }
        return wait_until(lock,
            detail::condition_variable_steady_deadline(relative_nanoseconds));
    }

    template<typename Rep, typename Period, typename Predicate>
    bool wait_for(unique_lock<mutex>& lock,
                  const chrono::duration<Rep, Period>& rel_time,
                  Predicate pred) {
        validate_wait_lock(lock, "condition_variable wait_for lock state");
        long long relative_nanoseconds = 0;
        if (!detail::condition_variable_duration_to_nanoseconds(
                rel_time, relative_nanoseconds)) {
            return pred();
        }
        return wait_until(lock,
            detail::condition_variable_steady_deadline(relative_nanoseconds),
            std::move(pred));
    }

    /* wait_until — pthread_cond_timedwait経由 (futexブロッキング) */
    template<typename Clock, typename Duration>
    cv_status wait_until(unique_lock<mutex>& lock,
                         const chrono::time_point<Clock, Duration>& timeout_time) {
        validate_wait_lock(lock, "condition_variable timed wait lock state");
        auto now = Clock::now();
        long long rel_ns = 0;
        if (!detail::condition_variable_timepoint_delta_nanoseconds(
                timeout_time, now, rel_ns)) {
            return cv_status::timeout;
        }

        struct timespec now_ts;
        if (clock_gettime(CLOCK_REALTIME, &now_ts) != 0) {
            const int clock_error = errno != 0 ? errno : EIO;
            detail::condition_variable_operation_failed(
                clock_error, "condition_variable realtime clock");
        }

        struct timespec ts;
        if (!detail::condition_variable_make_realtime_deadline(
                now_ts, rel_ns, ts)) {
            return cv_status::timeout;
        }

        pthread_mutex_t* gate = nullptr;
        volatile unsigned char* closing = nullptr;
        if (!detail::__condition_variable_registry_begin_wait(
                &cond_, registry_generation_, gate, closing)) {
            detail::condition_variable_operation_failed(
                EBUSY, "condition_variable timed wait closing");
        }
        pthread_mutex_t* pmtx = lock.mutex()->native_handle();
        const int gate_result = pthread_mutex_lock(gate);
        if (gate_result != 0) {
            detail::__condition_variable_registry_end_wait(
                &cond_, registry_generation_);
            detail::condition_variable_operation_failed(
                gate_result, "condition_variable timed wait gate");
        }
        if (__atomic_load_n(closing, __ATOMIC_ACQUIRE) != 0u) {
            (void)pthread_mutex_unlock(gate);
            detail::__condition_variable_registry_end_wait(
                &cond_, registry_generation_);
            return cv_status::no_timeout;
        }
        const int unlock_result = pthread_mutex_unlock(pmtx);
        if (unlock_result != 0) {
            (void)pthread_mutex_unlock(gate);
            detail::__condition_variable_registry_end_wait(
                &cond_, registry_generation_);
            detail::condition_variable_operation_failed(
                unlock_result, "condition_variable timed wait lock release");
        }
        int ret = pthread_cond_timedwait(&cond_, gate, &ts);
        const int gate_unlock_result = pthread_mutex_unlock(gate);
        const int relock_result = pthread_mutex_lock(pmtx);
        detail::__condition_variable_registry_end_wait(
            &cond_, registry_generation_);
        if (gate_unlock_result != 0) {
            detail::condition_variable_operation_failed(
                gate_unlock_result,
                "condition_variable timed wait gate release");
        }
        if (relock_result != 0) {
            detail::condition_variable_operation_failed(
                relock_result,
                "condition_variable timed wait lock reacquire");
        }
        if (ret == ETIMEDOUT) return cv_status::timeout;
        if (ret != 0) {
            detail::condition_variable_operation_failed(
                ret, "condition_variable timed wait");
        }
        return cv_status::no_timeout;
    }

    template<typename Clock, typename Duration, typename Predicate>
    bool wait_until(unique_lock<mutex>& lock,
                    const chrono::time_point<Clock, Duration>& timeout_time,
                    Predicate pred) {
        validate_wait_lock(lock,
                           "condition_variable timed wait lock state");
        while (!pred()) {
            if (wait_until(lock, timeout_time) == cv_status::timeout) {
                return pred();
            }
        }
        return true;
    }

    /* native_handle */
    native_handle_type native_handle() noexcept {
        return &cond_;
    }

    /* Internal lifetime token used by notify_all_at_thread_exit. */
    unsigned long long __rin_registry_generation() const noexcept {
        return registry_generation_;
    }
};

/* ═══════════════════════════════════════════════════════════════
 * condition_variable_any
 * 任意のロッカブル型に対応
 * ═══════════════════════════════════════════════════════════════*/

class condition_variable_any {
    mutex internal_mutex_;
    condition_variable cv_;
    /* The underlying condition_variable drains native waiters, but an
     * any-wrapper waiter still has to reacquire the caller lock and release
     * internal_mutex_ after that drain.  Keep the wrapper alive through that
     * final handoff instead of destroying its mutex immediately after cv_. */
    volatile unsigned int wait_state_;
    static constexpr unsigned int closing_bit_ = UINT32_C(0x80000000);
    static constexpr unsigned int waiter_mask_ = ~closing_bit_;

    bool begin_wait() noexcept {
        unsigned int state =
            __atomic_load_n(&wait_state_, __ATOMIC_ACQUIRE);
        for (;;) {
            if ((state & closing_bit_) != 0u ||
                (state & waiter_mask_) == waiter_mask_)
                return false;
            unsigned int desired = state + 1u;
            if (__atomic_compare_exchange_n(&wait_state_, &state, desired,
                                             false, __ATOMIC_ACQUIRE,
                                             __ATOMIC_ACQUIRE))
                return true;
        }
    }

    void end_wait() noexcept {
        (void)__atomic_fetch_sub(&wait_state_, 1u, __ATOMIC_RELEASE);
    }

    class wait_guard {
        condition_variable_any* owner_;
        bool active_;

    public:
        explicit wait_guard(condition_variable_any& owner)
            : owner_(&owner), active_(owner.begin_wait()) {
            if (!active_) {
                detail::condition_variable_operation_failed(
                    EBUSY, "condition_variable_any wait closing");
            }
        }

        ~wait_guard() {
            if (active_) owner_->end_wait();
        }

        wait_guard(const wait_guard&) = delete;
        wait_guard& operator=(const wait_guard&) = delete;
    };

public:
    condition_variable_any() : internal_mutex_(), cv_(), wait_state_(0u) {}

    ~condition_variable_any() {
        /* Exclude a waiter that has counted itself but has not entered the
         * native wait yet.  We publish closing while holding the wrapper
         * mutex, wake native waiters, then let pre-lock waiters observe the
         * bit and unwind without missing the wakeup. */
        internal_mutex_.lock();
        (void)__atomic_fetch_or(&wait_state_, closing_bit_, __ATOMIC_ACQ_REL);
        cv_.notify_all();
        internal_mutex_.unlock();
        while ((__atomic_load_n(&wait_state_, __ATOMIC_ACQUIRE) &
                waiter_mask_) != 0u) {
            /* A wrapper waiter may still be completing caller-lock
             * reacquisition after the native broadcast.  Let the hosted or
             * Rin scheduler run that handoff rather than busy-spinning. */
            detail::rin_cxx_scheduler_yield();
        }
    }

    condition_variable_any(const condition_variable_any&) = delete;
    condition_variable_any& operator=(const condition_variable_any&) = delete;

    void notify_one() noexcept {
        lock_guard<mutex> lock(internal_mutex_);
        cv_.notify_one();
    }

    void notify_all() noexcept {
        lock_guard<mutex> lock(internal_mutex_);
        cv_.notify_all();
    }

    template<typename Lock>
    void wait(Lock& lock) {
        detail::condition_variable_any_validate_lock(
            lock, "condition_variable_any wait lock state");
        wait_guard guard(*this);
        unique_lock<mutex> internal_lock(internal_mutex_);
        if ((__atomic_load_n(&wait_state_, __ATOMIC_ACQUIRE) &
             closing_bit_) != 0u) {
            return;
        }
        lock.unlock();

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            cv_.wait(internal_lock);
        } catch (...) {
            internal_lock.unlock();
            detail::condition_variable_any_relock_or_terminate(lock);
            throw;
        }
#else
        cv_.wait(internal_lock);
#endif

        internal_lock.unlock();
        detail::condition_variable_any_relock_or_terminate(lock);
    }

    template<typename Lock, typename Predicate>
    void wait(Lock& lock, Predicate pred) {
        detail::condition_variable_any_validate_lock(
            lock, "condition_variable_any wait lock state");
        while (!pred()) {
            wait(lock);
        }
    }

    template<typename Lock, typename Rep, typename Period>
    cv_status wait_for(Lock& lock,
                       const chrono::duration<Rep, Period>& rel_time) {
        detail::condition_variable_any_validate_lock(
            lock, "condition_variable_any wait_for lock state");
        long long relative_nanoseconds = 0;
        if (!detail::condition_variable_duration_to_nanoseconds(
                rel_time, relative_nanoseconds)) {
            return cv_status::timeout;
        }
        return wait_until(lock,
            detail::condition_variable_steady_deadline(relative_nanoseconds));
    }

    template<typename Lock, typename Rep, typename Period, typename Predicate>
    bool wait_for(Lock& lock,
                  const chrono::duration<Rep, Period>& rel_time,
                  Predicate pred) {
        detail::condition_variable_any_validate_lock(
            lock, "condition_variable_any wait_for lock state");
        long long relative_nanoseconds = 0;
        if (!detail::condition_variable_duration_to_nanoseconds(
                rel_time, relative_nanoseconds)) {
            return pred();
        }
        return wait_until(lock,
            detail::condition_variable_steady_deadline(relative_nanoseconds),
            std::move(pred));
    }

    template<typename Lock, typename Clock, typename Duration>
    cv_status wait_until(Lock& lock,
                         const chrono::time_point<Clock, Duration>& timeout_time) {
        detail::condition_variable_any_validate_lock(
            lock, "condition_variable_any wait_until lock state");
        wait_guard guard(*this);
        unique_lock<mutex> internal_lock(internal_mutex_);
        if ((__atomic_load_n(&wait_state_, __ATOMIC_ACQUIRE) &
             closing_bit_) != 0u) {
            return cv_status::no_timeout;
        }
        lock.unlock();

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        cv_status result;
        try {
            result = cv_.wait_until(internal_lock, timeout_time);
        } catch (...) {
            internal_lock.unlock();
            detail::condition_variable_any_relock_or_terminate(lock);
            throw;
        }
#else
        cv_status result = cv_.wait_until(internal_lock, timeout_time);
#endif

        internal_lock.unlock();
        detail::condition_variable_any_relock_or_terminate(lock);
        return result;
    }

    template<typename Lock, typename Clock, typename Duration, typename Predicate>
    bool wait_until(Lock& lock,
                    const chrono::time_point<Clock, Duration>& timeout_time,
                    Predicate pred) {
        detail::condition_variable_any_validate_lock(
            lock, "condition_variable_any wait_until lock state");
        while (!pred()) {
            if (wait_until(lock, timeout_time) == cv_status::timeout) {
                return pred();
            }
        }
        return true;
    }

    /* Internal lifetime observation for close-race contract tests. */
    unsigned int __rin_active_waiters() const noexcept {
        return __atomic_load_n(&wait_state_, __ATOMIC_ACQUIRE) & waiter_mask_;
    }
};

/* ═══════════════════════════════════════════════════════════════
 * notify_all_at_thread_exit
 * ═══════════════════════════════════════════════════════════════*/

namespace __detail {

struct __notify_at_thread_exit_node {
    pthread_cond_t* cond;
    unsigned long long generation;
    pthread_mutex_t* mtx;
    __notify_at_thread_exit_node* next;
};

/* Keep the node allocation as a small owner seam.  Production builds use the
 * process C allocator; hosted contract tests may inject a failing allocator
 * without replacing the CRT's global malloc symbol. */
inline void* __notify_at_thread_exit_node_allocate(size_t size) noexcept {
#if defined(RIN_CXX_CONDITION_VARIABLE_NODE_ALLOC)
    return RIN_CXX_CONDITION_VARIABLE_NODE_ALLOC(size);
#else
    return malloc(size);
#endif
}

/* Keep allocation and release on the same owner seam.  A product may use a
 * private arena (or a diagnostic allocator) for the TLS node; routing the
 * release through the matching hook avoids passing that pointer to the CRT
 * allocator after thread exit. */
inline void __notify_at_thread_exit_node_deallocate(void* pointer) noexcept {
    if (!pointer) return;
#if defined(RIN_CXX_CONDITION_VARIABLE_NODE_FREE)
    RIN_CXX_CONDITION_VARIABLE_NODE_FREE(pointer);
#else
    free(pointer);
#endif
}

/* Function-local statics in an external-linkage inline function have one
 * program-wide instance.  Unlike a C++17 inline variable they are also valid
 * in C++11, and the thread-local list retains one head per calling thread. */
inline pthread_key_t& __notify_at_exit_key() {
    static pthread_key_t key;
    return key;
}

inline once_flag& __notify_at_exit_once() {
    static once_flag once;
    return once;
}

inline int& __notify_at_exit_key_error() {
    static int error = 0;
    return error;
}

inline __notify_at_thread_exit_node*& __notify_at_exit_head() {
    static thread_local __notify_at_thread_exit_node* head = nullptr;
    return head;
}

inline void __notify_at_thread_exit_tls_dtor(void*) {
    /* POSIX may invoke a key destructor repeatedly while its value remains
     * non-NULL.  Clear the key before walking the private list so the
     * deallocated head is never presented to a second destructor pass. */
    (void)pthread_setspecific(__notify_at_exit_key(), nullptr);
    __notify_at_thread_exit_node* n = __notify_at_exit_head();
    __notify_at_exit_head() = nullptr;

    while (n) {
        __notify_at_thread_exit_node* next = n->next;
        if (n->mtx) {
            pthread_mutex_unlock(n->mtx);
        }
        (void)std::detail::__condition_variable_registry_notify(
            n->cond, n->generation);
        __notify_at_thread_exit_node_deallocate(n);
        n = next;
    }
}

inline void __notify_at_thread_exit_key_init() {
    __notify_at_exit_key_error() = pthread_key_create(
        &__notify_at_exit_key(), __notify_at_thread_exit_tls_dtor);
}

} /* namespace __detail */

inline void notify_all_at_thread_exit(condition_variable& cond, unique_lock<mutex> lk) {
    if (!lk.owns_lock() || lk.mutex() == nullptr) {
        detail::condition_variable_operation_failed(
            EINVAL, "notify_all_at_thread_exit lock state");
    }

    call_once(__detail::__notify_at_exit_once(), [] {
        __detail::__notify_at_thread_exit_key_init();
    });

    if (__detail::__notify_at_exit_key_error() != 0) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        detail::condition_variable_operation_failed(
            __detail::__notify_at_exit_key_error(),
            "notify_all_at_thread_exit key initialization");
#else
        /* A TLS destructor cannot be registered in a no-exception product;
         * preserve the existing liveness fallback at that boundary. */
        lk.unlock();
        cond.notify_all();
        return;
#endif
    }

    auto* node = static_cast<__detail::__notify_at_thread_exit_node*>(
        __detail::__notify_at_thread_exit_node_allocate(
            sizeof(__detail::__notify_at_thread_exit_node)));

    if (!node) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        throw bad_alloc();
#else
        /* Fallback: preserve liveness even under OOM. */
        lk.unlock();
        cond.notify_all();
        return;
#endif
    }

    node->cond = cond.native_handle();
    node->generation = cond.__rin_registry_generation();
    node->mtx = lk.mutex()->native_handle();
    node->next = __detail::__notify_at_exit_head();
    __detail::__notify_at_exit_head() = node;

    const int specific_result = pthread_setspecific(
        __detail::__notify_at_exit_key(), (void*)node);
    if (specific_result != 0) {
        __detail::__notify_at_exit_head() = node->next;
        __detail::__notify_at_thread_exit_node_deallocate(node);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        detail::condition_variable_operation_failed(
            specific_result, "notify_all_at_thread_exit registration");
#else
        lk.unlock();
        cond.notify_all();
        return;
#endif
    }

    /* Transfer ownership: keep mutex locked until thread exit destructor. */
    (void)lk.release();
}

} /* namespace std */

#endif /* RINCXX_CONDITION_VARIABLE_H */
