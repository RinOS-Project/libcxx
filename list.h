/*
 * RinOS C++ <list> ✿
 * 双方向連結リスト実装
 */

#ifndef RINCXX_LIST_H
#define RINCXX_LIST_H

#include "rincxx.h"
#include "algorithm.h"
#include "iterator.h"
#include "memory.h"
#include "utility.h"
#include "initializer_list.h"
#include "functional.h"
#include "exception.h"
#if __cplusplus >= 202002L
#include "compare.h"
#endif
#if __cplusplus > 202002L
#include "ranges.h"
#endif

#ifdef __cplusplus

namespace std {

namespace list_detail {

[[noreturn]] inline void list_length_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw length_error("list exceeds max_size");
#else
    terminate();
#endif
}

/* Avoid host/Rin std::swap overload ambiguity at linked-list publication
 * boundaries while preserving value-semantic exchange and exception traits. */
template<typename T>
inline void list_exchange(T& left, T& right) noexcept(
    noexcept(T(std::move(left))) &&
    noexcept(left = std::move(right)) &&
    noexcept(right = std::move(left))) {
    if (&left == &right) return;
    T temporary(std::move(left));
    left = std::move(right);
    right = std::move(temporary);
}

template<typename T, typename Allocator, typename = void>
struct effective_allocator { using type = allocator<T>; };

template<typename T, typename Allocator>
struct effective_allocator<T, Allocator, void_t<
    typename Allocator::value_type,
    decltype(declval<Allocator&>().allocate(size_t{}))>> {
    using type = Allocator;
};

template<typename T>
struct effective_allocator<T, void, void> { using type = allocator<T>; };

/* The linked-list links are raw NodeBase addresses, while a rebound node
 * allocator may use a fancy pointer.  Keep conversion at the allocation
 * boundary and never require pointer arithmetic or operator-> from list
 * mutation code. */
template<typename T>
inline T* pointer_address(T* pointer) noexcept {
    return pointer;
}

#if __cplusplus >= 202002L
template<typename Pointer>
inline auto pointer_address(const Pointer& pointer) noexcept
    -> decltype(std::to_address(pointer)) {
    return std::to_address(pointer);
}
#else
template<typename Pointer>
inline auto pointer_address(const Pointer& pointer) noexcept
    -> decltype(pointer_traits<Pointer>::to_address(pointer)) {
    return pointer_traits<Pointer>::to_address(pointer);
}

template<typename Pointer>
inline auto pointer_address(const Pointer& pointer) noexcept
    -> decltype(pointer_address(pointer.operator->())) {
    return pointer_address(pointer.operator->());
}
#endif
}

/* ═══════════════════════════════════════════════════════════════
 * list - 双方向連結リスト
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, typename Allocator = void>
class list {
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using allocator_type = typename list_detail::effective_allocator<T, Allocator>::type;
    
private:
    /* Base node for linked list structure (no data) */
    struct NodeBase {
        NodeBase* prev;
        NodeBase* next;

        NodeBase() : prev(nullptr), next(nullptr) {}
    };

    /* Data node with actual value */
    struct Node : NodeBase {
        T data;

        Node(const T& val) : NodeBase(), data(val) {}
        Node(T&& val) : NodeBase(), data(std::move(val)) {}

        template<typename... Args>
        explicit Node(Args&&... args) : NodeBase(), data(std::forward<Args>(args)...) {}
    };

    using node_allocator_type = typename allocator_traits<allocator_type>::template rebind_alloc<Node>;
    using node_allocator_traits = allocator_traits<node_allocator_type>;
    using node_pointer = typename node_allocator_traits::pointer;

    allocator_type m_alloc;
    NodeBase m_sentinel;  /* Sentinel node (embedded, not allocated) */
    size_type m_size;

    void init() {
        m_sentinel.prev = &m_sentinel;
        m_sentinel.next = &m_sentinel;
        m_size = 0;
    }

    void ensure_count(size_type count) const {
        if (count > max_size())
            list_detail::list_length_failure();
    }

    void ensure_additional(size_type current, size_type extra) const {
        const size_type limit = max_size();
        if (current > limit || extra > limit - current)
            list_detail::list_length_failure();
    }

    void ensure_next(size_type base, size_type appended) const {
        const size_type limit = max_size();
        if (base > limit || appended >= limit - base)
            list_detail::list_length_failure();
    }

    Node* to_node(NodeBase* base) {
        return static_cast<Node*>(base);
    }

    const Node* to_node(const NodeBase* base) const {
        return static_cast<const Node*>(base);
    }

    template<typename... Args>
    Node* create_node(Args&&... args) {
        node_allocator_type alloc(m_alloc);
        node_pointer allocation = node_allocator_traits::allocate(alloc, 1);
        Node* node = list_detail::pointer_address(allocation);
        if (allocation == node_pointer() || !node) {
            if (allocation != node_pointer())
                node_allocator_traits::deallocate(alloc, allocation, 1);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            throw bad_alloc();
#else
            terminate();
#endif
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            node_allocator_traits::construct(alloc, node,
                                             std::forward<Args>(args)...);
        } catch (...) {
            node_allocator_traits::deallocate(alloc, allocation, 1);
            throw;
        }
#else
        node_allocator_traits::construct(alloc, node,
                                         std::forward<Args>(args)...);
#endif
        return node;
    }

    void destroy_node(Node* node) noexcept {
        if (!node) return;
        node_allocator_type alloc(m_alloc);
        node_allocator_traits::destroy(alloc, node);
        node_allocator_traits::deallocate(
            alloc,
            pointer_traits<node_pointer>::pointer_to(*node),
            1);
    }

    void adopt_nodes(list& source) noexcept {
        if (source.empty()) {
            init();
            return;
        }
        m_sentinel.next = source.m_sentinel.next;
        m_sentinel.prev = source.m_sentinel.prev;
        m_sentinel.next->prev = &m_sentinel;
        m_sentinel.prev->next = &m_sentinel;
        m_size = source.m_size;
        source.init();
    }

    template<typename InputIt>
    void build_range(InputIt first, InputIt last) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (; first != last; ++first) push_back(*first);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            clear();
            throw;
        }
#endif
    }

    void build_count(size_type count) {
        ensure_count(count);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (size_type i = 0; i < count; ++i) emplace_back();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            clear();
            throw;
        }
#endif
    }

    void build_count(size_type count, const T& value) {
        ensure_count(count);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (size_type i = 0; i < count; ++i) push_back(value);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            clear();
            throw;
        }
#endif
    }
    
    void insert_before(NodeBase* pos, Node* new_node) {
        new_node->prev = pos->prev;
        new_node->next = pos;
        pos->prev->next = new_node;
        pos->prev = new_node;
        ++m_size;
    }

    void unlink(NodeBase* node) {
        node->prev->next = node->next;
        node->next->prev = node->prev;
        --m_size;
    }

    bool owns_node(const NodeBase* node) const noexcept {
        if (!node) return false;
        if (node == &m_sentinel) return true;
        for (const NodeBase* current = m_sentinel.next;
             current != &m_sentinel; current = current->next) {
            if (current == node) return true;
        }
        return false;
    }

    bool owns_range(const NodeBase* first, const NodeBase* last) const noexcept {
        if (!owns_node(first) || !owns_node(last)) return false;
        const NodeBase* current = first;
        while (current != last && current != &m_sentinel)
            current = current->next;
        return current == last;
    }

    void append_relocated(list& target, NodeBase* source, true_type) {
        target.push_back(to_node(source)->data);
    }

    void append_relocated(list& target, NodeBase* source, false_type) {
        /* The replacement chain is private until the complete operation
         * publishes it.  A throwing move may therefore leave an already
         * visited source value moved-from, but it cannot expose partial links
         * or transfer ownership.  This is the basic guarantee for a
         * copy-deleted value whose move constructor can throw. */
        target.push_back(std::move(to_node(source)->data));
    }

    void build_relocated(list& source) {
        for (NodeBase* node = source.m_sentinel.next;
             node != &source.m_sentinel; node = node->next) {
            append_relocated(*this, node,
                integral_constant<bool, is_copy_constructible<T>::value>());
        }
    }

    void relocate_splice(NodeBase* pos_node, list& other,
                         NodeBase* begin_node, NodeBase* end_node) {
        size_type moved = 0;
        for (NodeBase* node = begin_node; node != end_node; node = node->next)
            ++moved;
        if (m_size > max_size() || moved > max_size() - m_size)
            list_detail::list_length_failure();
        list replacement(m_alloc);
        for (NodeBase* node = begin_node; node != end_node; node = node->next) {
            append_relocated(replacement, node,
                integral_constant<bool, is_copy_constructible<T>::value>());
        }
        if (replacement.empty()) return;
        NodeBase* prior = pos_node->prev;
        NodeBase* first = replacement.m_sentinel.next;
        NodeBase* last = replacement.m_sentinel.prev;
        prior->next = first;
        first->prev = prior;
        last->next = pos_node;
        pos_node->prev = last;
        m_size += replacement.m_size;
        replacement.init();
        NodeBase* node = begin_node;
        while (node != end_node) {
            NodeBase* next = node->next;
            other.unlink(node);
            other.destroy_node(other.to_node(node));
            node = next;
        }
    }

    template<typename Compare>
    void merge_unequal(list& other, Compare comp) {
        if (m_size > max_size() || other.m_size > max_size() - m_size)
            list_detail::list_length_failure();
        list replacement(m_alloc);
        const_iterator left = begin();
        const_iterator right = other.begin();
        while (left != end() || right != other.end()) {
            if (right == other.end() ||
                (left != end() && !comp(*right, *left))) {
                append_relocated(replacement,
                    const_cast<NodeBase*>(left.m_node),
                    integral_constant<bool, is_copy_constructible<T>::value>());
                ++left;
            } else {
                append_relocated(replacement,
                    const_cast<NodeBase*>(right.m_node),
                    integral_constant<bool, is_copy_constructible<T>::value>());
                ++right;
            }
        }
        clear();
        other.clear();
        splice(end(), replacement);
    }

    void swap_unequal(list& other) {
        if (other.m_size > max_size() || m_size > other.max_size())
            list_detail::list_length_failure();
        list left(m_alloc);
        list right(other.m_alloc);
        left.build_relocated(other);
        right.build_relocated(*this);
        clear();
        other.clear();
        splice(end(), left);
        other.splice(other.end(), right);
    }
    
public:
    /* イテレータ */
    class iterator {
        friend class list;
        NodeBase* m_node;

    public:
        using iterator_category = bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        iterator() : m_node(nullptr) {}
        explicit iterator(NodeBase* node) : m_node(node) {}

        reference operator*() const { return static_cast<Node*>(m_node)->data; }
        pointer operator->() const { return &static_cast<Node*>(m_node)->data; }

        iterator& operator++() { m_node = m_node->next; return *this; }
        iterator operator++(int) { iterator tmp = *this; ++(*this); return tmp; }

        iterator& operator--() { m_node = m_node->prev; return *this; }
        iterator operator--(int) { iterator tmp = *this; --(*this); return tmp; }

        bool operator==(const iterator& other) const { return m_node == other.m_node; }
        bool operator!=(const iterator& other) const { return m_node != other.m_node; }
    };

    class const_iterator {
        friend class list;
        const NodeBase* m_node;

    public:
        using iterator_category = bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        const_iterator() : m_node(nullptr) {}
        explicit const_iterator(const NodeBase* node) : m_node(node) {}
        const_iterator(const iterator& it) : m_node(it.m_node) {}

        reference operator*() const { return static_cast<const Node*>(m_node)->data; }
        pointer operator->() const { return &static_cast<const Node*>(m_node)->data; }

        const_iterator& operator++() { m_node = m_node->next; return *this; }
        const_iterator operator++(int) { const_iterator tmp = *this; ++(*this); return tmp; }

        const_iterator& operator--() { m_node = m_node->prev; return *this; }
        const_iterator operator--(int) { const_iterator tmp = *this; --(*this); return tmp; }

        bool operator==(const const_iterator& other) const { return m_node == other.m_node; }
        bool operator!=(const const_iterator& other) const { return m_node != other.m_node; }

        /* Equality is specified for both iterator/const_iterator operand
         * orders.  A converting constructor alone only makes const_iterator
         * on the left participate; the symmetric non-member forms keep
         * `it == cit` and `cit == it` equally valid without exposing links. */
        friend bool operator==(const iterator& left,
                              const const_iterator& right) {
            return left.m_node == right.m_node;
        }
        friend bool operator==(const const_iterator& left,
                              const iterator& right) {
            return left.m_node == right.m_node;
        }
        friend bool operator!=(const iterator& left,
                              const const_iterator& right) {
            return !(left == right);
        }
        friend bool operator!=(const const_iterator& left,
                              const iterator& right) {
            return !(left == right);
        }
    };
    
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    
    /* コンストラクタ・デストラクタ */
    list() : m_alloc(), m_size(0) { init(); }

    explicit list(const allocator_type& alloc) : m_alloc(alloc), m_size(0) { init(); }

    explicit list(size_type count) : m_alloc(), m_size(0) {
        init();
        build_count(count);
    }

    list(size_type count, const T& value) : m_alloc(), m_size(0) {
        init();
        build_count(count, value);
    }

    list(size_type count, const T& value, const allocator_type& alloc)
        : m_alloc(alloc), m_size(0) {
        init();
        build_count(count, value);
    }

    list(size_type count, const allocator_type& alloc)
        : m_alloc(alloc), m_size(0) {
        init();
        build_count(count);
    }

    template<typename InputIt,
             typename = typename enable_if<
                 !is_integral<remove_cv_t<InputIt>>::value>::type>
    list(InputIt first, InputIt last) : m_alloc(), m_size(0) {
        init();
        build_range(first, last);
    }

    template<typename InputIt,
             typename = typename enable_if<
                 !is_integral<remove_cv_t<InputIt>>::value>::type>
    list(InputIt first, InputIt last, const allocator_type& alloc)
        : m_alloc(alloc), m_size(0) {
        init();
        build_range(first, last);
    }

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, T>
    list(from_range_t, R&& range)
        : m_alloc(), m_size(0) {
        init();
        build_range(ranges::begin(range), ranges::end(range));
    }

    template<typename R>
        requires detail::container_compatible_range<R, T>
    list(from_range_t, R&& range, const allocator_type& alloc)
        : m_alloc(alloc), m_size(0) {
        init();
        build_range(ranges::begin(range), ranges::end(range));
    }
#endif

    list(std::initializer_list<T> ilist) : m_alloc(), m_size(0) {
        init();
        build_range(ilist.begin(), ilist.end());
    }

    list(std::initializer_list<T> ilist, const allocator_type& alloc)
        : m_alloc(alloc), m_size(0) {
        init();
        build_range(ilist.begin(), ilist.end());
    }

    list(const list& other)
        : m_alloc(allocator_traits<allocator_type>::select_on_container_copy_construction(other.m_alloc)),
          m_size(0) {
        init();
        if (other.m_size > max_size())
            list_detail::list_length_failure();
        build_range(other.begin(), other.end());
    }

    list(const list& other, const allocator_type& alloc)
        : m_alloc(alloc), m_size(0) {
        init();
        if (other.m_size > max_size())
            list_detail::list_length_failure();
        build_range(other.begin(), other.end());
    }

    list(list&& other) noexcept
        : m_alloc(std::move(other.m_alloc)), m_size(0) {
        init();
        if (!other.empty()) {
            /* Transfer nodes from other */
            m_sentinel.next = other.m_sentinel.next;
            m_sentinel.prev = other.m_sentinel.prev;
            m_sentinel.next->prev = &m_sentinel;
            m_sentinel.prev->next = &m_sentinel;
            m_size = other.m_size;
            other.init();
        }
    }

    ~list() {
        clear();
        /* m_sentinel is embedded, no delete needed */
    }
    
    list& operator=(const list& other) {
        if (this != &other) {
            if (allocator_traits<allocator_type>::propagate_on_container_copy_assignment::value) {
                list replacement(other, other.m_alloc);
                clear();
                m_alloc = other.m_alloc;
                adopt_nodes(replacement);
            } else {
                list replacement(other, m_alloc);
                clear();
                adopt_nodes(replacement);
            }
        }
        return *this;
    }

    list& operator=(list&& other) {
        if (this != &other) {
            const bool propagate = allocator_traits<allocator_type>::propagate_on_container_move_assignment::value;
            if (propagate || m_alloc == other.m_alloc) {
                clear();
                if (propagate) m_alloc = std::move(other.m_alloc);
                if (!other.empty()) {
                    m_sentinel.next = other.m_sentinel.next;
                    m_sentinel.prev = other.m_sentinel.prev;
                    m_sentinel.next->prev = &m_sentinel;
                    m_sentinel.prev->next = &m_sentinel;
                    m_size = other.m_size;
                    other.init();
                }
        } else {
            /* An unequal allocator cannot adopt source nodes.  Build a
                 * destination-owned replacement through the same copy or
                 * guarded move relocation used by splice/merge/swap.  The
                 * copy-constructor-only path rejected copy-deleted noexcept
             * movable elements before they reached this operation. */
            if (other.m_size > max_size())
                list_detail::list_length_failure();
            list replacement(m_alloc);
                replacement.build_relocated(other);
                clear();
                adopt_nodes(replacement);
                other.clear();
            }
        }
        return *this;
    }
    
    list& operator=(std::initializer_list<T> ilist) {
        list replacement(ilist, m_alloc);
        clear();
        adopt_nodes(replacement);
        return *this;
    }

    allocator_type get_allocator() const noexcept { return m_alloc; }
    
    void assign(size_type count, const T& value) {
        list replacement(count, value, m_alloc);
        clear();
        adopt_nodes(replacement);
    }
    
    template<typename InputIt>
    void assign(InputIt first, InputIt last) {
        list replacement(first, last, m_alloc);
        clear();
        adopt_nodes(replacement);
    }

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, T>
    void assign_range(R&& range) {
        list replacement(m_alloc);
        replacement.build_range(ranges::begin(range), ranges::end(range));
        clear();
        adopt_nodes(replacement);
    }
#endif
    
    void assign(std::initializer_list<T> ilist) {
        list replacement(ilist, m_alloc);
        clear();
        adopt_nodes(replacement);
    }
    
    /* 要素アクセス */
    reference front() noexcept { return static_cast<Node*>(m_sentinel.next)->data; }
    const_reference front() const noexcept { return static_cast<const Node*>(m_sentinel.next)->data; }

    reference back() noexcept { return static_cast<Node*>(m_sentinel.prev)->data; }
    const_reference back() const noexcept { return static_cast<const Node*>(m_sentinel.prev)->data; }

    /* イテレータ */
    iterator begin() noexcept { return iterator(m_sentinel.next); }
    const_iterator begin() const noexcept { return const_iterator(m_sentinel.next); }
    const_iterator cbegin() const noexcept { return const_iterator(m_sentinel.next); }

    iterator end() noexcept { return iterator(&m_sentinel); }
    const_iterator end() const noexcept { return const_iterator(&m_sentinel); }
    const_iterator cend() const noexcept { return const_iterator(&m_sentinel); }
    
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
    
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }
    
    /* 容量 */
    bool empty() const noexcept { return m_size == 0; }
    size_type size() const noexcept { return m_size; }
    size_type max_size() const noexcept {
        return node_allocator_traits::max_size(node_allocator_type(m_alloc));
    }
    
    /* 変更 */
    void clear() noexcept {
        NodeBase* current = m_sentinel.next;
        while (current != &m_sentinel) {
            NodeBase* next = current->next;
            destroy_node(static_cast<Node*>(current));
            current = next;
        }
        m_sentinel.next = &m_sentinel;
        m_sentinel.prev = &m_sentinel;
        m_size = 0;
    }
    
    iterator insert(const_iterator pos, const T& value) {
        NodeBase* pos_node = const_cast<NodeBase*>(pos.m_node);
        if (!owns_node(pos_node)) return end();
        ensure_next(m_size, 0);
        Node* new_node = create_node(value);
        insert_before(pos_node, new_node);
        return iterator(new_node);
    }

    iterator insert(const_iterator pos, T&& value) {
        NodeBase* pos_node = const_cast<NodeBase*>(pos.m_node);
        if (!owns_node(pos_node)) return end();
        ensure_next(m_size, 0);
        Node* new_node = create_node(std::move(value));
        insert_before(pos_node, new_node);
        return iterator(new_node);
    }

    iterator insert(const_iterator pos, size_type count, const T& value) {
        NodeBase* pos_node = const_cast<NodeBase*>(pos.m_node);
        if (!owns_node(pos_node)) return end();
        if (count == 0) return iterator(pos_node);
        ensure_additional(m_size, count);

        /* Build all nodes off-list so a throwing copy or allocation leaves
         * the destination sequence untouched.  The completed chain is then
         * transferred in one link update. */
        list replacement(m_alloc);
        replacement.build_count(count, value);
        iterator first = replacement.begin();
        splice(pos, replacement);
        return first;
    }

    template<typename InputIt,
             typename = typename enable_if<
                 !is_integral<remove_cv_t<InputIt>>::value>::type>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        NodeBase* pos_node = const_cast<NodeBase*>(pos.m_node);
        if (!owns_node(pos_node)) return end();
        if (m_size >= max_size() && first == last)
            return iterator(pos_node);
        list replacement(m_alloc);
        replacement.build_range(first, last);
        if (replacement.empty()) return iterator(pos_node);
        ensure_additional(m_size, replacement.m_size);
        iterator result = replacement.begin();
        splice(pos, replacement);
        return result;
    }

    iterator insert(const_iterator pos, std::initializer_list<T> values) {
        return insert(pos, values.begin(), values.end());
    }

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, T>
    iterator insert_range(const_iterator pos, R&& range) {
        NodeBase* pos_node = const_cast<NodeBase*>(pos.m_node);
        if (!owns_node(pos_node)) return end();
        list replacement(m_alloc);
        replacement.build_range(ranges::begin(range), ranges::end(range));
        if (replacement.empty()) return iterator(pos_node);
        ensure_additional(m_size, replacement.m_size);
        iterator first = replacement.begin();
        splice(pos, replacement);
        return first;
    }

    template<typename R>
        requires detail::container_compatible_range<R, T>
    void prepend_range(R&& range) {
        insert_range(begin(), std::forward<R>(range));
    }

    template<typename R>
        requires detail::container_compatible_range<R, T>
    void append_range(R&& range) {
        insert_range(end(), std::forward<R>(range));
    }
#endif

    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        NodeBase* pos_node = const_cast<NodeBase*>(pos.m_node);
        if (!owns_node(pos_node)) return end();
        ensure_next(m_size, 0);
        Node* new_node = create_node(std::forward<Args>(args)...);
        insert_before(pos_node, new_node);
        return iterator(new_node);
    }

    iterator erase(const_iterator pos) {
        NodeBase* node = const_cast<NodeBase*>(pos.m_node);
        if (!owns_node(node) || node == &m_sentinel) return end();
        NodeBase* next = node->next;
        unlink(node);
        destroy_node(static_cast<Node*>(node));
        return iterator(next);
    }

    iterator erase(const_iterator first, const_iterator last) {
        if (!owns_range(first.m_node, last.m_node)) return end();
        while (first != last) {
            first = erase(first);
        }
        return iterator(const_cast<NodeBase*>(last.m_node));
    }
    
    void push_back(const T& value) {
        insert(end(), value);
    }
    
    void push_back(T&& value) {
        insert(end(), std::move(value));
    }
    
    template<typename... Args>
    reference emplace_back(Args&&... args) {
        emplace(end(), std::forward<Args>(args)...);
        return back();
    }
    
    void pop_back() noexcept {
        erase(--end());
    }
    
    void push_front(const T& value) {
        insert(begin(), value);
    }
    
    void push_front(T&& value) {
        insert(begin(), std::move(value));
    }
    
    template<typename... Args>
    reference emplace_front(Args&&... args) {
        emplace(begin(), std::forward<Args>(args)...);
        return front();
    }
    
    void pop_front() noexcept {
        erase(begin());
    }
    
    void resize(size_type count) {
        if (count > max_size())
            list_detail::list_length_failure();

        while (m_size > count) {
            pop_back();
        }
        if (m_size < count) {
            /* C++11 resize(count) is default insertion, not a copy of a
             * temporary T.  Build the suffix privately so a throwing default
             * constructor leaves the original list untouched. */
            list replacement(m_alloc);
            replacement.build_count(count - m_size);
            splice(end(), replacement);
        }
    }
    
    void resize(size_type count, const T& value) {
        if (count > max_size())
            list_detail::list_length_failure();

        while (m_size > count) {
            pop_back();
        }
        if (m_size < count) {
            /* Build the value-initialized suffix privately.  A throwing copy
             * must not publish a partial extension of the list. */
            list replacement(m_alloc);
            replacement.build_count(count - m_size, value);
            splice(end(), replacement);
        }
    }
    
    void swap(list& other) noexcept(
        allocator_traits<allocator_type>::propagate_on_container_swap::value ||
        allocator_traits<allocator_type>::is_always_equal::value) {
        if (this == &other) return;
        if (!allocator_traits<allocator_type>::propagate_on_container_swap::value &&
            m_alloc != other.m_alloc) {
            swap_unequal(other);
            return;
        }
        if (allocator_traits<allocator_type>::propagate_on_container_swap::value) {
            allocator_type tmp = std::move(m_alloc);
            m_alloc = std::move(other.m_alloc);
            other.m_alloc = std::move(tmp);
        }
        if (empty() && other.empty()) return;

        if (empty()) {
            /* Move other's content to this */
            m_sentinel.next = other.m_sentinel.next;
            m_sentinel.prev = other.m_sentinel.prev;
            m_sentinel.next->prev = &m_sentinel;
            m_sentinel.prev->next = &m_sentinel;
            other.m_sentinel.next = &other.m_sentinel;
            other.m_sentinel.prev = &other.m_sentinel;
        } else if (other.empty()) {
            /* Move this's content to other */
            other.m_sentinel.next = m_sentinel.next;
            other.m_sentinel.prev = m_sentinel.prev;
            other.m_sentinel.next->prev = &other.m_sentinel;
            other.m_sentinel.prev->next = &other.m_sentinel;
            m_sentinel.next = &m_sentinel;
            m_sentinel.prev = &m_sentinel;
        } else {
            /* Swap both */
            list_detail::list_exchange(m_sentinel.next, other.m_sentinel.next);
            list_detail::list_exchange(m_sentinel.prev, other.m_sentinel.prev);
            m_sentinel.next->prev = &m_sentinel;
            m_sentinel.prev->next = &m_sentinel;
            other.m_sentinel.next->prev = &other.m_sentinel;
            other.m_sentinel.prev->next = &other.m_sentinel;
        }
        list_detail::list_exchange(m_size, other.m_size);
    }
    
    /* 操作 */
    void merge(list& other) {
        merge(other, std::less<T>());
    }
    
    template<typename Compare>
    void merge(list& other, Compare comp) {
        if (this == &other) return;
        if (m_alloc != other.m_alloc) {
            merge_unequal(other, comp);
            return;
        }

        /* A comparison may throw after several elements have already been
         * selected.  Relinking nodes as we compare would publish a partial
         * merge and violate list's no-effects guarantee for comparator
         * failure.  Stage the complete pointer order first; only the final
         * non-throwing relink changes either public list. */
        if (other.m_size > max_size() - m_size) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            throw length_error("list::merge size exceeds max_size");
#else
            terminate();
#endif
        }
        const size_type total = m_size + other.m_size;
        if (total == 0) return;

        allocator<NodeBase*> scratch_allocator;
        NodeBase** order = scratch_allocator.allocate(total);
        if (!order) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            throw bad_alloc();
#else
            terminate();
#endif
        }

        size_type count = 0;
        NodeBase* left = m_sentinel.next;
        NodeBase* right = other.m_sentinel.next;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            while (left != &m_sentinel && right != &other.m_sentinel) {
                NodeBase* selected;
                if (comp(to_node(right)->data, to_node(left)->data)) {
                    selected = right;
                    right = right->next;
                } else {
                    selected = left;
                    left = left->next;
                }
                order[count++] = selected;
            }
            while (left != &m_sentinel) {
                order[count++] = left;
                left = left->next;
            }
            while (right != &other.m_sentinel) {
                order[count++] = right;
                right = right->next;
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            scratch_allocator.deallocate(order, total);
            throw;
        }
#endif

        NodeBase* previous = &m_sentinel;
        for (size_type index = 0; index < total; ++index) {
            NodeBase* node = order[index];
            previous->next = node;
            node->prev = previous;
            previous = node;
        }
        previous->next = &m_sentinel;
        m_sentinel.prev = previous;
        m_sentinel.next = order[0];
        m_size = total;
        other.init();
        scratch_allocator.deallocate(order, total);
    }
    
    void splice(const_iterator pos, list& other) {
        /* A list iterator carries only a raw node link.  Validate both the
         * destination position and source ownership before following links;
         * default/foreign positions must be a bounded no-op rather than an
         * accidental write through another list's sentinel. */
        NodeBase* pos_node = const_cast<NodeBase*>(pos.m_node);
        if (!owns_node(pos_node)) return;

        /* Splicing a list into itself is a no-op.  Detaching the sentinel
         * first would otherwise orphan every node and leave both size
         * counters inconsistent. */
        if (this == &other || other.empty()) return;

        if (m_size > max_size() ||
            other.m_size > max_size() - m_size)
            list_detail::list_length_failure();

        if (m_alloc != other.m_alloc) {
            relocate_splice(const_cast<NodeBase*>(pos.m_node), other,
                            other.m_sentinel.next, &other.m_sentinel);
            return;
        }

        const size_type moved = other.m_size;

        NodeBase* first = other.m_sentinel.next;
        NodeBase* last = other.m_sentinel.prev;

        /* otherから切り離し */
        other.m_sentinel.next = &other.m_sentinel;
        other.m_sentinel.prev = &other.m_sentinel;

        /* thisに挿入 */
        first->prev = pos_node->prev;
        last->next = pos_node;
        pos_node->prev->next = first;
        pos_node->prev = last;

        m_size += moved;
        other.m_size = 0;
    }

    void splice(const_iterator pos, list& other, const_iterator it) {
        NodeBase* pos_node = const_cast<NodeBase*>(pos.m_node);
        NodeBase* node = const_cast<NodeBase*>(it.m_node);
        if (!owns_node(pos_node) || !other.owns_node(node) ||
            node == &other.m_sentinel) return;

        NodeBase* next_node = node->next;
        if (this == &other && (pos_node == node || pos_node == next_node))
            return;

        if (this != &other && m_size >= max_size())
            list_detail::list_length_failure();

        if (this != &other && m_alloc != other.m_alloc) {
            relocate_splice(const_cast<NodeBase*>(pos.m_node), other,
                            node, next_node);
            return;
        }

        /* otherから切り離し */
        node->prev->next = node->next;
        node->next->prev = node->prev;
        --other.m_size;

        /* thisに挿入 */
        node->prev = pos_node->prev;
        node->next = pos_node;
        pos_node->prev->next = node;
        pos_node->prev = node;
        ++m_size;
    }

    void splice(const_iterator pos, list& other,
                const_iterator first, const_iterator last) {
        NodeBase* pos_node = const_cast<NodeBase*>(pos.m_node);
        NodeBase* first_node = const_cast<NodeBase*>(first.m_node);
        NodeBase* last_node = const_cast<NodeBase*>(last.m_node);
        if (!owns_node(pos_node) || !other.owns_range(first_node, last_node))
            return;
        if (first_node == last_node) return;
        if (this == &other) {
            /* A position inside the moved range would make the link update
             * self-overlapping.  The standard leaves that case undefined;
             * keeping it a no-op is safer for this bounded owner. */
            for (NodeBase* probe = first_node; probe != last_node;
                 probe = probe->next) {
                if (probe == pos_node) return;
            }
        }

        size_type moved = 0;
        for (NodeBase* node = first_node; node != last_node;
             node = node->next)
            ++moved;
        if (this != &other &&
            (m_size > max_size() || moved > max_size() - m_size))
            list_detail::list_length_failure();

        if (this != &other && m_alloc != other.m_alloc) {
            relocate_splice(pos_node, other, first_node, last_node);
            return;
        }

        NodeBase* before = first_node->prev;
        NodeBase* tail = last_node->prev;

        before->next = last_node;
        last_node->prev = before;

        NodeBase* prior = pos_node->prev;
        prior->next = first_node;
        first_node->prev = prior;
        tail->next = pos_node;
        pos_node->prev = tail;

        if (this != &other) {
            other.m_size -= moved;
            m_size += moved;
        }
    }

    list(list&& other, const allocator_type& alloc)
        : m_alloc(alloc), m_size(0) {
        init();
        if (m_alloc == other.m_alloc) {
            if (!other.empty()) {
                m_sentinel.next = other.m_sentinel.next;
                m_sentinel.prev = other.m_sentinel.prev;
                m_sentinel.next->prev = &m_sentinel;
                m_sentinel.prev->next = &m_sentinel;
                m_size = other.m_size;
                other.init();
            }
        } else {
            if (other.m_size > max_size())
                list_detail::list_length_failure();
            build_relocated(other);
            other.clear();
        }
    }
    
    size_type remove(const T& value) {
        return remove_if([&value](const T& v) { return v == value; });
    }
    
    template<typename UnaryPredicate>
    size_type remove_if(UnaryPredicate p) {
        if (m_size == 0) return 0;
        allocator<NodeBase*> scratch_allocator;
        NodeBase** removed_nodes = scratch_allocator.allocate(m_size);
        if (!removed_nodes) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            throw bad_alloc();
#else
            terminate();
#endif
        }
        size_type removed = 0;
        NodeBase* current = m_sentinel.next;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            while (current != &m_sentinel) {
                NodeBase* next = current->next;
                if (p(to_node(current)->data)) removed_nodes[removed++] = current;
                current = next;
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            scratch_allocator.deallocate(removed_nodes, m_size);
            throw;
        }
#endif
        for (size_type index = 0; index < removed; ++index) {
            NodeBase* node = removed_nodes[index];
            node->prev->next = node->next;
            node->next->prev = node->prev;
            --m_size;
        }
        for (size_type index = 0; index < removed; ++index) {
            NodeBase* node = removed_nodes[index];
            destroy_node(to_node(node));
        }
        scratch_allocator.deallocate(removed_nodes, m_size + removed);
        return removed;
    }
    
    void reverse() noexcept {
        if (m_size < 2) return;

        NodeBase* current = &m_sentinel;
        do {
            list_detail::list_exchange(current->prev, current->next);
            current = current->prev;
        } while (current != &m_sentinel);
    }
    
    size_type unique() {
        return unique(std::equal_to<T>());
    }
    
    template<typename BinaryPredicate>
    size_type unique(BinaryPredicate p) {
        if (m_size < 2) return 0;

        allocator<NodeBase*> scratch_allocator;
        NodeBase** removed_nodes = scratch_allocator.allocate(m_size);
        if (!removed_nodes) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            throw bad_alloc();
#else
            terminate();
#endif
        }
        size_type removed = 0;
        NodeBase* current = m_sentinel.next;
        NodeBase* next = current->next;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            while (next != &m_sentinel) {
                if (p(to_node(current)->data, to_node(next)->data)) {
                    removed_nodes[removed++] = next;
                    next = next->next;
                } else {
                    current = next;
                    next = next->next;
                }
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            scratch_allocator.deallocate(removed_nodes, m_size);
            throw;
        }
#endif
        for (size_type index = 0; index < removed; ++index) {
            NodeBase* node = removed_nodes[index];
            node->prev->next = node->next;
            node->next->prev = node->prev;
            --m_size;
        }
        for (size_type index = 0; index < removed; ++index) {
            NodeBase* node = removed_nodes[index];
            destroy_node(to_node(node));
        }
        scratch_allocator.deallocate(removed_nodes, m_size + removed);
        return removed;
    }
    
    void sort() {
        sort(std::less<T>());
    }
    
    template<typename Compare>
    void sort(Compare comp) {
        if (m_size < 2) return;

        /* Compare a detached pointer sequence first.  The former in-place
         * bottom-up merge moved nodes between temporary lists before each
         * comparison; if the comparator threw, the public list could lose its
         * original order across those temporaries.  Sorting raw node links
         * keeps every ownership/link invariant untouched until all
         * potentially-throwing work has completed. */
        allocator<NodeBase*> scratch_allocator;
        NodeBase** order = scratch_allocator.allocate(m_size);
        if (!order) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            throw bad_alloc();
#else
            terminate();
#endif
        }
        NodeBase* cursor = m_sentinel.next;
        for (size_type index = 0; index < m_size; ++index) {
            order[index] = cursor;
            cursor = cursor->next;
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            stable_sort(order, order + m_size,
                        [&comp](NodeBase* left, NodeBase* right) {
                            return comp(static_cast<Node*>(left)->data,
                                        static_cast<Node*>(right)->data);
                        });
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            scratch_allocator.deallocate(order, m_size);
            throw;
        }
#endif

        NodeBase* previous = &m_sentinel;
        for (size_type index = 0; index < m_size; ++index) {
            NodeBase* node = order[index];
            previous->next = node;
            node->prev = previous;
            previous = node;
        }
        previous->next = &m_sentinel;
        m_sentinel.prev = previous;
        m_sentinel.next = order[0];
        scratch_allocator.deallocate(order, m_size);
    }
};

/* 非メンバ関数 */
template<typename T, typename Allocator>
bool operator==(const list<T, Allocator>& lhs, const list<T, Allocator>& rhs) {
    if (lhs.size() != rhs.size()) return false;
    auto it1 = lhs.begin();
    auto it2 = rhs.begin();
    while (it1 != lhs.end()) {
        if (*it1 != *it2) return false;
        ++it1;
        ++it2;
    }
    return true;
}

template<typename T, typename Allocator>
bool operator!=(const list<T, Allocator>& lhs, const list<T, Allocator>& rhs) {
    return !(lhs == rhs);
}

template<typename T, typename Allocator>
bool operator<(const list<T, Allocator>& lhs, const list<T, Allocator>& rhs) {
    return lexicographical_compare(lhs.begin(), lhs.end(),
                                   rhs.begin(), rhs.end());
}

template<typename T, typename Allocator>
bool operator<=(const list<T, Allocator>& lhs, const list<T, Allocator>& rhs) {
    return !(rhs < lhs);
}

template<typename T, typename Allocator>
bool operator>(const list<T, Allocator>& lhs, const list<T, Allocator>& rhs) {
    return rhs < lhs;
}

template<typename T, typename Allocator>
bool operator>=(const list<T, Allocator>& lhs, const list<T, Allocator>& rhs) {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
template<typename T, typename Allocator>
constexpr auto operator<=>(const list<T, Allocator>& lhs,
                           const list<T, Allocator>& rhs)
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

template<typename T, typename Allocator>
void swap(list<T, Allocator>& lhs, list<T, Allocator>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

#if __cplusplus >= 202002L
template<typename T, typename Allocator>
typename list<T, Allocator>::size_type
erase(list<T, Allocator>& value, const T& element) {
    return value.remove(element);
}

template<typename T, typename Allocator, typename Pred>
typename list<T, Allocator>::size_type
erase_if(list<T, Allocator>& value, Pred predicate) {
    return value.remove_if(predicate);
}
#endif

#if __cplusplus > 202002L
template<ranges::input_range R>
list(from_range_t, R&&)
    -> list<ranges::range_value_t<R>>;

template<ranges::input_range R, class Allocator>
list(from_range_t, R&&, Allocator)
    -> list<ranges::range_value_t<R>, Allocator>;
#endif

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_LIST_H */
