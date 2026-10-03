/*
 * RinOS C++ <set> ✿
 * 完全な順序付き集合実装
 */

#ifndef RINCXX_SET_H
#define RINCXX_SET_H

#include "rincxx.h"
#include "functional.h"
#include "iterator.h"
#include "utility.h"
#include "initializer_list.h"
#include "exception.h"
#include "memory.h"
#include "cstdint.h"
#include "vector.h"
#if __cplusplus >= 202002L
#include "compare.h"
#endif

/* Keep heterogeneous extract on the same explicit C++26 preview gate as the
 * map family.  The final dialect may enable it without the opt-in macro. */
#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_ASSOCIATIVE_INSERTION)
#define RIN_SET_HAS_HETERO_INSERTION 1
#endif
#if __cplusplus > 202002L
#include "ranges.h"
#endif

#ifdef __cplusplus

namespace std {

template<typename Key, typename Compare, typename Allocator>
class multiset;

namespace set_detail {

/* Avoid an unqualified/qualified std::swap lookup collision when the Rin
 * compatibility swap and the host standard-library swap are both visible.
 * Associative-container storage only needs value-semantic three-move exchange
 * here; callers already publish the surrounding operation's exception policy. */
template<typename T>
inline void set_exchange(T& left, T& right) noexcept(
    noexcept(T(std::move(left))) &&
    noexcept(left = std::move(right)) &&
    noexcept(right = std::move(left))) {
    if (&left == &right) return;
    T temporary(std::move(left));
    left = std::move(right);
    right = std::move(temporary);
}

/* The historical Rin declarations used `void` as the default allocator.
 * Keep that spelling source-compatible while giving the container a real
 * allocator owner internally (and exposing the standard allocator_type). */
template<typename T, typename Allocator, typename = void>
struct effective_allocator {
    /* Preserve the legacy compatibility spelling for an empty third
     * template argument while rejecting it as a real allocator owner. */
    using type = allocator<T>;
};

template<typename T, typename Allocator>
struct effective_allocator<T, Allocator, void_t<
    typename Allocator::value_type,
    decltype(declval<Allocator&>().allocate(size_t{}))>> {
    using type = Allocator;
};

template<typename T>
struct effective_allocator<T, void, void> {
    using type = allocator<T>;
};

template<typename Compare, typename = void>
struct transparent_compare {};

template<typename Compare>
struct transparent_compare<Compare, void_t<typename Compare::is_transparent>> {
    using type = typename Compare::is_transparent;
};

[[noreturn]] inline void allocation_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_alloc();
#else
    __builtin_trap();
#endif
}

[[noreturn]] inline void length_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw length_error("set exceeds max_size");
#else
    __builtin_trap();
#endif
}

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
/* C++11--17 has no std::to_address, but allocator fancy pointers may still
 * provide the standard pointer_traits customization point.  Prefer it before
 * the legacy operator-> fallback so an address-only pointer is accepted. */
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

} /* namespace set_detail */

/* ═══════════════════════════════════════════════════════════════
 * set - 順序付き一意キーコンテナ（赤黒木実装）
 * ═══════════════════════════════════════════════════════════════*/

template<typename Key, typename Compare = less<Key>, typename Allocator = void>
class set {
public:
    using key_type = Key;
    using value_type = Key;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using key_compare = Compare;
    using allocator_type = typename set_detail::effective_allocator<
        value_type, Allocator>::type;
    using value_compare = Compare;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    
private:
    enum class Color { Red, Black };
    
    struct Node {
        value_type data;
        Node* parent;
        Node* left;
        Node* right;
        Color color;
        
        Node(const value_type& val, Node* p = nullptr)
            : data(val), parent(p), left(nullptr), right(nullptr), color(Color::Red) {}

        /* Preserve the rvalue insertion contract for move-only keys.  The
         * previous set::insert(value_type&&) routed through the const&
         * overload, making a move-only key appear non-insertable and
         * needlessly copying movable keys. */
        Node(value_type&& val, Node* p = nullptr)
            : data(std::move(val)), parent(p), left(nullptr), right(nullptr),
              color(Color::Red) {}
        
        Node* minimum() {
            Node* node = this;
            while (node->left) node = node->left;
            return node;
        }
        
        Node* maximum() {
            Node* node = this;
            while (node->right) node = node->right;
            return node;
        }
        
        Node* successor() {
            if (right) return right->minimum();
            Node* node = this;
            Node* p = parent;
            while (p && node == p->right) {
                node = p;
                p = p->parent;
            }
            return p;
        }
        
        Node* predecessor() {
            if (left) return left->maximum();
            Node* node = this;
            Node* p = parent;
            while (p && node == p->left) {
                node = p;
                p = p->parent;
            }
            return p;
        }
    };

    using node_allocator_type = typename allocator_traits<allocator_type>::template
        rebind_alloc<Node>;
    using node_allocator_traits = allocator_traits<node_allocator_type>;
    using node_pointer = typename node_allocator_traits::pointer;
    node_allocator_type m_alloc;

    Node* allocate_node(const value_type& value, Node* parent) {
        node_pointer allocation = node_allocator_traits::allocate(m_alloc, 1);
        Node* node = set_detail::pointer_address(allocation);
        if (allocation == node_pointer() || !node) {
            if (allocation != node_pointer())
                node_allocator_traits::deallocate(m_alloc, allocation, 1);
            set_detail::allocation_failure();
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            node_allocator_traits::construct(m_alloc, node, value, parent);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            node_allocator_traits::deallocate(m_alloc, allocation, 1);
            throw;
        }
#endif
        return node;
    }

    Node* allocate_node(value_type&& value, Node* parent) {
        node_pointer allocation = node_allocator_traits::allocate(m_alloc, 1);
        Node* node = set_detail::pointer_address(allocation);
        if (allocation == node_pointer() || !node) {
            if (allocation != node_pointer())
                node_allocator_traits::deallocate(m_alloc, allocation, 1);
            set_detail::allocation_failure();
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            node_allocator_traits::construct(m_alloc, node, std::move(value), parent);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            node_allocator_traits::deallocate(m_alloc, allocation, 1);
            throw;
        }
#endif
        return node;
    }

    void destroy_node(Node* node) noexcept {
        if (!node) return;
        node_allocator_traits::destroy(m_alloc, node);
        node_allocator_traits::deallocate(
            m_alloc,
            pointer_traits<node_pointer>::pointer_to(*node),
            1);
    }

    void relocate_values(set& source, set& target, true_type) {
        for (auto it = source.begin(); it != source.end(); ++it)
            target.insert(*it);
    }

    void relocate_values(set& source, set& target, false_type) {
        for (auto it = source.begin(); it != source.end(); ++it)
            target.insert(std::move(const_cast<value_type&>(*it)));
    }

    void swap_unequal(set& other) {
        if (other.m_size > max_size() || m_size > other.max_size())
            set_detail::length_failure();
        set left(other.m_comp, m_alloc);
        set right(m_comp, other.m_alloc);
        relocate_values(other, left, is_copy_constructible<value_type>());
        relocate_values(*this, right, is_copy_constructible<value_type>());

        /* The allocator stays with each container; only the comparator and
         * completed replacement roots cross the boundary.  Candidate
         * construction above is the rollback point for allocation/key
         * exceptions. */
        set_detail::set_exchange(m_comp, left.m_comp);
        set_detail::set_exchange(other.m_comp, right.m_comp);
        destroy_tree(m_root);
        other.destroy_tree(other.m_root);
        m_root = left.m_root;
        m_size = left.m_size;
        left.m_root = nullptr;
        left.m_size = 0;
        other.m_root = right.m_root;
        other.m_size = right.m_size;
        right.m_root = nullptr;
        right.m_size = 0;
    }

public:
    /* C++17 node-handle surface.  The handle owns a detached value rather
     * than a tree node so it can move between sets with different
     * comparators while preserving move-only keys. */
    class node_type {
        value_type* m_value;
        allocator_type m_alloc;
        using value_allocator_traits = allocator_traits<allocator_type>;

        explicit node_type(value_type* value, const allocator_type& alloc)
            : m_value(value), m_alloc(alloc) {}
        friend class set;

        void reset() noexcept {
            if (!m_value) return;
            value_allocator_traits::destroy(m_alloc, m_value);
            value_allocator_traits::deallocate(
                m_alloc,
                pointer_traits<typename value_allocator_traits::pointer>::pointer_to(
                    *m_value),
                1);
            m_value = nullptr;
        }

    public:
        node_type() : m_value(nullptr), m_alloc() {}
        node_type(node_type&& other) noexcept
            : m_value(other.m_value), m_alloc(std::move(other.m_alloc)) {
            other.m_value = nullptr;
        }
        node_type& operator=(node_type&& other) noexcept {
            if (this != &other) {
                reset();
                m_value = other.m_value;
                m_alloc = std::move(other.m_alloc);
                other.m_value = nullptr;
            }
            return *this;
        }
        node_type(const node_type&) = delete;
        node_type& operator=(const node_type&) = delete;
        ~node_type() { reset(); }

        bool empty() const noexcept { return m_value == nullptr; }
        allocator_type get_allocator() const noexcept { return m_alloc; }
        explicit operator bool() const noexcept { return !empty(); }
        const key_type& key() const { return *m_value; }
        value_type& value() const { return *m_value; }
    };

private:
    
    Node* m_root;
    size_type m_size;
    Compare m_comp;
    
    /* 回転操作 */
    void rotate_left(Node* x) {
        Node* y = x->right;
        x->right = y->left;
        if (y->left) y->left->parent = x;
        y->parent = x->parent;
        if (!x->parent) m_root = y;
        else if (x == x->parent->left) x->parent->left = y;
        else x->parent->right = y;
        y->left = x;
        x->parent = y;
    }
    
    void rotate_right(Node* x) {
        Node* y = x->left;
        x->left = y->right;
        if (y->right) y->right->parent = x;
        y->parent = x->parent;
        if (!x->parent) m_root = y;
        else if (x == x->parent->right) x->parent->right = y;
        else x->parent->left = y;
        y->right = x;
        x->parent = y;
    }
    
    /* 挿入後の修正 */
    void insert_fixup(Node* z) {
        while (z->parent && z->parent->color == Color::Red) {
            if (z->parent == z->parent->parent->left) {
                Node* y = z->parent->parent->right;
                if (y && y->color == Color::Red) {
                    z->parent->color = Color::Black;
                    y->color = Color::Black;
                    z->parent->parent->color = Color::Red;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->right) {
                        z = z->parent;
                        rotate_left(z);
                    }
                    z->parent->color = Color::Black;
                    z->parent->parent->color = Color::Red;
                    rotate_right(z->parent->parent);
                }
            } else {
                Node* y = z->parent->parent->left;
                if (y && y->color == Color::Red) {
                    z->parent->color = Color::Black;
                    y->color = Color::Black;
                    z->parent->parent->color = Color::Red;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->left) {
                        z = z->parent;
                        rotate_right(z);
                    }
                    z->parent->color = Color::Black;
                    z->parent->parent->color = Color::Red;
                    rotate_left(z->parent->parent);
                }
            }
        }
        m_root->color = Color::Black;
    }
    
    /* 移植（削除用） */
    void transplant(Node* u, Node* v) {
        if (!u->parent) m_root = v;
        else if (u == u->parent->left) u->parent->left = v;
        else u->parent->right = v;
        if (v) v->parent = u->parent;
    }
    
    /* 削除後の修正 */
    void erase_fixup(Node* x, Node* x_parent) {
        while (x != m_root && (!x || x->color == Color::Black)) {
            if (x == x_parent->left) {
                Node* w = x_parent->right;
                if (w && w->color == Color::Red) {
                    w->color = Color::Black;
                    x_parent->color = Color::Red;
                    rotate_left(x_parent);
                    w = x_parent->right;
                }
                if (!w) {
                    /* A missing sibling represents two black children. */
                    x = x_parent;
                    x_parent = x->parent;
                    continue;
                }
                if ((!w->left || w->left->color == Color::Black) &&
                    (!w->right || w->right->color == Color::Black)) {
                    if (w) w->color = Color::Red;
                    x = x_parent;
                    x_parent = x->parent;
                } else {
                    if (!w->right || w->right->color == Color::Black) {
                        if (w->left) w->left->color = Color::Black;
                        w->color = Color::Red;
                        rotate_right(w);
                        w = x_parent->right;
                    }
                    w->color = x_parent->color;
                    x_parent->color = Color::Black;
                    if (w->right) w->right->color = Color::Black;
                    rotate_left(x_parent);
                    x = m_root;
                }
            } else {
                Node* w = x_parent->left;
                if (w && w->color == Color::Red) {
                    w->color = Color::Black;
                    x_parent->color = Color::Red;
                    rotate_right(x_parent);
                    w = x_parent->left;
                }
                if (!w) {
                    /* A missing sibling represents two black children. */
                    x = x_parent;
                    x_parent = x->parent;
                    continue;
                }
                if ((!w->right || w->right->color == Color::Black) &&
                    (!w->left || w->left->color == Color::Black)) {
                    if (w) w->color = Color::Red;
                    x = x_parent;
                    x_parent = x->parent;
                } else {
                    if (!w->left || w->left->color == Color::Black) {
                        if (w->right) w->right->color = Color::Black;
                        w->color = Color::Red;
                        rotate_left(w);
                        w = x_parent->left;
                    }
                    w->color = x_parent->color;
                    x_parent->color = Color::Black;
                    if (w->left) w->left->color = Color::Black;
                    rotate_right(x_parent);
                    x = m_root;
                }
            }
        }
        if (x) x->color = Color::Black;
    }
    
    void destroy_tree(Node* node) {
        if (node) {
            destroy_tree(node->left);
            destroy_tree(node->right);
            destroy_node(node);
        }
    }

    bool owns_node(const Node* node) const {
        if (!node) return false;
        const Node* root = node;
        while (root->parent) root = root->parent;
        return root == m_root;
    }
    
    Node* copy_tree(Node* node, Node* parent) {
        if (!node) return nullptr;
        Node* new_node = allocate_node(node->data, parent);
        new_node->color = node->color;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
        new_node->left = copy_tree(node->left, new_node);
        new_node->right = copy_tree(node->right, new_node);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            /* A later child copy may fail after earlier children have
             * already been linked.  Reclaim the complete partial subtree
             * before propagating the original copy exception. */
            destroy_tree(new_node->left);
            destroy_tree(new_node->right);
            destroy_node(new_node);
            throw;
        }
#endif
        return new_node;
    }

    /* A merge must not detach a copyable source element before destination
     * comparison/allocation succeeds.  The old extract-then-move path could
     * lose that element when the destination comparator threw.  Dispatch on
     * copy construction so move-only keys retain the standard weaker
     * guarantee while copyable keys use an insert-before-erase transaction. */
    template<typename C2, typename A2>
    void merge_set_impl(set<Key, C2, A2>& source, true_type) {
        /* Build both the destination tree and the exact source-node list
         * before publishing either mutation.  The previous insert-before-
         * erase loop was only basic-guarantee: a later comparator or
         * allocation exception could leave earlier source nodes transferred.
         * Addresses remain stable until erase, so a pointer list lets the
         * commit phase erase without invoking Compare again. */
        vector<const value_type*> moved;
        set candidate(m_comp, m_alloc);
        auto existing = begin();
        while (existing != end()) {
            candidate.insert(*existing);
            ++existing;
        }
        auto current = source.begin();
        while (current != source.end()) {
            auto inserted = candidate.insert(*current);
            if (inserted.second) moved.push_back(&*current);
            ++current;
        }

        current = source.begin();
        while (current != source.end()) {
            auto node = current++;
            const value_type* address = &*node;
            bool transfer = false;
            for (size_type index = 0u; index < moved.size(); ++index) {
                if (moved[index] == address) {
                    transfer = true;
                    break;
                }
            }
            if (transfer) source.erase(node);
        }

        Node* old_root = m_root;
        size_type old_size = m_size;
        m_root = candidate.m_root;
        m_size = candidate.m_size;
        candidate.m_root = old_root;
        candidate.m_size = old_size;
    }

    template<typename C2, typename A2>
    void merge_set_impl(set<Key, C2, A2>& source, false_type) {
        /* Move-only keys use a candidate tree as the publication boundary.
         * Source nodes stay linked until every comparison and allocation has
         * completed; keys already moved from them may be unspecified on a
         * later exception, so this is the standard basic-guarantee path. */
        vector<const value_type*> moved;
        set candidate(m_comp, m_alloc);
        for (Node* existing = m_root ? m_root->minimum() : nullptr;
             existing; existing = existing->successor())
            candidate.insert(std::move(existing->data));
        for (auto current = source.begin(); current != source.end(); ++current) {
            auto inserted = candidate.insert(
                std::move(const_cast<value_type&>(*current)));
            if (inserted.second) moved.push_back(&*current);
        }

        for (auto current = source.begin(); current != source.end();) {
            auto node = current++;
            const value_type* address = &*node;
            for (size_type index = 0u; index < moved.size(); ++index) {
                if (moved[index] == address) {
                    source.erase(node);
                    break;
                }
            }
        }

        Node* old_root = m_root;
        size_type old_size = m_size;
        m_root = candidate.m_root;
        m_size = candidate.m_size;
        candidate.m_root = old_root;
        candidate.m_size = old_size;
    }

    template<typename C2, typename A2>
    void merge_multiset_impl(multiset<Key, C2, A2>& source,
                             true_type) {
        vector<size_type> moved;
        set candidate(m_comp, m_alloc);
        auto existing = begin();
        while (existing != end()) {
            candidate.insert(*existing);
            ++existing;
        }
        for (size_type index = 0u; index < source.size(); ++index) {
            auto inserted = candidate.insert(source.begin()[index]);
            if (inserted.second) moved.push_back(index);
        }

        /* multiset storage is a flat array, so erase selected source indices
         * from the end.  This avoids pointer invalidation and performs no
         * further comparator calls during the commit. */
        for (size_type index = moved.size(); index > 0u; --index)
            source.erase(source.begin() + moved[index - 1u]);

        Node* old_root = m_root;
        size_type old_size = m_size;
        m_root = candidate.m_root;
        m_size = candidate.m_size;
        candidate.m_root = old_root;
        candidate.m_size = old_size;
    }

    template<typename C2, typename A2>
    void merge_multiset_impl(multiset<Key, C2, A2>& source,
                             false_type) {
        vector<size_type> moved;
        set candidate(m_comp, m_alloc);
        for (Node* existing = m_root ? m_root->minimum() : nullptr;
             existing; existing = existing->successor())
            candidate.insert(std::move(existing->data));
        for (size_type index = 0u; index < source.size(); ++index) {
            auto inserted = candidate.insert(std::move(
                const_cast<value_type&>(source.begin()[index])));
            if (inserted.second) moved.push_back(index);
        }

        for (size_type index = moved.size(); index > 0u; --index)
            source.erase(source.begin() + moved[index - 1u]);

        Node* old_root = m_root;
        size_type old_size = m_size;
        m_root = candidate.m_root;
        m_size = candidate.m_size;
        candidate.m_root = old_root;
        candidate.m_size = old_size;
    }

#if __cplusplus > 202002L
    /* Copyable keys can stage the complete C++23 range insertion before
     * publishing the replacement tree.  This keeps comparator, allocation,
     * and key-construction exceptions from exposing a partially inserted
     * destination.  Move-only keys retain the existing basic-guarantee path. */
    template<typename R>
    void insert_range_impl(R&& range, true_type) {
        set candidate(m_comp, m_alloc);
        for (auto existing = begin(); existing != end(); ++existing)
            candidate.insert(*existing);
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        for (; first != last; ++first)
            candidate.insert(*first);

        Node* old_root = m_root;
        size_type old_size = m_size;
        m_root = candidate.m_root;
        m_size = candidate.m_size;
        candidate.m_root = old_root;
        candidate.m_size = old_size;
    }

    template<typename R>
    void insert_range_impl(R&& range, false_type) {
        /* A move-only key cannot be copied into the replacement tree.  Move
         * existing nodes and the incoming range into a private tree first so
         * publication remains one step; failures may leave already-moved
         * source keys unspecified, which is the basic-guarantee path. */
        set candidate(m_comp, m_alloc);
        if (m_root) {
            for (Node* existing = m_root->minimum(); existing;
                 existing = existing->successor())
                candidate.insert(std::move(existing->data));
        }
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        for (; first != last; ++first)
            candidate.insert(std::move(*first));

        Node* old_root = m_root;
        size_type old_size = m_size;
        m_root = candidate.m_root;
        m_size = candidate.m_size;
        candidate.m_root = old_root;
        candidate.m_size = old_size;
    }
#endif
     
public:
    /* イテレータ */
    class iterator {
        friend class set;
        Node* m_node;
        const set* m_set;
        
    public:
        using iterator_category = bidirectional_iterator_tag;
        using value_type = set::value_type;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;
        
        iterator() : m_node(nullptr), m_set(nullptr) {}
        iterator(Node* node, const set* s) : m_node(node), m_set(s) {}
        
        reference operator*() const { return m_node->data; }
        pointer operator->() const { return &m_node->data; }
        
        iterator& operator++() {
            m_node = m_node->successor();
            return *this;
        }
        
        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        
        iterator& operator--() {
            if (!m_node) {
                m_node = const_cast<Node*>(m_set->m_root);
                if (m_node) m_node = m_node->maximum();
            } else {
                m_node = m_node->predecessor();
            }
            return *this;
        }
        
        iterator operator--(int) {
            iterator tmp = *this;
            --(*this);
            return tmp;
        }
        
        bool operator==(const iterator& other) const {
            return m_set == other.m_set && m_node == other.m_node;
        }
        bool operator!=(const iterator& other) const { return !(*this == other); }
    };
    
    using const_iterator = iterator;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    struct insert_return_type {
        iterator position;
        bool inserted;
        node_type node;
    };
    
    /* コンストラクタ・デストラクタ */
    set() : m_alloc(), m_root(nullptr), m_size(0), m_comp() {}

    explicit set(const Compare& comp)
        : m_alloc(), m_root(nullptr), m_size(0), m_comp(comp) {}

    explicit set(const allocator_type& alloc)
        : m_alloc(alloc), m_root(nullptr), m_size(0), m_comp() {}

    set(const Compare& comp, const allocator_type& alloc)
        : m_alloc(alloc), m_root(nullptr), m_size(0), m_comp(comp) {}

    template<typename InputIt,
             typename enable_if<!is_integral<remove_cv_t<InputIt>>::value,
                                int>::type = 0>
    set(InputIt first, InputIt last, const Compare& comp = Compare(),
        const allocator_type& alloc = allocator_type())
        : m_alloc(alloc), m_root(nullptr), m_size(0), m_comp(comp) {
        for (; first != last; ++first) {
            insert(*first);
        }
    }
    
    set(std::initializer_list<value_type> init, const Compare& comp = Compare(),
        const allocator_type& alloc = allocator_type())
        : set(init.begin(), init.end(), comp, alloc) {}

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, value_type>
    set(from_range_t, R&& range, const Compare& comp = Compare(),
        const allocator_type& alloc = allocator_type())
        : set(comp, alloc) {
        insert_range(std::forward<R>(range));
    }
#endif
    
    set(const set& other)
        : m_alloc(allocator_traits<allocator_type>::
                      select_on_container_copy_construction(other.m_alloc)),
          m_root(nullptr), m_size(0),
          m_comp(other.m_comp) {
        if (other.m_size > max_size())
            set_detail::length_failure();
        m_root = copy_tree(other.m_root, nullptr);
        m_size = other.m_size;
    }

    set(const set& other, const allocator_type& alloc)
        : m_alloc(alloc), m_root(nullptr), m_size(0),
          m_comp(other.m_comp) {
        if (other.m_size > max_size())
            set_detail::length_failure();
        m_root = copy_tree(other.m_root, nullptr);
        m_size = other.m_size;
    }
    
    set(set&& other)
        noexcept(noexcept(Compare(std::move(other.m_comp))))
        : m_alloc(std::move(other.m_alloc)), m_root(other.m_root),
          m_size(other.m_size), m_comp(std::move(other.m_comp)) {
        other.m_root = nullptr;
        other.m_size = 0;
    }
    
    ~set() {
        destroy_tree(m_root);
    }
    
    set& operator=(const set& other) {
        if (this != &other) {
            /* Build the replacement before touching this tree.  The old
             * implementation destroyed the destination first, so a key
             * copy or descendant allocation exception left it empty.  A
             * complete candidate gives copy assignment its strong rollback
             * guarantee for the normal nothrow-swappable comparator path. */
            const bool propagate = allocator_traits<allocator_type>::
                propagate_on_container_copy_assignment::value;
            set candidate(other, propagate ? other.m_alloc : m_alloc);
            if (propagate) {
                m_comp = candidate.m_comp;
                destroy_tree(m_root);
                m_alloc = other.m_alloc;
                m_root = candidate.m_root;
                m_size = candidate.m_size;
                candidate.m_root = nullptr;
                candidate.m_size = 0;
            } else {
                swap(candidate);
            }
        }
        return *this;
    }
    
    set& operator=(set&& other)
        noexcept(node_allocator_traits::propagate_on_container_move_assignment::value &&
                 noexcept(m_comp = std::move(other.m_comp))) {
        if (this != &other) {
            if (!node_allocator_traits::propagate_on_container_move_assignment::value) {
                if (other.m_size > max_size())
                    set_detail::length_failure();
                set candidate(other.m_comp, m_alloc);
                for (auto it = other.begin(); it != other.end(); ++it)
                    candidate.insert(std::move(const_cast<value_type&>(*it)));
                clear();
                m_root = candidate.m_root;
                m_size = candidate.m_size;
                m_comp = std::move(other.m_comp);
                candidate.m_root = nullptr;
                candidate.m_size = 0;
                other.clear();
                return *this;
            }
            destroy_tree(m_root);
            if (node_allocator_traits::propagate_on_container_move_assignment::value)
                m_alloc = std::move(other.m_alloc);
            m_root = other.m_root;
            m_size = other.m_size;
            m_comp = std::move(other.m_comp);
            other.m_root = nullptr;
            other.m_size = 0;
        }
        return *this;
    }
    
    /* イテレータ */
    iterator begin() noexcept {
        return iterator(m_root ? m_root->minimum() : nullptr, this);
    }
    const_iterator begin() const noexcept {
        return const_iterator(m_root ? const_cast<Node*>(m_root)->minimum() : nullptr, this);
    }
    const_iterator cbegin() const noexcept { return begin(); }
    
    iterator end() noexcept { return iterator(nullptr, this); }
    const_iterator end() const noexcept { return const_iterator(nullptr, this); }
    const_iterator cend() const noexcept { return end(); }
    
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return rend(); }
    
    /* 容量 */
    bool empty() const noexcept { return m_size == 0; }
    size_type size() const noexcept { return m_size; }
    size_type max_size() const noexcept {
        return node_allocator_traits::max_size(m_alloc);
    }
    
    /* 変更 */
    void clear() noexcept {
        destroy_tree(m_root);
        m_root = nullptr;
        m_size = 0;
    }
    
    pair<iterator, bool> insert(const value_type& value) {
        Node* parent = nullptr;
        Node* current = m_root;
        bool insert_left = false;
        
        while (current) {
            parent = current;
            if (m_comp(value, current->data)) {
                insert_left = true;
                current = current->left;
            } else if (m_comp(current->data, value)) {
                insert_left = false;
                current = current->right;
            } else {
                return {iterator(current, this), false};
            }
        }
        if (m_size >= max_size())
            set_detail::length_failure();
        Node* new_node = allocate_node(value, parent);
        
        if (!parent) {
            m_root = new_node;
        } else if (insert_left) {
            parent->left = new_node;
        } else {
            parent->right = new_node;
        }
        
        insert_fixup(new_node);
        ++m_size;
        return {iterator(new_node, this), true};
    }
    
    pair<iterator, bool> insert(value_type&& value) {
        Node* parent = nullptr;
        Node* current = m_root;
        bool insert_left = false;

        while (current) {
            parent = current;
            if (m_comp(value, current->data)) {
                insert_left = true;
                current = current->left;
            } else if (m_comp(current->data, value)) {
                insert_left = false;
                current = current->right;
            } else {
                return {iterator(current, this), false};
            }
        }

        if (m_size >= max_size())
            set_detail::length_failure();
        Node* new_node = allocate_node(std::move(value), parent);
        if (!parent) {
            m_root = new_node;
        } else if (insert_left) {
            parent->left = new_node;
        } else {
            parent->right = new_node;
        }

        insert_fixup(new_node);
        ++m_size;
        return {iterator(new_node, this), true};
    }

    /* ヒント付き挿入（ヒントは無視、標準インターフェース互換用） */
    iterator insert(const_iterator /*hint*/, const value_type& value) {
        return insert(value).first;
    }

    iterator insert(const_iterator /*hint*/, value_type&& value) {
        return insert(std::move(value)).first;
    }

    template<typename... Args>
    pair<iterator, bool> emplace(Args&&... args) {
        return insert(value_type(std::forward<Args>(args)...));
    }

    template<typename... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        (void)hint;
        return emplace(std::forward<Args>(args)...).first;
    }

    /* Range insert */
    template<typename InputIt>
    void insert(InputIt first, InputIt last) {
        for (; first != last; ++first) {
            insert(*first);
        }
    }

    void insert(std::initializer_list<value_type> ilist) {
        insert(ilist.begin(), ilist.end());
    }

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, value_type>
    void insert_range(R&& range) {
        insert_range_impl(std::forward<R>(range),
                          integral_constant<bool,
                              is_copy_constructible<value_type>::value>());
    }
#endif

    node_type extract(const_iterator position) {
        if ((!position.m_node && position.m_set != this) ||
            !owns_node(position.m_node))
            return node_type();
        allocator_type value_alloc(m_alloc);
        using value_pointer = typename allocator_traits<allocator_type>::pointer;
        value_pointer allocation = allocator_traits<allocator_type>::allocate(value_alloc, 1);
        value_type* value = set_detail::pointer_address(allocation);
        if (allocation == value_pointer() || !value) {
            if (allocation != value_pointer())
                allocator_traits<allocator_type>::deallocate(value_alloc, allocation, 1);
            set_detail::allocation_failure();
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            allocator_traits<allocator_type>::construct(
                value_alloc, value, std::move(position.m_node->data));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            allocator_traits<allocator_type>::deallocate(value_alloc, allocation, 1);
            throw;
        }
#endif
        erase(position);
        return node_type(value, value_alloc);
    }

    node_type extract(const key_type& key) {
        iterator position = find(key);
        return position == end() ? node_type() : extract(position);
    }

#if defined(RIN_SET_HAS_HETERO_INSERTION)
    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type,
             typename = typename enable_if<
                 is_constructible<key_type, K&&>::value>::type>
    node_type extract(const K& key) {
        iterator position = find(key);
        return position == end() ? node_type() : extract(position);
    }
#endif

    insert_return_type insert(node_type&& node) {
        if (node.empty()) return {end(), false, node_type()};
        auto result = insert(std::move(node.value()));
        if (result.second) {
            node.reset();
            return {result.first, true, node_type()};
        }
        return {result.first, false, std::move(node)};
    }

    iterator insert(const_iterator /*hint*/, node_type&& node) {
        if (node.empty()) return end();
        auto result = insert(std::move(node));
        if (!result.inserted && result.node) node = std::move(result.node);
        return result.position;
    }

    iterator erase(const_iterator pos) {
        /* A foreign iterator is invalid input.  Do not interpret its node as
         * belonging to this tree; doing so can detach nodes from another set
         * and corrupt both containers. */
        if ((!pos.m_node && pos.m_set != this) || !owns_node(pos.m_node))
            return end();
        
        Node* z = pos.m_node;
        Node* succ = z->successor();
        
        Node* y = z;
        Node* x;
        Node* x_parent;
        Color y_original_color = y->color;
        
        if (!z->left) {
            x = z->right;
            x_parent = z->parent;
            transplant(z, z->right);
        } else if (!z->right) {
            x = z->left;
            x_parent = z->parent;
            transplant(z, z->left);
        } else {
            y = z->right->minimum();
            y_original_color = y->color;
            x = y->right;
            x_parent = y;
            
            if (y->parent == z) {
                if (x) x->parent = y;
            } else {
                x_parent = y->parent;
                transplant(y, y->right);
                y->right = z->right;
                y->right->parent = y;
            }
            
            transplant(z, y);
            y->left = z->left;
            y->left->parent = y;
            y->color = z->color;
        }
        
        destroy_node(z);
        --m_size;
        
        if (y_original_color == Color::Black) {
            erase_fixup(x, x_parent);
        }
        
        return iterator(succ, this);
    }
    
    size_type erase(const key_type& key) {
        iterator it = find(key);
        if (it == end()) return 0;
        erase(it);
        return 1;
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    size_type erase(const K& key) {
        iterator it = find(key);
        if (it == end()) return 0;
        erase(it);
        return 1;
    }

    /* 範囲削除 [first, last) */
    iterator erase(const_iterator first, const_iterator last) {
        if ((!first.m_node && first.m_set != this) ||
            (!last.m_node && last.m_set != this) ||
            (first.m_node && !owns_node(first.m_node)) ||
            (last.m_node && !owns_node(last.m_node))) return end();
        while (first != last) {
            first = erase(first);
        }
        return iterator(first.m_node, this);
    }

    void swap(set& other) {
        if (this == &other) return;
        if (!node_allocator_traits::propagate_on_container_swap::value &&
            m_alloc != other.m_alloc) {
            swap_unequal(other);
            return;
        }
        if (node_allocator_traits::propagate_on_container_swap::value)
            set_detail::set_exchange(m_alloc, other.m_alloc);
        set_detail::set_exchange(m_root, other.m_root);
        set_detail::set_exchange(m_size, other.m_size);
        set_detail::set_exchange(m_comp, other.m_comp);
    }
    
    /* 検索 */
    iterator find(const key_type& key) {
        Node* current = m_root;
        while (current) {
            if (m_comp(key, current->data)) {
                current = current->left;
            } else if (m_comp(current->data, key)) {
                current = current->right;
            } else {
                return iterator(current, this);
            }
        }
        return end();
    }
    
    const_iterator find(const key_type& key) const {
        return const_cast<set*>(this)->find(key);
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    iterator find(const K& key) {
        Node* current = m_root;
        while (current) {
            if (m_comp(key, current->data)) {
                current = current->left;
            } else if (m_comp(current->data, key)) {
                current = current->right;
            } else {
                return iterator(current, this);
            }
        }
        return end();
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    const_iterator find(const K& key) const {
        return const_cast<set*>(this)->find(key);
    }
    
    size_type count(const key_type& key) const {
        return find(key) != end() ? 1 : 0;
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    size_type count(const K& key) const {
        return find(key) != end() ? 1 : 0;
    }
    
    bool contains(const key_type& key) const {
        return find(key) != end();
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    bool contains(const K& key) const {
        return find(key) != end();
    }
    
    iterator lower_bound(const key_type& key) {
        Node* current = m_root;
        Node* result = nullptr;
        while (current) {
            if (!m_comp(current->data, key)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        return iterator(result, this);
    }
    
    const_iterator lower_bound(const key_type& key) const {
        return const_cast<set*>(this)->lower_bound(key);
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    iterator lower_bound(const K& key) {
        Node* current = m_root;
        Node* result = nullptr;
        while (current) {
            if (!m_comp(current->data, key)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        return iterator(result, this);
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    const_iterator lower_bound(const K& key) const {
        return const_cast<set*>(this)->lower_bound(key);
    }
    
    iterator upper_bound(const key_type& key) {
        Node* current = m_root;
        Node* result = nullptr;
        while (current) {
            if (m_comp(key, current->data)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        return iterator(result, this);
    }
    
    const_iterator upper_bound(const key_type& key) const {
        return const_cast<set*>(this)->upper_bound(key);
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    iterator upper_bound(const K& key) {
        Node* current = m_root;
        Node* result = nullptr;
        while (current) {
            if (m_comp(key, current->data)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        return iterator(result, this);
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    const_iterator upper_bound(const K& key) const {
        return const_cast<set*>(this)->upper_bound(key);
    }
    
    pair<iterator, iterator> equal_range(const key_type& key) {
        return {lower_bound(key), upper_bound(key)};
    }
    
    pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
        return {lower_bound(key), upper_bound(key)};
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    pair<iterator, iterator> equal_range(const K& key) {
        return {lower_bound(key), upper_bound(key)};
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    pair<const_iterator, const_iterator> equal_range(const K& key) const {
        return {lower_bound(key), upper_bound(key)};
    }

    template<typename C2, typename A2>
    void merge(set<Key, C2, A2>& source) {
        if (static_cast<const void*>(this) == static_cast<const void*>(&source)) return;
        merge_set_impl(source, integral_constant<bool,
                        is_copy_constructible<value_type>::value>());
    }

    template<typename C2, typename A2>
    void merge(multiset<Key, C2, A2>& source) {
        if (static_cast<const void*>(this) == static_cast<const void*>(&source)) return;
        merge_multiset_impl(source, integral_constant<bool,
                             is_copy_constructible<value_type>::value>());
    }
    
    /* オブザーバ */
    key_compare key_comp() const { return m_comp; }
    value_compare value_comp() const { return m_comp; }
    allocator_type get_allocator() const noexcept { return allocator_type(m_alloc); }
};

/* 非メンバ関数 */
template<typename K, typename C, typename A>
bool operator==(const set<K, C, A>& lhs, const set<K, C, A>& rhs) {
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

template<typename K, typename C, typename A>
bool operator!=(const set<K, C, A>& lhs, const set<K, C, A>& rhs) {
    return !(lhs == rhs);
}

template<typename K, typename C, typename A>
void swap(set<K, C, A>& lhs, set<K, C, A>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

template<typename K, typename C, typename A>
bool operator<(const set<K, C, A>& lhs, const set<K, C, A>& rhs) {
    auto left = lhs.begin();
    auto right = rhs.begin();
    while (left != lhs.end() && right != rhs.end()) {
        if (*left < *right) return true;
        if (*right < *left) return false;
        ++left;
        ++right;
    }
    return left == lhs.end() && right != rhs.end();
}

template<typename K, typename C, typename A>
bool operator>(const set<K, C, A>& lhs, const set<K, C, A>& rhs) {
    return rhs < lhs;
}

template<typename K, typename C, typename A>
bool operator<=(const set<K, C, A>& lhs, const set<K, C, A>& rhs) {
    return !(rhs < lhs);
}

template<typename K, typename C, typename A>
bool operator>=(const set<K, C, A>& lhs, const set<K, C, A>& rhs) {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
template<typename K, typename C, typename A>
auto operator<=>(const set<K, C, A>& lhs, const set<K, C, A>& rhs)
    -> detail::synth_three_way_result_t<K> {
    using result_type = detail::synth_three_way_result_t<K>;
    auto left = lhs.begin();
    auto right = rhs.begin();
    while (left != lhs.end() && right != rhs.end()) {
        const auto result = detail::synth_three_way(*left, *right);
        if (result != 0) return result;
        ++left;
        ++right;
    }
    if (left == lhs.end() && right != rhs.end()) return result_type::less;
    if (right == rhs.end() && left != lhs.end()) return result_type::greater;
    return result_type::equivalent;
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * multiset - 重複を許す順序付き集合（簡易実装）
 * ═══════════════════════════════════════════════════════════════*/

template<typename Key, typename Compare = less<Key>, typename Allocator = void>
class multiset {
public:
    using key_type = Key;
    using value_type = Key;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using key_compare = Compare;
    using allocator_type = typename set_detail::effective_allocator<
        value_type, Allocator>::type;
    using value_compare = Compare;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    class node_type {
        value_type* m_value;
        allocator_type m_alloc;
        using value_allocator_traits = allocator_traits<allocator_type>;

        explicit node_type(value_type* value, const allocator_type& alloc)
            : m_value(value), m_alloc(alloc) {}
        friend class multiset;

        void reset() noexcept {
            if (!m_value) return;
            value_allocator_traits::destroy(m_alloc, m_value);
            value_allocator_traits::deallocate(
                m_alloc,
                pointer_traits<typename value_allocator_traits::pointer>::pointer_to(
                    *m_value),
                1);
            m_value = nullptr;
        }

    public:
        node_type() : m_value(nullptr), m_alloc() {}
        node_type(node_type&& other) noexcept
            : m_value(other.m_value), m_alloc(std::move(other.m_alloc)) {
            other.m_value = nullptr;
        }
        node_type& operator=(node_type&& other) noexcept {
            if (this != &other) {
                reset();
                m_value = other.m_value;
                m_alloc = std::move(other.m_alloc);
                other.m_value = nullptr;
            }
            return *this;
        }
        node_type(const node_type&) = delete;
        node_type& operator=(const node_type&) = delete;
        ~node_type() { reset(); }

        bool empty() const noexcept { return m_value == nullptr; }
        allocator_type get_allocator() const noexcept { return m_alloc; }
        explicit operator bool() const noexcept { return !empty(); }
        const key_type& key() const { return *m_value; }
        value_type& value() const { return *m_value; }
    };

    class iterator;
    using const_iterator = iterator;

private:
    template<typename, typename, typename>
    friend class multiset;

    /* シンプルなソート済み動的配列実装 */
    using alloc_traits = allocator_traits<allocator_type>;
    using value_pointer = typename alloc_traits::pointer;
    allocator_type alloc_;
    value_type* data_;
    size_type size_;
    size_type capacity_;
    Compare comp_;

    value_type* allocate_data(size_type count) {
        value_pointer allocation = alloc_traits::allocate(alloc_, count);
        value_type* result = set_detail::pointer_address(allocation);
        if (allocation == value_pointer() || !result) {
            if (allocation != value_pointer())
                alloc_traits::deallocate(alloc_, allocation, count);
            set_detail::allocation_failure();
        }
        return result;
    }

    void deallocate_data(value_type* data, size_type count) noexcept {
        if (data) {
            alloc_traits::deallocate(
                alloc_, pointer_traits<value_pointer>::pointer_to(*data), count);
        }
    }

    template<typename... Args>
    void construct_value(value_type* location, Args&&... args) {
        alloc_traits::construct(alloc_, location, std::forward<Args>(args)...);
    }

    void destroy_value(value_type* location) noexcept {
        alloc_traits::destroy(alloc_, location);
    }

    template<typename Value>
    const value_type* insert_value_copy(Value&& value) {
        size_type pos = 0;
        while (pos < size_ && comp_(data_[pos], value)) ++pos;

        if (size_ >= max_size())
            set_detail::length_failure();
        if (size_ == static_cast<size_type>(-1)) {
            set_detail::allocation_failure();
        }
        size_type new_size = size_ + 1;
        size_type new_cap = capacity_ == 0 ? 8 : capacity_;
        if (new_cap < new_size) {
            if (new_cap > static_cast<size_type>(-1) / 2) {
                set_detail::allocation_failure();
            }
            new_cap *= 2;
            if (new_cap < new_size) new_cap = new_size;
        }
        if (new_cap > static_cast<size_type>(-1) / sizeof(value_type)) {
            set_detail::allocation_failure();
        }
        value_type* new_data = allocate_data(new_cap);

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        size_type constructed = 0;
        try {
#endif
            for (size_type i = 0; i < new_size; ++i) {
                if (i == pos) {
                    construct_value(new_data + i, std::forward<Value>(value));
                } else {
                    size_type old_i = i < pos ? i : i - 1;
                    construct_value(new_data + i, data_[old_i]);
                }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                ++constructed;
#endif
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            for (size_type i = 0; i < constructed; ++i) {
                destroy_value(new_data + i);
            }
            deallocate_data(new_data, new_cap);
            throw;
        }
#endif

        for (size_type i = 0; i < size_; ++i) destroy_value(data_ + i);
        deallocate_data(data_, capacity_);
        data_ = new_data;
        size_ = new_size;
        capacity_ = new_cap;
        return data_ + pos;
    }

    template<typename Value>
    const value_type* insert_value_move(Value&& value) {
        /* Find the position before allocating so a comparator exception cannot
         * observe a partially mutated array.  A move-only insertion uses the
         * same candidate publication boundary as growth: the old array stays
         * fully owned until every replacement element has been constructed. */
        size_type pos = 0;
        while (pos < size_ && comp_(data_[pos], value)) ++pos;

        if (size_ >= max_size())
            set_detail::length_failure();
        if (size_ == static_cast<size_type>(-1))
            set_detail::allocation_failure();
        size_type new_size = size_ + 1;
        size_type new_cap = capacity_ == 0 ? 8 : capacity_;
        if (new_cap < new_size) {
            if (new_cap > static_cast<size_type>(-1) / 2)
                set_detail::allocation_failure();
            new_cap *= 2;
            if (new_cap < new_size) new_cap = new_size;
        }
        if (new_cap > static_cast<size_type>(-1) / sizeof(value_type))
            set_detail::allocation_failure();
        value_type* new_data = allocate_data(new_cap);

        size_type constructed = 0;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (; constructed < pos; ++constructed)
                construct_value(new_data + constructed,
                                std::move(data_[constructed]));
            construct_value(new_data + constructed,
                            std::forward<Value>(value));
            ++constructed;
            for (size_type old_index = pos; old_index < size_; ++old_index,
                 ++constructed)
                construct_value(new_data + constructed,
                                std::move(data_[old_index]));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            while (constructed != 0) {
                --constructed;
                destroy_value(new_data + constructed);
            }
            deallocate_data(new_data, new_cap);
            throw;
        }
#endif

        for (size_type i = 0; i < size_; ++i) destroy_value(data_ + i);
        deallocate_data(data_, capacity_);
        data_ = new_data;
        size_ = new_size;
        capacity_ = new_cap;
        return data_ + pos;
    }

    template<typename Value>
    const value_type* insert_value_dispatch(Value&& value, true_type) {
        return insert_value_copy(std::forward<Value>(value));
    }

    template<typename Value>
    const value_type* insert_value_dispatch(Value&& value, false_type) {
        return insert_value_move(std::forward<Value>(value));
    }

    void grow() {
        if (capacity_ > static_cast<size_type>(-1) / 2) {
            set_detail::allocation_failure();
        }
        size_type new_cap = capacity_ == 0 ? 8 : capacity_ * 2;
        if (new_cap > static_cast<size_type>(-1) / sizeof(value_type)) {
            set_detail::allocation_failure();
        }
        value_type* new_data = allocate_data(new_cap);

        /* A move-only key may have a throwing move constructor.  Do not
         * destroy each source element as soon as its replacement is built:
         * an exception on a later element would leave a hole in the old
         * array while the new prefix was still owned only by this function.
         * Keep both arrays live until the complete candidate is ready, then
         * publish them in one step.  A throwing move may leave already moved
         * source values unspecified, but every live object and allocation is
         * still accounted for and the container remains destructible. */
        size_type constructed = 0;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (; constructed < size_; ++constructed) {
                construct_value(new_data + constructed,
                                std::move(data_[constructed]));
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            while (constructed != 0) {
                --constructed;
                destroy_value(new_data + constructed);
            }
            deallocate_data(new_data, new_cap);
            throw;
        }
#endif
        for (size_type i = 0; i < size_; ++i) destroy_value(data_ + i);
        deallocate_data(data_, capacity_);
        data_ = new_data;
        capacity_ = new_cap;
    }

    /* Copyable values can be merged transactionally by publishing the
     * destination copy before erasing the source.  This leaves the source
     * untouched if comparison, allocation, or element construction throws.
     * Move-only values keep the node-extraction path and its standard weaker
     * exception guarantee. */
    template<typename C2, typename A2>
    void merge_set_impl(set<Key, C2, A2>& source, true_type) {
        /* Complete a copyable source merge in private storage.  The source
         * stays untouched until every comparison, allocation, and element
         * construction has succeeded. */
        multiset candidate(comp_, alloc_);
        for (size_type index = 0u; index < size_; ++index)
            candidate.insert(data_[index]);
        for (auto current = source.begin(); current != source.end(); ++current)
            candidate.insert(*current);

        value_type* old_data = data_;
        size_type old_size = size_;
        size_type old_capacity = capacity_;
        data_ = candidate.data_;
        size_ = candidate.size_;
        capacity_ = candidate.capacity_;
        candidate.data_ = old_data;
        candidate.size_ = old_size;
        candidate.capacity_ = old_capacity;
        source.clear();
    }

    template<typename C2, typename A2>
    void merge_set_impl(set<Key, C2, A2>& source, false_type) {
        vector<const value_type*> moved;
        multiset candidate(comp_, alloc_);
        for (size_type index = 0u; index < size_; ++index)
            candidate.insert(std::move(data_[index]));
        for (auto current = source.begin(); current != source.end(); ++current) {
            candidate.insert(std::move(const_cast<value_type&>(*current)));
            moved.push_back(&*current);
        }

        for (auto current = source.begin(); current != source.end();) {
            auto node = current++;
            const value_type* address = &*node;
            for (size_type index = 0u; index < moved.size(); ++index) {
                if (moved[index] == address) {
                    source.erase(node);
                    break;
                }
            }
        }

        value_type* old_data = data_;
        size_type old_size = size_;
        size_type old_capacity = capacity_;
        data_ = candidate.data_;
        size_ = candidate.size_;
        capacity_ = candidate.capacity_;
        candidate.data_ = old_data;
        candidate.size_ = old_size;
        candidate.capacity_ = old_capacity;
    }

    template<typename C2, typename A2>
    void merge_multiset_impl(multiset<Key, C2, A2>& source,
                             true_type) {
        multiset candidate(comp_, alloc_);
        for (size_type index = 0u; index < size_; ++index)
            candidate.insert(data_[index]);
        for (auto current = source.begin(); current != source.end(); ++current)
            candidate.insert(*current);

        value_type* old_data = data_;
        size_type old_size = size_;
        size_type old_capacity = capacity_;
        data_ = candidate.data_;
        size_ = candidate.size_;
        capacity_ = candidate.capacity_;
        candidate.data_ = old_data;
        candidate.size_ = old_size;
        candidate.capacity_ = old_capacity;
        source.clear();
        source.deallocate_data(source.data_, source.capacity_);
        source.data_ = nullptr;
        source.capacity_ = 0u;
    }

    template<typename C2, typename A2>
    void merge_multiset_impl(multiset<Key, C2, A2>& source,
                             false_type) {
        multiset candidate(comp_, alloc_);
        for (size_type index = 0u; index < size_; ++index)
            candidate.insert(std::move(data_[index]));
        for (size_type index = 0u; index < source.size(); ++index)
            candidate.insert(std::move(
                const_cast<value_type&>(source.begin()[index])));
        source.clear();

        value_type* old_data = data_;
        size_type old_size = size_;
        size_type old_capacity = capacity_;
        data_ = candidate.data_;
        size_ = candidate.size_;
        capacity_ = candidate.capacity_;
        candidate.data_ = old_data;
        candidate.size_ = old_size;
        candidate.capacity_ = old_capacity;
    }

#if __cplusplus > 202002L
    /* Stage copyable multiset values in a replacement array so a throwing
     * comparator, allocator, or value constructor leaves the destination
     * unchanged.  Move-only values keep the existing basic-guarantee loop. */
    template<typename R>
    void insert_range_impl(R&& range, true_type) {
        multiset candidate(comp_, alloc_);
        for (size_type index = 0u; index < size_; ++index)
            candidate.insert(data_[index]);
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        for (; first != last; ++first)
            candidate.insert(*first);

        value_type* old_data = data_;
        size_type old_size = size_;
        size_type old_capacity = capacity_;
        data_ = candidate.data_;
        size_ = candidate.size_;
        capacity_ = candidate.capacity_;
        candidate.data_ = old_data;
        candidate.size_ = old_size;
        candidate.capacity_ = old_capacity;
    }

    template<typename R>
    void insert_range_impl(R&& range, false_type) {
        /* Move-only values use the same replacement-array publication as the
         * copyable path.  Existing values and the rvalue range are consumed
         * into the candidate before the destination storage is exchanged. */
        multiset candidate(comp_, alloc_);
        for (size_type index = 0u; index < size_; ++index)
            candidate.insert(std::move(data_[index]));
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        for (; first != last; ++first)
            candidate.insert(std::move(*first));

        value_type* old_data = data_;
        size_type old_size = size_;
        size_type old_capacity = capacity_;
        data_ = candidate.data_;
        size_ = candidate.size_;
        capacity_ = candidate.capacity_;
        candidate.data_ = old_data;
        candidate.size_ = old_size;
        candidate.capacity_ = old_capacity;
    }
#endif

public:
    class iterator {
        friend class multiset;
        const multiset* m_container;
        const value_type* m_storage;
        size_type m_index;

        bool same_owner(const iterator& other) const noexcept {
            if (m_storage || other.m_storage)
                return m_storage == other.m_storage;
            return m_container == other.m_container;
        }

        iterator(const multiset* container, const value_type* storage,
                 size_type index)
            : m_container(container), m_storage(storage), m_index(index) {}

    public:
        using iterator_category = random_access_iterator_tag;
        using value_type = multiset::value_type;
        using difference_type = multiset::difference_type;
        using pointer = const value_type*;
        using reference = const value_type&;

        iterator() : m_container(nullptr), m_storage(nullptr), m_index(0) {}
        iterator(decltype(nullptr))
            : m_container(nullptr), m_storage(nullptr), m_index(0) {}

        iterator& operator=(decltype(nullptr)) {
            m_container = nullptr;
            m_storage = nullptr;
            m_index = 0;
            return *this;
        }

        reference operator*() const { return m_storage[m_index]; }
        pointer operator->() const { return m_storage + m_index; }

        iterator& operator++() {
            ++m_index;
            return *this;
        }
        iterator operator++(int) {
            iterator copy = *this;
            ++(*this);
            return copy;
        }
        iterator& operator--() {
            --m_index;
            return *this;
        }
        iterator operator--(int) {
            iterator copy = *this;
            --(*this);
            return copy;
        }
        iterator& operator+=(difference_type offset) {
            if (offset >= 0)
                m_index += static_cast<size_type>(offset);
            else
                m_index -= static_cast<size_type>(-(offset + 1)) + 1u;
            return *this;
        }
        iterator& operator-=(difference_type offset) {
            if (offset >= 0)
                m_index -= static_cast<size_type>(offset);
            else
                m_index += static_cast<size_type>(-(offset + 1)) + 1u;
            return *this;
        }
        iterator operator+(difference_type offset) const {
            iterator copy = *this;
            copy += offset;
            return copy;
        }
        iterator operator-(difference_type offset) const {
            iterator copy = *this;
            copy -= offset;
            return copy;
        }
        difference_type operator-(const iterator& other) const {
            if (!same_owner(other)) return 0;
            return static_cast<difference_type>(m_index) -
                   static_cast<difference_type>(other.m_index);
        }
        reference operator[](difference_type offset) const {
            return *(*this + offset);
        }

        bool operator==(const iterator& other) const noexcept {
            return same_owner(other) && m_index == other.m_index;
        }
        bool operator!=(const iterator& other) const noexcept {
            return !(*this == other);
        }
        bool operator<(const iterator& other) const noexcept {
            return same_owner(other) && m_index < other.m_index;
        }
        bool operator>(const iterator& other) const noexcept { return other < *this; }
        bool operator<=(const iterator& other) const noexcept {
            return *this < other || *this == other;
        }
        bool operator>=(const iterator& other) const noexcept {
            return other < *this || *this == other;
        }
    };

    friend iterator operator+(typename iterator::difference_type offset,
                              iterator value) {
        value += offset;
        return value;
    }
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    bool pointer_index(const_iterator position, size_type& index) const
        noexcept {
        if (position.m_storage != data_) return false;
        if (!data_ && position.m_container != this) return false;
        if (position.m_index > size_) return false;
        index = position.m_index;
        return true;
    }

public:

    multiset() : alloc_(), data_(nullptr), size_(0), capacity_(0), comp_() {}

    explicit multiset(const Compare& comp)
        : alloc_(), data_(nullptr), size_(0), capacity_(0), comp_(comp) {}

    explicit multiset(const allocator_type& alloc)
        : alloc_(alloc), data_(nullptr), size_(0), capacity_(0), comp_() {}

    multiset(const Compare& comp, const allocator_type& alloc)
        : alloc_(alloc), data_(nullptr), size_(0), capacity_(0), comp_(comp) {}

    template<typename InputIt,
             typename enable_if<!is_integral<remove_cv_t<InputIt>>::value,
                                int>::type = 0>
    multiset(InputIt first, InputIt last, const Compare& comp = Compare(),
             const allocator_type& alloc = allocator_type())
        : alloc_(alloc), data_(nullptr), size_(0), capacity_(0), comp_(comp) {
        for (; first != last; ++first) insert(*first);
    }

    multiset(std::initializer_list<value_type> init,
             const Compare& comp = Compare(),
             const allocator_type& alloc = allocator_type())
        : multiset(init.begin(), init.end(), comp, alloc) {}

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, value_type>
    multiset(from_range_t, R&& range, const Compare& comp = Compare(),
             const allocator_type& alloc = allocator_type())
        : multiset(comp, alloc) {
        insert_range(std::forward<R>(range));
    }
#endif

    multiset(const multiset& other)
        : alloc_(allocator_traits<allocator_type>::
                     select_on_container_copy_construction(other.alloc_)),
          data_(nullptr), size_(0), capacity_(0),
          comp_(other.comp_) {
        if (other.size_ > max_size())
            set_detail::length_failure();
        if (other.size_ > 0) {
            if (other.size_ > static_cast<size_type>(-1) / sizeof(value_type)) {
                set_detail::allocation_failure();
            }
            data_ = allocate_data(other.size_);
            capacity_ = other.size_;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            try {
#endif
                for (size_type i = 0; i < other.size_; ++i) {
                    construct_value(data_ + i, other.data_[i]);
                    ++size_;
                }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            } catch (...) {
                /* A later element copy may throw after earlier elements
                 * have been constructed.  Keep ownership in this partial
                 * candidate until every constructed element is destroyed. */
                clear();
                deallocate_data(data_, capacity_);
                data_ = nullptr;
                capacity_ = 0;
                throw;
            }
#endif
        }
    }

    multiset(const multiset& other, const allocator_type& alloc)
        : alloc_(alloc), data_(nullptr), size_(0), capacity_(0),
          comp_(other.comp_) {
        if (other.size_ > max_size())
            set_detail::length_failure();
        if (other.size_ > 0) {
            if (other.size_ > static_cast<size_type>(-1) / sizeof(value_type)) {
                set_detail::allocation_failure();
            }
            data_ = allocate_data(other.size_);
            capacity_ = other.size_;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            try {
#endif
                for (size_type i = 0; i < other.size_; ++i) {
                    construct_value(data_ + i, other.data_[i]);
                    ++size_;
                }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            } catch (...) {
                clear();
                deallocate_data(data_, capacity_);
                data_ = nullptr;
                capacity_ = 0;
                throw;
            }
#endif
        }
    }

    multiset(multiset&& other)
        noexcept(noexcept(Compare(std::move(other.comp_))))
        : alloc_(std::move(other.alloc_)), data_(other.data_),
          size_(other.size_), capacity_(other.capacity_),
          comp_(std::move(other.comp_)) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    ~multiset() {
        clear();
        deallocate_data(data_, capacity_);
    }

    multiset& operator=(const multiset& other) {
        if (this == &other) return *this;
        const bool propagate = allocator_traits<allocator_type>::
            propagate_on_container_copy_assignment::value;
        multiset replacement(other, propagate ? other.alloc_ : alloc_);
        if (propagate) {
            comp_ = replacement.comp_;
            clear();
            deallocate_data(data_, capacity_);
            alloc_ = other.alloc_;
            data_ = replacement.data_;
            size_ = replacement.size_;
            capacity_ = replacement.capacity_;
            replacement.data_ = nullptr;
            replacement.size_ = replacement.capacity_ = 0;
        } else {
            swap(replacement);
        }
        return *this;
    }

    multiset& operator=(multiset&& other)
        noexcept(alloc_traits::propagate_on_container_move_assignment::value &&
                 noexcept(comp_ = std::move(other.comp_))) {
        if (this == &other) return *this;
        if (!alloc_traits::propagate_on_container_move_assignment::value) {
            if (other.size_ > max_size())
                set_detail::length_failure();
            multiset replacement(other.comp_, alloc_);
            for (size_type i = 0; i < other.size_; ++i)
                replacement.insert(std::move(other.data_[i]));
            clear();
            deallocate_data(data_, capacity_);
            data_ = replacement.data_;
            size_ = replacement.size_;
            capacity_ = replacement.capacity_;
            comp_ = std::move(other.comp_);
            replacement.data_ = nullptr;
            replacement.size_ = replacement.capacity_ = 0;
            other.clear();
            return *this;
        }
        clear();
        deallocate_data(data_, capacity_);
        if (alloc_traits::propagate_on_container_move_assignment::value)
            alloc_ = std::move(other.alloc_);
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        comp_ = std::move(other.comp_);
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
        return *this;
    }

    iterator begin() noexcept { return iterator(this, data_, 0); }
    const_iterator begin() const noexcept { return const_iterator(this, data_, 0); }
    iterator end() noexcept { return iterator(this, data_, size_); }
    const_iterator end() const noexcept { return const_iterator(this, data_, size_); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(end());
    }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(begin());
    }

    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type max_size() const noexcept {
        return alloc_traits::max_size(alloc_);
    }

    void clear() {
        for (size_type i = 0; i < size_; ++i) {
            destroy_value(data_ + i);
        }
        size_ = 0;
    }

private:
    template<typename T = value_type>
    typename enable_if<is_copy_constructible<T>::value, iterator>::type
    erase_copy_range(size_type start, size_type finish) {
        size_type removed = finish - start;
        size_type new_size = size_ - removed;
        if (new_size == 0) {
            clear();
            deallocate_data(data_, capacity_);
            data_ = nullptr;
            capacity_ = 0;
            return end();
        }

        if (capacity_ > static_cast<size_type>(-1) / sizeof(value_type)) {
            set_detail::allocation_failure();
        }
        value_type* new_data = allocate_data(capacity_);

        size_type constructed = 0;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (size_type i = 0; i < size_; ++i) {
                if (i >= start && i < finish) continue;
                construct_value(new_data + constructed, data_[i]);
                ++constructed;
            }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            for (size_type i = 0; i < constructed; ++i) {
                destroy_value(new_data + i);
            }
            deallocate_data(new_data, capacity_);
            throw;
        }
#endif

        for (size_type i = 0; i < size_; ++i) destroy_value(data_ + i);
        deallocate_data(data_, capacity_);
        data_ = new_data;
        size_ = new_size;
        return iterator(this, data_, start < new_size ? start : new_size);
    }

    iterator erase_move_range(size_type start, size_type finish) {
        size_type removed = finish - start;
        if (removed == size_) {
            clear();
            deallocate_data(data_, capacity_);
            data_ = nullptr;
            capacity_ = 0;
            return end();
        }

        /* Build prefix/suffix in a private candidate before changing the
         * published array.  Throwing move-only values can leave moved source
         * values unspecified, but they must never leave a dead slot. */
        size_type new_size = size_ - removed;
        value_type* new_data = allocate_data(capacity_);
        size_type constructed = 0;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            for (size_type i = 0; i < start; ++i, ++constructed)
                construct_value(new_data + constructed, std::move(data_[i]));
            for (size_type i = finish; i < size_; ++i, ++constructed)
                construct_value(new_data + constructed, std::move(data_[i]));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            while (constructed != 0) {
                --constructed;
                destroy_value(new_data + constructed);
            }
            deallocate_data(new_data, capacity_);
            throw;
        }
#endif
        for (size_type i = 0; i < size_; ++i) destroy_value(data_ + i);
        deallocate_data(data_, capacity_);
        data_ = new_data;
        size_ = new_size;
        return iterator(this, data_, start < new_size ? start : new_size);
    }

    template<typename T = value_type>
    typename enable_if<is_copy_constructible<T>::value, iterator>::type
    erase_range_dispatch(size_type start, size_type finish, true_type) {
        return erase_copy_range<T>(start, finish);
    }

    template<typename T = value_type>
    typename enable_if<!is_copy_constructible<T>::value, iterator>::type
    erase_range_dispatch(size_type start, size_type finish, false_type) {
        return erase_move_range(start, finish);
    }

    template<typename T = value_type>
    typename enable_if<is_copy_constructible<T>::value, size_type>::type
    erase_copy_key(const key_type& key) {
        size_type first = size_;
        size_type last = 0;
        for (size_type i = 0; i < size_; ++i) {
            if (!comp_(data_[i], key) && !comp_(key, data_[i])) {
                if (first == size_) first = i;
                last = i + 1;
            }
        }
        if (first == size_) return 0;
        erase_copy_range<T>(first, last);
        return last - first;
    }

    template<typename T = value_type>
    typename enable_if<!is_copy_constructible<T>::value, size_type>::type
    erase_move_key(const key_type& key) {
        /* The flat representation keeps equivalent keys contiguous.  First
         * locate the complete range without mutating storage, then reuse the
         * candidate prefix/suffix transaction used by iterator erase. */
        size_type first = size_;
        size_type last = 0;
        for (size_type i = 0; i < size_; ++i) {
            if (!comp_(data_[i], key) && !comp_(key, data_[i])) {
                if (first == size_) first = i;
                last = i + 1;
            }
        }
        if (first == size_) return 0;
        erase_move_range(first, last);
        return last - first;
    }

    template<typename T = value_type>
    typename enable_if<is_copy_constructible<T>::value, size_type>::type
    erase_key_dispatch(const key_type& key, true_type) {
        return erase_copy_key<T>(key);
    }

    template<typename T = value_type>
    typename enable_if<!is_copy_constructible<T>::value, size_type>::type
    erase_key_dispatch(const key_type& key, false_type) {
        return erase_move_key<T>(key);
    }

    template<typename Value>
    iterator insert_value(Value&& value) {
        const value_type* inserted =
            insert_value_dispatch(std::forward<Value>(value),
                                  is_copy_constructible<value_type>());
        return iterator(this, data_,
                        static_cast<size_type>(inserted - data_));
    }

    void relocate_values(multiset& source, multiset& target, true_type) {
        for (auto it = source.begin(); it != source.end(); ++it)
            target.insert(*it);
    }

    void relocate_values(multiset& source, multiset& target, false_type) {
        for (auto it = source.begin(); it != source.end(); ++it)
            target.insert(std::move(const_cast<value_type&>(*it)));
    }

      void swap_unequal(multiset& other) {
          if (other.size_ > max_size() || size_ > other.max_size())
              set_detail::length_failure();
          multiset left(other.comp_, alloc_);
        multiset right(comp_, other.alloc_);
        relocate_values(other, left, is_copy_constructible<value_type>());
        relocate_values(*this, right, is_copy_constructible<value_type>());

        /* Build both arrays before touching live storage.  Each replacement
         * is allocated by the allocator that will own it after publication. */
        set_detail::set_exchange(comp_, left.comp_);
        set_detail::set_exchange(other.comp_, right.comp_);
        clear();
        deallocate_data(data_, capacity_);
        other.clear();
        other.deallocate_data(other.data_, other.capacity_);
        data_ = left.data_;
        size_ = left.size_;
        capacity_ = left.capacity_;
        left.data_ = nullptr;
        left.size_ = left.capacity_ = 0;
        other.data_ = right.data_;
        other.size_ = right.size_;
        other.capacity_ = right.capacity_;
        right.data_ = nullptr;
        right.size_ = right.capacity_ = 0;
    }

public:
    iterator insert(const value_type& value) {
        return insert_value(value);
    }

    iterator insert(value_type&& value) {
        return insert_value(std::move(value));
    }

    node_type extract(const_iterator position) {
        size_type index = 0;
        if (!pointer_index(position, index) || index >= size_)
            return node_type();
        value_pointer allocation = alloc_traits::allocate(alloc_, 1);
        value_type* value = set_detail::pointer_address(allocation);
        if (allocation == value_pointer() || !value) {
            if (allocation != value_pointer())
                alloc_traits::deallocate(alloc_, allocation, 1);
            set_detail::allocation_failure();
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            alloc_traits::construct(alloc_, value, std::move(data_[index]));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            alloc_traits::deallocate(alloc_, allocation, 1);
            throw;
        }
#endif
        erase(position);
        return node_type(value, alloc_);
    }

    node_type extract(const key_type& key) {
        iterator position = find(key);
        return position == end() ? node_type() : extract(position);
    }

#if defined(RIN_SET_HAS_HETERO_INSERTION)
    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type,
             typename = typename enable_if<
                 is_constructible<key_type, K&&>::value>::type>
    node_type extract(const K& key) {
        iterator position = find(key);
        return position == end() ? node_type() : extract(position);
    }
#endif

    iterator insert(node_type&& node) {
        if (node.empty()) return end();
        iterator result = insert(std::move(node.value()));
        node.reset();
        return result;
    }

    iterator insert(const_iterator /*hint*/, node_type&& node) {
        return insert(std::move(node));
    }

    iterator insert(const_iterator hint, const value_type& value) {
        (void)hint;
        return insert(value);
    }

    iterator insert(const_iterator hint, value_type&& value) {
        (void)hint;
        return insert(std::move(value));
    }

    template<typename InputIt>
    void insert(InputIt first, InputIt last) {
        for (; first != last; ++first) insert(*first);
    }

    void insert(std::initializer_list<value_type> values) {
        insert(values.begin(), values.end());
    }

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, value_type>
    void insert_range(R&& range) {
        insert_range_impl(std::forward<R>(range),
                          integral_constant<bool,
                              is_copy_constructible<value_type>::value>());
    }
#endif

    template<typename... Args>
    iterator emplace(Args&&... args) {
        return insert(value_type(std::forward<Args>(args)...));
    }

    template<typename... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        (void)hint;
        return emplace(std::forward<Args>(args)...);
    }

    size_type count(const key_type& key) const {
        size_type c = 0;
        for (size_type i = 0; i < size_; ++i) {
            if (!comp_(data_[i], key) && !comp_(key, data_[i])) ++c;
        }
        return c;
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    size_type count(const K& key) const {
        size_type c = 0;
        for (size_type i = 0; i < size_; ++i) {
            if (!comp_(data_[i], key) && !comp_(key, data_[i])) ++c;
        }
        return c;
    }

    bool contains(const key_type& key) const {
        return find(key) != end();
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    bool contains(const K& key) const {
        return find(key) != end();
    }

    iterator find(const key_type& key) const {
        for (size_type i = 0; i < size_; ++i) {
            if (!comp_(data_[i], key) && !comp_(key, data_[i])) {
                return iterator(this, data_, i);
            }
        }
        return end();
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    iterator find(const K& key) const {
        for (size_type i = 0; i < size_; ++i) {
            if (!comp_(data_[i], key) && !comp_(key, data_[i])) {
                return iterator(this, data_, i);
            }
        }
        return end();
    }

    iterator lower_bound(const key_type& key) const {
        for (size_type i = 0; i < size_; ++i) {
            if (!comp_(data_[i], key)) return iterator(this, data_, i);
        }
        return end();
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    iterator lower_bound(const K& key) const {
        for (size_type i = 0; i < size_; ++i) {
            if (!comp_(data_[i], key)) return iterator(this, data_, i);
        }
        return end();
    }

    iterator upper_bound(const key_type& key) const {
        for (size_type i = 0; i < size_; ++i) {
            if (comp_(key, data_[i])) return iterator(this, data_, i);
        }
        return end();
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    iterator upper_bound(const K& key) const {
        for (size_type i = 0; i < size_; ++i) {
            if (comp_(key, data_[i])) return iterator(this, data_, i);
        }
        return end();
    }

    pair<iterator, iterator> equal_range(const key_type& key) const {
        return {lower_bound(key), upper_bound(key)};
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    pair<iterator, iterator> equal_range(const K& key) const {
        return {lower_bound(key), upper_bound(key)};
    }

    template<typename C2, typename A2>
    void merge(set<Key, C2, A2>& source) {
        if (static_cast<const void*>(this) == static_cast<const void*>(&source)) return;
        merge_set_impl(source, integral_constant<bool,
                        is_copy_constructible<value_type>::value>());
    }

    template<typename C2, typename A2>
    void merge(multiset<Key, C2, A2>& source) {
        if (static_cast<const void*>(this) == static_cast<const void*>(&source)) return;
        merge_multiset_impl(source, integral_constant<bool,
                             is_copy_constructible<value_type>::value>());
    }

    void swap(multiset& other) {
        if (this == &other) return;
        if (!alloc_traits::propagate_on_container_swap::value &&
            alloc_ != other.alloc_) {
            swap_unequal(other);
            return;
        }
        if (alloc_traits::propagate_on_container_swap::value)
            set_detail::set_exchange(alloc_, other.alloc_);
        set_detail::set_exchange(data_, other.data_);
        set_detail::set_exchange(size_, other.size_);
        set_detail::set_exchange(capacity_, other.capacity_);
        set_detail::set_exchange(comp_, other.comp_);
    }

    key_compare key_comp() const { return comp_; }
    value_compare value_comp() const { return comp_; }
    allocator_type get_allocator() const noexcept { return allocator_type(alloc_); }

    /* erase - イテレータで指定した要素を削除 */
    iterator erase(const_iterator pos) {
        size_type idx = 0;
        if (!pointer_index(pos, idx) || idx >= size_) return end();
        return erase_range_dispatch(idx, idx + 1,
                                    is_copy_constructible<value_type>());
    }

    /* erase - キーに一致する全要素を削除 */
    size_type erase(const key_type& key) {
        return erase_key_dispatch(key, is_copy_constructible<value_type>());
    }

    template<typename K, typename C = Compare,
             typename = typename set_detail::transparent_compare<C>::type>
    size_type erase(const K& key) {
        iterator it = lower_bound(key);
        size_type removed = 0;
        while (it != end() && !comp_(key, *it) && !comp_(*it, key)) {
            it = erase(it);
            ++removed;
        }
        return removed;
    }

    /* erase - 範囲削除 */
    iterator erase(const_iterator first, const_iterator last) {
        if (!data_) return end();
        size_type start_idx = 0;
        size_type end_idx = 0;
        if (!pointer_index(first, start_idx) ||
            !pointer_index(last, end_idx) || start_idx > end_idx) {
            return end();
        }
        if (start_idx == end_idx) return iterator(this, data_, start_idx);
        return erase_range_dispatch(start_idx, end_idx,
                                    is_copy_constructible<value_type>());
    }
};

template<typename Key, typename Compare, typename Allocator>
bool operator==(const multiset<Key, Compare, Allocator>& lhs,
                const multiset<Key, Compare, Allocator>& rhs) {
    if (lhs.size() != rhs.size()) return false;
    typename multiset<Key, Compare, Allocator>::const_iterator left = lhs.begin();
    typename multiset<Key, Compare, Allocator>::const_iterator right = rhs.begin();
    while (left != lhs.end()) {
        if (!(*left == *right)) return false;
        ++left;
        ++right;
    }
    return true;
}

template<typename Key, typename Compare, typename Allocator>
bool operator!=(const multiset<Key, Compare, Allocator>& lhs,
                const multiset<Key, Compare, Allocator>& rhs) {
    return !(lhs == rhs);
}

template<typename Key, typename Compare, typename Allocator>
void swap(multiset<Key, Compare, Allocator>& lhs,
          multiset<Key, Compare, Allocator>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

template<typename Key, typename Compare, typename Allocator>
bool operator<(const multiset<Key, Compare, Allocator>& lhs,
               const multiset<Key, Compare, Allocator>& rhs) {
    auto left = lhs.begin();
    auto right = rhs.begin();
    while (left != lhs.end() && right != rhs.end()) {
        if (*left < *right) return true;
        if (*right < *left) return false;
        ++left;
        ++right;
    }
    return left == lhs.end() && right != rhs.end();
}

template<typename Key, typename Compare, typename Allocator>
bool operator>(const multiset<Key, Compare, Allocator>& lhs,
               const multiset<Key, Compare, Allocator>& rhs) {
    return rhs < lhs;
}

template<typename Key, typename Compare, typename Allocator>
bool operator<=(const multiset<Key, Compare, Allocator>& lhs,
                const multiset<Key, Compare, Allocator>& rhs) {
    return !(rhs < lhs);
}

template<typename Key, typename Compare, typename Allocator>
bool operator>=(const multiset<Key, Compare, Allocator>& lhs,
                const multiset<Key, Compare, Allocator>& rhs) {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
template<typename Key, typename Compare, typename Allocator>
auto operator<=>(const multiset<Key, Compare, Allocator>& lhs,
                 const multiset<Key, Compare, Allocator>& rhs)
    -> detail::synth_three_way_result_t<Key> {
    using result_type = detail::synth_three_way_result_t<Key>;
    auto left = lhs.begin();
    auto right = rhs.begin();
    while (left != lhs.end() && right != rhs.end()) {
        const auto result = detail::synth_three_way(*left, *right);
        if (result != 0) return result;
        ++left;
        ++right;
    }
    if (left == lhs.end() && right != rhs.end()) return result_type::less;
    if (right == rhs.end() && left != lhs.end()) return result_type::greater;
    return result_type::equivalent;
}
#endif

#if __cplusplus >= 202002L
template<typename Key, typename Compare, typename Allocator>
typename set<Key, Compare, Allocator>::size_type
erase(set<Key, Compare, Allocator>& value, const Key& key) {
    return value.erase(key);
}

template<typename Key, typename Compare, typename Allocator, typename Pred>
typename set<Key, Compare, Allocator>::size_type
erase_if(set<Key, Compare, Allocator>& value, Pred predicate) {
    typename set<Key, Compare, Allocator>::size_type removed = 0u;
    for (typename set<Key, Compare, Allocator>::iterator it = value.begin();
         it != value.end();) {
        if (predicate(*it)) {
            it = value.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

template<typename Key, typename Compare, typename Allocator>
typename multiset<Key, Compare, Allocator>::size_type
erase(multiset<Key, Compare, Allocator>& value, const Key& key) {
    return value.erase(key);
}

template<typename Key, typename Compare, typename Allocator, typename K,
         typename = typename set_detail::transparent_compare<Compare>::type>
typename set<Key, Compare, Allocator>::size_type
erase(set<Key, Compare, Allocator>& value, const K& key) {
    return value.erase(key);
}

template<typename Key, typename Compare, typename Allocator, typename K,
         typename = typename set_detail::transparent_compare<Compare>::type>
typename multiset<Key, Compare, Allocator>::size_type
erase(multiset<Key, Compare, Allocator>& value, const K& key) {
    return value.erase(key);
}

template<typename Key, typename Compare, typename Allocator, typename Pred>
typename multiset<Key, Compare, Allocator>::size_type
erase_if(multiset<Key, Compare, Allocator>& value, Pred predicate) {
    typename multiset<Key, Compare, Allocator>::size_type removed = 0u;
    for (typename multiset<Key, Compare, Allocator>::iterator it = value.begin();
         it != value.end();) {
        if (predicate(*it)) {
            it = value.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}
#endif

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_SET_H */
