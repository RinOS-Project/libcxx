/* SPDX-License-Identifier: MIT */
/*
 * RinOS C++ chrono clock adapter
 * Hosted POSIX clocks and RinOS syscalls are deliberately separate.
 */

#ifndef RINCXX_CHRONO_CLOCK_H
#define RINCXX_CHRONO_CLOCK_H

#include "rincxx.h"
#include "ctime.h"
#include "limits.h"

#if (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0) && \
    !defined(RIN_CXX_CHRONO_READ_CLOCK)
#include "../libc/sys/syscall.h"
#endif

namespace std {
namespace chrono {
namespace detail {

enum chrono_clock_kind {
    chrono_clock_realtime = 0,
    chrono_clock_monotonic = 1
};

struct chrono_clock_sample {
    long long seconds;
    long nanoseconds;
};

inline bool chrono_platform_clock_read(chrono_clock_kind kind,
                                       chrono_clock_sample& sample) noexcept
{
#if defined(RIN_CXX_CHRONO_READ_CLOCK)
    long long seconds = 0;
    long nanoseconds = 0;
    if (RIN_CXX_CHRONO_READ_CLOCK(static_cast<int>(kind),
                                  &seconds, &nanoseconds) != 0) {
        return false;
    }
    sample.seconds = seconds;
    sample.nanoseconds = nanoseconds;
    return true;
#elif defined(__STDC_HOSTED__) && __STDC_HOSTED__ != 0
    struct timespec value = {};
    const clockid_t clock_id = kind == chrono_clock_monotonic
        ? CLOCK_MONOTONIC : CLOCK_REALTIME;
    if (::clock_gettime(clock_id, &value) != 0) return false;
    sample.seconds = static_cast<long long>(value.tv_sec);
    sample.nanoseconds = value.tv_nsec;
    return true;
#else
    if (kind == chrono_clock_monotonic) {
        unsigned long long value = 0;
#if defined(__x86_64__) || defined(_M_X64)
        const intptr_t result = _syscall1((uintptr_t)SYS_TIME_NS,
                                          (uintptr_t)0);
        if (result < 0) return false;
        value = static_cast<unsigned long long>(result);
#else
        const intptr_t result = _syscall1(
            (uintptr_t)SYS_TIME_NS, reinterpret_cast<uintptr_t>(&value));
        if (result != 0) return false;
#endif
        constexpr unsigned long long billion = 1000000000ULL;
        const unsigned long long seconds = value / billion;
        if (seconds > static_cast<unsigned long long>(
                numeric_limits<long long>::max())) {
            return false;
        }
        sample.seconds = static_cast<long long>(seconds);
        sample.nanoseconds = static_cast<long>(value % billion);
        return true;
    }

    /* The kernel exposes the legacy gettimeofday carrier with native word
     * fields: two 32-bit words in the IA-32 dispatcher and two 64-bit words
     * in the x86_64 dispatcher.  Do not spell this as `long`: Win64/LLP64
     * freestanding builds have a 32-bit long even though their syscall ABI is
     * 64-bit, and the kernel would otherwise overwrite a short buffer. */
#if defined(__x86_64__) || defined(_M_X64)
    struct rin_timeval_wire {
        unsigned long long seconds;
        unsigned long long microseconds;
    } value = {};
    static_assert(sizeof(rin_timeval_wire) == 16,
                  "x86_64 gettimeofday wire must be 16 bytes");
#else
    struct rin_timeval_wire {
        unsigned long seconds;
        unsigned long microseconds;
    } value = {};
    static_assert(sizeof(rin_timeval_wire) == 8,
                  "i686 gettimeofday wire must be 8 bytes");
#endif
    const intptr_t result = _syscall2(
        (uintptr_t)SYS_GETTIMEOFDAY, reinterpret_cast<uintptr_t>(&value),
        (uintptr_t)0);
    if (result != 0) return false;
    if (value.seconds > static_cast<unsigned long long>(
            numeric_limits<long long>::max()) ||
        value.microseconds >= 1000000ULL)
        return false;
    sample.seconds = static_cast<long long>(value.seconds);
    sample.nanoseconds = static_cast<long>(value.microseconds * 1000ULL);
    return true;
#endif
}

inline bool chrono_sample_to_nanoseconds(const chrono_clock_sample& sample,
                                         long long& result) noexcept
{
    constexpr long long billion = 1000000000LL;
    constexpr long long maximum = numeric_limits<long long>::max();
    constexpr long long minimum = numeric_limits<long long>::min();

    if (sample.nanoseconds < 0 || sample.nanoseconds >= billion)
        return false;
    if (sample.seconds > maximum / billion) {
        result = maximum;
        return true;
    }
    if (sample.seconds < minimum / billion) {
        result = minimum;
        return true;
    }

    const long long base = sample.seconds * billion;
    const long long fraction = static_cast<long long>(sample.nanoseconds);
    if (base > maximum - fraction) {
        result = maximum;
        return true;
    }
    result = base + fraction;
    return true;
}

struct alignas(8) chrono_clock_storage {
    long long value;
};

inline chrono_clock_storage& chrono_steady_last_storage() noexcept
{
    static chrono_clock_storage value = {0};
    return value;
}

inline chrono_clock_storage& chrono_realtime_last_storage() noexcept
{
    static chrono_clock_storage value = {0};
    return value;
}

inline long long chrono_steady_now_nanoseconds() noexcept
{
    chrono_clock_sample sample = {};
    long long candidate = 0;
    chrono_clock_storage& storage = chrono_steady_last_storage();
    long long observed = __atomic_load_n(&storage.value, __ATOMIC_ACQUIRE);
    if (!chrono_platform_clock_read(chrono_clock_monotonic, sample) ||
        sample.seconds < 0 ||
        !chrono_sample_to_nanoseconds(sample, candidate)) {
        return observed;
    }

    while (candidate > observed) {
        if (__atomic_compare_exchange_n(&storage.value, &observed, candidate,
                                        false, __ATOMIC_RELEASE,
                                        __ATOMIC_ACQUIRE)) {
            return candidate;
        }
    }
    return observed;
}

inline long long chrono_realtime_now_nanoseconds() noexcept
{
    chrono_clock_sample sample = {};
    long long candidate = 0;
    chrono_clock_storage& storage = chrono_realtime_last_storage();
    if (!chrono_platform_clock_read(chrono_clock_realtime, sample) ||
        !chrono_sample_to_nanoseconds(sample, candidate)) {
        return __atomic_load_n(&storage.value, __ATOMIC_ACQUIRE);
    }
    __atomic_store_n(&storage.value, candidate, __ATOMIC_RELEASE);
    return candidate;
}

} /* namespace detail */
} /* namespace chrono */
} /* namespace std */

#endif /* RINCXX_CHRONO_CLOCK_H */
