/*
 * RinOS C++ Smart Pointers ✿
 * unique_ptr / shared_ptr 実装
 */

#ifndef RINCXX_MEMORY_H
#define RINCXX_MEMORY_H

#include "rincxx.h"
#include "type_traits.h"
#include "iterator.h"
#include "utility.h"
#include "pointer_order.h"
#if __cplusplus >= 202002L
#include "compare.h"
#endif

#ifdef __cplusplus

namespace std {

#if __cplusplus >= 201402L
#define RIN_MEMORY_CONSTEXPR14 constexpr
#else
#define RIN_MEMORY_CONSTEXPR14 inline
#endif

#if __cplusplus >= 201703L
#define RIN_MEMORY_NODISCARD [[nodiscard]]
#else
#define RIN_MEMORY_NODISCARD
#endif

/* ═══════════════════════════════════════════════════════════════
 * unique_ptr
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

template<typename T, typename Deleter, typename = void>
struct unique_ptr_pointer {
    using type = T*;
};

template<typename T, typename Deleter>
struct unique_ptr_pointer<
    T, Deleter,
    void_t<typename remove_reference<Deleter>::type::pointer>> {
    using type = typename remove_reference<Deleter>::type::pointer;
};

} /* namespace detail */

template<typename T>
struct default_delete {
    constexpr default_delete() noexcept = default;

    template<typename U,
             typename = typename enable_if<is_convertible<U*, T*>::value>::type>
    constexpr default_delete(const default_delete<U>&) noexcept {}

    void operator()(T* ptr) const noexcept {
        delete ptr;
    }
};

template<typename T>
struct default_delete<T[]> {
    constexpr default_delete() noexcept = default;

    /* U* が T* に変換可能な場合に変換コンストラクタを有効にする */
    template<typename U, typename = typename enable_if<is_convertible<U(*)[], T(*)[]>::value>::type>
    constexpr default_delete(const default_delete<U[]>&) noexcept {}

    void operator()(T* ptr) const noexcept {
        delete[] ptr;
    }
};

template<typename T, typename Deleter = default_delete<T>>
class unique_ptr {
public:
    using pointer = typename detail::unique_ptr_pointer<T, Deleter>::type;
    using element_type = T;
    using deleter_type = Deleter;

private:
    pointer m_ptr;
    Deleter m_deleter;

public:
    /* コンストラクタ */
    constexpr unique_ptr() noexcept : m_ptr(nullptr), m_deleter() {}
    constexpr unique_ptr(nullptr_t) noexcept : m_ptr(nullptr), m_deleter() {}
    
    explicit unique_ptr(pointer p) noexcept : m_ptr(p), m_deleter() {}
    
    unique_ptr(pointer p, const Deleter& d) noexcept : m_ptr(p), m_deleter(d) {}
    
    unique_ptr(unique_ptr&& u) noexcept
        : m_ptr(u.m_ptr), m_deleter(move(u.m_deleter)) {
        u.m_ptr = nullptr;
    }

    /* 変換ムーブコンストラクタ - U* が T* に変換可能な場合のみ有効 */
    template<typename U, typename E,
             typename = enable_if_t<
                                    is_convertible<
                                        typename unique_ptr<U, E>::pointer,
                                        pointer>::value &&
                                    !is_array<U>::value &&
                                    is_constructible<Deleter, E&&>::value>>
    unique_ptr(unique_ptr<U, E>&& u) noexcept
        : m_ptr(u.release()), m_deleter(move(u.get_deleter())) {}

    /* コピー禁止 */
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;
    
    ~unique_ptr() {
        if (m_ptr) m_deleter(m_ptr);
    }
    
    /* 代入 */
    unique_ptr& operator=(unique_ptr&& r) noexcept {
        if (this != &r) {
            reset(r.release());
            m_deleter = move(r.m_deleter);
        }
        return *this;
    }

    /* 変換ムーブ代入演算子 - U* が T* に変換可能な場合のみ有効 */
    template<typename U, typename E,
             typename = enable_if_t<is_convertible<U*, T*>::value &&
                                    !is_array<U>::value &&
                                    is_constructible<Deleter, E&&>::value>>
    unique_ptr& operator=(unique_ptr<U, E>&& r) noexcept {
        reset(r.release());
        m_deleter = move(r.get_deleter());
        return *this;
    }

    unique_ptr& operator=(nullptr_t) noexcept {
        reset();
        return *this;
    }
    
    /* アクセス */
    pointer get() const noexcept { return m_ptr; }
    Deleter& get_deleter() noexcept { return m_deleter; }
    const Deleter& get_deleter() const noexcept { return m_deleter; }
    
    explicit operator bool() const noexcept { return m_ptr != nullptr; }

    /* operator* - voidでない場合のみ有効 */
    template<typename U = T, typename = enable_if_t<!is_void<U>::value>>
    U& operator*() const noexcept { return *m_ptr; }
    pointer operator->() const noexcept { return m_ptr; }
    
    /* 変更 */
    pointer release() noexcept {
        pointer p = m_ptr;
        m_ptr = nullptr;
        return p;
    }
    
    void reset(pointer p = pointer()) noexcept {
        pointer old = m_ptr;
        m_ptr = p;
        if (old) m_deleter(old);
    }
    
    void swap(unique_ptr& other) noexcept {
        std::swap(m_ptr, other.m_ptr);
        std::swap(m_deleter, other.m_deleter);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * unique_ptr<T[]> 配列特殊化
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, typename Deleter>
class unique_ptr<T[], Deleter> {
public:
    using pointer = typename detail::unique_ptr_pointer<T, Deleter>::type;
    using element_type = T;
    using deleter_type = Deleter;

private:
    pointer m_ptr;
    Deleter m_deleter;

public:
    /* コンストラクタ */
    constexpr unique_ptr() noexcept : m_ptr(nullptr), m_deleter() {}
    constexpr unique_ptr(nullptr_t) noexcept : m_ptr(nullptr), m_deleter() {}

    explicit unique_ptr(pointer p) noexcept : m_ptr(p), m_deleter() {}

    unique_ptr(pointer p, const Deleter& d) noexcept : m_ptr(p), m_deleter(d) {}

    unique_ptr(unique_ptr&& u) noexcept
        : m_ptr(u.m_ptr), m_deleter(move(u.m_deleter)) {
        u.m_ptr = nullptr;
    }

    /* 異なる型からのムーブコンストラクタ (例: unique_ptr<T[]> -> unique_ptr<const T[]>) */
    template<typename U, typename E,
             typename = typename enable_if<
                 is_convertible<typename unique_ptr<U[], E>::pointer, pointer>::value &&
                 is_constructible<Deleter, E&&>::value
             >::type>
    unique_ptr(unique_ptr<U[], E>&& u) noexcept
        : m_ptr(u.release()), m_deleter(forward<E>(u.get_deleter())) {}

    /* コピー禁止 */
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;

    ~unique_ptr() {
        if (m_ptr) m_deleter(m_ptr);
    }

    /* 代入 */
    unique_ptr& operator=(unique_ptr&& r) noexcept {
        if (this != &r) {
            reset(r.release());
            m_deleter = move(r.m_deleter);
        }
        return *this;
    }

    /* 異なる型からの代入 (例: unique_ptr<T[]> -> unique_ptr<const T[]>) */
    template<typename U, typename E,
             typename = typename enable_if<
                 is_convertible<typename unique_ptr<U[], E>::pointer, pointer>::value &&
                 is_constructible<Deleter, E&&>::value
             >::type>
    unique_ptr& operator=(unique_ptr<U[], E>&& r) noexcept {
        reset(r.release());
        m_deleter = forward<E>(r.get_deleter());
        return *this;
    }

    unique_ptr& operator=(nullptr_t) noexcept {
        reset();
        return *this;
    }

    /* アクセス */
    pointer get() const noexcept { return m_ptr; }
    Deleter& get_deleter() noexcept { return m_deleter; }
    const Deleter& get_deleter() const noexcept { return m_deleter; }

    explicit operator bool() const noexcept { return m_ptr != nullptr; }

    /* 配列アクセス */
    T& operator[](size_t i) const noexcept { return m_ptr[i]; }

    /* 変更 */
    pointer release() noexcept {
        pointer p = m_ptr;
        m_ptr = nullptr;
        return p;
    }

    void reset(pointer p = pointer()) noexcept {
        pointer old = m_ptr;
        m_ptr = p;
        if (old) m_deleter(old);
    }

    void swap(unique_ptr& other) noexcept {
        std::swap(m_ptr, other.m_ptr);
        std::swap(m_deleter, other.m_deleter);
    }
};

/* make_unique - non-array version (enabled only for non-array types) */
template<typename T, typename... Args>
enable_if_t<!is_array<T>::value, unique_ptr<T>>
make_unique(Args&&... args) {
    return unique_ptr<T>(new T(forward<Args>(args)...));
}

/* make_unique for unbounded arrays T[] */
template<typename T>
enable_if_t<is_array<T>::value && extent<T>::value == 0, unique_ptr<T>>
make_unique(size_t n) {
    return unique_ptr<T>(new typename remove_extent<T>::type[n]());
}

/* make_unique for bounded arrays T[N] is deleted */
template<typename T, typename... Args>
enable_if_t<is_array<T>::value && extent<T>::value != 0, void>
make_unique(Args&&...) = delete;

/* make_unique_for_overwrite (C++20) - array version without value initialization */
template<typename T>
enable_if_t<is_array<T>::value && extent<T>::value == 0, unique_ptr<T>>
make_unique_for_overwrite(size_t n) {
    return unique_ptr<T>(new typename remove_extent<T>::type[n]);
}

/* make_unique_for_overwrite - non-array version */
template<typename T>
enable_if_t<!is_array<T>::value, unique_ptr<T>>
make_unique_for_overwrite() {
    return unique_ptr<T>(new T);
}

/* ═══════════════════════════════════════════════════════════════
 * unique_ptr comparison operators
 * ═══════════════════════════════════════════════════════════════*/

template<typename T1, typename D1, typename T2, typename D2>
bool operator==(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) {
    return a.get() == b.get();
}

template<typename T1, typename D1, typename T2, typename D2>
bool operator!=(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) {
    return a.get() != b.get();
}

template<typename T, typename D>
bool operator==(const unique_ptr<T, D>& p, nullptr_t) noexcept {
    return !p;
}

template<typename T, typename D>
bool operator==(nullptr_t, const unique_ptr<T, D>& p) noexcept {
    return !p;
}

template<typename T, typename D>
bool operator!=(const unique_ptr<T, D>& p, nullptr_t) noexcept {
    return static_cast<bool>(p);
}

template<typename T, typename D>
bool operator!=(nullptr_t, const unique_ptr<T, D>& p) noexcept {
    return static_cast<bool>(p);
}

template<typename T1, typename D1, typename T2, typename D2>
bool operator<(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) {
    return a.get() < b.get();
}

template<typename T1, typename D1, typename T2, typename D2>
bool operator<=(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) {
    return a.get() <= b.get();
}

template<typename T1, typename D1, typename T2, typename D2>
bool operator>(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) {
    return a.get() > b.get();
}

template<typename T1, typename D1, typename T2, typename D2>
bool operator>=(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) {
    return a.get() >= b.get();
}

#if __cplusplus >= 202002L
template<typename T1, typename D1, typename T2, typename D2,
         enable_if_t<
             is_pointer<typename unique_ptr<T1, D1>::pointer>::value &&
             is_pointer<typename unique_ptr<T2, D2>::pointer>::value,
             int> = 0>
strong_ordering operator<=>(const unique_ptr<T1, D1>& a,
                           const unique_ptr<T2, D2>& b) noexcept {
    const auto left = a.get();
    const auto right = b.get();
    if (detail::object_pointer_equal(left, right)) {
        return strong_ordering::equal;
    }
    return detail::object_pointer_total_less(left, right)
        ? strong_ordering::less : strong_ordering::greater;
}

template<typename T, typename D>
strong_ordering operator<=>(const unique_ptr<T, D>& pointer,
                           nullptr_t) noexcept {
    return pointer ? strong_ordering::greater : strong_ordering::equal;
}

template<typename T, typename D>
strong_ordering operator<=>(nullptr_t,
                           const unique_ptr<T, D>& pointer) noexcept {
    return pointer ? strong_ordering::less : strong_ordering::equal;
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * shared_ptr
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {
template<typename Deleter>
inline const void* shared_ptr_deleter_key() noexcept {
    static const int token = 0;
    return &token;
}
} /* namespace detail */

struct shared_ptr_control_block {
    size_t strong_count;
    size_t weak_count;
    void* owned_pointer;
    void (*destroy_owned)(void*);
    const void* deleter_key;
    
    shared_ptr_control_block(void* pointer, void (*destroy)(void*),
                             const void* key = nullptr)
        : strong_count(1), weak_count(1), owned_pointer(pointer),
          destroy_owned(destroy), deleter_key(key) {}
};

/* A unique_ptr conversion must retain the source deleter (including its
 * state) until the last shared owner is released.  Store the pointer and
 * deleter in one control-block-owned holder instead of silently falling back
 * to delete-expression semantics. */
template<typename T, typename Deleter>
struct shared_ptr_unique_deleter_holder {
    T* pointer;
    Deleter deleter;

    shared_ptr_unique_deleter_holder(T* value, Deleter&& owner)
        : pointer(value), deleter(std::move(owner)) {}
};

template<typename T, typename Deleter>
inline void shared_ptr_destroy_unique_holder(void* state) noexcept {
    typedef shared_ptr_unique_deleter_holder<T, Deleter> holder_type;
    holder_type* holder = static_cast<holder_type*>(state);
    holder->deleter(holder->pointer);
    holder->~holder_type();
    rin_free(holder);
}

/* Reference counts are shared by independent smart-pointer objects and may
 * be touched concurrently.  Keep the count operations in one small owner so
 * weak_ptr::lock() can acquire a strong reference without racing the final
 * release.  The fallback is retained for non-GNU freestanding compilers that
 * do not expose the __atomic builtins. */
inline size_t shared_ptr_count_load(const size_t* count) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return __atomic_load_n(count, __ATOMIC_ACQUIRE);
#else
    return *count;
#endif
}

inline void shared_ptr_count_increment(size_t* count) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    size_t observed = __atomic_load_n(count, __ATOMIC_RELAXED);
    for (;;) {
        if (observed == static_cast<size_t>(-1))
            rin_panic("shared_ptr reference count overflow");
        if (__atomic_compare_exchange_n(
                count, &observed, observed + static_cast<size_t>(1), false,
                __ATOMIC_RELAXED, __ATOMIC_RELAXED))
            return;
    }
#else
    if (*count == static_cast<size_t>(-1))
        rin_panic("shared_ptr reference count overflow");
    ++*count;
#endif
}

inline bool shared_ptr_count_decrement(size_t* count) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return __atomic_sub_fetch(count, static_cast<size_t>(1),
                              __ATOMIC_ACQ_REL) == 0;
#else
    return --*count == 0;
#endif
}

inline bool shared_ptr_count_try_increment(size_t* count) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    size_t observed = __atomic_load_n(count, __ATOMIC_ACQUIRE);
    for (;;) {
        if (observed == 0) return false;
        if (observed == static_cast<size_t>(-1))
            rin_panic("shared_ptr reference count overflow");
        if (__atomic_compare_exchange_n(
                count, &observed, observed + static_cast<size_t>(1), false,
                __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            return true;
    }
#else
    if (*count == 0) return false;
    if (*count == static_cast<size_t>(-1))
        rin_panic("shared_ptr reference count overflow");
    ++*count;
    return true;
#endif
}

template<typename T>
class weak_ptr;

template<typename T>
class shared_ptr;

template<typename T>
class allocator;

template<typename T, typename Alloc, typename... Args,
         typename enable_if<!is_array<T>::value, int>::type = 0>
shared_ptr<T> allocate_shared(const Alloc& alloc, Args&&... args);

template<typename Array, typename Alloc, typename... Args>
shared_ptr<Array> allocate_shared_array_impl(const Alloc&, size_t, Args&&...);

template<typename T>
class enable_shared_from_this;

template<typename T>
struct owner_less;

namespace detail {

/* shared_ptr<T[]> stores an element pointer but owns the complete array. */
template<typename T>
struct shared_ptr_default_destroy {
    static void run(void* pointer) noexcept {
        delete static_cast<T*>(pointer);
    }
};

template<typename T>
struct shared_ptr_default_destroy<T[]> {
    static void run(void* pointer) noexcept {
        delete[] static_cast<T*>(pointer);
    }
};

template<typename T, size_t N>
struct shared_ptr_default_destroy<T[N]> {
    static void run(void* pointer) noexcept {
        delete[] static_cast<T*>(pointer);
    }
};

/* The C++11 shared-pointer atomic free functions predate atomic<T>
 * specializations.  A single process-wide gate is deliberately conservative:
 * it provides the required indivisible snapshot/replace contract without
 * exposing a lock-free claim that the target runtime cannot prove. */
inline volatile unsigned int& shared_ptr_atomic_gate() noexcept {
    static volatile unsigned int gate = 0u;
    return gate;
}

inline void shared_ptr_atomic_lock() noexcept {
#if defined(__GNUC__) || defined(__clang__)
    while (__sync_lock_test_and_set(&shared_ptr_atomic_gate(), 1u) != 0u)
        __asm__ volatile("pause" ::: "memory");
#else
    while (shared_ptr_atomic_gate() != 0u) {}
    shared_ptr_atomic_gate() = 1u;
#endif
}

inline void shared_ptr_atomic_unlock() noexcept {
#if defined(__GNUC__) || defined(__clang__)
    __sync_lock_release(&shared_ptr_atomic_gate());
#else
    shared_ptr_atomic_gate() = 0u;
#endif
}

} /* namespace detail */

template<typename T>
class shared_ptr {
    template<typename U> friend class shared_ptr;
    template<typename U> friend class weak_ptr;
    template<typename U> friend class enable_shared_from_this;
    template<typename T1, typename U1> friend shared_ptr<T1> static_pointer_cast(const shared_ptr<U1>&) noexcept;
    template<typename T1, typename U1> friend shared_ptr<T1> static_pointer_cast(shared_ptr<U1>&&) noexcept;
    template<typename T1, typename U1> friend shared_ptr<T1> dynamic_pointer_cast(const shared_ptr<U1>&) noexcept;
    template<typename T1, typename U1> friend shared_ptr<T1> dynamic_pointer_cast(shared_ptr<U1>&&) noexcept;
    template<typename T1, typename U1> friend shared_ptr<T1> const_pointer_cast(const shared_ptr<U1>&) noexcept;
    template<typename T1, typename U1> friend shared_ptr<T1> const_pointer_cast(shared_ptr<U1>&&) noexcept;
    template<typename T1, typename U1> friend shared_ptr<T1> reinterpret_pointer_cast(const shared_ptr<U1>&) noexcept;
    template<typename T1, typename U1> friend shared_ptr<T1> reinterpret_pointer_cast(shared_ptr<U1>&&) noexcept;
    template<typename U> friend class owner_less;
    template<typename T1, typename Alloc, typename... Args,
             typename enable_if<!is_array<T1>::value, int>::type>
    friend shared_ptr<T1> allocate_shared(const Alloc&, Args&&...);
    template<typename Array, typename Alloc, typename... Args>
    friend shared_ptr<Array> allocate_shared_array_impl(const Alloc&, size_t,
                                                        Args&&...);

public:
    using element_type = typename remove_extent<T>::type;
    using weak_type = weak_ptr<T>;

private:
    element_type* m_ptr;
    shared_ptr_control_block* m_ctrl;

    void release() {
        if (m_ctrl) {
            if (shared_ptr_count_decrement(&m_ctrl->strong_count)) {
                if (m_ctrl->destroy_owned)
                    m_ctrl->destroy_owned(m_ctrl->owned_pointer);
                if (shared_ptr_count_decrement(&m_ctrl->weak_count)) {
                    rin_free(m_ctrl);
                }
            }
        }
        m_ptr = nullptr;
        m_ctrl = nullptr;
    }

    /* enable_shared_from_this サポート */
    /* T が enable_shared_from_this<U> を継承している場合に m_weak_this を設定 */
    template<typename U>
    void _setup_enable_shared_from_this(enable_shared_from_this<U>* esft) {
        if (esft && m_ctrl && esft->m_weak_this.expired()) {
            /* A second shared_ptr constructed from the same raw object must
             * not steal the established enable_shared_from_this owner.  An
             * expired weak owner may be replaced, but release it first so
             * its weak count and control block lifetime remain balanced. */
            esft->m_weak_this.reset();
            /* weak_ptr を直接構築して m_weak_this に設定 */
            esft->m_weak_this.m_ptr = static_cast<U*>(m_ptr);
            esft->m_weak_this.m_ctrl = m_ctrl;
            shared_ptr_count_increment(&m_ctrl->weak_count);
        }
    }

    /* enable_shared_from_this を継承していない場合のフォールバック */
    void _setup_enable_shared_from_this(...) {
        /* 何もしない */
    }

public:
    /* コンストラクタ */
    constexpr shared_ptr() noexcept : m_ptr(nullptr), m_ctrl(nullptr) {}
    constexpr shared_ptr(nullptr_t) noexcept : m_ptr(nullptr), m_ctrl(nullptr) {}

    explicit shared_ptr(element_type* p) : m_ptr(p), m_ctrl(nullptr) {
        if (p) {
            m_ctrl = static_cast<shared_ptr_control_block*>(
                rin_malloc(sizeof(shared_ptr_control_block)));
            if (!m_ctrl) {
                detail::shared_ptr_default_destroy<T>::run(
                    const_cast<void*>(static_cast<const void*>(p)));
                m_ptr = nullptr;
                rin_panic("shared_ptr control block allocation failed");
            }
            new (m_ctrl) shared_ptr_control_block(
                const_cast<void*>(static_cast<const void*>(p)),
                &detail::shared_ptr_default_destroy<T>::run);
            _setup_enable_shared_from_this(p);
        }
    }

    /* Custom deleter ownership follows the same holder path used by the
     * unique_ptr conversion.  The deleter is moved into control-block-owned
     * storage before the source pointer becomes visible to shared owners. */
    template<typename Deleter>
    shared_ptr(element_type* p, Deleter deleter)
        : m_ptr(nullptr), m_ctrl(nullptr) {
        if (!p) return;
        using deleter_type = typename decay<Deleter>::type;
        using holder_type = shared_ptr_unique_deleter_holder<element_type,
                                                            deleter_type>;
        holder_type* holder = static_cast<holder_type*>(
            rin_malloc(sizeof(holder_type)));
        if (!holder) {
            deleter(p);
            rin_panic("shared_ptr deleter allocation failed");
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            new (holder) holder_type(p, std::move(deleter));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            rin_free(holder);
            deleter(p);
            throw;
        }
#endif
        m_ctrl = static_cast<shared_ptr_control_block*>(
            rin_malloc(sizeof(shared_ptr_control_block)));
        if (!m_ctrl) {
            shared_ptr_destroy_unique_holder<element_type, deleter_type>(
                holder);
            rin_panic("shared_ptr control block allocation failed");
        }
        m_ptr = p;
        new (m_ctrl) shared_ptr_control_block(
            static_cast<void*>(holder),
            &shared_ptr_destroy_unique_holder<element_type, deleter_type>,
            detail::shared_ptr_deleter_key<deleter_type>());
        _setup_enable_shared_from_this(p);
    }
    
    shared_ptr(const shared_ptr& r) noexcept : m_ptr(r.m_ptr), m_ctrl(r.m_ctrl) {
        if (m_ctrl) shared_ptr_count_increment(&m_ctrl->strong_count);
    }
    
    shared_ptr(shared_ptr&& r) noexcept : m_ptr(r.m_ptr), m_ctrl(r.m_ctrl) {
        r.m_ptr = nullptr;
        r.m_ctrl = nullptr;
    }
    
    /* weak_ptrからのコンストラクタ - 定義はweak_ptrの後 */
    template<typename U,
             typename enable_if<is_convertible<
                 typename remove_extent<U>::type*,
                 element_type*>::value, int>::type = 0>
    explicit shared_ptr(const weak_ptr<U>& r);
    
    template<typename U,
             typename enable_if<is_convertible<
                 typename shared_ptr<U>::element_type*,
                 element_type*>::value, int>::type = 0>
    shared_ptr(const shared_ptr<U>& r) noexcept
        : m_ptr(r.m_ptr), m_ctrl(r.m_ctrl) {
        if (m_ctrl) shared_ptr_count_increment(&m_ctrl->strong_count);
    }

    template<typename U,
             typename enable_if<is_convertible<
                 typename shared_ptr<U>::element_type*,
                 element_type*>::value, int>::type = 0>
    shared_ptr(shared_ptr<U>&& r) noexcept
        : m_ptr(r.m_ptr), m_ctrl(r.m_ctrl) {
        r.m_ptr = nullptr;
        r.m_ctrl = nullptr;
    }

    /* Aliasing constructors preserve the owner's control block while
     * exposing an independently selected pointer.  The control block stores
     * the object it owns, so releasing an alias cannot delete a subobject. */
    template<typename U>
    shared_ptr(const shared_ptr<U>& owner, T* pointer) noexcept
        : m_ptr(pointer), m_ctrl(owner.m_ctrl) {
        if (m_ctrl) shared_ptr_count_increment(&m_ctrl->strong_count);
    }

    template<typename U>
    shared_ptr(shared_ptr<U>&& owner, T* pointer) noexcept
        : m_ptr(pointer), m_ctrl(owner.m_ctrl) {
        owner.m_ptr = nullptr;
        owner.m_ctrl = nullptr;
    }
    
    ~shared_ptr() {
        release();
    }
    
    /* 代入 */
    shared_ptr& operator=(const shared_ptr& r) noexcept {
        if (this != &r) {
            release();
            m_ptr = r.m_ptr;
            m_ctrl = r.m_ctrl;
            if (m_ctrl) shared_ptr_count_increment(&m_ctrl->strong_count);
        }
        return *this;
    }
    
    shared_ptr& operator=(shared_ptr&& r) noexcept {
        if (this != &r) {
            release();
            m_ptr = r.m_ptr;
            m_ctrl = r.m_ctrl;
            r.m_ptr = nullptr;
            r.m_ctrl = nullptr;
        }
        return *this;
    }

    template<typename U,
             typename enable_if<is_convertible<
                 typename shared_ptr<U>::element_type*,
                 element_type*>::value, int>::type = 0>
    shared_ptr& operator=(const shared_ptr<U>& r) noexcept {
        if (m_ctrl != r.m_ctrl || m_ptr != r.m_ptr) {
            release();
            m_ptr = r.m_ptr;
            m_ctrl = r.m_ctrl;
            if (m_ctrl) shared_ptr_count_increment(&m_ctrl->strong_count);
        }
        return *this;
    }

    template<typename U,
             typename enable_if<is_convertible<
                 typename shared_ptr<U>::element_type*,
                 element_type*>::value, int>::type = 0>
    shared_ptr& operator=(shared_ptr<U>&& r) noexcept {
        release();
        m_ptr = r.m_ptr;
        m_ctrl = r.m_ctrl;
        r.m_ptr = nullptr;
        r.m_ctrl = nullptr;
        return *this;
    }

    /* unique_ptr からの変換コンストラクタ */
    template<typename U, typename Deleter>
    shared_ptr(unique_ptr<U, Deleter>&& r) : m_ptr(nullptr), m_ctrl(nullptr) {
        U* pointer = r.get();
        if (!pointer) return;

        typedef shared_ptr_unique_deleter_holder<U, Deleter> holder_type;
        holder_type* holder = static_cast<holder_type*>(
            rin_malloc(sizeof(holder_type)));
        if (!holder) {
            rin_panic("shared_ptr unique_ptr deleter allocation failed");
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            new (holder) holder_type(pointer, std::move(r.get_deleter()));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            rin_free(holder);
            throw;
        }
#endif
        r.release();

        m_ctrl = static_cast<shared_ptr_control_block*>(
            rin_malloc(sizeof(shared_ptr_control_block)));
        if (!m_ctrl) {
            shared_ptr_destroy_unique_holder<U, Deleter>(holder);
            rin_panic("shared_ptr control block allocation failed");
        }
        m_ptr = pointer;
        new (m_ctrl) shared_ptr_control_block(
            static_cast<void*>(holder),
            &shared_ptr_destroy_unique_holder<U, Deleter>,
            detail::shared_ptr_deleter_key<Deleter>());
        _setup_enable_shared_from_this(m_ptr);
    }

    /* unique_ptr からの代入演算子 */
    template<typename U, typename Deleter>
    shared_ptr& operator=(unique_ptr<U, Deleter>&& r) {
        shared_ptr replacement(std::move(r));
        swap(replacement);
        return *this;
    }

    /* アクセス */
    element_type* get() const noexcept { return m_ptr; }
    element_type& operator*() const noexcept { return *m_ptr; }
    element_type* operator->() const noexcept { return m_ptr; }

    template<typename U = T, typename enable_if<is_array<U>::value, int>::type = 0>
    element_type& operator[](size_t index) const noexcept { return m_ptr[index]; }

    template<typename Deleter>
    Deleter* get_deleter() noexcept {
        using key_type = typename remove_cv<Deleter>::type;
        if (!m_ctrl || m_ctrl->deleter_key !=
                          detail::shared_ptr_deleter_key<key_type>())
            return nullptr;
        using holder_type = shared_ptr_unique_deleter_holder<element_type,
                                                            key_type>;
        holder_type* holder = static_cast<holder_type*>(m_ctrl->owned_pointer);
        return static_cast<Deleter*>(&holder->deleter);
    }

    template<typename Deleter>
    const Deleter* get_deleter() const noexcept {
        using key_type = typename remove_cv<Deleter>::type;
        if (!m_ctrl || m_ctrl->deleter_key !=
                          detail::shared_ptr_deleter_key<key_type>())
            return nullptr;
        using holder_type = shared_ptr_unique_deleter_holder<element_type,
                                                            key_type>;
        const holder_type* holder = static_cast<const holder_type*>(
            m_ctrl->owned_pointer);
        return static_cast<const Deleter*>(&holder->deleter);
    }
    
    size_t use_count() const noexcept {
        return m_ctrl ? shared_ptr_count_load(&m_ctrl->strong_count) : 0;
    }
    
    bool unique() const noexcept {
        return use_count() == 1;
    }
    
    explicit operator bool() const noexcept { return m_ptr != nullptr; }
    
    /* 変更 */
    void reset() noexcept {
        release();
    }
    
    void reset(element_type* p) {
        /* Construct the replacement before releasing the current owner.  This
         * preserves reset's strong guarantee when control-block allocation or
         * enable_shared_from_this binding fails. */
        shared_ptr replacement(p);
        swap(replacement);
    }
    
    void swap(shared_ptr& r) noexcept {
        std::swap(m_ptr, r.m_ptr);
        std::swap(m_ctrl, r.m_ctrl);
    }

    /* Compare ownership rather than stored pointers.  A shared_ptr alias
     * (including an empty pointer with a live control block in a future
     * implementation) must sort with every other owner of that control
     * block.  The control block address is converted through RinOS's
     * documented flat object-pointer total order instead of using relational
     * pointer operators on unrelated allocations. */
    template<typename U>
    bool owner_before(const shared_ptr<U>& other) const noexcept {
        return detail::object_pointer_total_less(m_ctrl, other.m_ctrl);
    }

    template<typename U>
    bool owner_before(const weak_ptr<U>& other) const noexcept {
        return detail::object_pointer_total_less(m_ctrl, other.m_ctrl);
    }
    
    /* 比較演算子 */
    template<typename U>
    bool operator==(const shared_ptr<U>& r) const noexcept {
        return m_ptr == r.get();
    }
    
    template<typename U>
    bool operator!=(const shared_ptr<U>& r) const noexcept {
        return m_ptr != r.get();
    }
    
    bool operator==(nullptr_t) const noexcept {
        return m_ptr == nullptr;
    }
    
    bool operator!=(nullptr_t) const noexcept {
        return m_ptr != nullptr;
    }
};

/* shared_ptr 非メンバ比較演算子 */
template<typename T>
bool operator==(nullptr_t, const shared_ptr<T>& r) noexcept {
    return r.get() == nullptr;
}

template<typename T>
bool operator!=(nullptr_t, const shared_ptr<T>& r) noexcept {
    return r.get() != nullptr;
}

#if __cplusplus >= 202002L
template<typename T1, typename T2,
         enable_if_t<!is_function<T1>::value && !is_function<T2>::value,
                     int> = 0>
strong_ordering operator<=>(const shared_ptr<T1>& left,
                           const shared_ptr<T2>& right) noexcept {
    if (detail::object_pointer_equal(left.get(), right.get())) {
        return strong_ordering::equal;
    }
    return detail::object_pointer_total_less(left.get(), right.get())
        ? strong_ordering::less : strong_ordering::greater;
}

template<typename T>
strong_ordering operator<=>(const shared_ptr<T>& pointer,
                           nullptr_t) noexcept {
    return pointer.get() != nullptr
        ? strong_ordering::greater : strong_ordering::equal;
}

template<typename T>
strong_ordering operator<=>(nullptr_t,
                           const shared_ptr<T>& pointer) noexcept {
    return pointer.get() != nullptr
        ? strong_ordering::less : strong_ordering::equal;
}
#endif

template<typename Deleter, typename T>
Deleter* get_deleter(const shared_ptr<T>& owner) noexcept {
    return const_cast<shared_ptr<T>&>(owner).template get_deleter<Deleter>();
}

/* make_shared */
template<typename T, typename... Args,
         typename enable_if<!is_array<T>::value, int>::type = 0>
shared_ptr<T> make_shared(Args&&... args) {
    return allocate_shared<T>(allocator<T>(),
                              forward<Args>(args)...);
}

template<typename T,
         typename enable_if<is_array<T>::value &&
                            (extent<T>::value == 0), int>::type = 0>
shared_ptr<T> make_shared(size_t count) {
    return allocate_shared<T>(allocator<typename remove_extent<T>::type>(),
                              count);
}

template<typename T,
         typename enable_if<is_array<T>::value &&
                            (extent<T>::value == 0), int>::type = 0>
shared_ptr<T> make_shared(size_t count,
                          const typename remove_extent<T>::type& value) {
    return allocate_shared<T>(allocator<typename remove_extent<T>::type>(),
                              count, value);
}

template<typename T,
         typename enable_if<is_array<T>::value &&
                            (extent<T>::value != 0), int>::type = 0>
shared_ptr<T> make_shared() {
    return allocate_shared<T>(allocator<typename remove_extent<T>::type>());
}

template<typename T,
         typename enable_if<is_array<T>::value &&
                            (extent<T>::value != 0), int>::type = 0>
shared_ptr<T> make_shared(const typename remove_extent<T>::type& value) {
    return allocate_shared<T>(allocator<typename remove_extent<T>::type>(),
                              value);
}

#if __cplusplus >= 202002L
/* C++20's overwrite form intentionally performs default initialization.  It
 * is useful for trivially default-initialized buffers while retaining the
 * ordinary shared_ptr control-block and destruction path. */
template<typename T,
         typename enable_if<!is_array<T>::value, int>::type = 0>
shared_ptr<T> make_shared_for_overwrite() {
    return shared_ptr<T>(new T);
}

template<typename T,
         typename enable_if<is_array<T>::value &&
                            (extent<T>::value == 0), int>::type = 0>
shared_ptr<T> make_shared_for_overwrite(size_t count) {
    using element_type = typename remove_extent<T>::type;
    return allocate_shared_for_overwrite<T>(allocator<element_type>(), count);
}

template<typename T,
         typename enable_if<is_array<T>::value &&
                            (extent<T>::value != 0), int>::type = 0>
shared_ptr<T> make_shared_for_overwrite() {
    using element_type = typename remove_extent<T>::type;
    return allocate_shared_for_overwrite<T>(allocator<element_type>());
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * weak_ptr
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
class weak_ptr {
    template<typename U> friend class shared_ptr;
    template<typename U> friend class weak_ptr;
    template<typename U> friend class owner_less;
    
private:
    using element_type = typename remove_extent<T>::type;
    element_type* m_ptr;
    shared_ptr_control_block* m_ctrl;

public:
    constexpr weak_ptr() noexcept : m_ptr(nullptr), m_ctrl(nullptr) {}
    
    weak_ptr(const shared_ptr<T>& r) noexcept : m_ptr(r.m_ptr), m_ctrl(r.m_ctrl) {
        if (m_ctrl) shared_ptr_count_increment(&m_ctrl->weak_count);
    }

    template<typename U,
             typename enable_if<is_convertible<
                 typename remove_extent<U>::type*,
                 element_type*>::value, int>::type = 0>
    weak_ptr(const shared_ptr<U>& r) noexcept
        : m_ptr(r.m_ptr), m_ctrl(r.m_ctrl) {
        if (m_ctrl) shared_ptr_count_increment(&m_ctrl->weak_count);
    }
    
    weak_ptr(const weak_ptr& r) noexcept : m_ptr(r.m_ptr), m_ctrl(r.m_ctrl) {
        if (m_ctrl) shared_ptr_count_increment(&m_ctrl->weak_count);
    }

    template<typename U,
             typename enable_if<is_convertible<
                 typename remove_extent<U>::type*,
                 element_type*>::value, int>::type = 0>
    weak_ptr(const weak_ptr<U>& r) noexcept
        : m_ptr(r.m_ptr), m_ctrl(r.m_ctrl) {
        if (m_ctrl) shared_ptr_count_increment(&m_ctrl->weak_count);
    }

    weak_ptr(weak_ptr&& r) noexcept : m_ptr(r.m_ptr), m_ctrl(r.m_ctrl) {
        r.m_ptr = nullptr;
        r.m_ctrl = nullptr;
    }

    template<typename U,
             typename enable_if<is_convertible<
                 typename remove_extent<U>::type*,
                 element_type*>::value, int>::type = 0>
    weak_ptr(weak_ptr<U>&& r) noexcept
        : m_ptr(r.m_ptr), m_ctrl(r.m_ctrl) {
        r.m_ptr = nullptr;
        r.m_ctrl = nullptr;
    }
    
    ~weak_ptr() {
        if (m_ctrl) {
            const bool no_weak =
                shared_ptr_count_decrement(&m_ctrl->weak_count);
            if (shared_ptr_count_load(&m_ctrl->strong_count) == 0 && no_weak) {
                rin_free(m_ctrl);
            }
        }
    }
    
    weak_ptr& operator=(const weak_ptr& r) noexcept {
        if (this != &r) {
            if (m_ctrl) {
                const bool no_weak =
                    shared_ptr_count_decrement(&m_ctrl->weak_count);
                if (shared_ptr_count_load(&m_ctrl->strong_count) == 0 &&
                    no_weak) {
                    rin_free(m_ctrl);
                }
            }
            m_ptr = r.m_ptr;
            m_ctrl = r.m_ctrl;
            if (m_ctrl) shared_ptr_count_increment(&m_ctrl->weak_count);
        }
        return *this;
    }

    weak_ptr& operator=(weak_ptr&& r) noexcept {
        if (this != &r) {
            if (m_ctrl) {
                const bool no_weak =
                    shared_ptr_count_decrement(&m_ctrl->weak_count);
                if (shared_ptr_count_load(&m_ctrl->strong_count) == 0 &&
                    no_weak) {
                    rin_free(m_ctrl);
                }
            }
            m_ptr = r.m_ptr;
            m_ctrl = r.m_ctrl;
            r.m_ptr = nullptr;
            r.m_ctrl = nullptr;
        }
        return *this;
    }

    template<typename U,
             typename enable_if<is_convertible<
                 typename remove_extent<U>::type*,
                 element_type*>::value, int>::type = 0>
    weak_ptr& operator=(const weak_ptr<U>& r) noexcept {
        if (m_ctrl != r.m_ctrl || m_ptr != r.m_ptr) {
            reset();
            m_ptr = r.m_ptr;
            m_ctrl = r.m_ctrl;
            if (m_ctrl) shared_ptr_count_increment(&m_ctrl->weak_count);
        }
        return *this;
    }

    template<typename U,
             typename enable_if<is_convertible<
                 typename remove_extent<U>::type*,
                 element_type*>::value, int>::type = 0>
    weak_ptr& operator=(weak_ptr<U>&& r) noexcept {
        reset();
        m_ptr = r.m_ptr;
        m_ctrl = r.m_ctrl;
        r.m_ptr = nullptr;
        r.m_ctrl = nullptr;
        return *this;
    }

    weak_ptr& operator=(const shared_ptr<T>& r) noexcept {
        if (m_ctrl) {
            const bool no_weak =
                shared_ptr_count_decrement(&m_ctrl->weak_count);
            if (shared_ptr_count_load(&m_ctrl->strong_count) == 0 && no_weak) {
                rin_free(m_ctrl);
            }
        }
        m_ptr = r.m_ptr;
        m_ctrl = r.m_ctrl;
        if (m_ctrl) shared_ptr_count_increment(&m_ctrl->weak_count);
        return *this;
    }
    
    size_t use_count() const noexcept {
        return m_ctrl ? shared_ptr_count_load(&m_ctrl->strong_count) : 0;
    }
    
    bool expired() const noexcept {
        return use_count() == 0;
    }
    
    shared_ptr<T> lock() const noexcept {
        if (!m_ctrl || !shared_ptr_count_try_increment(&m_ctrl->strong_count))
            return shared_ptr<T>();
        shared_ptr<T> sp;
        sp.m_ptr = m_ptr;
        sp.m_ctrl = m_ctrl;
        return sp;
    }
    
    void reset() noexcept {
        if (m_ctrl) {
            const bool no_weak =
                shared_ptr_count_decrement(&m_ctrl->weak_count);
            if (shared_ptr_count_load(&m_ctrl->strong_count) == 0 && no_weak) {
                rin_free(m_ctrl);
            }
        }
        m_ptr = nullptr;
        m_ctrl = nullptr;
    }

    template<typename U>
    bool owner_before(const shared_ptr<U>& other) const noexcept {
        return detail::object_pointer_total_less(m_ctrl, other.m_ctrl);
    }

    template<typename U>
    bool owner_before(const weak_ptr<U>& other) const noexcept {
        return detail::object_pointer_total_less(m_ctrl, other.m_ctrl);
    }
};

/* shared_ptr コンストラクタ（weak_ptr から） */
template<typename T>
template<typename U,
         typename enable_if<is_convertible<
             typename remove_extent<U>::type*,
             typename remove_extent<T>::type*>::value, int>::type>
shared_ptr<T>::shared_ptr(const weak_ptr<U>& r) : m_ptr(nullptr), m_ctrl(nullptr) {
    if (r.m_ctrl &&
        shared_ptr_count_try_increment(&r.m_ctrl->strong_count)) {
        m_ptr = r.m_ptr;
        m_ctrl = r.m_ctrl;
    }
}

/* Ownership-ordering predicates.  The standard comparator deliberately
 * ignores the pointee address and observes only the control block, allowing
 * shared_ptr and weak_ptr instances that share ownership to be equivalent
 * even when they point at different subobjects. */
template<typename T>
struct owner_less<shared_ptr<T>> {
    using first_argument_type = shared_ptr<T>;
    using second_argument_type = shared_ptr<T>;
    using result_type = bool;

    template<typename U>
    bool operator()(const shared_ptr<T>& left,
                    const shared_ptr<U>& right) const noexcept {
        return left.owner_before(right);
    }
    template<typename U>
    bool operator()(const shared_ptr<T>& left,
                    const weak_ptr<U>& right) const noexcept {
        return left.owner_before(right);
    }
    template<typename U>
    bool operator()(const weak_ptr<T>& left,
                    const shared_ptr<U>& right) const noexcept {
        return left.owner_before(right);
    }
    template<typename U>
    bool operator()(const weak_ptr<T>& left,
                    const weak_ptr<U>& right) const noexcept {
        return left.owner_before(right);
    }
};

template<typename T>
struct owner_less<weak_ptr<T>> : owner_less<shared_ptr<T>> {};

#if __cplusplus >= 201703L
template<>
struct owner_less<void> {
    using is_transparent = void;

    template<typename T, typename U>
    bool operator()(const shared_ptr<T>& left,
                    const shared_ptr<U>& right) const noexcept {
        return left.owner_before(right);
    }
    template<typename T, typename U>
    bool operator()(const shared_ptr<T>& left,
                    const weak_ptr<U>& right) const noexcept {
        return left.owner_before(right);
    }
    template<typename T, typename U>
    bool operator()(const weak_ptr<T>& left,
                    const shared_ptr<U>& right) const noexcept {
        return left.owner_before(right);
    }
    template<typename T, typename U>
    bool operator()(const weak_ptr<T>& left,
                    const weak_ptr<U>& right) const noexcept {
        return left.owner_before(right);
    }
};
#endif

template<typename T>
inline bool shared_ptr_atomic_equal(const shared_ptr<T>& left,
                                    const shared_ptr<T>& right) noexcept {
    return left.get() == right.get() &&
           !left.owner_before(right) && !right.owner_before(left);
}

template<typename T>
shared_ptr<T> atomic_load(const shared_ptr<T>* value) noexcept {
    if (!value) return shared_ptr<T>();
    detail::shared_ptr_atomic_lock();
    shared_ptr<T> result(*value);
    detail::shared_ptr_atomic_unlock();
    return result;
}

/* Volatile overloads are part of the C++11 free-function surface.  The
 * implementation uses the same process-wide gate as the non-volatile forms;
 * removing volatile only for the guarded access keeps the API usable for
 * memory-mapped/shared control words without weakening the snapshot rule. */
template<typename T>
shared_ptr<T> atomic_load(const volatile shared_ptr<T>* value) noexcept {
    return atomic_load(const_cast<const shared_ptr<T>*>(value));
}

template<typename T, typename Order>
shared_ptr<T> atomic_load_explicit(const shared_ptr<T>* value,
                                   Order) noexcept {
    return atomic_load(value);
}

template<typename T, typename Order>
shared_ptr<T> atomic_load_explicit(const volatile shared_ptr<T>* value,
                                   Order order) noexcept {
    return atomic_load_explicit(const_cast<const shared_ptr<T>*>(value),
                                order);
}

template<typename T>
void atomic_store(shared_ptr<T>* value, shared_ptr<T> desired) noexcept {
    if (!value) return;
    detail::shared_ptr_atomic_lock();
    value->swap(desired);
    detail::shared_ptr_atomic_unlock();
}

template<typename T>
void atomic_store(volatile shared_ptr<T>* value, shared_ptr<T> desired) noexcept {
    atomic_store(const_cast<shared_ptr<T>*>(value), std::move(desired));
}

template<typename T, typename Order>
void atomic_store_explicit(shared_ptr<T>* value, shared_ptr<T> desired,
                           Order) noexcept {
    atomic_store(value, std::move(desired));
}

template<typename T, typename Order>
void atomic_store_explicit(volatile shared_ptr<T>* value, shared_ptr<T> desired,
                           Order order) noexcept {
    atomic_store_explicit(const_cast<shared_ptr<T>*>(value),
                          std::move(desired), order);
}

template<typename T>
shared_ptr<T> atomic_exchange(shared_ptr<T>* value,
                              shared_ptr<T> desired) noexcept {
    if (!value) return shared_ptr<T>();
    detail::shared_ptr_atomic_lock();
    shared_ptr<T> result(*value);
    value->swap(desired);
    detail::shared_ptr_atomic_unlock();
    return result;
}

template<typename T>
shared_ptr<T> atomic_exchange(volatile shared_ptr<T>* value,
                              shared_ptr<T> desired) noexcept {
    return atomic_exchange(const_cast<shared_ptr<T>*>(value),
                           std::move(desired));
}

template<typename T, typename Order>
shared_ptr<T> atomic_exchange_explicit(shared_ptr<T>* value,
                                       shared_ptr<T> desired,
                                       Order) noexcept {
    return atomic_exchange(value, std::move(desired));
}

template<typename T, typename Order>
shared_ptr<T> atomic_exchange_explicit(volatile shared_ptr<T>* value,
                                       shared_ptr<T> desired,
                                       Order order) noexcept {
    return atomic_exchange_explicit(const_cast<shared_ptr<T>*>(value),
                                    std::move(desired), order);
}

template<typename T>
bool atomic_compare_exchange_strong(shared_ptr<T>* value,
                                    shared_ptr<T>* expected,
                                    shared_ptr<T> desired) noexcept {
    if (!value || !expected) return false;
    detail::shared_ptr_atomic_lock();
    shared_ptr<T> actual(*value);
    const bool matches = shared_ptr_atomic_equal(actual, *expected);
    if (matches) {
        value->swap(desired);
    } else {
        *expected = actual;
    }
    detail::shared_ptr_atomic_unlock();
    return matches;
}

template<typename T>
bool atomic_compare_exchange_strong(volatile shared_ptr<T>* value,
                                    shared_ptr<T>* expected,
                                    shared_ptr<T> desired) noexcept {
    return atomic_compare_exchange_strong(const_cast<shared_ptr<T>*>(value),
                                          expected, std::move(desired));
}

template<typename T>
bool atomic_compare_exchange_weak(shared_ptr<T>* value,
                                  shared_ptr<T>* expected,
                                  shared_ptr<T> desired) noexcept {
    /* The gate makes this operation indivisible; a spurious failure is not
     * needed for the bounded implementation, so weak delegates to strong. */
    return atomic_compare_exchange_strong(value, expected,
                                          std::move(desired));
}

template<typename T>
bool atomic_compare_exchange_weak(volatile shared_ptr<T>* value,
                                  shared_ptr<T>* expected,
                                  shared_ptr<T> desired) noexcept {
    return atomic_compare_exchange_weak(const_cast<shared_ptr<T>*>(value),
                                        expected, std::move(desired));
}

template<typename T, typename Order>
bool atomic_compare_exchange_strong_explicit(
    shared_ptr<T>* value, shared_ptr<T>* expected, shared_ptr<T> desired,
    Order, Order) noexcept {
    return atomic_compare_exchange_strong(value, expected,
                                          std::move(desired));
}

template<typename T, typename Order>
bool atomic_compare_exchange_strong_explicit(
    volatile shared_ptr<T>* value, shared_ptr<T>* expected,
    shared_ptr<T> desired, Order success, Order failure) noexcept {
    return atomic_compare_exchange_strong_explicit(
        const_cast<shared_ptr<T>*>(value), expected, std::move(desired),
        success, failure);
}

template<typename T, typename Order>
bool atomic_compare_exchange_weak_explicit(
    shared_ptr<T>* value, shared_ptr<T>* expected, shared_ptr<T> desired,
    Order, Order) noexcept {
    return atomic_compare_exchange_weak(value, expected,
                                        std::move(desired));
}

template<typename T, typename Order>
bool atomic_compare_exchange_weak_explicit(
    volatile shared_ptr<T>* value, shared_ptr<T>* expected,
    shared_ptr<T> desired, Order success, Order failure) noexcept {
    return atomic_compare_exchange_weak_explicit(
        const_cast<shared_ptr<T>*>(value), expected, std::move(desired),
        success, failure);
}

template<typename T>
bool atomic_is_lock_free(const shared_ptr<T>*) noexcept {
    return false;
}

template<typename T>
bool atomic_is_lock_free(const volatile shared_ptr<T>*) noexcept {
    return false;
}

/* ═══════════════════════════════════════════════════════════════
 * enable_shared_from_this
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
class enable_shared_from_this {
    mutable weak_ptr<T> m_weak_this;
    
    template<typename U> friend class shared_ptr;
    
protected:
    constexpr enable_shared_from_this() noexcept {}
    enable_shared_from_this(const enable_shared_from_this&) noexcept {}
    enable_shared_from_this& operator=(const enable_shared_from_this&) noexcept { return *this; }
    ~enable_shared_from_this() {}
    
public:
    shared_ptr<T> shared_from_this() {
        return shared_ptr<T>(m_weak_this);
    }
    
    shared_ptr<const T> shared_from_this() const {
        return shared_ptr<const T>(m_weak_this);
    }
    
    weak_ptr<T> weak_from_this() noexcept {
        return m_weak_this;
    }
    
    weak_ptr<const T> weak_from_this() const noexcept {
        return m_weak_this;
    }
};

/* ═══════════════════════════════════════════════════════════════
 * pointer casts
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, typename U>
shared_ptr<T> static_pointer_cast(const shared_ptr<U>& r) noexcept {
    T* p = static_cast<T*>(r.get());
    shared_ptr<T> result;
    result.m_ptr = p;
    result.m_ctrl = r.m_ctrl;
    if (result.m_ctrl)
        shared_ptr_count_increment(&result.m_ctrl->strong_count);
    return result;
}

template<typename T, typename U>
shared_ptr<T> dynamic_pointer_cast(const shared_ptr<U>& r) noexcept {
#if defined(__GXX_RTTI) || defined(_CPPRTTI)
    T* p = dynamic_cast<T*>(r.get());
#else
    /* A static cast here would turn a failed downcast into type confusion.
     * The freestanding no-RTTI product mode therefore fails closed. */
    T* p = nullptr;
#endif
    if (p) {
        shared_ptr<T> result;
        result.m_ptr = p;
        result.m_ctrl = r.m_ctrl;
        if (result.m_ctrl)
            shared_ptr_count_increment(&result.m_ctrl->strong_count);
        return result;
    }
    return shared_ptr<T>();
}

#if __cplusplus >= 202002L
template<typename T, typename U>
shared_ptr<T> dynamic_pointer_cast(shared_ptr<U>&& r) noexcept {
#if defined(__GXX_RTTI) || defined(_CPPRTTI)
    T* p = dynamic_cast<T*>(r.get());
#else
    T* p = nullptr;
#endif
    if (!p) return shared_ptr<T>();
    shared_ptr<T> result;
    result.m_ptr = p;
    result.m_ctrl = r.m_ctrl;
    r.m_ptr = nullptr;
    r.m_ctrl = nullptr;
    return result;
}

#if __cplusplus >= 202002L
template<typename T, typename U>
shared_ptr<T> static_pointer_cast(shared_ptr<U>&& r) noexcept {
    shared_ptr<T> result;
    result.m_ptr = static_cast<T*>(r.m_ptr);
    result.m_ctrl = r.m_ctrl;
    r.m_ptr = nullptr;
    r.m_ctrl = nullptr;
    return result;
}
#endif
#endif

template<typename T, typename U>
shared_ptr<T> const_pointer_cast(const shared_ptr<U>& r) noexcept {
    T* p = const_cast<T*>(r.get());
    shared_ptr<T> result;
    result.m_ptr = p;
    result.m_ctrl = r.m_ctrl;
    if (result.m_ctrl)
        shared_ptr_count_increment(&result.m_ctrl->strong_count);
    return result;
}

template<typename T, typename U>
shared_ptr<T> reinterpret_pointer_cast(const shared_ptr<U>& r) noexcept {
    T* p = reinterpret_cast<T*>(r.get());
    shared_ptr<T> result;
    result.m_ptr = p;
    result.m_ctrl = r.m_ctrl;
    if (result.m_ctrl)
        shared_ptr_count_increment(&result.m_ctrl->strong_count);
    return result;
}

#if __cplusplus >= 202002L
template<typename T, typename U>
shared_ptr<T> reinterpret_pointer_cast(shared_ptr<U>&& r) noexcept {
    shared_ptr<T> result;
    result.m_ptr = reinterpret_cast<T*>(r.m_ptr);
    result.m_ctrl = r.m_ctrl;
    r.m_ptr = nullptr;
    r.m_ctrl = nullptr;
    return result;
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * allocator
 * ═══════════════════════════════════════════════════════════════*/

/* `allocator_arg` is the public tag used by allocator-aware aggregate
 * constructors.  Keep the pre-C++17 object header-local, like `ignore` in
 * tuple.h: it is a stateless tag and therefore does not require a shared
 * definition merely to select an overload. */
struct allocator_arg_t {
    explicit constexpr allocator_arg_t() {}
};

#if __cplusplus >= 201703L
inline constexpr allocator_arg_t allocator_arg{};
#else
static constexpr allocator_arg_t allocator_arg = allocator_arg_t();
#endif

namespace _uses_allocator_detail {

template<typename T, typename Alloc, typename = void>
struct trait : false_type {};

template<typename T, typename Alloc>
struct trait<T, Alloc, void_t<typename T::allocator_type>>
    : is_convertible<Alloc, typename T::allocator_type> {};

template<typename Alloc, typename = void>
struct qualifies_as_allocator : false_type {};

template<typename Alloc>
struct qualifies_as_allocator<
    Alloc,
    void_t<typename Alloc::value_type,
           decltype(declval<Alloc&>().allocate(size_t{}))>> : true_type {};

} /* namespace _uses_allocator_detail */

template<typename T, typename Alloc>
struct uses_allocator : _uses_allocator_detail::trait<T, Alloc> {};

#if __cplusplus >= 201703L
template<typename T, typename Alloc>
inline constexpr bool uses_allocator_v = uses_allocator<T, Alloc>::value;
#endif

template<typename T>
class allocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using propagate_on_container_move_assignment = true_type;
    using is_always_equal = true_type;
    
    template<typename U>
    struct rebind { using other = allocator<U>; };
    
    allocator() noexcept {}
    allocator(const allocator&) noexcept {}
    template<typename U> allocator(const allocator<U>&) noexcept {}
    allocator& operator=(const allocator&) noexcept = default;
    allocator& operator=(allocator&&) noexcept = default;
    ~allocator() {}
    
    pointer allocate(size_type n) {
        if (n > max_size()) return nullptr;
        size_type total = n * sizeof(T);
        return static_cast<pointer>(rin_malloc(total));
    }
    
    void deallocate(pointer p, size_type) noexcept {
        rin_free(p);
    }
    
    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        new (p) U(std::forward<Args>(args)...);
    }
    
    template<typename U>
    void destroy(U* p) {
        p->~U();
    }
    
    size_type max_size() const noexcept {
        return static_cast<size_type>(-1) / sizeof(T);
    }
};

template<typename T, typename U>
bool operator==(const allocator<T>&, const allocator<U>&) noexcept { return true; }

template<typename T, typename U>
bool operator!=(const allocator<T>&, const allocator<U>&) noexcept { return false; }

/* ═══════════════════════════════════════════════════════════════
 * allocator_traits
 * ═══════════════════════════════════════════════════════════════*/

/* void_t ヘルパー（未定義の場合） */
#ifndef RINCXX_VOID_T_DEFINED
#define RINCXX_VOID_T_DEFINED
template<typename...>
using void_t = void;
#endif

/* allocator_traits デフォルト実装用ヘルパー */
namespace _alloc_traits_detail {
    /* pointer 検出 */
    template<typename Alloc, typename = void>
    struct _get_pointer { using type = typename Alloc::value_type*; };

    template<typename Alloc>
    struct _get_pointer<Alloc, void_t<typename Alloc::pointer>> {
        using type = typename Alloc::pointer;
    };

    /* const_pointer 検出 */
    template<typename Alloc, typename Pointer, typename = void>
    struct _get_const_pointer {
        using type = typename pointer_traits<Pointer>::template
            rebind<const typename Alloc::value_type>;
    };

    template<typename Alloc, typename Pointer>
    struct _get_const_pointer<
        Alloc, Pointer, void_t<typename Alloc::const_pointer>> {
        using type = typename Alloc::const_pointer;
    };

    template<typename Alloc, typename Pointer, typename = void>
    struct _get_void_pointer {
        using type = typename pointer_traits<Pointer>::template rebind<void>;
    };

    template<typename Alloc, typename Pointer>
    struct _get_void_pointer<
        Alloc, Pointer, void_t<typename Alloc::void_pointer>> {
        using type = typename Alloc::void_pointer;
    };

    template<typename Alloc, typename Pointer, typename = void>
    struct _get_const_void_pointer {
        using type = typename pointer_traits<Pointer>::template
            rebind<const void>;
    };

    template<typename Alloc, typename Pointer>
    struct _get_const_void_pointer<
        Alloc, Pointer, void_t<typename Alloc::const_void_pointer>> {
        using type = typename Alloc::const_void_pointer;
    };

    /* size_type 検出 */
    template<typename Alloc, typename Difference, typename = void>
    struct _get_size_type { using type = make_unsigned_t<Difference>; };

    template<typename Alloc, typename Difference>
    struct _get_size_type<Alloc, Difference,
                          void_t<typename Alloc::size_type>> {
        using type = typename Alloc::size_type;
    };

    /* difference_type 検出 */
    template<typename Alloc, typename Pointer, typename = void>
    struct _get_difference_type {
        using type = typename pointer_traits<Pointer>::difference_type;
    };

    template<typename Alloc, typename Pointer>
    struct _get_difference_type<
        Alloc, Pointer, void_t<typename Alloc::difference_type>> {
        using type = typename Alloc::difference_type;
    };

    /* rebind 検出: Alloc::rebind<T>::other がある場合はそれ、なければテンプレート引数を置換 */
    template<typename Alloc, typename T>
    struct _replace_first_arg {};

    template<template<typename, typename...> class Template, typename U, typename... Args, typename T>
    struct _replace_first_arg<Template<U, Args...>, T> {
        using type = Template<T, Args...>;
    };

    template<typename Alloc, typename T, typename = void>
    struct _get_rebind : _replace_first_arg<Alloc, T> {};

    template<typename Alloc, typename T>
    struct _get_rebind<Alloc, T, void_t<typename Alloc::template rebind<T>::other>> {
        using type = typename Alloc::template rebind<T>::other;
    };

    template<typename Alloc, typename = void>
    struct _get_copy_propagation { using type = false_type; };

    template<typename Alloc>
    struct _get_copy_propagation<
        Alloc,
        void_t<typename Alloc::propagate_on_container_copy_assignment>> {
        using type = typename Alloc::propagate_on_container_copy_assignment;
    };

    template<typename Alloc, typename = void>
    struct _get_move_propagation { using type = false_type; };

    template<typename Alloc>
    struct _get_move_propagation<
        Alloc,
        void_t<typename Alloc::propagate_on_container_move_assignment>> {
        using type = typename Alloc::propagate_on_container_move_assignment;
    };

    template<typename Alloc, typename = void>
    struct _get_swap_propagation { using type = false_type; };

    template<typename Alloc>
    struct _get_swap_propagation<
        Alloc,
        void_t<typename Alloc::propagate_on_container_swap>> {
        using type = typename Alloc::propagate_on_container_swap;
    };

    template<typename Alloc, typename = void>
    struct _get_is_always_equal { using type = is_empty<Alloc>; };

    template<typename Alloc>
    struct _get_is_always_equal<Alloc,
                                void_t<typename Alloc::is_always_equal>> {
        using type = typename Alloc::is_always_equal;
    };

    template<typename Alloc, typename = void>
    struct _has_copy_selection : false_type {};

    template<typename Alloc>
    struct _has_copy_selection<
        Alloc,
        void_t<decltype(declval<const Alloc&>().
                            select_on_container_copy_construction())>>
        : true_type {};

    template<typename Alloc, typename Size, typename Hint, typename = void>
    struct _has_allocate_hint : false_type {};

    template<typename Alloc, typename Size, typename Hint>
    struct _has_allocate_hint<
        Alloc, Size, Hint,
        void_t<decltype(declval<Alloc&>().allocate(
            declval<Size>(), declval<Hint&>()))>> : true_type {};

    template<typename, typename Alloc, typename T, typename... Args>
    struct _has_construct_impl : false_type {};

    template<typename Alloc, typename T, typename... Args>
    struct _has_construct_impl<
        void_t<decltype(declval<Alloc&>().construct(
            declval<T*>(), declval<Args>()...))>,
        Alloc, T, Args...> : true_type {};

    template<typename Alloc, typename T, typename... Args>
    using _has_construct = _has_construct_impl<void, Alloc, T, Args...>;

    template<typename Alloc, typename T, typename = void>
    struct _has_destroy : false_type {};

    template<typename Alloc, typename T>
    struct _has_destroy<
        Alloc, T,
        void_t<decltype(declval<Alloc&>().destroy(declval<T*>()))>>
        : true_type {};

    template<typename Alloc, typename = void>
    struct _has_max_size : false_type {};

    template<typename Alloc>
    struct _has_max_size<
        Alloc,
        void_t<decltype(declval<const Alloc&>().max_size())>> : true_type {};
}

template<typename Alloc>
struct allocator_traits {
    using allocator_type = Alloc;
    using value_type = typename Alloc::value_type;
    using pointer = typename _alloc_traits_detail::_get_pointer<Alloc>::type;
    using const_pointer = typename _alloc_traits_detail::_get_const_pointer<
        Alloc, pointer>::type;
    using void_pointer = typename _alloc_traits_detail::_get_void_pointer<
        Alloc, pointer>::type;
    using const_void_pointer =
        typename _alloc_traits_detail::_get_const_void_pointer<
            Alloc, pointer>::type;
    using difference_type =
        typename _alloc_traits_detail::_get_difference_type<
            Alloc, pointer>::type;
    using size_type = typename _alloc_traits_detail::_get_size_type<
        Alloc, difference_type>::type;
    using propagate_on_container_copy_assignment =
        typename _alloc_traits_detail::_get_copy_propagation<Alloc>::type;
    using propagate_on_container_move_assignment =
        typename _alloc_traits_detail::_get_move_propagation<Alloc>::type;
    using propagate_on_container_swap =
        typename _alloc_traits_detail::_get_swap_propagation<Alloc>::type;
    using is_always_equal =
        typename _alloc_traits_detail::_get_is_always_equal<Alloc>::type;

    /* rebind - SFINAEで Alloc::rebind<T>::other がなければテンプレート引数を置換 */
    template<typename T>
    using rebind_alloc = typename _alloc_traits_detail::_get_rebind<Alloc, T>::type;

    template<typename T>
    using rebind_traits = allocator_traits<rebind_alloc<T>>;

private:
    static RIN_MEMORY_CONSTEXPR14 pointer allocate_with_hint(
        Alloc& a, size_type n, const_void_pointer hint, true_type) {
        return a.allocate(n, hint);
    }

    static RIN_MEMORY_CONSTEXPR14 pointer allocate_with_hint(
        Alloc& a, size_type n, const_void_pointer hint, false_type) {
        (void)hint;
        return a.allocate(n);
    }

    template<typename T, typename... Args>
    static RIN_MEMORY_CONSTEXPR14 void construct_with_allocator(
        Alloc& a, T* p, true_type, Args&&... args) {
        a.construct(p, std::forward<Args>(args)...);
    }

    template<typename T, typename... Args>
    static RIN_MEMORY_CONSTEXPR14 void construct_with_allocator(
        Alloc&, T* p, false_type, Args&&... args) {
        ::new (static_cast<void*>(p)) T(std::forward<Args>(args)...);
    }

    template<typename T>
    static RIN_MEMORY_CONSTEXPR14 void destroy_with_allocator(
        Alloc& a, T* p, true_type) {
        a.destroy(p);
    }

    template<typename T>
    static RIN_MEMORY_CONSTEXPR14 void destroy_with_allocator(
        Alloc&, T* p, false_type) {
        p->~T();
    }

    static RIN_MEMORY_CONSTEXPR14 size_type max_size_with_allocator(
        const Alloc& a, true_type) noexcept {
        return a.max_size();
    }

    static RIN_MEMORY_CONSTEXPR14 size_type max_size_with_allocator(
        const Alloc&, false_type) noexcept {
        return static_cast<size_type>(-1) / sizeof(value_type);
    }

    static RIN_MEMORY_CONSTEXPR14 Alloc select_copy_allocator(
        const Alloc& a, true_type) {
        return a.select_on_container_copy_construction();
    }

    static RIN_MEMORY_CONSTEXPR14 Alloc select_copy_allocator(
        const Alloc& a, false_type) {
        return a;
    }

public:
    /* allocate */
    RIN_MEMORY_NODISCARD static RIN_MEMORY_CONSTEXPR14 pointer allocate(Alloc& a, size_type n) {
        return a.allocate(n);
    }

    RIN_MEMORY_NODISCARD static RIN_MEMORY_CONSTEXPR14 pointer allocate(
        Alloc& a, size_type n, const_void_pointer hint) {
        return allocate_with_hint(
            a, n, hint,
            typename _alloc_traits_detail::_has_allocate_hint<
                Alloc, size_type, const_void_pointer>());
    }

    /* deallocate */
    static RIN_MEMORY_CONSTEXPR14 void deallocate(Alloc& a, pointer p, size_type n)
        noexcept(noexcept(a.deallocate(p, n))) {
        a.deallocate(p, n);
    }

    /* construct */
    template<typename T, typename... Args>
    static RIN_MEMORY_CONSTEXPR14 void construct(Alloc& a, T* p, Args&&... args) {
        construct_with_allocator(
            a, p,
            typename _alloc_traits_detail::_has_construct<
                Alloc, T, Args&&...>(),
            std::forward<Args>(args)...);
    }

    /* destroy */
    template<typename T>
    static RIN_MEMORY_CONSTEXPR14 void destroy(Alloc& a, T* p) {
        destroy_with_allocator(
            a, p,
            typename _alloc_traits_detail::_has_destroy<Alloc, T>());
    }

    /* max_size */
    static RIN_MEMORY_CONSTEXPR14 size_type max_size(const Alloc& a) noexcept {
        return max_size_with_allocator(
            a, typename _alloc_traits_detail::_has_max_size<Alloc>());
    }

    /* select_on_container_copy_construction */
    static RIN_MEMORY_CONSTEXPR14 Alloc select_on_container_copy_construction(const Alloc& a) {
        return select_copy_allocator(
            a, typename _alloc_traits_detail::_has_copy_selection<Alloc>());
    }
};

/* A bounded allocator-aware shared owner.  The object allocation is owned by
 * the rebound allocator while the shared_ptr control block keeps a small
 * type-erased holder alive until the final strong release.  The holder keeps
 * the allocator's pointer object for deallocation, while shared_ptr exposes a
 * raw view obtained through the standard address customization. */
template<typename Pointer>
constexpr auto allocate_shared_pointer_address(const Pointer& pointer) noexcept
#if __cplusplus >= 202002L
    -> decltype(std::to_address(pointer))
#else
    -> decltype(pointer_traits<Pointer>::to_address(pointer))
#endif
{
#if __cplusplus >= 202002L
    return std::to_address(pointer);
#else
    return pointer_traits<Pointer>::to_address(pointer);
#endif
}

#if __cplusplus < 202002L
template<typename Pointer>
constexpr auto allocate_shared_pointer_address(const Pointer& pointer) noexcept
    -> decltype(allocate_shared_pointer_address(pointer.operator->())) {
    return allocate_shared_pointer_address(pointer.operator->());
}
#endif

template<typename T>
constexpr T* allocate_shared_pointer_address(T* pointer) noexcept {
    return pointer;
}
template<typename T, typename Alloc>
struct allocate_shared_holder {
    using allocator_type = Alloc;
    using pointer = typename allocator_traits<allocator_type>::pointer;
    allocator_type alloc;
    pointer object;
    T* raw_object;

    allocate_shared_holder(const allocator_type& owner, pointer value,
                           T* raw_value)
        : alloc(owner), object(value), raw_object(raw_value) {}
};

template<typename T, typename Alloc>
inline void destroy_allocate_shared_holder(void* state) noexcept {
    using holder_type = allocate_shared_holder<T, Alloc>;
    holder_type* holder = static_cast<holder_type*>(state);
    allocator_traits<Alloc>::destroy(holder->alloc, holder->raw_object);
    allocator_traits<Alloc>::deallocate(holder->alloc, holder->object, 1);
    holder->~holder_type();
    rin_free(holder);
}

template<typename T, typename Alloc, typename... Args,
         typename enable_if<!is_array<T>::value, int>::type>
shared_ptr<T> allocate_shared(const Alloc& alloc, Args&&... args) {
    using object_allocator = typename allocator_traits<Alloc>::template
        rebind_alloc<T>;
    using object_traits = allocator_traits<object_allocator>;

    object_allocator object_owner(alloc);
    using object_pointer = typename object_traits::pointer;
    object_pointer object_owner_pointer = object_traits::allocate(object_owner, 1);
    T* object = allocate_shared_pointer_address(object_owner_pointer);
    if (!object) {
        if (object_owner_pointer != object_pointer())
            object_traits::deallocate(object_owner, object_owner_pointer, 1);
        rin_panic("allocate_shared object allocation failed");
    }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
        object_traits::construct(object_owner, object,
                                 std::forward<Args>(args)...);
    } catch (...) {
        object_traits::deallocate(object_owner, object_owner_pointer, 1);
        throw;
    }
#else
    object_traits::construct(object_owner, object,
                             std::forward<Args>(args)...);
#endif

    using holder_type = allocate_shared_holder<T, object_allocator>;
    holder_type* holder = static_cast<holder_type*>(rin_malloc(sizeof(holder_type)));
    if (!holder) {
        object_traits::destroy(object_owner, object);
        object_traits::deallocate(object_owner, object_owner_pointer, 1);
        rin_panic("allocate_shared holder allocation failed");
    }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        new (holder) holder_type(object_owner, object_owner_pointer, object);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        rin_free(holder);
        object_traits::destroy(object_owner, object);
        object_traits::deallocate(object_owner, object_owner_pointer, 1);
        throw;
    }
#endif

    shared_ptr<T> result;
    result.m_ptr = object;
    result.m_ctrl = static_cast<shared_ptr_control_block*>(
        rin_malloc(sizeof(shared_ptr_control_block)));
    if (!result.m_ctrl) {
        destroy_allocate_shared_holder<T, object_allocator>(holder);
        result.m_ptr = nullptr;
        rin_panic("allocate_shared control block allocation failed");
    }
    new (result.m_ctrl) shared_ptr_control_block(
        static_cast<void*>(holder),
        &destroy_allocate_shared_holder<T, object_allocator>);
    result._setup_enable_shared_from_this(object);
    return result;
}

#if __cplusplus >= 202002L
template<typename T, typename U>
shared_ptr<T> const_pointer_cast(shared_ptr<U>&& r) noexcept {
    shared_ptr<T> result;
    result.m_ptr = const_cast<T*>(r.m_ptr);
    result.m_ctrl = r.m_ctrl;
    r.m_ptr = nullptr;
    r.m_ctrl = nullptr;
    return result;
}
#endif

/* Array factories keep the rebound allocator's pointer object until the
 * final owner is released.  Elements are constructed one by one in a private
 * transaction; a throwing element constructor destroys the already-built
 * prefix before returning the allocation to the same allocator owner. */
template<typename Element, typename Alloc>
struct allocate_shared_array_holder {
    using allocator_type = Alloc;
    using pointer = typename allocator_traits<allocator_type>::pointer;
    allocator_type alloc;
    pointer object;
    Element* raw_object;
    size_t count;

    allocate_shared_array_holder(const allocator_type& owner, pointer value,
                                 Element* raw_value, size_t size)
        : alloc(owner), object(value), raw_object(raw_value), count(size) {}
};

template<typename Element, typename Alloc>
inline void destroy_allocate_shared_array_holder(void* state) noexcept {
    using holder_type = allocate_shared_array_holder<Element, Alloc>;
    holder_type* holder = static_cast<holder_type*>(state);
    for (size_t index = holder->count; index != 0; --index)
        allocator_traits<Alloc>::destroy(holder->alloc,
                                         holder->raw_object + (index - 1));
    allocator_traits<Alloc>::deallocate(holder->alloc, holder->object,
                                        holder->count);
    holder->~holder_type();
    rin_free(holder);
}

template<typename Element, typename Alloc>
inline void destroy_array_prefix(Alloc& alloc, Element* raw_object,
                                 size_t count) noexcept {
    while (count != 0) {
        --count;
        allocator_traits<Alloc>::destroy(alloc, raw_object + count);
    }
}

template<typename Element, typename Alloc>
inline void construct_array_elements(Alloc& alloc, Element* raw_object,
                                     size_t count) {
    size_t constructed = 0;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        for (; constructed < count; ++constructed)
            allocator_traits<Alloc>::construct(alloc, raw_object + constructed);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        destroy_array_prefix(alloc, raw_object, constructed);
        throw;
    }
#endif
}

template<typename Element, typename Alloc>
inline void construct_array_elements(Alloc& alloc, Element* raw_object,
                                     size_t count, const Element& value) {
    size_t constructed = 0;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        for (; constructed < count; ++constructed)
            allocator_traits<Alloc>::construct(alloc, raw_object + constructed,
                                               value);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        destroy_array_prefix(alloc, raw_object, constructed);
        throw;
    }
#endif
}

/* `make_shared_for_overwrite` requires default-initialization rather than the
 * value-initialization performed by allocator_traits::construct with no
 * arguments.  Keep this tag private to the array factory so ordinary
 * make_shared<T[]> retains its value-initialization contract. */
struct shared_array_overwrite_tag {};

template<typename Element, typename Alloc>
inline void construct_array_elements(Alloc& alloc, Element* raw_object,
                                     size_t count,
                                     shared_array_overwrite_tag) {
    size_t constructed = 0;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        for (; constructed < count; ++constructed)
            ::new (static_cast<void*>(raw_object + constructed)) Element;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        destroy_array_prefix(alloc, raw_object, constructed);
        throw;
    }
#endif
}

template<typename Array, typename Alloc, typename... Args>
shared_ptr<Array> allocate_shared_array_impl(const Alloc& alloc, size_t count,
                                              Args&&... args) {
    using element_type = typename remove_extent<Array>::type;
    using object_allocator = typename allocator_traits<Alloc>::template
        rebind_alloc<element_type>;
    using object_traits = allocator_traits<object_allocator>;
    object_allocator object_owner(alloc);
    if (count == 0)
        return shared_ptr<Array>();
    if (count > object_traits::max_size(object_owner))
        rin_panic("allocate_shared array size overflow");
    using object_pointer = typename object_traits::pointer;
    object_pointer object_owner_pointer = object_traits::allocate(object_owner,
                                                                   count);
    element_type* object = allocate_shared_pointer_address(object_owner_pointer);
    if (!object) {
        if (object_owner_pointer != object_pointer())
            object_traits::deallocate(object_owner, object_owner_pointer, count);
        rin_panic("allocate_shared array allocation failed");
    }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        construct_array_elements(object_owner, object, count,
                                 std::forward<Args>(args)...);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        object_traits::deallocate(object_owner, object_owner_pointer, count);
        throw;
    }
#endif
    using holder_type = allocate_shared_array_holder<element_type,
                                                     object_allocator>;
    holder_type* holder = static_cast<holder_type*>(
        rin_malloc(sizeof(holder_type)));
    if (!holder) {
        destroy_array_prefix(object_owner, object, count);
        object_traits::deallocate(object_owner, object_owner_pointer, count);
        rin_panic("allocate_shared array holder allocation failed");
    }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        new (holder) holder_type(object_owner, object_owner_pointer, object,
                                 count);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        rin_free(holder);
        destroy_array_prefix(object_owner, object, count);
        object_traits::deallocate(object_owner, object_owner_pointer, count);
        throw;
    }
#endif

    shared_ptr<Array> result;
    result.m_ptr = object;
    result.m_ctrl = static_cast<shared_ptr_control_block*>(
        rin_malloc(sizeof(shared_ptr_control_block)));
    if (!result.m_ctrl) {
        destroy_allocate_shared_array_holder<element_type, object_allocator>(
            holder);
        result.m_ptr = nullptr;
        rin_panic("allocate_shared array control block allocation failed");
    }
    new (result.m_ctrl) shared_ptr_control_block(
        static_cast<void*>(holder),
        &destroy_allocate_shared_array_holder<element_type, object_allocator>);
    return result;
}

template<typename T, typename Alloc,
         typename enable_if<is_array<T>::value &&
                            (extent<T>::value == 0), int>::type = 0>
shared_ptr<T> allocate_shared(const Alloc& alloc, size_t count) {
    return allocate_shared_array_impl<T>(alloc, count);
}

template<typename T, typename Alloc,
         typename enable_if<is_array<T>::value &&
                            (extent<T>::value == 0), int>::type = 0>
shared_ptr<T> allocate_shared(const Alloc& alloc, size_t count,
                              const typename remove_extent<T>::type& value) {
    return allocate_shared_array_impl<T>(alloc, count, value);
}

template<typename T, typename Alloc,
         typename enable_if<is_array<T>::value &&
                            (extent<T>::value != 0), int>::type = 0>
shared_ptr<T> allocate_shared(const Alloc& alloc) {
    return allocate_shared_array_impl<T>(alloc, extent<T>::value);
}

template<typename T, typename Alloc,
         typename enable_if<is_array<T>::value &&
                            (extent<T>::value != 0), int>::type = 0>
shared_ptr<T> allocate_shared(
    const Alloc& alloc,
    const typename remove_extent<T>::type& value) {
    return allocate_shared_array_impl<T>(alloc, extent<T>::value, value);
}

#if __cplusplus >= 202002L
template<typename T, typename Alloc,
         typename enable_if<is_array<T>::value &&
                            (extent<T>::value == 0), int>::type = 0>
shared_ptr<T> allocate_shared_for_overwrite(const Alloc& alloc, size_t count) {
    return allocate_shared_array_impl<T>(alloc, count,
                                         shared_array_overwrite_tag{});
}

template<typename T, typename Alloc,
         typename enable_if<is_array<T>::value &&
                            (extent<T>::value != 0), int>::type = 0>
shared_ptr<T> allocate_shared_for_overwrite(const Alloc& alloc) {
    return allocate_shared_array_impl<T>(alloc, extent<T>::value,
                                         shared_array_overwrite_tag{});
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * addressof
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
T* addressof(T& arg) noexcept {
    return __builtin_addressof(arg);
}

template<typename T>
const T* addressof(const T&& arg) = delete;

/* ═══════════════════════════════════════════════════════════════
 * uninitialized_* algorithms
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
void destroy_at(T* p) noexcept {
    p->~T();
}

#if __cplusplus >= 202002L
template<typename ForwardIt>
void destroy_constructed_prefix(ForwardIt first, ForwardIt current) noexcept {
    /* ForwardIterator does not promise decrement.  Destroying the private
     * prefix in construction order still releases every completed object and
     * keeps the algorithm valid for singly-linked iterator destinations. */
    while (first != current) {
        destroy_at(addressof(*first));
        ++first;
    }
}

template<typename ForwardIt, typename T>
void uninitialized_fill(ForwardIt first, ForwardIt last, const T& value) {
    using V = typename std::iterator_traits<ForwardIt>::value_type;
    ForwardIt current = first;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        for (; current != last; ++current)
            ::new (static_cast<void*>(addressof(*current))) V(value);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        destroy_constructed_prefix(first, current);
        throw;
    }
#endif
}

template<typename ForwardIt, typename Size, typename T>
ForwardIt uninitialized_fill_n(ForwardIt first, Size count, const T& value) {
    using V = typename std::iterator_traits<ForwardIt>::value_type;
    ForwardIt current = first;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        for (Size i = 0; i < count; ++i, ++current)
            ::new (static_cast<void*>(addressof(*current))) V(value);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        destroy_constructed_prefix(first, current);
        throw;
    }
#endif
    return current;
}

template<typename InputIt, typename ForwardIt>
ForwardIt uninitialized_copy(InputIt first, InputIt last, ForwardIt d_first) {
    using V = typename std::iterator_traits<ForwardIt>::value_type;
    ForwardIt current = d_first;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        for (; first != last; ++first, ++current)
            ::new (static_cast<void*>(addressof(*current))) V(*first);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        destroy_constructed_prefix(d_first, current);
        throw;
    }
#endif
    return current;
}

template<typename InputIt, typename Size, typename ForwardIt>
ForwardIt uninitialized_copy_n(InputIt first, Size count, ForwardIt d_first) {
    using V = typename std::iterator_traits<ForwardIt>::value_type;
    ForwardIt current = d_first;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        for (Size i = 0; i < count; ++i, ++first, ++current)
            ::new (static_cast<void*>(addressof(*current))) V(*first);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        destroy_constructed_prefix(d_first, current);
        throw;
    }
#endif
    return current;
}

template<typename ForwardIt>
void uninitialized_default_construct(ForwardIt first, ForwardIt last) {
    using V = typename std::iterator_traits<ForwardIt>::value_type;
    ForwardIt current = first;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        for (; current != last; ++current)
            ::new (static_cast<void*>(addressof(*current))) V;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        destroy_constructed_prefix(first, current);
        throw;
    }
#endif
}

template<typename ForwardIt, typename Size>
ForwardIt uninitialized_default_construct_n(ForwardIt first, Size count) {
    using V = typename std::iterator_traits<ForwardIt>::value_type;
    ForwardIt current = first;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        for (Size i = 0; i < count; ++i, ++current)
            ::new (static_cast<void*>(addressof(*current))) V;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        destroy_constructed_prefix(first, current);
        throw;
    }
#endif
    return current;
}

template<typename ForwardIt>
void uninitialized_value_construct(ForwardIt first, ForwardIt last) {
    using V = typename std::iterator_traits<ForwardIt>::value_type;
    ForwardIt current = first;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        for (; current != last; ++current)
            ::new (static_cast<void*>(addressof(*current))) V();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        destroy_constructed_prefix(first, current);
        throw;
    }
#endif
}

template<typename ForwardIt, typename Size>
ForwardIt uninitialized_value_construct_n(ForwardIt first, Size count) {
    using V = typename std::iterator_traits<ForwardIt>::value_type;
    ForwardIt current = first;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        for (Size i = 0; i < count; ++i, ++current)
            ::new (static_cast<void*>(addressof(*current))) V();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        destroy_constructed_prefix(first, current);
        throw;
    }
#endif
    return current;
}

template<typename T, typename... Args>
constexpr T* construct_at(T* p, Args&&... args) {
    return ::new (static_cast<void*>(p)) T(std::forward<Args>(args)...);
}
#endif

template<typename ForwardIt>
void destroy(ForwardIt first, ForwardIt last) {
    for (; first != last; ++first) {
        destroy_at(addressof(*first));
    }
}

template<typename ForwardIt, typename Size>
ForwardIt destroy_n(ForwardIt first, Size count) {
    for (Size i = 0; i < count; ++i, ++first) {
        destroy_at(addressof(*first));
    }
    return first;
}

/* uninitialized_move algorithms */
template<typename InputIt, typename ForwardIt>
ForwardIt uninitialized_move(InputIt first, InputIt last, ForwardIt d_first) {
    using V = typename std::iterator_traits<ForwardIt>::value_type;
    ForwardIt current = d_first;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        for (; first != last; ++first, ++current)
            ::new (static_cast<void*>(addressof(*current))) V(move(*first));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        destroy_constructed_prefix(d_first, current);
        throw;
    }
#endif
    return current;
}

template<typename InputIt, typename Size, typename ForwardIt>
pair<InputIt, ForwardIt> uninitialized_move_n(InputIt first, Size count, ForwardIt d_first) {
    using V = typename std::iterator_traits<ForwardIt>::value_type;
    ForwardIt current = d_first;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        for (Size i = 0; i < count; ++i, ++first, ++current)
            ::new (static_cast<void*>(addressof(*current))) V(move(*first));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        destroy_constructed_prefix(d_first, current);
        throw;
    }
#endif
    return {first, current};
}

#undef RIN_MEMORY_NODISCARD
#undef RIN_MEMORY_CONSTEXPR14

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_MEMORY_H */
