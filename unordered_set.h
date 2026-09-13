/*
 * RinOS C++ <unordered_set> ✿
 * ハッシュセット - 完全実装
 */

#ifndef RINCXX_UNORDERED_SET_H
#define RINCXX_UNORDERED_SET_H

#include "rincxx.h"
#include "type_traits.h"
#include "functional.h"
#include "initializer_list.h"
#include "iterator.h"
#include "utility.h"
#include "memory.h"
#include "stdexcept.h"
#if __cplusplus > 202002L
#include "ranges.h"
#endif

namespace std {

template<class Key, class Hash, class KeyEqual, class Allocator>
class unordered_multiset;

namespace detail {

template<typename Hash, typename Equal, typename Key, typename K,
         typename = void>
struct unordered_heterogeneous_lookup : false_type {};

template<typename Hash, typename Equal, typename Key, typename K>
struct unordered_heterogeneous_lookup<Hash, Equal, Key, K, void_t<
    typename Hash::is_transparent,
    typename Equal::is_transparent,
    decltype(declval<const Hash&>()(declval<const K&>())),
    decltype(declval<const Equal&>()(
        declval<const Key&>(), declval<const K&>())),
    decltype(declval<const Equal&>()(
        declval<const K&>(), declval<const Key&>()))>> : true_type {};

/* Keep unordered-container publication independent from host std::swap
 * overloads.  The Rin compatibility headers and a hosted standard library
 * can both contribute a qualified std::swap candidate; this explicit
 * value-semantic exchange keeps lookup deterministic while preserving the
 * actual move exception specification. */
template<typename T>
inline void unordered_set_exchange(T& left, T& right) noexcept(
    noexcept(T(std::move(left))) &&
    noexcept(left = std::move(right)) &&
    noexcept(right = std::move(left))) {
    if (&left == &right) return;
    T temporary(std::move(left));
    left = std::move(right);
    right = std::move(temporary);
}

template<typename T, typename Allocator, typename = void>
struct unordered_effective_allocator {
    using type = allocator<T>;
};

template<typename T, typename Allocator>
struct unordered_effective_allocator<T, Allocator, void_t<
    typename Allocator::value_type,
    decltype(declval<Allocator&>().allocate(size_t{}))>> {
    using type = Allocator;
};

template<typename T>
struct unordered_effective_allocator<T, void, void> {
    using type = allocator<T>;
};

[[noreturn]] inline void unordered_allocation_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_alloc();
#else
    __builtin_trap();
#endif
}

[[noreturn]] inline void unordered_length_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw length_error("unordered_set::swap exceeds max_size");
#else
    __builtin_trap();
#endif
}

[[noreturn]] inline void unordered_load_factor_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw invalid_argument("unordered_set::max_load_factor requires a positive value");
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

template<class Node>
Node** unordered_allocate_buckets(size_t count) {
    Node** buckets = static_cast<Node**>(rin_malloc(count * sizeof(Node*)));
    if (!buckets)
        unordered_allocation_failure();
    for (size_t i = 0; i < count; ++i)
        buckets[i] = nullptr;
    return buckets;
}

template<class Node, class... Args>
Node* unordered_allocate_node(Args&&... args) {
    void* memory = rin_malloc(sizeof(Node));
    if (!memory)
        unordered_allocation_failure();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
        return ::new (memory) Node(forward<Args>(args)...);
    } catch (...) {
        rin_free(memory);
        throw;
    }
#else
    return ::new (memory) Node(forward<Args>(args)...);
#endif
}

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * unordered_set クラス
 * ═══════════════════════════════════════════════════════════════*/

template<
    class Key,
    class Hash = hash<Key>,
    class KeyEqual = equal_to<Key>,
    class Allocator = void
>
class unordered_set {
public:
    using key_type = Key;
    using value_type = Key;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = typename detail::unordered_effective_allocator<
        value_type, Allocator>::type;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    
private:
    /* ノード構造 */
    struct Node {
        value_type value;
        Node* next;
        size_type hash_value;
        
        template<class... Args>
        Node(size_type h, Args&&... args)
            : value(forward<Args>(args)...), next(nullptr), hash_value(h) {}
    };

    using node_allocator_type = typename allocator_traits<allocator_type>::template
        rebind_alloc<Node>;
    using node_allocator_traits = allocator_traits<node_allocator_type>;
    using node_pointer = typename node_allocator_traits::pointer;

public:
    class node_type {
        Node* m_node;
        allocator_type m_alloc;

        node_type(Node* node, const allocator_type& alloc)
            : m_node(node), m_alloc(alloc) {}
        friend class unordered_set;

        void reset() noexcept {
            if (!m_node) return;
            node_allocator_type node_alloc(m_alloc);
            node_allocator_traits::destroy(node_alloc, m_node);
            node_allocator_traits::deallocate(
                node_alloc,
                pointer_traits<node_pointer>::pointer_to(*m_node), 1);
            m_node = nullptr;
        }

    public:
        node_type() : m_node(nullptr), m_alloc() {}
        node_type(node_type&& other) noexcept
            : m_node(other.m_node), m_alloc(move(other.m_alloc)) {
            other.m_node = nullptr;
        }
        node_type& operator=(node_type&& other) noexcept {
            if (this != &other) {
                reset();
                m_node = other.m_node;
                m_alloc = move(other.m_alloc);
                other.m_node = nullptr;
            }
            return *this;
        }
        node_type(const node_type&) = delete;
        node_type& operator=(const node_type&) = delete;
        ~node_type() { reset(); }

        bool empty() const noexcept { return m_node == nullptr; }
        explicit operator bool() const noexcept { return !empty(); }
        value_type& value() const { return m_node->value; }
        allocator_type get_allocator() const noexcept { return m_alloc; }
    };

private:
    using bucket_allocator_type = typename allocator_traits<allocator_type>::template
        rebind_alloc<Node*>;
    using bucket_allocator_traits = allocator_traits<bucket_allocator_type>;
    using bucket_pointer = typename bucket_allocator_traits::pointer;

    allocator_type allocator_;
    Node** buckets_;
    size_type bucket_count_;
    size_type size_;
    float max_load_factor_;
    Hash hasher_;
    KeyEqual key_equal_;
    
    static constexpr size_type initial_bucket_count = 8;

    static constexpr float default_max_load_factor = 1.0f;

    Node** allocate_buckets(size_type count) {
        bucket_allocator_type bucket_alloc(allocator_);
        bucket_pointer allocation = bucket_allocator_traits::allocate(bucket_alloc, count);
        Node** buckets = detail::pointer_address(allocation);
        if (allocation == bucket_pointer() || !buckets) {
            if (allocation != bucket_pointer())
                bucket_allocator_traits::deallocate(bucket_alloc, allocation, count);
            detail::unordered_allocation_failure();
        }
        for (size_type i = 0; i < count; ++i) buckets[i] = nullptr;
        return buckets;
    }

    void deallocate_buckets(Node** buckets, size_type count) noexcept {
        if (!buckets) return;
        bucket_allocator_type bucket_alloc(allocator_);
        bucket_allocator_traits::deallocate(
            bucket_alloc, pointer_traits<bucket_pointer>::pointer_to(*buckets), count);
    }

    template<class... Args>
    Node* allocate_node(Args&&... args) {
        node_allocator_type node_alloc(allocator_);
        node_pointer allocation = node_allocator_traits::allocate(node_alloc, 1);
        Node* node = detail::pointer_address(allocation);
        if (allocation == node_pointer() || !node) {
            if (allocation != node_pointer())
                node_allocator_traits::deallocate(node_alloc, allocation, 1);
            detail::unordered_allocation_failure();
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            node_allocator_traits::construct(node_alloc, node,
                                              forward<Args>(args)...);
        } catch (...) {
            node_allocator_traits::deallocate(node_alloc, allocation, 1);
            throw;
        }
#else
        node_allocator_traits::construct(node_alloc, node,
                                          forward<Args>(args)...);
#endif
        return node;
    }

    void destroy_node(Node* node) noexcept {
        node_allocator_type node_alloc(allocator_);
        node_allocator_traits::destroy(node_alloc, node);
        node_allocator_traits::deallocate(
            node_alloc, pointer_traits<node_pointer>::pointer_to(*node), 1);
    }

    void relocate_values(unordered_set& source, unordered_set& target,
                         true_type) {
        for (const auto& value : source)
            target.insert(value);
    }

    void relocate_values(unordered_set& source, unordered_set& target,
                         false_type) {
        /* Iterators expose const keys, but this private replacement pass owns
         * the source nodes and can move a non-copyable key before publication. */
        for (size_type index = 0; index < source.bucket_count_; ++index) {
            for (Node* node = source.buckets_[index]; node; node = node->next)
                target.insert(std::move(node->value));
        }
    }

    void swap_unequal(unordered_set& other) {
        if (other.size_ > max_size() || size_ > other.max_size())
            detail::unordered_length_failure();
        const size_type left_buckets = other.bucket_count_ ?
            other.bucket_count_ : initial_bucket_count;
        const size_type right_buckets = bucket_count_ ?
            bucket_count_ : initial_bucket_count;
        unordered_set left(left_buckets, other.hasher_, other.key_equal_, allocator_);
        unordered_set right(right_buckets, hasher_, key_equal_, other.allocator_);
        left.max_load_factor_ = other.max_load_factor_;
        right.max_load_factor_ = max_load_factor_;
        relocate_values(other, left, is_copy_constructible<value_type>());
        relocate_values(*this, right, is_copy_constructible<value_type>());
        swap(left);
        other.swap(right);
    }

    void copy_assign_allocator(const allocator_type& other, true_type) {
        deallocate_buckets(buckets_, bucket_count_);
        buckets_ = nullptr;
        bucket_count_ = 0;
        allocator_ = other;
        bucket_count_ = other.bucket_count_ ? other.bucket_count_ : initial_bucket_count;
        buckets_ = allocate_buckets(bucket_count_);
    }

    void copy_assign_allocator(const allocator_type&, false_type) {}

    void commit_copy_assignment(unordered_set& candidate, true_type) {
        hasher_ = candidate.hasher_;
        key_equal_ = candidate.key_equal_;
        max_load_factor_ = candidate.max_load_factor_;
        clear();
        deallocate_buckets(buckets_, bucket_count_);
        allocator_ = candidate.allocator_;
        buckets_ = candidate.buckets_;
        bucket_count_ = candidate.bucket_count_;
        size_ = candidate.size_;
        candidate.buckets_ = nullptr;
        candidate.bucket_count_ = 0;
        candidate.size_ = 0;
    }

    void commit_copy_assignment(unordered_set& candidate, false_type) {
        swap(candidate);
    }
    
    /* バケットインデックスを計算 */
    size_type bucket_index(size_type hash) const {
        return hash % bucket_count_;
    }

    bool locate_node(const Node* target, size_type& bucket) const noexcept {
        if (!target || !buckets_) return false;
        for (size_type i = 0; i < bucket_count_; ++i) {
            for (const Node* node = buckets_[i]; node; node = node->next) {
                if (node == target) {
                    bucket = i;
                    return true;
                }
            }
        }
        return false;
    }

    Node* insert_node_from_handle(node_type& node, true_type) {
        Node* replacement = allocate_node(node.m_node->hash_value,
                                          node.m_node->value);
        node.reset();
        return replacement;
    }

    Node* insert_node_from_handle(node_type& node, false_type) {
        Node* replacement = allocate_node(node.m_node->hash_value,
                                          move(node.m_node->value));
        node.reset();
        return replacement;
    }

    void ensure_bucket_storage() {
        if (buckets_ && bucket_count_ != 0) return;
        Node** storage = allocate_buckets(initial_bucket_count);
        bucket_count_ = initial_bucket_count;
        buckets_ = storage;
    }
    
    /* リハッシュ */
    void rehash_if_needed() {
        if (load_factor() > max_load_factor_) {
            rehash(bucket_count_ * 2);
        }
    }

#if __cplusplus > 202002L
    /* Copyable keys can stage the complete range in a table owned by the
     * destination allocator.  Hash/equality, allocation, and key-construction
     * failures therefore leave the live table untouched. */
    template<class R>
    void insert_range_impl(R&& range, true_type) {
        const size_type candidate_buckets =
            bucket_count_ ? bucket_count_ : initial_bucket_count;
        unordered_set candidate(candidate_buckets, hasher_, key_equal_, allocator_);
        candidate.max_load_factor_ = max_load_factor_;
        for (const auto& value : *this)
            candidate.insert(value);
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        for (; first != last; ++first)
            candidate.insert(*first);

        clear();
        deallocate_buckets(buckets_, bucket_count_);
        buckets_ = candidate.buckets_;
        bucket_count_ = candidate.bucket_count_;
        size_ = candidate.size_;
        candidate.buckets_ = nullptr;
        candidate.bucket_count_ = 0;
        candidate.size_ = 0;
    }

    template<class R>
    void insert_range_impl(R&& range, false_type) {
        /* Move-only keys cannot be copied into the candidate table.  Moving
         * the existing nodes first still keeps publication transactional: a
         * throwing hash, equality, allocation, or key move leaves a valid
         * destination table, although keys already moved from the source may
         * be unspecified (the standard basic guarantee). */
        const size_type candidate_buckets =
            bucket_count_ ? bucket_count_ : initial_bucket_count;
        unordered_set candidate(candidate_buckets, hasher_, key_equal_, allocator_);
        candidate.max_load_factor_ = max_load_factor_;
        for (size_type index = 0; index < bucket_count_; ++index) {
            for (Node* node = buckets_[index]; node; node = node->next)
                candidate.insert(std::move(node->value));
        }
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        for (; first != last; ++first)
            candidate.insert(std::move(*first));

        clear();
        deallocate_buckets(buckets_, bucket_count_);
        buckets_ = candidate.buckets_;
        bucket_count_ = candidate.bucket_count_;
        size_ = candidate.size_;
        candidate.buckets_ = nullptr;
        candidate.bucket_count_ = 0;
        candidate.size_ = 0;
    }
#endif
    
public:
    /* ═══════════════════════════════════════════════════════════
     * イテレータ
     * ═══════════════════════════════════════════════════════════*/
    
    class iterator {
        friend class unordered_set;
        const unordered_set* set_;
        size_type bucket_;
        Node* node_;
        
        void advance() {
            if (node_) {
                node_ = node_->next;
            }
            while (!node_ && bucket_ + 1 < set_->bucket_count_) {
                ++bucket_;
                node_ = set_->buckets_[bucket_];
            }
        }
        
    public:
        using iterator_category = forward_iterator_tag;
        using value_type = Key;
        using difference_type = ptrdiff_t;
        using pointer = const Key*;
        using reference = const Key&;
        
        iterator() : set_(nullptr), bucket_(0), node_(nullptr) {}
        iterator(const unordered_set* s, size_type b, Node* n)
            : set_(s), bucket_(b), node_(n) {}
        
        reference operator*() const { return node_->value; }
        pointer operator->() const { return &node_->value; }
        
        iterator& operator++() {
            advance();
            return *this;
        }
        
        iterator operator++(int) {
            iterator tmp = *this;
            advance();
            return tmp;
        }
        
        bool operator==(const iterator& other) const {
            // The bucket index is a traversal cache and may be stale after a rehash; iterator identity is the owning container plus node.
            return set_ == other.set_ && node_ == other.node_;
        }
        
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
    };
    
    using const_iterator = iterator;  /* セットのイテレータは常にconst */
    using local_iterator = iterator;
    using const_local_iterator = const_iterator;

    struct insert_return_type {
        iterator position;
        bool inserted;
        node_type node;
    };
    
    /* ═══════════════════════════════════════════════════════════
     * コンストラクタ・デストラクタ
     * ═══════════════════════════════════════════════════════════*/
    
    unordered_set()
        : allocator_()
        , bucket_count_(initial_bucket_count)
        , size_(0)
        , max_load_factor_(default_max_load_factor)
        , hasher_()
        , key_equal_()
    {
        buckets_ = allocate_buckets(bucket_count_);
    }
    
    explicit unordered_set(size_type bucket_count,
                           const Hash& hash = Hash(),
                           const KeyEqual& equal = KeyEqual())
        : allocator_()
        , bucket_count_(bucket_count ? bucket_count : initial_bucket_count)
        , size_(0)
        , max_load_factor_(default_max_load_factor)
        , hasher_(hash)
        , key_equal_(equal)
    {
        buckets_ = allocate_buckets(bucket_count_);
    }

    unordered_set(size_type bucket_count,
                  const Hash& hash,
                  const KeyEqual& equal,
                  const allocator_type& alloc)
        : allocator_(alloc)
        , bucket_count_(bucket_count ? bucket_count : initial_bucket_count)
        , size_(0)
        , max_load_factor_(default_max_load_factor)
        , hasher_(hash)
        , key_equal_(equal)
    {
        buckets_ = allocate_buckets(bucket_count_);
    }

    explicit unordered_set(const allocator_type& alloc)
        : allocator_(alloc)
        , bucket_count_(initial_bucket_count)
        , size_(0)
        , max_load_factor_(default_max_load_factor)
        , hasher_()
        , key_equal_()
    {
        buckets_ = allocate_buckets(bucket_count_);
    }

    template<class InputIt,
             class = typename enable_if<!is_integral<InputIt>::value>::type>
    unordered_set(InputIt first, InputIt last,
                  size_type bucket_count = initial_bucket_count,
                  const Hash& hash = Hash(),
                  const KeyEqual& equal = KeyEqual(),
                  const allocator_type& alloc = allocator_type())
        : unordered_set(bucket_count, hash, equal, alloc)
    {
        for (; first != last; ++first)
            insert(*first);
    }
    
    unordered_set(initializer_list<value_type> init,
                  size_type bucket_count = initial_bucket_count,
                  const Hash& hash = Hash(),
                  const KeyEqual& equal = KeyEqual(),
                  const allocator_type& alloc = allocator_type())
        : unordered_set(bucket_count, hash, equal, alloc)
    {
        for (const auto& v : init)
            insert(v);
    }

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, value_type>
    unordered_set(from_range_t, R&& range,
                  size_type bucket_count = initial_bucket_count,
                  const Hash& hash = Hash(),
                  const KeyEqual& equal = KeyEqual(),
                  const allocator_type& alloc = allocator_type())
        : unordered_set(bucket_count, hash, equal, alloc)
    {
        insert_range(std::forward<R>(range));
    }
#endif
    
    unordered_set(const unordered_set& other)
        : allocator_(allocator_traits<allocator_type>::select_on_container_copy_construction(other.allocator_))
        , bucket_count_(other.bucket_count_ ? other.bucket_count_ : initial_bucket_count)
        , size_(0)
        , max_load_factor_(other.max_load_factor_)
        , hasher_(other.hasher_)
        , key_equal_(other.key_equal_)
    {
        buckets_ = allocate_buckets(bucket_count_);
        
        for (const auto& v : other)
            insert(v);
    }
    
    unordered_set(unordered_set&& other) noexcept
        : allocator_(move(other.allocator_))
        , buckets_(other.buckets_)
        , bucket_count_(other.bucket_count_)
        , size_(other.size_)
        , max_load_factor_(other.max_load_factor_)
        , hasher_(move(other.hasher_))
        , key_equal_(move(other.key_equal_))
    {
        other.buckets_ = nullptr;
        other.bucket_count_ = 0;
        other.size_ = 0;
    }
    
    ~unordered_set() {
        clear();
        deallocate_buckets(buckets_, bucket_count_);
    }
    
    /* ═══════════════════════════════════════════════════════════
     * 代入
     * ═══════════════════════════════════════════════════════════*/
    
    unordered_set& operator=(const unordered_set& other) {
        if (this != &other) {
            using propagate = typename allocator_traits<allocator_type>::
                propagate_on_container_copy_assignment;
            unordered_set candidate(
                other.bucket_count_, other.hasher_, other.key_equal_,
                propagate::value ? other.allocator_ : allocator_);
            candidate.max_load_factor_ = other.max_load_factor_;
            for (const auto& v : other) candidate.insert(v);
            commit_copy_assignment(candidate, propagate());
        }
        return *this;
    }
    
    unordered_set& operator=(unordered_set&& other) noexcept {
        if (this != &other) {
            using propagate = typename allocator_traits<allocator_type>::
                propagate_on_container_move_assignment;
            if (propagate::value || allocator_ == other.allocator_) {
                clear();
                deallocate_buckets(buckets_, bucket_count_);
                if (propagate::value) allocator_ = move(other.allocator_);
                buckets_ = other.buckets_;
                bucket_count_ = other.bucket_count_;
                size_ = other.size_;
                max_load_factor_ = other.max_load_factor_;
                hasher_ = move(other.hasher_);
                key_equal_ = move(other.key_equal_);
                other.buckets_ = nullptr;
                other.bucket_count_ = 0;
                other.size_ = 0;
            } else {
                clear();
                hasher_ = other.hasher_;
                key_equal_ = other.key_equal_;
                max_load_factor_ = other.max_load_factor_;
                for (size_type index = 0; index < other.bucket_count_; ++index) {
                    for (Node* node = other.buckets_[index]; node;
                         node = node->next) {
                        insert(std::move(node->value));
                    }
                }
                other.clear();
            }
        }
        return *this;
    }
    
    unordered_set& operator=(initializer_list<value_type> init) {
        clear();
        for (const auto& v : init)
            insert(v);
        return *this;
    }
    
    /* ═══════════════════════════════════════════════════════════
     * イテレータ
     * ═══════════════════════════════════════════════════════════*/
    
    iterator begin() noexcept {
        for (size_type i = 0; i < bucket_count_; ++i) {
            if (buckets_[i])
                return iterator(this, i, buckets_[i]);
        }
        return end();
    }
    
    const_iterator begin() const noexcept {
        for (size_type i = 0; i < bucket_count_; ++i) {
            if (buckets_[i])
                return const_iterator(this, i, buckets_[i]);
        }
        return end();
    }
    
    const_iterator cbegin() const noexcept { return begin(); }
    
    iterator end() noexcept {
        return iterator(this, bucket_count_, nullptr);
    }
    
    const_iterator end() const noexcept {
        return const_iterator(this, bucket_count_, nullptr);
    }
    
    const_iterator cend() const noexcept { return end(); }
    
    /* ═══════════════════════════════════════════════════════════
     * 容量
     * ═══════════════════════════════════════════════════════════*/
    
    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type max_size() const noexcept {
        return allocator_traits<allocator_type>::max_size(allocator_);
    }
    
    /* ═══════════════════════════════════════════════════════════
     * 変更
     * ═══════════════════════════════════════════════════════════*/
    
    void clear() noexcept {
        for (size_type i = 0; i < bucket_count_; ++i) {
            Node* node = buckets_[i];
            while (node) {
                Node* next = node->next;
                destroy_node(node);
                node = next;
            }
            buckets_[i] = nullptr;
        }
        size_ = 0;
    }
    
    pair<iterator, bool> insert(const value_type& value) {
        return emplace(value);
    }
    
    pair<iterator, bool> insert(value_type&& value) {
        return emplace(move(value));
    }
    
    template<class InputIt>
    void insert(InputIt first, InputIt last) {
        for (; first != last; ++first)
            insert(*first);
    }
    
    void insert(initializer_list<value_type> init) {
        for (const auto& v : init)
            insert(v);
    }

#if __cplusplus > 202002L
    template<class R>
        requires detail::container_compatible_range<R, value_type>
    void insert_range(R&& range) {
        insert_range_impl(std::forward<R>(range),
                          integral_constant<bool,
                              is_copy_constructible<value_type>::value>());
    }
#endif
    
    template<class... Args>
    pair<iterator, bool> emplace(Args&&... args) {
        /* 一時的にキーを構築 */
        size_type h = 0;  /* 仮のハッシュ値 */
        Node* new_node = allocate_node(h, forward<Args>(args)...);
        
        /* 実際のハッシュ値を計算 */
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            h = hasher_(new_node->value);
        } catch (...) {
            destroy_node(new_node);
            throw;
        }
#else
        h = hasher_(new_node->value);
#endif
        new_node->hash_value = h;

        ensure_bucket_storage();
        
        size_type idx = bucket_index(h);
        
        /* 既存の要素を検索 */
        for (Node* node = buckets_[idx]; node; node = node->next) {
            bool equivalent = false;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            try {
                equivalent = node->hash_value == h &&
                             key_equal_(node->value, new_node->value);
            } catch (...) {
                destroy_node(new_node);
                throw;
            }
#else
            equivalent = node->hash_value == h &&
                         key_equal_(node->value, new_node->value);
#endif
            if (equivalent) {
                /* 既に存在 */
                destroy_node(new_node);
                return pair<iterator, bool>(iterator(this, idx, node), false);
            }
        }
        
        /* 新しい要素を挿入 */
        new_node->next = buckets_[idx];
        buckets_[idx] = new_node;
        ++size_;
        
        rehash_if_needed();

        return pair<iterator, bool>(
            iterator(this, bucket_index(new_node->hash_value), new_node), true);
    }
    
    iterator erase(const_iterator pos) {
        /* A node iterator remains usable by the table that owns the node
         * after a valid swap.  Null/default iterators still require an exact
         * owner match so foreign end iterators cannot be mistaken for ours. */
        if (!pos.node_ && pos.set_ != this) return end();
        size_type idx = 0;
        Node* node = pos.node_;
        if (!locate_node(node, idx)) return end();
        
        /* 次のイテレータを準備 */
        /* ノードを削除 */
        if (buckets_[idx] == node) {
            buckets_[idx] = node->next;
        } else {
            Node* prev = buckets_[idx];
            while (prev && prev->next != node)
                prev = prev->next;
            if (prev)
                prev->next = node->next;
        }
        
        destroy_node(node);
        --size_;
        if (buckets_[idx]) return iterator(this, idx, buckets_[idx]);
        for (size_type next = idx + 1; next < bucket_count_; ++next)
            if (buckets_[next]) return iterator(this, next, buckets_[next]);
        return end();
    }

    iterator erase(const_iterator first, const_iterator last) {
        if ((!first.node_ && first.set_ != this) ||
            (!last.node_ && last.set_ != this)) return end();
        if (first.node_ && !locate_node(first.node_, first.bucket_)) return end();
        if (last.node_ && !locate_node(last.node_, last.bucket_)) return end();
        const_iterator boundary(this, last.bucket_, last.node_);
        while (first.node_ != boundary.node_) first = erase(first);
        return iterator(this, boundary.bucket_, boundary.node_);
    }

    insert_return_type insert(node_type&& node) {
        if (node.empty()) return {end(), false, node_type()};
        const size_type h = hasher_(node.m_node->value);
        ensure_bucket_storage();
        const size_type idx = bucket_index(h);
        for (Node* current = buckets_[idx]; current; current = current->next) {
            if (current->hash_value == h &&
                key_equal_(current->value, node.m_node->value)) {
                return {iterator(this, idx, current), false, move(node)};
            }
        }
        rehash_if_needed();
        const size_type destination = bucket_index(h);
        Node* adopted = nullptr;
        if (allocator_ == node.m_alloc) {
            adopted = node.m_node;
            node.m_node = nullptr;
        } else {
            adopted = insert_node_from_handle(node,
                integral_constant<bool, is_copy_constructible<value_type>::value>());
        }
        adopted->hash_value = h;
        adopted->next = buckets_[destination];
        buckets_[destination] = adopted;
        ++size_;
        return {iterator(this, destination, adopted), true, node_type()};
    }

    iterator insert(const_iterator /* hint */, node_type&& node) {
        return insert(move(node)).position;
    }

    node_type extract(const_iterator position) {
        if ((!position.node_ && position.set_ != this) || !position.node_)
            return node_type();
        size_type idx = 0;
        Node* target = position.node_;
        if (!locate_node(target, idx)) return node_type();
        if (buckets_[idx] == target) {
            buckets_[idx] = target->next;
        } else {
            Node* previous = buckets_[idx];
            while (previous && previous->next != target) previous = previous->next;
            if (!previous) return node_type();
            previous->next = target->next;
        }
        target->next = nullptr;
        --size_;
        return node_type(target, allocator_);
    }

    node_type extract(const key_type& key) {
        iterator position = find(key);
        return position == end() ? node_type() : extract(position);
    }

#if __cplusplus >= 202002L
    template<class K,
             class = typename enable_if<
                 detail::unordered_heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>::type>
    node_type extract(const K& key) {
        iterator position = find(key);
        return position == end() ? node_type() : extract(position);
    }
#endif

    void merge(unordered_set& source) {
        if (this == &source) return;
        for (size_type index = 0; index < source.bucket_count_; ++index) {
            Node* current = source.buckets_[index];
            while (current) {
                Node* next = current->next;
                if (find(current->value) == end()) {
                    node_type detached = source.extract(
                        typename unordered_set::const_iterator(
                            &source, index, current));
                    insert(std::move(detached));
                }
                current = next;
            }
        }
    }

    void merge(unordered_multiset<Key, Hash, KeyEqual, Allocator>& source);

    template<class H2, class E2, class A2>
    void merge(unordered_set<Key, H2, E2, A2>& source);

    template<class H2, class E2, class A2>
    void merge(unordered_multiset<Key, H2, E2, A2>& source);
    
    size_type erase(const key_type& key) {
        iterator it = find(key);
        if (it == end()) return 0;
        erase(it);
        return 1;
    }
    
    void swap(unordered_set& other) {
        if (this == &other) return;
        if (!allocator_traits<allocator_type>::propagate_on_container_swap::value &&
            allocator_ != other.allocator_) {
            swap_unequal(other);
            return;
        }
        swap_allocator(other,
            typename allocator_traits<allocator_type>::propagate_on_container_swap());
        detail::unordered_set_exchange(buckets_, other.buckets_);
        detail::unordered_set_exchange(bucket_count_, other.bucket_count_);
        detail::unordered_set_exchange(size_, other.size_);
        detail::unordered_set_exchange(max_load_factor_, other.max_load_factor_);
        detail::unordered_set_exchange(hasher_, other.hasher_);
        detail::unordered_set_exchange(key_equal_, other.key_equal_);
    }

private:
    void swap_allocator(unordered_set& other, true_type) noexcept {
        detail::unordered_set_exchange(allocator_, other.allocator_);
    }

    void swap_allocator(unordered_set&, false_type) noexcept {}

public:
    allocator_type get_allocator() const noexcept { return allocator_; }
    
    /* ═══════════════════════════════════════════════════════════
     * 検索
     * ═══════════════════════════════════════════════════════════*/
    
    size_type count(const key_type& key) const {
        return find(key) != end() ? 1 : 0;
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 detail::unordered_heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    size_type count(const K& key) const {
        return find(key) != end() ? 1 : 0;
    }
#endif
    
    iterator find(const key_type& key) {
        if (!buckets_ || bucket_count_ == 0) return end();
        size_type h = hasher_(key);
        size_type idx = bucket_index(h);
        
        for (Node* node = buckets_[idx]; node; node = node->next) {
            if (node->hash_value == h && key_equal_(node->value, key))
                return iterator(this, idx, node);
        }
        
        return end();
    }
    
    const_iterator find(const key_type& key) const {
        if (!buckets_ || bucket_count_ == 0) return end();
        size_type h = hasher_(key);
        size_type idx = bucket_index(h);
        
        for (Node* node = buckets_[idx]; node; node = node->next) {
            if (node->hash_value == h && key_equal_(node->value, key))
                return const_iterator(this, idx, node);
        }
        
        return end();
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 detail::unordered_heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    iterator find(const K& key) {
        if (!buckets_ || bucket_count_ == 0) return end();
        const size_type h = hasher_(key);
        const size_type idx = bucket_index(h);
        for (Node* node = buckets_[idx]; node; node = node->next) {
            if (node->hash_value == h && key_equal_(node->value, key))
                return iterator(this, idx, node);
        }
        return end();
    }

    template<typename K,
             typename = enable_if_t<
                 detail::unordered_heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    const_iterator find(const K& key) const {
        if (!buckets_ || bucket_count_ == 0) return end();
        const size_type h = hasher_(key);
        const size_type idx = bucket_index(h);
        for (Node* node = buckets_[idx]; node; node = node->next) {
            if (node->hash_value == h && key_equal_(node->value, key))
                return const_iterator(this, idx, node);
        }
        return end();
    }
#endif
    
    bool contains(const key_type& key) const {
        return find(key) != end();
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 detail::unordered_heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    bool contains(const K& key) const {
        return find(key) != end();
    }
#endif
    
    pair<iterator, iterator> equal_range(const key_type& key) {
        iterator it = find(key);
        if (it == end())
            return pair<iterator, iterator>(end(), end());
        iterator next = it;
        ++next;
        return pair<iterator, iterator>(it, next);
    }

    pair<const_iterator, const_iterator> equal_range(
        const key_type& key) const {
        const_iterator it = find(key);
        if (it == end())
            return pair<const_iterator, const_iterator>(end(), end());
        const_iterator next = it;
        ++next;
        return pair<const_iterator, const_iterator>(it, next);
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 detail::unordered_heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    pair<iterator, iterator> equal_range(const K& key) {
        iterator it = find(key);
        if (it == end())
            return pair<iterator, iterator>(end(), end());
        iterator next = it;
        ++next;
        return pair<iterator, iterator>(it, next);
    }

    template<typename K,
             typename = enable_if_t<
                 detail::unordered_heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    pair<const_iterator, const_iterator> equal_range(const K& key) const {
        const_iterator it = find(key);
        if (it == end())
            return pair<const_iterator, const_iterator>(end(), end());
        const_iterator next = it;
        ++next;
        return pair<const_iterator, const_iterator>(it, next);
    }
#endif
    
    /* ═══════════════════════════════════════════════════════════
     * バケットインターフェース
     * ═══════════════════════════════════════════════════════════*/
    
    size_type bucket_count() const noexcept { return bucket_count_; }
    size_type max_bucket_count() const noexcept { return max_size(); }
    
    size_type bucket_size(size_type n) const {
        size_type count = 0;
        for (Node* node = buckets_[n]; node; node = node->next)
            ++count;
        return count;
    }
    
    size_type bucket(const key_type& key) const {
        if (!buckets_ || bucket_count_ == 0) return 0;
        return bucket_index(hasher_(key));
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 detail::unordered_heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    size_type bucket(const K& key) const {
        if (!buckets_ || bucket_count_ == 0) return 0;
        return bucket_index(hasher_(key));
    }
#endif
    
    /* ═══════════════════════════════════════════════════════════
     * ハッシュポリシー
     * ═══════════════════════════════════════════════════════════*/
    
    float load_factor() const noexcept {
        return bucket_count_ ? static_cast<float>(size_) / bucket_count_ : 0;
    }
    
    float max_load_factor() const noexcept { return max_load_factor_; }
    void max_load_factor(float ml) {
        if (!(ml > 0.0f))
            detail::unordered_load_factor_failure();
        max_load_factor_ = ml;
    }
    
    void rehash(size_type count) {
        if (count < size_ / max_load_factor_)
            count = static_cast<size_type>(size_ / max_load_factor_);
        if (count < initial_bucket_count)
            count = initial_bucket_count;
        if (count > max_bucket_count())
            detail::unordered_length_failure();
        if (count == bucket_count_)
            return;
        
        Node** old_buckets = buckets_;
        size_type old_bucket_count = bucket_count_;
        Node** new_buckets = allocate_buckets(count);
        
        /* 全要素を再挿入 */
        for (size_type i = 0; i < old_bucket_count; ++i) {
            Node* node = old_buckets[i];
            while (node) {
                Node* next = node->next;
                size_type idx = node->hash_value % count;
                node->next = new_buckets[idx];
                new_buckets[idx] = node;
                node = next;
            }
        }
        
        buckets_ = new_buckets;
        bucket_count_ = count;
        deallocate_buckets(old_buckets, old_bucket_count);
    }
    
    void reserve(size_type count) {
        if (count > max_size())
            detail::unordered_length_failure();
        rehash(static_cast<size_type>(count / max_load_factor_) + 1);
    }
    
    /* ═══════════════════════════════════════════════════════════
     * オブザーバー
     * ═══════════════════════════════════════════════════════════*/
    
    hasher hash_function() const { return hasher_; }
    key_equal key_eq() const { return key_equal_; }
};

/* ═══════════════════════════════════════════════════════════════
 * 比較演算子
 * ═══════════════════════════════════════════════════════════════*/

template<class Key, class Hash, class KeyEqual, class Allocator>
bool operator==(const unordered_set<Key, Hash, KeyEqual, Allocator>& lhs,
                const unordered_set<Key, Hash, KeyEqual, Allocator>& rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (const auto& k : lhs) {
        if (!rhs.contains(k)) return false;
    }
    return true;
}

template<class Key, class Hash, class KeyEqual, class Allocator>
bool operator!=(const unordered_set<Key, Hash, KeyEqual, Allocator>& lhs,
                const unordered_set<Key, Hash, KeyEqual, Allocator>& rhs) {
    return !(lhs == rhs);
}

template<class Key, class Hash, class KeyEqual, class Allocator>
void swap(unordered_set<Key, Hash, KeyEqual, Allocator>& lhs,
          unordered_set<Key, Hash, KeyEqual, Allocator>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

/* ═══════════════════════════════════════════════════════════════
 * unordered_multiset (複数同一キー許可)
 * ═══════════════════════════════════════════════════════════════*/

template<
    class Key,
    class Hash = hash<Key>,
    class KeyEqual = equal_to<Key>,
    class Allocator = void
>
class unordered_multiset {
public:
    using key_type = Key;
    using value_type = Key;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = typename detail::unordered_effective_allocator<
        value_type, Allocator>::type;
    
private:
    struct Node {
        value_type value;
        Node* next;
        size_type hash_value;
        
        template<class... Args>
        Node(size_type h, Args&&... args)
            : value(forward<Args>(args)...), next(nullptr), hash_value(h) {}
    };

    using node_allocator_type = typename allocator_traits<allocator_type>::template
        rebind_alloc<Node>;
    using node_allocator_traits = allocator_traits<node_allocator_type>;
    using node_pointer = typename node_allocator_traits::pointer;

public:
    class node_type {
        Node* m_node;
        allocator_type m_alloc;

        node_type(Node* node, const allocator_type& alloc)
            : m_node(node), m_alloc(alloc) {}
        friend class unordered_multiset;

        void reset() noexcept {
            if (!m_node) return;
            node_allocator_type node_alloc(m_alloc);
            node_allocator_traits::destroy(node_alloc, m_node);
            node_allocator_traits::deallocate(
                node_alloc,
                pointer_traits<node_pointer>::pointer_to(*m_node), 1);
            m_node = nullptr;
        }

    public:
        node_type() : m_node(nullptr), m_alloc() {}
        node_type(node_type&& other) noexcept
            : m_node(other.m_node), m_alloc(move(other.m_alloc)) {
            other.m_node = nullptr;
        }
        node_type& operator=(node_type&& other) noexcept {
            if (this != &other) {
                reset();
                m_node = other.m_node;
                m_alloc = move(other.m_alloc);
                other.m_node = nullptr;
            }
            return *this;
        }
        node_type(const node_type&) = delete;
        node_type& operator=(const node_type&) = delete;
        ~node_type() { reset(); }

        bool empty() const noexcept { return m_node == nullptr; }
        explicit operator bool() const noexcept { return !empty(); }
        value_type& value() const { return m_node->value; }
        allocator_type get_allocator() const noexcept { return m_alloc; }
    };

private:
    using bucket_allocator_type = typename allocator_traits<allocator_type>::template
        rebind_alloc<Node*>;
    using bucket_allocator_traits = allocator_traits<bucket_allocator_type>;
    using bucket_pointer = typename bucket_allocator_traits::pointer;

    allocator_type allocator_;
    Node** buckets_;
    size_type bucket_count_;
    size_type size_;
    float max_load_factor_;
    Hash hasher_;
    KeyEqual key_equal_;
    
    static constexpr size_type initial_bucket_count = 8;

    Node** allocate_buckets(size_type count) {
        bucket_allocator_type bucket_alloc(allocator_);
        bucket_pointer allocation = bucket_allocator_traits::allocate(bucket_alloc, count);
        Node** buckets = detail::pointer_address(allocation);
        if (allocation == bucket_pointer() || !buckets) {
            if (allocation != bucket_pointer())
                bucket_allocator_traits::deallocate(bucket_alloc, allocation, count);
            detail::unordered_allocation_failure();
        }
        for (size_type i = 0; i < count; ++i) buckets[i] = nullptr;
        return buckets;
    }

    void deallocate_buckets(Node** buckets, size_type count) noexcept {
        if (!buckets) return;
        bucket_allocator_type bucket_alloc(allocator_);
        bucket_allocator_traits::deallocate(
            bucket_alloc, pointer_traits<bucket_pointer>::pointer_to(*buckets), count);
    }

    template<class... Args>
    Node* allocate_node(Args&&... args) {
        node_allocator_type node_alloc(allocator_);
        node_pointer allocation = node_allocator_traits::allocate(node_alloc, 1);
        Node* node = detail::pointer_address(allocation);
        if (allocation == node_pointer() || !node) {
            if (allocation != node_pointer())
                node_allocator_traits::deallocate(node_alloc, allocation, 1);
            detail::unordered_allocation_failure();
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            node_allocator_traits::construct(node_alloc, node,
                                              forward<Args>(args)...);
        } catch (...) {
            node_allocator_traits::deallocate(node_alloc, allocation, 1);
            throw;
        }
#else
        node_allocator_traits::construct(node_alloc, node,
                                          forward<Args>(args)...);
#endif
        return node;
    }

    void destroy_node(Node* node) noexcept {
        node_allocator_type node_alloc(allocator_);
        node_allocator_traits::destroy(node_alloc, node);
        node_allocator_traits::deallocate(
            node_alloc, pointer_traits<node_pointer>::pointer_to(*node), 1);
    }

    void relocate_values(unordered_multiset& source,
                         unordered_multiset& target, true_type) {
        for (const auto& value : source)
            target.insert(value);
    }

    void relocate_values(unordered_multiset& source,
                         unordered_multiset& target, false_type) {
        for (size_type index = 0; index < source.bucket_count_; ++index) {
            for (Node* node = source.buckets_[index]; node; node = node->next)
                target.insert(std::move(node->value));
        }
    }

    void swap_unequal(unordered_multiset& other) {
        if (other.size_ > max_size() || size_ > other.max_size())
            detail::unordered_length_failure();
        const size_type left_buckets = other.bucket_count_ ?
            other.bucket_count_ : initial_bucket_count;
        const size_type right_buckets = bucket_count_ ?
            bucket_count_ : initial_bucket_count;
        unordered_multiset left(left_buckets, other.hasher_, other.key_equal_, allocator_);
        unordered_multiset right(right_buckets, hasher_, key_equal_, other.allocator_);
        left.max_load_factor_ = other.max_load_factor_;
        right.max_load_factor_ = max_load_factor_;
        relocate_values(other, left, is_copy_constructible<value_type>());
        relocate_values(*this, right, is_copy_constructible<value_type>());
        swap(left);
        other.swap(right);
    }

    void ensure_bucket_storage() {
        if (buckets_ && bucket_count_ != 0) return;
        bucket_count_ = initial_bucket_count;
        buckets_ = allocate_buckets(bucket_count_);
    }

    bool locate_node(const Node* target, size_type& bucket) const noexcept {
        if (!target || !buckets_) return false;
        for (size_type i = 0; i < bucket_count_; ++i) {
            for (const Node* node = buckets_[i]; node; node = node->next) {
                if (node == target) {
                    bucket = i;
                    return true;
                }
            }
        }
        return false;
    }

    Node* insert_node_from_handle(node_type& node, true_type) {
        Node* replacement = allocate_node(node.m_node->hash_value,
                                          node.m_node->value);
        node.reset();
        return replacement;
    }

    Node* insert_node_from_handle(node_type& node, false_type) {
        Node* replacement = allocate_node(node.m_node->hash_value,
                                          move(node.m_node->value));
        node.reset();
        return replacement;
    }

    void commit_copy_assignment(unordered_multiset& candidate, true_type) {
        hasher_ = candidate.hasher_;
        key_equal_ = candidate.key_equal_;
        max_load_factor_ = candidate.max_load_factor_;
        clear();
        deallocate_buckets(buckets_, bucket_count_);
        allocator_ = candidate.allocator_;
        buckets_ = candidate.buckets_;
        bucket_count_ = candidate.bucket_count_;
        size_ = candidate.size_;
        candidate.buckets_ = nullptr;
        candidate.bucket_count_ = 0;
        candidate.size_ = 0;
    }

    void commit_copy_assignment(unordered_multiset& candidate, false_type) {
        swap(candidate);
    }

#if __cplusplus > 202002L
    /* The multi-container uses the same allocator-owned candidate table but
     * intentionally retains every equivalent key from the input range. */
    template<class R>
    void insert_range_impl(R&& range, true_type) {
        const size_type candidate_buckets =
            bucket_count_ ? bucket_count_ : initial_bucket_count;
        unordered_multiset candidate(
            candidate_buckets, hasher_, key_equal_, allocator_);
        candidate.max_load_factor_ = max_load_factor_;
        for (const auto& value : *this)
            candidate.insert(value);
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        for (; first != last; ++first)
            candidate.insert(*first);

        clear();
        deallocate_buckets(buckets_, bucket_count_);
        buckets_ = candidate.buckets_;
        bucket_count_ = candidate.bucket_count_;
        size_ = candidate.size_;
        candidate.buckets_ = nullptr;
        candidate.bucket_count_ = 0;
        candidate.size_ = 0;
    }

    template<class R>
    void insert_range_impl(R&& range, false_type) {
        /* As with unordered_set, move-only keys use a private candidate table
         * and publish only after all existing and incoming keys are present.
         * This is a valid basic guarantee, not the copyable strong guarantee. */
        const size_type candidate_buckets =
            bucket_count_ ? bucket_count_ : initial_bucket_count;
        unordered_multiset candidate(
            candidate_buckets, hasher_, key_equal_, allocator_);
        candidate.max_load_factor_ = max_load_factor_;
        for (size_type index = 0; index < bucket_count_; ++index) {
            for (Node* node = buckets_[index]; node; node = node->next)
                candidate.insert(std::move(node->value));
        }
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        for (; first != last; ++first)
            candidate.insert(std::move(*first));

        clear();
        deallocate_buckets(buckets_, bucket_count_);
        buckets_ = candidate.buckets_;
        bucket_count_ = candidate.bucket_count_;
        size_ = candidate.size_;
        candidate.buckets_ = nullptr;
        candidate.bucket_count_ = 0;
        candidate.size_ = 0;
    }
#endif
    
public:
    class iterator {
        friend class unordered_multiset;
        const unordered_multiset* set_;
        size_type bucket_;
        Node* node_;
        
    public:
        using iterator_category = forward_iterator_tag;
        using value_type = Key;
        using difference_type = ptrdiff_t;
        using pointer = const Key*;
        using reference = const Key&;
        
        iterator() : set_(nullptr), bucket_(0), node_(nullptr) {}
        iterator(const unordered_multiset* s, size_type b, Node* n)
            : set_(s), bucket_(b), node_(n) {}
        
        reference operator*() const { return node_->value; }
        pointer operator->() const { return &node_->value; }
        
        iterator& operator++() {
            if (node_) node_ = node_->next;
            while (!node_ && bucket_ + 1 < set_->bucket_count_) {
                ++bucket_;
                node_ = set_->buckets_[bucket_];
            }
            return *this;
        }
        
        iterator operator++(int) { iterator t = *this; ++(*this); return t; }
        
        bool operator==(const iterator& o) const {
            // Bucket is traversal state, not identity: rehash may change it.
            return set_ == o.set_ && node_ == o.node_;
        }
        bool operator!=(const iterator& o) const { return !(*this == o); }
    };
    
    using const_iterator = iterator;
    
    unordered_multiset() 
        : allocator_()
        , bucket_count_(initial_bucket_count)
        , size_(0)
        , max_load_factor_(1.0f) 
    {
        buckets_ = allocate_buckets(bucket_count_);
    }

    explicit unordered_multiset(const allocator_type& alloc)
        : allocator_(alloc)
        , bucket_count_(initial_bucket_count)
        , size_(0)
        , max_load_factor_(1.0f)
    {
        buckets_ = allocate_buckets(bucket_count_);
    }

    unordered_multiset(size_type bucket_count,
                       const Hash& hash,
                       const KeyEqual& equal,
                       const allocator_type& alloc)
        : allocator_(alloc)
        , bucket_count_(bucket_count ? bucket_count : initial_bucket_count)
        , size_(0)
        , max_load_factor_(1.0f)
        , hasher_(hash)
        , key_equal_(equal)
    {
        buckets_ = allocate_buckets(bucket_count_);
    }

    template<class InputIt,
             class = typename std::enable_if<!is_integral<InputIt>::value>::type>
    unordered_multiset(InputIt first, InputIt last,
                       size_type bucket_count = initial_bucket_count,
                       const Hash& hash = Hash(),
                       const KeyEqual& equal = KeyEqual(),
                       const allocator_type& alloc = allocator_type())
        : unordered_multiset(bucket_count, hash, equal, alloc) {
        insert(first, last);
    }

    unordered_multiset(initializer_list<value_type> init,
                       size_type bucket_count = initial_bucket_count,
                       const Hash& hash = Hash(),
                       const KeyEqual& equal = KeyEqual(),
                       const allocator_type& alloc = allocator_type())
        : unordered_multiset(bucket_count, hash, equal, alloc) {
        insert(init);
    }

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, value_type>
    unordered_multiset(from_range_t, R&& range,
                       size_type bucket_count = initial_bucket_count,
                       const Hash& hash = Hash(),
                       const KeyEqual& equal = KeyEqual(),
                       const allocator_type& alloc = allocator_type())
        : unordered_multiset(bucket_count, hash, equal, alloc) {
        insert_range(std::forward<R>(range));
    }
#endif

    unordered_multiset(const unordered_multiset& other)
        : allocator_(allocator_traits<allocator_type>::select_on_container_copy_construction(other.allocator_))
        , bucket_count_(other.bucket_count_)
        , size_(0)
        , max_load_factor_(other.max_load_factor_)
        , hasher_(other.hasher_)
        , key_equal_(other.key_equal_)
    {
        buckets_ = allocate_buckets(bucket_count_ ? bucket_count_ : initial_bucket_count);
        if (!bucket_count_) bucket_count_ = initial_bucket_count;
        for (const auto& value : other) insert(value);
    }

    unordered_multiset(unordered_multiset&& other) noexcept
        : allocator_(move(other.allocator_))
        , buckets_(other.buckets_)
        , bucket_count_(other.bucket_count_)
        , size_(other.size_)
        , max_load_factor_(other.max_load_factor_)
        , hasher_(move(other.hasher_))
        , key_equal_(move(other.key_equal_))
    {
        other.buckets_ = nullptr;
        other.bucket_count_ = 0;
        other.size_ = 0;
    }
    
    ~unordered_multiset() {
        clear();
        deallocate_buckets(buckets_, bucket_count_);
    }
    
    void clear() noexcept {
        for (size_type i = 0; i < bucket_count_; ++i) {
            Node* node = buckets_[i];
            while (node) {
                Node* next = node->next;
                destroy_node(node);
                node = next;
            }
            buckets_[i] = nullptr;
        }
        size_ = 0;
    }
    
    iterator insert(const value_type& value) {
        size_type h = hasher_(value);
        ensure_bucket_storage();
        size_type idx = h % bucket_count_;

        Node* last_equivalent = nullptr;
        for (Node* current = buckets_[idx]; current; current = current->next) {
            if (current->hash_value == h && key_equal_(current->value, value))
                last_equivalent = current;
        }
        
        Node* node = allocate_node(h, value);
        if (last_equivalent) {
            node->next = last_equivalent->next;
            last_equivalent->next = node;
        } else {
            node->next = buckets_[idx];
            buckets_[idx] = node;
        }
        ++size_;
        
        return iterator(this, idx, node);
    }

    iterator insert(value_type&& value) {
        const size_type h = hasher_(value);
        ensure_bucket_storage();
        const size_type idx = h % bucket_count_;
        Node* last_equivalent = nullptr;
        for (Node* current = buckets_[idx]; current; current = current->next) {
            if (current->hash_value == h && key_equal_(current->value, value))
                last_equivalent = current;
        }
        Node* node = allocate_node(h, std::move(value));
        if (last_equivalent) {
            node->next = last_equivalent->next;
            last_equivalent->next = node;
        } else {
            node->next = buckets_[idx];
            buckets_[idx] = node;
        }
        ++size_;
        return iterator(this, idx, node);
    }

    iterator insert(node_type&& node) {
        if (node.empty()) return end();
        const size_type h = hasher_(node.m_node->value);
        ensure_bucket_storage();
        const size_type idx = h % bucket_count_;
        Node* last_equivalent = nullptr;
        for (Node* current = buckets_[idx]; current; current = current->next) {
            if (current->hash_value == h &&
                key_equal_(current->value, node.m_node->value))
                last_equivalent = current;
        }
        Node* adopted = nullptr;
        if (allocator_ == node.m_alloc) {
            adopted = node.m_node;
            node.m_node = nullptr;
        } else {
            adopted = insert_node_from_handle(node,
                integral_constant<bool, is_copy_constructible<value_type>::value>());
        }
        adopted->hash_value = h;
        if (last_equivalent) {
            adopted->next = last_equivalent->next;
            last_equivalent->next = adopted;
        } else {
            adopted->next = buckets_[idx];
            buckets_[idx] = adopted;
        }
        ++size_;
        return iterator(this, idx, adopted);
    }

    iterator insert(const_iterator /* hint */, node_type&& node) {
        return insert(move(node));
    }

    node_type extract(const_iterator position) {
        if ((!position.node_ && position.set_ != this) || !position.node_)
            return node_type();
        size_type idx = 0;
        Node* target = position.node_;
        if (!locate_node(target, idx)) return node_type();
        if (buckets_[idx] == target) {
            buckets_[idx] = target->next;
        } else {
            Node* previous = buckets_[idx];
            while (previous && previous->next != target) previous = previous->next;
            if (!previous) return node_type();
            previous->next = target->next;
        }
        target->next = nullptr;
        --size_;
        return node_type(target, allocator_);
    }

    node_type extract(const key_type& key) {
        iterator position = find(key);
        return position == end() ? node_type() : extract(position);
    }

#if __cplusplus >= 202002L
    template<class K,
             class = typename enable_if<
                 detail::unordered_heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>::type>
    node_type extract(const K& key) {
        iterator position = find(key);
        return position == end() ? node_type() : extract(position);
    }
#endif

    void merge(unordered_multiset& source) {
        if (this == &source) return;
        for (size_type index = 0; index < source.bucket_count_; ++index) {
            Node* current = source.buckets_[index];
            while (current) {
                Node* next = current->next;
                node_type detached = source.extract(
                    typename unordered_multiset::const_iterator(
                        &source, index, current));
                insert(std::move(detached));
                current = next;
            }
        }
    }

    void merge(unordered_set<Key, Hash, KeyEqual, Allocator>& source);

    template<class H2, class E2, class A2>
    void merge(unordered_set<Key, H2, E2, A2>& source);

    template<class H2, class E2, class A2>
    void merge(unordered_multiset<Key, H2, E2, A2>& source);

    iterator insert(const_iterator, const value_type& value) {
        return insert(value);
    }

    iterator insert(const_iterator, value_type&& value) {
        return insert(std::move(value));
    }

    template<class InputIt>
    void insert(InputIt first, InputIt last) {
        for (; first != last; ++first) insert(*first);
    }

    void insert(initializer_list<value_type> init) {
        insert(init.begin(), init.end());
    }

#if __cplusplus > 202002L
    template<class R>
        requires detail::container_compatible_range<R, value_type>
    void insert_range(R&& range) {
        insert_range_impl(std::forward<R>(range),
                          integral_constant<bool,
                              is_copy_constructible<value_type>::value>());
    }
#endif

    template<class... Args>
    iterator emplace(Args&&... args) {
        value_type value(std::forward<Args>(args)...);
        return insert(std::move(value));
    }

    template<class... Args>
    iterator emplace_hint(const_iterator, Args&&... args) {
        return emplace(std::forward<Args>(args)...);
    }
    
    size_type count(const key_type& key) const {
        if (!buckets_ || bucket_count_ == 0) return 0;
        size_type h = hasher_(key);
        size_type idx = h % bucket_count_;
        size_type cnt = 0;
        
        for (Node* node = buckets_[idx]; node; node = node->next) {
            if (node->hash_value == h && key_equal_(node->value, key))
                ++cnt;
        }
        return cnt;
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 detail::unordered_heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    size_type count(const K& key) const {
        if (!buckets_ || bucket_count_ == 0) return 0;
        const size_type h = hasher_(key);
        const size_type idx = h % bucket_count_;
        size_type count = 0;
        for (Node* node = buckets_[idx]; node; node = node->next) {
            if (node->hash_value == h && key_equal_(node->value, key))
                ++count;
        }
        return count;
    }
#endif

    iterator erase(const_iterator pos) {
        if (!pos.node_ && pos.set_ != this) return end();
        size_type idx = 0;
        Node* node = pos.node_;
        if (!locate_node(node, idx)) return end();
        Node* previous = nullptr;
        Node* current = buckets_[idx];
        while (current && current != node) {
            previous = current;
            current = current->next;
        }
        if (!current) return end();
        if (previous) previous->next = current->next;
        else buckets_[idx] = current->next;
        destroy_node(current);
        --size_;
        if (buckets_[idx]) return iterator(this, idx, buckets_[idx]);
        for (size_type next = idx + 1; next < bucket_count_; ++next)
            if (buckets_[next]) return iterator(this, next, buckets_[next]);
        return end();
    }

    iterator erase(const_iterator first, const_iterator last) {
        if ((!first.node_ && first.set_ != this) ||
            (!last.node_ && last.set_ != this)) return end();
        if (first.node_ && !locate_node(first.node_, first.bucket_)) return end();
        if (last.node_ && !locate_node(last.node_, last.bucket_)) return end();
        const_iterator boundary(this, last.bucket_, last.node_);
        while (first.node_ != boundary.node_) first = erase(first);
        return iterator(this, boundary.bucket_, boundary.node_);
    }

    size_type erase(const key_type& key) {
        if (!buckets_ || bucket_count_ == 0) return 0;
        const size_type h = hasher_(key);
        const size_type idx = h % bucket_count_;
        size_type removed = 0u;
        Node* previous = nullptr;
        Node* current = buckets_[idx];
        while (current) {
            Node* next = current->next;
            if (current->hash_value == h && key_equal_(current->value, key)) {
                if (previous) previous->next = next;
                else buckets_[idx] = next;
                destroy_node(current);
                --size_;
                ++removed;
            } else {
                previous = current;
            }
            current = next;
        }
        return removed;
    }

    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type max_size() const noexcept {
        return allocator_traits<allocator_type>::max_size(allocator_);
    }

    iterator find(const key_type& key) {
        if (!buckets_ || bucket_count_ == 0) return end();
        const size_type h = hasher_(key);
        const size_type idx = h % bucket_count_;
        for (Node* node = buckets_[idx]; node; node = node->next) {
            if (node->hash_value == h && key_equal_(node->value, key))
                return iterator(this, idx, node);
        }
        return end();
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 detail::unordered_heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    iterator find(const K& key) {
        if (!buckets_ || bucket_count_ == 0) return end();
        const size_type h = hasher_(key);
        const size_type idx = h % bucket_count_;
        for (Node* node = buckets_[idx]; node; node = node->next) {
            if (node->hash_value == h && key_equal_(node->value, key))
                return iterator(this, idx, node);
        }
        return end();
    }

    template<typename K,
             typename = enable_if_t<
                 detail::unordered_heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    const_iterator find(const K& key) const {
        return const_cast<unordered_multiset*>(this)->find(key);
    }
#endif

    const_iterator find(const key_type& key) const {
        return const_cast<unordered_multiset*>(this)->find(key);
    }

    bool contains(const key_type& key) const {
        return find(key) != end();
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 detail::unordered_heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    bool contains(const K& key) const {
        return find(key) != end();
    }
#endif

    pair<iterator, iterator> equal_range(const key_type& key) {
        iterator first = find(key);
        if (first == end()) return pair<iterator, iterator>(end(), end());
        iterator last = first;
        const size_type h = hasher_(key);
        ++last;
        while (last != end() && last.node_ && last.node_->hash_value == h &&
               key_equal_(last.node_->value, key)) {
            ++last;
        }
        return pair<iterator, iterator>(first, last);
    }

    pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
        return const_cast<unordered_multiset*>(this)->equal_range(key);
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 detail::unordered_heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    pair<iterator, iterator> equal_range(const K& key) {
        iterator first = find(key);
        if (first == end()) return pair<iterator, iterator>(end(), end());
        iterator last = first;
        const size_type h = hasher_(key);
        ++last;
        while (last != end() && last.node_ && last.node_->hash_value == h &&
               key_equal_(last.node_->value, key)) {
            ++last;
        }
        return pair<iterator, iterator>(first, last);
    }

    template<typename K,
             typename = enable_if_t<
                 detail::unordered_heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    pair<const_iterator, const_iterator> equal_range(const K& key) const {
        return const_cast<unordered_multiset*>(this)->equal_range(key);
    }
#endif

    size_type bucket_count() const noexcept { return bucket_count_; }
    size_type max_bucket_count() const noexcept { return max_size(); }

    size_type bucket_size(size_type n) const {
        if (!buckets_ || n >= bucket_count_) return 0;
        size_type count = 0;
        for (Node* node = buckets_[n]; node; node = node->next) ++count;
        return count;
    }

    size_type bucket(const key_type& key) const {
        if (!buckets_ || bucket_count_ == 0) return 0;
        return hasher_(key) % bucket_count_;
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 detail::unordered_heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    size_type bucket(const K& key) const {
        if (!buckets_ || bucket_count_ == 0) return 0;
        return hasher_(key) % bucket_count_;
    }
#endif

    float load_factor() const noexcept {
        return bucket_count_ ? static_cast<float>(size_) / bucket_count_ : 0.0f;
    }

    float max_load_factor() const noexcept { return max_load_factor_; }
    void max_load_factor(float value) {
        if (!(value > 0.0f))
            detail::unordered_load_factor_failure();
        max_load_factor_ = value;
    }

    void rehash(size_type count) {
        if (count < initial_bucket_count) count = initial_bucket_count;
        if (max_load_factor_ > 0.0f &&
            count < static_cast<size_type>(size_ / max_load_factor_)) {
            count = static_cast<size_type>(size_ / max_load_factor_);
        }
        if (count > max_bucket_count())
            detail::unordered_length_failure();
        if (count == bucket_count_) return;
        Node** old_buckets = buckets_;
        const size_type old_bucket_count = bucket_count_;
        Node** new_buckets = allocate_buckets(count);
        for (size_type i = 0; i < old_bucket_count; ++i) {
            Node* node = old_buckets[i];
            while (node) {
                Node* next = node->next;
                const size_type idx = node->hash_value % count;
                node->next = new_buckets[idx];
                new_buckets[idx] = node;
                node = next;
            }
        }
        buckets_ = new_buckets;
        bucket_count_ = count;
        deallocate_buckets(old_buckets, old_bucket_count);
    }

    void reserve(size_type count) {
        if (count > max_size())
            detail::unordered_length_failure();
        if (max_load_factor_ > 0.0f)
            rehash(static_cast<size_type>(count / max_load_factor_) + 1);
        else
            rehash(count);
    }

    hasher hash_function() const { return hasher_; }
    key_equal key_eq() const { return key_equal_; }
    
    iterator begin() noexcept {
        for (size_type i = 0; i < bucket_count_; ++i) {
            if (buckets_[i]) return iterator(this, i, buckets_[i]);
        }
        return end();
    }
    
    iterator end() noexcept { return iterator(this, bucket_count_, nullptr); }
    const_iterator begin() const noexcept {
        for (size_type i = 0; i < bucket_count_; ++i) {
            if (buckets_[i]) return const_iterator(this, i, buckets_[i]);
        }
        return end();
    }
    const_iterator end() const noexcept { return const_iterator(this, bucket_count_, nullptr); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    unordered_multiset& operator=(const unordered_multiset& other) {
        if (this != &other) {
            using propagate = typename allocator_traits<allocator_type>::
                propagate_on_container_copy_assignment;
            unordered_multiset candidate(
                other.bucket_count_, other.hasher_, other.key_equal_,
                propagate::value ? other.allocator_ : allocator_);
            candidate.max_load_factor_ = other.max_load_factor_;
            for (const auto& value : other) candidate.insert(value);
            commit_copy_assignment(candidate, propagate());
        }
        return *this;
    }

    unordered_multiset& operator=(unordered_multiset&& other) noexcept {
        if (this != &other) {
            using propagate = typename allocator_traits<allocator_type>::
                propagate_on_container_move_assignment;
            if (propagate::value || allocator_ == other.allocator_) {
                clear();
                deallocate_buckets(buckets_, bucket_count_);
                if (propagate::value) allocator_ = move(other.allocator_);
                buckets_ = other.buckets_;
                bucket_count_ = other.bucket_count_;
                size_ = other.size_;
                max_load_factor_ = other.max_load_factor_;
                hasher_ = move(other.hasher_);
                key_equal_ = move(other.key_equal_);
                other.buckets_ = nullptr;
                other.bucket_count_ = 0;
                other.size_ = 0;
            } else {
                clear();
                hasher_ = other.hasher_;
                key_equal_ = other.key_equal_;
                max_load_factor_ = other.max_load_factor_;
                for (size_type index = 0; index < other.bucket_count_; ++index) {
                    for (Node* node = other.buckets_[index]; node;
                         node = node->next) {
                        insert(std::move(node->value));
                    }
                }
                other.clear();
            }
        }
        return *this;
    }

    void swap(unordered_multiset& other) {
        if (this == &other) return;
        if (!allocator_traits<allocator_type>::propagate_on_container_swap::value &&
            allocator_ != other.allocator_) {
            swap_unequal(other);
            return;
        }
        swap_allocator(other,
            typename allocator_traits<allocator_type>::propagate_on_container_swap());
        detail::unordered_set_exchange(buckets_, other.buckets_);
        detail::unordered_set_exchange(bucket_count_, other.bucket_count_);
        detail::unordered_set_exchange(size_, other.size_);
        detail::unordered_set_exchange(max_load_factor_, other.max_load_factor_);
        detail::unordered_set_exchange(hasher_, other.hasher_);
        detail::unordered_set_exchange(key_equal_, other.key_equal_);
    }

private:
    void swap_allocator(unordered_multiset& other, true_type) noexcept {
        detail::unordered_set_exchange(allocator_, other.allocator_);
    }

    void swap_allocator(unordered_multiset&, false_type) noexcept {}

public:
    allocator_type get_allocator() const noexcept { return allocator_; }
};

template<class Key, class Hash, class KeyEqual, class Allocator>
void unordered_set<Key, Hash, KeyEqual, Allocator>::merge(
    unordered_multiset<Key, Hash, KeyEqual, Allocator>& source) {
    typedef unordered_multiset<Key, Hash, KeyEqual, Allocator> source_type;
    for (typename source_type::iterator it = source.begin();
         it != source.end();) {
        typename source_type::iterator next = it;
        ++next;
        if (find(*it) == end()) {
            typename source_type::node_type node = source.extract(it);
            value_type value(std::move(node.value()));
            insert(std::move(value));
        }
        it = next;
    }
}

template<class Key, class Hash, class KeyEqual, class Allocator>
void unordered_multiset<Key, Hash, KeyEqual, Allocator>::merge(
    unordered_set<Key, Hash, KeyEqual, Allocator>& source) {
    typedef unordered_set<Key, Hash, KeyEqual, Allocator> source_type;
    for (typename source_type::iterator it = source.begin();
         it != source.end();) {
        typename source_type::iterator next = it;
        ++next;
        typename source_type::node_type node = source.extract(it);
        value_type value(std::move(node.value()));
        insert(std::move(value));
        it = next;
    }
}

template<class Key, class Hash, class KeyEqual, class Allocator>
template<class H2, class E2, class A2>
void unordered_set<Key, Hash, KeyEqual, Allocator>::merge(
    unordered_set<Key, H2, E2, A2>& source) {
    typedef unordered_set<Key, H2, E2, A2> source_type;
    for (typename source_type::iterator it = source.begin();
         it != source.end();) {
        typename source_type::iterator next = it;
        ++next;
        if (find(*it) == end()) {
            typename source_type::node_type node = source.extract(it);
            value_type value(std::move(node.value()));
            insert(std::move(value));
        }
        it = next;
    }
}

template<class Key, class Hash, class KeyEqual, class Allocator>
template<class H2, class E2, class A2>
void unordered_set<Key, Hash, KeyEqual, Allocator>::merge(
    unordered_multiset<Key, H2, E2, A2>& source) {
    typedef unordered_multiset<Key, H2, E2, A2> source_type;
    for (typename source_type::iterator it = source.begin();
         it != source.end();) {
        typename source_type::iterator next = it;
        ++next;
        if (find(*it) == end()) {
            typename source_type::node_type node = source.extract(it);
            value_type value(std::move(node.value()));
            insert(std::move(value));
        }
        it = next;
    }
}

template<class Key, class Hash, class KeyEqual, class Allocator>
template<class H2, class E2, class A2>
void unordered_multiset<Key, Hash, KeyEqual, Allocator>::merge(
    unordered_set<Key, H2, E2, A2>& source) {
    typedef unordered_set<Key, H2, E2, A2> source_type;
    for (typename source_type::iterator it = source.begin();
         it != source.end();) {
        typename source_type::iterator next = it;
        ++next;
        typename source_type::node_type node = source.extract(it);
        value_type value(std::move(node.value()));
        insert(std::move(value));
        it = next;
    }
}

template<class Key, class Hash, class KeyEqual, class Allocator>
template<class H2, class E2, class A2>
void unordered_multiset<Key, Hash, KeyEqual, Allocator>::merge(
    unordered_multiset<Key, H2, E2, A2>& source) {
    typedef unordered_multiset<Key, H2, E2, A2> source_type;
    for (typename source_type::iterator it = source.begin();
         it != source.end();) {
        typename source_type::iterator next = it;
        ++next;
        typename source_type::node_type node = source.extract(it);
        value_type value(std::move(node.value()));
        insert(std::move(value));
        it = next;
    }
}

template<class Key, class Hash, class KeyEqual, class Allocator>
bool operator==(const unordered_multiset<Key, Hash, KeyEqual, Allocator>& lhs,
                const unordered_multiset<Key, Hash, KeyEqual, Allocator>& rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (const auto& value : lhs) {
        if (lhs.count(value) != rhs.count(value)) return false;
    }
    return true;
}

template<class Key, class Hash, class KeyEqual, class Allocator>
bool operator!=(const unordered_multiset<Key, Hash, KeyEqual, Allocator>& lhs,
                const unordered_multiset<Key, Hash, KeyEqual, Allocator>& rhs) {
    return !(lhs == rhs);
}

template<class Key, class Hash, class KeyEqual, class Allocator>
void swap(unordered_multiset<Key, Hash, KeyEqual, Allocator>& lhs,
          unordered_multiset<Key, Hash, KeyEqual, Allocator>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

#if __cplusplus >= 202002L
template<class Key, class Hash, class KeyEqual, class Allocator>
typename unordered_set<Key, Hash, KeyEqual, Allocator>::size_type
erase(unordered_set<Key, Hash, KeyEqual, Allocator>& value,
      const Key& key) {
    return value.erase(key);
}

template<class Key, class Hash, class KeyEqual, class Allocator, class Pred>
typename unordered_set<Key, Hash, KeyEqual, Allocator>::size_type
erase_if(unordered_set<Key, Hash, KeyEqual, Allocator>& value, Pred predicate) {
    typename unordered_set<Key, Hash, KeyEqual, Allocator>::size_type removed = 0u;
    for (typename unordered_set<Key, Hash, KeyEqual, Allocator>::iterator it =
             value.begin(); it != value.end();) {
        if (predicate(*it)) {
            it = value.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

template<class Key, class Hash, class KeyEqual, class Allocator>
typename unordered_multiset<Key, Hash, KeyEqual, Allocator>::size_type
erase(unordered_multiset<Key, Hash, KeyEqual, Allocator>& value,
      const Key& key) {
    return value.erase(key);
}

template<class Key, class Hash, class KeyEqual, class Allocator, class Pred>
typename unordered_multiset<Key, Hash, KeyEqual, Allocator>::size_type
erase_if(unordered_multiset<Key, Hash, KeyEqual, Allocator>& value,
         Pred predicate) {
    typename unordered_multiset<Key, Hash, KeyEqual, Allocator>::size_type removed = 0u;
    for (typename unordered_multiset<Key, Hash, KeyEqual, Allocator>::iterator it =
             value.begin(); it != value.end();) {
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

#endif /* RINCXX_UNORDERED_SET_H */
