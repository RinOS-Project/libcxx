/*
 * RinOS C++ <forward_list>
 * 単方向連結リスト実装
 */

#ifndef RINCXX_FORWARD_LIST_H
#define RINCXX_FORWARD_LIST_H

#include "rincxx.h"
#include "memory.h"
#include "iterator.h"
#include "initializer_list.h"
#include "exception.h"
#if __cplusplus >= 202002L
#include "compare.h"
#endif
#if __cplusplus > 202002L
#include "ranges.h"
#endif

#ifdef __cplusplus

namespace std {

namespace detail {

[[noreturn]] inline void forward_list_length_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw length_error("forward_list exceeds max_size");
#else
    terminate();
#endif
}

/* forward_list keeps its intrusive next links as raw addresses, but an
 * allocator is allowed to return a fancy pointer for the node allocation.
 * Convert only at the allocation boundary and reconstruct the allocator
 * pointer for deallocation through pointer_traits.  This keeps every link
 * operation independent of operator-> or pointer arithmetic on the fancy
 * pointer itself. */
template<typename T>
inline T* forward_list_pointer_address(T* pointer) noexcept {
    return pointer;
}

#if __cplusplus >= 202002L
template<typename Pointer>
inline auto forward_list_pointer_address(const Pointer& pointer) noexcept
    -> decltype(std::to_address(pointer)) {
    return std::to_address(pointer);
}
#else
template<typename Pointer>
inline auto forward_list_pointer_address(const Pointer& pointer) noexcept
    -> decltype(pointer_traits<Pointer>::to_address(pointer)) {
    return pointer_traits<Pointer>::to_address(pointer);
}

template<typename Pointer>
inline auto forward_list_pointer_address(const Pointer& pointer) noexcept
    -> decltype(forward_list_pointer_address(pointer.operator->())) {
    return forward_list_pointer_address(pointer.operator->());
}
#endif

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * forward_list node
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
struct _forward_list_node {
    T value;
    _forward_list_node* next;

    template<typename... Args>
    _forward_list_node(Args&&... args)
        : value(std::forward<Args>(args)...), next(nullptr) {}
};

/* ═══════════════════════════════════════════════════════════════
 * forward_list iterator
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
class _forward_list_iterator {
public:
    using iterator_category = forward_iterator_tag;
    using value_type = T;
    using difference_type = ptrdiff_t;
    using pointer = T*;
    using reference = T&;
    using node_type = _forward_list_node<T>;

private:
    node_type* node_;
    node_type** before_head_;
    node_type** owner_;

public:
    _forward_list_iterator()
        : node_(nullptr), before_head_(nullptr), owner_(nullptr) {}
    explicit _forward_list_iterator(node_type* n, node_type** owner = nullptr)
        : node_(n), before_head_(nullptr), owner_(owner) {}
    _forward_list_iterator(node_type** head, bool before)
        : node_(nullptr), before_head_(before ? head : nullptr), owner_(head) {}

    reference operator*() const { return node_->value; }
    pointer operator->() const { return &node_->value; }

    _forward_list_iterator& operator++() {
        if (before_head_) {
            node_ = *before_head_;
            before_head_ = nullptr;
        } else if (node_) {
            node_ = node_->next;
        }
        return *this;
    }

    _forward_list_iterator operator++(int) {
        _forward_list_iterator tmp = *this;
        ++(*this);
        return tmp;
    }

    bool operator==(const _forward_list_iterator& other) const {
        return node_ == other.node_ && before_head_ == other.before_head_;
    }

    bool operator!=(const _forward_list_iterator& other) const {
        return !(*this == other);
    }

    node_type* _get_node() const { return node_; }
    bool _is_before_begin() const { return before_head_ != nullptr; }
    node_type** _get_before_head() const { return before_head_; }
    node_type** _get_owner() const { return owner_; }
};

template<typename T>
class _forward_list_const_iterator {
public:
    using iterator_category = forward_iterator_tag;
    using value_type = T;
    using difference_type = ptrdiff_t;
    using pointer = const T*;
    using reference = const T&;
    using node_type = _forward_list_node<T>;

private:
    node_type* node_;
    node_type** before_head_;
    node_type** owner_;

public:
    _forward_list_const_iterator()
        : node_(nullptr), before_head_(nullptr), owner_(nullptr) {}
    explicit _forward_list_const_iterator(node_type* n, node_type** owner = nullptr)
        : node_(n), before_head_(nullptr), owner_(owner) {}
    _forward_list_const_iterator(node_type** head, bool before)
        : node_(nullptr), before_head_(before ? head : nullptr), owner_(head) {}
    _forward_list_const_iterator(const _forward_list_iterator<T>& it)
        : node_(it._get_node()), before_head_(it._get_before_head()),
          owner_(it._get_owner()) {}

    reference operator*() const { return node_->value; }
    pointer operator->() const { return &node_->value; }

    _forward_list_const_iterator& operator++() {
        if (before_head_) {
            node_ = *before_head_;
            before_head_ = nullptr;
        } else if (node_) {
            node_ = node_->next;
        }
        return *this;
    }

    _forward_list_const_iterator operator++(int) {
        _forward_list_const_iterator tmp = *this;
        ++(*this);
        return tmp;
    }

    bool operator==(const _forward_list_const_iterator& other) const {
        return node_ == other.node_ && before_head_ == other.before_head_;
    }

    bool operator!=(const _forward_list_const_iterator& other) const {
        return !(*this == other);
    }

    const _forward_list_node<T>* _get_node() const { return node_; }
    bool _is_before_begin() const { return before_head_ != nullptr; }
    node_type** _get_before_head() const { return before_head_; }
    node_type** _get_owner() const { return owner_; }
};

template<typename T>
inline bool operator==(const _forward_list_iterator<T>& left,
                       const _forward_list_const_iterator<T>& right) {
    return left._get_node() == right._get_node() &&
           left._get_before_head() == right._get_before_head();
}

template<typename T>
inline bool operator==(const _forward_list_const_iterator<T>& left,
                       const _forward_list_iterator<T>& right) {
    return right == left;
}

template<typename T>
inline bool operator!=(const _forward_list_iterator<T>& left,
                       const _forward_list_const_iterator<T>& right) {
    return !(left == right);
}

template<typename T>
inline bool operator!=(const _forward_list_const_iterator<T>& left,
                       const _forward_list_iterator<T>& right) {
    return !(left == right);
}

/* ═══════════════════════════════════════════════════════════════
 * forward_list
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, typename Allocator = allocator<T>>
class forward_list {
public:
    using value_type = T;
    using allocator_type = Allocator;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = _forward_list_iterator<T>;
    using const_iterator = _forward_list_const_iterator<T>;

private:
    using node_type = _forward_list_node<T>;
    using node_allocator_type = typename allocator_traits<allocator_type>::template rebind_alloc<node_type>;
    using node_allocator_traits = allocator_traits<node_allocator_type>;
    using node_pointer = typename node_allocator_traits::pointer;
    struct default_compare {
        bool operator()(const T& left, const T& right) const {
            return left < right;
        }
    };
    allocator_type allocator_;
    node_type* head_;

    node_type** owner_token() noexcept { return &head_; }
    node_type** owner_token() const noexcept {
        return const_cast<node_type**>(&head_);
    }

    bool owns_node(const node_type* node) const noexcept {
        if (!node) return false;
        for (const node_type* current = head_; current; current = current->next) {
            if (current == node) return true;
        }
        return false;
    }

    bool valid_position(const const_iterator& position) const noexcept {
        if (position._is_before_begin())
            return position._get_before_head() == owner_token();
        const node_type* node = position._get_node();
        if (!node) return position._get_owner() == owner_token();
        /* Node iterators remain usable with the destination after splice or
         * swap.  Prove membership by walking this list instead of trusting
         * the iterator's historical owner token. */
        return owns_node(node);
    }

    bool valid_predecessor(const const_iterator& position) const noexcept {
        if (!valid_position(position)) return false;
        return position._is_before_begin() || position._get_node() != nullptr;
    }

    bool valid_endpoint(const const_iterator& position) const noexcept {
        return valid_position(position) && !position._is_before_begin();
    }

    bool reachable_after(const const_iterator& first,
                         const const_iterator& last) const noexcept {
        if (!valid_predecessor(first) || !valid_endpoint(last)) return false;
        node_type* current = first._is_before_begin()
            ? head_ : const_cast<node_type*>(first._get_node())->next;
        node_type* stop = const_cast<node_type*>(last._get_node());
        while (current && current != stop) current = current->next;
        return current == stop;
    }

    iterator make_iterator(node_type* node) noexcept {
        return iterator(node, owner_token());
    }

    void destroy_chain(node_type* head) noexcept {
        while (head) {
            node_type* next = head->next;
            destroy_node(head);
            head = next;
        }
    }

    void ensure_count(size_type count) const {
        if (count > max_size())
            detail::forward_list_length_failure();
    }

    void ensure_additional(size_type current, size_type extra) const {
        const size_type limit = max_size();
        if (current > limit || extra > limit - current)
            detail::forward_list_length_failure();
    }

    void ensure_next(size_type base, size_type appended) const {
        const size_type limit = max_size();
        if (base > limit || appended >= limit - base)
            detail::forward_list_length_failure();
    }

    [[noreturn]] static void allocation_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        throw bad_alloc();
#else
        terminate();
#endif
    }

    template<typename... Args>
    node_type* allocate_node(Args&&... args) {
        node_allocator_type alloc(allocator_);
        node_pointer allocation = node_allocator_traits::allocate(alloc, 1);
        node_type* node = detail::forward_list_pointer_address(allocation);
        if (allocation == node_pointer() || !node) {
            if (allocation != node_pointer())
                node_allocator_traits::deallocate(alloc, allocation, 1);
            allocation_failure();
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            node_allocator_traits::construct(alloc, node,
                                             std::forward<Args>(args)...);
            return node;
        } catch (...) {
            node_allocator_traits::deallocate(alloc, allocation, 1);
            throw;
        }
#else
        node_allocator_traits::construct(alloc, node,
                                         std::forward<Args>(args)...);
        return node;
#endif
    }

    node_type* create_node(const T& value) {
        return allocate_node(value);
    }

    node_type* create_node(T&& value) {
        return allocate_node(std::move(value));
    }

    template<typename... Args>
    node_type* create_node_emplace(Args&&... args) {
        return allocate_node(std::forward<Args>(args)...);
    }

    node_type* clone_chain(const node_type* source) {
        size_type count = 0;
        for (const node_type* current = source; current; current = current->next)
            ++count;
        ensure_count(count);
        node_type* result = nullptr;
        node_type** tail = &result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            while (source) {
                node_type* node = create_node(source->value);
                *tail = node;
                tail = &node->next;
                source = source->next;
            }
            return result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_chain(result);
            throw;
        }
#endif
    }

    node_type* repeated_chain(size_type count, const T& value) {
        ensure_count(count);
        node_type* result = nullptr;
        node_type** tail = &result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (size_type i = 0; i < count; ++i) {
                node_type* node = create_node(value);
                *tail = node;
                tail = &node->next;
            }
            return result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_chain(result);
            throw;
        }
#endif
    }

    node_type* default_chain(size_type count) {
        ensure_count(count);
        node_type* result = nullptr;
        node_type** tail = &result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (size_type i = 0; i < count; ++i) {
                node_type* node = create_node_emplace();
                *tail = node;
                tail = &node->next;
            }
            return result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_chain(result);
            throw;
        }
#endif
    }

    node_type* initializer_chain(initializer_list<T> init) {
        ensure_count(init.size());
        node_type* result = nullptr;
        node_type** tail = &result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (const T* it = init.begin(); it != init.end(); ++it) {
                node_type* node = create_node(*it);
                *tail = node;
                tail = &node->next;
            }
            return result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_chain(result);
            throw;
        }
#endif
    }

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, T>
    node_type* range_chain(R&& range, size_type base = 0) {
        node_type* result = nullptr;
        node_type** tail = &result;
        size_type count = 0;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            auto first = ranges::begin(range);
            auto last = ranges::end(range);
            for (; first != last; ++first) {
                ensure_next(base, count);
                node_type* node = create_node(*first);
                *tail = node;
                tail = &node->next;
                ++count;
            }
            return result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_chain(result);
            throw;
        }
#endif
    }
#endif

    void destroy_node(node_type* node) {
        if (node) {
            node_allocator_type alloc(allocator_);
            node_allocator_traits::destroy(alloc, node);
            node_allocator_traits::deallocate(
                alloc,
                pointer_traits<node_pointer>::pointer_to(*node),
                1);
        }
    }

    node_type* relocate_value(node_type* source, true_type) {
        return create_node(source->value);
    }

    node_type* relocate_value(node_type* source, false_type) {
        /* A copy-deleted value can still be MoveInsertable when its move
         * constructor may throw.  Build the replacement node privately and
         * let the constructor exception propagate; callers retain all list
         * links and therefore expose only the standard basic guarantee (a
         * previously moved source value may be changed). */
        return create_node(std::move(source->value));
    }

    node_type* relocate_chain(node_type* first, node_type* stop,
                              node_type*& tail) {
        node_type* result = nullptr;
        tail = nullptr;
        node_type** link = &result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (node_type* source = first; source != stop; source = source->next) {
                node_type* node = relocate_value(
                    source, integral_constant<bool,
                    is_copy_constructible<T>::value>());
                *link = node;
                link = &node->next;
                tail = node;
            }
            return result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_chain(result);
            throw;
        }
#endif
    }

    template<typename Compare>
    void merge_unequal(forward_list& other, Compare comp) {
        node_type* result = nullptr;
        node_type* tail = nullptr;
        node_type* left = head_;
        node_type* right = other.head_;
        node_type* original_left = head_;
        node_type* original_right = other.head_;
        node_type** link = &result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            while (left || right) {
                node_type* selected = nullptr;
                if (!right || (left && !comp(right->value, left->value))) {
                    selected = left;
                    left = left->next;
                } else {
                    selected = right;
                    right = right->next;
                }
                node_type* node = relocate_value(
                    selected, integral_constant<bool,
                    is_copy_constructible<T>::value>());
                *link = node;
                link = &node->next;
                tail = node;
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_chain(result);
            throw;
        }
#endif
        (void)tail;
        destroy_chain(original_left);
        other.destroy_chain(original_right);
        head_ = result;
        other.head_ = nullptr;
    }

    void swap_unequal(forward_list& other) {
        const size_type this_count = count_nodes(head_);
        const size_type other_count = count_nodes(other.head_);
        if (other_count > max_size() || this_count > other.max_size())
            detail::forward_list_length_failure();
        forward_list left(allocator_);
        forward_list right(other.allocator_);
        node_type* left_tail = nullptr;
        node_type* right_tail = nullptr;
        left.head_ = left.relocate_chain(other.head_, nullptr, left_tail);
        right.head_ = right.relocate_chain(head_, nullptr, right_tail);

        node_type* old_head = head_;
        node_type* old_other = other.head_;
        head_ = left.head_;
        left.head_ = nullptr;
        other.head_ = right.head_;
        right.head_ = nullptr;
        destroy_chain(old_head);
        other.destroy_chain(old_other);
    }

public:
    /* コンストラクタ */
    forward_list() : allocator_(), head_(nullptr) {}

    explicit forward_list(const allocator_type& alloc)
        : allocator_(alloc), head_(nullptr) {}

    forward_list(size_type count, const T& value)
        : allocator_(), head_(repeated_chain(count, value)) {}

    forward_list(size_type count, const T& value, const allocator_type& alloc)
        : allocator_(alloc), head_(repeated_chain(count, value)) {}

    explicit forward_list(size_type count) : allocator_(), head_(nullptr) {
        ensure_count(count);
        node_type* result = nullptr;
        node_type** tail = &result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (size_type i = 0; i < count; ++i) {
                node_type* node = create_node_emplace();
                *tail = node;
                tail = &node->next;
            }
            head_ = result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_chain(result);
            throw;
        }
#endif
    }

    forward_list(size_type count, const allocator_type& alloc)
        : allocator_(alloc), head_(nullptr) {
        ensure_count(count);
        node_type* result = nullptr;
        node_type** tail = &result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (size_type i = 0; i < count; ++i) {
                node_type* node = create_node_emplace();
                *tail = node;
                tail = &node->next;
            }
            head_ = result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_chain(result);
            throw;
        }
#endif
    }

    template<typename InputIt,
             typename = typename enable_if<!is_integral<remove_cv_t<InputIt>>::value>::type>
    forward_list(InputIt first, InputIt last) : allocator_(), head_(nullptr) {
        node_type* result = nullptr;
        node_type** tail = &result;
        size_type count = 0;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (; first != last; ++first) {
                ensure_next(0, count);
                node_type* node = create_node(*first);
                *tail = node;
                tail = &node->next;
                ++count;
            }
            head_ = result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_chain(result);
            throw;
        }
#endif
    }

    template<typename InputIt,
             typename = typename enable_if<!is_integral<remove_cv_t<InputIt>>::value>::type>
    forward_list(InputIt first, InputIt last, const allocator_type& alloc)
        : allocator_(alloc), head_(nullptr) {
        node_type* result = nullptr;
        node_type** tail = &result;
        size_type count = 0;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (; first != last; ++first) {
                ensure_next(0, count);
                node_type* node = create_node(*first);
                *tail = node;
                tail = &node->next;
                ++count;
            }
            head_ = result;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_chain(result);
            throw;
        }
#endif
    }

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, T>
    forward_list(from_range_t, R&& range)
        : allocator_(), head_(range_chain(std::forward<R>(range))) {}

    template<typename R>
        requires detail::container_compatible_range<R, T>
    forward_list(from_range_t, R&& range, const allocator_type& alloc)
        : allocator_(alloc), head_(range_chain(std::forward<R>(range))) {}
#endif

    forward_list(initializer_list<T> init)
        : allocator_(), head_(initializer_chain(init)) {}

    forward_list(initializer_list<T> init, const allocator_type& alloc)
        : allocator_(alloc), head_(initializer_chain(init)) {}

    forward_list(const forward_list& other)
        : allocator_(node_allocator_traits::select_on_container_copy_construction(
              other.allocator_)),
          head_(nullptr) {
        if (count_nodes(other.head_) > max_size())
            detail::forward_list_length_failure();
        head_ = clone_chain(other.head_);
    }

    forward_list(const forward_list& other, const allocator_type& alloc)
        : allocator_(alloc), head_(nullptr) {
        if (count_nodes(other.head_) > max_size())
            detail::forward_list_length_failure();
        head_ = clone_chain(other.head_);
    }

    forward_list(forward_list&& other) noexcept
        : allocator_(std::move(other.allocator_)), head_(other.head_) {
        other.head_ = nullptr;
    }

    forward_list(forward_list&& other, const allocator_type& alloc)
        : allocator_(alloc), head_(nullptr) {
        if (allocator_ == other.allocator_) {
            head_ = other.head_;
            other.head_ = nullptr;
        } else {
            if (count_nodes(other.head_) > max_size())
                detail::forward_list_length_failure();
            node_type* tail = nullptr;
            head_ = relocate_chain(other.head_, nullptr, tail);
            other.clear();
        }
    }

    ~forward_list() {
        clear();
    }

    /* 代入演算子 */
    forward_list& operator=(const forward_list& other) {
        if (this != &other) {
            const bool propagate = node_allocator_traits::propagate_on_container_copy_assignment::value;
            if (propagate) {
                forward_list replacement(other, other.allocator_);
                destroy_chain(head_);
                allocator_ = other.allocator_;
                head_ = replacement.head_;
                replacement.head_ = nullptr;
            } else {
                if (count_nodes(other.head_) > max_size())
                    detail::forward_list_length_failure();
                node_type* replacement = clone_chain(other.head_);
                destroy_chain(head_);
                head_ = replacement;
            }
        }
        return *this;
    }

    forward_list& operator=(forward_list&& other) {
        if (this != &other) {
            const bool propagate = node_allocator_traits::propagate_on_container_move_assignment::value;
            if (propagate || allocator_ == other.allocator_) {
                clear();
                if (propagate) allocator_ = std::move(other.allocator_);
                head_ = other.head_;
                other.head_ = nullptr;
            } else {
                if (count_nodes(other.head_) > max_size())
                    detail::forward_list_length_failure();
                /* Rebuild with this allocator so copy-deleted noexcept
                 * movable values use the admitted relocation path instead of
                 * an ill-formed copy constructor instantiation. */
                forward_list replacement(allocator_);
                node_type* tail = nullptr;
                replacement.head_ = replacement.relocate_chain(
                    other.head_, nullptr, tail);
                clear();
                head_ = replacement.head_;
                replacement.head_ = nullptr;
                other.clear();
            }
        }
        return *this;
    }

    forward_list& operator=(initializer_list<T> init) {
        node_type* replacement = initializer_chain(init);
        destroy_chain(head_);
        head_ = replacement;
        return *this;
    }

    void assign(size_type count, const T& value) {
        node_type* replacement = repeated_chain(count, value);
        destroy_chain(head_);
        head_ = replacement;
    }

    template<typename InputIt,
             typename = typename enable_if<!is_integral<remove_cv_t<InputIt>>::value>::type>
    void assign(InputIt first, InputIt last) {
        node_type* replacement = nullptr;
        node_type** tail = &replacement;
        size_type count = 0;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (; first != last; ++first) {
                ensure_next(0, count);
                node_type* node = create_node(*first);
                *tail = node;
                tail = &node->next;
                ++count;
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_chain(replacement);
            throw;
        }
#endif
        destroy_chain(head_);
        head_ = replacement;
    }

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, T>
    void assign_range(R&& range) {
        node_type* replacement = range_chain(std::forward<R>(range));
        destroy_chain(head_);
        head_ = replacement;
    }
#endif

    void assign(initializer_list<T> init) {
        node_type* replacement = initializer_chain(init);
        destroy_chain(head_);
        head_ = replacement;
    }

    /* イテレータ */
    iterator before_begin() noexcept {
        return iterator(owner_token(), true);
    }

    const_iterator before_begin() const noexcept {
        return const_iterator(owner_token(), true);
    }

    iterator begin() noexcept { return make_iterator(head_); }
    const_iterator begin() const noexcept {
        return const_iterator(head_, owner_token());
    }
    const_iterator cbegin() const noexcept {
        return const_iterator(head_, owner_token());
    }

    const_iterator cbefore_begin() const noexcept {
        return const_iterator(owner_token(), true);
    }

    iterator end() noexcept { return make_iterator(nullptr); }
    const_iterator end() const noexcept {
        return const_iterator(nullptr, owner_token());
    }
    const_iterator cend() const noexcept {
        return const_iterator(nullptr, owner_token());
    }

    allocator_type get_allocator() const noexcept { return allocator_; }

    /* 容量 */
    bool empty() const noexcept { return head_ == nullptr; }

    size_type max_size() const noexcept {
        /* The rebound node allocator owns every element; report its bound
         * instead of an address-space estimate that bounded allocators cannot
         * actually satisfy. */
        return node_allocator_traits::max_size(node_allocator_type(allocator_));
    }

    /* 要素アクセス */
    reference front() noexcept { return head_->value; }
    const_reference front() const noexcept { return head_->value; }

    /* 変更 */
    void clear() noexcept {
        while (head_) {
            node_type* next = head_->next;
            destroy_node(head_);
            head_ = next;
        }
    }

    void push_front(const T& value) {
        ensure_next(count_nodes(head_), 0);
        node_type* node = create_node(value);
        node->next = head_;
        head_ = node;
    }

    void push_front(T&& value) {
        ensure_next(count_nodes(head_), 0);
        node_type* node = create_node_emplace(std::move(value));
        node->next = head_;
        head_ = node;
    }

    template<typename... Args>
    reference emplace_front(Args&&... args) {
        ensure_next(count_nodes(head_), 0);
        node_type* node = create_node_emplace(std::forward<Args>(args)...);
        node->next = head_;
        head_ = node;
        return node->value;
    }

    void pop_front() noexcept {
        if (head_) {
            node_type* next = head_->next;
            destroy_node(head_);
            head_ = next;
        }
    }

    iterator insert_after(const_iterator pos, const T& value) {
        if (!valid_predecessor(pos)) return end();
        ensure_next(count_nodes(head_), 0);
        node_type* pos_node = const_cast<node_type*>(pos._get_node());
        node_type* node = create_node(value);
        node_type** link = pos._is_before_begin() ? &head_ : &pos_node->next;
        node->next = *link;
        *link = node;
        return make_iterator(node);
    }

    iterator insert_after(const_iterator pos, T&& value) {
        if (!valid_predecessor(pos)) return end();
        ensure_next(count_nodes(head_), 0);
        node_type* pos_node = const_cast<node_type*>(pos._get_node());
        node_type* node = create_node_emplace(std::move(value));
        node_type** link = pos._is_before_begin() ? &head_ : &pos_node->next;
        node->next = *link;
        *link = node;
        return make_iterator(node);
    }

    iterator insert_after(const_iterator pos, size_type count, const T& value) {
        if (!valid_predecessor(pos)) return end();
        const size_type current = count_nodes(head_);
        ensure_additional(current, count);
        node_type* pos_node = const_cast<node_type*>(pos._get_node());
        node_type** link = pos._is_before_begin() ? &head_ : &pos_node->next;
        if (count == 0) return pos._is_before_begin() ? before_begin() : make_iterator(pos_node);
        node_type* chain = repeated_chain(count, value);
        node_type* chain_tail = chain;
        while (chain_tail->next) chain_tail = chain_tail->next;
        chain_tail->next = *link;
        *link = chain;
        return make_iterator(chain);
    }

    template<typename InputIt,
             typename = typename enable_if<!is_integral<remove_cv_t<InputIt>>::value>::type>
    iterator insert_after(const_iterator pos, InputIt first, InputIt last) {
        if (!valid_predecessor(pos)) return end();
        const size_type current = count_nodes(head_);
        node_type* chain = nullptr;
        node_type* chain_tail = nullptr;
        size_type count = 0;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (; first != last; ++first) {
                ensure_next(current, count);
                node_type* node = create_node(*first);
                if (chain_tail) {
                    chain_tail->next = node;
                } else {
                    chain = node;
                }
                chain_tail = node;
                ++count;
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_chain(chain);
            throw;
        }
#endif
        node_type* pos_node = const_cast<node_type*>(pos._get_node());
        node_type** link = pos._is_before_begin() ? &head_ : &pos_node->next;
        if (!chain) return pos._is_before_begin() ? before_begin() : make_iterator(pos_node);
        chain_tail->next = *link;
        *link = chain;
        return make_iterator(chain);
    }

    iterator insert_after(const_iterator pos, initializer_list<T> values) {
        return insert_after(pos, values.begin(), values.end());
    }

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, T>
    iterator insert_range_after(const_iterator pos, R&& range) {
        if (!valid_predecessor(pos)) return end();
        node_type* chain = range_chain(std::forward<R>(range),
                                       count_nodes(head_));
        if (!chain) return pos._is_before_begin()
            ? before_begin() : make_iterator(const_cast<node_type*>(pos._get_node()));
        node_type* tail = chain;
        while (tail->next) tail = tail->next;
        node_type* pos_node = const_cast<node_type*>(pos._get_node());
        node_type** link = pos._is_before_begin() ? &head_ : &pos_node->next;
        tail->next = *link;
        *link = chain;
        return make_iterator(chain);
    }

    template<typename R>
        requires detail::container_compatible_range<R, T>
    void prepend_range(R&& range) {
        insert_range_after(before_begin(), std::forward<R>(range));
    }
#endif

    template<typename... Args>
    iterator emplace_after(const_iterator pos, Args&&... args) {
        if (!valid_predecessor(pos)) return end();
        ensure_next(count_nodes(head_), 0);
        node_type* pos_node = const_cast<node_type*>(pos._get_node());
        node_type* node = create_node_emplace(std::forward<Args>(args)...);
        node_type** link = pos._is_before_begin() ? &head_ : &pos_node->next;
        node->next = *link;
        *link = node;
        return make_iterator(node);
    }

    void splice_after(const_iterator pos, forward_list& other) {
        if (!valid_predecessor(pos)) return;
        if (this == &other || !other.head_) return;
        const size_type current = count_nodes(head_);
        const size_type moved = count_nodes(other.head_);
        if (current > max_size() || moved > max_size() - current)
            detail::forward_list_length_failure();
        node_type* pos_node = const_cast<node_type*>(pos._get_node());
        node_type** destination = pos._is_before_begin() ? &head_ : &pos_node->next;
        if (allocator_ != other.allocator_) {
            node_type* tail = nullptr;
            node_type* chain = relocate_chain(other.head_, nullptr, tail);
            if (!chain) return;
            tail->next = *destination;
            *destination = chain;
            node_type* old = other.head_;
            other.head_ = nullptr;
            other.destroy_chain(old);
            return;
        }
        node_type* tail = other.head_;
        while (tail->next) tail = tail->next;
        tail->next = *destination;
        *destination = other.head_;
        other.head_ = nullptr;
    }

    void splice_after(const_iterator pos, forward_list& other,
                      const_iterator before) {
        if (!valid_predecessor(pos) || !other.valid_predecessor(before)) return;
        node_type* before_node = const_cast<node_type*>(before._get_node());
        node_type** source = before._is_before_begin() ? &other.head_ : &before_node->next;
        node_type* moved = *source;
        if (!moved) return;

        node_type* pos_node = const_cast<node_type*>(pos._get_node());
        node_type** destination = pos._is_before_begin() ? &head_ : &pos_node->next;
        if (this == &other && (destination == source || pos_node == moved)) return;

        if (this != &other) {
            const size_type current = count_nodes(head_);
            if (current > max_size() || 1 > max_size() - current)
                detail::forward_list_length_failure();
        }

        if (allocator_ != other.allocator_) {
            node_type* replacement = relocate_value(
                moved, integral_constant<bool, is_copy_constructible<T>::value>());
            *source = moved->next;
            other.destroy_node(moved);
            replacement->next = *destination;
            *destination = replacement;
            return;
        }

        *source = moved->next;
        moved->next = *destination;
        *destination = moved;
    }

    void splice_after(const_iterator pos, forward_list& other,
                      const_iterator before_first, const_iterator before_last) {
        if (!valid_predecessor(pos) ||
            !other.reachable_after(before_first, before_last)) return;
        node_type* first_node = const_cast<node_type*>(before_first._get_node());
        node_type** source = before_first._is_before_begin() ? &other.head_ : &first_node->next;
        node_type* first = *source;
        node_type* stop = before_last._is_before_begin()
            ? other.head_ : const_cast<node_type*>(before_last._get_node());
        if (!first || first == stop) return;

        node_type* tail = first;
        size_type moved = 1;
        while (tail->next && tail->next != stop) tail = tail->next;
        if (tail->next != stop) return;
        for (node_type* node = first; node != tail; node = node->next)
            ++moved;

        node_type* pos_node = const_cast<node_type*>(pos._get_node());
        node_type** destination = pos._is_before_begin() ? &head_ : &pos_node->next;
        if (this == &other) {
            /* Inserting after the range's end marker is already the
             * range's current position.  Treat it as the standard no-op;
             * linking the tail after stop would otherwise create a cycle. */
            if (destination == source || pos_node == stop) return;
            for (node_type* node = first; node != stop; node = node->next) {
                if (node == pos_node) return;
            }
        }

        if (this != &other) {
            const size_type current = count_nodes(head_);
            if (current > max_size() || moved > max_size() - current)
                detail::forward_list_length_failure();
        }

        if (allocator_ != other.allocator_) {
            node_type* relocated_tail = nullptr;
            node_type* relocated = relocate_chain(
                first, stop, relocated_tail);
            if (!relocated) return;
            *source = stop;
            node_type* old = first;
            while (old != stop) {
                node_type* next = old->next;
                other.destroy_node(old);
                old = next;
            }
            relocated_tail->next = *destination;
            *destination = relocated;
            return;
        }

        *source = stop;
        tail->next = *destination;
        *destination = first;
    }

    iterator erase_after(const_iterator pos) {
        if (!valid_predecessor(pos)) return end();
        node_type* pos_node = const_cast<node_type*>(pos._get_node());
        node_type** link = pos._is_before_begin() ? &head_ :
            (pos_node ? &pos_node->next : nullptr);
        if (link && *link) {
            node_type* to_delete = *link;
            *link = to_delete->next;
            destroy_node(to_delete);
        }
        return make_iterator(link ? *link : nullptr);
    }

    iterator erase_after(const_iterator first, const_iterator last) {
        if (!reachable_after(first, last)) return end();
        node_type* first_node = const_cast<node_type*>(first._get_node());
        node_type* last_node = const_cast<node_type*>(last._get_node());

        node_type** link = first._is_before_begin() ? &head_ : &first_node->next;
        node_type* curr = *link;
        while (curr != last_node) {
            node_type* next = curr->next;
            destroy_node(curr);
            curr = next;
        }
        *link = last_node;
        return make_iterator(last_node);
    }

    void resize(size_type count) {
        if (count > max_size())
            detail::forward_list_length_failure();

        size_type current_size = 0;
        node_type* prev = nullptr;
        node_type* curr = head_;

        while (curr && current_size < count) {
            prev = curr;
            curr = curr->next;
            ++current_size;
        }

        if (current_size < count) {
            /* Default insertion must not require copying a temporary T.  The
             * suffix is built privately so a throwing constructor preserves
             * the existing prefix. */
            node_type* chain = default_chain(count - current_size);
            if (prev) {
                prev->next = chain;
            } else {
                head_ = chain;
            }
        } else {
            while (curr) {
                node_type* next = curr->next;
                destroy_node(curr);
                curr = next;
            }
            if (prev) {
                prev->next = nullptr;
            } else {
                head_ = nullptr;
            }
        }
    }

    void resize(size_type count, const T& value) {
        if (count > max_size())
            detail::forward_list_length_failure();

        size_type current_size = 0;
        node_type* prev = nullptr;
        node_type* curr = head_;

        while (curr && current_size < count) {
            prev = curr;
            curr = curr->next;
            ++current_size;
        }

        if (current_size < count) {
            /* 追加 */
            node_type* chain = repeated_chain(count - current_size, value);
            if (prev) {
                prev->next = chain;
            } else {
                head_ = chain;
            }
        } else {
            /* 削除 */
            while (curr) {
                node_type* next = curr->next;
                destroy_node(curr);
                curr = next;
            }
            if (prev) {
                prev->next = nullptr;
            } else {
                head_ = nullptr;
            }
        }
    }

    void swap(forward_list& other) noexcept(
        node_allocator_traits::propagate_on_container_swap::value ||
        node_allocator_traits::is_always_equal::value) {
        if (this == &other) return;
        if (!node_allocator_traits::propagate_on_container_swap::value &&
            allocator_ != other.allocator_) {
            swap_unequal(other);
            return;
        }
        if (node_allocator_traits::propagate_on_container_swap::value) {
            allocator_type tmp = std::move(allocator_);
            allocator_ = std::move(other.allocator_);
            other.allocator_ = std::move(tmp);
        }
        node_type* tmp = head_;
        head_ = other.head_;
        other.head_ = tmp;
    }

    /* 操作 */
    void reverse() noexcept {
        node_type* prev = nullptr;
        node_type* curr = head_;
        while (curr) {
            node_type* next = curr->next;
            curr->next = prev;
            prev = curr;
            curr = next;
        }
        head_ = prev;
    }

    size_type remove(const T& value) {
        return remove_if([&value](const T& candidate) {
            return candidate == value;
        });
    }

    template<typename UnaryPredicate>
    size_type remove_if(UnaryPredicate pred) {
        if (!head_) return 0;
        const size_type capacity = count_nodes(head_);
        node_type** removed_nodes = allocate_order(capacity);
        size_type removed = 0;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (node_type* node = head_; node; node = node->next) {
                if (pred(node->value)) removed_nodes[removed++] = node;
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            rin_free(removed_nodes);
            throw;
        }
#endif
        size_type selected = 0;
        node_type** link = &head_;
        while (*link) {
            if (selected < removed && *link == removed_nodes[selected]) {
                node_type* node = *link;
                *link = node->next;
                destroy_node(node);
                ++selected;
            } else {
                link = &(*link)->next;
            }
        }
        rin_free(removed_nodes);
        return removed;
    }

    size_type unique() {
        if (!head_) return 0;
        size_type removed = 0;
        node_type* curr = head_;
        while (curr->next) {
            if (curr->value == curr->next->value) {
                node_type* to_delete = curr->next;
                curr->next = to_delete->next;
                destroy_node(to_delete);
                ++removed;
            } else {
                curr = curr->next;
            }
        }
        return removed;
    }

    template<typename BinaryPredicate>
    size_type unique(BinaryPredicate pred) {
        if (!head_) return 0;
        const size_type capacity = count_nodes(head_);
        node_type** removed_nodes = allocate_order(capacity);
        size_type removed = 0;
        node_type* curr = head_;
        node_type* next = curr->next;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
        while (next) {
            if (pred(curr->value, next->value)) {
                removed_nodes[removed++] = next;
                /* Keep curr as the last retained value, matching unique's
                 * adjacent-duplicate semantics while the chain is staged. */
                next = next->next;
            } else {
                curr = next;
                next = next->next;
            }
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            rin_free(removed_nodes);
            throw;
        }
#endif
        size_type selected = 0;
        node_type** link = &head_;
        while (*link) {
            if (selected < removed && *link == removed_nodes[selected]) {
                node_type* node = *link;
                *link = node->next;
                destroy_node(node);
                ++selected;
            } else {
                link = &(*link)->next;
            }
        }
        rin_free(removed_nodes);
        return removed;
    }

    void sort() {
        sort(default_compare());
    }

    template<typename Compare>
    void sort(Compare comp) {
        if (!head_ || !head_->next) return;
        const size_type count = count_nodes(head_);
        node_type** order = allocate_order(count);
        node_type* node = head_;
        for (size_type i = 0; i < count; ++i) {
            order[i] = node;
            node = node->next;
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (size_type i = 1; i < count; ++i) {
                node_type* selected = order[i];
                size_type position = i;
                while (position > 0 &&
                       comp(selected->value, order[position - 1]->value)) {
                    order[position] = order[position - 1];
                    --position;
                }
                order[position] = selected;
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            rin_free(order);
            throw;
        }
#endif
        for (size_type i = 1; i < count; ++i) order[i - 1]->next = order[i];
        order[count - 1]->next = nullptr;
        head_ = order[0];
        rin_free(order);
    }

    void merge(forward_list& other) {
        merge(other, default_compare());
    }

    template<typename Compare>
    void merge(forward_list& other, Compare comp) {
        if (this == &other || !other.head_) return;
        const size_type left_count = count_nodes(head_);
        const size_type right_count = count_nodes(other.head_);
        if (left_count > max_size() ||
            right_count > max_size() - left_count)
            detail::forward_list_length_failure();
        if (allocator_ != other.allocator_) {
            merge_unequal(other, comp);
            return;
        }
        const size_type count = left_count + right_count;
        if (count < left_count) allocation_failure();
        node_type** order = allocate_order(count);
        node_type* node = head_;
        for (size_type i = 0; i < left_count; ++i) {
            order[i] = node;
            node = node->next;
        }
        node = other.head_;
        for (size_type i = 0; i < right_count; ++i) {
            order[left_count + i] = node;
            node = node->next;
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (size_type i = 1; i < count; ++i) {
                node_type* selected = order[i];
                size_type position = i;
                while (position > 0 &&
                       comp(selected->value, order[position - 1]->value)) {
                    order[position] = order[position - 1];
                    --position;
                }
                order[position] = selected;
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            rin_free(order);
            throw;
        }
#endif
        for (size_type i = 1; i < count; ++i) order[i - 1]->next = order[i];
        order[count - 1]->next = nullptr;
        head_ = order[0];
        other.head_ = nullptr;
        rin_free(order);
    }

private:
    size_type count_nodes(const node_type* node) const noexcept {
        size_type count = 0;
        while (node) {
            if (count == static_cast<size_type>(-1)) allocation_failure();
            ++count;
            node = node->next;
        }
        return count;
    }

    node_type** allocate_order(size_type count) {
        if (count > static_cast<size_type>(-1) / sizeof(node_type*)) {
            allocation_failure();
        }
        void* memory = rin_malloc(count * sizeof(node_type*));
        if (!memory) allocation_failure();
        return static_cast<node_type**>(memory);
    }

};

/* 非メンバ関数 */
template<typename T, typename Alloc>
bool operator==(const forward_list<T, Alloc>& lhs, const forward_list<T, Alloc>& rhs) {
    auto it1 = lhs.begin();
    auto it2 = rhs.begin();
    while (it1 != lhs.end() && it2 != rhs.end()) {
        if (*it1 != *it2) return false;
        ++it1;
        ++it2;
    }
    return it1 == lhs.end() && it2 == rhs.end();
}

template<typename T, typename Alloc>
bool operator!=(const forward_list<T, Alloc>& lhs, const forward_list<T, Alloc>& rhs) {
    return !(lhs == rhs);
}

template<typename T, typename Alloc>
bool operator<(const forward_list<T, Alloc>& lhs,
               const forward_list<T, Alloc>& rhs) {
    return lexicographical_compare(lhs.begin(), lhs.end(),
                                   rhs.begin(), rhs.end());
}

template<typename T, typename Alloc>
bool operator<=(const forward_list<T, Alloc>& lhs,
                const forward_list<T, Alloc>& rhs) {
    return !(rhs < lhs);
}

template<typename T, typename Alloc>
bool operator>(const forward_list<T, Alloc>& lhs,
               const forward_list<T, Alloc>& rhs) {
    return rhs < lhs;
}

template<typename T, typename Alloc>
bool operator>=(const forward_list<T, Alloc>& lhs,
                const forward_list<T, Alloc>& rhs) {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
template<typename T, typename Alloc>
constexpr auto operator<=>(const forward_list<T, Alloc>& lhs,
                           const forward_list<T, Alloc>& rhs)
    -> detail::synth_three_way_result_t<T> {
    using result_type = detail::synth_three_way_result_t<T>;
    auto left = lhs.begin();
    auto right = rhs.begin();
    while (left != lhs.end() && right != rhs.end()) {
        const auto result = detail::synth_three_way(*left, *right);
        if (result != 0) return result;
        ++left;
        ++right;
    }
    if (left != lhs.end()) return result_type::greater;
    if (right != rhs.end()) return result_type::less;
    return result_type::equivalent;
}
#endif

template<typename T, typename Alloc>
void swap(forward_list<T, Alloc>& lhs, forward_list<T, Alloc>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

#if __cplusplus >= 202002L
template<typename T, typename Allocator>
typename forward_list<T, Allocator>::size_type
erase(forward_list<T, Allocator>& value, const T& element) {
    return value.remove(element);
}

template<typename T, typename Allocator, typename Pred>
typename forward_list<T, Allocator>::size_type
erase_if(forward_list<T, Allocator>& value, Pred predicate) {
    return value.remove_if(predicate);
}
#endif

#if __cplusplus > 202002L
template<ranges::input_range R>
forward_list(from_range_t, R&&)
    -> forward_list<ranges::range_value_t<R>>;

template<ranges::input_range R, class Allocator>
forward_list(from_range_t, R&&, Allocator)
    -> forward_list<ranges::range_value_t<R>, Allocator>;
#endif

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_FORWARD_LIST_H */
