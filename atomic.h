/*
 * RinOS C++ <atomic>
 * Atomic operations - complete x86 implementation without compiler builtins
 *
 * Supports:
 * - Lock-free atomics for 1/2/4/8 byte trivially-copyable types
 * - All atomic operations use x86 inline assembly
 */

#ifndef RINCXX_ATOMIC_H
#define RINCXX_ATOMIC_H

#include "rincxx.h"
#include "cstdint.h"
#include "type_traits.h"
#include "cstring.h"
#include "memory.h"
#include "pointer_order.h"

/* The hosted Linux regression build opts into the native futex wait path.
 * Freestanding targets keep the generation-table implementation below until
 * their scheduler-owned parking contract is available. */
#if defined(RIN_HOST_LINUX_FUTEX_TEST) && defined(__has_include)
#if __has_include(<sys/syscall.h>) && __has_include(<linux/futex.h>) && \
    __has_include(<unistd.h>)
#include <sys/syscall.h>
#include <linux/futex.h>
#include <unistd.h>
#define RIN_ATOMIC_HAS_LINUX_FUTEX 1
#endif
#endif

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * Memory order
 * ═══════════════════════════════════════════════════════════════*/

enum class memory_order : int {
    relaxed = 0,
    consume = 1,
    acquire = 2,
    release = 3,
    acq_rel = 4,
    seq_cst = 5
};

static constexpr memory_order memory_order_relaxed = memory_order::relaxed;
static constexpr memory_order memory_order_consume = memory_order::consume;
static constexpr memory_order memory_order_acquire = memory_order::acquire;
static constexpr memory_order memory_order_release = memory_order::release;
static constexpr memory_order memory_order_acq_rel = memory_order::acq_rel;
static constexpr memory_order memory_order_seq_cst = memory_order::seq_cst;

/* ═══════════════════════════════════════════════════════════════
 * Memory fence - x86 implementation
 * ═══════════════════════════════════════════════════════════════*/

inline void atomic_thread_fence(memory_order order) noexcept {
    if (order == memory_order_seq_cst) {
        __asm__ volatile("mfence" ::: "memory");
    } else if (order == memory_order_acquire || order == memory_order_consume) {
        __asm__ volatile("" ::: "memory"); /* x86 loads have acquire semantics */
    } else if (order == memory_order_release) {
        __asm__ volatile("" ::: "memory"); /* x86 stores have release semantics */
    } else if (order == memory_order_acq_rel) {
        __asm__ volatile("" ::: "memory");
    }
    /* relaxed - no fence needed */
}

inline void atomic_signal_fence(memory_order order) noexcept {
    if (order != memory_order_relaxed) {
        __asm__ volatile("" ::: "memory");
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Internal x86 atomic primitives
 * ═══════════════════════════════════════════════════════════════*/

namespace __detail {

/* Compiler barrier */
inline void __compiler_barrier() noexcept {
    __asm__ volatile("" ::: "memory");
}

/* Full memory barrier */
inline void __memory_barrier() noexcept {
    __asm__ volatile("mfence" ::: "memory");
}

/* ─────────────────────────────────────────────────────────────
 * Atomic load operations
 * ─────────────────────────────────────────────────────────────*/

template<typename T>
inline T __atomic_load_1(const volatile T* ptr) noexcept {
    T result;
    __asm__ volatile("movb %1, %0" : "=q"(result) : "m"(*ptr) : "memory");
    return result;
}

template<typename T>
inline T __atomic_load_2(const volatile T* ptr) noexcept {
    T result;
    __asm__ volatile("movw %1, %0" : "=r"(result) : "m"(*ptr) : "memory");
    return result;
}

template<typename T>
inline T __atomic_load_4(const volatile T* ptr) noexcept {
    T result;
    __asm__ volatile("movl %1, %0" : "=r"(result) : "m"(*ptr) : "memory");
    return result;
}

template<typename T>
inline T __atomic_load_8(const volatile T* ptr) noexcept {
    T result;
#if defined(__x86_64__) || defined(_M_X64)
    /* 64-bit: use movq */
    __asm__ volatile("movq %1, %0" : "=r"(result) : "m"(*ptr) : "memory");
#else
    /* On 32-bit x86, cmpxchg8b returns the current value in EDX:EAX when
     * the zero expectation does not match.  A zero value is already equal
     * to the zero desired value, so this is a non-mutating atomic load for
     * both cases.  The old implementation copied EBX/ECX into the expected
     * registers, which could accidentally overwrite the atomic with those
     * unrelated register values. */
    __asm__ volatile(
        "xorl %%eax, %%eax\n\t"
        "xorl %%edx, %%edx\n\t"
        "lock; cmpxchg8b %1"
        : "=&A"(result)
        : "m"(*ptr), "b"(0u), "c"(0u)
        : "cc", "memory"
    );
#endif
    return result;
}

/* ─────────────────────────────────────────────────────────────
 * Atomic store operations
 * ─────────────────────────────────────────────────────────────*/

template<typename T>
inline void __atomic_store_1(volatile T* ptr, T val) noexcept {
    __asm__ volatile("movb %1, %0" : "=m"(*ptr) : "q"(val) : "memory");
}

template<typename T>
inline void __atomic_store_2(volatile T* ptr, T val) noexcept {
    __asm__ volatile("movw %1, %0" : "=m"(*ptr) : "r"(val) : "memory");
}

template<typename T>
inline void __atomic_store_4(volatile T* ptr, T val) noexcept {
    __asm__ volatile("movl %1, %0" : "=m"(*ptr) : "r"(val) : "memory");
}

template<typename T>
inline void __atomic_store_8(volatile T* ptr, T val) noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    /* 64-bit: use movq */
    __asm__ volatile("movq %1, %0" : "=m"(*ptr) : "r"(val) : "memory");
#else
    /* cmpxchg8b updates EDX:EAX with the observed value on failure.  Keep
     * that pair as a read-write expectation so every retry compares against
     * the value just observed instead of looping on a stale snapshot. */
    uint64_t desired;
    uint64_t expected = 0u;
    __builtin_memcpy(&desired, &val, sizeof(desired));
    uint32_t lo = static_cast<uint32_t>(desired);
    uint32_t hi = static_cast<uint32_t>(desired >> 32);
    __asm__ volatile(
        "1:\n\t"
        "lock; cmpxchg8b %1\n\t"
        "jnz 1b"
        : "+A"(expected), "+m"(*ptr)
        : "b"(lo), "c"(hi)
        : "cc", "memory"
    );
#endif
}

/* ─────────────────────────────────────────────────────────────
 * Atomic exchange operations (xchg is implicitly locked on x86)
 * ─────────────────────────────────────────────────────────────*/

template<typename T>
inline T __atomic_exchange_1(volatile T* ptr, T val) noexcept {
    T result;
    __asm__ volatile("xchgb %0, %1" : "=q"(result), "+m"(*ptr) : "0"(val) : "memory");
    return result;
}

template<typename T>
inline T __atomic_exchange_2(volatile T* ptr, T val) noexcept {
    T result;
    __asm__ volatile("xchgw %0, %1" : "=r"(result), "+m"(*ptr) : "0"(val) : "memory");
    return result;
}

template<typename T>
inline T __atomic_exchange_4(volatile T* ptr, T val) noexcept {
    T result;
    __asm__ volatile("xchgl %0, %1" : "=r"(result), "+m"(*ptr) : "0"(val) : "memory");
    return result;
}

template<typename T>
inline T __atomic_exchange_8(volatile T* ptr, T val) noexcept {
    T result;
#if defined(__x86_64__) || defined(_M_X64)
    /* 64-bit: use xchgq */
    __asm__ volatile("xchgq %0, %1" : "=r"(result), "+m"(*ptr) : "0"(val) : "memory");
#else
    /* Use the same retrying expectation as store.  On success EDX:EAX still
     * contains the old value, which is the required exchange result. */
    uint64_t desired;
    uint64_t expected = 0u;
    __builtin_memcpy(&desired, &val, sizeof(desired));
    uint32_t lo = static_cast<uint32_t>(desired);
    uint32_t hi = static_cast<uint32_t>(desired >> 32);
    __asm__ volatile(
        "1:\n\t"
        "lock; cmpxchg8b %1\n\t"
        "jnz 1b"
        : "+A"(expected), "+m"(*ptr)
        : "b"(lo), "c"(hi)
        : "cc", "memory"
    );
    __builtin_memcpy(&result, &expected, sizeof(result));
#endif
    return result;
}

/* ─────────────────────────────────────────────────────────────
 * Compare-and-exchange operations
 * ─────────────────────────────────────────────────────────────*/

template<typename T>
inline bool __atomic_cmpxchg_1(volatile T* ptr, T* expected, T desired) noexcept {
    T exp = *expected;
    T prev;
    bool success;
    __asm__ volatile(
        "lock; cmpxchgb %3, %1"
        : "=a"(prev), "+m"(*ptr), "=@ccz"(success)
        : "q"(desired), "0"(exp)
        : "memory"
    );
    if (!success) *expected = prev;
    return success;
}

template<typename T>
inline bool __atomic_cmpxchg_2(volatile T* ptr, T* expected, T desired) noexcept {
    T exp = *expected;
    T prev;
    bool success;
    __asm__ volatile(
        "lock; cmpxchgw %3, %1"
        : "=a"(prev), "+m"(*ptr), "=@ccz"(success)
        : "r"(desired), "0"(exp)
        : "memory"
    );
    if (!success) *expected = prev;
    return success;
}

template<typename T>
inline bool __atomic_cmpxchg_4(volatile T* ptr, T* expected, T desired) noexcept {
    T exp = *expected;
    T prev;
    bool success;
    __asm__ volatile(
        "lock; cmpxchgl %3, %1"
        : "=a"(prev), "+m"(*ptr), "=@ccz"(success)
        : "r"(desired), "0"(exp)
        : "memory"
    );
    if (!success) *expected = prev;
    return success;
}

template<typename T>
inline bool __atomic_cmpxchg_8(volatile T* ptr, T* expected, T desired) noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    /* 64-bit: use cmpxchgq */
    T exp = *expected;
    T prev;
    bool success;
    __asm__ volatile(
        "lock; cmpxchgq %3, %1"
        : "=a"(prev), "+m"(*ptr), "=@ccz"(success)
        : "r"(desired), "0"(exp)
        : "memory"
    );
    if (!success) *expected = prev;
    return success;
#else
    /* 32-bit: use cmpxchg8b */
    uint64_t exp64 = reinterpret_cast<uint64_t&>(*expected);
    uint64_t des64 = reinterpret_cast<uint64_t&>(desired);
    bool success;
    __asm__ volatile(
        "lock; cmpxchg8b %1"
        : "+A"(exp64), "+m"(*ptr), "=@ccz"(success)
        : "b"(static_cast<uint32_t>(des64)), "c"(static_cast<uint32_t>(des64 >> 32))
        : "memory"
    );
    if (!success) {
        reinterpret_cast<uint64_t&>(*expected) = exp64;
    }
    return success;
#endif
}

/* ─────────────────────────────────────────────────────────────
 * Fetch-and-add operations
 * ─────────────────────────────────────────────────────────────*/

template<typename T>
inline T __atomic_fetch_add_1(volatile T* ptr, T val) noexcept {
    T result;
    __asm__ volatile("lock; xaddb %0, %1" : "=q"(result), "+m"(*ptr) : "0"(val) : "memory");
    return result;
}

template<typename T>
inline T __atomic_fetch_add_2(volatile T* ptr, T val) noexcept {
    T result;
    __asm__ volatile("lock; xaddw %0, %1" : "=r"(result), "+m"(*ptr) : "0"(val) : "memory");
    return result;
}

template<typename T>
inline T __atomic_fetch_add_4(volatile T* ptr, T val) noexcept {
    T result;
    __asm__ volatile("lock; xaddl %0, %1" : "=r"(result), "+m"(*ptr) : "0"(val) : "memory");
    return result;
}

template<typename T>
inline T __atomic_fetch_add_8(volatile T* ptr, T val) noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    /* 64-bit: use lock xaddq */
    T result;
    __asm__ volatile("lock; xaddq %0, %1" : "=r"(result), "+m"(*ptr) : "0"(val) : "memory");
    return result;
#else
    /* CAS loop for 64-bit fetch-add on 32-bit x86 */
    T old_val;
    T new_val;
    do {
        old_val = __atomic_load_8(ptr);
        new_val = old_val + val;
    } while (!__atomic_cmpxchg_8(ptr, &old_val, new_val));
    return old_val;
#endif
}

/* ─────────────────────────────────────────────────────────────
 * Spinlock implementation (for truly non-lock-free cases)
 * ─────────────────────────────────────────────────────────────*/

class __spinlock {
    volatile int locked_ = 0;
public:
    void lock() noexcept {
        while (true) {
            int expected = 0;
            int desired = 1;
            bool success;
            __asm__ volatile(
                "lock; cmpxchgl %3, %1"
                : "=a"(expected), "+m"(locked_), "=@ccz"(success)
                : "r"(desired), "0"(expected)
                : "memory"
            );
            if (success) return;

            /* Spin-wait */
            while (locked_) {
                __asm__ volatile("pause" ::: "memory");
            }
        }
    }

    void unlock() noexcept {
        __asm__ volatile("" ::: "memory"); /* compiler barrier */
        locked_ = 0;
    }
};

/* Global spinlock table for non-lock-free atomics */
inline __spinlock& __get_lock(const void* addr) noexcept {
    static __spinlock locks[16];
    auto idx = (detail::object_pointer_hash(addr) >> 4) & 15;
    return locks[idx];
}

/* ─────────────────────────────────────────────────────────────
 * Type traits for lock-free detection
 * Lock-free if: trivially copyable and size is 1, 2, 4, or 8 bytes
 * ─────────────────────────────────────────────────────────────*/

template<typename T>
struct __is_lock_free_size : integral_constant<bool,
    sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8
> {};

template<typename T>
struct __is_lock_free_type : integral_constant<bool,
    is_trivially_copyable<T>::value && __is_lock_free_size<T>::value
> {};

/* ─────────────────────────────────────────────────────────────
 * Size-dispatched atomic operations
 * ─────────────────────────────────────────────────────────────*/

template<size_t Size>
struct __atomic_ops;

template<>
struct __atomic_ops<1> {
    template<typename T>
    static T load(const volatile T* ptr) noexcept { return __atomic_load_1(ptr); }

    template<typename T>
    static void store(volatile T* ptr, T val) noexcept { __atomic_store_1(ptr, val); }

    template<typename T>
    static T exchange(volatile T* ptr, T val) noexcept { return __atomic_exchange_1(ptr, val); }

    template<typename T>
    static bool cmpxchg(volatile T* ptr, T* exp, T des) noexcept { return __atomic_cmpxchg_1(ptr, exp, des); }

    template<typename T>
    static T fetch_add(volatile T* ptr, T val) noexcept { return __atomic_fetch_add_1(ptr, val); }
};

template<>
struct __atomic_ops<2> {
    template<typename T>
    static T load(const volatile T* ptr) noexcept { return __atomic_load_2(ptr); }

    template<typename T>
    static void store(volatile T* ptr, T val) noexcept { __atomic_store_2(ptr, val); }

    template<typename T>
    static T exchange(volatile T* ptr, T val) noexcept { return __atomic_exchange_2(ptr, val); }

    template<typename T>
    static bool cmpxchg(volatile T* ptr, T* exp, T des) noexcept { return __atomic_cmpxchg_2(ptr, exp, des); }

    template<typename T>
    static T fetch_add(volatile T* ptr, T val) noexcept { return __atomic_fetch_add_2(ptr, val); }
};

template<>
struct __atomic_ops<4> {
    template<typename T>
    static T load(const volatile T* ptr) noexcept { return __atomic_load_4(ptr); }

    template<typename T>
    static void store(volatile T* ptr, T val) noexcept { __atomic_store_4(ptr, val); }

    template<typename T>
    static T exchange(volatile T* ptr, T val) noexcept { return __atomic_exchange_4(ptr, val); }

    template<typename T>
    static bool cmpxchg(volatile T* ptr, T* exp, T des) noexcept { return __atomic_cmpxchg_4(ptr, exp, des); }

    template<typename T>
    static T fetch_add(volatile T* ptr, T val) noexcept { return __atomic_fetch_add_4(ptr, val); }
};

template<>
struct __atomic_ops<8> {
    template<typename T>
    static T load(const volatile T* ptr) noexcept { return __atomic_load_8(ptr); }

    template<typename T>
    static void store(volatile T* ptr, T val) noexcept { __atomic_store_8(ptr, val); }

    template<typename T>
    static T exchange(volatile T* ptr, T val) noexcept { return __atomic_exchange_8(ptr, val); }

    template<typename T>
    static bool cmpxchg(volatile T* ptr, T* exp, T des) noexcept { return __atomic_cmpxchg_8(ptr, exp, des); }

    template<typename T>
    static T fetch_add(volatile T* ptr, T val) noexcept { return __atomic_fetch_add_8(ptr, val); }
};

/*
 * C++20 atomic wait/notify needs a notification channel even when the
 * target has no futex backend.  Keep the atomic object's ABI unchanged by
 * using a bounded, address-hashed generation table.  A hash collision can
 * cause an internal wake/recheck, but wait never returns until the value
 * differs from the requested old value.  The generation is sampled after
 * the value check, so a notify racing with registration cannot be lost.
 */
struct __atomic_wait_registry {
    volatile uint32_t sequence[64];
};

inline __atomic_wait_registry& __atomic_wait_registry_instance() noexcept {
    static __atomic_wait_registry registry = {};
    return registry;
}

inline volatile uint32_t* __atomic_wait_sequence_slot(
    const void* address) noexcept {
    const uintptr_t key = detail::object_pointer_hash(address);
    return &__atomic_wait_registry_instance().sequence[(key >> 4u) & 63u];
}

inline uint32_t __atomic_wait_generation(const void* address) noexcept {
    return __atomic_ops<4>::load(__atomic_wait_sequence_slot(address));
}

inline void __atomic_wait_notify(const void* address, bool all) noexcept {
    volatile uint32_t* sequence = __atomic_wait_sequence_slot(address);
    (void)__atomic_ops<4>::fetch_add(sequence, static_cast<uint32_t>(1u));
#if defined(RIN_ATOMIC_HAS_LINUX_FUTEX)
    /* The generation increment closes the sample→wait race: a waiter that
     * observed the old value receives EAGAIN when the notify won the race.
     * Wake one for notify_one and all for notify_all; hash collisions still
     * only cause a value recheck. */
    (void)syscall(SYS_futex, const_cast<uint32_t*>(sequence), FUTEX_WAKE_PRIVATE,
                  all ? 0x7fffffff : 1, nullptr, nullptr, 0);
#else
    (void)all;
#endif
}

inline void __atomic_wait_futex(volatile uint32_t* sequence,
                                uint32_t generation) noexcept {
#if defined(RIN_ATOMIC_HAS_LINUX_FUTEX)
    (void)syscall(SYS_futex, const_cast<uint32_t*>(sequence), FUTEX_WAIT_PRIVATE,
                  static_cast<int>(generation), nullptr, nullptr, 0);
#else
    (void)sequence;
    (void)generation;
#endif
}

template<class Predicate>
inline void __atomic_wait_until_changed(const void* address,
                                        Predicate&& unchanged) noexcept {
    while (unchanged()) {
        const uint32_t generation = __atomic_wait_generation(address);
        while (unchanged() &&
               __atomic_wait_generation(address) == generation) {
#if defined(RIN_ATOMIC_HAS_LINUX_FUTEX)
            __atomic_wait_futex(__atomic_wait_sequence_slot(address),
                                generation);
#else
            __asm__ volatile("pause" ::: "memory");
#endif
        }
    }
}

} /* namespace __detail */

/* ═══════════════════════════════════════════════════════════════
 * atomic_flag - lock-free boolean
 * ═══════════════════════════════════════════════════════════════*/

class atomic_flag {
    volatile unsigned char flag_ = 0;

public:
    atomic_flag() noexcept = default;

    atomic_flag(const atomic_flag&) = delete;
    atomic_flag& operator=(const atomic_flag&) = delete;
    atomic_flag& operator=(const atomic_flag&) volatile = delete;

    void clear(memory_order order = memory_order_seq_cst) noexcept {
        if (order == memory_order_seq_cst) {
            __detail::__memory_barrier();
        }
        flag_ = 0;
        __detail::__compiler_barrier();
    }

    void clear(memory_order order = memory_order_seq_cst) volatile noexcept {
        if (order == memory_order_seq_cst) {
            __detail::__memory_barrier();
        }
        flag_ = 0;
        __detail::__compiler_barrier();
    }

    bool test_and_set(memory_order order = memory_order_seq_cst) noexcept {
        unsigned char result;
        __asm__ volatile("xchgb %0, %1" : "=q"(result), "+m"(flag_) : "0"((unsigned char)1) : "memory");
        return result != 0;
    }

    bool test_and_set(memory_order order = memory_order_seq_cst) volatile noexcept {
        unsigned char result;
        __asm__ volatile("xchgb %0, %1" : "=q"(result), "+m"(flag_) : "0"((unsigned char)1) : "memory");
        return result != 0;
    }

    bool test(memory_order order = memory_order_seq_cst) const noexcept {
        __detail::__compiler_barrier();
        return flag_ != 0;
    }

    bool test(memory_order order = memory_order_seq_cst) const volatile noexcept {
        __detail::__compiler_barrier();
        return flag_ != 0;
    }

#if __cplusplus >= 202002L
    /* C++20 atomic_flag waiting uses the same generation channel as the
     * integral/pointer atomic specializations.  The value predicate is
     * rechecked after every wake, so hash collisions and spurious wakes do
     * not expose a stale flag state. */
    void wait(bool old, memory_order order = memory_order_seq_cst) const
        noexcept {
        __detail::__atomic_wait_until_changed(
            this, [this, old, order]() noexcept {
                return test(order) == old;
            });
    }

    void wait(bool old, memory_order order = memory_order_seq_cst) const volatile
        noexcept {
        const_cast<const atomic_flag*>(this)->wait(old, order);
    }

    void notify_one() noexcept {
        __detail::__atomic_wait_notify(this, false);
    }

    void notify_one() volatile noexcept {
        __detail::__atomic_wait_notify(
            const_cast<const atomic_flag*>(this), false);
    }

    void notify_all() noexcept {
        __detail::__atomic_wait_notify(this, true);
    }

    void notify_all() volatile noexcept {
        __detail::__atomic_wait_notify(
            const_cast<const atomic_flag*>(this), true);
    }
#endif
};

#define ATOMIC_FLAG_INIT { 0 }

/* ═══════════════════════════════════════════════════════════════
 * atomic_base - lock-free implementation for 1/2/4/8 byte types
 * ═══════════════════════════════════════════════════════════════*/

namespace __detail {

template<class T>
inline T __atomic_wrap_add(T lhs, T rhs) noexcept {
    using unsigned_type = typename make_unsigned<T>::type;
    return static_cast<T>(static_cast<unsigned_type>(lhs) +
                          static_cast<unsigned_type>(rhs));
}

template<class T>
inline T __atomic_wrap_sub(T lhs, T rhs) noexcept {
    using unsigned_type = typename make_unsigned<T>::type;
    return static_cast<T>(static_cast<unsigned_type>(lhs) -
                          static_cast<unsigned_type>(rhs));
}

template<class T, bool IsLockFree = __is_lock_free_type<T>::value>
class __atomic_base;

/* Lock-free specialization */
template<class T>
class __atomic_base<T, true> {
protected:
    alignas(sizeof(T)) mutable volatile T value_;

    using ops = __atomic_ops<sizeof(T)>;

public:
    using value_type = T;

    static constexpr bool is_always_lock_free = true;

    __atomic_base() noexcept = default;
    constexpr __atomic_base(T desired) noexcept : value_(desired) {}

    __atomic_base(const __atomic_base&) = delete;
    __atomic_base& operator=(const __atomic_base&) = delete;

    bool is_lock_free() const noexcept { return true; }
    bool is_lock_free() const volatile noexcept { return true; }

    void store(T desired, memory_order order = memory_order_seq_cst) noexcept {
        if (order == memory_order_seq_cst) {
            /* Use xchg for seq_cst store (implicitly locked) */
            ops::exchange(&value_, desired);
        } else {
            if (order == memory_order_release) {
                __compiler_barrier();
            }
            ops::store(&value_, desired);
            __compiler_barrier();
        }
    }

    void store(T desired, memory_order order = memory_order_seq_cst) volatile noexcept {
        const_cast<__atomic_base*>(this)->store(desired, order);
    }

    T load(memory_order order = memory_order_seq_cst) const noexcept {
        T result = ops::load(&value_);
        if (order == memory_order_seq_cst || order == memory_order_acquire) {
            __compiler_barrier();
        }
        return result;
    }

    T load(memory_order order = memory_order_seq_cst) const volatile noexcept {
        return const_cast<const __atomic_base*>(this)->load(order);
    }

    operator T() const noexcept { return load(); }
    operator T() const volatile noexcept { return load(); }

    T operator=(T desired) noexcept { store(desired); return desired; }
    T operator=(T desired) volatile noexcept { store(desired); return desired; }

    T exchange(T desired, memory_order order = memory_order_seq_cst) noexcept {
        return ops::exchange(&value_, desired);
    }

    T exchange(T desired, memory_order order = memory_order_seq_cst) volatile noexcept {
        return const_cast<__atomic_base*>(this)->exchange(desired, order);
    }

    bool compare_exchange_weak(T& expected, T desired,
                               memory_order success,
                               memory_order failure) noexcept {
        return ops::cmpxchg(&value_, &expected, desired);
    }

    bool compare_exchange_weak(T& expected, T desired,
                               memory_order order = memory_order_seq_cst) noexcept {
        return compare_exchange_weak(expected, desired, order, order);
    }

    bool compare_exchange_strong(T& expected, T desired,
                                 memory_order success,
                                 memory_order failure) noexcept {
        return ops::cmpxchg(&value_, &expected, desired);
    }

    bool compare_exchange_strong(T& expected, T desired,
                                 memory_order order = memory_order_seq_cst) noexcept {
        return compare_exchange_strong(expected, desired, order, order);
    }

    void wait(T old, memory_order order = memory_order_seq_cst) const noexcept {
        __detail::__atomic_wait_until_changed(
            this, [this, old, order]() noexcept { return load(order) == old; });
    }

    void notify_one() noexcept { __detail::__atomic_wait_notify(this, false); }
    void notify_all() noexcept { __detail::__atomic_wait_notify(this, true); }
};

/* Spinlock-based specialization for non-lock-free types */
template<class T>
class __atomic_base<T, false> {
protected:
    mutable T value_;

public:
    using value_type = T;

    static constexpr bool is_always_lock_free = false;

    __atomic_base() noexcept = default;
    constexpr __atomic_base(T desired) noexcept : value_(desired) {}

    __atomic_base(const __atomic_base&) = delete;
    __atomic_base& operator=(const __atomic_base&) = delete;

    bool is_lock_free() const noexcept { return false; }
    bool is_lock_free() const volatile noexcept { return false; }

    void store(T desired, memory_order order = memory_order_seq_cst) noexcept {
        auto& lock = __get_lock(&value_);
        lock.lock();
        for (size_t i = 0; i < sizeof(T); ++i) {
            reinterpret_cast<volatile char*>(&value_)[i] =
                reinterpret_cast<const char*>(&desired)[i];
        }
        lock.unlock();
        if (order == memory_order_seq_cst) {
            __memory_barrier();
        }
    }

    T load(memory_order order = memory_order_seq_cst) const noexcept {
        T result;
        auto& lock = __get_lock(&value_);
        lock.lock();
        for (size_t i = 0; i < sizeof(T); ++i) {
            reinterpret_cast<char*>(&result)[i] =
                reinterpret_cast<const volatile char*>(&value_)[i];
        }
        lock.unlock();
        return result;
    }

    operator T() const noexcept { return load(); }

    T operator=(T desired) noexcept { store(desired); return desired; }

    T exchange(T desired, memory_order order = memory_order_seq_cst) noexcept {
        T result;
        auto& lock = __get_lock(&value_);
        lock.lock();
        for (size_t i = 0; i < sizeof(T); ++i) {
            reinterpret_cast<char*>(&result)[i] =
                reinterpret_cast<const volatile char*>(&value_)[i];
            reinterpret_cast<volatile char*>(&value_)[i] =
                reinterpret_cast<const char*>(&desired)[i];
        }
        lock.unlock();
        return result;
    }

    /* Integral atomics remain required to provide read-modify-write
     * operations even when their representation is larger than the
     * lock-free set.  Keep the operation under the same address-hashed
     * spinlock used by load/store.  Signed arithmetic is performed in the
     * corresponding unsigned type so wraparound never invokes signed-overflow
     * undefined behaviour (this also covers the supported __int128 ABI). */
    template<typename U = T>
    typename enable_if<is_integral<U>::value &&
                       !is_same<U, bool>::value, U>::type
    __fetch_add(U arg, memory_order order = memory_order_seq_cst) noexcept {
        (void)order;
        using unsigned_type = typename make_unsigned<U>::type;
        auto& lock = __get_lock(&value_);
        lock.lock();
        const U old_value = value_;
        const unsigned_type next =
            static_cast<unsigned_type>(old_value) +
            static_cast<unsigned_type>(arg);
        value_ = static_cast<T>(next);
        lock.unlock();
        return old_value;
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value &&
                       !is_same<U, bool>::value, U>::type
    __fetch_sub(U arg, memory_order order = memory_order_seq_cst) noexcept {
        (void)order;
        using unsigned_type = typename make_unsigned<U>::type;
        auto& lock = __get_lock(&value_);
        lock.lock();
        const U old_value = value_;
        const unsigned_type next =
            static_cast<unsigned_type>(old_value) -
            static_cast<unsigned_type>(arg);
        value_ = static_cast<T>(next);
        lock.unlock();
        return old_value;
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value &&
                       !is_same<U, bool>::value, U>::type
    __fetch_and(U arg, memory_order order = memory_order_seq_cst) noexcept {
        (void)order;
        auto& lock = __get_lock(&value_);
        lock.lock();
        const U old_value = value_;
        value_ = static_cast<T>(old_value & arg);
        lock.unlock();
        return old_value;
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value &&
                       !is_same<U, bool>::value, U>::type
    __fetch_or(U arg, memory_order order = memory_order_seq_cst) noexcept {
        (void)order;
        auto& lock = __get_lock(&value_);
        lock.lock();
        const U old_value = value_;
        value_ = static_cast<T>(old_value | arg);
        lock.unlock();
        return old_value;
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value &&
                       !is_same<U, bool>::value, U>::type
    __fetch_xor(U arg, memory_order order = memory_order_seq_cst) noexcept {
        (void)order;
        auto& lock = __get_lock(&value_);
        lock.lock();
        const U old_value = value_;
        value_ = static_cast<T>(old_value ^ arg);
        lock.unlock();
        return old_value;
    }

    bool compare_exchange_weak(T& expected, T desired,
                               memory_order success,
                               memory_order failure) noexcept {
        return compare_exchange_strong(expected, desired, success, failure);
    }

    bool compare_exchange_weak(T& expected, T desired,
                               memory_order order = memory_order_seq_cst) noexcept {
        return compare_exchange_strong(expected, desired, order, order);
    }

    bool compare_exchange_strong(T& expected, T desired,
                                 memory_order success,
                                 memory_order failure) noexcept {
        auto& lock = __get_lock(&value_);
        lock.lock();
        bool equal = true;
        for (size_t i = 0; i < sizeof(T); ++i) {
            if (reinterpret_cast<const volatile char*>(&value_)[i] !=
                reinterpret_cast<const char*>(&expected)[i]) {
                equal = false;
                break;
            }
        }
        if (equal) {
            for (size_t i = 0; i < sizeof(T); ++i) {
                reinterpret_cast<volatile char*>(&value_)[i] =
                    reinterpret_cast<const char*>(&desired)[i];
            }
            lock.unlock();
            return true;
        } else {
            for (size_t i = 0; i < sizeof(T); ++i) {
                reinterpret_cast<char*>(&expected)[i] =
                    reinterpret_cast<const volatile char*>(&value_)[i];
            }
            lock.unlock();
            return false;
        }
    }

    bool compare_exchange_strong(T& expected, T desired,
                                 memory_order order = memory_order_seq_cst) noexcept {
        return compare_exchange_strong(expected, desired, order, order);
    }

    void wait(T old, memory_order order = memory_order_seq_cst) const noexcept {
        __detail::__atomic_wait_until_changed(
            this, [this, old, order]() noexcept {
                T current = load(order);
                for (size_t i = 0; i < sizeof(T); ++i) {
                    if (reinterpret_cast<const char*>(&current)[i] !=
                        reinterpret_cast<const char*>(&old)[i]) {
                        return false;
                    }
                }
                return true;
            });
    }

    void notify_one() noexcept { __detail::__atomic_wait_notify(this, false); }
    void notify_all() noexcept { __detail::__atomic_wait_notify(this, true); }
};

} /* namespace __detail */

/* ═══════════════════════════════════════════════════════════════
 * atomic<T> - generic atomic type
 * ═══════════════════════════════════════════════════════════════*/

template<class T>
class atomic : public __detail::__atomic_base<T> {
    static_assert(is_trivially_copyable<T>::value,
                  "atomic requires trivially copyable type");

    using base = __detail::__atomic_base<T>;
    using ops = __detail::__atomic_ops<sizeof(T)>;

public:
    using base::base;
    using base::operator=;

    /* Integral operations (only for integral types with lock-free support) */
    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value && __detail::__is_lock_free_type<U>::value, T>::type
    fetch_add(T arg, memory_order order = memory_order_seq_cst) noexcept {
        return ops::fetch_add(&this->value_, arg);
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value && __detail::__is_lock_free_type<U>::value, T>::type
    fetch_sub(T arg, memory_order order = memory_order_seq_cst) noexcept {
        using unsigned_type = typename make_unsigned<T>::type;
        const unsigned_type delta = static_cast<unsigned_type>(0) -
            static_cast<unsigned_type>(arg);
        return ops::fetch_add(&this->value_, static_cast<T>(delta));
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value && __detail::__is_lock_free_type<U>::value, T>::type
    fetch_and(T arg, memory_order order = memory_order_seq_cst) noexcept {
        T old_val;
        T new_val;
        do {
            old_val = this->load(memory_order_relaxed);
            new_val = old_val & arg;
        } while (!this->compare_exchange_weak(old_val, new_val, order, memory_order_relaxed));
        return old_val;
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value && __detail::__is_lock_free_type<U>::value, T>::type
    fetch_or(T arg, memory_order order = memory_order_seq_cst) noexcept {
        T old_val;
        T new_val;
        do {
            old_val = this->load(memory_order_relaxed);
            new_val = old_val | arg;
        } while (!this->compare_exchange_weak(old_val, new_val, order, memory_order_relaxed));
        return old_val;
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value && __detail::__is_lock_free_type<U>::value, T>::type
    fetch_xor(T arg, memory_order order = memory_order_seq_cst) noexcept {
        T old_val;
        T new_val;
        do {
            old_val = this->load(memory_order_relaxed);
            new_val = old_val ^ arg;
        } while (!this->compare_exchange_weak(old_val, new_val, order, memory_order_relaxed));
        return old_val;
    }

    /* The generic base serializes wider-than-word integral values.  Expose
     * the same standard RMW surface as the lock-free specialization without
     * pretending that the operation is lock-free. */
    template<typename U = T>
    typename enable_if<is_integral<U>::value &&
                       !is_same<U, bool>::value &&
                       !__detail::__is_lock_free_type<U>::value, U>::type
    fetch_add(U arg, memory_order order = memory_order_seq_cst) noexcept {
        return base::__fetch_add(arg, order);
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value &&
                       !is_same<U, bool>::value &&
                       !__detail::__is_lock_free_type<U>::value, U>::type
    fetch_sub(U arg, memory_order order = memory_order_seq_cst) noexcept {
        return base::__fetch_sub(arg, order);
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value &&
                       !is_same<U, bool>::value &&
                       !__detail::__is_lock_free_type<U>::value, U>::type
    fetch_and(U arg, memory_order order = memory_order_seq_cst) noexcept {
        return base::__fetch_and(arg, order);
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value &&
                       !is_same<U, bool>::value &&
                       !__detail::__is_lock_free_type<U>::value, U>::type
    fetch_or(U arg, memory_order order = memory_order_seq_cst) noexcept {
        return base::__fetch_or(arg, order);
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value &&
                       !is_same<U, bool>::value &&
                       !__detail::__is_lock_free_type<U>::value, U>::type
    fetch_xor(U arg, memory_order order = memory_order_seq_cst) noexcept {
        return base::__fetch_xor(arg, order);
    }

#if __cplusplus >= 202002L
    /* C++20 floating atomics use a compare-exchange loop.  The integer
     * xadd instruction cannot operate on IEEE representations: perform the
     * arithmetic in the value domain, while compare_exchange keeps the
     * update atomic for both lock-free and hashed-lock representations. */
    template<typename U = T>
    typename enable_if<is_floating_point<U>::value, U>::type
    fetch_add(U arg, memory_order order = memory_order_seq_cst) noexcept {
        U old = this->load(memory_order_relaxed);
        for (;;) {
            const U desired = static_cast<U>(old + arg);
            if (this->compare_exchange_weak(old, desired, order,
                                            memory_order_relaxed)) {
                return old;
            }
        }
    }

    template<typename U = T>
    typename enable_if<is_floating_point<U>::value, U>::type
    fetch_sub(U arg, memory_order order = memory_order_seq_cst) noexcept {
        U old = this->load(memory_order_relaxed);
        for (;;) {
            const U desired = static_cast<U>(old - arg);
            if (this->compare_exchange_weak(old, desired, order,
                                            memory_order_relaxed)) {
                return old;
            }
        }
    }
#endif

    /* Integral operators */
    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator++() noexcept {
        const T one = static_cast<T>(1);
        return __detail::__atomic_wrap_add(fetch_add(one), one);
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator++(int) noexcept { return fetch_add(static_cast<T>(1)); }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator--() noexcept {
        const T one = static_cast<T>(1);
        return __detail::__atomic_wrap_sub(fetch_sub(one), one);
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator--(int) noexcept { return fetch_sub(static_cast<T>(1)); }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator+=(T arg) noexcept { return __detail::__atomic_wrap_add(fetch_add(arg), arg); }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator-=(T arg) noexcept { return __detail::__atomic_wrap_sub(fetch_sub(arg), arg); }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator&=(T arg) noexcept { return fetch_and(arg) & arg; }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator|=(T arg) noexcept { return fetch_or(arg) | arg; }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator^=(T arg) noexcept { return fetch_xor(arg) ^ arg; }
};

/* ═══════════════════════════════════════════════════════════════
 * atomic<T*> - pointer specialization
 * Uses uintptr_t internally for operations, T* for storage
 * ═══════════════════════════════════════════════════════════════*/

template<class T>
class atomic<T*> {
    /* Union allows constexpr init via pointer, runtime access via raw bytes */
    union {
        alignas(sizeof(T*)) unsigned char storage_[sizeof(T*)];
        T* ptr_init_;
    };

public:
    using value_type = T*;
    using difference_type = ptrdiff_t;

    static constexpr bool is_always_lock_free = true;

    /* Default constructor - constant initialization to nullptr */
    constexpr atomic() noexcept : ptr_init_(nullptr) {}

    /* nullptr constructor - constant initialization */
    constexpr atomic(decltype(nullptr)) noexcept : ptr_init_(nullptr) {}

    /* Pointer constructor - constexpr for constinit support */
    constexpr atomic(T* desired) noexcept : ptr_init_(desired) {}

    atomic(const atomic&) = delete;
    atomic& operator=(const atomic&) = delete;

    bool is_lock_free() const noexcept { return true; }

    void store(T* desired, memory_order order = memory_order_seq_cst) noexcept {
        if (order == memory_order_seq_cst) {
            __detail::__atomic_ops<sizeof(T*)>::exchange(
                reinterpret_cast<volatile uintptr_t*>(storage_),
                reinterpret_cast<uintptr_t>(desired));
        } else {
            if (order == memory_order_release) {
                __detail::__compiler_barrier();
            }
            __detail::__atomic_ops<sizeof(T*)>::store(
                reinterpret_cast<volatile uintptr_t*>(storage_),
                reinterpret_cast<uintptr_t>(desired));
            __detail::__compiler_barrier();
        }
    }

    T* load(memory_order order = memory_order_seq_cst) const noexcept {
        uintptr_t raw = __detail::__atomic_ops<sizeof(T*)>::load(
            reinterpret_cast<const volatile uintptr_t*>(storage_));
        if (order == memory_order_seq_cst || order == memory_order_acquire) {
            __detail::__compiler_barrier();
        }
        return reinterpret_cast<T*>(raw);
    }

    operator T*() const noexcept { return load(); }
    T* operator=(T* desired) noexcept { store(desired); return desired; }

    T* exchange(T* desired, memory_order order = memory_order_seq_cst) noexcept {
        return reinterpret_cast<T*>(
            __detail::__atomic_ops<sizeof(T*)>::exchange(
                reinterpret_cast<volatile uintptr_t*>(storage_),
                reinterpret_cast<uintptr_t>(desired)));
    }

    bool compare_exchange_weak(T*& expected, T* desired,
                               memory_order success,
                               memory_order failure) noexcept {
        return __detail::__atomic_ops<sizeof(T*)>::cmpxchg(
            reinterpret_cast<volatile uintptr_t*>(storage_),
            reinterpret_cast<uintptr_t*>(&expected),
            reinterpret_cast<uintptr_t>(desired));
    }

    bool compare_exchange_weak(T*& expected, T* desired,
                               memory_order order = memory_order_seq_cst) noexcept {
        return compare_exchange_weak(expected, desired, order, order);
    }

    bool compare_exchange_strong(T*& expected, T* desired,
                                 memory_order success,
                                 memory_order failure) noexcept {
        return compare_exchange_weak(expected, desired, success, failure);
    }

    bool compare_exchange_strong(T*& expected, T* desired,
                                 memory_order order = memory_order_seq_cst) noexcept {
        return compare_exchange_strong(expected, desired, order, order);
    }

    T* fetch_add(ptrdiff_t arg, memory_order order = memory_order_seq_cst) noexcept {
        T* old_val;
        T* new_val;
        do {
            old_val = load(memory_order_relaxed);
            new_val = old_val + arg;
        } while (!compare_exchange_weak(old_val, new_val, order, memory_order_relaxed));
        return old_val;
    }

    T* fetch_sub(ptrdiff_t arg, memory_order order = memory_order_seq_cst) noexcept {
        /* Do not negate arg: PTRDIFF_MIN cannot be represented as its
         * positive counterpart.  Pointer subtraction accepts the signed
         * offset directly and preserves the atomic CAS transaction. */
        T* old_val;
        T* new_val;
        do {
            old_val = load(memory_order_relaxed);
            new_val = old_val - arg;
        } while (!compare_exchange_weak(old_val, new_val, order,
                                        memory_order_relaxed));
        return old_val;
    }

    T* operator++() noexcept { return fetch_add(1) + 1; }
    T* operator++(int) noexcept { return fetch_add(1); }
    T* operator--() noexcept { return fetch_sub(1) - 1; }
    T* operator--(int) noexcept { return fetch_sub(1); }
    T* operator+=(ptrdiff_t arg) noexcept { return fetch_add(arg) + arg; }
    T* operator-=(ptrdiff_t arg) noexcept { return fetch_sub(arg) - arg; }

    void wait(T* old, memory_order order = memory_order_seq_cst) const noexcept {
        __detail::__atomic_wait_until_changed(
            this, [this, old, order]() noexcept { return load(order) == old; });
    }

    void notify_one() noexcept { __detail::__atomic_wait_notify(this, false); }
    void notify_all() noexcept { __detail::__atomic_wait_notify(this, true); }
};

/* ═══════════════════════════════════════════════════════════════
 * atomic<shared_ptr<T>> - C++20 smart-pointer specialization
 *
 * shared_ptr is intentionally not trivially copyable, so the primary
 * atomic<T> cannot represent it.  Reuse the C++11 free-function gate to
 * provide the indivisible ownership operations while conservatively
 * reporting a non-lock-free implementation.  The gate also keeps the
 * reference-count increments/decrements inside each snapshot transaction.
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 202002L
template<class T>
class atomic<shared_ptr<T>> {
    shared_ptr<T> value_;

public:
    using value_type = shared_ptr<T>;
    static constexpr bool is_always_lock_free = false;

    atomic() noexcept : value_() {}
    atomic(shared_ptr<T> desired) noexcept : value_(std::move(desired)) {}

    atomic(const atomic&) = delete;
    atomic& operator=(const atomic&) = delete;

    bool is_lock_free() const noexcept { return false; }

    void store(shared_ptr<T> desired,
               memory_order order = memory_order_seq_cst) noexcept {
        atomic_store_explicit(&value_, std::move(desired), order);
    }

    shared_ptr<T> load(
        memory_order order = memory_order_seq_cst) const noexcept {
        return atomic_load_explicit(&value_, order);
    }

    operator shared_ptr<T>() const noexcept { return load(); }

    shared_ptr<T> exchange(
        shared_ptr<T> desired,
        memory_order order = memory_order_seq_cst) noexcept {
        return atomic_exchange_explicit(&value_, std::move(desired), order);
    }

    bool compare_exchange_weak(
        shared_ptr<T>& expected, shared_ptr<T> desired,
        memory_order success, memory_order failure) noexcept {
        return atomic_compare_exchange_weak_explicit(
            &value_, &expected, std::move(desired), success, failure);
    }

    bool compare_exchange_weak(
        shared_ptr<T>& expected, shared_ptr<T> desired,
        memory_order order = memory_order_seq_cst) noexcept {
        return compare_exchange_weak(expected, std::move(desired), order,
                                      order);
    }

    bool compare_exchange_strong(
        shared_ptr<T>& expected, shared_ptr<T> desired,
        memory_order success, memory_order failure) noexcept {
        return atomic_compare_exchange_strong_explicit(
            &value_, &expected, std::move(desired), success, failure);
    }

    bool compare_exchange_strong(
        shared_ptr<T>& expected, shared_ptr<T> desired,
        memory_order order = memory_order_seq_cst) noexcept {
        return compare_exchange_strong(expected, std::move(desired), order,
                                        order);
    }

    shared_ptr<T> operator=(shared_ptr<T> desired) noexcept {
        store(std::move(desired));
        return load(memory_order_relaxed);
    }

    void wait(shared_ptr<T> old,
              memory_order order = memory_order_seq_cst) const noexcept {
        __detail::__atomic_wait_until_changed(
            this, [this, old, order]() noexcept {
                return shared_ptr_atomic_equal(load(order), old);
            });
    }

    void notify_one() noexcept { __detail::__atomic_wait_notify(this, false); }
    void notify_all() noexcept { __detail::__atomic_wait_notify(this, true); }
};
#endif

/* ═══════════════════════════════════════════════════════════════
 * atomic_ref (C++20) - atomic access to existing objects
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 202002L
template<class T>
class atomic_ref {
    static_assert(is_trivially_copyable<T>::value,
                  "atomic_ref requires trivially copyable type");

    T* ptr_;

    static constexpr bool __is_lock_free = __detail::__is_lock_free_type<T>::value;
    using ops = __detail::__atomic_ops<sizeof(T)>;

public:
    using value_type = T;

    static constexpr bool is_always_lock_free = __is_lock_free;
    static constexpr size_t required_alignment = alignof(T);

    explicit atomic_ref(T& obj) noexcept : ptr_(&obj) {}
    atomic_ref(const atomic_ref&) noexcept = default;
    atomic_ref& operator=(const atomic_ref&) = delete;

    bool is_lock_free() const noexcept { return __is_lock_free; }

    void store(T desired, memory_order order = memory_order_seq_cst) const noexcept {
        if constexpr (__is_lock_free) {
            if (order == memory_order_seq_cst) {
                ops::exchange(reinterpret_cast<volatile T*>(ptr_), desired);
            } else {
                if (order == memory_order_release) {
                    __detail::__compiler_barrier();
                }
                ops::store(reinterpret_cast<volatile T*>(ptr_), desired);
                __detail::__compiler_barrier();
            }
        } else {
            auto& lock = __detail::__get_lock(ptr_);
            lock.lock();
            for (size_t i = 0; i < sizeof(T); ++i) {
                reinterpret_cast<volatile char*>(ptr_)[i] =
                    reinterpret_cast<const char*>(&desired)[i];
            }
            lock.unlock();
        }
    }

    T load(memory_order order = memory_order_seq_cst) const noexcept {
        if constexpr (__is_lock_free) {
            T result = ops::load(reinterpret_cast<const volatile T*>(ptr_));
            if (order == memory_order_seq_cst || order == memory_order_acquire) {
                __detail::__compiler_barrier();
            }
            return result;
        } else {
            T result;
            auto& lock = __detail::__get_lock(ptr_);
            lock.lock();
            for (size_t i = 0; i < sizeof(T); ++i) {
                reinterpret_cast<char*>(&result)[i] =
                    reinterpret_cast<const volatile char*>(ptr_)[i];
            }
            lock.unlock();
            return result;
        }
    }

    operator T() const noexcept { return load(); }
    T operator=(T desired) const noexcept { store(desired); return desired; }

    T exchange(T desired, memory_order order = memory_order_seq_cst) const noexcept {
        if constexpr (__is_lock_free) {
            return ops::exchange(reinterpret_cast<volatile T*>(ptr_), desired);
        } else {
            T result;
            auto& lock = __detail::__get_lock(ptr_);
            lock.lock();
            for (size_t i = 0; i < sizeof(T); ++i) {
                reinterpret_cast<char*>(&result)[i] =
                    reinterpret_cast<const volatile char*>(ptr_)[i];
                reinterpret_cast<volatile char*>(ptr_)[i] =
                    reinterpret_cast<const char*>(&desired)[i];
            }
            lock.unlock();
            return result;
        }
    }

    bool compare_exchange_weak(T& expected, T desired,
                               memory_order success,
                               memory_order failure) const noexcept {
        if constexpr (__is_lock_free) {
            return ops::cmpxchg(reinterpret_cast<volatile T*>(ptr_), &expected, desired);
        } else {
            return compare_exchange_strong(expected, desired, success, failure);
        }
    }

    bool compare_exchange_strong(T& expected, T desired,
                                 memory_order success,
                                 memory_order failure) const noexcept {
        if constexpr (__is_lock_free) {
            return ops::cmpxchg(reinterpret_cast<volatile T*>(ptr_), &expected, desired);
        } else {
            auto& lock = __detail::__get_lock(ptr_);
            lock.lock();
            bool equal = true;
            for (size_t i = 0; i < sizeof(T); ++i) {
                if (reinterpret_cast<const volatile char*>(ptr_)[i] !=
                    reinterpret_cast<const char*>(&expected)[i]) {
                    equal = false;
                    break;
                }
            }
            if (equal) {
                for (size_t i = 0; i < sizeof(T); ++i) {
                    reinterpret_cast<volatile char*>(ptr_)[i] =
                        reinterpret_cast<const char*>(&desired)[i];
                }
                lock.unlock();
                return true;
            } else {
                for (size_t i = 0; i < sizeof(T); ++i) {
                    reinterpret_cast<char*>(&expected)[i] =
                        reinterpret_cast<const volatile char*>(ptr_)[i];
                }
                lock.unlock();
                return false;
            }
        }
    }

    bool compare_exchange_weak(T& expected, T desired,
                               memory_order order = memory_order_seq_cst) const noexcept {
        return compare_exchange_weak(expected, desired, order, order);
    }

    bool compare_exchange_strong(T& expected, T desired,
                                 memory_order order = memory_order_seq_cst) const noexcept {
        return compare_exchange_strong(expected, desired, order, order);
    }

    /* Integral operations */
    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value && __is_lock_free, T>::type
    fetch_add(T arg, memory_order order = memory_order_seq_cst) const noexcept {
        return ops::fetch_add(reinterpret_cast<volatile T*>(ptr_), arg);
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value && __is_lock_free, T>::type
    fetch_sub(T arg, memory_order order = memory_order_seq_cst) const noexcept {
        using unsigned_type = typename make_unsigned<T>::type;
        const unsigned_type delta = static_cast<unsigned_type>(0) -
            static_cast<unsigned_type>(arg);
        return ops::fetch_add(reinterpret_cast<volatile T*>(ptr_),
            static_cast<T>(delta));
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value && __is_lock_free, T>::type
    fetch_and(T arg, memory_order order = memory_order_seq_cst) const noexcept {
        T old_val;
        T new_val;
        do {
            old_val = load(memory_order_relaxed);
            new_val = old_val & arg;
        } while (!compare_exchange_weak(old_val, new_val, order, memory_order_relaxed));
        return old_val;
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value && __is_lock_free, T>::type
    fetch_or(T arg, memory_order order = memory_order_seq_cst) const noexcept {
        T old_val;
        T new_val;
        do {
            old_val = load(memory_order_relaxed);
            new_val = old_val | arg;
        } while (!compare_exchange_weak(old_val, new_val, order, memory_order_relaxed));
        return old_val;
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value && __is_lock_free, T>::type
    fetch_xor(T arg, memory_order order = memory_order_seq_cst) const noexcept {
        T old_val;
        T new_val;
        do {
            old_val = load(memory_order_relaxed);
            new_val = old_val ^ arg;
        } while (!compare_exchange_weak(old_val, new_val, order, memory_order_relaxed));
        return old_val;
    }

    /* Integral operations for wider-than-word objects use the same hashed
     * lock as load/store.  Keep arithmetic in the unsigned counterpart so
     * signed wraparound is defined and commit the bytes only while locked. */
    template<typename U = T>
    typename enable_if<is_integral<U>::value &&
                       !is_same<U, bool>::value && !__is_lock_free, U>::type
    fetch_add(U arg, memory_order order = memory_order_seq_cst) const noexcept {
        (void)order;
        using unsigned_type = typename make_unsigned<U>::type;
        auto& lock = __detail::__get_lock(ptr_);
        lock.lock();
        U old_value;
        for (size_t i = 0; i < sizeof(U); ++i) {
            reinterpret_cast<char*>(&old_value)[i] =
                reinterpret_cast<const volatile char*>(ptr_)[i];
        }
        const unsigned_type next =
            static_cast<unsigned_type>(old_value) +
            static_cast<unsigned_type>(arg);
        const U next_value = static_cast<U>(next);
        for (size_t i = 0; i < sizeof(U); ++i) {
            reinterpret_cast<volatile char*>(ptr_)[i] =
                reinterpret_cast<const char*>(&next_value)[i];
        }
        lock.unlock();
        return old_value;
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value &&
                       !is_same<U, bool>::value && !__is_lock_free, U>::type
    fetch_sub(U arg, memory_order order = memory_order_seq_cst) const noexcept {
        (void)order;
        using unsigned_type = typename make_unsigned<U>::type;
        auto& lock = __detail::__get_lock(ptr_);
        lock.lock();
        U old_value;
        for (size_t i = 0; i < sizeof(U); ++i) {
            reinterpret_cast<char*>(&old_value)[i] =
                reinterpret_cast<const volatile char*>(ptr_)[i];
        }
        const unsigned_type next =
            static_cast<unsigned_type>(old_value) -
            static_cast<unsigned_type>(arg);
        const U next_value = static_cast<U>(next);
        for (size_t i = 0; i < sizeof(U); ++i) {
            reinterpret_cast<volatile char*>(ptr_)[i] =
                reinterpret_cast<const char*>(&next_value)[i];
        }
        lock.unlock();
        return old_value;
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value &&
                       !is_same<U, bool>::value && !__is_lock_free, U>::type
    fetch_and(U arg, memory_order order = memory_order_seq_cst) const noexcept {
        (void)order;
        using unsigned_type = typename make_unsigned<U>::type;
        auto& lock = __detail::__get_lock(ptr_);
        lock.lock();
        U old_value;
        for (size_t i = 0; i < sizeof(U); ++i) {
            reinterpret_cast<char*>(&old_value)[i] =
                reinterpret_cast<const volatile char*>(ptr_)[i];
        }
        const unsigned_type next =
            static_cast<unsigned_type>(old_value) &
            static_cast<unsigned_type>(arg);
        const U next_value = static_cast<U>(next);
        for (size_t i = 0; i < sizeof(U); ++i) {
            reinterpret_cast<volatile char*>(ptr_)[i] =
                reinterpret_cast<const char*>(&next_value)[i];
        }
        lock.unlock();
        return old_value;
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value &&
                       !is_same<U, bool>::value && !__is_lock_free, U>::type
    fetch_or(U arg, memory_order order = memory_order_seq_cst) const noexcept {
        (void)order;
        using unsigned_type = typename make_unsigned<U>::type;
        auto& lock = __detail::__get_lock(ptr_);
        lock.lock();
        U old_value;
        for (size_t i = 0; i < sizeof(U); ++i) {
            reinterpret_cast<char*>(&old_value)[i] =
                reinterpret_cast<const volatile char*>(ptr_)[i];
        }
        const unsigned_type next =
            static_cast<unsigned_type>(old_value) |
            static_cast<unsigned_type>(arg);
        const U next_value = static_cast<U>(next);
        for (size_t i = 0; i < sizeof(U); ++i) {
            reinterpret_cast<volatile char*>(ptr_)[i] =
                reinterpret_cast<const char*>(&next_value)[i];
        }
        lock.unlock();
        return old_value;
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value &&
                       !is_same<U, bool>::value && !__is_lock_free, U>::type
    fetch_xor(U arg, memory_order order = memory_order_seq_cst) const noexcept {
        (void)order;
        using unsigned_type = typename make_unsigned<U>::type;
        auto& lock = __detail::__get_lock(ptr_);
        lock.lock();
        U old_value;
        for (size_t i = 0; i < sizeof(U); ++i) {
            reinterpret_cast<char*>(&old_value)[i] =
                reinterpret_cast<const volatile char*>(ptr_)[i];
        }
        const unsigned_type next =
            static_cast<unsigned_type>(old_value) ^
            static_cast<unsigned_type>(arg);
        const U next_value = static_cast<U>(next);
        for (size_t i = 0; i < sizeof(U); ++i) {
            reinterpret_cast<volatile char*>(ptr_)[i] =
                reinterpret_cast<const char*>(&next_value)[i];
        }
        lock.unlock();
        return old_value;
    }

#if __cplusplus >= 202002L
    template<typename U = T>
    typename enable_if<is_floating_point<U>::value, U>::type
    fetch_add(U arg, memory_order order = memory_order_seq_cst) const noexcept {
        U old = load(memory_order_relaxed);
        for (;;) {
            const U desired = static_cast<U>(old + arg);
            if (compare_exchange_weak(old, desired, order,
                                      memory_order_relaxed)) {
                return old;
            }
        }
    }

    template<typename U = T>
    typename enable_if<is_floating_point<U>::value, U>::type
    fetch_sub(U arg, memory_order order = memory_order_seq_cst) const noexcept {
        U old = load(memory_order_relaxed);
        for (;;) {
            const U desired = static_cast<U>(old - arg);
            if (compare_exchange_weak(old, desired, order,
                                      memory_order_relaxed)) {
                return old;
            }
        }
    }
#endif

    /* Integral operators */
    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator++() const noexcept {
        const T one = static_cast<T>(1);
        return __detail::__atomic_wrap_add(fetch_add(one), one);
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator++(int) const noexcept { return fetch_add(static_cast<T>(1)); }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator--() const noexcept {
        const T one = static_cast<T>(1);
        return __detail::__atomic_wrap_sub(fetch_sub(one), one);
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator--(int) const noexcept { return fetch_sub(static_cast<T>(1)); }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator+=(T arg) const noexcept {
        return __detail::__atomic_wrap_add(fetch_add(arg), arg);
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator-=(T arg) const noexcept {
        return __detail::__atomic_wrap_sub(fetch_sub(arg), arg);
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator&=(T arg) const noexcept {
        return static_cast<T>(fetch_and(arg) & arg);
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator|=(T arg) const noexcept {
        return static_cast<T>(fetch_or(arg) | arg);
    }

    template<typename U = T>
    typename enable_if<is_integral<U>::value && !is_same<U, bool>::value, T>::type
    operator^=(T arg) const noexcept {
        return static_cast<T>(fetch_xor(arg) ^ arg);
    }

    void wait(T old, memory_order order = memory_order_seq_cst) const noexcept {
        __detail::__atomic_wait_until_changed(
            this, [this, old, order]() noexcept { return load(order) == old; });
    }

    void notify_one() const noexcept { __detail::__atomic_wait_notify(this, false); }
    void notify_all() const noexcept { __detail::__atomic_wait_notify(this, true); }
};
#endif

/* ═══════════════════════════════════════════════════════════════
 * Type aliases
 * ═══════════════════════════════════════════════════════════════*/

using atomic_bool = atomic<bool>;
using atomic_char = atomic<char>;
using atomic_schar = atomic<signed char>;
using atomic_uchar = atomic<unsigned char>;
using atomic_short = atomic<short>;
using atomic_ushort = atomic<unsigned short>;
using atomic_int = atomic<int>;
using atomic_uint = atomic<unsigned int>;
using atomic_long = atomic<long>;
using atomic_ulong = atomic<unsigned long>;
using atomic_llong = atomic<long long>;
using atomic_ullong = atomic<unsigned long long>;

using atomic_int8_t = atomic<int8_t>;
using atomic_uint8_t = atomic<uint8_t>;
using atomic_int16_t = atomic<int16_t>;
using atomic_uint16_t = atomic<uint16_t>;
using atomic_int32_t = atomic<int32_t>;
using atomic_uint32_t = atomic<uint32_t>;
using atomic_int64_t = atomic<int64_t>;
using atomic_uint64_t = atomic<uint64_t>;

using atomic_intptr_t = atomic<intptr_t>;
using atomic_uintptr_t = atomic<uintptr_t>;
using atomic_size_t = atomic<size_t>;
using atomic_ptrdiff_t = atomic<ptrdiff_t>;

/* ═══════════════════════════════════════════════════════════════
 * Non-member functions
 * ═══════════════════════════════════════════════════════════════*/

template<class T>
T kill_dependency(T y) noexcept {
    return y;
}

#if __cplusplus >= 202002L
/* C++20 atomic_flag free-function surface.  Keep these as thin wrappers so
 * member and free calls share the same generation owner and memory-order
 * validation path. */
inline bool atomic_flag_test(const volatile atomic_flag* object) noexcept {
    return object->test();
}

inline bool atomic_flag_test_explicit(const volatile atomic_flag* object,
                                      memory_order order) noexcept {
    return object->test(order);
}

inline void atomic_flag_clear(volatile atomic_flag* object) noexcept {
    object->clear();
}

inline void atomic_flag_clear_explicit(volatile atomic_flag* object,
                                       memory_order order) noexcept {
    object->clear(order);
}

inline bool atomic_flag_test_and_set(volatile atomic_flag* object) noexcept {
    return object->test_and_set();
}

inline bool atomic_flag_test_and_set_explicit(volatile atomic_flag* object,
                                              memory_order order) noexcept {
    return object->test_and_set(order);
}

inline void atomic_flag_wait(const volatile atomic_flag* object,
                             bool old) noexcept {
    object->wait(old);
}

inline void atomic_flag_wait_explicit(const volatile atomic_flag* object,
                                      bool old, memory_order order) noexcept {
    object->wait(old, order);
}

inline void atomic_flag_notify_one(volatile atomic_flag* object) noexcept {
    object->notify_one();
}

inline void atomic_flag_notify_all(volatile atomic_flag* object) noexcept {
    object->notify_all();
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * Free function versions of atomic operations (C++11)
 * ═══════════════════════════════════════════════════════════════*/

template<class T>
T atomic_load(const atomic<T>* obj) noexcept {
    return obj->load();
}

template<class T>
T atomic_load_explicit(const atomic<T>* obj, memory_order order) noexcept {
    return obj->load(order);
}

template<class T>
void atomic_store(atomic<T>* obj, T desr) noexcept {
    obj->store(desr);
}

template<class T>
void atomic_store_explicit(atomic<T>* obj, T desr, memory_order order) noexcept {
    obj->store(desr, order);
}

template<class T>
T atomic_exchange(atomic<T>* obj, T desr) noexcept {
    return obj->exchange(desr);
}

template<class T>
T atomic_exchange_explicit(atomic<T>* obj, T desr, memory_order order) noexcept {
    return obj->exchange(desr, order);
}

template<class T>
bool atomic_compare_exchange_weak(atomic<T>* obj, T* expected, T desired) noexcept {
    return obj->compare_exchange_weak(*expected, desired);
}

template<class T>
bool atomic_compare_exchange_strong(atomic<T>* obj, T* expected, T desired) noexcept {
    return obj->compare_exchange_strong(*expected, desired);
}

template<class T>
bool atomic_compare_exchange_weak_explicit(atomic<T>* obj, T* expected, T desired,
                                           memory_order success, memory_order failure) noexcept {
    return obj->compare_exchange_weak(*expected, desired, success, failure);
}

template<class T>
bool atomic_compare_exchange_strong_explicit(atomic<T>* obj, T* expected, T desired,
                                             memory_order success, memory_order failure) noexcept {
    return obj->compare_exchange_strong(*expected, desired, success, failure);
}

template<class T>
T atomic_fetch_add(atomic<T>* obj, T arg) noexcept {
    return obj->fetch_add(arg);
}

template<class T>
T atomic_fetch_add_explicit(atomic<T>* obj, T arg, memory_order order) noexcept {
    return obj->fetch_add(arg, order);
}

template<class T>
T atomic_fetch_sub(atomic<T>* obj, T arg) noexcept {
    return obj->fetch_sub(arg);
}

template<class T>
T atomic_fetch_sub_explicit(atomic<T>* obj, T arg, memory_order order) noexcept {
    return obj->fetch_sub(arg, order);
}

template<class T>
T atomic_fetch_and(atomic<T>* obj, T arg) noexcept {
    return obj->fetch_and(arg);
}

template<class T>
T atomic_fetch_and_explicit(atomic<T>* obj, T arg, memory_order order) noexcept {
    return obj->fetch_and(arg, order);
}

template<class T>
T atomic_fetch_or(atomic<T>* obj, T arg) noexcept {
    return obj->fetch_or(arg);
}

template<class T>
T atomic_fetch_or_explicit(atomic<T>* obj, T arg, memory_order order) noexcept {
    return obj->fetch_or(arg, order);
}

template<class T>
T atomic_fetch_xor(atomic<T>* obj, T arg) noexcept {
    return obj->fetch_xor(arg);
}

template<class T>
T atomic_fetch_xor_explicit(atomic<T>* obj, T arg, memory_order order) noexcept {
    return obj->fetch_xor(arg, order);
}

} /* namespace std */

#endif /* RINCXX_ATOMIC_H */
