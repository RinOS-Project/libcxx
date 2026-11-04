/*
 * RinOS C++ Functional ✿
 * std::function 互換実装
 */

#ifndef RINCXX_FUNCTIONAL_H
#define RINCXX_FUNCTIONAL_H

#include "rincxx.h"
#include "version.h"
#include "type_traits.h"
#include "invoke.h"
#include "hash_integral.h"
#include "exception.h"
#include "new.h"
#include "initializer_list.h"
#include "memory.h"
#include "typeinfo.h"
#include "pointer_order.h"
#include "iterator.h"
#include "utility.h"  /* pair for C++17 searcher results */

#if defined(__RCC__) || defined(__GXX_RTTI) || defined(_CPPRTTI)
#define RIN_CXX_FUNCTION_HAS_STATIC_TYPEINFO 1
#else
#define RIN_CXX_FUNCTION_HAS_STATIC_TYPEINFO 0
#endif

#ifdef __cplusplus

namespace std {

template<typename T>
struct hash;

#if __cplusplus >= 201402L
#define RIN_FUNCTIONAL_CONSTEXPR14 constexpr
#else
#define RIN_FUNCTIONAL_CONSTEXPR14 inline
#endif

/* ═══════════════════════════════════════════════════════════════
 * std::function implementation helpers
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

/* Allocator-extended function targets are owned by the allocator selected at
 * construction time.  A conforming allocator may expose a fancy pointer for
 * its rebound storage, so keep the raw address conversion at this one
 * boundary and reconstruct the pointer for deallocation through
 * pointer_traits. */
template<typename T>
inline T* functional_pointer_address(T* pointer) noexcept {
    return pointer;
}

#if __cplusplus >= 202002L
template<typename Pointer>
inline auto functional_pointer_address(const Pointer& pointer) noexcept
    -> decltype(std::to_address(pointer)) {
    return std::to_address(pointer);
}
#else
template<typename Pointer>
inline auto functional_pointer_address(const Pointer& pointer) noexcept
    -> decltype(pointer_traits<Pointer>::to_address(pointer)) {
    return pointer_traits<Pointer>::to_address(pointer);
}

template<typename Pointer>
inline auto functional_pointer_address(const Pointer& pointer) noexcept
    -> decltype(functional_pointer_address(pointer.operator->())) {
    return functional_pointer_address(pointer.operator->());
}
#endif

inline unsigned long long functional_type_key_hash(const char* text) noexcept {
    unsigned long long value = 1469598103934665603ull;
    if (!text) return value;
    while (*text) {
        value ^= static_cast<unsigned char>(*text++);
        value *= 1099511628211ull;
    }
    return value;
}

template<typename T>
inline unsigned long long functional_type_token() noexcept {
#if defined(_MSC_VER)
    return functional_type_key_hash(__FUNCSIG__);
#elif defined(__clang__) || defined(__GNUC__)
    return functional_type_key_hash(__PRETTY_FUNCTION__);
#else
    return 0xcbf29ce484222325ull;
#endif
}

template<typename T>
inline const char* functional_type_signature() noexcept {
#if defined(_MSC_VER)
    return __FUNCSIG__;
#elif defined(__clang__) || defined(__GNUC__)
    return __PRETTY_FUNCTION__;
#else
    return "rin.functional.unknown-type";
#endif
}

inline bool functional_type_signature_equal(const char* left,
                                            const char* right) noexcept {
    if (!left || !right) return left == right;
    while (*left != '\0' && *right != '\0') {
        if (*left++ != *right++) return false;
    }
    return *left == *right;
}

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * function
 * ═══════════════════════════════════════════════════════════════*/

template<typename>
class function;

class bad_function_call : public exception {
public:
    bad_function_call() noexcept = default;
    const char* what() const noexcept override
    {
        return "std::bad_function_call";
    }
};

namespace detail {

[[noreturn]] inline void functional_empty_call()
{
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_function_call();
#else
    __builtin_trap();
#endif
}

[[noreturn]] inline void functional_allocation_failure()
{
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_alloc();
#else
    __builtin_trap();
#endif
}

template<typename F>
constexpr bool functional_is_null_target_impl(
    const F& target, true_type) noexcept {
    return target == nullptr;
}

template<typename F>
constexpr bool functional_is_null_target_impl(
    const F&, false_type) noexcept {
    return false;
}

template<typename F>
constexpr bool functional_is_null_target(const F& target) noexcept {
    return functional_is_null_target_impl(
        target,
        integral_constant<bool,
            is_pointer<F>::value || is_member_pointer<F>::value>());
}

template<typename Result>
struct functional_function_call {
    template<typename F, typename... CallArgs>
    static Result call(F& function, CallArgs&&... args) {
        return std::invoke(function, std::forward<CallArgs>(args)...);
    }
};

template<>
struct functional_function_call<void> {
    template<typename F, typename... CallArgs>
    static void call(F& function, CallArgs&&... args) {
        std::invoke(function, std::forward<CallArgs>(args)...);
    }
};

template<typename Pointer>
constexpr bool functional_pointer_less_impl(
    Pointer left, Pointer right, true_type) noexcept {
    return object_pointer_total_less(left, right);
}

template<typename Pointer>
constexpr bool functional_pointer_less_impl(
    Pointer left, Pointer right, false_type) noexcept {
    return function_pointer_total_less(left, right);
}

template<typename Pointer>
constexpr bool functional_pointer_less(
    Pointer left, Pointer right) noexcept {
    return functional_pointer_less_impl(
        left, right,
        integral_constant<bool, is_object_pointer<Pointer>::value>());
}

} /* namespace detail */

template<typename R, typename... Args>
class function<R(Args...)> {
private:
    /*
     * Keep the erased object itself in this buffer.  Three pointer words are
     * enough for a vptr plus the ordinary function-pointer/reference-wrapper
     * targets on the supported x86 ABIs, while max_align_t keeps every
     * normally-aligned callable correctly placed.  Over-aligned objects stay
     * heap-owned below; pretending that they fit here would be undefined.
     */
    static constexpr size_t inline_storage_size = 3 * sizeof(void*);
    using inline_storage_type = aligned_storage_t<
        inline_storage_size, alignof(max_align_t)>;

    struct callable_base {
        virtual ~callable_base() = default;
        virtual R invoke(Args... args) = 0;
        virtual callable_base* clone(void* storage, bool& local) const = 0;
        virtual callable_base* move_into(void* storage) noexcept = 0;
        virtual void destroy(bool local) noexcept = 0;
        virtual void* target_address() noexcept = 0;
        virtual const void* target_address() const noexcept = 0;
#if !RIN_CXX_FUNCTION_HAS_STATIC_TYPEINFO
        virtual unsigned long long target_token() const noexcept = 0;
        virtual const char* target_signature() const noexcept = 0;
#endif
        virtual const type_info& target_type() const noexcept = 0;
    };
    
template<typename F>
    struct callable : callable_base {
        F func;
        callable(const F& f) : func(f) {}
        callable(F&& f) : func(std::move(f)) {}

        static constexpr bool fits_inline() noexcept {
            return sizeof(callable) <= inline_storage_size &&
                alignof(callable) <= alignof(inline_storage_type) &&
                is_nothrow_move_constructible<F>::value;
        }

        R invoke(Args... args) override {
            return detail::functional_function_call<R>::call(
                func, std::forward<Args>(args)...);
        }
        callable_base* clone(void* storage, bool& local) const override {
            local = false;
            if (fits_inline()) {
                callable_base* result = new (storage) callable(func);
                local = true;
                return result;
            }
            return new callable(func);
        }
        callable_base* move_into(void* storage) noexcept override {
            return new (storage) callable(std::move(func));
        }
        void destroy(bool local) noexcept override {
            if (local) {
                this->~callable();
            } else {
                delete this;
            }
        }
        void* target_address() noexcept override { return &func; }
        const void* target_address() const noexcept override { return &func; }
#if !RIN_CXX_FUNCTION_HAS_STATIC_TYPEINFO
        unsigned long long target_token() const noexcept override {
            return detail::functional_type_token<remove_cv_t<F>>();
        }
        const char* target_signature() const noexcept override {
            return detail::functional_type_signature<remove_cv_t<F>>();
        }
#endif
#if RIN_CXX_FUNCTION_HAS_STATIC_TYPEINFO
        const type_info& target_type() const noexcept override {
            return typeid(F);
        }
#else
        const type_info& target_type() const noexcept override {
            return detail::rin_no_rtti_type_info<F>();
        }
#endif
    };

    /* Allocator-aware construction is intentionally a heap-only path.  The
     * allocator object must live with the erased target so copy/destroy use
     * the exact stateful allocator selected by the caller; placing it in the
     * small inline buffer would silently discard that state. */
    template<typename Alloc, typename Target, typename = void>
    struct allocator_target_compatible : false_type {};

    template<typename Alloc, typename Target>
    struct allocator_target_compatible<
        Alloc, Target,
        void_t<typename allocator_traits<Alloc>::template rebind_traits<
                   Target>::pointer,
                decltype(detail::functional_pointer_address(
                    declval<const typename allocator_traits<Alloc>::template
                                 rebind_traits<Target>::pointer&>())),
                decltype(pointer_traits<typename allocator_traits<Alloc>::template
                             rebind_traits<Target>::pointer>::pointer_to(
                    declval<Target&>())),
                typename allocator_traits<Alloc>::template rebind_alloc<
                    Target>>>
        : integral_constant<bool,
            is_constructible<typename allocator_traits<Alloc>::template
                                 rebind_alloc<Target>, Alloc>::value> {};

    template<typename F, typename Alloc>
    struct allocator_callable : callable_base {
        using self_type = allocator_callable<F, Alloc>;
        using allocator_type = typename allocator_traits<Alloc>::template
            rebind_alloc<self_type>;
        using allocator_traits_type = allocator_traits<allocator_type>;

        Alloc allocator_;
        F func;

        allocator_callable(const F& value, const Alloc& alloc)
            : allocator_(alloc), func(value) {}
        allocator_callable(F&& value, const Alloc& alloc)
            : allocator_(alloc), func(std::move(value)) {}

        R invoke(Args... args) override {
            return detail::functional_function_call<R>::call(
                func, std::forward<Args>(args)...);
        }

        callable_base* clone(void*, bool& local) const override {
            local = false;
            allocator_type alloc(allocator_);
            using storage_pointer = typename allocator_traits_type::pointer;
            storage_pointer storage =
                allocator_traits_type::allocate(alloc, 1);
            self_type* result = detail::functional_pointer_address(storage);
            if (storage == storage_pointer() || !result) {
                if (storage != storage_pointer())
                    allocator_traits_type::deallocate(alloc, storage, 1);
                detail::functional_allocation_failure();
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            try {
                allocator_traits_type::construct(
                    alloc, result, func, allocator_);
            } catch (...) {
                allocator_traits_type::deallocate(alloc, storage, 1);
                throw;
            }
#else
            allocator_traits_type::construct(alloc, result, func, allocator_);
#endif
            return result;
        }

        callable_base* move_into(void*) noexcept override {
            /* Allocator-aware targets never use the inline storage. */
            __builtin_trap();
        }

        void destroy(bool local) noexcept override {
            if (local) {
                this->~allocator_callable();
                return;
            }
            allocator_type alloc(allocator_);
            using storage_pointer = typename allocator_traits_type::pointer;
            storage_pointer storage =
                pointer_traits<storage_pointer>::pointer_to(*this);
            allocator_traits_type::destroy(alloc, this);
            allocator_traits_type::deallocate(alloc, storage, 1);
        }

        void* target_address() noexcept override { return &func; }
        const void* target_address() const noexcept override { return &func; }
#if !RIN_CXX_FUNCTION_HAS_STATIC_TYPEINFO
        unsigned long long target_token() const noexcept override {
            return detail::functional_type_token<remove_cv_t<F>>();
        }
        const char* target_signature() const noexcept override {
            return detail::functional_type_signature<remove_cv_t<F>>();
        }
#endif
#if RIN_CXX_FUNCTION_HAS_STATIC_TYPEINFO
        const type_info& target_type() const noexcept override {
            return typeid(F);
        }
#else
        const type_info& target_type() const noexcept override {
            return detail::rin_no_rtti_type_info<F>();
        }
#endif
    };
    
    inline_storage_type m_storage;
    callable_base* m_callable;
    bool m_local;

    void destroy() noexcept {
        if (m_callable) {
            m_callable->destroy(m_local);
            m_callable = nullptr;
            m_local = false;
        }
    }

    void move_from(function&& other) noexcept {
        if (!other.m_callable) return;

        if (other.m_local) {
            m_callable = other.m_callable->move_into(&m_storage);
            m_local = true;
            other.destroy();
            return;
        }

        m_callable = other.m_callable;
        m_local = false;
        other.m_callable = nullptr;
        other.m_local = false;
    }

    template<typename F>
    void initialize_impl(F&& target, true_type) {
        using target_type = decay_t<F>;
        m_callable = new (&m_storage) callable<target_type>(
            std::forward<F>(target));
        m_local = true;
    }

    template<typename F>
    void initialize_impl(F&& target, false_type) {
        using target_type = decay_t<F>;
        m_callable = new callable<target_type>(std::forward<F>(target));
        m_local = false;
    }

    template<typename F>
    void initialize(F&& target) {
        using target_type = decay_t<F>;
        initialize_impl(std::forward<F>(target),
                        integral_constant<bool,
                            callable<target_type>::fits_inline()>());
    }

    template<typename F, typename Alloc>
    void initialize_with_allocator(F&& target, const Alloc& allocator) {
        using target_type = decay_t<F>;
        using callable_type = allocator_callable<target_type, Alloc>;
        using rebound_allocator = typename allocator_traits<Alloc>::template
            rebind_alloc<callable_type>;
        using rebound_traits = allocator_traits<rebound_allocator>;

        rebound_allocator rebound(allocator);
        using storage_pointer = typename rebound_traits::pointer;
        storage_pointer storage =
            rebound_traits::allocate(rebound, 1);
        callable_type* result = detail::functional_pointer_address(storage);
        if (storage == storage_pointer() || !result) {
            if (storage != storage_pointer())
                rebound_traits::deallocate(rebound, storage, 1);
            detail::functional_allocation_failure();
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            rebound_traits::construct(rebound, result,
                                      std::forward<F>(target), allocator);
        } catch (...) {
            rebound_traits::deallocate(rebound, storage, 1);
            throw;
        }
#else
        rebound_traits::construct(rebound, result,
                                  std::forward<F>(target), allocator);
#endif
        m_callable = result;
        m_local = false;
    }

    template<typename T>
    enable_if_t<!is_object<T>::value, T*> target_impl() noexcept {
        return nullptr;
    }

    template<typename T>
    enable_if_t<is_object<T>::value, T*> target_impl() noexcept {
        if (!m_callable) return nullptr;
#if RIN_CXX_FUNCTION_HAS_STATIC_TYPEINFO
        if (m_callable->target_type() != typeid(T)) return nullptr;
#else
        if (m_callable->target_token() !=
            detail::functional_type_token<remove_cv_t<T>>() ||
            !detail::functional_type_signature_equal(
                m_callable->target_signature(),
                detail::functional_type_signature<remove_cv_t<T>>())) {
            return nullptr;
        }
#endif
        return static_cast<T*>(m_callable->target_address());
    }

    template<typename T>
    enable_if_t<!is_object<T>::value, const T*> target_impl() const noexcept {
        return nullptr;
    }

    template<typename T>
    enable_if_t<is_object<T>::value, const T*> target_impl() const noexcept {
        if (!m_callable) return nullptr;
#if RIN_CXX_FUNCTION_HAS_STATIC_TYPEINFO
        if (m_callable->target_type() != typeid(T)) return nullptr;
#else
        if (m_callable->target_token() !=
            detail::functional_type_token<remove_cv_t<T>>() ||
            !detail::functional_type_signature_equal(
                m_callable->target_signature(),
                detail::functional_type_signature<remove_cv_t<T>>())) {
            return nullptr;
        }
#endif
        return static_cast<const T*>(m_callable->target_address());
    }

public:
    function() noexcept : m_callable(nullptr), m_local(false) {}
    function(nullptr_t) noexcept : m_callable(nullptr), m_local(false) {}
    
    /* Only accept callable types that return a type convertible to R */
    template<typename F,
             typename = enable_if_t<
                 !is_same<decay_t<F>, function>::value &&
                 is_copy_constructible<decay_t<F>>::value &&
                 is_invocable_r<R, F, Args...>::value
             >>
    function(F&& f) : m_callable(nullptr), m_local(false) {
        if (!detail::functional_is_null_target<decay_t<F>>(f)) {
            initialize(std::forward<F>(f));
        }
    }

#if __cplusplus < 201703L
    /* The allocator_arg constructor was removed from the standard function
     * surface in C++17.  Keep the C++11/14 form available for Rin consumers,
     * while ensuring non-conforming allocator and target types disappear via
     * SFINAE instead of producing a hard error. */
    template<typename Alloc, typename F,
             typename = enable_if_t<
                 allocator_target_compatible<
                     Alloc, allocator_callable<decay_t<F>, Alloc>>::value &&
                 !is_same<decay_t<F>, function>::value &&
                 is_copy_constructible<decay_t<F>>::value &&
                 is_invocable_r<R, F, Args...>::value>>
    function(allocator_arg_t, const Alloc& allocator, F&& f)
        : m_callable(nullptr), m_local(false) {
        if (!detail::functional_is_null_target<decay_t<F>>(f)) {
            initialize_with_allocator(std::forward<F>(f), allocator);
        }
    }
#endif
    
    function(const function& other) : m_callable(nullptr), m_local(false) {
        if (other.m_callable) {
            m_callable = other.m_callable->clone(&m_storage, m_local);
        }
    }
    
    function(function&& other) noexcept : m_callable(nullptr), m_local(false) {
        move_from(std::move(other));
    }
    
    ~function() {
        destroy();
    }
    
    function& operator=(const function& other) {
        if (this != &other) {
            function replacement(other);
            swap(replacement);
        }
        return *this;
    }
    
    function& operator=(function&& other) noexcept {
        if (this != &other) {
            destroy();
            move_from(std::move(other));
        }
        return *this;
    }
    
    function& operator=(nullptr_t) noexcept {
        destroy();
        return *this;
    }
    
    template<typename F,
             typename = enable_if_t<
                 !is_same<decay_t<F>, function>::value &&
                 is_copy_constructible<decay_t<F>>::value &&
                 is_invocable_r<R, F, Args...>::value
             >>
    function& operator=(F&& f) {
        function replacement(std::forward<F>(f));
        swap(replacement);
        return *this;
    }

    void swap(function& other) noexcept {
        if (this == &other) return;
        function temporary(std::move(*this));
        move_from(std::move(other));
        other.move_from(std::move(temporary));
    }
    
    explicit operator bool() const noexcept {
        return m_callable != nullptr;
    }

    const type_info& target_type() const noexcept {
#if RIN_CXX_FUNCTION_HAS_STATIC_TYPEINFO
        return m_callable ? m_callable->target_type() : typeid(void);
#else
        return m_callable ? m_callable->target_type()
                          : detail::rin_no_rtti_type_info<void>();
#endif
    }

    template<typename T>
    T* target() noexcept {
        return target_impl<T>();
    }

    template<typename T>
    const T* target() const noexcept {
        return target_impl<T>();
    }
    
    R operator()(Args... args) const {
        if (!m_callable) {
            detail::functional_empty_call();
        }
        return m_callable->invoke(std::forward<Args>(args)...);
    }
};

#if __cplusplus >= 201703L
/* C++17 class-template argument deduction.  A function pointer carries the
 * complete call signature, so preserve its result and argument types while
 * letting the ordinary function constructor own the target.  The noexcept
 * form is a distinct function-pointer type in C++17, but std::function's
 * erased signature intentionally remains the non-noexcept form. */
template<typename R, typename... Args>
function(R (*)(Args...)) -> function<R(Args...)>;

template<typename R, typename... Args>
function(R (*)(Args...) noexcept) -> function<R(Args...)>;

namespace detail {

template<typename>
struct function_callable_signature;

#define RIN_FUNCTION_CALLABLE_SIGNATURE(CVREF, NOEXCEPT_SPEC) \
template<typename R, typename C, typename... Args> \
struct function_callable_signature<R (C::*)(Args...) CVREF NOEXCEPT_SPEC> { \
    using type = R(Args...); \
};

RIN_FUNCTION_CALLABLE_SIGNATURE(, )
RIN_FUNCTION_CALLABLE_SIGNATURE(const, )
RIN_FUNCTION_CALLABLE_SIGNATURE(volatile, )
RIN_FUNCTION_CALLABLE_SIGNATURE(const volatile, )
RIN_FUNCTION_CALLABLE_SIGNATURE(&, )
RIN_FUNCTION_CALLABLE_SIGNATURE(const &, )
RIN_FUNCTION_CALLABLE_SIGNATURE(volatile &, )
RIN_FUNCTION_CALLABLE_SIGNATURE(const volatile &, )
RIN_FUNCTION_CALLABLE_SIGNATURE(&&, )
RIN_FUNCTION_CALLABLE_SIGNATURE(const &&, )
RIN_FUNCTION_CALLABLE_SIGNATURE(volatile &&, )
RIN_FUNCTION_CALLABLE_SIGNATURE(const volatile &&, )

RIN_FUNCTION_CALLABLE_SIGNATURE(, noexcept)
RIN_FUNCTION_CALLABLE_SIGNATURE(const, noexcept)
RIN_FUNCTION_CALLABLE_SIGNATURE(volatile, noexcept)
RIN_FUNCTION_CALLABLE_SIGNATURE(const volatile, noexcept)
RIN_FUNCTION_CALLABLE_SIGNATURE(&, noexcept)
RIN_FUNCTION_CALLABLE_SIGNATURE(const &, noexcept)
RIN_FUNCTION_CALLABLE_SIGNATURE(volatile &, noexcept)
RIN_FUNCTION_CALLABLE_SIGNATURE(const volatile &, noexcept)
RIN_FUNCTION_CALLABLE_SIGNATURE(&&, noexcept)
RIN_FUNCTION_CALLABLE_SIGNATURE(const &&, noexcept)
RIN_FUNCTION_CALLABLE_SIGNATURE(volatile &&, noexcept)
RIN_FUNCTION_CALLABLE_SIGNATURE(const volatile &&, noexcept)

#undef RIN_FUNCTION_CALLABLE_SIGNATURE

} /* namespace detail */

/* A non-generic callable object exposes a single non-overloaded
 * `operator()`.  Taking its address recovers the erased signature without
 * pretending that generic or overloaded call operators have one unique
 * deduction.  The helper is intentionally only available in C++17+, where
 * class-template argument deduction is part of the language. */
template<typename F>
function(F) -> function<typename detail::function_callable_signature<
    decltype(&F::operator())>::type>;
#endif

template<typename R, typename... Args>
void swap(function<R(Args...)>& left, function<R(Args...)>& right) noexcept
{
    left.swap(right);
}

#if __cplusplus > 202002L
/* C++23 move-only type erasure.  All signature qualifiers share one owner,
 * while the mode selects the cv/ref category used to invoke the stored target.
 * Keeping the qualifier in the erased node prevents a const wrapper from
 * accidentally calling a mutable-only target and makes noexcept signatures
 * reject potentially-throwing callables at construction. */
template<typename>
class move_only_function;

namespace detail {

template<int Mode, typename F>
struct move_only_function_target;

template<typename F>
struct move_only_function_target<0, F> { using type = F&; };

template<typename F>
struct move_only_function_target<1, F> { using type = const F&; };

template<typename F>
struct move_only_function_target<2, F> { using type = F&&; };

template<typename F>
struct move_only_function_target<3, F> { using type = const F&&; };

template<typename R, int CvRefMode, int InvMode, bool NoexceptSignature,
         typename F, typename... Args>
struct move_only_function_is_callable_from
    : integral_constant<bool,
          NoexceptSignature
              ? (is_nothrow_invocable_r<
                     R, typename move_only_function_target<
                            CvRefMode, F>::type, Args...>::value &&
                 is_nothrow_invocable_r<
                     R, typename move_only_function_target<
                            InvMode, F>::type, Args...>::value)
              : (is_invocable_r<
                     R, typename move_only_function_target<
                            CvRefMode, F>::type, Args...>::value &&
                 is_invocable_r<
                     R, typename move_only_function_target<
                            InvMode, F>::type, Args...>::value)> {};

template<typename T>
struct is_in_place_type_tag : false_type {};

template<typename T>
struct is_in_place_type_tag<in_place_type_t<T>> : true_type {};

template<int Mode, typename F, typename... CallArgs>
decltype(auto) move_only_function_invoke(F& function,
                                         CallArgs&&... args) {
    if constexpr (Mode == 0) {
        return std::invoke(function, std::forward<CallArgs>(args)...);
    } else if constexpr (Mode == 1) {
        return std::invoke(static_cast<const F&>(function),
                           std::forward<CallArgs>(args)...);
    } else if constexpr (Mode == 2) {
        return std::invoke(static_cast<F&&>(function),
                           std::forward<CallArgs>(args)...);
    } else {
        return std::invoke(static_cast<const F&&>(function),
                           std::forward<CallArgs>(args)...);
    }
}

template<typename R, int Mode, int CvRefMode, bool NoexceptSignature,
         typename Owner, typename... Args>
class move_only_function_storage {
    static constexpr size_t inline_storage_size = 3 * sizeof(void*);
    using inline_storage_type = aligned_storage_t<
        inline_storage_size, alignof(max_align_t)>;

    struct callable_base {
        virtual ~callable_base() = default;
        virtual R invoke(Args&&... args) = 0;
        virtual callable_base* move_inline(void* storage) noexcept = 0;
        virtual void destroy(bool local) noexcept = 0;
    };

    template<typename F>
    struct callable final : callable_base {
        F function_;

        template<typename... CtorArgs>
        explicit callable(CtorArgs&&... args)
            : function_(std::forward<CtorArgs>(args)...) {}

        R invoke(Args&&... args) override {
            if constexpr (is_void<R>::value) {
                move_only_function_invoke<Mode>(
                    function_, std::forward<Args>(args)...);
            } else {
                return move_only_function_invoke<Mode>(
                    function_, std::forward<Args>(args)...);
            }
        }

        static constexpr bool fits_inline() noexcept {
            return sizeof(callable) <= inline_storage_size &&
                alignof(callable) <= alignof(inline_storage_type) &&
                is_nothrow_move_constructible<F>::value;
        }

        callable_base* move_inline(void* storage) noexcept override {
            return new (storage) callable(std::move(function_));
        }

        void destroy(bool local) noexcept override {
            if (local) {
                this->~callable();
            } else {
                delete this;
            }
        }
    };

    callable_base* callable_;
    bool local_;
    inline_storage_type storage_;

    template<typename F, typename... CtorArgs>
    void construct(CtorArgs&&... args) {
        if constexpr (callable<F>::fits_inline()) {
            callable_ = new (&storage_)
                callable<F>(std::forward<CtorArgs>(args)...);
            local_ = true;
        } else {
            callable_ = new callable<F>(
                std::forward<CtorArgs>(args)...);
            local_ = false;
        }
    }

    void destroy() noexcept {
        if (callable_) {
            callable_->destroy(local_);
        }
        callable_ = nullptr;
        local_ = false;
    }

    void take_from(move_only_function_storage& other) noexcept {
        if (!other.callable_) {
            callable_ = nullptr;
            local_ = false;
            return;
        }

        if (other.local_) {
            callable_ = other.callable_->move_inline(&storage_);
            local_ = true;
            other.callable_->destroy(true);
        } else {
            callable_ = other.callable_;
            local_ = false;
        }
        other.callable_ = nullptr;
        other.local_ = false;
    }

public:
    using result_type = R;

    move_only_function_storage() noexcept
        : callable_(nullptr), local_(false), storage_{} {}
    move_only_function_storage(nullptr_t) noexcept
        : callable_(nullptr), local_(false), storage_{} {}

    template<typename F,
             typename = enable_if_t<
                 !is_same<remove_cvref_t<F>, Owner>::value &&
                 !is_in_place_type_tag<remove_cvref_t<F>>::value &&
                 is_constructible<decay_t<F>, F>::value &&
                 move_only_function_is_callable_from<
                     R, CvRefMode, Mode, NoexceptSignature,
                     decay_t<F>, Args...>::value>>
    explicit move_only_function_storage(F&& function)
        : callable_(nullptr), local_(false), storage_{} {
        using target_type = decay_t<F>;
        if (!detail::functional_is_null_target<target_type>(function)) {
            construct<target_type>(std::forward<F>(function));
        }
    }

    template<typename T, typename... CtorArgs,
             typename target_type = decay_t<T>,
             typename = enable_if_t<
                 is_same<target_type, T>::value &&
                 is_constructible<target_type, CtorArgs...>::value &&
                 move_only_function_is_callable_from<
                     R, CvRefMode, Mode, NoexceptSignature,
                     target_type, Args...>::value>>
    explicit move_only_function_storage(
        in_place_type_t<T>, CtorArgs&&... args)
        : callable_(nullptr), local_(false), storage_{} {
        construct<target_type>(std::forward<CtorArgs>(args)...);
    }

    template<typename T, typename U, typename... CtorArgs,
             typename target_type = decay_t<T>,
             typename = enable_if_t<
                 is_same<target_type, T>::value &&
                 is_constructible<target_type, initializer_list<U>&,
                                  CtorArgs...>::value &&
                 move_only_function_is_callable_from<
                     R, CvRefMode, Mode, NoexceptSignature,
                     target_type, Args...>::value>>
    explicit move_only_function_storage(
        in_place_type_t<T>, initializer_list<U> init_list,
        CtorArgs&&... args)
        : callable_(nullptr), local_(false), storage_{} {
        construct<target_type>(init_list,
                               std::forward<CtorArgs>(args)...);
    }

    move_only_function_storage(const move_only_function_storage&) = delete;
    move_only_function_storage& operator=(
        const move_only_function_storage&) = delete;

    move_only_function_storage(move_only_function_storage&& other) noexcept
        : callable_(nullptr), local_(false), storage_{} {
        take_from(other);
    }

    Owner& operator=(move_only_function_storage&& other) noexcept {
        if (this != &other) {
            destroy();
            take_from(other);
        }
        return static_cast<Owner&>(*this);
    }

    ~move_only_function_storage() { destroy(); }

    Owner& operator=(nullptr_t) noexcept {
        destroy();
        return static_cast<Owner&>(*this);
    }

    template<typename F,
             typename = enable_if_t<
                 !is_same<remove_cvref_t<F>, Owner>::value &&
                 !is_in_place_type_tag<remove_cvref_t<F>>::value &&
                 is_constructible<decay_t<F>, F>::value &&
                 move_only_function_is_callable_from<
                     R, CvRefMode, Mode, NoexceptSignature,
                     decay_t<F>, Args...>::value>>
    Owner& operator=(F&& function) {
        move_only_function_storage replacement(std::forward<F>(function));
        swap(replacement);
        return static_cast<Owner&>(*this);
    }

    void swap(move_only_function_storage& other) noexcept {
        move_only_function_storage temporary;
        temporary.take_from(*this);
        take_from(other);
        other.take_from(temporary);
    }

    explicit operator bool() const noexcept { return callable_ != nullptr; }

    R invoke(Args... args) const {
        if (!callable_) detail::functional_empty_call();
        return callable_->invoke(std::forward<Args>(args)...);
    }
};

template<typename Signature, typename Owner>
class move_only_function_signature;

#define RIN_MOVE_ONLY_FUNCTION_SIGNATURE(SIGNATURE, MODE, CVREF_MODE, NOEXCEPT_SPEC, QUALIFIER) \
template<typename R, typename... Args, typename Owner> \
class move_only_function_signature<SIGNATURE, Owner> \
    : public move_only_function_storage<R, MODE, CVREF_MODE, NOEXCEPT_SPEC, Owner, Args...> { \
    using base = move_only_function_storage<R, MODE, CVREF_MODE, NOEXCEPT_SPEC, Owner, Args...>; \
public: \
    using base::base; \
    using base::operator=; \
    R operator()(Args... args) QUALIFIER { \
        return this->invoke(std::forward<Args>(args)...); \
    } \
};

RIN_MOVE_ONLY_FUNCTION_SIGNATURE(R(Args...), 0, 2, false, )
RIN_MOVE_ONLY_FUNCTION_SIGNATURE(R(Args...) const, 1, 3, false, const)
RIN_MOVE_ONLY_FUNCTION_SIGNATURE(R(Args...) &, 0, 0, false, &)
RIN_MOVE_ONLY_FUNCTION_SIGNATURE(R(Args...) const &, 1, 1, false, const &)
RIN_MOVE_ONLY_FUNCTION_SIGNATURE(R(Args...) &&, 2, 2, false, &&)
RIN_MOVE_ONLY_FUNCTION_SIGNATURE(R(Args...) const &&, 3, 3, false, const &&)
RIN_MOVE_ONLY_FUNCTION_SIGNATURE(R(Args...) noexcept, 0, 2, true, noexcept)
RIN_MOVE_ONLY_FUNCTION_SIGNATURE(R(Args...) const noexcept, 1, 3, true, const noexcept)
RIN_MOVE_ONLY_FUNCTION_SIGNATURE(R(Args...) & noexcept, 0, 0, true, & noexcept)
RIN_MOVE_ONLY_FUNCTION_SIGNATURE(R(Args...) const & noexcept, 1, 1, true, const & noexcept)
RIN_MOVE_ONLY_FUNCTION_SIGNATURE(R(Args...) && noexcept, 2, 2, true, && noexcept)
RIN_MOVE_ONLY_FUNCTION_SIGNATURE(R(Args...) const && noexcept, 3, 3, true, const && noexcept)

#undef RIN_MOVE_ONLY_FUNCTION_SIGNATURE

} /* namespace detail */

template<typename Signature>
class move_only_function
    : public detail::move_only_function_signature<
          Signature, move_only_function<Signature>> {
    using base = detail::move_only_function_signature<
        Signature, move_only_function<Signature>>;
public:
    friend bool operator==(const move_only_function& function,
                           nullptr_t) noexcept {
        return !function;
    }

    using base::base;
    using base::operator=;
    move_only_function() noexcept = default;
    move_only_function(move_only_function&&) noexcept = default;
    move_only_function& operator=(move_only_function&&) noexcept = default;
    move_only_function(const move_only_function&) = delete;
    move_only_function& operator=(const move_only_function&) = delete;

    void swap(move_only_function& other) noexcept {
        base::swap(other);
    }
};

template<typename Signature>
void swap(move_only_function<Signature>& left,
          move_only_function<Signature>& right) noexcept {
    left.swap(right);
}
#endif

#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_FUNCTION_WRAPPERS)
/* C++26 copyable type erasure.  Unlike move_only_function this node must be
 * cloneable; copy-then-swap preserves the target when a copy throws. */
template<typename>
class copyable_function;

namespace detail {

template<typename R, int Mode, int CvRefMode, bool NoexceptSignature,
         typename Owner, typename... Args>
class copyable_function_storage {
    static constexpr size_t inline_storage_size = 3 * sizeof(void*);
    using inline_storage_type = aligned_storage_t<
        inline_storage_size, alignof(max_align_t)>;

    struct callable_base {
        virtual ~callable_base() = default;
        virtual callable_base* clone(void* storage, bool& local) const = 0;
        virtual callable_base* move_inline(void* storage) noexcept = 0;
        virtual void destroy(bool local) noexcept = 0;
        virtual R invoke(Args&&... args) = 0;
    };

    template<typename F>
    struct callable final : callable_base {
        F function_;

        template<typename... CtorArgs>
        explicit callable(CtorArgs&&... args)
            : function_(std::forward<CtorArgs>(args)...) {}

        static constexpr bool fits_inline() noexcept {
            return sizeof(callable) <= inline_storage_size &&
                alignof(callable) <= alignof(inline_storage_type) &&
                is_nothrow_move_constructible<F>::value;
        }

        callable_base* clone(void* storage, bool& local) const override {
            if constexpr (fits_inline()) {
                local = true;
                return new (storage) callable(function_);
            } else {
                local = false;
                return new callable(function_);
            }
        }

        callable_base* move_inline(void* storage) noexcept override {
            return new (storage) callable(std::move(function_));
        }

        void destroy(bool local) noexcept override {
            if (local) {
                this->~callable();
            } else {
                delete this;
            }
        }

        R invoke(Args&&... args) override {
            if constexpr (is_void<R>::value) {
                move_only_function_invoke<Mode>(
                    function_, std::forward<Args>(args)...);
            } else {
                return move_only_function_invoke<Mode>(
                    function_, std::forward<Args>(args)...);
            }
        }
    };

    callable_base* callable_;
    bool local_;
    inline_storage_type storage_;

    template<typename F, typename... CtorArgs>
    void construct(CtorArgs&&... args) {
        if constexpr (callable<F>::fits_inline()) {
            callable_ = new (&storage_)
                callable<F>(std::forward<CtorArgs>(args)...);
            local_ = true;
        } else {
            callable_ = new callable<F>(
                std::forward<CtorArgs>(args)...);
            local_ = false;
        }
    }

    void destroy() noexcept {
        if (callable_) callable_->destroy(local_);
        callable_ = nullptr;
        local_ = false;
    }

    void take_from(copyable_function_storage& other) noexcept {
        if (!other.callable_) {
            callable_ = nullptr;
            local_ = false;
            return;
        }
        if (other.local_) {
            callable_ = other.callable_->move_inline(&storage_);
            local_ = true;
            other.callable_->destroy(true);
        } else {
            callable_ = other.callable_;
            local_ = false;
        }
        other.callable_ = nullptr;
        other.local_ = false;
    }

public:
    using result_type = R;

    copyable_function_storage() noexcept
        : callable_(nullptr), local_(false), storage_{} {}
    copyable_function_storage(nullptr_t) noexcept
        : callable_(nullptr), local_(false), storage_{} {}

    template<typename F,
             typename = enable_if_t<
                 !is_same<remove_cvref_t<F>, Owner>::value &&
                 !is_in_place_type_tag<remove_cvref_t<F>>::value &&
                 is_constructible<decay_t<F>, F>::value &&
                 is_copy_constructible<decay_t<F>>::value &&
                 move_only_function_is_callable_from<
                     R, CvRefMode, Mode, NoexceptSignature,
                     decay_t<F>, Args...>::value>>
    copyable_function_storage(F&& function)
        : callable_(nullptr), local_(false), storage_{} {
        using target_type = decay_t<F>;
        if (!functional_is_null_target<target_type>(function)) {
            construct<target_type>(std::forward<F>(function));
        }
    }

    template<typename T, typename... CtorArgs,
             typename target_type = decay_t<T>,
             typename = enable_if_t<
                 is_same<target_type, T>::value &&
                 is_constructible<target_type, CtorArgs...>::value &&
                 is_copy_constructible<target_type>::value &&
                 move_only_function_is_callable_from<
                     R, CvRefMode, Mode, NoexceptSignature,
                     target_type, Args...>::value>>
    explicit copyable_function_storage(
        in_place_type_t<T>, CtorArgs&&... args)
        : callable_(nullptr), local_(false), storage_{} {
        construct<target_type>(std::forward<CtorArgs>(args)...);
    }

    template<typename T, typename U, typename... CtorArgs,
             typename target_type = decay_t<T>,
             typename = enable_if_t<
                 is_same<target_type, T>::value &&
                 is_constructible<target_type, initializer_list<U>&,
                                  CtorArgs...>::value &&
                 is_copy_constructible<target_type>::value &&
                 move_only_function_is_callable_from<
                     R, CvRefMode, Mode, NoexceptSignature,
                     target_type, Args...>::value>>
    explicit copyable_function_storage(
        in_place_type_t<T>, initializer_list<U> init_list,
        CtorArgs&&... args)
        : callable_(nullptr), local_(false), storage_{} {
        construct<target_type>(init_list,
                               std::forward<CtorArgs>(args)...);
    }

    copyable_function_storage(const copyable_function_storage& other)
        : callable_(nullptr), local_(false), storage_{} {
        if (other.callable_) {
            callable_ = other.callable_->clone(&storage_, local_);
        }
    }

    copyable_function_storage(copyable_function_storage&& other) noexcept
        : callable_(nullptr), local_(false), storage_{} {
        take_from(other);
    }

    copyable_function_storage& operator=(
        const copyable_function_storage& other) {
        if (this != &other) {
            copyable_function_storage replacement(other);
            swap(replacement);
        }
        return *this;
    }

    copyable_function_storage& operator=(
        copyable_function_storage&& other) noexcept {
        if (this != &other) {
            destroy();
            take_from(other);
        }
        return *this;
    }

    ~copyable_function_storage() { destroy(); }

    Owner& operator=(nullptr_t) noexcept {
        destroy();
        return static_cast<Owner&>(*this);
    }

    template<typename F,
             typename = enable_if_t<
                 !is_same<remove_cvref_t<F>, Owner>::value &&
                 !is_in_place_type_tag<remove_cvref_t<F>>::value &&
                 is_constructible<decay_t<F>, F>::value &&
                 is_copy_constructible<decay_t<F>>::value &&
                 move_only_function_is_callable_from<
                     R, CvRefMode, Mode, NoexceptSignature,
                     decay_t<F>, Args...>::value>>
    Owner& operator=(F&& function) {
        copyable_function_storage replacement(std::forward<F>(function));
        swap(replacement);
        return static_cast<Owner&>(*this);
    }

    void swap(copyable_function_storage& other) noexcept {
        copyable_function_storage temporary;
        temporary.take_from(*this);
        take_from(other);
        other.take_from(temporary);
    }

    explicit operator bool() const noexcept { return callable_ != nullptr; }

    R invoke(Args... args) const {
        if (!callable_) functional_empty_call();
        return callable_->invoke(std::forward<Args>(args)...);
    }
};

template<typename Signature, typename Owner>
class copyable_function_signature;

#define RIN_COPYABLE_FUNCTION_SIGNATURE(SIGNATURE, MODE, CVREF_MODE, NOEXCEPT_SPEC, QUALIFIER) \
template<typename R, typename... Args, typename Owner> \
class copyable_function_signature<SIGNATURE, Owner> \
    : public copyable_function_storage<R, MODE, CVREF_MODE, NOEXCEPT_SPEC, Owner, Args...> { \
    using base = copyable_function_storage<R, MODE, CVREF_MODE, NOEXCEPT_SPEC, Owner, Args...>; \
public: \
    using base::base; \
    using base::operator=; \
    R operator()(Args... args) QUALIFIER { \
        return this->invoke(std::forward<Args>(args)...); \
    } \
};

RIN_COPYABLE_FUNCTION_SIGNATURE(R(Args...), 0, 0, false, )
RIN_COPYABLE_FUNCTION_SIGNATURE(R(Args...) const, 1, 1, false, const)
RIN_COPYABLE_FUNCTION_SIGNATURE(R(Args...) &, 0, 0, false, &)
RIN_COPYABLE_FUNCTION_SIGNATURE(R(Args...) const &, 1, 1, false, const &)
RIN_COPYABLE_FUNCTION_SIGNATURE(R(Args...) &&, 2, 2, false, &&)
RIN_COPYABLE_FUNCTION_SIGNATURE(R(Args...) const &&, 3, 3, false, const &&)
RIN_COPYABLE_FUNCTION_SIGNATURE(R(Args...) noexcept, 0, 0, true, noexcept)
RIN_COPYABLE_FUNCTION_SIGNATURE(R(Args...) const noexcept, 1, 1, true, const noexcept)
RIN_COPYABLE_FUNCTION_SIGNATURE(R(Args...) & noexcept, 0, 0, true, & noexcept)
RIN_COPYABLE_FUNCTION_SIGNATURE(R(Args...) const & noexcept, 1, 1, true, const & noexcept)
RIN_COPYABLE_FUNCTION_SIGNATURE(R(Args...) && noexcept, 2, 2, true, && noexcept)
RIN_COPYABLE_FUNCTION_SIGNATURE(R(Args...) const && noexcept, 3, 3, true, const && noexcept)

#undef RIN_COPYABLE_FUNCTION_SIGNATURE

} /* namespace detail */

template<typename Signature>
class copyable_function
    : public detail::copyable_function_signature<
          Signature, copyable_function<Signature>> {
    using base = detail::copyable_function_signature<
        Signature, copyable_function<Signature>>;
public:
    friend bool operator==(const copyable_function& function,
                           nullptr_t) noexcept {
        return !function;
    }

    using base::base;
    using base::operator=;
    copyable_function() noexcept = default;
    copyable_function(const copyable_function&) = default;
    copyable_function(copyable_function&&) noexcept = default;
    copyable_function& operator=(const copyable_function&) = default;
    copyable_function& operator=(copyable_function&&) = default;

    void swap(copyable_function& other) noexcept { base::swap(other); }
};

template<typename Signature>
void swap(copyable_function<Signature>& left,
          copyable_function<Signature>& right) noexcept {
    left.swap(right);
}

/* Non-owning type erasure stores object, constant, and function addresses in
 * a trivially copyable bound entity plus a typed invocation thunk. */
template<typename>
class function_ref;

namespace detail {

struct function_ref_bound_entity {
    void* object;
    const void* constant;
    void (*function)();
};

constexpr function_ref_bound_entity function_ref_bind_object(
    void* object) noexcept {
    return {object, nullptr, nullptr};
}

constexpr function_ref_bound_entity function_ref_bind_function(
    void (*function)()) noexcept {
    return {nullptr, nullptr, function};
}

constexpr function_ref_bound_entity function_ref_bind_constant(
    const void* constant) noexcept {
    return {nullptr, constant, nullptr};
}

constexpr function_ref_bound_entity function_ref_bind_object_and_constant(
    void* object, const void* constant) noexcept {
    return {object, constant, nullptr};
}

template<auto Value, typename F>
struct function_ref_constant_storage {
    static constexpr F value = Value;
};

template<auto Value, typename F>
constexpr const void* function_ref_constant_address() noexcept {
    return static_cast<const void*>(
        std::addressof(function_ref_constant_storage<Value, F>::value));
}

template<typename R, int CvMode, bool NoexceptSignature,
         typename Owner, typename... Args>
class function_ref_storage;

template<typename Signature>
struct function_ref_signature_traits;

template<typename R, typename... Args>
struct function_ref_signature_traits<function_ref<R(Args...)>> {
    using function_type = R(Args...);
    using object_cvref = int&;
};

template<typename R, typename... Args>
struct function_ref_signature_traits<function_ref<R(Args...) const>> {
    using function_type = R(Args...);
    using object_cvref = const int&;
};

template<typename R, typename... Args>
struct function_ref_signature_traits<function_ref<R(Args...) noexcept>> {
    using function_type = R(Args...) noexcept;
    using object_cvref = int&;
};

template<typename R, typename... Args>
struct function_ref_signature_traits<
    function_ref<R(Args...) const noexcept>> {
    using function_type = R(Args...) noexcept;
    using object_cvref = const int&;
};

template<typename To, typename From, typename = void>
struct function_ref_is_compatible : false_type {};

template<typename To, typename From>
struct function_ref_is_compatible<
    To, From,
    void_t<typename function_ref_signature_traits<To>::function_type,
           typename function_ref_signature_traits<From>::function_type>>
    : integral_constant<bool,
          is_convertible<
              typename function_ref_signature_traits<From>::function_type&,
              typename function_ref_signature_traits<To>::function_type&>::value &&
          is_convertible<
              typename function_ref_signature_traits<To>::object_cvref,
              typename function_ref_signature_traits<From>::object_cvref>::value> {};

template<typename T>
struct is_constant_wrapper : false_type {};

template<auto Value, typename T>
struct is_constant_wrapper<constant_wrapper<Value, T>> : true_type {};

template<typename R, bool NoexceptSignature, typename F, typename... Args>
struct function_ref_invocable_from
    : integral_constant<bool,
          NoexceptSignature
              ? is_nothrow_invocable_r<R, F, Args...>::value
              : is_invocable_r<R, F, Args...>::value> {};

template<typename R, int CvMode, bool NoexceptSignature,
         typename F, typename... Args>
struct function_ref_is_callable
    : move_only_function_is_callable_from<
          R, CvMode, CvMode, NoexceptSignature, F, Args...> {};

template<typename F>
struct function_ref_member_call_signature;

#define RIN_FUNCTION_REF_MEMBER_SIGNATURE(CVREF, NOEXCEPT_SPEC) \
template<typename R, typename C, typename... Args> \
struct function_ref_member_call_signature< \
    R (C::*)(Args...) CVREF NOEXCEPT_SPEC> { \
    using type = R(Args...) NOEXCEPT_SPEC; \
};

RIN_FUNCTION_REF_MEMBER_SIGNATURE(, )
RIN_FUNCTION_REF_MEMBER_SIGNATURE(const, )
RIN_FUNCTION_REF_MEMBER_SIGNATURE(volatile, )
RIN_FUNCTION_REF_MEMBER_SIGNATURE(const volatile, )
RIN_FUNCTION_REF_MEMBER_SIGNATURE(&, )
RIN_FUNCTION_REF_MEMBER_SIGNATURE(const &, )
RIN_FUNCTION_REF_MEMBER_SIGNATURE(volatile &, )
RIN_FUNCTION_REF_MEMBER_SIGNATURE(const volatile &, )
RIN_FUNCTION_REF_MEMBER_SIGNATURE(&&, )
RIN_FUNCTION_REF_MEMBER_SIGNATURE(const &&, )
RIN_FUNCTION_REF_MEMBER_SIGNATURE(volatile &&, )
RIN_FUNCTION_REF_MEMBER_SIGNATURE(const volatile &&, )
RIN_FUNCTION_REF_MEMBER_SIGNATURE(, noexcept)
RIN_FUNCTION_REF_MEMBER_SIGNATURE(const, noexcept)
RIN_FUNCTION_REF_MEMBER_SIGNATURE(volatile, noexcept)
RIN_FUNCTION_REF_MEMBER_SIGNATURE(const volatile, noexcept)
RIN_FUNCTION_REF_MEMBER_SIGNATURE(&, noexcept)
RIN_FUNCTION_REF_MEMBER_SIGNATURE(const &, noexcept)
RIN_FUNCTION_REF_MEMBER_SIGNATURE(volatile &, noexcept)
RIN_FUNCTION_REF_MEMBER_SIGNATURE(const volatile &, noexcept)
RIN_FUNCTION_REF_MEMBER_SIGNATURE(&&, noexcept)
RIN_FUNCTION_REF_MEMBER_SIGNATURE(const &&, noexcept)
RIN_FUNCTION_REF_MEMBER_SIGNATURE(volatile &&, noexcept)
RIN_FUNCTION_REF_MEMBER_SIGNATURE(const volatile &&, noexcept)

#undef RIN_FUNCTION_REF_MEMBER_SIGNATURE

template<typename F, typename T, typename = void>
struct function_ref_constant_bound_signature {};

template<typename R, typename G, typename... Args, typename T>
struct function_ref_constant_bound_signature<R (*)(G, Args...), T, void> {
    using type = R(Args...);
};

template<typename R, typename G, typename... Args, typename T>
struct function_ref_constant_bound_signature<
    R (*)(G, Args...) noexcept, T, void> {
    using type = R(Args...) noexcept;
};

template<typename F, typename T>
struct function_ref_constant_bound_signature<
    F, T, enable_if_t<is_member_function_pointer<F>::value>> {
    using type = typename function_ref_member_call_signature<F>::type;
};

template<typename F, typename T>
struct function_ref_constant_bound_signature<
    F, T, enable_if_t<is_member_object_pointer<F>::value,
                      void_t<invoke_result_t<F, T&>>>> {
    using type = invoke_result_t<F, T&>() noexcept;
};

template<typename R, int CvMode, bool NoexceptSignature,
         typename Owner, typename... Args>
class function_ref_storage {
    using bound_entity = function_ref_bound_entity;
    using thunk_type = R (*)(bound_entity, Args&&...) noexcept(NoexceptSignature);

    template<typename, int, bool, typename, typename...>
    friend class function_ref_storage;

    bound_entity bound_;
    thunk_type thunk_;

    template<typename T>
    static R object_thunk(bound_entity bound, Args&&... args)
        noexcept(NoexceptSignature) {
        using object_type = remove_reference_t<
            typename move_only_function_target<CvMode, T>::type>;
        object_type& object = *static_cast<object_type*>(bound.object);
        if constexpr (is_void<R>::value) {
            std::invoke(object, std::forward<Args>(args)...);
        } else {
            return std::invoke(object, std::forward<Args>(args)...);
        }
    }

    template<typename F>
    static R function_thunk(bound_entity bound, Args&&... args)
        noexcept(NoexceptSignature) {
        using function_pointer = F*;
        auto function = reinterpret_cast<function_pointer>(bound.function);
        if constexpr (is_void<R>::value) {
            std::invoke(function, std::forward<Args>(args)...);
        } else {
            return std::invoke(function, std::forward<Args>(args)...);
        }
    }

    template<typename F>
    static R constant_thunk(bound_entity bound, Args&&... args)
        noexcept(NoexceptSignature) {
        const F& function = *static_cast<const F*>(bound.constant);
        if constexpr (is_void<R>::value) {
            std::invoke(function, std::forward<Args>(args)...);
        } else {
            return std::invoke(function, std::forward<Args>(args)...);
        }
    }

    template<typename F, typename T>
    static R constant_object_thunk(bound_entity bound, Args&&... args)
        noexcept(NoexceptSignature) {
        const F& function = *static_cast<const F*>(bound.constant);
        using object_type = remove_reference_t<
            typename move_only_function_target<CvMode, T>::type>;
        object_type& object = *static_cast<object_type*>(bound.object);
        if constexpr (is_void<R>::value) {
            std::invoke(function, object, std::forward<Args>(args)...);
        } else {
            return std::invoke(function, object, std::forward<Args>(args)...);
        }
    }

    template<typename F, typename T>
    static R constant_pointer_thunk(bound_entity bound, Args&&... args)
        noexcept(NoexceptSignature) {
        const F& function = *static_cast<const F*>(bound.constant);
        using object_type = remove_reference_t<
            typename move_only_function_target<CvMode, T>::type>;
        auto object = static_cast<object_type*>(bound.object);
        if constexpr (is_void<R>::value) {
            std::invoke(function, object, std::forward<Args>(args)...);
        } else {
            return std::invoke(function, object, std::forward<Args>(args)...);
        }
    }

    template<typename F>
    static constexpr bool callable() {
        return move_only_function_is_callable_from<
            R, CvMode, CvMode, NoexceptSignature, F, Args...>::value;
    }

public:
    template<typename F, typename T = remove_reference_t<F>,
             typename = enable_if_t<
                 !is_same<remove_cvref_t<F>, Owner>::value &&
                 !is_constant_wrapper<remove_cvref_t<F>>::value &&
                 !is_function<T>::value &&
                 !is_member_pointer<T>::value &&
                 callable<T>()>>
    constexpr function_ref_storage(F&& function) noexcept
        : bound_(function_ref_bind_object(const_cast<void*>(
              static_cast<const volatile void*>(std::addressof(function))))),
          thunk_(nullptr) {
        using source_type = remove_cv_t<T>;
        if constexpr (function_ref_is_compatible<
                          Owner, source_type>::value) {
            const source_type& source = function;
            bound_ = source.bound_;
            thunk_ = source.thunk_;
        } else {
            thunk_ = &object_thunk<T>;
        }
    }

    template<typename F,
             typename = enable_if_t<is_function<F>::value && callable<F>()>>
    function_ref_storage(F* function) noexcept
        : bound_(function_ref_bind_function(
              reinterpret_cast<void (*)()>(function))),
          thunk_(&function_thunk<F>) {}

    template<auto Value, typename F,
             typename = enable_if_t<callable<const F&>()>>
    constexpr function_ref_storage(constant_wrapper<Value, F>) noexcept
        : bound_(function_ref_bind_constant(
              function_ref_constant_address<Value, F>())),
          thunk_(&constant_thunk<F>) {}

    template<auto Value, typename F, typename U,
             typename T = remove_reference_t<U>,
             typename = enable_if_t<
                 is_lvalue_reference<U&&>::value &&
                 !is_pointer<remove_cvref_t<U>>::value &&
                 function_ref_invocable_from<
                     R, NoexceptSignature, const F&,
                     typename move_only_function_target<
                         CvMode, T>::type, Args...>::value>>
    constexpr function_ref_storage(constant_wrapper<Value, F>,
                                   U&& object) noexcept
        : bound_(function_ref_bind_object_and_constant(
              const_cast<void*>(static_cast<const volatile void*>(
                  std::addressof(object))),
              function_ref_constant_address<Value, F>())),
          thunk_(&constant_object_thunk<F, T>) {}

    template<auto Value, typename F, typename T,
             typename = enable_if_t<
                 function_ref_invocable_from<
                     R, NoexceptSignature, const F&,
                     typename move_only_function_target<
                         CvMode, T>::type*, Args...>::value>>
    constexpr function_ref_storage(constant_wrapper<Value, F>,
                                   T* object) noexcept
        : bound_(function_ref_bind_object_and_constant(
              const_cast<void*>(static_cast<const volatile void*>(object)),
              function_ref_constant_address<Value, F>())),
          thunk_(&constant_pointer_thunk<F, T>) {}

    constexpr function_ref_storage(const function_ref_storage&) noexcept = default;
    constexpr function_ref_storage& operator=(
        const function_ref_storage&) noexcept = default;

    template<typename T,
             typename = enable_if_t<
                 !function_ref_is_compatible<Owner, remove_cvref_t<T>>::value &&
                 !is_pointer<T>::value &&
                 !is_constant_wrapper<remove_cvref_t<T>>::value>>
    function_ref_storage& operator=(T) = delete;

    R invoke(Args... args) const noexcept(NoexceptSignature) {
        return thunk_(bound_, std::forward<Args>(args)...);
    }
};

} /* namespace detail */

#define RIN_FUNCTION_REF_CLASS(SIGNATURE, CVMODE, NOEXCEPT_VALUE, QUALIFIER) \
template<typename R, typename... Args> \
class function_ref<SIGNATURE> \
    : public detail::function_ref_storage< \
          R, CVMODE, NOEXCEPT_VALUE, function_ref<SIGNATURE>, Args...> { \
    using owner = function_ref<SIGNATURE>; \
    using base = detail::function_ref_storage< \
        R, CVMODE, NOEXCEPT_VALUE, owner, Args...>; \
    template<typename> friend class function_ref; \
public: \
    template<typename F, typename T = remove_reference_t<F>, \
             typename = enable_if_t< \
                 !is_same<remove_cvref_t<F>, owner>::value && \
                 !detail::is_constant_wrapper<remove_cvref_t<F>>::value && \
                 !is_function<T>::value && !is_member_pointer<T>::value && \
                 detail::function_ref_is_callable< \
                     R, CVMODE, NOEXCEPT_VALUE, T, Args...>::value>> \
    constexpr function_ref(F&& function) noexcept \
        : base(std::forward<F>(function)) {} \
    template<typename F, typename = enable_if_t< \
        is_function<F>::value && detail::function_ref_is_callable< \
            R, CVMODE, NOEXCEPT_VALUE, F, Args...>::value>> \
    function_ref(F* function) noexcept : base(function) {} \
    template<auto Value, typename F, typename = enable_if_t< \
        detail::function_ref_is_callable< \
            R, CVMODE, NOEXCEPT_VALUE, const F&, Args...>::value>> \
    constexpr function_ref(constant_wrapper<Value, F> function) noexcept \
        : base(function) {} \
    template<auto Value, typename F, typename U, \
             typename T = remove_reference_t<U>, \
             typename = enable_if_t< \
                 is_lvalue_reference<U&&>::value && \
                 !is_pointer<remove_cvref_t<U>>::value && \
                 detail::function_ref_invocable_from< \
                     R, NOEXCEPT_VALUE, const F&, \
                     typename detail::move_only_function_target< \
                         CVMODE, T>::type, Args...>::value>> \
    constexpr function_ref(constant_wrapper<Value, F> function, \
                           U&& object) noexcept \
        : base(function, std::forward<U>(object)) {} \
    template<auto Value, typename F, typename T, \
             typename = enable_if_t< \
                 detail::function_ref_invocable_from< \
                     R, NOEXCEPT_VALUE, const F&, \
                     typename detail::move_only_function_target< \
                         CVMODE, T>::type*, Args...>::value>> \
    constexpr function_ref(constant_wrapper<Value, F> function, \
                           T* object) noexcept : base(function, object) {} \
    constexpr function_ref(const function_ref&) noexcept = default; \
    constexpr function_ref& operator=(const function_ref&) noexcept = default; \
    R operator()(Args... args) const noexcept(NOEXCEPT_VALUE) { \
        return this->invoke(std::forward<Args>(args)...); \
    } \
    template<typename T, typename = enable_if_t< \
        !detail::function_ref_is_compatible< \
            owner, remove_cvref_t<T>>::value && \
        !is_pointer<T>::value && \
        !detail::is_constant_wrapper<remove_cvref_t<T>>::value>> \
    function_ref& operator=(T) = delete; \
};

RIN_FUNCTION_REF_CLASS(R(Args...), 0, false, )
RIN_FUNCTION_REF_CLASS(R(Args...) const, 1, false, const)
RIN_FUNCTION_REF_CLASS(R(Args...) noexcept, 0, true, noexcept)
RIN_FUNCTION_REF_CLASS(R(Args...) const noexcept, 1, true, const noexcept)

#undef RIN_FUNCTION_REF_CLASS

template<typename F>
function_ref(F*) -> function_ref<F>;

template<auto Value, typename F,
         enable_if_t<is_pointer<F>::value &&
                     is_function<remove_pointer_t<F>>::value, int> = 0>
function_ref(constant_wrapper<Value, F>) -> function_ref<remove_pointer_t<F>>;

template<auto Value, typename F, typename T>
function_ref(constant_wrapper<Value, F>, T&&)
    -> function_ref<typename detail::function_ref_constant_bound_signature<
        F, remove_reference_t<T>>::type>;
#endif

/* C++11/14/17 expose the nullable function comparisons as non-members.
 * They were removed in C++20, so keep the old surface behind the language
 * mode gate instead of leaking a legacy overload into newer programs. */
#if __cplusplus < 202002L
template<typename R, typename... Args>
bool operator==(const function<R(Args...)>& target, nullptr_t) noexcept {
    return !target;
}

template<typename R, typename... Args>
bool operator==(nullptr_t, const function<R(Args...)>& target) noexcept {
    return !target;
}

template<typename R, typename... Args>
bool operator!=(const function<R(Args...)>& target, nullptr_t) noexcept {
    return static_cast<bool>(target);
}

template<typename R, typename... Args>
bool operator!=(nullptr_t, const function<R(Args...)>& target) noexcept {
    return static_cast<bool>(target);
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * reference_wrapper
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
class reference_wrapper {
    T* m_ptr;
public:
    using type = T;
    
    constexpr reference_wrapper(T& ref) noexcept : m_ptr(&ref) {}
    reference_wrapper(const reference_wrapper&) noexcept = default;
    
    reference_wrapper& operator=(const reference_wrapper&) noexcept = default;
    
    constexpr operator T&() const noexcept { return *m_ptr; }
    constexpr T& get() const noexcept { return *m_ptr; }

    template<typename... Args>
    constexpr invoke_result_t<T&, Args...>
    operator()(Args&&... args) const
        noexcept(is_nothrow_invocable<T&, Args...>::value) {
        return std::invoke(get(), std::forward<Args>(args)...);
    }
};

#if __cplusplus >= 201703L
template<typename T>
reference_wrapper(T&) -> reference_wrapper<T>;
#endif

template<typename T>
constexpr reference_wrapper<T> ref(T& t) noexcept {
    return reference_wrapper<T>(t);
}

template<typename T>
constexpr reference_wrapper<T> ref(reference_wrapper<T> wrapper) noexcept {
    return wrapper;
}

template<typename T>
void ref(const T&&) = delete;

template<typename T>
constexpr reference_wrapper<const T> cref(const T& t) noexcept {
    return reference_wrapper<const T>(t);
}

template<typename T>
constexpr reference_wrapper<const T> cref(reference_wrapper<T> wrapper) noexcept {
    return reference_wrapper<const T>(wrapper.get());
}

template<typename T>
void cref(const T&&) = delete;

/* ═══════════════════════════════════════════════════════════════
 * mem_fn (C++11)
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

template<typename Member>
class mem_fn_wrapper {
    Member member_;

public:
    constexpr explicit mem_fn_wrapper(Member member) noexcept
        : member_(member) {}

    template<typename... Args>
    constexpr auto operator()(Args&&... args) const
        noexcept(noexcept(std::invoke(member_, std::forward<Args>(args)...)))
        -> decltype(std::invoke(member_, std::forward<Args>(args)...)) {
        return std::invoke(member_, std::forward<Args>(args)...);
    }
};

} /* namespace detail */

template<typename Member, typename Class>
constexpr detail::mem_fn_wrapper<Member Class::*>
mem_fn(Member Class::* member) noexcept {
    return detail::mem_fn_wrapper<Member Class::*>(member);
}

/* ═══════════════════════════════════════════════════════════════
 * bind
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

template<int Index>
struct functional_placeholder {};

template<typename Callable, typename... Bound>
class functional_bind_wrapper;

template<typename Result, typename Inner>
class functional_bind_result_wrapper;

template<typename...>
struct functional_bind_make_void {
    using type = void;
};

template<typename... Types>
using functional_bind_void_t = typename functional_bind_make_void<Types...>::type;

/* C++11 bind exposes result_type only when its decayed callable does.  Keep
 * this as a separate public base so absence stays SFINAE-observable. */
template<typename Callable, typename = void>
struct functional_bind_result_type {};

template<typename Callable>
struct functional_bind_result_type<
    Callable, functional_bind_void_t<typename Callable::result_type>> {
    using result_type = typename Callable::result_type;
};

} /* namespace detail */

namespace placeholders {
    static constexpr detail::functional_placeholder<1> _1{};
    static constexpr detail::functional_placeholder<2> _2{};
    static constexpr detail::functional_placeholder<3> _3{};
    static constexpr detail::functional_placeholder<4> _4{};
    static constexpr detail::functional_placeholder<5> _5{};
    static constexpr detail::functional_placeholder<6> _6{};
    static constexpr detail::functional_placeholder<7> _7{};
    static constexpr detail::functional_placeholder<8> _8{};
    static constexpr detail::functional_placeholder<9> _9{};
    static constexpr detail::functional_placeholder<10> _10{};
}

template<typename T>
struct is_placeholder : integral_constant<int, 0> {};

template<int Index>
struct is_placeholder<detail::functional_placeholder<Index>>
    : integral_constant<int, Index> {};

template<typename T>
struct is_placeholder<const T> : is_placeholder<T> {};

template<typename T>
struct is_placeholder<volatile T> : is_placeholder<T> {};

template<typename T>
struct is_placeholder<const volatile T> : is_placeholder<T> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr int is_placeholder_v = is_placeholder<T>::value;
#endif

template<typename T>
struct is_bind_expression : false_type {};

template<typename Callable, typename... Bound>
struct is_bind_expression<detail::functional_bind_wrapper<Callable, Bound...>>
    : true_type {};

template<typename Result, typename Inner>
struct is_bind_expression<
    detail::functional_bind_result_wrapper<Result, Inner>> : true_type {};

template<typename T>
struct is_bind_expression<const T> : is_bind_expression<T> {};

template<typename T>
struct is_bind_expression<volatile T> : is_bind_expression<T> {};

template<typename T>
struct is_bind_expression<const volatile T> : is_bind_expression<T> {};

#if __cplusplus >= 201703L
template<typename T>
inline constexpr bool is_bind_expression_v = is_bind_expression<T>::value;
#endif

namespace detail {

template<size_t Index, typename T>
struct functional_bind_leaf {
    T value;

    template<typename U,
             enable_if_t<
                 !is_same<remove_cvref_t<U>, functional_bind_leaf>::value &&
                 is_constructible<T, U&&>::value,
                 int> = 0>
    constexpr explicit functional_bind_leaf(U&& input)
        : value(std::forward<U>(input)) {}
};

template<typename Indexes, typename... Types>
struct functional_bind_storage;

template<size_t... Indexes, typename... Types>
struct functional_bind_storage<index_sequence<Indexes...>, Types...>
    : functional_bind_leaf<Indexes, Types>... {
    static constexpr size_t size = sizeof...(Types);

    template<typename... Inputs,
             enable_if_t<
                 sizeof...(Inputs) == sizeof...(Types) &&
                 conjunction<is_constructible<Types, Inputs&&>...>::value,
                 int> = 0>
    constexpr explicit functional_bind_storage(Inputs&&... inputs)
        : functional_bind_leaf<Indexes, Types>(
              std::forward<Inputs>(inputs))... {}
};

template<size_t Index, typename T>
constexpr functional_bind_leaf<Index, T>&
functional_bind_select(functional_bind_leaf<Index, T>& leaf) noexcept {
    return leaf;
}

template<size_t Index, typename T>
constexpr const functional_bind_leaf<Index, T>&
functional_bind_select(const functional_bind_leaf<Index, T>& leaf) noexcept {
    return leaf;
}

template<size_t Index, typename T>
constexpr volatile functional_bind_leaf<Index, T>&
functional_bind_select(volatile functional_bind_leaf<Index, T>& leaf) noexcept {
    return leaf;
}

template<size_t Index, typename T>
constexpr const volatile functional_bind_leaf<Index, T>&
functional_bind_select(const volatile functional_bind_leaf<Index, T>& leaf) noexcept {
    return leaf;
}

template<size_t Index, typename T>
constexpr functional_bind_leaf<Index, T>&&
functional_bind_select(functional_bind_leaf<Index, T>&& leaf) noexcept {
    return std::move(leaf);
}

template<size_t Index, typename T>
constexpr const functional_bind_leaf<Index, T>&&
functional_bind_select(const functional_bind_leaf<Index, T>&& leaf) noexcept {
    return std::move(leaf);
}

template<size_t Index, typename Storage>
constexpr auto functional_bind_stored(Storage& storage) noexcept
    -> decltype((functional_bind_select<Index>(storage).value)) {
    return (functional_bind_select<Index>(storage).value);
}

template<size_t Index, typename Storage>
constexpr auto functional_bind_stored_forward(
    Storage&& storage) noexcept
    -> decltype((functional_bind_select<Index>(
        std::forward<Storage>(storage)).value)) {
    return (functional_bind_select<Index>(
        std::forward<Storage>(storage)).value);
}

template<size_t Index, typename T>
constexpr T&& functional_bind_call_select(
    functional_bind_leaf<Index, T>& leaf) noexcept {
    return static_cast<T&&>(leaf.value);
}

template<size_t Index, typename CallStorage>
constexpr auto functional_bind_call_arg(
    CallStorage& storage) noexcept
    -> decltype(functional_bind_call_select<Index>(storage)) {
    return functional_bind_call_select<Index>(storage);
}

template<typename Nested, typename CallStorage, size_t... Indexes>
constexpr auto functional_bind_nested(
    Nested&& nested, CallStorage& calls, index_sequence<Indexes...>)
    noexcept(noexcept(std::forward<Nested>(nested)(
        functional_bind_call_arg<Indexes>(calls)...)))
    -> decltype(std::forward<Nested>(nested)(
        functional_bind_call_arg<Indexes>(calls)...)) {
    return std::forward<Nested>(nested)(
        functional_bind_call_arg<Indexes>(calls)...);
}

template<typename Bound, typename CallStorage,
         enable_if_t<
             is_reference_wrapper<remove_cvref_t<Bound>>::value,
             int> = 0>
constexpr auto functional_bind_bound_arg(
    Bound&& bound, CallStorage&) noexcept
    -> decltype(bound.get()) {
    return bound.get();
}

template<typename Bound, typename CallStorage,
         enable_if_t<
             (is_placeholder<remove_cvref_t<Bound>>::value > 0) &&
             (static_cast<size_t>(
                 is_placeholder<remove_cvref_t<Bound>>::value) <=
              CallStorage::size),
             int> = 0>
constexpr auto functional_bind_bound_arg(
    Bound&&, CallStorage& calls) noexcept
    -> decltype(functional_bind_call_arg<static_cast<size_t>(
        is_placeholder<remove_cvref_t<Bound>>::value - 1)>(calls)) {
    return functional_bind_call_arg<static_cast<size_t>(
        is_placeholder<remove_cvref_t<Bound>>::value - 1)>(calls);
}

template<typename Bound, typename CallStorage,
         enable_if_t<is_bind_expression<remove_cvref_t<Bound>>::value,
                     int> = 0>
constexpr auto functional_bind_bound_arg(Bound&& bound, CallStorage& calls)
    noexcept(noexcept(functional_bind_nested(
        std::forward<Bound>(bound), calls,
        make_index_sequence<CallStorage::size>{})))
    -> decltype(functional_bind_nested(
        std::forward<Bound>(bound), calls,
        make_index_sequence<CallStorage::size>{})) {
    return functional_bind_nested(
        std::forward<Bound>(bound), calls,
        make_index_sequence<CallStorage::size>{});
}

template<typename Bound, typename CallStorage,
         enable_if_t<
             !is_reference_wrapper<remove_cvref_t<Bound>>::value &&
             (is_placeholder<remove_cvref_t<Bound>>::value == 0) &&
             !is_bind_expression<remove_cvref_t<Bound>>::value,
             int> = 0>
constexpr Bound&& functional_bind_bound_arg(
    Bound&& bound, CallStorage&) noexcept {
    return std::forward<Bound>(bound);
}

template<typename Callable, typename... Bound>
class functional_bind_wrapper : public functional_bind_result_type<Callable> {
    Callable callable_;
    using storage_type = functional_bind_storage<
        index_sequence_for<Bound...>, Bound...>;
    storage_type bound_;

    template<typename Self, typename CallStorage, size_t... Indexes>
    static constexpr auto call_impl(
        Self& self, CallStorage& calls, index_sequence<Indexes...>)
        noexcept(noexcept(std::invoke(
            self.callable_,
            functional_bind_bound_arg(
                functional_bind_stored<Indexes>(self.bound_), calls)...)))
        -> decltype(std::invoke(
            self.callable_,
            functional_bind_bound_arg(
                functional_bind_stored<Indexes>(self.bound_), calls)...)) {
        return std::invoke(
            self.callable_,
            functional_bind_bound_arg(
                functional_bind_stored<Indexes>(self.bound_), calls)...);
    }

public:
    template<typename F, typename... Inputs,
             enable_if_t<
                 !is_same<remove_cvref_t<F>, functional_bind_wrapper>::value &&
                 is_constructible<Callable, F&&>::value &&
                 sizeof...(Inputs) == sizeof...(Bound) &&
                 conjunction<is_constructible<Bound, Inputs&&>...>::value,
                 int> = 0>
    constexpr explicit functional_bind_wrapper(F&& callable, Inputs&&... inputs)
        : callable_(std::forward<F>(callable)),
          bound_(std::forward<Inputs>(inputs)...) {}

    template<typename... CallArgs>
    RIN_FUNCTIONAL_CONSTEXPR14 auto operator()(CallArgs&&... args)
        noexcept(noexcept(call_impl(
            *this,
            std::declval<functional_bind_storage<
                index_sequence_for<CallArgs...>, CallArgs&&...>&>(),
            index_sequence_for<Bound...>{})))
        -> decltype(call_impl(
            *this,
            std::declval<functional_bind_storage<
                index_sequence_for<CallArgs...>, CallArgs&&...>&>(),
            index_sequence_for<Bound...>{})) {
        functional_bind_storage<
            index_sequence_for<CallArgs...>, CallArgs&&...> calls(
                std::forward<CallArgs>(args)...);
        return call_impl(*this, calls, index_sequence_for<Bound...>{});
    }

    template<typename... CallArgs>
    RIN_FUNCTIONAL_CONSTEXPR14 auto operator()(CallArgs&&... args) const
        noexcept(noexcept(call_impl(
            *this,
            std::declval<functional_bind_storage<
                index_sequence_for<CallArgs...>, CallArgs&&...>&>(),
            index_sequence_for<Bound...>{})))
        -> decltype(call_impl(
            *this,
            std::declval<functional_bind_storage<
                index_sequence_for<CallArgs...>, CallArgs&&...>&>(),
            index_sequence_for<Bound...>{})) {
        functional_bind_storage<
            index_sequence_for<CallArgs...>, CallArgs&&...> calls(
                std::forward<CallArgs>(args)...);
        return call_impl(*this, calls, index_sequence_for<Bound...>{});
    }

#if __cplusplus < 202002L
#if __cplusplus >= 201703L
#define RIN_FUNCTIONAL_BIND_VOLATILE_DEPRECATED \
    [[deprecated("std::bind does not support volatile in C++17")]]
#else
#define RIN_FUNCTIONAL_BIND_VOLATILE_DEPRECATED
#endif
    template<typename... CallArgs>
    RIN_FUNCTIONAL_BIND_VOLATILE_DEPRECATED
    RIN_FUNCTIONAL_CONSTEXPR14 auto operator()(CallArgs&&... args) volatile
        noexcept(noexcept(call_impl(
            *this,
            std::declval<functional_bind_storage<
                index_sequence_for<CallArgs...>, CallArgs&&...>&>(),
            index_sequence_for<Bound...>{})))
        -> decltype(call_impl(
            *this,
            std::declval<functional_bind_storage<
                index_sequence_for<CallArgs...>, CallArgs&&...>&>(),
            index_sequence_for<Bound...>{})) {
        functional_bind_storage<
            index_sequence_for<CallArgs...>, CallArgs&&...> calls(
                std::forward<CallArgs>(args)...);
        return call_impl(*this, calls, index_sequence_for<Bound...>{});
    }

    template<typename... CallArgs>
    RIN_FUNCTIONAL_BIND_VOLATILE_DEPRECATED
    RIN_FUNCTIONAL_CONSTEXPR14 auto operator()(CallArgs&&... args) const volatile
        noexcept(noexcept(call_impl(
            *this,
            std::declval<functional_bind_storage<
                index_sequence_for<CallArgs...>, CallArgs&&...>&>(),
            index_sequence_for<Bound...>{})))
        -> decltype(call_impl(
            *this,
            std::declval<functional_bind_storage<
                index_sequence_for<CallArgs...>, CallArgs&&...>&>(),
            index_sequence_for<Bound...>{})) {
        functional_bind_storage<
            index_sequence_for<CallArgs...>, CallArgs&&...> calls(
                std::forward<CallArgs>(args)...);
        return call_impl(*this, calls, index_sequence_for<Bound...>{});
    }
#undef RIN_FUNCTIONAL_BIND_VOLATILE_DEPRECATED
#endif
};

template<typename Result>
struct functional_bind_result_call {
    template<typename Inner, typename... CallArgs>
    static Result call(Inner& inner, CallArgs&&... args) {
        return inner(std::forward<CallArgs>(args)...);
    }
};

template<>
struct functional_bind_result_call<void> {
    template<typename Inner, typename... CallArgs>
    static void call(Inner& inner, CallArgs&&... args) {
        inner(std::forward<CallArgs>(args)...);
    }
};

template<typename Result, typename Inner>
class functional_bind_result_wrapper {
    Inner inner_;

public:
    using result_type = Result;

    template<typename Input,
             enable_if_t<is_constructible<Inner, Input&&>::value, int> = 0>
    constexpr explicit functional_bind_result_wrapper(Input&& input)
        : inner_(std::forward<Input>(input)) {}

    template<typename... CallArgs,
             enable_if_t<
                 is_invocable_r<Result, Inner&, CallArgs...>::value,
                 int> = 0>
    RIN_FUNCTIONAL_CONSTEXPR14 Result operator()(CallArgs&&... args)
        noexcept(is_nothrow_invocable_r<
            Result, Inner&, CallArgs...>::value) {
        return functional_bind_result_call<Result>::call(
            inner_, std::forward<CallArgs>(args)...);
    }

    template<typename... CallArgs,
             enable_if_t<
                 is_invocable_r<Result, const Inner&, CallArgs...>::value,
                 int> = 0>
    RIN_FUNCTIONAL_CONSTEXPR14 Result operator()(CallArgs&&... args) const
        noexcept(is_nothrow_invocable_r<
            Result, const Inner&, CallArgs...>::value) {
        return functional_bind_result_call<Result>::call(
            inner_, std::forward<CallArgs>(args)...);
    }

#if __cplusplus < 202002L
#if __cplusplus >= 201703L
#define RIN_FUNCTIONAL_BIND_VOLATILE_DEPRECATED \
    [[deprecated("std::bind does not support volatile in C++17")]]
#else
#define RIN_FUNCTIONAL_BIND_VOLATILE_DEPRECATED
#endif
    template<typename... CallArgs,
             enable_if_t<
                 is_invocable_r<Result, volatile Inner&, CallArgs...>::value,
                 int> = 0>
    RIN_FUNCTIONAL_BIND_VOLATILE_DEPRECATED
    RIN_FUNCTIONAL_CONSTEXPR14 Result operator()(CallArgs&&... args) volatile
        noexcept(is_nothrow_invocable_r<
            Result, volatile Inner&, CallArgs...>::value) {
        return functional_bind_result_call<Result>::call(
            inner_, std::forward<CallArgs>(args)...);
    }

    template<typename... CallArgs,
             enable_if_t<
                 is_invocable_r<Result, const volatile Inner&, CallArgs...>::value,
                 int> = 0>
    RIN_FUNCTIONAL_BIND_VOLATILE_DEPRECATED
    RIN_FUNCTIONAL_CONSTEXPR14 Result operator()(CallArgs&&... args) const volatile
        noexcept(is_nothrow_invocable_r<
            Result, const volatile Inner&, CallArgs...>::value) {
        return functional_bind_result_call<Result>::call(
            inner_, std::forward<CallArgs>(args)...);
    }
#undef RIN_FUNCTIONAL_BIND_VOLATILE_DEPRECATED
#endif
};

template<bool Back, typename Self, size_t... Indexes, typename... CallArgs>
constexpr auto functional_partial_call(
    false_type, Self&& self, index_sequence<Indexes...>, CallArgs&&... args)
    noexcept(noexcept(std::invoke(
        std::forward<Self>(self).callable_,
        functional_bind_stored_forward<Indexes>(
            std::forward<Self>(self).bound_)...,
        std::forward<CallArgs>(args)...)))
    -> decltype(std::invoke(
        std::forward<Self>(self).callable_,
        functional_bind_stored_forward<Indexes>(
            std::forward<Self>(self).bound_)...,
        std::forward<CallArgs>(args)...)) {
    return std::invoke(
        std::forward<Self>(self).callable_,
        functional_bind_stored_forward<Indexes>(
            std::forward<Self>(self).bound_)...,
        std::forward<CallArgs>(args)...);
}

template<bool Back, typename Self, size_t... Indexes, typename... CallArgs>
constexpr auto functional_partial_call(
    true_type, Self&& self, index_sequence<Indexes...>, CallArgs&&... args)
    noexcept(noexcept(std::invoke(
        std::forward<Self>(self).callable_,
        std::forward<CallArgs>(args)...,
        functional_bind_stored_forward<Indexes>(
            std::forward<Self>(self).bound_)...)))
    -> decltype(std::invoke(
        std::forward<Self>(self).callable_,
        std::forward<CallArgs>(args)...,
        functional_bind_stored_forward<Indexes>(
            std::forward<Self>(self).bound_)...)) {
    return std::invoke(
        std::forward<Self>(self).callable_,
        std::forward<CallArgs>(args)...,
        functional_bind_stored_forward<Indexes>(
            std::forward<Self>(self).bound_)...);
}

template<bool Back, typename Callable, typename... Bound>
class functional_partial_wrapper {
public:
    Callable callable_;
    using storage_type = functional_bind_storage<
        index_sequence_for<Bound...>, Bound...>;
    storage_type bound_;

    template<typename F, typename... Inputs,
             enable_if_t<
                 !is_same<remove_cvref_t<F>, functional_partial_wrapper>::value &&
                 is_constructible<Callable, F&&>::value &&
                 sizeof...(Inputs) == sizeof...(Bound) &&
                 conjunction<is_constructible<Bound, Inputs&&>...>::value,
                 int> = 0>
    constexpr explicit functional_partial_wrapper(F&& callable,
                                                  Inputs&&... inputs)
        : callable_(std::forward<F>(callable)),
          bound_(std::forward<Inputs>(inputs)...) {}

#define RIN_FUNCTIONAL_PARTIAL_CALL(cvref, self_expr)                         \
    template<typename... CallArgs>                                            \
    RIN_FUNCTIONAL_CONSTEXPR14 auto operator()(CallArgs&&... args) cvref      \
        noexcept(noexcept(functional_partial_call<Back>(                       \
            bool_constant<Back>{}, self_expr, index_sequence_for<Bound...>{},  \
            std::forward<CallArgs>(args)...)))                                 \
        -> decltype(functional_partial_call<Back>(                             \
            bool_constant<Back>{}, self_expr, index_sequence_for<Bound...>{},  \
            std::forward<CallArgs>(args)...)) {                                \
        return functional_partial_call<Back>(                                 \
            bool_constant<Back>{}, self_expr, index_sequence_for<Bound...>{},  \
            std::forward<CallArgs>(args)...);                                  \
    }

    RIN_FUNCTIONAL_PARTIAL_CALL(&, *this)
    RIN_FUNCTIONAL_PARTIAL_CALL(const &, *this)
    RIN_FUNCTIONAL_PARTIAL_CALL(&&, std::move(*this))
    RIN_FUNCTIONAL_PARTIAL_CALL(const &&, std::move(*this))

#undef RIN_FUNCTIONAL_PARTIAL_CALL
};

} /* namespace detail */

template<typename Callable, typename... Bound>
constexpr auto bind(Callable&& callable, Bound&&... bound)
    -> detail::functional_bind_wrapper<
        decay_t<Callable>, decay_t<Bound>...> {
    using result_type = detail::functional_bind_wrapper<
        decay_t<Callable>, decay_t<Bound>...>;
    return result_type(std::forward<Callable>(callable),
                       std::forward<Bound>(bound)...);
}

template<typename Result, typename Callable, typename... Bound>
RIN_FUNCTIONAL_CONSTEXPR14 auto bind(Callable&& callable, Bound&&... bound)
    -> detail::functional_bind_result_wrapper<
        Result,
        detail::functional_bind_wrapper<
            decay_t<Callable>, decay_t<Bound>...>> {
    auto inner = std::bind(std::forward<Callable>(callable),
                           std::forward<Bound>(bound)...);
    using wrapper_type = detail::functional_bind_result_wrapper<
        Result, decltype(inner)>;
    return wrapper_type(std::move(inner));
}

#if __cplusplus >= 202002L
template<typename Callable, typename... Bound>
constexpr auto bind_front(Callable&& callable, Bound&&... bound)
    -> detail::functional_partial_wrapper<
        false, decay_t<Callable>, decay_t<Bound>...> {
    using result_type = detail::functional_partial_wrapper<
        false, decay_t<Callable>, decay_t<Bound>...>;
    return result_type(std::forward<Callable>(callable),
                       std::forward<Bound>(bound)...);
}
#endif

#if __cplusplus > 202002L
template<typename Callable, typename... Bound>
constexpr auto bind_back(Callable&& callable, Bound&&... bound)
    -> detail::functional_partial_wrapper<
        true, decay_t<Callable>, decay_t<Bound>...> {
    using result_type = detail::functional_partial_wrapper<
        true, decay_t<Callable>, decay_t<Bound>...>;
    return result_type(std::forward<Callable>(callable),
                       std::forward<Bound>(bound)...);
}
#endif

/* less / greater / equal_to */
template<typename T = void>
struct less {
    constexpr bool operator()(const T& a, const T& b) const {
        return a < b;
    }
};

template<typename T>
struct less<T*> {
    constexpr bool operator()(T* a, T* b) const noexcept {
        return detail::functional_pointer_less(a, b);
    }
};

template<>
struct less<void> {
    using is_transparent = void;

    template<typename T, typename U,
             enable_if_t<detail::is_object_pointer<
                             remove_reference_t<T>>::value &&
                             detail::is_object_pointer<
                             remove_reference_t<U>>::value,
                         int> = 0>
    constexpr bool operator()(T&& a, U&& b) const noexcept {
        return detail::object_pointer_total_less(static_cast<T&&>(a),
                                                 static_cast<U&&>(b));
    }

    template<typename T, typename U,
             enable_if_t<!detail::is_object_pointer<
                              remove_reference_t<T>>::value ||
                             !detail::is_object_pointer<
                              remove_reference_t<U>>::value,
                         int> = 0>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(std::forward<T>(a) < std::forward<U>(b)))
        -> decltype(std::forward<T>(a) < std::forward<U>(b)) {
        return std::forward<T>(a) < std::forward<U>(b);
    }
};

template<typename T = void>
struct greater {
    constexpr bool operator()(const T& a, const T& b) const {
        return a > b;
    }
};

template<typename T>
struct greater<T*> {
    constexpr bool operator()(T* a, T* b) const noexcept {
        return detail::functional_pointer_less(b, a);
    }
};

template<>
struct greater<void> {
    using is_transparent = void;

    template<typename T, typename U,
             enable_if_t<detail::is_object_pointer<
                             remove_reference_t<T>>::value &&
                             detail::is_object_pointer<
                             remove_reference_t<U>>::value,
                         int> = 0>
    constexpr bool operator()(T&& a, U&& b) const noexcept {
        return detail::object_pointer_total_less(static_cast<U&&>(b),
                                                 static_cast<T&&>(a));
    }

    template<typename T, typename U,
             enable_if_t<!detail::is_object_pointer<
                              remove_reference_t<T>>::value ||
                             !detail::is_object_pointer<
                              remove_reference_t<U>>::value,
                         int> = 0>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(std::forward<T>(a) > std::forward<U>(b)))
        -> decltype(std::forward<T>(a) > std::forward<U>(b)) {
        return std::forward<T>(a) > std::forward<U>(b);
    }
};

template<typename T = void>
struct equal_to {
    constexpr bool operator()(const T& a, const T& b) const {
        return a == b;
    }
};

template<>
struct equal_to<void> {
    using is_transparent = void;

    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(std::forward<T>(a) == std::forward<U>(b)))
        -> decltype(std::forward<T>(a) == std::forward<U>(b)) {
        return std::forward<T>(a) == std::forward<U>(b);
    }
};

template<typename T = void>
struct not_equal_to {
    constexpr bool operator()(const T& a, const T& b) const {
        return a != b;
    }
};

template<>
struct not_equal_to<void> {
    using is_transparent = void;

    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(std::forward<T>(a) != std::forward<U>(b)))
        -> decltype(std::forward<T>(a) != std::forward<U>(b)) {
        return std::forward<T>(a) != std::forward<U>(b);
    }
};

template<typename T = void>
struct greater_equal {
    constexpr bool operator()(const T& a, const T& b) const {
        return a >= b;
    }
};

template<typename T>
struct greater_equal<T*> {
    constexpr bool operator()(T* a, T* b) const noexcept {
        return !detail::functional_pointer_less(a, b);
    }
};

template<>
struct greater_equal<void> {
    using is_transparent = void;

    template<typename T, typename U,
             enable_if_t<detail::is_object_pointer<
                             remove_reference_t<T>>::value &&
                             detail::is_object_pointer<
                             remove_reference_t<U>>::value,
                         int> = 0>
    constexpr bool operator()(T&& a, U&& b) const noexcept {
        return !detail::object_pointer_total_less(static_cast<T&&>(a),
                                                  static_cast<U&&>(b));
    }

    template<typename T, typename U,
             enable_if_t<!detail::is_object_pointer<
                              remove_reference_t<T>>::value ||
                             !detail::is_object_pointer<
                              remove_reference_t<U>>::value,
                         int> = 0>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(std::forward<T>(a) >= std::forward<U>(b)))
        -> decltype(std::forward<T>(a) >= std::forward<U>(b)) {
        return std::forward<T>(a) >= std::forward<U>(b);
    }
};

template<typename T = void>
struct less_equal {
    constexpr bool operator()(const T& a, const T& b) const {
        return a <= b;
    }
};

template<typename T>
struct less_equal<T*> {
    constexpr bool operator()(T* a, T* b) const noexcept {
        return !detail::functional_pointer_less(b, a);
    }
};

template<>
struct less_equal<void> {
    using is_transparent = void;

    template<typename T, typename U,
             enable_if_t<detail::is_object_pointer<
                             remove_reference_t<T>>::value &&
                             detail::is_object_pointer<
                             remove_reference_t<U>>::value,
                         int> = 0>
    constexpr bool operator()(T&& a, U&& b) const noexcept {
        return !detail::object_pointer_total_less(static_cast<U&&>(b),
                                                  static_cast<T&&>(a));
    }

    template<typename T, typename U,
             enable_if_t<!detail::is_object_pointer<
                              remove_reference_t<T>>::value ||
                             !detail::is_object_pointer<
                              remove_reference_t<U>>::value,
                         int> = 0>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(std::forward<T>(a) <= std::forward<U>(b)))
        -> decltype(std::forward<T>(a) <= std::forward<U>(b)) {
        return std::forward<T>(a) <= std::forward<U>(b);
    }
};

/* 算術演算ファンクタ */
template<typename T = void>
struct plus {
    constexpr T operator()(const T& a, const T& b) const {
        return a + b;
    }
};

template<>
struct plus<void> {
    using is_transparent = void;

    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(std::forward<T>(a) + std::forward<U>(b)))
        -> decltype(std::forward<T>(a) + std::forward<U>(b)) {
        return std::forward<T>(a) + std::forward<U>(b);
    }
};

template<typename T = void>
struct minus {
    constexpr T operator()(const T& a, const T& b) const {
        return a - b;
    }
};

template<>
struct minus<void> {
    using is_transparent = void;

    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(std::forward<T>(a) - std::forward<U>(b)))
        -> decltype(std::forward<T>(a) - std::forward<U>(b)) {
        return std::forward<T>(a) - std::forward<U>(b);
    }
};

template<typename T = void>
struct multiplies {
    constexpr T operator()(const T& a, const T& b) const {
        return a * b;
    }
};

template<>
struct multiplies<void> {
    using is_transparent = void;

    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(std::forward<T>(a) * std::forward<U>(b)))
        -> decltype(std::forward<T>(a) * std::forward<U>(b)) {
        return std::forward<T>(a) * std::forward<U>(b);
    }
};

template<typename T = void>
struct divides {
    constexpr T operator()(const T& a, const T& b) const {
        return a / b;
    }
};

template<>
struct divides<void> {
    using is_transparent = void;

    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(std::forward<T>(a) / std::forward<U>(b)))
        -> decltype(std::forward<T>(a) / std::forward<U>(b)) {
        return std::forward<T>(a) / std::forward<U>(b);
    }
};

template<typename T = void>
struct modulus {
    constexpr T operator()(const T& a, const T& b) const {
        return a % b;
    }
};

template<>
struct modulus<void> {
    using is_transparent = void;

    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(std::forward<T>(a) % std::forward<U>(b)))
        -> decltype(std::forward<T>(a) % std::forward<U>(b)) {
        return std::forward<T>(a) % std::forward<U>(b);
    }
};

template<typename T = void>
struct negate {
    constexpr T operator()(const T& a) const {
        return -a;
    }
};

template<>
struct negate<void> {
    using is_transparent = void;

    template<typename T>
    constexpr auto operator()(T&& value) const
        noexcept(noexcept(-std::forward<T>(value)))
        -> decltype(-std::forward<T>(value)) {
        return -std::forward<T>(value);
    }
};

/* 論理演算ファンクタ */
template<typename T = void>
struct logical_and {
    constexpr bool operator()(const T& a, const T& b) const {
        return a && b;
    }
};

template<>
struct logical_and<void> {
    using is_transparent = void;

    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(std::forward<T>(a) && std::forward<U>(b)))
        -> decltype(std::forward<T>(a) && std::forward<U>(b)) {
        return std::forward<T>(a) && std::forward<U>(b);
    }
};

template<typename T = void>
struct logical_or {
    constexpr bool operator()(const T& a, const T& b) const {
        return a || b;
    }
};

template<>
struct logical_or<void> {
    using is_transparent = void;

    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(std::forward<T>(a) || std::forward<U>(b)))
        -> decltype(std::forward<T>(a) || std::forward<U>(b)) {
        return std::forward<T>(a) || std::forward<U>(b);
    }
};

template<typename T = void>
struct logical_not {
    constexpr bool operator()(const T& a) const {
        return !a;
    }
};

template<>
struct logical_not<void> {
    using is_transparent = void;

    template<typename T>
    constexpr auto operator()(T&& value) const
        noexcept(noexcept(!std::forward<T>(value)))
        -> decltype(!std::forward<T>(value)) {
        return !std::forward<T>(value);
    }
};

/* ビット演算ファンクタ */
template<typename T = void>
struct bit_and {
    constexpr T operator()(const T& a, const T& b) const {
        return a & b;
    }
};

template<>
struct bit_and<void> {
    using is_transparent = void;

    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(std::forward<T>(a) & std::forward<U>(b)))
        -> decltype(std::forward<T>(a) & std::forward<U>(b)) {
        return std::forward<T>(a) & std::forward<U>(b);
    }
};

template<typename T = void>
struct bit_or {
    constexpr T operator()(const T& a, const T& b) const {
        return a | b;
    }
};

template<>
struct bit_or<void> {
    using is_transparent = void;

    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(std::forward<T>(a) | std::forward<U>(b)))
        -> decltype(std::forward<T>(a) | std::forward<U>(b)) {
        return std::forward<T>(a) | std::forward<U>(b);
    }
};

template<typename T = void>
struct bit_xor {
    constexpr T operator()(const T& a, const T& b) const {
        return a ^ b;
    }
};

template<>
struct bit_xor<void> {
    using is_transparent = void;

    template<typename T, typename U>
    constexpr auto operator()(T&& a, U&& b) const
        noexcept(noexcept(std::forward<T>(a) ^ std::forward<U>(b)))
        -> decltype(std::forward<T>(a) ^ std::forward<U>(b)) {
        return std::forward<T>(a) ^ std::forward<U>(b);
    }
};

template<typename T = void>
struct bit_not {
    constexpr T operator()(const T& a) const {
        return ~a;
    }
};

template<>
struct bit_not<void> {
    using is_transparent = void;

    template<typename T>
    constexpr auto operator()(T&& value) const
        noexcept(noexcept(~std::forward<T>(value)))
        -> decltype(~std::forward<T>(value)) {
        return ~std::forward<T>(value);
    }
};

#if __cplusplus >= 202002L
struct identity {
    using is_transparent = void;

    template<typename T>
    [[nodiscard]] constexpr T&& operator()(T&& value) const noexcept {
        return std::forward<T>(value);
    }
};
#endif

#if __cplusplus >= 201703L
/* C++17 not_fn adaptor.  Each cv/ref-qualified call path forwards the
 * stored callable exactly as the wrapper's value category requires, while a
 * trailing return type keeps unsupported invocations out of overload
 * resolution instead of manufacturing a success value. */
template<typename F>
class not_fn_t {
    F function_;

public:
    template<typename Fn>
    explicit constexpr not_fn_t(Fn&& function)
        : function_(std::forward<Fn>(function)) {}

    template<typename... Args>
    constexpr auto operator()(Args&&... args) &
        noexcept(noexcept(!std::invoke(function_,
                                        std::forward<Args>(args)...)))
        -> decltype(!std::invoke(function_, std::forward<Args>(args)...)) {
        return !std::invoke(function_, std::forward<Args>(args)...);
    }

    template<typename... Args>
    constexpr auto operator()(Args&&... args) const &
        noexcept(noexcept(!std::invoke(function_,
                                        std::forward<Args>(args)...)))
        -> decltype(!std::invoke(function_, std::forward<Args>(args)...)) {
        return !std::invoke(function_, std::forward<Args>(args)...);
    }

    template<typename... Args>
    constexpr auto operator()(Args&&... args) &&
        noexcept(noexcept(!std::invoke(std::move(function_),
                                        std::forward<Args>(args)...)))
        -> decltype(!std::invoke(std::move(function_),
                                 std::forward<Args>(args)...)) {
        return !std::invoke(std::move(function_),
                            std::forward<Args>(args)...);
    }

    template<typename... Args>
    constexpr auto operator()(Args&&... args) const &&
        noexcept(noexcept(!std::invoke(std::move(function_),
                                        std::forward<Args>(args)...)))
        -> decltype(!std::invoke(std::move(function_),
                                 std::forward<Args>(args)...)) {
        return !std::invoke(std::move(function_),
                            std::forward<Args>(args)...);
    }
};

template<typename F>
constexpr not_fn_t<decay_t<F>> not_fn(F&& function) {
    return not_fn_t<decay_t<F>>(std::forward<F>(function));
}
#endif

#if __cplusplus >= 201703L
namespace detail {

template<typename PatternIt, typename HaystackIt, typename BinaryPredicate>
constexpr pair<HaystackIt, HaystackIt>
functional_forward_search(PatternIt pattern_first, PatternIt pattern_last,
                          HaystackIt first, HaystackIt last,
                          const BinaryPredicate& predicate) {
    if (pattern_first == pattern_last) {
        return pair<HaystackIt, HaystackIt>(first, first);
    }

    for (HaystackIt candidate = first; candidate != last; ++candidate) {
        HaystackIt haystack = candidate;
        PatternIt pattern = pattern_first;
        while (haystack != last && pattern != pattern_last &&
               predicate(*haystack, *pattern)) {
            ++haystack;
            ++pattern;
        }
        if (pattern == pattern_last) {
            return pair<HaystackIt, HaystackIt>(candidate, haystack);
        }
    }
    return pair<HaystackIt, HaystackIt>(last, last);
}

/* Allocation-free bad-character search for the random-access searchers.
 * The standard constructor retains a hash object, but using that hash as the
 * sole skip key would be unsound when a caller intentionally pairs the
 * default hash with a case-folding predicate.  Locate the rightmost
 * predicate-equivalent pattern element instead; this preserves correctness
 * for heterogeneous predicate/hash state while still skipping impossible
 * alignments without an unbounded table allocation. */
template<typename PatternIt, typename HaystackIt, typename BinaryPredicate>
constexpr pair<HaystackIt, HaystackIt>
functional_bad_character_search(PatternIt pattern_first, PatternIt pattern_last,
                                HaystackIt first, HaystackIt last,
                                const BinaryPredicate& predicate) {
    using pattern_difference =
        typename iterator_traits<PatternIt>::difference_type;
    using haystack_difference =
        typename iterator_traits<HaystackIt>::difference_type;

    const pattern_difference pattern_size = pattern_last - pattern_first;
    if (pattern_size <= 0)
        return pair<HaystackIt, HaystackIt>(first, first);
    if (last - first < static_cast<haystack_difference>(pattern_size))
        return pair<HaystackIt, HaystackIt>(last, last);

    HaystackIt candidate = first;
    while (last - candidate >=
           static_cast<haystack_difference>(pattern_size)) {
        pattern_difference index = pattern_size;
        while (index != 0) {
            --index;
            if (!predicate(*(candidate + index), *(pattern_first + index)))
                break;
        }
        if (index == 0 &&
            predicate(*candidate, *pattern_first)) {
            return pair<HaystackIt, HaystackIt>(
                candidate, candidate + pattern_size);
        }

        /* `index` identifies the actual mismatching pattern position.  The
         * bad-character rule must use that window character (not the final
         * character, which may belong to an already matched suffix). */
        const auto& mismatching = *(candidate + index);
        pattern_difference last_occurrence = -1;
        for (pattern_difference probe = index; probe != 0;) {
            --probe;
            if (predicate(mismatching, *(pattern_first + probe))) {
                last_occurrence = probe;
                break;
            }
        }
        pattern_difference shift = index + 1;
        if (last_occurrence >= 0)
            shift = index - last_occurrence;
        if (shift <= 0) shift = 1;
        candidate += shift;
    }
    return pair<HaystackIt, HaystackIt>(last, last);
}

} /* namespace detail */

/* C++17 default_searcher.  The pattern iterators are retained by value, as
 * required by the standard searcher object model.  Matching is performed by
 * a single forward pass over the haystack; no allocation or second traversal
 * of the pattern is needed, and an empty pattern returns the beginning of the
 * searched range. */
template<typename ForwardIt1, typename BinaryPredicate = equal_to<>>
class default_searcher {
    ForwardIt1 pattern_first_;
    ForwardIt1 pattern_last_;
    BinaryPredicate predicate_;

public:
    constexpr default_searcher(ForwardIt1 first, ForwardIt1 last,
                               BinaryPredicate predicate = BinaryPredicate())
        : pattern_first_(first), pattern_last_(last),
          predicate_(std::move(predicate)) {}

    template<typename ForwardIt2>
    constexpr pair<ForwardIt2, ForwardIt2>
    operator()(ForwardIt2 first, ForwardIt2 last) const {
        return detail::functional_forward_search(
            pattern_first_, pattern_last_, first, last, predicate_);
    }
};

/* The standard also exposes the Boyer--Moore and Horspool searcher types.
 * Rin keeps their iterator/hash/predicate ownership and result contract
 * exact, while using the bounded bad-character skip above.  This avoids an
 * unbounded table allocation in freestanding builds; hash state is still
 * retained so construction and copy semantics match the standard object
 * shape. */
template<typename RandomIt1,
         typename Hash = hash<typename iterator_traits<RandomIt1>::value_type>,
         typename BinaryPredicate = equal_to<>>
class boyer_moore_searcher {
    RandomIt1 pattern_first_;
    RandomIt1 pattern_last_;
    Hash hash_;
    BinaryPredicate predicate_;

public:
    constexpr boyer_moore_searcher(
        RandomIt1 first, RandomIt1 last,
        Hash hash = Hash(), BinaryPredicate predicate = BinaryPredicate())
        : pattern_first_(first), pattern_last_(last), hash_(std::move(hash)),
          predicate_(std::move(predicate)) {}

    template<typename RandomIt2>
    constexpr pair<RandomIt2, RandomIt2>
    operator()(RandomIt2 first, RandomIt2 last) const {
        (void)hash_;
        return detail::functional_bad_character_search(
            pattern_first_, pattern_last_, first, last, predicate_);
    }
};

template<typename RandomIt1,
         typename Hash = hash<typename iterator_traits<RandomIt1>::value_type>,
         typename BinaryPredicate = equal_to<>>
class boyer_moore_horspool_searcher {
    RandomIt1 pattern_first_;
    RandomIt1 pattern_last_;
    Hash hash_;
    BinaryPredicate predicate_;

public:
    constexpr boyer_moore_horspool_searcher(
        RandomIt1 first, RandomIt1 last,
        Hash hash = Hash(), BinaryPredicate predicate = BinaryPredicate())
        : pattern_first_(first), pattern_last_(last), hash_(std::move(hash)),
          predicate_(std::move(predicate)) {}

    template<typename RandomIt2>
    constexpr pair<RandomIt2, RandomIt2>
    operator()(RandomIt2 first, RandomIt2 last) const {
        (void)hash_;
        return detail::functional_bad_character_search(
            pattern_first_, pattern_last_, first, last, predicate_);
    }
};
#endif

/* hash */

namespace detail {

template<typename T, bool = is_enum<T>::value>
struct functional_hash_base {
    functional_hash_base() = delete;
    functional_hash_base(const functional_hash_base&) = delete;
    functional_hash_base(functional_hash_base&&) = delete;
    functional_hash_base& operator=(const functional_hash_base&) = delete;
    functional_hash_base& operator=(functional_hash_base&&) = delete;
    ~functional_hash_base() = delete;
};

template<typename T>
struct functional_hash_base<T, true> {
    size_t operator()(T value) const noexcept {
        using underlying = typename underlying_type<T>::type;
        return hash<underlying>{}(static_cast<underlying>(value));
    }
};

inline size_t functional_hash_bytes(const unsigned char* bytes,
                                    size_t count) noexcept {
    size_t value = sizeof(size_t) == 8
        ? static_cast<size_t>(14695981039346656037ull)
        : static_cast<size_t>(2166136261u);
    const size_t prime = sizeof(size_t) == 8
        ? static_cast<size_t>(1099511628211ull)
        : static_cast<size_t>(16777619u);
    for (size_t index = 0; index < count; ++index) {
        value ^= static_cast<size_t>(bytes[index]);
        value *= prime;
    }
    return value;
}

inline unsigned long long functional_hash_load_u64(
    const unsigned char* bytes) noexcept {
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    unsigned long long value = 0;
    for (size_t index = 0; index != 8; ++index)
        value = (value << 8) | static_cast<unsigned long long>(bytes[index]);
    return value;
#else
    unsigned long long value = 0;
    __builtin_memcpy(&value, bytes, sizeof(value));
    return value;
#endif
}

inline void functional_hash_store_u64(
    unsigned char* bytes, unsigned long long value) noexcept {
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    for (size_t index = 0; index != 8; ++index) {
        bytes[7 - index] = static_cast<unsigned char>(value & 0xffu);
        value >>= 8;
    }
#else
    __builtin_memcpy(bytes, &value, sizeof(value));
#endif
}

} /* namespace detail */

template<typename T>
struct hash : detail::functional_hash_base<T> {};

/* Floating hashes canonicalize both signed zero representations. */
template<>
struct hash<double> {
    size_t operator()(double value) const noexcept {
        static_assert(sizeof(double) == sizeof(unsigned long long),
                      "RinOS hash<double> requires binary64 storage");
        unsigned long long bits = 0;
        __builtin_memcpy(&bits, &value, sizeof(bits));
        if ((bits & 0x7fffffffffffffffull) == 0) bits = 0;
        return hash<unsigned long long>{}(bits);
    }
};

template<>
struct hash<float> {
    size_t operator()(float value) const noexcept {
        static_assert(sizeof(float) == sizeof(unsigned int),
                      "RinOS hash<float> requires binary32 storage");
        unsigned int bits = 0;
        __builtin_memcpy(&bits, &value, sizeof(bits));
        if ((bits & 0x7fffffffu) == 0) bits = 0;
        return hash<unsigned int>{}(bits);
    }
};

template<>
struct hash<long double> {
    size_t operator()(long double value) const noexcept {
#if __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
    __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        /* i686/x86_64 store the 80-bit value before any ABI padding. */
        unsigned char bytes[10] = {};
        __builtin_memcpy(bytes, &value, sizeof(bytes));
        unsigned int exponent = static_cast<unsigned int>(bytes[8]) |
            (static_cast<unsigned int>(bytes[9] & 0x7fu) << 8);
        bool zero_significand = true;
        for (size_t index = 0; index < 8; ++index) {
            if (bytes[index] != 0) zero_significand = false;
        }
        if (exponent == 0 && zero_significand) {
            bytes[9] = 0;  /* +0 and -0 must hash identically. */
        } else if (exponent == 0 && (bytes[7] & 0x80u) != 0) {
            /* Canonicalize an x87 pseudo-denormal to its equal normal form. */
            bytes[8] = 1;
        }
        return detail::functional_hash_bytes(bytes, sizeof(bytes));
#elif __LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024 && \
      __SIZEOF_LONG_DOUBLE__ == 8
        double binary64 = 0;
        __builtin_memcpy(&binary64, &value, sizeof(binary64));
        return hash<double>{}(binary64);
#elif __LDBL_MANT_DIG__ == 113 && __LDBL_MAX_EXP__ == 16384 && \
      __SIZEOF_LONG_DOUBLE__ == 16 && \
      (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
       __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
        /* IEEE binary128 has no padding in its 16-byte interchange
         * encoding.  Canonicalize both signed zeros before hashing the
         * representation so equal values share a hash without narrowing to
         * binary64. */
        unsigned char bytes[16] = {};
        __builtin_memcpy(bytes, &value, sizeof(bytes));
        bool zero = true;
        for (size_t index = 0; index != sizeof(bytes); ++index) {
            const unsigned char mask =
                (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ && index == 15u) ||
                (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ && index == 0u)
                    ? 0x7fu : 0xffu;
            if ((bytes[index] & mask) != 0u) zero = false;
        }
        if (zero) {
            if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) bytes[15] = 0;
            else bytes[0] = 0;
        }
        return detail::functional_hash_bytes(bytes, sizeof(bytes));
#elif __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
      __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ && \
      __SIZEOF_LONG_DOUBLE__ >= 10
        /* Big-endian x87 keeps the sign/exponent word before the
         * significand.  Hash only the 80-bit payload (not ABI padding), and
         * normalize signed zero and pseudo-denormal spellings exactly as in
         * the little-endian path above. */
        unsigned char bytes[10] = {};
        __builtin_memcpy(bytes, &value, sizeof(bytes));
        unsigned int exponent =
            (static_cast<unsigned int>(bytes[0] & 0x7fu) << 8) |
            static_cast<unsigned int>(bytes[1]);
        bool zero_significand = true;
        for (size_t index = 2; index < sizeof(bytes); ++index) {
            if (bytes[index] != 0) zero_significand = false;
        }
        if (exponent == 0 && zero_significand) {
            bytes[0] &= 0x7fu;  /* +0 and -0 hash identically. */
        } else if (exponent == 0 && (bytes[2] & 0x80u) != 0) {
            /* Canonicalize an x87 pseudo-denormal to its equal normal form. */
            bytes[1] = 1;
        }
        return detail::functional_hash_bytes(bytes, sizeof(bytes));
#elif __LDBL_MANT_DIG__ == 106 && __LDBL_MAX_EXP__ == 1024 && \
      __SIZEOF_LONG_DOUBLE__ == 16 && \
      (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
       __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
        /* The IBM double-double ABI stores a high binary64 followed by a
         * low binary64.  Hash the complete pair instead of narrowing to
         * binary64: the low component carries the precision that is lost by
         * the old fallback.  A zero low component has no value contribution,
         * so clear either sign; likewise canonicalize the low component of
         * infinities.  This keeps equivalent encodings stable without
         * attempting to normalize a NaN payload (NaNs are not equal). */
        unsigned char bytes[16] = {};
        __builtin_memcpy(bytes, &value, sizeof(bytes));
        unsigned long long high =
            detail::functional_hash_load_u64(bytes);
        unsigned long long low =
            detail::functional_hash_load_u64(bytes + sizeof(high));
        const unsigned long long exponent_mask =
            0x7ff0000000000000ull;
        const unsigned long long fraction_mask =
            0x000fffffffffffffull;
        if ((low & 0x7fffffffffffffffull) == 0ull) {
            low = 0ull;
            detail::functional_hash_store_u64(bytes + sizeof(high), low);
        }
        if ((high & exponent_mask) == exponent_mask &&
            (high & fraction_mask) == 0ull) {
            low = 0ull;
            detail::functional_hash_store_u64(bytes + sizeof(high), low);
        }
        if ((high & 0x7fffffffffffffffull) == 0ull &&
            (low & 0x7fffffffffffffffull) == 0ull) {
            high = 0ull;
            low = 0ull;
            detail::functional_hash_store_u64(bytes, high);
            detail::functional_hash_store_u64(bytes + sizeof(high), low);
        }
        return detail::functional_hash_bytes(bytes, sizeof(bytes));
#else
        /* Unknown ABIs must not narrow to binary64: two unequal extended
         * values can otherwise collapse to the same hash.  Snapshot the
         * complete object representation and canonicalize the only
         * representation-independent equality mandated here, signed zero.
         * The byte buffer starts zeroed so any ABI tail padding is stable for
         * a value whose representation does not initialize it. */
        unsigned char bytes[sizeof(long double)] = {};
        __builtin_memcpy(bytes, &value, sizeof(bytes));
        if (value == static_cast<long double>(0)) {
            for (size_t index = 0; index != sizeof(bytes); ++index)
                bytes[index] = 0;
        }
        return detail::functional_hash_bytes(bytes, sizeof(bytes));
#endif
    }
};

namespace detail {

template<typename Pointer>
inline size_t functional_pointer_hash(Pointer pointer, true_type) noexcept {
    return static_cast<size_t>(object_pointer_hash(pointer));
}

template<typename Pointer>
inline size_t functional_pointer_hash(Pointer pointer, false_type) noexcept {
    return static_cast<size_t>(function_pointer_hash(pointer));
}

} /* namespace detail */

/* ポインタのハッシュ */
template<typename T>
struct hash<T*> {
    size_t operator()(T* ptr) const noexcept {
        return detail::functional_pointer_hash(
            ptr, integral_constant<bool,
                detail::is_object_pointer<T*>::value>());
    }
};

#if __cplusplus >= 201703L
template<>
struct hash<nullptr_t> {
    size_t operator()(nullptr_t) const noexcept {
        return hash<void*>{}(nullptr);
    }
};
#endif

/* unique_ptr hash participates only when its possibly fancy pointer hash does. */
template<typename T, typename Deleter>
class unique_ptr;

namespace detail {

template<typename UniquePtr, typename Pointer = typename UniquePtr::pointer,
         bool Enabled = conjunction<
             is_default_constructible<hash<Pointer>>,
             is_copy_constructible<hash<Pointer>>,
             is_copy_assignable<hash<Pointer>>,
             is_destructible<hash<Pointer>>,
             is_swappable<hash<Pointer>>,
             is_invocable_r<size_t, const hash<Pointer>&, Pointer>>::value>
struct functional_unique_ptr_hash
    : functional_hash_base<UniquePtr, false> {};

template<typename UniquePtr, typename Pointer>
struct functional_unique_ptr_hash<UniquePtr, Pointer, true> {
    size_t operator()(const UniquePtr& pointer) const
        noexcept(noexcept(hash<Pointer>{}(pointer.get()))) {
        return hash<Pointer>{}(pointer.get());
    }
};

} /* namespace detail */

template<typename T, typename Deleter>
struct hash<unique_ptr<T, Deleter>>
    : detail::functional_unique_ptr_hash<unique_ptr<T, Deleter>> {};

#if defined(__cpp_lib_indirect)
namespace detail {

template<typename Indirect, typename Value, typename = void>
struct functional_indirect_hash : functional_hash_base<Indirect, false> {};

template<typename Indirect, typename Value>
struct functional_indirect_hash<Indirect, Value, void_t<decltype(
    hash<Value>{}(declval<const Value&>()))>> {
    size_t operator()(const Indirect& value) const
        noexcept(noexcept(hash<Value>{}(*value))) {
        return value.valueless_after_move()
            ? size_t(0)
            : hash<Value>{}(*value);
    }
};

} // namespace detail

template<typename T, typename Allocator>
struct hash<indirect<T, Allocator>>
    : detail::functional_indirect_hash<indirect<T, Allocator>, T> {};
#endif

/* shared_ptrのハッシュ（forward declaration） */
template<typename T>
class shared_ptr;

template<typename T>
struct hash<shared_ptr<T>> {
    size_t operator()(const shared_ptr<T>& ptr) const
        noexcept(noexcept(hash<T*>{}(ptr.get()))) {
        return hash<T*>{}(ptr.get());
    }
};

#undef RIN_FUNCTIONAL_CONSTEXPR14

} /* namespace std */

#endif /* __cplusplus */

#undef RIN_CXX_FUNCTION_HAS_STATIC_TYPEINFO
#endif /* RINCXX_FUNCTIONAL_H */
