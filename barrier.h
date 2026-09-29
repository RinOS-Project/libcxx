/*
 * RinOS C++ <barrier> ✿
 * バリア (C++20)
 */

#ifndef RINCXX_BARRIER_H
#define RINCXX_BARRIER_H

#include "rincxx.h"
#include "version.h"

/* std::barrier is a C++20 synchronization facility.  Its Rin futex
 * implementation must not make a direct include ill-formed for old modes. */
#if defined(__cplusplus) && __cplusplus >= 202002L

#include "atomic.h"
#include "functional.h"
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && \
    __STDC_HOSTED__ && defined(SYS_OPEN)
#pragma push_macro("SYS_OPEN")
#undef SYS_OPEN
#define RINCXX_BARRIER_RESTORE_HOST_SYS_OPEN 1
#endif
#include "../libc/sys/syscall.h"
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && __STDC_HOSTED__
#ifdef RINCXX_BARRIER_RESTORE_HOST_SYS_OPEN
#undef SYS_OPEN
#pragma pop_macro("SYS_OPEN")
#undef RINCXX_BARRIER_RESTORE_HOST_SYS_OPEN
#else
/* Do not leak the Rin target-only uppercase alias into a hosted CRT. */
#undef SYS_OPEN
#endif
#endif
#include "../libc/linux/futex.h"

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * barrier
 * 再利用可能な同期プリミティブ
 * ═══════════════════════════════════════════════════════════════*/

/* デフォルトの完了関数 */
struct __barrier_empty_completion {
    void operator()() const noexcept {}
};

template<typename CompletionFunction = __barrier_empty_completion>
class barrier {
    static_assert(sizeof(unsigned int) == 4,
                  "RinOS futex words must be 32 bits");
    static_assert(is_nothrow_invocable<CompletionFunction&>::value,
                  "barrier completion function must be nothrow-invocable");

    atomic<ptrdiff_t> expected_;
    atomic<ptrdiff_t> remaining_;
    atomic<unsigned long long> phase_;
    alignas(int) mutable unsigned int wake_word_;
    CompletionFunction completion_;

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

    void complete_phase() noexcept {
        completion_();
        remaining_.store(expected_.load(memory_order_acquire),
                         memory_order_relaxed);
        phase_.fetch_add(1, memory_order_release);
        publish_wake();
    }

public:
    class arrival_token {
        friend class barrier;

        unsigned long long phase_;

        explicit constexpr arrival_token(unsigned long long phase) noexcept
            : phase_(phase) {}

    public:
        arrival_token(arrival_token&&) noexcept = default;
        arrival_token& operator=(arrival_token&&) noexcept = default;
        arrival_token(const arrival_token&) = delete;
        arrival_token& operator=(const arrival_token&) = delete;
        ~arrival_token() = default;
    };

    static constexpr ptrdiff_t max() noexcept {
        return static_cast<ptrdiff_t>(__PTRDIFF_MAX__);
    }

    /* コンストラクタ */
    explicit barrier(ptrdiff_t expected, CompletionFunction f = CompletionFunction())
        : expected_(expected), remaining_(expected), phase_(0), wake_word_(0),
          completion_(std::move(f)) {}

    /* デストラクタ */
    ~barrier() = default;

    /* コピー・ムーブ禁止 */
    barrier(const barrier&) = delete;
    barrier& operator=(const barrier&) = delete;

    /* arrive - 到着を報告してトークンを返す */
    [[nodiscard]]
    arrival_token arrive(ptrdiff_t n = 1) noexcept {
        const unsigned long long current_phase =
            phase_.load(memory_order_acquire);
        const ptrdiff_t old_remaining =
            remaining_.fetch_sub(n, memory_order_acq_rel);

        if (old_remaining == n)
            complete_phase();

        return arrival_token(current_phase);
    }

    /* wait - 指定フェーズが完了するまで待機 (futexブロッキング) */
    void wait(arrival_token&& token) const noexcept {
        while (phase_.load(memory_order_acquire) == token.phase_) {
            const unsigned int generation =
                __atomic_load_n(&wake_word_, __ATOMIC_ACQUIRE);
            if (phase_.load(memory_order_acquire) == token.phase_) {
                if (generation == ~0U) {
                    (void)syscall(SYS_sched_yield);
                } else {
                    syscall(SYS_futex, wake_address(), FUTEX_WAIT_PRIVATE,
                            generation, NULL, NULL, 0);
                }
            }
        }
    }

    /* arrive_and_wait - 到着して待機 */
    void arrive_and_wait() noexcept {
        wait(arrive());
    }

    /* arrive_and_drop - 参加者数を減らして到着 */
    void arrive_and_drop() noexcept {
        expected_.fetch_sub(1, memory_order_acq_rel);
        (void)arrive();
    }
};

} /* namespace std */

#endif /* defined(__cplusplus) && __cplusplus >= 202002L */
#endif /* RINCXX_BARRIER_H */
