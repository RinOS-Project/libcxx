/*
 * RinOS C++ Exception Support ✿
 * 例外型とbounded runtime boundary
 *
 * 注意: RinOSはfreestanding環境のため、throw/catchとexception_ptrの完全な
 * runtimeはまだサポートしません。対応済みのterminate handler境界と、
 * 明示的な未対応APIを分離して提供します。
 */

#ifndef RINCXX_EXCEPTION_H
#define RINCXX_EXCEPTION_H

#include "../libc/stddef.h"
#define RINCXX_EXCEPTION_IN_PROGRESS 1
#include "string.h"
#undef RINCXX_EXCEPTION_IN_PROGRESS

#ifdef __cplusplus

namespace __cxxabiv1 {

struct __cxa_eh_globals;
extern "C" __cxa_eh_globals* __cxa_get_globals();

} /* namespace __cxxabiv1 */

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * exception基底クラス
 * ═══════════════════════════════════════════════════════════════*/

class exception {
public:
    exception() noexcept {}
    exception(const exception&) noexcept = default;
    exception& operator=(const exception&) noexcept = default;
    virtual ~exception() noexcept {}
    virtual const char* what() const noexcept { return "std::exception"; }
};

/* ═══════════════════════════════════════════════════════════════
 * 標準例外クラス
 * ═══════════════════════════════════════════════════════════════*/

class bad_alloc : public exception {
public:
    bad_alloc() noexcept {}
    const char* what() const noexcept override { return "std::bad_alloc"; }
};

class bad_cast : public exception {
public:
    bad_cast() noexcept {}
    const char* what() const noexcept override { return "std::bad_cast"; }
};

class bad_typeid : public exception {
public:
    bad_typeid() noexcept {}
    const char* what() const noexcept override { return "std::bad_typeid"; }
};

class bad_array_new_length : public bad_alloc {
public:
    bad_array_new_length() noexcept {}
    const char* what() const noexcept override { return "std::bad_array_new_length"; }
};

/* ═══════════════════════════════════════════════════════════════
 * logic_error系
 * ═══════════════════════════════════════════════════════════════*/

class logic_error : public exception {
private:
    string msg_;
public:
    explicit logic_error(const string& msg) : msg_(msg) {}
    explicit logic_error(const char* msg) : msg_(msg ? msg : "std::logic_error") {}
    const char* what() const noexcept override { return msg_.c_str(); }
};

class domain_error : public logic_error {
public:
    explicit domain_error(const string& msg) : logic_error(msg) {}
    explicit domain_error(const char* msg) : logic_error(msg) {}
};

class invalid_argument : public logic_error {
public:
    explicit invalid_argument(const string& msg) : logic_error(msg) {}
    explicit invalid_argument(const char* msg) : logic_error(msg) {}
};

class length_error : public logic_error {
public:
    explicit length_error(const string& msg) : logic_error(msg) {}
    explicit length_error(const char* msg) : logic_error(msg) {}
};

class out_of_range : public logic_error {
public:
    explicit out_of_range(const string& msg) : logic_error(msg) {}
    explicit out_of_range(const char* msg) : logic_error(msg) {}
};

namespace __detail {

/*
 * string.h must define basic_string before exception.h can define
 * logic_error's owned diagnostic.  Keep the checked string/string_view
 * boundary behind this late-defined helper so both include orders work.
 */
[[noreturn]] inline void string_position_out_of_range(const char* diagnostic) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw out_of_range(diagnostic);
#else
    (void)diagnostic;
    __builtin_trap();
#endif
}

[[noreturn]] inline void string_numeric_invalid(const char* diagnostic) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw invalid_argument(diagnostic);
#else
    (void)diagnostic;
    __builtin_trap();
#endif
}

[[noreturn]] inline void string_numeric_range(const char* diagnostic) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw out_of_range(diagnostic);
#else
    (void)diagnostic;
    __builtin_trap();
#endif
}

inline bool string_allocation_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_alloc();
#else
    return false;
#endif
}

inline bool string_length_failure(const char* diagnostic) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw length_error(diagnostic);
#else
    (void)diagnostic;
    return false;
#endif
}

} /* namespace __detail */

/* ═══════════════════════════════════════════════════════════════
 * runtime_error系
 * ═══════════════════════════════════════════════════════════════*/

class runtime_error : public exception {
private:
    string msg_;
public:
    explicit runtime_error(const string& msg) : msg_(msg) {}
    explicit runtime_error(const char* msg) : msg_(msg ? msg : "std::runtime_error") {}
    const char* what() const noexcept override { return msg_.c_str(); }
};

class range_error : public runtime_error {
public:
    explicit range_error(const string& msg) : runtime_error(msg) {}
    explicit range_error(const char* msg) : runtime_error(msg) {}
};

class overflow_error : public runtime_error {
public:
    explicit overflow_error(const string& msg) : runtime_error(msg) {}
    explicit overflow_error(const char* msg) : runtime_error(msg) {}
};

class underflow_error : public runtime_error {
public:
    explicit underflow_error(const string& msg) : runtime_error(msg) {}
    explicit underflow_error(const char* msg) : runtime_error(msg) {}
};

/* system_error は system_error.h で定義 */

/* ═══════════════════════════════════════════════════════════════
 * 例外処理ヘルパー
 * ═══════════════════════════════════════════════════════════════*/

#if defined(_GLIBCXX_CXX_CONFIG_H)
__attribute__((noreturn)) inline void terminate() noexcept;
#else
[[noreturn]] inline void terminate() noexcept;
#endif

namespace __exception_ptr {
class exception_ptr;
}

using __exception_ptr::exception_ptr;

exception_ptr current_exception() noexcept;
[[noreturn]] void rethrow_exception(exception_ptr);

template<typename E>
exception_ptr make_exception_ptr(E value) noexcept;

namespace __exception_ptr {

class exception_ptr {
private:
    void* exception_object_;

    explicit exception_ptr(void* object) noexcept;

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    void _M_addref() noexcept;
    void _M_release() noexcept;
#else
    void _M_addref() noexcept {}
    void _M_release() noexcept {}
#endif

    friend exception_ptr std::current_exception() noexcept;
    friend void std::rethrow_exception(exception_ptr);

    template<typename E>
    friend exception_ptr std::make_exception_ptr(E) noexcept;

public:
    exception_ptr() noexcept : exception_object_(nullptr) {}
    exception_ptr(decltype(nullptr)) noexcept : exception_object_(nullptr) {}

    exception_ptr(const exception_ptr& other) noexcept
        : exception_object_(other.exception_object_) {
        if (exception_object_) {
            _M_addref();
        }
    }

    exception_ptr(exception_ptr&& other) noexcept
        : exception_object_(other.exception_object_) {
        other.exception_object_ = nullptr;
    }

    exception_ptr& operator=(const exception_ptr& other) noexcept {
        exception_ptr(other).swap(*this);
        return *this;
    }

    exception_ptr& operator=(exception_ptr&& other) noexcept {
        exception_ptr(static_cast<exception_ptr&&>(other)).swap(*this);
        return *this;
    }

    exception_ptr& operator=(decltype(nullptr)) noexcept {
        exception_ptr().swap(*this);
        return *this;
    }

    ~exception_ptr() noexcept {
        if (exception_object_) {
            _M_release();
        }
    }

    void swap(exception_ptr& other) noexcept {
        void* temporary = exception_object_;
        exception_object_ = other.exception_object_;
        other.exception_object_ = temporary;
    }

    explicit operator bool() const noexcept { return exception_object_ != nullptr; }

    friend bool operator==(const exception_ptr& left,
                           const exception_ptr& right) noexcept {
        return left.exception_object_ == right.exception_object_;
    }

    friend bool operator!=(const exception_ptr& left,
                           const exception_ptr& right) noexcept {
        return !(left == right);
    }
};

inline void swap(exception_ptr& left, exception_ptr& right) noexcept {
    left.swap(right);
}

} /* namespace __exception_ptr */

#if !defined(__cpp_exceptions) && !defined(__EXCEPTIONS)
inline exception_ptr current_exception() noexcept { return exception_ptr(); }
[[noreturn]] inline void rethrow_exception(exception_ptr) { terminate(); }
#endif

template<typename E>
exception_ptr make_exception_ptr(E value) noexcept {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
        throw value;
    } catch (...) {
        return current_exception();
    }
#else
    (void)value;
    return exception_ptr();
#endif
}

/* terminate handler ownership */
using terminate_handler = void (*)();

namespace __rin_exception_detail {

/* Itanium C++ ABI 1.2, section 2.2.2. The runtime owns this TLS object. */
struct cxa_eh_globals_view {
    void* caught_exceptions;
    unsigned int uncaught_exceptions;
};

[[noreturn]] inline void default_terminate_handler() noexcept {
    __builtin_trap();
}

/*
 * C++11 inline functions share their local static object across translation
 * units. The function pointer initializes on first use without a C++17
 * inline-variable declaration.
 */
inline terminate_handler& terminate_handler_state() noexcept {
    static terminate_handler state = default_terminate_handler;
    return state;
}

} /* namespace __rin_exception_detail */

inline terminate_handler set_terminate(terminate_handler handler) noexcept {
    if (!handler) {
        handler = __rin_exception_detail::default_terminate_handler;
    }
    return __atomic_exchange_n(
        &__rin_exception_detail::terminate_handler_state(),
        handler,
        __ATOMIC_ACQ_REL);
}

inline terminate_handler get_terminate() noexcept {
    return __atomic_load_n(
        &__rin_exception_detail::terminate_handler_state(),
        __ATOMIC_ACQUIRE);
}

#if defined(_GLIBCXX_CXX_CONFIG_H)
__attribute__((noreturn)) inline void terminate() noexcept {
#else
[[noreturn]] inline void terminate() noexcept {
#endif
    terminate_handler handler = get_terminate();
    if (handler) {
        handler();
    }
    /* A handler that returns must not let execution continue. */
    __builtin_trap();
}

/*
 * uncaught_exceptions (C++17)
 *
 * Keep the hosted header-only implementation local to each translation unit.
 * MinGW's libsupc++ also exports the same standard symbol; emitting a strong
 * out-of-line copy from this header would make a hosted link fail with a
 * duplicate definition.  The state itself remains process/thread-owned by
 * the Itanium ABI runtime queried below, so internal linkage does not create a
 * second exception counter.  Freestanding builds retain the normal inline
 * external-linkage form for compatibility with a target C++ runtime.
 */
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && __STDC_HOSTED__
static inline int uncaught_exceptions() noexcept {
#else
inline int uncaught_exceptions() noexcept {
#endif
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    auto* globals = reinterpret_cast<
        const __rin_exception_detail::cxa_eh_globals_view*>(
            __cxxabiv1::__cxa_get_globals());
    if (!globals) {
        __builtin_trap();
    }
    return static_cast<int>(globals->uncaught_exceptions);
#else
    /* An exception cannot be active in an exception-disabled translation unit. */
    return 0;
#endif
}

/* nested_exception */
class nested_exception {
private:
    exception_ptr nested_;

public:
    nested_exception() noexcept : nested_(current_exception()) {}
    nested_exception(const nested_exception&) noexcept = default;
    nested_exception& operator=(const nested_exception&) noexcept = default;
    virtual ~nested_exception() noexcept {}

    [[noreturn]] void rethrow_nested() const {
        if (nested_) {
            rethrow_exception(nested_);
        }
        terminate();
    }

    exception_ptr nested_ptr() const noexcept { return nested_; }
};

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)

namespace __rin_exception_detail {

template<typename T>
struct with_nested : T, nested_exception {
    template<typename U>
    explicit with_nested(U&& value)
        : T(std::forward<U>(value)), nested_exception() {}
};

template<typename T>
struct use_nested_wrapper : integral_constant<bool,
    is_class<typename decay<T>::type>::value
    && !is_final<typename decay<T>::type>::value
    && !is_base_of<nested_exception, typename decay<T>::type>::value> {};

template<typename T>
[[noreturn]] inline typename enable_if<
    use_nested_wrapper<T>::value>::type
throw_with_nested_impl(T&& value) {
    using value_type = typename decay<T>::type;
    throw with_nested<value_type>(std::forward<T>(value));
}

template<typename T>
[[noreturn]] inline typename enable_if<
    !use_nested_wrapper<T>::value>::type
throw_with_nested_impl(T&& value) {
    throw std::forward<T>(value);
}

template<typename T>
inline typename enable_if<is_polymorphic<T>::value>::type
rethrow_if_nested_impl(const T& value) {
    const nested_exception* nested =
        dynamic_cast<const nested_exception*>(__builtin_addressof(value));
    if (nested) {
        nested->rethrow_nested();
    }
}

template<typename T>
inline typename enable_if<!is_polymorphic<T>::value>::type
rethrow_if_nested_impl(const T&) noexcept {}

} /* namespace __rin_exception_detail */

template<typename T>
[[noreturn]] inline void throw_with_nested(T&& value) {
    __rin_exception_detail::throw_with_nested_impl(
        std::forward<T>(value));
}

template<typename T>
inline void rethrow_if_nested(const T& value) {
    __rin_exception_detail::rethrow_if_nested_impl(value);
}

#else

template<typename T>
[[noreturn]] inline void throw_with_nested(T&& value) {
    (void)value;
    terminate();
}

template<typename T>
inline void rethrow_if_nested(const T&) noexcept {}

#endif

} /* namespace std */

/* ═══════════════════════════════════════════════════════════════
 * ABI用外部シンボル（コンパイラが必要とする）
 * cxxabi.hがインクルードされている場合はそちらの宣言を使用
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _CXXABI_H

extern "C" {

/* 例外投げ（無効化） */
void __cxa_throw(void*, void*, void (*)(void*));
void* __cxa_allocate_exception(size_t) noexcept;
void __cxa_free_exception(void*) noexcept;
/* Note: __cxa_begin_catch and __cxa_end_catch are implicitly declared by compiler */

/* 純粋仮想関数呼び出しハンドラ */
void __cxa_pure_virtual();

/* ABIガード */
int __cxa_guard_acquire(long long*);
void __cxa_guard_release(long long*);
void __cxa_guard_abort(long long*);

}

/* GCC/Itanium emits these helpers for a failed reference dynamic_cast and
 * for typeid applied to a null polymorphic pointer.  Keep the ABI boundary
 * on Rin's exception classes so callers can catch the exact standard type
 * instead of silently linking the hosted libsupc++ implementation. */
#ifndef RINCXX_CXA_BAD_CAST_DEFINED
#define RINCXX_CXA_BAD_CAST_DEFINED 1
extern "C" {
[[noreturn]] inline void __cxa_bad_cast() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw ::std::bad_cast();
#else
    __builtin_trap();
#endif
}

[[noreturn]] inline void __cxa_bad_typeid() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw ::std::bad_typeid();
#else
    __builtin_trap();
#endif
}
}
#endif

#endif /* _CXXABI_H */

#endif /* __cplusplus */
#endif /* RINCXX_EXCEPTION_H */
