/*
 * RinOS User-space Object Pool (Slab Allocator)
 * 固定サイズオブジェクトの高速プール
 *
 * 使い方:
 *   std::object_pool<MyType> pool;
 *   MyType* obj = pool.alloc();
 *   pool.free(obj);
 */

#ifndef RINCXX_SLAB_H
#define RINCXX_SLAB_H

#include "rincxx.h"

#ifdef __cplusplus

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * object_pool<T> - 固定型オブジェクトプール
 *
 * ページ単位(4KB)でメモリを確保し、T型オブジェクト用の
 * フリーリストを管理。alloc/freeがO(1)。
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
class object_pool {
    static constexpr size_t PAGE_SIZE = 4096;
    static constexpr size_t HEADER_SIZE = sizeof(void*);  /* PageHeader.next */

    /* フリーノード（解放済みオブジェクト領域にオーバーレイ） */
    struct FreeNode {
        FreeNode* next;
    };

    /* ページヘッダ */
    struct PageHeader {
        PageHeader* next;
        /* 残りの領域がオブジェクト格納エリア */
    };

    /* オブジェクトサイズ（最低でもFreeNodeが入るサイズ） */
    static constexpr size_t OBJ_SIZE =
        sizeof(T) >= sizeof(FreeNode) ? sizeof(T) : sizeof(FreeNode);

    /* アライメント: max(alignof(T), alignof(FreeNode)) */
    static constexpr size_t OBJ_ALIGN =
        alignof(T) >= alignof(FreeNode) ? alignof(T) : alignof(FreeNode);

    /* アライメント済みオブジェクトサイズ */
    static constexpr size_t ALIGNED_OBJ = (OBJ_SIZE + OBJ_ALIGN - 1) & ~(OBJ_ALIGN - 1);

    /* ヘッダ後の使用可能領域 */
    static constexpr size_t ALIGNED_HEADER = (sizeof(PageHeader) + OBJ_ALIGN - 1) & ~(OBJ_ALIGN - 1);
    static constexpr size_t USABLE = PAGE_SIZE > ALIGNED_HEADER ? PAGE_SIZE - ALIGNED_HEADER : 0;

    /* ページあたりオブジェクト数 */
    static constexpr size_t OBJS_PER_PAGE = USABLE / ALIGNED_OBJ > 0 ? USABLE / ALIGNED_OBJ : 1;

    /* 実際のページサイズ（大きいオブジェクトはPAGE_SIZEを超える） */
    static constexpr size_t ACTUAL_PAGE_SIZE =
        OBJS_PER_PAGE > 0 && USABLE >= ALIGNED_OBJ
            ? PAGE_SIZE
            : ALIGNED_HEADER + ALIGNED_OBJ;

    FreeNode* m_free;
    PageHeader* m_pages;
    size_t m_active;
    size_t m_total;

    void grow() {
        void* mem = rin_malloc(ACTUAL_PAGE_SIZE);
        if (!mem) return;

        PageHeader* page = static_cast<PageHeader*>(mem);
        page->next = m_pages;
        m_pages = page;

        char* obj_area = static_cast<char*>(mem) + ALIGNED_HEADER;
        for (size_t i = 0; i < OBJS_PER_PAGE; i++) {
            FreeNode* node = reinterpret_cast<FreeNode*>(obj_area + i * ALIGNED_OBJ);
            node->next = m_free;
            m_free = node;
        }
        m_total += OBJS_PER_PAGE;
    }

public:
    object_pool() : m_free(nullptr), m_pages(nullptr), m_active(0), m_total(0) {}

    ~object_pool() {
        PageHeader* p = m_pages;
        while (p) {
            PageHeader* next = p->next;
            rin_free(p);
            p = next;
        }
    }

    /* コピー禁止 */
    object_pool(const object_pool&) = delete;
    object_pool& operator=(const object_pool&) = delete;

    /* 未初期化メモリを割り当て（コンストラクタは呼ばない） */
    void* alloc_raw() {
        if (!m_free) grow();
        if (!m_free) return nullptr;

        FreeNode* node = m_free;
        m_free = node->next;
        m_active++;
        return static_cast<void*>(node);
    }

    /* オブジェクトを割り当ててコンストラクト */
    template<typename... Args>
    T* alloc(Args&&... args) {
        void* mem = alloc_raw();
        if (!mem) return nullptr;
        return new (mem) T(static_cast<Args&&>(args)...);
    }

    /* オブジェクトをデストラクトしてフリーリストに返却 */
    void free(T* obj) {
        if (!obj) return;
        obj->~T();
        FreeNode* node = reinterpret_cast<FreeNode*>(obj);
        node->next = m_free;
        m_free = node;
        m_active--;
    }

    /* 統計 */
    size_t active() const { return m_active; }
    size_t total() const { return m_total; }
};

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_SLAB_H */
