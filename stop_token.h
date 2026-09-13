/*
 * RinOS C++ <stop_token> ✿
 * 協調的スレッドキャンセル (C++20)
 */

#ifndef RINCXX_STOP_TOKEN_H
#define RINCXX_STOP_TOKEN_H

#include "rincxx.h"
#include "version.h"

/* stop tokens and their pthread-backed state are C++20-only. */
#if defined(__cplusplus) && __cplusplus >= 202002L

#include "atomic.h"
#include "memory.h"
#include "functional.h"
#include "type_traits.h"

#include "__pthread.h"

namespace std {

/* 前方宣言 */
class stop_source;
class stop_token;
template<typename Callback> class stop_callback;

/* ═══════════════════════════════════════════════════════════════
 * 内部: 停止状態の共有
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

struct stop_callback_base {
    stop_callback_base* next_ = nullptr;
    stop_callback_base* prev_ = nullptr;
    void (*execute_)(stop_callback_base*) noexcept = nullptr;
    bool registered_ = false;
};

struct stop_state {
    atomic<size_t> ref_count_{1};
    atomic<size_t> source_count_{1};
    atomic<bool> stop_requested_{false};
    stop_callback_base* callbacks_ = nullptr;
    stop_callback_base* executing_callback_ = nullptr;
    pthread_t executing_thread_{};
    unsigned char lock_ = 0;

    void lock() noexcept {
        while (__atomic_test_and_set(&lock_, __ATOMIC_ACQUIRE)) {
#if defined(__i386__) || defined(__x86_64__)
            __asm__ volatile("pause" ::: "memory");
#endif
        }
    }

    void unlock() noexcept {
        __atomic_clear(&lock_, __ATOMIC_RELEASE);
    }

    stop_callback_base* executing_callback() const noexcept {
        return __atomic_load_n(&executing_callback_, __ATOMIC_ACQUIRE);
    }

    void set_executing_callback(stop_callback_base* callback) noexcept {
        __atomic_store_n(&executing_callback_, callback, __ATOMIC_RELEASE);
    }

    void add_ref() noexcept {
        ref_count_.fetch_add(1, memory_order_relaxed);
    }

    void release() noexcept {
        if (ref_count_.fetch_sub(1, memory_order_acq_rel) == 1) {
            delete this;
        }
    }

    void add_source() noexcept {
        add_ref();
        source_count_.fetch_add(1, memory_order_relaxed);
    }

    void release_source() noexcept {
        source_count_.fetch_sub(1, memory_order_release);
        release();
    }

    bool stop_possible() const noexcept {
        return stop_requested_.load(memory_order_acquire) ||
               source_count_.load(memory_order_acquire) != 0;
    }

    bool request_stop() noexcept {
        /* Callback code may release every external owner, including itself. */
        add_ref();
        bool expected = false;
        if (!stop_requested_.compare_exchange_strong(expected, true,
                memory_order_acq_rel, memory_order_relaxed)) {
            release();
            return false;
        }

        /*
         * Detach one callback at a time.  The request thread never touches a
         * callback object after invoking it: a callback is therefore allowed
         * to destroy its own stop_callback without leaving a dangling access.
         */
        for (;;) {
            lock();
            stop_callback_base* cb = callbacks_;
            if (!cb) {
                unlock();
                break;
            }
            callbacks_ = cb->next_;
            if (callbacks_)
                callbacks_->prev_ = nullptr;
            cb->next_ = nullptr;
            cb->prev_ = nullptr;
            cb->registered_ = false;
            executing_thread_ = pthread_self();
            set_executing_callback(cb);
            void (*execute)(stop_callback_base*) noexcept = cb->execute_;
            unlock();

            execute(cb);

            /* Do not dereference cb here; it may have destroyed itself. */
            set_executing_callback(nullptr);
        }
        release();
        return true;
    }

    bool register_callback(stop_callback_base* cb) noexcept {
        lock();
        if (stop_requested_.load(memory_order_acquire)) {
            unlock();
            /* A callback registered after the stop request runs synchronously. */
            cb->execute_(cb);
            return false;
        }
        if (source_count_.load(memory_order_acquire) != 0) {
            cb->next_ = callbacks_;
            cb->prev_ = nullptr;
            if (callbacks_)
                callbacks_->prev_ = cb;
            callbacks_ = cb;
            cb->registered_ = true;
            unlock();
            return true;
        }
        unlock();
        return false;
    }

    void unregister_callback(stop_callback_base* cb) noexcept {
        bool wait_for_execution = false;

        lock();
        if (cb->registered_) {
            if (cb->prev_)
                cb->prev_->next_ = cb->next_;
            else
                callbacks_ = cb->next_;
            if (cb->next_)
                cb->next_->prev_ = cb->prev_;
            cb->registered_ = false;
            cb->next_ = nullptr;
            cb->prev_ = nullptr;
        } else if (executing_callback() == cb) {
            /* The invoking thread must be able to destroy its own callback. */
            wait_for_execution = !pthread_equal(executing_thread_, pthread_self());
        }
        unlock();

        while (wait_for_execution &&
               executing_callback() == cb) {
            rin_cxx_scheduler_yield();
        }
    }

    ~stop_state() {
        /* A state cannot die while a source, token, or callback owns it. */
        if (callbacks_ || executing_callback()) {
            __builtin_trap();
        }
    }
};

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * nostopstate_t
 * ═══════════════════════════════════════════════════════════════*/

struct nostopstate_t {
    explicit nostopstate_t() = default;
};

inline constexpr nostopstate_t nostopstate{};

/* ═══════════════════════════════════════════════════════════════
 * stop_token
 * ═══════════════════════════════════════════════════════════════*/

class stop_token {
    detail::stop_state* state_ = nullptr;

    friend class stop_source;
    template<typename Callback> friend class stop_callback;

    explicit stop_token(detail::stop_state* state) noexcept : state_(state) {
        if (state_) {
            state_->add_ref();
        }
    }

public:
    /* コンストラクタ */
    stop_token() noexcept = default;

    /* コピー */
    stop_token(const stop_token& other) noexcept : state_(other.state_) {
        if (state_) {
            state_->add_ref();
        }
    }

    stop_token& operator=(const stop_token& other) noexcept {
        if (this != &other) {
            if (state_) {
                state_->release();
            }
            state_ = other.state_;
            if (state_) {
                state_->add_ref();
            }
        }
        return *this;
    }

    /* ムーブ */
    stop_token(stop_token&& other) noexcept : state_(other.state_) {
        other.state_ = nullptr;
    }

    stop_token& operator=(stop_token&& other) noexcept {
        if (this != &other) {
            if (state_) {
                state_->release();
            }
            state_ = other.state_;
            other.state_ = nullptr;
        }
        return *this;
    }

    /* デストラクタ */
    ~stop_token() {
        if (state_) {
            state_->release();
        }
    }

    /* swap */
    void swap(stop_token& other) noexcept {
        detail::stop_state* tmp = state_;
        state_ = other.state_;
        other.state_ = tmp;
    }

    /* 停止要求されたか */
    [[nodiscard]] bool stop_requested() const noexcept {
        return state_ && state_->stop_requested_.load(memory_order_acquire);
    }

    /* 停止可能か */
    [[nodiscard]] bool stop_possible() const noexcept {
        return state_ && state_->stop_possible();
    }

    /* 比較 */
    friend bool operator==(const stop_token& lhs, const stop_token& rhs) noexcept {
        return lhs.state_ == rhs.state_;
    }

    friend bool operator!=(const stop_token& lhs, const stop_token& rhs) noexcept {
        return lhs.state_ != rhs.state_;
    }
};

inline void swap(stop_token& lhs, stop_token& rhs) noexcept {
    lhs.swap(rhs);
}

/* ═══════════════════════════════════════════════════════════════
 * stop_source
 * ═══════════════════════════════════════════════════════════════*/

class stop_source {
    detail::stop_state* state_;

public:
    /* コンストラクタ */
    stop_source() : state_(new detail::stop_state()) {}

    explicit stop_source(nostopstate_t) noexcept : state_(nullptr) {}

    /* コピー */
    stop_source(const stop_source& other) noexcept : state_(other.state_) {
        if (state_)
            state_->add_source();
    }

    stop_source& operator=(const stop_source& other) noexcept {
        if (this != &other) {
            detail::stop_state* replacement = other.state_;
            if (replacement)
                replacement->add_source();
            if (state_)
                state_->release_source();
            state_ = replacement;
        }
        return *this;
    }

    /* ムーブ */
    stop_source(stop_source&& other) noexcept : state_(other.state_) {
        other.state_ = nullptr;
    }

    stop_source& operator=(stop_source&& other) noexcept {
        if (this != &other) {
            if (state_)
                state_->release_source();
            state_ = other.state_;
            other.state_ = nullptr;
        }
        return *this;
    }

    /* デストラクタ */
    ~stop_source() {
        if (state_)
            state_->release_source();
    }

    /* swap */
    void swap(stop_source& other) noexcept {
        detail::stop_state* tmp = state_;
        state_ = other.state_;
        other.state_ = tmp;
    }

    /* トークン取得 */
    [[nodiscard]] stop_token get_token() const noexcept {
        return stop_token(state_);
    }

    /* 停止要求 */
    bool request_stop() const noexcept {
        return state_ && state_->request_stop();
    }

    /* 停止要求されたか */
    [[nodiscard]] bool stop_requested() const noexcept {
        return state_ && state_->stop_requested_.load(memory_order_acquire);
    }

    /* 停止可能か */
    [[nodiscard]] bool stop_possible() const noexcept {
        return state_ && state_->stop_possible();
    }

    /* 比較 */
    friend bool operator==(const stop_source& lhs, const stop_source& rhs) noexcept {
        return lhs.state_ == rhs.state_;
    }

    friend bool operator!=(const stop_source& lhs, const stop_source& rhs) noexcept {
        return lhs.state_ != rhs.state_;
    }
};

inline void swap(stop_source& lhs, stop_source& rhs) noexcept {
    lhs.swap(rhs);
}

/* ═══════════════════════════════════════════════════════════════
 * stop_callback
 * ═══════════════════════════════════════════════════════════════*/

template<typename Callback>
class [[nodiscard]] stop_callback : private detail::stop_callback_base {
    static_assert(is_nothrow_destructible_v<Callback>,
                  "stop_callback callback type must be nothrow destructible");
    static_assert(is_invocable_v<Callback>,
                  "stop_callback callback type must be invocable without arguments");

    Callback callback_;
    detail::stop_state* state_ = nullptr;

    static void execute_impl(detail::stop_callback_base* base) noexcept {
        stop_callback* self = static_cast<stop_callback*>(base);
        self->callback_();
    }

public:
    using callback_type = Callback;

    /* コンストラクタ */
    template<typename C,
             enable_if_t<is_constructible_v<Callback, C>, int> = 0>
    explicit stop_callback(const stop_token& token, C&& cb)
        noexcept(is_nothrow_constructible_v<Callback, C>)
        : callback_(std::forward<C>(cb)), state_(token.state_) {
        execute_ = &execute_impl;
        if (state_) {
            state_->add_ref();
            if (!state_->register_callback(this)) {
                state_->release();
                state_ = nullptr;
            }
        }
    }

    template<typename C,
             enable_if_t<is_constructible_v<Callback, C>, int> = 0>
    explicit stop_callback(stop_token&& token, C&& cb)
        noexcept(is_nothrow_constructible_v<Callback, C>)
        : callback_(std::forward<C>(cb)), state_(token.state_) {
        execute_ = &execute_impl;
        if (state_) {
            if (state_->register_callback(this)) {
                token.state_ = nullptr;
            } else {
                state_ = nullptr;
            }
        }
    }

    /* コピー・ムーブ禁止 */
    stop_callback(const stop_callback&) = delete;
    stop_callback& operator=(const stop_callback&) = delete;
    stop_callback(stop_callback&&) = delete;
    stop_callback& operator=(stop_callback&&) = delete;

    /* デストラクタ */
    ~stop_callback() {
        if (state_) {
            state_->unregister_callback(this);
            state_->release();
        }
    }
};

/* 推論ガイド */
template<typename Callback>
stop_callback(stop_token, Callback) -> stop_callback<Callback>;

} /* namespace std */

#endif /* defined(__cplusplus) && __cplusplus >= 202002L */
#endif /* RINCXX_STOP_TOKEN_H */
