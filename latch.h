/*
 * RinOS C++ <latch> ✿
 * ラッチ (C++20)
 */

#ifndef RINCXX_LATCH_H
#define RINCXX_LATCH_H

#include "rincxx.h"
#include "version.h"

#if __cplusplus >= 202002L
#include "atomic.h"
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && \
    __STDC_HOSTED__ && defined(SYS_OPEN)
#pragma push_macro("SYS_OPEN")
#undef SYS_OPEN
#define RINCXX_LATCH_RESTORE_HOST_SYS_OPEN 1
#endif
#include "../libc/sys/syscall.h"
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && __STDC_HOSTED__
#ifdef RINCXX_LATCH_RESTORE_HOST_SYS_OPEN
#undef SYS_OPEN
#pragma pop_macro("SYS_OPEN")
#undef RINCXX_LATCH_RESTORE_HOST_SYS_OPEN
#else
/* Do not leak the Rin target-only uppercase alias into a hosted CRT. */
#undef SYS_OPEN
#endif
#endif
#include "../libc/linux/futex.h"

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * latch
 * 単回使用の同期プリミティブ
 * ═══════════════════════════════════════════════════════════════*/

class latch {
    static_assert(sizeof(int) == 4, "RinOS futex words must be 32 bits");

    atomic<ptrdiff_t> counter_;
    alignas(int) mutable int generation_;

    int* generation_address() const noexcept {
        return const_cast<int*>(&generation_);
    }

public:
    static constexpr ptrdiff_t max() noexcept {
        return static_cast<ptrdiff_t>(__PTRDIFF_MAX__);
    }

    /* コンストラクタ */
    constexpr explicit latch(ptrdiff_t expected) noexcept
        : counter_(expected), generation_(0) {}

    /* デストラクタ */
    ~latch() = default;

    /* コピー・ムーブ禁止 */
    latch(const latch&) = delete;
    latch& operator=(const latch&) = delete;

    /* count_down - カウントを減らす (futex WAKE) */
    void count_down(ptrdiff_t n = 1) noexcept {
        if (counter_.fetch_sub(n, memory_order_release) == n) {
            __atomic_add_fetch(&generation_, 1, __ATOMIC_RELEASE);
            syscall(SYS_futex, generation_address(), FUTEX_WAKE,
                    0x7fffffff, NULL, NULL, 0);
        }
    }

    /* try_wait - ゼロかどうかテスト */
    bool try_wait() const noexcept {
        return counter_.load(memory_order_acquire) == 0;
    }

    /* wait - ゼロになるまで待機 (futexブロッキング) */
    void wait() const noexcept {
        for (;;) {
            if (counter_.load(memory_order_acquire) == 0)
                return;
            const int generation =
                __atomic_load_n(&generation_, __ATOMIC_ACQUIRE);
            if (counter_.load(memory_order_acquire) == 0)
                return;
            syscall(SYS_futex, generation_address(), FUTEX_WAIT,
                    generation, NULL, NULL, 0);
        }
    }

    /* count_down_and_wait - カウントダウンして待機 */
    void count_down_and_wait(ptrdiff_t n = 1) noexcept {
        count_down(n);
        wait();
    }

    /* Keep the historical RinOS spelling as a compatibility alias. */
    void arrive_and_wait(ptrdiff_t n = 1) noexcept {
        count_down_and_wait(n);
    }
};

} /* namespace std */

#endif /* __cplusplus >= 202002L */

#endif /* RINCXX_LATCH_H */
