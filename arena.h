/*
 * RinOS Arena Allocator
 * バルク割り当て＋一括解放のアリーナアロケータ
 * CSS/HTML/JSパーサー等の一時データに最適
 */

#ifndef RINCXX_ARENA_H
#define RINCXX_ARENA_H

#include "rincxx.h"
#include "type_traits.h"

#ifdef __cplusplus

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * arena - バンプアロケータ（一括解放型）
 *
 * 大きなチャンクを確保し、そこからバンプポインタで高速割り当て。
 * 個別解放は不可、reset()で全チャンクを一括再利用。
 * ═══════════════════════════════════════════════════════════════*/

class arena {
public:
    static constexpr size_t DEFAULT_CHUNK_SIZE = 64 * 1024;  /* 64KB */
    static constexpr size_t ALIGN = alignof(max_align_t);    /* 16 on 64-bit */

private:
    struct Chunk {
        Chunk* next;
        size_t capacity;  /* data()の使用可能バイト数 */
        size_t used;

        char* data() { return reinterpret_cast<char*>(this + 1); }
    };

    Chunk* m_current;     /* 現在割り当て中のチャンク */
    Chunk* m_first;       /* 最初のチャンク（reset時に辿る） */
    size_t m_chunk_size;  /* デフォルトチャンクサイズ */

    Chunk* alloc_chunk(size_t min_data_size) {
        size_t data_size = min_data_size > m_chunk_size ? min_data_size : m_chunk_size;
        size_t total = sizeof(Chunk) + data_size;
        void* mem = rin_malloc(total);
        if (!mem) return nullptr;

        Chunk* c = static_cast<Chunk*>(mem);
        c->next = nullptr;
        c->capacity = data_size;
        c->used = 0;
        return c;
    }

public:
    explicit arena(size_t chunk_size = DEFAULT_CHUNK_SIZE)
        : m_current(nullptr), m_first(nullptr), m_chunk_size(chunk_size)
    {
        m_first = alloc_chunk(chunk_size);
        m_current = m_first;
    }

    ~arena() {
        Chunk* c = m_first;
        while (c) {
            Chunk* next = c->next;
            rin_free(c);
            c = next;
        }
    }

    /* コピー禁止 */
    arena(const arena&) = delete;
    arena& operator=(const arena&) = delete;

    /* アライメント付きバンプ割り当て */
    void* allocate(size_t size) {
        size_t aligned_size = (size + ALIGN - 1) & ~(ALIGN - 1);

        if (m_current) {
            size_t avail = m_current->capacity - m_current->used;
            if (aligned_size <= avail) {
                void* ptr = m_current->data() + m_current->used;
                m_current->used += aligned_size;
                return ptr;
            }
        }

        /* 現チャンクに収まらない → 新チャンク割り当て */
        Chunk* nc = alloc_chunk(aligned_size);
        if (!nc) {
            rin_panic("arena: out of memory");
            return nullptr;
        }
        if (m_current) {
            m_current->next = nc;
        } else {
            m_first = nc;
        }
        m_current = nc;

        void* ptr = nc->data();
        nc->used = aligned_size;
        return ptr;
    }

    /* 全チャンクの used を 0 にリセット（メモリは保持、再利用） */
    void reset() {
        Chunk* c = m_first;
        while (c) {
            c->used = 0;
            c = c->next;
        }
        m_current = m_first;
    }
};

/* ═══════════════════════════════════════════════════════════════
 * arena_allocator<T> - std::allocator互換ラッパー
 *
 * vector<T, arena_allocator<T>> 等で使用可能。
 * deallocate()はno-op。arena::reset()で一括解放。
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
class arena_allocator {
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;

    arena* m_arena;

    explicit arena_allocator(arena* a) noexcept : m_arena(a) {}

    template<typename U>
    arena_allocator(const arena_allocator<U>& o) noexcept : m_arena(o.m_arena) {}

    pointer allocate(size_type n) {
        return static_cast<pointer>(m_arena->allocate(n * sizeof(T)));
    }

    void deallocate(pointer, size_type) noexcept {
        /* no-op: arena::reset()で一括解放 */
    }

    template<typename U>
    struct rebind { using other = arena_allocator<U>; };

    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        ::new (static_cast<void*>(p)) U(static_cast<Args&&>(args)...);
    }

    template<typename U>
    void destroy(U* p) { p->~U(); }

    arena* get_arena() const noexcept { return m_arena; }
};

template<typename T, typename U>
bool operator==(const arena_allocator<T>& a, const arena_allocator<U>& b) noexcept {
    return a.m_arena == b.m_arena;
}

template<typename T, typename U>
bool operator!=(const arena_allocator<T>& a, const arena_allocator<U>& b) noexcept {
    return a.m_arena != b.m_arena;
}

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_ARENA_H */
