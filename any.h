/*
 * RinOS C++ <any> ✿
 * 任意型 - bounded ownership subset
 */

#ifndef RINCXX_ANY_H
#define RINCXX_ANY_H

#include "rincxx.h"
#include "version.h"
#include "exception.h"
#include "initializer_list.h"
#include "new.h"
#include "type_traits.h"
#include "typeinfo.h"
#include "utility.h"

#if __cplusplus >= 201703L
#if defined(__GXX_RTTI) || defined(_CPPRTTI)
#define RIN_CXX_ANY_HAS_RTTI 1
#else
#define RIN_CXX_ANY_HAS_RTTI 0
#endif

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * bad_any_cast 例外
 * ═══════════════════════════════════════════════════════════════*/

class bad_any_cast : public bad_cast {
public:
    const char* what() const noexcept override { return "bad any cast"; }
};

namespace detail {

/* A token's address is not a type identity across a shared-library boundary:
 * each image gets a distinct instantiation of the inline variable.  Keep the
 * no-RTTI path numeric and deterministic instead.  The compiler-generated
 * signature contains the complete template argument and is identical in
 * every image built with the same ABI. */
inline unsigned long long any_type_key_hash(const char* text) noexcept {
    unsigned long long value = 1469598103934665603ull;
    if (!text) return value;
    while (*text) {
        value ^= static_cast<unsigned char>(*text++);
        value *= 1099511628211ull;
    }
    return value;
}

/* FNV-1a is a compact dispatch key, not a proof of type identity.  Keep the
 * compiler's complete template signature beside it so two distinct types
 * that happen to hash to the same 64-bit value cannot be cast as one another.
 * `__PRETTY_FUNCTION__`/`__FUNCSIG__` has static storage and is identical for
 * the same T across translation units built with one ABI. */
template<class T>
inline const char* any_type_signature() noexcept {
#if defined(_MSC_VER)
    return __FUNCSIG__;
#elif defined(__clang__) || defined(__GNUC__)
    return __PRETTY_FUNCTION__;
#else
    return "rin.any.unknown-type";
#endif
}

inline bool any_type_signature_equal(const char* left,
                                     const char* right) noexcept {
    if (!left || !right) return left == right;
    while (*left != '\0' && *right != '\0') {
        if (*left++ != *right++) return false;
    }
    return *left == *right;
}

template<class T>
inline unsigned long long any_type_key() noexcept {
#if defined(_MSC_VER)
    return any_type_key_hash(__FUNCSIG__);
#elif defined(__clang__) || defined(__GNUC__)
    return any_type_key_hash(__PRETTY_FUNCTION__);
#else
    /* Rin's supported compilers expose one of the signatures above.  Keep a
     * deterministic fallback for syntax-only ports rather than reintroducing
     * image-local addresses. */
    return 0xcbf29ce484222325ull;
#endif
}

template<class T>
struct is_in_place_type : false_type {};

template<class T>
struct is_in_place_type<in_place_type_t<T>> : true_type {};

[[noreturn]] inline void any_cast_fail() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_any_cast();
#else
    __builtin_trap();
#endif
}

[[noreturn]] inline void any_allocation_fail() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_alloc();
#else
    __builtin_trap();
#endif
}

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * any クラス
 * ═══════════════════════════════════════════════════════════════*/

class any {
    /* 型消去のための基底クラス */
    struct holder_base {
        virtual ~holder_base() = default;
        virtual holder_base* clone() const = 0;
        virtual unsigned long long type_key() const noexcept = 0;
#if !RIN_CXX_ANY_HAS_RTTI
        virtual const char* type_signature() const noexcept = 0;
#endif
        virtual const type_info& type() const noexcept = 0;
    };
    
    /* 具体的な型を保持するクラス */
    template<class T>
    struct holder : holder_base {
        T value;
        
        template<class... Args>
        explicit holder(in_place_type_t<T>, Args&&... args)
            : value(std::forward<Args>(args)...) {}
        
        holder_base* clone() const override {
            holder* result = new (nothrow) holder(in_place_type<T>, value);
            if (!result) detail::any_allocation_fail();
            return result;
        }
        
        unsigned long long type_key() const noexcept override {
            return get_type_key<T>();
        }

#if !RIN_CXX_ANY_HAS_RTTI
        const char* type_signature() const noexcept override {
            return detail::any_type_signature<
                typename remove_cv<T>::type>();
        }
#endif

#if RIN_CXX_ANY_HAS_RTTI
        const type_info& type() const noexcept override {
            return typeid(T);
        }
#else
        const type_info& type() const noexcept override {
            return detail::rin_no_rtti_type_info<T>();
        }
#endif
    };
    
    template<class T>
    static unsigned long long get_type_key() noexcept {
        using RawT = typename remove_cv<T>::type;
        return detail::any_type_key<RawT>();
    }

    template<class T, class... Args>
    static holder<T>* make_holder(Args&&... args) {
        holder<T>* result = new (nothrow) holder<T>(
            in_place_type<T>, std::forward<Args>(args)...);
        if (!result) detail::any_allocation_fail();
        return result;
    }
    
    holder_base* ptr_ = nullptr;
    
public:
    /* ═══════════════════════════════════════════════════════════
     * コンストラクタ・デストラクタ
     * ═══════════════════════════════════════════════════════════*/
    
    constexpr any() noexcept : ptr_(nullptr) {}
    
    any(const any& other) : ptr_(other.ptr_ ? other.ptr_->clone() : nullptr) {}
    
    any(any&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    
    template<class T,
             class DecayT = typename decay<T>::type,
             typename enable_if<
                 !is_same<DecayT, any>::value &&
                 !detail::is_in_place_type<DecayT>::value &&
                 is_copy_constructible<DecayT>::value &&
                 is_constructible<DecayT, T&&>::value,
                 int
             >::type = 0>
    any(T&& value)
        : ptr_(make_holder<DecayT>(std::forward<T>(value))) {}

    template<class T, class... Args,
             typename enable_if<
                 is_copy_constructible<typename decay<T>::type>::value &&
                 is_constructible<typename decay<T>::type, Args&&...>::value,
                 int
             >::type = 0>
    explicit any(in_place_type_t<T>, Args&&... args)
        : ptr_(make_holder<typename decay<T>::type>(
              std::forward<Args>(args)...)) {}

    template<class T, class U, class... Args,
             typename enable_if<
                 is_copy_constructible<typename decay<T>::type>::value &&
                 is_constructible<typename decay<T>::type,
                                  initializer_list<U>&, Args&&...>::value,
                 int
             >::type = 0>
    explicit any(in_place_type_t<T>, initializer_list<U> values,
                 Args&&... args)
        : ptr_(make_holder<typename decay<T>::type>(
              values, std::forward<Args>(args)...)) {}
    
    ~any() {
        reset();
    }
    
    /* ═══════════════════════════════════════════════════════════
     * 代入
     * ═══════════════════════════════════════════════════════════*/
    
    any& operator=(const any& other) {
        if (this != &other) {
            any candidate(other);
            swap(candidate);
        }
        return *this;
    }
    
    any& operator=(any&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    template<class T,
             class DecayT = typename decay<T>::type,
             typename enable_if<
                 !is_same<DecayT, any>::value &&
                 is_copy_constructible<DecayT>::value &&
                 is_constructible<DecayT, T&&>::value,
                 int
             >::type = 0>
    any& operator=(T&& value) {
        any candidate(std::forward<T>(value));
        swap(candidate);
        return *this;
    }
    
    /* ═══════════════════════════════════════════════════════════
     * 変更
     * ═══════════════════════════════════════════════════════════*/
    
    template<class T, class... Args>
    typename enable_if<
        is_copy_constructible<typename decay<T>::type>::value &&
        is_constructible<typename decay<T>::type, Args&&...>::value,
        typename decay<T>::type&
    >::type emplace(Args&&... args) {
        using DecayT = typename decay<T>::type;
        /* any::emplace has a deliberately weaker contract than assignment:
         * the old contained object is destroyed before construction starts.
         * If allocation or T's constructor throws, this leaves *this empty as
         * required by [any.modifiers], rather than preserving a stale value. */
        reset();
        ptr_ = make_holder<DecayT>(std::forward<Args>(args)...);
        return static_cast<holder<DecayT>*>(ptr_)->value;
    }

    template<class T, class U, class... Args>
    typename enable_if<
        is_copy_constructible<typename decay<T>::type>::value &&
        is_constructible<typename decay<T>::type,
                         initializer_list<U>&, Args&&...>::value,
        typename decay<T>::type&
    >::type emplace(initializer_list<U> values, Args&&... args) {
        using DecayT = typename decay<T>::type;
        reset();
        ptr_ = make_holder<DecayT>(values, std::forward<Args>(args)...);
        return static_cast<holder<DecayT>*>(ptr_)->value;
    }
    
    void reset() noexcept {
        if (ptr_) {
            delete ptr_;
            ptr_ = nullptr;
        }
    }
    
    void swap(any& other) noexcept {
        std::swap(ptr_, other.ptr_);
    }
    
    /* ═══════════════════════════════════════════════════════════
     * 状態
     * ═══════════════════════════════════════════════════════════*/
    
    bool has_value() const noexcept {
        return ptr_ != nullptr;
    }

    const type_info& type() const noexcept {
#if RIN_CXX_ANY_HAS_RTTI
        return ptr_ ? ptr_->type() : typeid(void);
#else
        return ptr_ ? ptr_->type() : detail::rin_no_rtti_type_info<void>();
#endif
    }
    
    /* ═══════════════════════════════════════════════════════════
     * any_cast用のfriend
     * ═══════════════════════════════════════════════════════════*/
    
    template<class T>
    friend T* any_cast_impl(any* operand) noexcept;
    
    template<class T>
    friend const T* any_cast_impl(const any* operand) noexcept;
};

/* ═══════════════════════════════════════════════════════════════
 * any_cast 実装
 * ═══════════════════════════════════════════════════════════════*/

template<class T>
T* any_cast_impl(any* operand) noexcept {
    if (!operand || !operand->ptr_) return nullptr;
    
    using DecayT = typename decay<T>::type;
#if RIN_CXX_ANY_HAS_RTTI
    if (operand->ptr_->type() != typeid(DecayT)) {
        return nullptr;
    }
#else
    if (operand->ptr_->type_key() != any::get_type_key<DecayT>() ||
        !detail::any_type_signature_equal(
            operand->ptr_->type_signature(),
            detail::any_type_signature<DecayT>())) {
        return nullptr;
    }
#endif
    
    return &static_cast<any::holder<DecayT>*>(operand->ptr_)->value;
}

template<class T>
const T* any_cast_impl(const any* operand) noexcept {
    return any_cast_impl<T>(const_cast<any*>(operand));
}

/* ═══════════════════════════════════════════════════════════════
 * any_cast 関数
 * ═══════════════════════════════════════════════════════════════*/

template<class T,
         typename enable_if<
             is_constructible<
                 T,
                 const typename remove_cv<
                     typename remove_reference<T>::type>::type&>::value,
             int>::type = 0>
T any_cast(const any& operand) {
    using DecayT = typename remove_cv<typename remove_reference<T>::type>::type;
    const DecayT* result = any_cast_impl<DecayT>(&operand);
    if (!result) detail::any_cast_fail();
    return static_cast<T>(*result);
}

template<class T,
         typename enable_if<
             is_constructible<
                 T,
                 typename remove_cv<
                     typename remove_reference<T>::type>::type&>::value,
             int>::type = 0>
T any_cast(any& operand) {
    using DecayT = typename remove_cv<typename remove_reference<T>::type>::type;
    DecayT* result = any_cast_impl<DecayT>(&operand);
    if (!result) detail::any_cast_fail();
    return static_cast<T>(*result);
}

template<class T,
         typename enable_if<
             is_constructible<
                 T,
                 typename remove_cv<
                     typename remove_reference<T>::type>::type&&>::value,
             int>::type = 0>
T any_cast(any&& operand) {
    using DecayT = typename remove_cv<typename remove_reference<T>::type>::type;
    DecayT* result = any_cast_impl<DecayT>(&operand);
    if (!result) detail::any_cast_fail();
    return static_cast<T>(std::move(*result));
}

template<class T>
const T* any_cast(const any* operand) noexcept {
    return any_cast_impl<T>(operand);
}

template<class T>
T* any_cast(any* operand) noexcept {
    return any_cast_impl<T>(operand);
}

/* ═══════════════════════════════════════════════════════════════
 * make_any
 * ═══════════════════════════════════════════════════════════════*/

template<class T, class... Args>
typename enable_if<
    is_copy_constructible<typename decay<T>::type>::value &&
    is_constructible<typename decay<T>::type, Args&&...>::value,
    any
>::type make_any(Args&&... args) {
    return any(in_place_type<T>, std::forward<Args>(args)...);
}

template<class T, class U, class... Args>
typename enable_if<
    is_copy_constructible<typename decay<T>::type>::value &&
    is_constructible<typename decay<T>::type,
                     initializer_list<U>&, Args&&...>::value,
    any
>::type make_any(initializer_list<U> values, Args&&... args) {
    return any(in_place_type<T>, values, std::forward<Args>(args)...);
}

/* ═══════════════════════════════════════════════════════════════
 * swap
 * ═══════════════════════════════════════════════════════════════*/

inline void swap(any& lhs, any& rhs) noexcept {
    lhs.swap(rhs);
}

} /* namespace std */

#undef RIN_CXX_ANY_HAS_RTTI

#endif /* __cplusplus >= 201703L */

#endif /* RINCXX_ANY_H */
