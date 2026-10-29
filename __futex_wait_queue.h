/*
 * RinOS libc++ internal per-waiter futex queue.
 *
 * The one-shot waiter word never carries a reusable sequence number, so a
 * delayed waiter cannot mistake a later wake generation for its original
 * one after 32-bit rollover.
 */
#ifndef RINCXX___FUTEX_WAIT_QUEUE_H
#define RINCXX___FUTEX_WAIT_QUEUE_H

namespace std {
namespace detail {

struct __futex_waiter {
    alignas(int) unsigned int notified;
    __futex_waiter* previous;
    __futex_waiter* next;
    bool linked;

    __futex_waiter() noexcept
        : notified(0), previous(nullptr), next(nullptr), linked(false) {}
};

class __futex_wait_queue {
    /* 0 = unlocked, 1 = held without known contention, 2 = contended. */
    alignas(int) unsigned int lock_word_;
    __futex_waiter* head_;
    __futex_waiter* tail_;

    void unlink_locked(__futex_waiter& waiter) noexcept {
        if (waiter.previous)
            waiter.previous->next = waiter.next;
        else
            head_ = waiter.next;
        if (waiter.next)
            waiter.next->previous = waiter.previous;
        else
            tail_ = waiter.previous;
        waiter.previous = nullptr;
        waiter.next = nullptr;
        waiter.linked = false;
    }

public:
    constexpr __futex_wait_queue() noexcept
        : lock_word_(0), head_(nullptr), tail_(nullptr) {}

    __futex_wait_queue(const __futex_wait_queue&) = delete;
    __futex_wait_queue& operator=(const __futex_wait_queue&) = delete;

    void lock() noexcept {
        unsigned int expected = 0;
        if (__atomic_compare_exchange_n(&lock_word_, &expected, 1, false,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            return;
        for (;;) {
            if (__atomic_exchange_n(&lock_word_, 2, __ATOMIC_ACQUIRE) == 0)
                return;
            (void)syscall(SYS_futex, &lock_word_, FUTEX_WAIT_PRIVATE,
                          2, nullptr, nullptr, 0);
        }
    }

    void unlock() noexcept {
        if (__atomic_exchange_n(&lock_word_, 0, __ATOMIC_RELEASE) == 2)
            (void)syscall(SYS_futex, &lock_word_, FUTEX_WAKE_PRIVATE,
                          1, nullptr, nullptr, 0);
    }

    void enqueue_locked(__futex_waiter& waiter) noexcept {
        if (waiter.linked) return;
        __atomic_store_n(&waiter.notified, 0, __ATOMIC_RELAXED);
        waiter.previous = tail_;
        waiter.next = nullptr;
        waiter.linked = true;
        if (tail_)
            tail_->next = &waiter;
        else
            head_ = &waiter;
        tail_ = &waiter;
    }

    bool remove_locked(__futex_waiter& waiter) noexcept {
        if (!waiter.linked) return false;
        unlink_locked(waiter);
        return true;
    }

    void notify_one_locked() noexcept {
        if (!head_) return;
        __futex_waiter& waiter = *head_;
        unlink_locked(waiter);
        __atomic_store_n(&waiter.notified, 1, __ATOMIC_RELEASE);
        /* Keep the queue lock until WAKE returns. A waiter acknowledges the
         * notification by reacquiring this lock before its stack node dies. */
        (void)syscall(SYS_futex, &waiter.notified, FUTEX_WAKE_PRIVATE,
                      1, nullptr, nullptr, 0);
    }

    void notify_n_locked(ptrdiff_t count) noexcept {
        while (count > 0 && head_) {
            notify_one_locked();
            --count;
        }
    }

    void notify_all_locked() noexcept {
        while (head_)
            notify_one_locked();
    }

    void retire(__futex_waiter& waiter) noexcept {
        lock();
        remove_locked(waiter);
        unlock();
    }
};

class __futex_wait_registration {
    __futex_wait_queue* queue_;
    __futex_waiter waiter_;

public:
    explicit __futex_wait_registration(__futex_wait_queue& queue) noexcept
        : queue_(&queue), waiter_() {}

    __futex_wait_registration(const __futex_wait_registration&) = delete;
    __futex_wait_registration& operator=(const __futex_wait_registration&) = delete;

    ~__futex_wait_registration() {
        queue_->retire(waiter_);
    }

    __futex_waiter& waiter() noexcept { return waiter_; }
};

} /* namespace detail */
} /* namespace std */

#endif /* RINCXX___FUTEX_WAIT_QUEUE_H */
