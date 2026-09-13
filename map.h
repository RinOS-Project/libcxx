/*
 * RinOS C++ <map>
 * Ordered map/multimap implementation backed by red-black trees.
 */

#ifndef RINCXX_MAP_H
#define RINCXX_MAP_H

#include "rincxx.h"
#include "functional.h"
#include "iterator.h"
#include "utility.h"
#include "initializer_list.h"
#include "stdexcept.h"
#include "exception.h"
#include "memory.h"
#include "vector.h"
#if __cplusplus >= 202002L
#include "compare.h"
#endif
#if __cplusplus > 202002L
#include "ranges.h"
#endif

/* P2363 heterogeneous insertion is a C++26 facility.  GCC 13 still reports
 * the C++23 working-draft value, so Rin exposes the tested surface only when
 * the final dialect is selected or the caller explicitly opts into the
 * preview. */
#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_ASSOCIATIVE_INSERTION)
#define RIN_MAP_HAS_HETERO_INSERTION 1
#endif

#ifdef __cplusplus

namespace std {

template<typename Key, typename T, typename Compare, typename Allocator>
class multimap;

namespace map_detail {

/* Keep map storage exchange independent from host std::swap overloads.  The
 * Rin compatibility layer and MinGW's <type_traits> may both contribute a
 * std::swap candidate, so the container uses an explicit value-semantic
 * three-move exchange at its publication boundaries. */
template<typename T>
inline void map_exchange(T& left, T& right) noexcept(
    noexcept(T(std::move(left))) &&
    noexcept(left = std::move(right)) &&
    noexcept(right = std::move(left))) {
    if (&left == &right) return;
    T temporary(std::move(left));
    left = std::move(right);
    right = std::move(temporary);
}
template<typename T, typename Allocator, typename = void>
struct effective_allocator {
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
    throw length_error("map exceeds max_size");
#else
    __builtin_trap();
#endif
}

/* Allocators may expose fancy pointers.  Tree links stay raw for compact
 * traversal, so normalize allocator results only at the boundary and rebuild
 * the allocator pointer when releasing storage. */
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
/* C++11--17 has no std::to_address, but allocator fancy pointers may expose
 * their raw address through pointer_traits alone.  Keep operator-> as a
 * compatibility fallback for older fancy-pointer implementations. */
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
} /* namespace map_detail */

template<typename Key, typename T, typename Compare = less<Key>, typename Allocator = void>
class map {
public:
    class iterator;
    class const_iterator;

    using key_type = Key;
    using mapped_type = T;
    using value_type = pair<const Key, T>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using key_compare = Compare;
    using allocator_type = typename map_detail::effective_allocator<
        value_type, Allocator>::type;
    using key_allocator_type = typename allocator_traits<allocator_type>::template
        rebind_alloc<key_type>;
    using mapped_allocator_type = typename allocator_traits<allocator_type>::template
        rebind_alloc<mapped_type>;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    class value_compare {
        Compare m_comp;

    public:
        explicit value_compare(const Compare& comp) : m_comp(comp) {}

        bool operator()(const value_type& left, const value_type& right) const {
            return m_comp(left.first, right.first);
        }
    };

    /* C++17 node handle.  A map value has a const key, so the detached
     * representation stores the key and mapped object separately.  This
     * keeps the key immutable while it is in the tree and still lets a
     * caller update a detached key before reinsertion. */
    class node_type {
        key_type* m_key;
        mapped_type* m_mapped;
        allocator_type m_alloc;
        using key_allocator_traits = allocator_traits<key_allocator_type>;
        using mapped_allocator_traits = allocator_traits<mapped_allocator_type>;

        node_type(key_type* key, mapped_type* mapped, const allocator_type& alloc)
            : m_key(key), m_mapped(mapped), m_alloc(alloc) {}
        friend class map;

        void reset() noexcept {
            if (m_key) {
                key_allocator_type key_alloc(m_alloc);
                key_allocator_traits::destroy(key_alloc, m_key);
                key_allocator_traits::deallocate(
                    key_alloc,
                    pointer_traits<typename key_allocator_traits::pointer>::pointer_to(
                        *m_key),
                    1);
                m_key = nullptr;
            }
            if (m_mapped) {
                mapped_allocator_type mapped_alloc(m_alloc);
                mapped_allocator_traits::destroy(mapped_alloc, m_mapped);
                mapped_allocator_traits::deallocate(
                    mapped_alloc,
                    pointer_traits<typename mapped_allocator_traits::pointer>::pointer_to(
                        *m_mapped),
                    1);
                m_mapped = nullptr;
            }
        }

    public:
        node_type() : m_key(nullptr), m_mapped(nullptr), m_alloc() {}
        node_type(node_type&& other) noexcept
            : m_key(other.m_key), m_mapped(other.m_mapped),
              m_alloc(std::move(other.m_alloc)) {
            other.m_key = nullptr;
            other.m_mapped = nullptr;
        }
        node_type& operator=(node_type&& other) noexcept {
            if (this != &other) {
                reset();
                m_key = other.m_key;
                m_mapped = other.m_mapped;
                m_alloc = std::move(other.m_alloc);
                other.m_key = nullptr;
                other.m_mapped = nullptr;
            }
            return *this;
        }
        node_type(const node_type&) = delete;
        node_type& operator=(const node_type&) = delete;
        ~node_type() { reset(); }

        bool empty() const noexcept { return m_key == nullptr; }
        allocator_type get_allocator() const noexcept { return m_alloc; }
        explicit operator bool() const noexcept { return !empty(); }
        key_type& key() const { return *m_key; }
        mapped_type& mapped() const { return *m_mapped; }
    };

private:
    enum class Color { Red, Black };

    struct Node {
        value_type data;
        Node* parent;
        Node* left;
        Node* right;
        Color color;

        template<typename KArg, typename VArg>
        Node(KArg&& key, VArg&& value, Node* p = nullptr)
            : data(std::forward<KArg>(key), std::forward<VArg>(value)),
              parent(p), left(nullptr), right(nullptr), color(Color::Red) {}

        Node* minimum() {
            Node* n = this;
            while (n->left) n = n->left;
            return n;
        }

        const Node* minimum() const {
            const Node* n = this;
            while (n->left) n = n->left;
            return n;
        }

        Node* maximum() {
            Node* n = this;
            while (n->right) n = n->right;
            return n;
        }

        const Node* maximum() const {
            const Node* n = this;
            while (n->right) n = n->right;
            return n;
        }

        Node* successor() {
            if (right) return right->minimum();
            Node* n = this;
            Node* p = parent;
            while (p && n == p->right) {
                n = p;
                p = p->parent;
            }
            return p;
        }

        const Node* successor() const {
            if (right) return right->minimum();
            const Node* n = this;
            const Node* p = parent;
            while (p && n == p->right) {
                n = p;
                p = p->parent;
            }
            return p;
        }

        Node* predecessor() {
            if (left) return left->maximum();
            Node* n = this;
            Node* p = parent;
            while (p && n == p->left) {
                n = p;
                p = p->parent;
            }
            return p;
        }

        const Node* predecessor() const {
            if (left) return left->maximum();
            const Node* n = this;
            const Node* p = parent;
            while (p && n == p->left) {
                n = p;
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
    Node* m_root;
    size_type m_size;
    Compare m_comp;

    template<typename KArg, typename VArg>
    Node* allocate_node(KArg&& key, VArg&& value, Node* parent) {
        node_pointer allocation = node_allocator_traits::allocate(m_alloc, 1);
        Node* node = map_detail::pointer_address(allocation);
        if (allocation == node_pointer() || !node) {
            if (allocation != node_pointer())
                node_allocator_traits::deallocate(m_alloc, allocation, 1);
            map_detail::allocation_failure();
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            node_allocator_traits::construct(
                m_alloc, node, std::forward<KArg>(key),
                std::forward<VArg>(value), parent);
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
            m_alloc, pointer_traits<node_pointer>::pointer_to(*node), 1);
    }

    void relocate_values(map& source, map& target, true_type) {
        for (auto it = source.begin(); it != source.end(); ++it)
            target.insert(*it);
    }

    void relocate_values(map& source, map& target, false_type) {
        for (auto it = source.begin(); it != source.end(); ++it)
            target.insert(std::move(*it));
    }

    void swap_unequal(map& other) {
        if (other.m_size > max_size() || m_size > other.max_size())
            map_detail::length_failure();
        map left(other.m_comp, m_alloc);
        map right(m_comp, other.m_alloc);
        relocate_values(other, left, is_copy_constructible<value_type>());
        relocate_values(*this, right, is_copy_constructible<value_type>());

        /* Candidate trees are allocator-owned before either live tree is
         * touched.  The allocator remains with its original container while
         * comparator state and completed roots exchange. */
        map_detail::map_exchange(m_comp, left.m_comp);
        map_detail::map_exchange(other.m_comp, right.m_comp);
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

    static bool key_equal(const key_type& a, const key_type& b, const Compare& comp) {
        return !comp(a, b) && !comp(b, a);
    }

    static Color node_color(Node* n) {
        return n ? n->color : Color::Black;
    }

    void rotate_left(Node* x) {
        Node* y = x->right;
        x->right = y->left;
        if (y->left) y->left->parent = x;
        y->parent = x->parent;
        if (!x->parent) {
            m_root = y;
        } else if (x == x->parent->left) {
            x->parent->left = y;
        } else {
            x->parent->right = y;
        }
        y->left = x;
        x->parent = y;
    }

    void rotate_right(Node* x) {
        Node* y = x->left;
        x->left = y->right;
        if (y->right) y->right->parent = x;
        y->parent = x->parent;
        if (!x->parent) {
            m_root = y;
        } else if (x == x->parent->right) {
            x->parent->right = y;
        } else {
            x->parent->left = y;
        }
        y->right = x;
        x->parent = y;
    }

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
        if (m_root) m_root->color = Color::Black;
    }

    void transplant(Node* u, Node* v) {
        if (!u->parent) {
            m_root = v;
        } else if (u == u->parent->left) {
            u->parent->left = v;
        } else {
            u->parent->right = v;
        }
        if (v) v->parent = u->parent;
    }

    void erase_fixup(Node* x, Node* x_parent) {
        while (x != m_root && node_color(x) == Color::Black) {
            if (!x_parent) break;
            if (x == x_parent->left) {
                Node* w = x_parent->right;
                if (node_color(w) == Color::Red) {
                    w->color = Color::Black;
                    x_parent->color = Color::Red;
                    rotate_left(x_parent);
                    w = x_parent->right;
                }
                if (node_color(w ? w->left : nullptr) == Color::Black &&
                    node_color(w ? w->right : nullptr) == Color::Black) {
                    if (w) w->color = Color::Red;
                    x = x_parent;
                    x_parent = x_parent->parent;
                } else {
                    if (node_color(w ? w->right : nullptr) == Color::Black) {
                        if (w && w->left) w->left->color = Color::Black;
                        if (w) {
                            w->color = Color::Red;
                            rotate_right(w);
                        }
                        w = x_parent->right;
                    }
                    if (w) w->color = x_parent->color;
                    x_parent->color = Color::Black;
                    if (w && w->right) w->right->color = Color::Black;
                    rotate_left(x_parent);
                    x = m_root;
                    x_parent = nullptr;
                }
            } else {
                Node* w = x_parent->left;
                if (node_color(w) == Color::Red) {
                    w->color = Color::Black;
                    x_parent->color = Color::Red;
                    rotate_right(x_parent);
                    w = x_parent->left;
                }
                if (node_color(w ? w->right : nullptr) == Color::Black &&
                    node_color(w ? w->left : nullptr) == Color::Black) {
                    if (w) w->color = Color::Red;
                    x = x_parent;
                    x_parent = x_parent->parent;
                } else {
                    if (node_color(w ? w->left : nullptr) == Color::Black) {
                        if (w && w->right) w->right->color = Color::Black;
                        if (w) {
                            w->color = Color::Red;
                            rotate_left(w);
                        }
                        w = x_parent->left;
                    }
                    if (w) w->color = x_parent->color;
                    x_parent->color = Color::Black;
                    if (w && w->left) w->left->color = Color::Black;
                    rotate_right(x_parent);
                    x = m_root;
                    x_parent = nullptr;
                }
            }
        }
        if (x) x->color = Color::Black;
    }

    void destroy_tree(Node* node) {
        if (!node) return;
        destroy_tree(node->left);
        destroy_tree(node->right);
        destroy_node(node);
    }

    Node* copy_tree(const Node* node, Node* parent) {
        if (!node) return nullptr;
        Node* n = allocate_node(node->data.first, node->data.second, parent);
        n->color = node->color;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
        n->left = copy_tree(node->left, n);
        n->right = copy_tree(node->right, n);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            /* Keep the partial subtree owned by this node until all child
             * construction has completed; then reclaim it on failure. */
            destroy_tree(n->left);
            destroy_tree(n->right);
            destroy_node(n);
            throw;
        }
#endif
        return n;
    }

    Node* find_node(const key_type& key) const {
        Node* current = m_root;
        while (current) {
            if (m_comp(key, current->data.first)) {
                current = current->left;
            } else if (m_comp(current->data.first, key)) {
                current = current->right;
            } else {
                return current;
            }
        }
        return nullptr;
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    Node* find_node(const K& key) const {
        Node* current = m_root;
        while (current) {
            if (m_comp(key, current->data.first)) {
                current = current->left;
            } else if (m_comp(current->data.first, key)) {
                current = current->right;
            } else {
                return current;
            }
        }
        return nullptr;
    }

    [[noreturn]] static void at_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        throw out_of_range("map::at: key not found");
#else
        __builtin_trap();
#endif
    }

    Node* lower_bound_node(const key_type& key) const {
        Node* current = m_root;
        Node* result = nullptr;
        while (current) {
            if (!m_comp(current->data.first, key)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        return result;
    }

    Node* upper_bound_node(const key_type& key) const {
        Node* current = m_root;
        Node* result = nullptr;
        while (current) {
            if (m_comp(key, current->data.first)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        return result;
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    Node* lower_bound_node(const K& key) const {
        Node* current = m_root;
        Node* result = nullptr;
        while (current) {
            if (!m_comp(current->data.first, key)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        return result;
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    Node* upper_bound_node(const K& key) const {
        Node* current = m_root;
        Node* result = nullptr;
        while (current) {
            if (m_comp(key, current->data.first)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        return result;
    }

    template<typename KArg, typename VArg>
    pair<typename map::iterator, bool> insert_unique_node(KArg&& key, VArg&& value) {
        Node* parent = nullptr;
        Node* current = m_root;
        bool insert_left = false;

        while (current) {
            parent = current;
            if (m_comp(key, current->data.first)) {
                insert_left = true;
                current = current->left;
            } else if (m_comp(current->data.first, key)) {
                insert_left = false;
                current = current->right;
            } else {
                return { iterator(current, this), false };
            }
        }

        if (m_size >= max_size())
            map_detail::length_failure();
        Node* n = allocate_node(std::forward<KArg>(key),
                                std::forward<VArg>(value), parent);
        if (!parent) {
            m_root = n;
        } else if (insert_left) {
            parent->left = n;
        } else {
            parent->right = n;
        }
        insert_fixup(n);
        ++m_size;
        return { iterator(n, this), true };
    }

    Node* erase_node(Node* z) {
        if (!z) return nullptr;

        Node* next = z->successor();
        Node* y = z;
        Color y_original_color = y->color;
        Node* x = nullptr;
        Node* x_parent = nullptr;

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
            if (y->parent == z) {
                x_parent = y;
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
        return next;
    }

    bool owns_node(const Node* node) const {
        if (!node) return false;
        const Node* root = node;
        while (root->parent) root = root->parent;
        return root == m_root;
    }

    /* Keep copyable mapped values in the source until destination insertion
     * has completed.  The prior extract/move ordering could leave a source
     * mapped value moved-from when a comparator or allocation threw. */
    template<typename C2, typename A2>
    void merge_map_impl(map<Key, T, C2, A2>& source, true_type) {
        vector<const value_type*> moved;
        map candidate(m_comp, m_alloc);
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
    void merge_map_impl(map<Key, T, C2, A2>& source, false_type) {
        vector<const value_type*> moved;
        map candidate(m_comp, m_alloc);
        for (Node* existing = m_root ? m_root->minimum() : nullptr;
             existing; existing = existing->successor())
            candidate.insert(value_type(existing->data.first,
                                        std::move(existing->data.second)));
        for (auto current = source.begin(); current != source.end(); ++current) {
            if (candidate.find(current->first) == candidate.end()) {
                candidate.insert(value_type(current->first,
                                            std::move(current->second)));
                moved.push_back(&*current);
            }
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
    void merge_multimap_impl(multimap<Key, T, C2, A2>& source,
                             true_type) {
        vector<const value_type*> moved;
        map candidate(m_comp, m_alloc);
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
    void merge_multimap_impl(multimap<Key, T, C2, A2>& source,
                             false_type) {
        vector<const value_type*> moved;
        map candidate(m_comp, m_alloc);
        for (Node* existing = m_root ? m_root->minimum() : nullptr;
             existing; existing = existing->successor())
            candidate.insert(value_type(existing->data.first,
                                        std::move(existing->data.second)));
        for (auto current = source.begin(); current != source.end(); ++current) {
            if (candidate.find(current->first) == candidate.end()) {
                candidate.insert(value_type(current->first,
                                            std::move(current->second)));
                moved.push_back(&*current);
            }
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

#if __cplusplus > 202002L
    /* Copyable map values are staged in a replacement tree so comparator,
     * allocation, and element-construction failures cannot expose a partial
     * C++23 range insertion.  Move-only values retain the direct path. */
    template<typename R>
    void insert_range_impl(R&& range, true_type) {
        map candidate(m_comp, m_alloc);
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
        /* Move-only mapped values are staged in an allocator-owned candidate
         * tree.  Existing keys stay in place while their mapped subobjects
         * move into the candidate; a later comparator/allocation/move failure
         * may leave already-moved mapped values unspecified (basic guarantee). */
        map candidate(m_comp, m_alloc);
        if (m_root) {
            for (Node* existing = m_root->minimum(); existing;
                 existing = existing->successor())
                candidate.insert(value_type(existing->data.first,
                                            std::move(existing->data.second)));
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
    class iterator {
        friend class map;
        friend class const_iterator;
        Node* m_node;
        const map* m_map;

    public:
        using iterator_category = bidirectional_iterator_tag;
        using value_type = map::value_type;
        using difference_type = ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        iterator() : m_node(nullptr), m_map(nullptr) {}
        iterator(Node* node, const map* owner) : m_node(node), m_map(owner) {}

        reference operator*() const { return m_node->data; }

        pointer operator->() const {
            return &(operator*());
        }

        iterator& operator++() {
            if (m_node) m_node = m_node->successor();
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        iterator& operator--() {
            if (!m_map) return *this;
            if (!m_node) {
                m_node = m_map->m_root ? m_map->m_root->maximum() : nullptr;
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
            return m_map == other.m_map && m_node == other.m_node;
        }
        bool operator!=(const iterator& other) const { return !(*this == other); }

        Node* node() const { return m_node; }
    };

    class const_iterator {
        friend class map;
        const Node* m_node;
        const map* m_map;

    public:
        using iterator_category = bidirectional_iterator_tag;
        using value_type = map::value_type;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        const_iterator() : m_node(nullptr), m_map(nullptr) {}
        const_iterator(const Node* node, const map* owner) : m_node(node), m_map(owner) {}
        const_iterator(const iterator& it) : m_node(it.m_node), m_map(it.m_map) {}

        reference operator*() const { return m_node->data; }

        pointer operator->() const {
            return &(operator*());
        }

        const_iterator& operator++() {
            if (m_node) m_node = m_node->successor();
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        const_iterator& operator--() {
            if (!m_map) return *this;
            if (!m_node) {
                m_node = m_map->m_root ? m_map->m_root->maximum() : nullptr;
            } else {
                m_node = m_node->predecessor();
            }
            return *this;
        }

        const_iterator operator--(int) {
            const_iterator tmp = *this;
            --(*this);
            return tmp;
        }

        bool operator==(const const_iterator& other) const {
            return m_map == other.m_map && m_node == other.m_node;
        }
        bool operator!=(const const_iterator& other) const { return !(*this == other); }

        friend bool operator==(const iterator& left,
                              const const_iterator& right) {
            return left.m_map == right.m_map && left.m_node == right.m_node;
        }
        friend bool operator==(const const_iterator& left,
                              const iterator& right) {
            return left.m_map == right.m_map && left.m_node == right.m_node;
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

    struct insert_return_type {
        iterator position;
        bool inserted;
        node_type node;
    };

    map() : m_alloc(), m_root(nullptr), m_size(0), m_comp() {}
    explicit map(const Compare& comp)
        : m_alloc(), m_root(nullptr), m_size(0), m_comp(comp) {}
    explicit map(const allocator_type& alloc)
        : m_alloc(alloc), m_root(nullptr), m_size(0), m_comp() {}
    map(const Compare& comp, const allocator_type& alloc)
        : m_alloc(alloc), m_root(nullptr), m_size(0), m_comp(comp) {}

    template<typename InputIt,
             typename enable_if<!is_integral<remove_cv_t<InputIt>>::value,
                                int>::type = 0>
    map(InputIt first, InputIt last, const Compare& comp = Compare(),
        const allocator_type& alloc = allocator_type())
        : m_alloc(alloc), m_root(nullptr), m_size(0), m_comp(comp) {
        insert(first, last);
    }

    map(std::initializer_list<value_type> init, const Compare& comp = Compare(),
        const allocator_type& alloc = allocator_type())
        : map(init.begin(), init.end(), comp, alloc) {}

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, value_type>
    map(from_range_t, R&& range, const Compare& comp = Compare(),
        const allocator_type& alloc = allocator_type())
        : map(comp, alloc) {
        insert_range(std::forward<R>(range));
    }
#endif

    map(const map& other)
        : m_alloc(allocator_traits<allocator_type>::
                      select_on_container_copy_construction(other.m_alloc)),
          m_root(nullptr), m_size(0),
          m_comp(other.m_comp) {
        if (other.m_size > max_size())
            map_detail::length_failure();
        m_root = copy_tree(other.m_root, nullptr);
        m_size = other.m_size;
    }

    map(const map& other, const allocator_type& alloc)
        : m_alloc(alloc), m_root(nullptr), m_size(0), m_comp(other.m_comp) {
        if (other.m_size > max_size())
            map_detail::length_failure();
        m_root = copy_tree(other.m_root, nullptr);
        m_size = other.m_size;
    }

    map(map&& other)
        noexcept(noexcept(Compare(std::move(other.m_comp))))
        : m_alloc(std::move(other.m_alloc)), m_root(other.m_root),
          m_size(other.m_size), m_comp(std::move(other.m_comp)) {
        other.m_root = nullptr;
        other.m_size = 0;
    }

    ~map() { clear(); }

    map& operator=(const map& other) {
        if (this != &other) {
            /* Copy the replacement before touching the destination. */
            const bool propagate = allocator_traits<allocator_type>::
                propagate_on_container_copy_assignment::value;
            map candidate(other, propagate ? other.m_alloc : m_alloc);
            if (propagate) {
                m_comp = candidate.m_comp;
                clear();
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

    map& operator=(map&& other)
        noexcept(allocator_traits<allocator_type>::
                     propagate_on_container_move_assignment::value &&
                 noexcept(m_comp = std::move(other.m_comp))) {
        if (this != &other) {
            using alloc_traits = allocator_traits<allocator_type>;
            if (alloc_traits::propagate_on_container_move_assignment::value) {
                clear();
                m_alloc = std::move(other.m_alloc);
                m_root = other.m_root;
                m_size = other.m_size;
                m_comp = std::move(other.m_comp);
                other.m_root = nullptr;
                other.m_size = 0;
            } else {
                if (other.m_size > max_size())
                    map_detail::length_failure();
                map candidate(other.m_comp, m_alloc);
                for (auto it = other.begin(); it != other.end(); ++it)
                    candidate.insert_unique_node(it->first,
                                                 std::move(it->second));
                clear();
                m_root = candidate.m_root;
                m_size = candidate.m_size;
                m_comp = std::move(candidate.m_comp);
                candidate.m_root = nullptr;
                candidate.m_size = 0;
                other.clear();
            }
        }
        return *this;
    }

    iterator begin() noexcept { return iterator(m_root ? m_root->minimum() : nullptr, this); }
    iterator end() noexcept { return iterator(nullptr, this); }
    const_iterator begin() const noexcept { return const_iterator(m_root ? m_root->minimum() : nullptr, this); }
    const_iterator end() const noexcept { return const_iterator(nullptr, this); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

    bool empty() const noexcept { return m_size == 0; }
    size_type size() const noexcept { return m_size; }
    size_type max_size() const noexcept {
        return node_allocator_traits::max_size(m_alloc);
    }

    mapped_type& operator[](const key_type& key) {
        Node* n = find_node(key);
        if (n) return n->data.second;
        return insert_unique_node(key, mapped_type()).first.node()->data.second;
    }

    mapped_type& operator[](key_type&& key) {
        Node* n = find_node(key);
        if (n) return n->data.second;
        return insert_unique_node(std::move(key), mapped_type()).first.node()->data.second;
    }

#if defined(RIN_MAP_HAS_HETERO_INSERTION)
    template<typename K, typename C = Compare,
             typename = typename map_detail::transparent_compare<C>::type,
             typename = typename enable_if<
                 is_constructible<key_type, K&&>::value>::type>
    mapped_type& operator[](K&& key) {
        Node* n = find_node(key);
        if (n) return n->data.second;
        return insert_unique_node(std::forward<K>(key), mapped_type())
            .first.node()->data.second;
    }
#endif

    mapped_type& at(const key_type& key) {
        Node* n = find_node(key);
        if (!n) at_failure();
        return n->data.second;
    }

    const mapped_type& at(const key_type& key) const {
        Node* n = find_node(key);
        if (!n) at_failure();
        return n->data.second;
    }

#if defined(RIN_MAP_HAS_HETERO_INSERTION)
    template<typename K, typename C = Compare,
             typename = typename map_detail::transparent_compare<C>::type,
             typename = typename enable_if<
                 is_constructible<key_type, K&&>::value>::type>
    mapped_type& at(K&& key) {
        Node* n = find_node(key);
        if (!n) at_failure();
        return n->data.second;
    }

    template<typename K, typename C = Compare,
             typename = typename map_detail::transparent_compare<C>::type,
             typename = typename enable_if<
                 is_constructible<key_type, K&&>::value>::type>
    const mapped_type& at(K&& key) const {
        Node* n = find_node(key);
        if (!n) at_failure();
        return n->data.second;
    }
#endif

    pair<iterator, bool> insert(const value_type& value) {
        return insert_unique_node(value.first, value.second);
    }

    pair<iterator, bool> insert(value_type&& value) {
        return insert_unique_node(value.first, std::move(value.second));
    }

    template<typename K2, typename V2,
             typename = typename enable_if<
                 is_constructible<key_type, K2>::value && is_constructible<mapped_type, V2>::value
             >::type>
    pair<iterator, bool> insert(const pair<K2, V2>& value) {
        return insert_unique_node(value.first, value.second);
    }

    template<typename K2, typename V2,
             typename = typename enable_if<
                 is_constructible<key_type, K2&&>::value && is_constructible<mapped_type, V2&&>::value
             >::type>
    pair<iterator, bool> insert(pair<K2, V2>&& value) {
        return insert_unique_node(std::forward<K2>(value.first), std::forward<V2>(value.second));
    }

    iterator insert(const_iterator /*hint*/, const value_type& value) {
        return insert(value).first;
    }

    iterator insert(const_iterator /*hint*/, value_type&& value) {
        return insert(std::move(value)).first;
    }

    template<typename InputIt>
    void insert(InputIt first, InputIt last) {
        for (; first != last; ++first) {
            insert(*first);
        }
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

    insert_return_type insert(node_type&& node) {
        if (node.empty()) return { end(), false, node_type() };
        /* Compare before moving the mapped object.  A duplicate therefore
         * leaves the handle fully intact, matching the standard contract. */
        auto result = insert_unique_node(*node.m_key, std::move(*node.m_mapped));
        if (result.second) {
            node.reset();
            return { result.first, true, node_type() };
        }
        return { result.first, false, std::move(node) };
    }

    iterator insert(const_iterator /*hint*/, node_type&& node) {
        if (node.empty()) return end();
        auto result = insert(std::move(node));
        if (!result.inserted && result.node) node = std::move(result.node);
        return result.position;
    }

    template<typename... Args>
    pair<iterator, bool> emplace(Args&&... args) {
        pair<key_type, mapped_type> tmp(std::forward<Args>(args)...);
        return insert_unique_node(std::move(tmp.first), std::move(tmp.second));
    }

    template<typename... Args>
    iterator emplace_hint(const_iterator /*hint*/, Args&&... args) {
        return emplace(std::forward<Args>(args)...).first;
    }

    template<typename... Args>
    pair<iterator, bool> try_emplace(const key_type& key, Args&&... args) {
        Node* n = find_node(key);
        if (n) return { iterator(n, this), false };
        return insert_unique_node(key, mapped_type(std::forward<Args>(args)...));
    }

    template<typename... Args>
    pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
        Node* n = find_node(key);
        if (n) return { iterator(n, this), false };
        return insert_unique_node(std::move(key), mapped_type(std::forward<Args>(args)...));
    }

#if defined(RIN_MAP_HAS_HETERO_INSERTION)
    template<typename K, typename... Args,
             typename C = Compare,
             typename = typename map_detail::transparent_compare<C>::type,
             typename = typename enable_if<
                 is_constructible<key_type, K&&>::value>::type>
    pair<iterator, bool> try_emplace(K&& key, Args&&... args) {
        /* Probe before constructing the mapped object.  Besides matching the
         * standard's no-effect duplicate path, this keeps a heterogeneous
         * key's conversion and a throwing mapped constructor out of the tree
         * mutation transaction. */
        Node* n = find_node(key);
        if (n) return { iterator(n, this), false };
        mapped_type mapped(std::forward<Args>(args)...);
        return insert_unique_node(std::forward<K>(key), std::move(mapped));
    }

    template<typename K, typename... Args,
             typename C = Compare,
             typename = typename map_detail::transparent_compare<C>::type,
             typename = typename enable_if<
                 is_constructible<key_type, K&&>::value>::type>
    iterator try_emplace(const_iterator /*hint*/, K&& key, Args&&... args) {
        return try_emplace(std::forward<K>(key),
                           std::forward<Args>(args)...).first;
    }
#endif

    /* insert_or_assign - C++17: insert a key or replace its mapped value */
    template<typename M>
    pair<iterator, bool> insert_or_assign(const key_type& key, M&& obj) {
        Node* n = find_node(key);
        if (n) {
            n->data.second = std::forward<M>(obj);
            return { iterator(n, this), false };
        }
        return insert_unique_node(key, mapped_type(std::forward<M>(obj)));
    }

    template<typename M>
    pair<iterator, bool> insert_or_assign(key_type&& key, M&& obj) {
        Node* n = find_node(key);
        if (n) {
            n->data.second = std::forward<M>(obj);
            return { iterator(n, this), false };
        }
        return insert_unique_node(std::move(key), mapped_type(std::forward<M>(obj)));
    }

#if defined(RIN_MAP_HAS_HETERO_INSERTION)
    template<typename K, typename M,
             typename C = Compare,
             typename = typename map_detail::transparent_compare<C>::type,
             typename = typename enable_if<
                 is_constructible<key_type, K&&>::value &&
                 is_constructible<mapped_type, M&&>::value>::type>
    pair<iterator, bool> insert_or_assign(K&& key, M&& obj) {
        Node* n = find_node(key);
        if (n) {
            n->data.second = std::forward<M>(obj);
            return { iterator(n, this), false };
        }
        mapped_type mapped(std::forward<M>(obj));
        return insert_unique_node(std::forward<K>(key), std::move(mapped));
    }

    template<typename K, typename M,
             typename C = Compare,
             typename = typename map_detail::transparent_compare<C>::type,
             typename = typename enable_if<
                 is_constructible<key_type, K&&>::value &&
                 is_constructible<mapped_type, M&&>::value>::type>
    iterator insert_or_assign(const_iterator /*hint*/, K&& key, M&& obj) {
        return insert_or_assign(std::forward<K>(key),
                                std::forward<M>(obj)).first;
    }
#endif

    template<typename M>
    iterator insert_or_assign(const_iterator /*hint*/, const key_type& key, M&& obj) {
        return insert_or_assign(key, std::forward<M>(obj)).first;
    }

    template<typename M>
    iterator insert_or_assign(const_iterator /*hint*/, key_type&& key, M&& obj) {
        return insert_or_assign(std::move(key), std::forward<M>(obj)).first;
    }

    iterator erase(iterator pos) {
        if ((!pos.m_node && pos.m_map != this) || !owns_node(pos.m_node))
            return end();
        Node* next = erase_node(pos.node());
        return iterator(next, this);
    }

    iterator erase(const_iterator pos) {
        if ((!pos.m_node && pos.m_map != this) || !owns_node(pos.m_node))
            return end();
        Node* next = erase_node(const_cast<Node*>(pos.m_node));
        return iterator(next, this);
    }

    iterator erase(iterator first, iterator last) {
        if ((!first.m_node && first.m_map != this) ||
            (!last.m_node && last.m_map != this) ||
            (first.m_node && !owns_node(first.m_node)) ||
            (last.m_node && !owns_node(last.m_node))) return end();
        while (first != last) {
            first = erase(first);
        }
        return last;
    }

    iterator erase(const_iterator first, const_iterator last) {
        if ((!first.m_node && first.m_map != this) ||
            (!last.m_node && last.m_map != this) ||
            (first.m_node && !owns_node(first.m_node)) ||
            (last.m_node && !owns_node(last.m_node))) return end();
        iterator it(const_cast<Node*>(first.m_node), this);
        iterator e(const_cast<Node*>(last.m_node), this);
        return erase(it, e);
    }

    size_type erase(const key_type& key) {
        Node* n = find_node(key);
        if (!n) return 0;
        erase_node(n);
        return 1;
    }

    template<typename K, typename C = Compare,
             typename = typename map_detail::transparent_compare<C>::type>
    size_type erase(const K& key) {
        Node* n = find_node(key);
        if (!n) return 0;
        erase_node(n);
        return 1;
    }

    node_type extract(const_iterator position) {
        if ((!position.m_node && position.m_map != this) ||
            !owns_node(position.m_node))
            return node_type();
        Node* node = const_cast<Node*>(position.m_node);
        key_allocator_type key_alloc(m_alloc);
        using key_allocator_traits = allocator_traits<key_allocator_type>;
        using key_pointer = typename key_allocator_traits::pointer;
        key_pointer key_allocation = key_allocator_traits::allocate(key_alloc, 1);
        key_type* key = map_detail::pointer_address(key_allocation);
        if (key_allocation == key_pointer() || !key) {
            if (key_allocation != key_pointer())
                key_allocator_traits::deallocate(key_alloc, key_allocation, 1);
            map_detail::allocation_failure();
        }
        mapped_allocator_type mapped_alloc(m_alloc);
        using mapped_allocator_traits = allocator_traits<mapped_allocator_type>;
        using mapped_pointer = typename mapped_allocator_traits::pointer;
        mapped_pointer mapped_allocation = mapped_pointer();
        mapped_type* mapped = nullptr;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        bool key_constructed = false;
        try {
#endif
            key_allocator_traits::construct(
                key_alloc, key, node->data.first);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            key_constructed = true;
#endif
            mapped_allocation = mapped_allocator_traits::allocate(mapped_alloc, 1);
            mapped = map_detail::pointer_address(mapped_allocation);
            if (mapped_allocation == mapped_pointer() || !mapped)
                map_detail::allocation_failure();
            mapped_allocator_traits::construct(
                mapped_alloc, mapped, std::move(node->data.second));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            if (mapped_allocation != mapped_pointer())
                mapped_allocator_traits::deallocate(
                    mapped_alloc, mapped_allocation, 1);
            if (key_constructed)
                key_allocator_traits::destroy(key_alloc, key);
            key_allocator_traits::deallocate(key_alloc, key_allocation, 1);
            throw;
        }
#endif
        erase_node(node);
        return node_type(key, mapped, allocator_type(m_alloc));
    }

    node_type extract(const key_type& key) {
        iterator position = find(key);
        return position == end() ? node_type() : extract(position);
    }

#if defined(RIN_MAP_HAS_HETERO_INSERTION)
    template<typename K, typename C = Compare,
             typename = typename map_detail::transparent_compare<C>::type,
             typename = typename enable_if<
                 is_constructible<key_type, K&&>::value>::type>
    node_type extract(const K& key) {
        iterator position = find(key);
        return position == end() ? node_type() : extract(position);
    }
#endif

    void clear() {
        destroy_tree(m_root);
        m_root = nullptr;
        m_size = 0;
    }

    void swap(map& other) {
        if (this == &other) return;
        if (!allocator_traits<allocator_type>::propagate_on_container_swap::value &&
            m_alloc != other.m_alloc) {
            swap_unequal(other);
            return;
        }
        map_detail::map_exchange(m_root, other.m_root);
        map_detail::map_exchange(m_size, other.m_size);
        map_detail::map_exchange(m_comp, other.m_comp);
        if (allocator_traits<allocator_type>::
                propagate_on_container_swap::value) {
            map_detail::map_exchange(m_alloc, other.m_alloc);
        }
    }

    allocator_type get_allocator() const noexcept { return allocator_type(m_alloc); }

    iterator find(const key_type& key) {
        return iterator(find_node(key), this);
    }

    const_iterator find(const key_type& key) const {
        return const_iterator(find_node(key), this);
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    iterator find(const K& key) {
        return iterator(find_node(key), this);
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    const_iterator find(const K& key) const {
        return const_iterator(find_node(key), this);
    }

    size_type count(const key_type& key) const {
        return find_node(key) ? 1 : 0;
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    size_type count(const K& key) const {
        return find_node(key) ? 1 : 0;
    }

    bool contains(const key_type& key) const {
        return find_node(key) != nullptr;
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    bool contains(const K& key) const {
        return find_node(key) != nullptr;
    }

    iterator lower_bound(const key_type& key) {
        return iterator(lower_bound_node(key), this);
    }

    const_iterator lower_bound(const key_type& key) const {
        return const_iterator(lower_bound_node(key), this);
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    iterator lower_bound(const K& key) {
        return iterator(lower_bound_node(key), this);
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    const_iterator lower_bound(const K& key) const {
        return const_iterator(lower_bound_node(key), this);
    }

    iterator upper_bound(const key_type& key) {
        return iterator(upper_bound_node(key), this);
    }

    const_iterator upper_bound(const key_type& key) const {
        return const_iterator(upper_bound_node(key), this);
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    iterator upper_bound(const K& key) {
        return iterator(upper_bound_node(key), this);
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    const_iterator upper_bound(const K& key) const {
        return const_iterator(upper_bound_node(key), this);
    }

    pair<iterator, iterator> equal_range(const key_type& key) {
        return { lower_bound(key), upper_bound(key) };
    }

    pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
        return { lower_bound(key), upper_bound(key) };
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    pair<iterator, iterator> equal_range(const K& key) {
        return { lower_bound(key), upper_bound(key) };
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    pair<const_iterator, const_iterator> equal_range(const K& key) const {
        return { lower_bound(key), upper_bound(key) };
    }

    template<typename C2, typename A2>
    void merge(map<Key, T, C2, A2>& source) {
        if (static_cast<const void*>(this) == static_cast<const void*>(&source)) return;
        merge_map_impl(source, integral_constant<bool,
                        is_copy_constructible<value_type>::value>());
    }

    template<typename C2, typename A2>
    void merge(multimap<Key, T, C2, A2>& source) {
        if (static_cast<const void*>(this) == static_cast<const void*>(&source)) return;
        merge_multimap_impl(source, integral_constant<bool,
                             is_copy_constructible<value_type>::value>());
    }

    key_compare key_comp() const { return m_comp; }
    value_compare value_comp() const { return value_compare(m_comp); }
};

template<typename Key, typename T, typename Compare, typename Allocator>
bool operator==(const map<Key, T, Compare, Allocator>& lhs,
                const map<Key, T, Compare, Allocator>& rhs) {
    if (lhs.size() != rhs.size()) return false;
    auto it1 = lhs.begin();
    auto it2 = rhs.begin();
    while (it1 != lhs.end()) {
        if (it1->first != it2->first || it1->second != it2->second) return false;
        ++it1;
        ++it2;
    }
    return true;
}

template<typename Key, typename T, typename Compare, typename Allocator>
bool operator!=(const map<Key, T, Compare, Allocator>& lhs,
                const map<Key, T, Compare, Allocator>& rhs) {
    return !(lhs == rhs);
}

template<typename Key, typename T, typename Compare, typename Allocator>
void swap(map<Key, T, Compare, Allocator>& lhs,
          map<Key, T, Compare, Allocator>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

template<typename Key, typename T, typename Compare, typename Allocator>
bool operator<(const map<Key, T, Compare, Allocator>& lhs,
               const map<Key, T, Compare, Allocator>& rhs) {
    auto left = lhs.begin();
    auto right = rhs.begin();
    while (left != lhs.end() && right != rhs.end()) {
        if (left->first < right->first) return true;
        if (right->first < left->first) return false;
        if (left->second < right->second) return true;
        if (right->second < left->second) return false;
        ++left;
        ++right;
    }
    return left == lhs.end() && right != rhs.end();
}

template<typename Key, typename T, typename Compare, typename Allocator>
bool operator>(const map<Key, T, Compare, Allocator>& lhs,
               const map<Key, T, Compare, Allocator>& rhs) {
    return rhs < lhs;
}

template<typename Key, typename T, typename Compare, typename Allocator>
bool operator<=(const map<Key, T, Compare, Allocator>& lhs,
                const map<Key, T, Compare, Allocator>& rhs) {
    return !(rhs < lhs);
}

template<typename Key, typename T, typename Compare, typename Allocator>
bool operator>=(const map<Key, T, Compare, Allocator>& lhs,
                const map<Key, T, Compare, Allocator>& rhs) {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
template<typename Key, typename T, typename Compare, typename Allocator>
auto operator<=>(const map<Key, T, Compare, Allocator>& lhs,
                 const map<Key, T, Compare, Allocator>& rhs)
    -> detail::synth_three_way_result_t<typename map<Key, T, Compare, Allocator>::value_type> {
    using result_type = detail::synth_three_way_result_t<
        typename map<Key, T, Compare, Allocator>::value_type>;
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

template<typename Key, typename T, typename Compare = less<Key>, typename Allocator = void>
class multimap {
public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = pair<const Key, T>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using key_compare = Compare;
    using allocator_type = typename map_detail::effective_allocator<
        value_type, Allocator>::type;
    using key_allocator_type = typename allocator_traits<allocator_type>::template
        rebind_alloc<key_type>;
    using mapped_allocator_type = typename allocator_traits<allocator_type>::template
        rebind_alloc<mapped_type>;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    class value_compare {
        Compare m_comp;

    public:
        explicit value_compare(const Compare& comp) : m_comp(comp) {}

        bool operator()(const value_type& left, const value_type& right) const {
            return m_comp(left.first, right.first);
        }
    };

    /* C++17 node handle; see map::node_type for why key and mapped value are
     * owned independently while detached from the tree. */
    class node_type {
        key_type* m_key;
        mapped_type* m_mapped;
        allocator_type m_alloc;
        using key_allocator_traits = allocator_traits<key_allocator_type>;
        using mapped_allocator_traits = allocator_traits<mapped_allocator_type>;

        node_type(key_type* key, mapped_type* mapped, const allocator_type& alloc)
            : m_key(key), m_mapped(mapped), m_alloc(alloc) {}
        friend class multimap;

        void reset() noexcept {
            if (m_key) {
                key_allocator_type key_alloc(m_alloc);
                key_allocator_traits::destroy(key_alloc, m_key);
                key_allocator_traits::deallocate(
                    key_alloc,
                    pointer_traits<typename key_allocator_traits::pointer>::pointer_to(
                        *m_key),
                    1);
                m_key = nullptr;
            }
            if (m_mapped) {
                mapped_allocator_type mapped_alloc(m_alloc);
                mapped_allocator_traits::destroy(mapped_alloc, m_mapped);
                mapped_allocator_traits::deallocate(
                    mapped_alloc,
                    pointer_traits<typename mapped_allocator_traits::pointer>::pointer_to(
                        *m_mapped),
                    1);
                m_mapped = nullptr;
            }
        }

    public:
        node_type() : m_key(nullptr), m_mapped(nullptr), m_alloc() {}
        node_type(node_type&& other) noexcept
            : m_key(other.m_key), m_mapped(other.m_mapped),
              m_alloc(std::move(other.m_alloc)) {
            other.m_key = nullptr;
            other.m_mapped = nullptr;
        }
        node_type& operator=(node_type&& other) noexcept {
            if (this != &other) {
                reset();
                m_key = other.m_key;
                m_mapped = other.m_mapped;
                m_alloc = std::move(other.m_alloc);
                other.m_key = nullptr;
                other.m_mapped = nullptr;
            }
            return *this;
        }
        node_type(const node_type&) = delete;
        node_type& operator=(const node_type&) = delete;
        ~node_type() { reset(); }

        bool empty() const noexcept { return m_key == nullptr; }
        allocator_type get_allocator() const noexcept { return m_alloc; }
        explicit operator bool() const noexcept { return !empty(); }
        key_type& key() const { return *m_key; }
        mapped_type& mapped() const { return *m_mapped; }
    };

private:
    enum class Color { Red, Black };

    struct Node {
        value_type data;
        Node* parent;
        Node* left;
        Node* right;
        Color color;

        template<typename KArg, typename VArg>
        Node(KArg&& key, VArg&& value, Node* p = nullptr)
            : data(std::forward<KArg>(key), std::forward<VArg>(value)),
              parent(p), left(nullptr), right(nullptr), color(Color::Red) {}

        Node* minimum() {
            Node* n = this;
            while (n->left) n = n->left;
            return n;
        }

        const Node* minimum() const {
            const Node* n = this;
            while (n->left) n = n->left;
            return n;
        }

        Node* maximum() {
            Node* n = this;
            while (n->right) n = n->right;
            return n;
        }

        const Node* maximum() const {
            const Node* n = this;
            while (n->right) n = n->right;
            return n;
        }

        Node* successor() {
            if (right) return right->minimum();
            Node* n = this;
            Node* p = parent;
            while (p && n == p->right) {
                n = p;
                p = p->parent;
            }
            return p;
        }

        const Node* successor() const {
            if (right) return right->minimum();
            const Node* n = this;
            const Node* p = parent;
            while (p && n == p->right) {
                n = p;
                p = p->parent;
            }
            return p;
        }

        Node* predecessor() {
            if (left) return left->maximum();
            Node* n = this;
            Node* p = parent;
            while (p && n == p->left) {
                n = p;
                p = p->parent;
            }
            return p;
        }

        const Node* predecessor() const {
            if (left) return left->maximum();
            const Node* n = this;
            const Node* p = parent;
            while (p && n == p->left) {
                n = p;
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
    Node* m_root;
    size_type m_size;
    Compare m_comp;

    template<typename KArg, typename VArg>
    Node* allocate_node(KArg&& key, VArg&& value, Node* parent) {
        node_pointer allocation = node_allocator_traits::allocate(m_alloc, 1);
        Node* node = map_detail::pointer_address(allocation);
        if (allocation == node_pointer() || !node) {
            if (allocation != node_pointer())
                node_allocator_traits::deallocate(m_alloc, allocation, 1);
            map_detail::allocation_failure();
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            node_allocator_traits::construct(
                m_alloc, node, std::forward<KArg>(key),
                std::forward<VArg>(value), parent);
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
            m_alloc, pointer_traits<node_pointer>::pointer_to(*node), 1);
    }

    void relocate_values(multimap& source, multimap& target, true_type) {
        for (auto it = source.begin(); it != source.end(); ++it)
            target.insert(*it);
    }

    void relocate_move_nodes(Node* node, multimap& target) {
        if (!node) return;
        relocate_move_nodes(node->left, target);
        target.insert(value_type(node->data.first,
                                 std::move(node->data.second)));
        relocate_move_nodes(node->right, target);
    }

    void relocate_values(multimap& source, multimap& target, false_type) {
        relocate_move_nodes(source.m_root, target);
    }

    void swap_unequal(multimap& other) {
        if (other.m_size > max_size() || m_size > other.max_size())
            map_detail::length_failure();
        multimap left(other.m_comp, m_alloc);
        multimap right(m_comp, other.m_alloc);
        relocate_values(other, left, is_copy_constructible<value_type>());
        relocate_values(*this, right, is_copy_constructible<value_type>());

        /* Publish only completed allocator-owned replacement trees. */
        map_detail::map_exchange(m_comp, left.m_comp);
        map_detail::map_exchange(other.m_comp, right.m_comp);
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

    static Color node_color(Node* n) {
        return n ? n->color : Color::Black;
    }

    void rotate_left(Node* x) {
        Node* y = x->right;
        x->right = y->left;
        if (y->left) y->left->parent = x;
        y->parent = x->parent;
        if (!x->parent) {
            m_root = y;
        } else if (x == x->parent->left) {
            x->parent->left = y;
        } else {
            x->parent->right = y;
        }
        y->left = x;
        x->parent = y;
    }

    void rotate_right(Node* x) {
        Node* y = x->left;
        x->left = y->right;
        if (y->right) y->right->parent = x;
        y->parent = x->parent;
        if (!x->parent) {
            m_root = y;
        } else if (x == x->parent->right) {
            x->parent->right = y;
        } else {
            x->parent->left = y;
        }
        y->right = x;
        x->parent = y;
    }

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
        if (m_root) m_root->color = Color::Black;
    }

    void transplant(Node* u, Node* v) {
        if (!u->parent) {
            m_root = v;
        } else if (u == u->parent->left) {
            u->parent->left = v;
        } else {
            u->parent->right = v;
        }
        if (v) v->parent = u->parent;
    }

    void erase_fixup(Node* x, Node* x_parent) {
        while (x != m_root && node_color(x) == Color::Black) {
            if (!x_parent) break;
            if (x == x_parent->left) {
                Node* w = x_parent->right;
                if (node_color(w) == Color::Red) {
                    w->color = Color::Black;
                    x_parent->color = Color::Red;
                    rotate_left(x_parent);
                    w = x_parent->right;
                }
                if (node_color(w ? w->left : nullptr) == Color::Black &&
                    node_color(w ? w->right : nullptr) == Color::Black) {
                    if (w) w->color = Color::Red;
                    x = x_parent;
                    x_parent = x_parent->parent;
                } else {
                    if (node_color(w ? w->right : nullptr) == Color::Black) {
                        if (w && w->left) w->left->color = Color::Black;
                        if (w) {
                            w->color = Color::Red;
                            rotate_right(w);
                        }
                        w = x_parent->right;
                    }
                    if (w) w->color = x_parent->color;
                    x_parent->color = Color::Black;
                    if (w && w->right) w->right->color = Color::Black;
                    rotate_left(x_parent);
                    x = m_root;
                    x_parent = nullptr;
                }
            } else {
                Node* w = x_parent->left;
                if (node_color(w) == Color::Red) {
                    w->color = Color::Black;
                    x_parent->color = Color::Red;
                    rotate_right(x_parent);
                    w = x_parent->left;
                }
                if (node_color(w ? w->right : nullptr) == Color::Black &&
                    node_color(w ? w->left : nullptr) == Color::Black) {
                    if (w) w->color = Color::Red;
                    x = x_parent;
                    x_parent = x_parent->parent;
                } else {
                    if (node_color(w ? w->left : nullptr) == Color::Black) {
                        if (w && w->right) w->right->color = Color::Black;
                        if (w) {
                            w->color = Color::Red;
                            rotate_left(w);
                        }
                        w = x_parent->left;
                    }
                    if (w) w->color = x_parent->color;
                    x_parent->color = Color::Black;
                    if (w && w->left) w->left->color = Color::Black;
                    rotate_right(x_parent);
                    x = m_root;
                    x_parent = nullptr;
                }
            }
        }
        if (x) x->color = Color::Black;
    }

    void destroy_tree(Node* node) {
        if (!node) return;
        destroy_tree(node->left);
        destroy_tree(node->right);
        destroy_node(node);
    }

    Node* copy_tree(const Node* node, Node* parent) {
        if (!node) return nullptr;
        Node* n = allocate_node(node->data.first, node->data.second, parent);
        n->color = node->color;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
        n->left = copy_tree(node->left, n);
        n->right = copy_tree(node->right, n);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            destroy_tree(n->left);
            destroy_tree(n->right);
            destroy_node(n);
            throw;
        }
#endif
        return n;
    }

    Node* lower_bound_node(const key_type& key) const {
        Node* current = m_root;
        Node* result = nullptr;
        while (current) {
            if (!m_comp(current->data.first, key)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        return result;
    }

    Node* upper_bound_node(const key_type& key) const {
        Node* current = m_root;
        Node* result = nullptr;
        while (current) {
            if (m_comp(key, current->data.first)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        return result;
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    Node* lower_bound_node(const K& key) const {
        Node* current = m_root;
        Node* result = nullptr;
        while (current) {
            if (!m_comp(current->data.first, key)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        return result;
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    Node* upper_bound_node(const K& key) const {
        Node* current = m_root;
        Node* result = nullptr;
        while (current) {
            if (m_comp(key, current->data.first)) {
                result = current;
                current = current->left;
            } else {
                current = current->right;
            }
        }
        return result;
    }

    template<typename KArg, typename VArg>
    Node* insert_multi_node(KArg&& key, VArg&& value) {
        Node* parent = nullptr;
        Node* current = m_root;
        bool insert_left = false;
        while (current) {
            parent = current;
            if (m_comp(key, current->data.first)) {
                insert_left = true;
                current = current->left;
            } else {
                insert_left = false;
                current = current->right;
            }
        }

        if (m_size >= max_size())
            map_detail::length_failure();
        Node* n = allocate_node(std::forward<KArg>(key),
                                std::forward<VArg>(value), parent);
        if (!parent) {
            m_root = n;
        } else if (insert_left) {
            parent->left = n;
        } else {
            parent->right = n;
        }
        insert_fixup(n);
        ++m_size;
        return n;
    }

    Node* erase_node(Node* z) {
        if (!z) return nullptr;

        Node* next = z->successor();
        Node* y = z;
        Color y_original_color = y->color;
        Node* x = nullptr;
        Node* x_parent = nullptr;

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
            if (y->parent == z) {
                x_parent = y;
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
        return next;
    }

    bool owns_node(const Node* node) const {
        if (!node) return false;
        const Node* root = node;
        while (root->parent) root = root->parent;
        return root == m_root;
    }

    /* Copyable map values are published in the destination before source
     * erasure, so comparator/allocation/element construction failure leaves
     * both containers unchanged.  Move-only values retain extraction order. */
    template<typename C2, typename A2>
    void merge_map_impl(map<Key, T, C2, A2>& source, true_type) {
        multimap candidate(m_comp, m_alloc);
        for (auto existing = begin(); existing != end(); ++existing)
            candidate.insert(*existing);
        for (auto current = source.begin(); current != source.end(); ++current)
            candidate.insert(*current);
        source.clear();

        Node* old_root = m_root;
        size_type old_size = m_size;
        m_root = candidate.m_root;
        m_size = candidate.m_size;
        candidate.m_root = old_root;
        candidate.m_size = old_size;
    }

    template<typename C2, typename A2>
    void merge_map_impl(map<Key, T, C2, A2>& source, false_type) {
        multimap candidate(m_comp, m_alloc);
        for (Node* existing = m_root ? m_root->minimum() : nullptr;
             existing; existing = existing->successor())
            candidate.insert(value_type(existing->data.first,
                                        std::move(existing->data.second)));
        for (auto current = source.begin(); current != source.end(); ++current)
            candidate.insert(value_type(current->first,
                                        std::move(current->second)));
        source.clear();

        Node* old_root = m_root;
        size_type old_size = m_size;
        m_root = candidate.m_root;
        m_size = candidate.m_size;
        candidate.m_root = old_root;
        candidate.m_size = old_size;
    }

    template<typename C2, typename A2>
    void merge_multimap_impl(multimap<Key, T, C2, A2>& source,
                             true_type) {
        multimap candidate(m_comp, m_alloc);
        for (auto existing = begin(); existing != end(); ++existing)
            candidate.insert(*existing);
        for (auto current = source.begin(); current != source.end(); ++current)
            candidate.insert(*current);
        source.clear();

        Node* old_root = m_root;
        size_type old_size = m_size;
        m_root = candidate.m_root;
        m_size = candidate.m_size;
        candidate.m_root = old_root;
        candidate.m_size = old_size;
    }

    template<typename C2, typename A2>
    void merge_multimap_impl(multimap<Key, T, C2, A2>& source,
                             false_type) {
        multimap candidate(m_comp, m_alloc);
        for (Node* existing = m_root ? m_root->minimum() : nullptr;
             existing; existing = existing->successor())
            candidate.insert(value_type(existing->data.first,
                                        std::move(existing->data.second)));
        for (auto current = source.begin(); current != source.end(); ++current)
            candidate.insert(value_type(current->first,
                                        std::move(current->second)));
        source.clear();

        Node* old_root = m_root;
        size_type old_size = m_size;
        m_root = candidate.m_root;
        m_size = candidate.m_size;
        candidate.m_root = old_root;
        candidate.m_size = old_size;
    }

#if __cplusplus > 202002L
    /* Multimap uses the same destination-owned candidate tree, preserving
     * duplicate keys while keeping copyable range insertion failure-atomic. */
    template<typename R>
    void insert_range_impl(R&& range, true_type) {
        multimap candidate(m_comp, m_alloc);
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
        /* The multimap keeps every equivalent key while replacing the live
         * tree only after existing pairs and the rvalue range are staged. */
        multimap candidate(m_comp, m_alloc);
        if (m_root) {
            for (Node* existing = m_root->minimum(); existing;
                 existing = existing->successor())
                candidate.insert(value_type(existing->data.first,
                                            std::move(existing->data.second)));
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
    class const_iterator;

    class iterator {
        friend class multimap;
        friend class const_iterator;
        Node* m_node;
        const multimap* m_map;

    public:
        using iterator_category = bidirectional_iterator_tag;
        using value_type = multimap::value_type;
        using difference_type = ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        iterator() : m_node(nullptr), m_map(nullptr) {}
        iterator(Node* node, const multimap* owner) : m_node(node), m_map(owner) {}

        reference operator*() const { return m_node->data; }

        pointer operator->() const {
            return &(operator*());
        }

        iterator& operator++() {
            if (m_node) m_node = m_node->successor();
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        iterator& operator--() {
            if (!m_map) return *this;
            if (!m_node) {
                m_node = m_map->m_root ? m_map->m_root->maximum() : nullptr;
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
            return m_map == other.m_map && m_node == other.m_node;
        }
        bool operator!=(const iterator& other) const { return !(*this == other); }
    };

    class const_iterator {
        friend class multimap;
        const Node* m_node;
        const multimap* m_map;

    public:
        using iterator_category = bidirectional_iterator_tag;
        using value_type = multimap::value_type;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        const_iterator() : m_node(nullptr), m_map(nullptr) {}
        const_iterator(const Node* node, const multimap* owner) : m_node(node), m_map(owner) {}
        const_iterator(const iterator& it) : m_node(it.m_node), m_map(it.m_map) {}

        reference operator*() const { return m_node->data; }

        pointer operator->() const {
            return &(operator*());
        }

        const_iterator& operator++() {
            if (m_node) m_node = m_node->successor();
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        const_iterator& operator--() {
            if (!m_map) return *this;
            if (!m_node) {
                m_node = m_map->m_root ? m_map->m_root->maximum() : nullptr;
            } else {
                m_node = m_node->predecessor();
            }
            return *this;
        }

        const_iterator operator--(int) {
            const_iterator tmp = *this;
            --(*this);
            return tmp;
        }

        bool operator==(const const_iterator& other) const {
            return m_map == other.m_map && m_node == other.m_node;
        }
        bool operator!=(const const_iterator& other) const { return !(*this == other); }

        friend bool operator==(const iterator& left,
                              const const_iterator& right) {
            return left.m_map == right.m_map && left.m_node == right.m_node;
        }
        friend bool operator==(const const_iterator& left,
                              const iterator& right) {
            return left.m_map == right.m_map && left.m_node == right.m_node;
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

    multimap() : m_alloc(), m_root(nullptr), m_size(0), m_comp() {}
    explicit multimap(const Compare& comp)
        : m_alloc(), m_root(nullptr), m_size(0), m_comp(comp) {}
    explicit multimap(const allocator_type& alloc)
        : m_alloc(alloc), m_root(nullptr), m_size(0), m_comp() {}
    multimap(const Compare& comp, const allocator_type& alloc)
        : m_alloc(alloc), m_root(nullptr), m_size(0), m_comp(comp) {}

    template<typename InputIt,
             typename enable_if<!is_integral<remove_cv_t<InputIt>>::value,
                                int>::type = 0>
    multimap(InputIt first, InputIt last, const Compare& comp = Compare(),
             const allocator_type& alloc = allocator_type())
        : m_alloc(alloc), m_root(nullptr), m_size(0), m_comp(comp) {
        insert(first, last);
    }

    multimap(std::initializer_list<value_type> init,
             const Compare& comp = Compare(),
             const allocator_type& alloc = allocator_type())
        : multimap(init.begin(), init.end(), comp, alloc) {}

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, value_type>
    multimap(from_range_t, R&& range, const Compare& comp = Compare(),
             const allocator_type& alloc = allocator_type())
        : multimap(comp, alloc) {
        insert_range(std::forward<R>(range));
    }
#endif

    multimap(const multimap& other)
        : m_alloc(allocator_traits<allocator_type>::
                      select_on_container_copy_construction(other.m_alloc)),
          m_root(nullptr), m_size(0),
          m_comp(other.m_comp) {
        if (other.m_size > max_size())
            map_detail::length_failure();
        m_root = copy_tree(other.m_root, nullptr);
        m_size = other.m_size;
    }

    multimap(const multimap& other, const allocator_type& alloc)
        : m_alloc(alloc), m_root(nullptr), m_size(0), m_comp(other.m_comp) {
        if (other.m_size > max_size())
            map_detail::length_failure();
        m_root = copy_tree(other.m_root, nullptr);
        m_size = other.m_size;
    }

    multimap(multimap&& other)
        noexcept(noexcept(Compare(std::move(other.m_comp))))
        : m_alloc(std::move(other.m_alloc)), m_root(other.m_root),
          m_size(other.m_size), m_comp(std::move(other.m_comp)) {
        other.m_root = nullptr;
        other.m_size = 0;
    }

    ~multimap() { clear(); }

    multimap& operator=(const multimap& other) {
        if (this != &other) {
            const bool propagate = allocator_traits<allocator_type>::
                propagate_on_container_copy_assignment::value;
            multimap candidate(other, propagate ? other.m_alloc : m_alloc);
            if (propagate) {
                m_comp = candidate.m_comp;
                clear();
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

    multimap& operator=(multimap&& other)
        noexcept(allocator_traits<allocator_type>::
                     propagate_on_container_move_assignment::value &&
                 noexcept(m_comp = std::move(other.m_comp))) {
        if (this != &other) {
            using alloc_traits = allocator_traits<allocator_type>;
            if (alloc_traits::propagate_on_container_move_assignment::value) {
                clear();
                m_alloc = std::move(other.m_alloc);
                m_root = other.m_root;
                m_size = other.m_size;
                m_comp = std::move(other.m_comp);
                other.m_root = nullptr;
                other.m_size = 0;
            } else {
                if (other.m_size > max_size())
                    map_detail::length_failure();
                multimap candidate(other.m_comp, m_alloc);
                for (auto it = other.begin(); it != other.end(); ++it)
                    candidate.insert(std::move(*it));
                clear();
                m_root = candidate.m_root;
                m_size = candidate.m_size;
                m_comp = std::move(candidate.m_comp);
                candidate.m_root = nullptr;
                candidate.m_size = 0;
                other.clear();
            }
        }
        return *this;
    }

    iterator begin() noexcept { return iterator(m_root ? m_root->minimum() : nullptr, this); }
    iterator end() noexcept { return iterator(nullptr, this); }
    const_iterator begin() const noexcept { return const_iterator(m_root ? m_root->minimum() : nullptr, this); }
    const_iterator end() const noexcept { return const_iterator(nullptr, this); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

    bool empty() const noexcept { return m_size == 0; }
    size_type size() const noexcept { return m_size; }
    size_type max_size() const noexcept {
        return node_allocator_traits::max_size(m_alloc);
    }

    iterator insert(const value_type& value) {
        return iterator(insert_multi_node(value.first, value.second), this);
    }

    iterator insert(value_type&& value) {
        return iterator(insert_multi_node(value.first, std::move(value.second)), this);
    }

    template<typename K2, typename V2,
             typename = typename enable_if<
                 is_constructible<key_type, K2>::value && is_constructible<mapped_type, V2>::value
             >::type>
    iterator insert(const pair<K2, V2>& value) {
        return iterator(insert_multi_node(value.first, value.second), this);
    }

    template<typename K2, typename V2,
             typename = typename enable_if<
                 is_constructible<key_type, K2&&>::value && is_constructible<mapped_type, V2&&>::value
             >::type>
    iterator insert(pair<K2, V2>&& value) {
        return iterator(insert_multi_node(std::forward<K2>(value.first), std::forward<V2>(value.second)), this);
    }

    iterator insert(const_iterator /*hint*/, const value_type& value) {
        return insert(value);
    }

    iterator insert(const_iterator /*hint*/, value_type&& value) {
        return insert(std::move(value));
    }

    template<typename InputIt>
    void insert(InputIt first, InputIt last) {
        for (; first != last; ++first) {
            insert(*first);
        }
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

    iterator insert(node_type&& node) {
        if (node.empty()) return end();
        iterator result = iterator(
            insert_multi_node(*node.m_key, std::move(*node.m_mapped)), this);
        node.reset();
        return result;
    }

    iterator insert(const_iterator /*hint*/, node_type&& node) {
        return insert(std::move(node));
    }

    template<typename... Args>
    iterator emplace(Args&&... args) {
        pair<key_type, mapped_type> tmp(std::forward<Args>(args)...);
        return iterator(insert_multi_node(std::move(tmp.first), std::move(tmp.second)), this);
    }

    template<typename... Args>
    iterator emplace_hint(const_iterator /*hint*/, Args&&... args) {
        return emplace(std::forward<Args>(args)...);
    }

    iterator erase(iterator pos) {
        if ((!pos.m_node && pos.m_map != this) || !owns_node(pos.m_node))
            return end();
        Node* next = erase_node(pos.m_node);
        return iterator(next, this);
    }

    iterator erase(const_iterator pos) {
        if ((!pos.m_node && pos.m_map != this) || !owns_node(pos.m_node))
            return end();
        Node* next = erase_node(const_cast<Node*>(pos.m_node));
        return iterator(next, this);
    }

    iterator erase(iterator first, iterator last) {
        if ((!first.m_node && first.m_map != this) ||
            (!last.m_node && last.m_map != this) ||
            (first.m_node && !owns_node(first.m_node)) ||
            (last.m_node && !owns_node(last.m_node))) return end();
        while (first != last) {
            first = erase(first);
        }
        return last;
    }

    iterator erase(const_iterator first, const_iterator last) {
        if ((!first.m_node && first.m_map != this) ||
            (!last.m_node && last.m_map != this) ||
            (first.m_node && !owns_node(first.m_node)) ||
            (last.m_node && !owns_node(last.m_node))) return end();
        iterator it(const_cast<Node*>(first.m_node), this);
        iterator e(const_cast<Node*>(last.m_node), this);
        return erase(it, e);
    }

    size_type erase(const key_type& key) {
        auto range = equal_range(key);
        size_type removed = 0;
        while (range.first != range.second) {
            range.first = erase(range.first);
            ++removed;
        }
        return removed;
    }

    template<typename K, typename C = Compare,
             typename = typename map_detail::transparent_compare<C>::type>
    size_type erase(const K& key) {
        auto range = equal_range(key);
        size_type removed = 0;
        while (range.first != range.second) {
            range.first = erase(range.first);
            ++removed;
        }
        return removed;
    }

    node_type extract(const_iterator position) {
        if ((!position.m_node && position.m_map != this) ||
            !owns_node(position.m_node))
            return node_type();
        Node* node = const_cast<Node*>(position.m_node);
        key_allocator_type key_alloc(m_alloc);
        using key_allocator_traits = allocator_traits<key_allocator_type>;
        using key_pointer = typename key_allocator_traits::pointer;
        key_pointer key_allocation = key_allocator_traits::allocate(key_alloc, 1);
        key_type* key = map_detail::pointer_address(key_allocation);
        if (key_allocation == key_pointer() || !key) {
            if (key_allocation != key_pointer())
                key_allocator_traits::deallocate(key_alloc, key_allocation, 1);
            map_detail::allocation_failure();
        }
        mapped_allocator_type mapped_alloc(m_alloc);
        using mapped_allocator_traits = allocator_traits<mapped_allocator_type>;
        using mapped_pointer = typename mapped_allocator_traits::pointer;
        mapped_pointer mapped_allocation = mapped_pointer();
        mapped_type* mapped = nullptr;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        bool key_constructed = false;
        try {
#endif
            key_allocator_traits::construct(
                key_alloc, key, node->data.first);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            key_constructed = true;
#endif
            mapped_allocation = mapped_allocator_traits::allocate(mapped_alloc, 1);
            mapped = map_detail::pointer_address(mapped_allocation);
            if (mapped_allocation == mapped_pointer() || !mapped)
                map_detail::allocation_failure();
            mapped_allocator_traits::construct(
                mapped_alloc, mapped, std::move(node->data.second));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            if (mapped_allocation != mapped_pointer())
                mapped_allocator_traits::deallocate(
                    mapped_alloc, mapped_allocation, 1);
            if (key_constructed)
                key_allocator_traits::destroy(key_alloc, key);
            key_allocator_traits::deallocate(key_alloc, key_allocation, 1);
            throw;
        }
#endif
        erase_node(node);
        return node_type(key, mapped, allocator_type(m_alloc));
    }

    node_type extract(const key_type& key) {
        iterator position = find(key);
        return position == end() ? node_type() : extract(position);
    }

#if defined(RIN_MAP_HAS_HETERO_INSERTION)
    template<typename K, typename C = Compare,
             typename = typename map_detail::transparent_compare<C>::type,
             typename = typename enable_if<
                 is_constructible<key_type, K&&>::value>::type>
    node_type extract(const K& key) {
        iterator position = find(key);
        return position == end() ? node_type() : extract(position);
    }
#endif

    void clear() {
        destroy_tree(m_root);
        m_root = nullptr;
        m_size = 0;
    }

    void swap(multimap& other) {
        if (this == &other) return;
        if (!allocator_traits<allocator_type>::propagate_on_container_swap::value &&
            m_alloc != other.m_alloc) {
            swap_unequal(other);
            return;
        }
        map_detail::map_exchange(m_root, other.m_root);
        map_detail::map_exchange(m_size, other.m_size);
        map_detail::map_exchange(m_comp, other.m_comp);
        if (allocator_traits<allocator_type>::
                propagate_on_container_swap::value) {
            map_detail::map_exchange(m_alloc, other.m_alloc);
        }
    }

    allocator_type get_allocator() const noexcept { return allocator_type(m_alloc); }

    iterator find(const key_type& key) {
        iterator it = lower_bound(key);
        if (it != end() && !m_comp(key, it->first) && !m_comp(it->first, key)) {
            return it;
        }
        return end();
    }

    const_iterator find(const key_type& key) const {
        const_iterator it = lower_bound(key);
        if (it != end() && !m_comp(key, it->first) && !m_comp(it->first, key)) {
            return it;
        }
        return end();
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    iterator find(const K& key) {
        iterator it = lower_bound(key);
        if (it != end() && !m_comp(key, it->first) && !m_comp(it->first, key)) {
            return it;
        }
        return end();
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    const_iterator find(const K& key) const {
        const_iterator it = lower_bound(key);
        if (it != end() && !m_comp(key, it->first) && !m_comp(it->first, key)) {
            return it;
        }
        return end();
    }

    size_type count(const key_type& key) const {
        auto range = equal_range(key);
        size_type c = 0;
        for (auto it = range.first; it != range.second; ++it) ++c;
        return c;
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    size_type count(const K& key) const {
        auto range = equal_range(key);
        size_type c = 0;
        for (auto it = range.first; it != range.second; ++it) ++c;
        return c;
    }

    bool contains(const key_type& key) const {
        return lower_bound_node(key) != upper_bound_node(key);
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    bool contains(const K& key) const {
        return lower_bound_node(key) != upper_bound_node(key);
    }

    iterator lower_bound(const key_type& key) {
        return iterator(lower_bound_node(key), this);
    }

    const_iterator lower_bound(const key_type& key) const {
        return const_iterator(lower_bound_node(key), this);
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    iterator lower_bound(const K& key) {
        return iterator(lower_bound_node(key), this);
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    const_iterator lower_bound(const K& key) const {
        return const_iterator(lower_bound_node(key), this);
    }

    iterator upper_bound(const key_type& key) {
        return iterator(upper_bound_node(key), this);
    }

    const_iterator upper_bound(const key_type& key) const {
        return const_iterator(upper_bound_node(key), this);
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    iterator upper_bound(const K& key) {
        return iterator(upper_bound_node(key), this);
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    const_iterator upper_bound(const K& key) const {
        return const_iterator(upper_bound_node(key), this);
    }

    pair<iterator, iterator> equal_range(const key_type& key) {
        return { lower_bound(key), upper_bound(key) };
    }

    pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
        return { lower_bound(key), upper_bound(key) };
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    pair<iterator, iterator> equal_range(const K& key) {
        return { lower_bound(key), upper_bound(key) };
    }

    template<typename K, typename C = Compare, typename = typename map_detail::transparent_compare<C>::type>
    pair<const_iterator, const_iterator> equal_range(const K& key) const {
        return { lower_bound(key), upper_bound(key) };
    }

    template<typename C2, typename A2>
    void merge(map<Key, T, C2, A2>& source) {
        if (static_cast<const void*>(this) == static_cast<const void*>(&source)) return;
        merge_map_impl(source, integral_constant<bool,
                        is_copy_constructible<value_type>::value>());
    }

    template<typename C2, typename A2>
    void merge(multimap<Key, T, C2, A2>& source) {
        if (static_cast<const void*>(this) == static_cast<const void*>(&source)) return;
        merge_multimap_impl(source, integral_constant<bool,
                             is_copy_constructible<value_type>::value>());
    }

    key_compare key_comp() const { return m_comp; }
    value_compare value_comp() const { return value_compare(m_comp); }
};

template<typename Key, typename T, typename Compare, typename Allocator>
bool operator==(const multimap<Key, T, Compare, Allocator>& lhs,
                const multimap<Key, T, Compare, Allocator>& rhs) {
    if (lhs.size() != rhs.size()) return false;
    auto it1 = lhs.begin();
    auto it2 = rhs.begin();
    while (it1 != lhs.end()) {
        if (it1->first != it2->first || it1->second != it2->second) return false;
        ++it1;
        ++it2;
    }
    return true;
}

template<typename Key, typename T, typename Compare, typename Allocator>
bool operator!=(const multimap<Key, T, Compare, Allocator>& lhs,
                const multimap<Key, T, Compare, Allocator>& rhs) {
    return !(lhs == rhs);
}

template<typename Key, typename T, typename Compare, typename Allocator>
void swap(multimap<Key, T, Compare, Allocator>& lhs,
          multimap<Key, T, Compare, Allocator>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

template<typename Key, typename T, typename Compare, typename Allocator>
bool operator<(const multimap<Key, T, Compare, Allocator>& lhs,
               const multimap<Key, T, Compare, Allocator>& rhs) {
    auto left = lhs.begin();
    auto right = rhs.begin();
    while (left != lhs.end() && right != rhs.end()) {
        if (left->first < right->first) return true;
        if (right->first < left->first) return false;
        if (left->second < right->second) return true;
        if (right->second < left->second) return false;
        ++left;
        ++right;
    }
    return left == lhs.end() && right != rhs.end();
}

template<typename Key, typename T, typename Compare, typename Allocator>
bool operator>(const multimap<Key, T, Compare, Allocator>& lhs,
               const multimap<Key, T, Compare, Allocator>& rhs) {
    return rhs < lhs;
}

template<typename Key, typename T, typename Compare, typename Allocator>
bool operator<=(const multimap<Key, T, Compare, Allocator>& lhs,
                const multimap<Key, T, Compare, Allocator>& rhs) {
    return !(rhs < lhs);
}

template<typename Key, typename T, typename Compare, typename Allocator>
bool operator>=(const multimap<Key, T, Compare, Allocator>& lhs,
                const multimap<Key, T, Compare, Allocator>& rhs) {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
template<typename Key, typename T, typename Compare, typename Allocator>
auto operator<=>(const multimap<Key, T, Compare, Allocator>& lhs,
                 const multimap<Key, T, Compare, Allocator>& rhs)
    -> detail::synth_three_way_result_t<typename multimap<Key, T, Compare, Allocator>::value_type> {
    using result_type = detail::synth_three_way_result_t<
        typename multimap<Key, T, Compare, Allocator>::value_type>;
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

template<typename Key, typename T, typename Compare, typename Allocator>
typename map<Key, T, Compare, Allocator>::size_type
erase(map<Key, T, Compare, Allocator>& value, const Key& key) {
    return value.erase(key);
}

template<typename Key, typename T, typename Compare, typename Allocator>
typename multimap<Key, T, Compare, Allocator>::size_type
erase(multimap<Key, T, Compare, Allocator>& value, const Key& key) {
    return value.erase(key);
}

template<typename Key, typename T, typename Compare, typename Allocator,
         typename K,
         typename = typename map_detail::transparent_compare<Compare>::type>
typename map<Key, T, Compare, Allocator>::size_type
erase(map<Key, T, Compare, Allocator>& value, const K& key) {
    return value.erase(key);
}

template<typename Key, typename T, typename Compare, typename Allocator,
         typename K,
         typename = typename map_detail::transparent_compare<Compare>::type>
typename multimap<Key, T, Compare, Allocator>::size_type
erase(multimap<Key, T, Compare, Allocator>& value, const K& key) {
    return value.erase(key);
}

template<typename Key, typename T, typename Compare, typename Allocator, typename Pred>
typename map<Key, T, Compare, Allocator>::size_type
erase_if(map<Key, T, Compare, Allocator>& c, Pred pred) {
    auto old_size = c.size();
    for (auto it = c.begin(); it != c.end();) {
        if (pred(*it)) it = c.erase(it);
        else ++it;
    }
    return old_size - c.size();
}

template<typename Key, typename T, typename Compare, typename Allocator, typename Pred>
typename multimap<Key, T, Compare, Allocator>::size_type
erase_if(multimap<Key, T, Compare, Allocator>& c, Pred pred) {
    auto old_size = c.size();
    for (auto it = c.begin(); it != c.end();) {
        if (pred(*it)) it = c.erase(it);
        else ++it;
    }
    return old_size - c.size();
}

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_MAP_H */
