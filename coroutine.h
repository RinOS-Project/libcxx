/*
 * RinOS C++ <coroutine> ✿
 * コルーチンサポート (C++20)
 *
 * 注: コルーチンはコンパイラの組み込みサポートが必要
 *     GCC: -fcoroutines フラグが必要
 *     Clang: -fcoroutines-ts または -std=c++20
 */

#ifndef RINCXX_COROUTINE_H
#define RINCXX_COROUTINE_H

#include "rincxx.h"
#include "type_traits.h"
#include "functional.h"
#include "exception.h"
#include "pointer_order.h"

#define RIN_HAS_COROUTINES 0

#if __cplusplus >= 202002L
/* コルーチンサポートの検出 */
#undef RIN_HAS_COROUTINES
#if defined(__cpp_impl_coroutine) && __cpp_impl_coroutine >= 201902L
    #define RIN_HAS_COROUTINES 1
#elif defined(__cpp_coroutines) && __cpp_coroutines >= 201703L
    #define RIN_HAS_COROUTINES 1
#elif defined(__GNUC__) && defined(__cpp_impl_coroutine)
    #define RIN_HAS_COROUTINES 1
#else
    #define RIN_HAS_COROUTINES 0
#endif

namespace std {

namespace detail {
void* noop_frame_address() noexcept;
bool is_noop_frame_address(void* address) noexcept;

template<typename Result, typename = void>
struct coroutine_traits_base {};

template<typename Result>
struct coroutine_traits_base<Result, void_t<typename Result::promise_type>> {
    using promise_type = typename Result::promise_type;
};
} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * coroutine_traits
 * コルーチンの戻り値型からpromise_typeを推論
 * ═══════════════════════════════════════════════════════════════*/

template<typename R, typename... Args>
struct coroutine_traits : detail::coroutine_traits_base<R> {};

/* ═══════════════════════════════════════════════════════════════
 * coroutine_handle
 * コルーチンへの型消去されたハンドル
 * ═══════════════════════════════════════════════════════════════*/

template<typename Promise = void>
struct coroutine_handle;

/* void特殊化 - 基底クラス */
template<>
struct coroutine_handle<void> {
    /* コンストラクタ */
    constexpr coroutine_handle() noexcept : handle_(nullptr) {}
    constexpr coroutine_handle(nullptr_t) noexcept : handle_(nullptr) {}

    /* 代入 */
    coroutine_handle& operator=(nullptr_t) noexcept {
        handle_ = nullptr;
        return *this;
    }

    /* アドレス取得 */
    constexpr void* address() const noexcept {
        return handle_;
    }

    /* アドレスからの復元 */
    static constexpr coroutine_handle from_address(void* addr) noexcept {
        coroutine_handle h;
        h.handle_ = addr;
        return h;
    }

    /* 有効性チェック */
    constexpr explicit operator bool() const noexcept {
        return handle_ != nullptr;
    }

    /* 完了チェック */
    bool done() const noexcept {
        if (detail::is_noop_frame_address(handle_)) return false;
#if RIN_HAS_COROUTINES
        return __builtin_coro_done(handle_);
#else
        return true;
#endif
    }

    /* 再開 */
    void resume() const {
        if (detail::is_noop_frame_address(handle_)) return;
#if RIN_HAS_COROUTINES
        __builtin_coro_resume(handle_);
#endif
    }

    void operator()() const {
        resume();
    }

    /* 破棄 */
    void destroy() const {
        if (detail::is_noop_frame_address(handle_)) return;
#if RIN_HAS_COROUTINES
        __builtin_coro_destroy(handle_);
#endif
    }

    /* 比較 */
    friend bool operator==(coroutine_handle a, coroutine_handle b) noexcept {
        return a.handle_ == b.handle_;
    }

    friend bool operator!=(coroutine_handle a, coroutine_handle b) noexcept {
        return a.handle_ != b.handle_;
    }

    friend bool operator<(coroutine_handle a, coroutine_handle b) noexcept {
        return detail::object_pointer_total_less(a.handle_, b.handle_);
    }

    friend bool operator>(coroutine_handle a, coroutine_handle b) noexcept {
        return detail::object_pointer_total_less(b.handle_, a.handle_);
    }

    friend bool operator<=(coroutine_handle a, coroutine_handle b) noexcept {
        return !detail::object_pointer_total_less(b.handle_, a.handle_);
    }

    friend bool operator>=(coroutine_handle a, coroutine_handle b) noexcept {
        return !detail::object_pointer_total_less(a.handle_, b.handle_);
    }

protected:
    void* handle_;
};

/* Promise型を持つ特殊化 */
template<typename Promise>
struct coroutine_handle : coroutine_handle<void> {
    using coroutine_handle<void>::coroutine_handle;

    /* promise からの変換 */
    static coroutine_handle from_promise(Promise& promise) noexcept {
        coroutine_handle h;
#if RIN_HAS_COROUTINES
        h.handle_ = __builtin_coro_promise(&promise, alignof(Promise), true);
#else
        (void)promise;
#endif
        return h;
    }

    /* promise へのアクセス */
    Promise& promise() const {
#if RIN_HAS_COROUTINES
        return *static_cast<Promise*>(
            __builtin_coro_promise(handle_, alignof(Promise), false));
#else
        /* ダミー - 実際には使用不可 */
        return *static_cast<Promise*>(handle_);
#endif
    }

    /* アドレスからの復元 */
    static constexpr coroutine_handle from_address(void* addr) noexcept {
        coroutine_handle h;
        h.handle_ = addr;
        return h;
    }
};

/* ═══════════════════════════════════════════════════════════════
 * noop_coroutine
 * 何もしないコルーチン (C++20)
 * ═══════════════════════════════════════════════════════════════*/

struct noop_coroutine_promise {};

template<>
struct coroutine_handle<noop_coroutine_promise> : coroutine_handle<void> {
    using promise_type = noop_coroutine_promise;

    constexpr explicit operator bool() const noexcept { return true; }
    constexpr bool done() const noexcept { return false; }

    constexpr void resume() const noexcept {}
    constexpr void destroy() const noexcept {}
    constexpr void operator()() const noexcept {}

    noop_coroutine_promise& promise() const noexcept {
#if RIN_HAS_COROUTINES && defined(__clang__)
        return *static_cast<noop_coroutine_promise*>(
            __builtin_coro_promise(handle_, alignof(noop_coroutine_promise),
                                   false));
#else
        static noop_coroutine_promise promise;
        return promise;
#endif
    }

    constexpr void* address() const noexcept {
        /* 実装定義の静的アドレス */
        return handle_;
    }

private:
    friend coroutine_handle<noop_coroutine_promise> noop_coroutine() noexcept;

    coroutine_handle() noexcept {
        handle_ = detail::noop_frame_address();
    }
};

using noop_coroutine_handle = coroutine_handle<noop_coroutine_promise>;

inline noop_coroutine_handle noop_coroutine() noexcept {
    return noop_coroutine_handle();
}

/* ═══════════════════════════════════════════════════════════════
 * suspend_never
 * 決して中断しないアウェイター
 * ═══════════════════════════════════════════════════════════════*/

struct suspend_never {
    constexpr bool await_ready() const noexcept { return true; }
    constexpr void await_suspend(coroutine_handle<>) const noexcept {}
    constexpr void await_resume() const noexcept {}
};

/* ═══════════════════════════════════════════════════════════════
 * suspend_always
 * 常に中断するアウェイター
 * ═══════════════════════════════════════════════════════════════*/

struct suspend_always {
    constexpr bool await_ready() const noexcept { return false; }
    constexpr void await_suspend(coroutine_handle<>) const noexcept {}
    constexpr void await_resume() const noexcept {}
};

namespace detail {

inline void*& noop_frame_slot() noexcept {
    static void* address = nullptr;
    return address;
}

inline volatile int& noop_frame_initializing() noexcept {
    static volatile int initializing = 0;
    return initializing;
}

inline bool is_noop_frame_address(void* address) noexcept {
    return address &&
           __atomic_load_n(&noop_frame_slot(), __ATOMIC_ACQUIRE) == address;
}

#if RIN_HAS_COROUTINES && !defined(__clang__)
struct portable_noop_frame {
    struct promise_type;
    coroutine_handle<promise_type> handle;

    struct promise_type {
        portable_noop_frame get_return_object() noexcept {
            return portable_noop_frame{
                coroutine_handle<promise_type>::from_promise(*this)};
        }

        suspend_always initial_suspend() noexcept { return {}; }
        suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() noexcept { __builtin_trap(); }
    };
};

inline portable_noop_frame make_portable_noop_frame() {
    for (;;) {
        co_await suspend_always{};
    }
}
#endif

inline void* noop_frame_address() noexcept {
    void* address = __atomic_load_n(&noop_frame_slot(), __ATOMIC_ACQUIRE);
    if (address) return address;
#if RIN_HAS_COROUTINES && defined(__clang__)
    address = __builtin_coro_noop();
#elif RIN_HAS_COROUTINES
    while (__sync_lock_test_and_set(&noop_frame_initializing(), 1) != 0) {
    }
    address = __atomic_load_n(&noop_frame_slot(), __ATOMIC_ACQUIRE);
    if (!address) {
        static portable_noop_frame frame = make_portable_noop_frame();
        address = frame.handle.address();
        __atomic_store_n(&noop_frame_slot(), address, __ATOMIC_RELEASE);
    }
    __sync_lock_release(&noop_frame_initializing());
    return address;
#else
    static char frame;
    address = &frame;
#endif
    __atomic_store_n(&noop_frame_slot(), address, __ATOMIC_RELEASE);
    return address;
}

} /* namespace detail */

template<typename Promise>
struct hash<coroutine_handle<Promise>> {
    size_t operator()(const coroutine_handle<Promise>& value) const noexcept {
        return hash<void*>{}(value.address());
    }
};

/* ═══════════════════════════════════════════════════════════════
 * ヘルパーコンセプト (内部用)
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

/* await_suspend の戻り値型チェック */
template<typename T>
struct is_coroutine_handle : false_type {};

template<typename Promise>
struct is_coroutine_handle<coroutine_handle<Promise>> : true_type {};

template<typename T>
inline constexpr bool is_coroutine_handle_v = is_coroutine_handle<T>::value;

} /* namespace detail */

} /* namespace std */

/* ═══════════════════════════════════════════════════════════════
 * サンプル: 基本的なジェネレータ
 * ═══════════════════════════════════════════════════════════════*/

#if RIN_HAS_COROUTINES

namespace std {

template<typename T>
struct generator {
    struct promise_type {
        T current_value;
        exception_ptr exception;

        /* A coroutine promise may opt into the standard allocation-failure
         * path only when its allocation function is non-throwing.  Keep the
         * generator frame on the Rin allocator and return an empty generator
         * when that allocator cannot supply a frame. */
        static void* operator new(size_t size) noexcept {
            return rin_malloc(size);
        }

        static void operator delete(void* memory) noexcept {
            rin_free(memory);
        }

        static void operator delete(void* memory, size_t) noexcept {
            rin_free(memory);
        }

        static generator get_return_object_on_allocation_failure() noexcept {
            return generator{coroutine_handle<promise_type>{}};
        }

        generator get_return_object() noexcept {
            return generator{coroutine_handle<promise_type>::from_promise(*this)};
        }

        suspend_always initial_suspend() noexcept { return {}; }
        suspend_always final_suspend() noexcept { return {}; }

        suspend_always yield_value(T value) {
            current_value = std::move(value);
            return {};
        }

        void return_void() {}
        void unhandled_exception() noexcept {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            exception = current_exception();
#else
            __builtin_trap();
#endif
        }
    };

    coroutine_handle<promise_type> handle_;

    generator(coroutine_handle<promise_type> h) noexcept : handle_(h) {}

    ~generator() {
        if (handle_) {
            handle_.destroy();
        }
    }

    generator(const generator&) = delete;
    generator& operator=(const generator&) = delete;

    generator(generator&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    generator& operator=(generator&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    /* イテレータインターフェース */
    struct iterator {
        coroutine_handle<promise_type> handle_;

        iterator& operator++() {
            handle_.resume();
            if (handle_.promise().exception) {
                exception_ptr exception = handle_.promise().exception;
                handle_ = nullptr;
                rethrow_exception(exception);
            }
            if (handle_.done()) {
                handle_ = nullptr;
            }
            return *this;
        }

        T& operator*() {
            return handle_.promise().current_value;
        }

        bool operator!=(const iterator& other) const {
            return handle_ != other.handle_;
        }
    };

    iterator begin() {
        if (handle_) {
            if (handle_.promise().exception) {
                rethrow_exception(handle_.promise().exception);
            }
            if (handle_.done()) {
                return {nullptr};
            }
            handle_.resume();
            if (handle_.promise().exception) {
                rethrow_exception(handle_.promise().exception);
            }
            if (handle_.done()) {
                return {nullptr};
            }
        }
        return {handle_};
    }

    iterator end() {
        return {nullptr};
    }
};

} /* namespace std */

#endif /* RIN_HAS_COROUTINES */

#endif /* __cplusplus >= 202002L */

#endif /* RINCXX_COROUTINE_H */
