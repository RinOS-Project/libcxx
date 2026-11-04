/*
 * RinOS C++26 <hazard_pointer>
 * Single-writer, multi-reader safe reclamation.
 */

#ifndef RINCXX_HAZARD_POINTER_H
#define RINCXX_HAZARD_POINTER_H

#include "rincxx.h"

#if defined(__cplusplus) && __cplusplus > 202002L && \
    (__cplusplus > 202302L || defined(RIN_ENABLE_CXX26_HAZARD_POINTER))

#include "atomic.h"
#include "exception.h"
#include "memory.h"
#include "span.h"
#include "type_traits.h"
#include "utility.h"

#ifndef __cpp_lib_hazard_pointer
#define __cpp_lib_hazard_pointer 202306L
#endif

namespace std {
namespace hazard_pointer_detail {

struct retired_node {
    retired_node* next;
    const void* object;
    void (*reclaim)(retired_node*) noexcept;
    atomic<bool> retired;

    retired_node() noexcept
        : next(nullptr), object(nullptr), reclaim(nullptr), retired(false) {}
    retired_node(const retired_node&) noexcept
        : next(nullptr), object(nullptr), reclaim(nullptr), retired(false) {}
    retired_node(retired_node&&) noexcept
        : next(nullptr), object(nullptr), reclaim(nullptr), retired(false) {}
    retired_node& operator=(const retired_node&) noexcept {
        next = nullptr;
        object = nullptr;
        reclaim = nullptr;
        retired.store(false, memory_order_relaxed);
        return *this;
    }
    retired_node& operator=(retired_node&&) noexcept {
        next = nullptr;
        object = nullptr;
        reclaim = nullptr;
        retired.store(false, memory_order_relaxed);
        return *this;
    }
};

struct hazard_record {
    atomic<void*> value;
    hazard_record* next;
    bool owned;

    hazard_record() noexcept : value(nullptr), next(nullptr), owned(false) {}
};

class hazard_domain {
    atomic_flag lock_;
    hazard_record* records_ = nullptr;
    retired_node* retired_ = nullptr;

    void lock() noexcept {
        while (lock_.test_and_set(memory_order_acquire)) {}
    }

    void unlock() noexcept { lock_.clear(memory_order_release); }

    bool is_protected(const void* object) const noexcept {
        for (hazard_record* record = records_; record; record = record->next)
            if (record->value.load(memory_order_seq_cst) == object) return true;
        return false;
    }

    retired_node* collect_locked() noexcept {
        retired_node* ready = nullptr;
        retired_node** link = &retired_;
        while (*link) {
            retired_node* node = *link;
            if (is_protected(node->object)) {
                link = &node->next;
            } else {
                *link = node->next;
                node->next = ready;
                ready = node;
            }
        }
        return ready;
    }

    static void invoke_reclaimers(retired_node* ready) noexcept {
        while (ready) {
            retired_node* node = ready;
            ready = ready->next;
            node->next = nullptr;
            if (node->reclaim) node->reclaim(node);
        }
    }

public:
    hazard_domain() noexcept = default;
    hazard_domain(const hazard_domain&) = delete;
    hazard_domain& operator=(const hazard_domain&) = delete;

    hazard_record* acquire() {
        lock();
        for (hazard_record* record = records_; record; record = record->next) {
            if (!record->owned) {
                record->value.store(nullptr, memory_order_seq_cst);
                record->owned = true;
                unlock();
                return record;
            }
        }

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            hazard_record* record = new hazard_record();
            record->owned = true;
            record->next = records_;
            records_ = record;
            unlock();
            return record;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            unlock();
            throw;
        }
#endif
    }

    void retire(retired_node* node) noexcept {
        lock();
        node->next = retired_;
        retired_ = node;
        retired_node* ready = collect_locked();
        unlock();
        invoke_reclaimers(ready);
    }

    void release(hazard_record* record) noexcept {
        record->value.store(nullptr, memory_order_seq_cst);
        lock();
        record->owned = false;
        retired_node* ready = collect_locked();
        unlock();
        invoke_reclaimers(ready);
    }

    void set(hazard_record* record, const void* object) noexcept {
        record->value.store(const_cast<void*>(object), memory_order_seq_cst);
        if (!object) {
            lock();
            retired_node* ready = collect_locked();
            unlock();
            invoke_reclaimers(ready);
        }
    }
};

inline hazard_domain& default_domain() noexcept {
    static hazard_domain domain;
    return domain;
}

template<class T>
struct is_protectable : integral_constant<bool,
    is_class<typename remove_cv<T>::type>::value &&
    is_base_of<retired_node, typename remove_cv<T>::type>::value> {};

} // namespace hazard_pointer_detail

template<class T, class D = default_delete<T>>
class hazard_pointer_obj_base : public hazard_pointer_detail::retired_node {
    static_assert(is_class<T>::value,
                  "hazard_pointer_obj_base requires a class type");
    D deleter_;

    static void reclaim_object(hazard_pointer_detail::retired_node* node) noexcept {
        using base_type = hazard_pointer_obj_base<T, D>;
        base_type* base = static_cast<base_type*>(node);
        T* object = static_cast<T*>(base);
        D deleter;
        deleter = std::move(base->deleter_);
        deleter(object);
    }

public:
    void retire(D deleter = D()) noexcept {
        using detail_type = hazard_pointer_detail::retired_node;
        if (this->retired.exchange(true, memory_order_acq_rel)) return;
        deleter_ = std::move(deleter);
        T* object = static_cast<T*>(this);
        this->object = static_cast<const void*>(object);
        this->reclaim = &reclaim_object;
        hazard_pointer_detail::default_domain().retire(
            static_cast<detail_type*>(this));
    }

protected:
    hazard_pointer_obj_base() = default;
    hazard_pointer_obj_base(const hazard_pointer_obj_base& other)
        : hazard_pointer_detail::retired_node(other), deleter_(other.deleter_) {}
    hazard_pointer_obj_base(hazard_pointer_obj_base&& other)
        noexcept(is_nothrow_move_constructible<D>::value)
        : hazard_pointer_detail::retired_node(std::move(other)),
          deleter_(std::move(other.deleter_)) {}
    hazard_pointer_obj_base& operator=(const hazard_pointer_obj_base& other) {
        if (this != &other) {
            hazard_pointer_detail::retired_node::operator=(other);
            deleter_ = other.deleter_;
        }
        return *this;
    }
    hazard_pointer_obj_base& operator=(hazard_pointer_obj_base&& other)
        noexcept(is_nothrow_move_assignable<D>::value) {
        if (this != &other) {
            hazard_pointer_detail::retired_node::operator=(std::move(other));
            deleter_ = std::move(other.deleter_);
        }
        return *this;
    }
    ~hazard_pointer_obj_base() = default;
};

class hazard_pointer {
    hazard_pointer_detail::hazard_record* record_;

    explicit hazard_pointer(hazard_pointer_detail::hazard_record* record) noexcept
        : record_(record) {}

    void release() noexcept {
        if (!record_) return;
        hazard_pointer_detail::hazard_record* record = record_;
        record_ = nullptr;
        hazard_pointer_detail::default_domain().release(record);
    }

    void require_nonempty() const noexcept {
        if (!record_) terminate();
    }

    template<class T>
    static void require_protectable() noexcept {
        static_assert(hazard_pointer_detail::is_protectable<T>::value,
                      "T must derive from hazard_pointer_obj_base<T, D>");
    }

    friend hazard_pointer make_hazard_pointer();

public:
    hazard_pointer() noexcept : record_(nullptr) {}
    hazard_pointer(const hazard_pointer&) = delete;
    hazard_pointer& operator=(const hazard_pointer&) = delete;

    hazard_pointer(hazard_pointer&& other) noexcept : record_(other.record_) {
        other.record_ = nullptr;
    }

    hazard_pointer& operator=(hazard_pointer&& other) noexcept {
        if (this == &other) return *this;
        release();
        record_ = other.record_;
        other.record_ = nullptr;
        return *this;
    }

    ~hazard_pointer() { release(); }

    [[nodiscard]] bool empty() const noexcept { return record_ == nullptr; }

    template<class T>
    T* protect(const atomic<T*>& source) noexcept {
        require_protectable<T>();
        require_nonempty();
        T* pointer = source.load(memory_order_relaxed);
        while (!try_protect(pointer, source)) {}
        return pointer;
    }

    template<class T>
    bool try_protect(T*& pointer, const atomic<T*>& source) noexcept {
        require_protectable<T>();
        require_nonempty();
        T* old = pointer;
        reset_protection(old);
        pointer = source.load(memory_order_acquire);
        if (old != pointer) reset_protection();
        return old == pointer;
    }

    template<class T>
    void reset_protection(const T* pointer) noexcept {
        require_protectable<T>();
        require_nonempty();
        if (!pointer) {
            reset_protection();
            return;
        }
        const volatile void* qualified_address =
            static_cast<const volatile void*>(pointer);
        const void* address = const_cast<const void*>(qualified_address);
        hazard_pointer_detail::default_domain().set(record_, address);
    }

    void reset_protection(nullptr_t = nullptr) noexcept {
        require_nonempty();
        hazard_pointer_detail::default_domain().set(record_, nullptr);
    }

    void swap(hazard_pointer& other) noexcept {
        hazard_pointer_detail::hazard_record* temporary = record_;
        record_ = other.record_;
        other.record_ = temporary;
    }
};

inline hazard_pointer make_hazard_pointer() {
    return hazard_pointer(hazard_pointer_detail::default_domain().acquire());
}

inline void swap(hazard_pointer& left, hazard_pointer& right) noexcept {
    left.swap(right);
}

/* The batch helpers are kept transactional even when allocation of a later
 * record fails: acquired records live in a temporary array until every
 * requested acquisition succeeds. */
inline void make_hazard_pointer_batch(span<hazard_pointer> batch) {
    size_t count = 0;
    for (size_t i = 0; i < batch.size(); ++i)
        if (batch[i].empty()) ++count;
    if (!count) return;

    hazard_pointer* staged = new hazard_pointer[count];
    size_t staged_count = 0;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
        for (; staged_count < count; ++staged_count)
            staged[staged_count] = make_hazard_pointer();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        delete[] staged;
        throw;
    }
#endif

    size_t next = 0;
    for (size_t i = 0; i < batch.size(); ++i)
        if (batch[i].empty()) batch[i] = std::move(staged[next++]);
    delete[] staged;
}

inline void clear_hazard_pointer_batch(span<hazard_pointer> batch) noexcept {
    for (size_t i = 0; i < batch.size(); ++i)
        if (!batch[i].empty()) batch[i] = hazard_pointer();
}

} // namespace std

#endif /* C++26 hazard pointers */
#endif /* RINCXX_HAZARD_POINTER_H */
