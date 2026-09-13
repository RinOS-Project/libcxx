/*
 * RinOS String Interning
 * 文字列の重複排除テーブル
 * 同一文字列は同一ポインタを返す → ポインタ比較で高速一致判定
 */

#ifndef RINCXX_INTERN_H
#define RINCXX_INTERN_H

#include "rincxx.h"
#include "arena.h"
#include "cstdint.h"
#if defined(__cplusplus) && __cplusplus >= 201703L
#include "string_view.h"
#endif

#ifdef __cplusplus

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * string_intern_table - ハッシュテーブルベースのstring intern
 *
 * arenaバッキング: Entryとそのデータはarenaに格納。
 * arenaのreset/破棄で全エントリが一括解放される。
 * ═══════════════════════════════════════════════════════════════*/

class string_intern_table {
public:
    static constexpr size_t BUCKETS = 128;

private:
    struct Entry {
        Entry* next;       /* ハッシュチェーン */
        uint32_t hash;
        uint16_t length;
        /* data: Entryの直後にlength+1バイト（null終端含む）を配置 */
        const char* data() const {
            return reinterpret_cast<const char*>(this + 1);
        }
        char* data_mut() {
            return reinterpret_cast<char*>(this + 1);
        }
    };

    Entry* m_buckets[BUCKETS];
    arena m_storage;
    size_t m_count;

    static uint32_t fnv1a(const char* s, size_t len) {
        uint32_t h = 2166136261u;
        for (size_t i = 0; i < len; i++) {
            h ^= static_cast<uint8_t>(s[i]);
            h *= 16777619u;
        }
        return h;
    }

public:
    string_intern_table()
        : m_storage(16 * 1024), m_count(0)
    {
        for (size_t i = 0; i < BUCKETS; i++) {
            m_buckets[i] = nullptr;
        }
    }

    /* 文字列をintern。同一文字列は同一ポインタを返す。 */
    const char* intern(const char* s, size_t len) {
        if (!s || len == 0) return "";

        uint32_t h = fnv1a(s, len);
        size_t bucket = h & (BUCKETS - 1);

        /* 既存エントリを検索 */
        Entry* e = m_buckets[bucket];
        while (e) {
            if (e->hash == h && e->length == (uint16_t)len) {
                const char* edata = e->data();
                bool match = true;
                for (size_t i = 0; i < len; i++) {
                    if (edata[i] != s[i]) { match = false; break; }
                }
                if (match) return edata;
            }
            e = e->next;
        }

        /* 新規エントリを作成（arenaに割り当て） */
        size_t alloc_size = sizeof(Entry) + len + 1;
        void* mem = m_storage.allocate(alloc_size);
        Entry* ne = static_cast<Entry*>(mem);
        ne->hash = h;
        ne->length = (uint16_t)len;
        ne->next = m_buckets[bucket];

        char* dst = ne->data_mut();
        for (size_t i = 0; i < len; i++) dst[i] = s[i];
        dst[len] = '\0';

        m_buckets[bucket] = ne;
        m_count++;

        return ne->data();
    }

    const char* intern(const char* s) {
        if (!s) return "";
        size_t len = 0;
        while (s[len]) len++;
        return intern(s, len);
    }

 #if __cplusplus >= 201703L
    const char* intern(string_view sv) {
        return intern(sv.data(), sv.size());
    }
 #endif

    size_t count() const { return m_count; }

    /* テーブルリセット（arenaごと） */
    void reset() {
        for (size_t i = 0; i < BUCKETS; i++) {
            m_buckets[i] = nullptr;
        }
        m_storage.reset();
        m_count = 0;
    }
};

/* ═══════════════════════════════════════════════════════════════
 * interned_sv - internedポインタを持つ軽量文字列参照
 *
 * 同一intern tableから得たinterned_sv同士はポインタ比較で一致判定可能。
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 201703L

class interned_sv {
    const char* m_ptr;
    size_t m_len;

public:
    interned_sv() : m_ptr(nullptr), m_len(0) {}
    interned_sv(const char* p, size_t l) : m_ptr(p), m_len(l) {}

    /* ポインタ比較（同一intern tableからのもの同士） */
    bool operator==(const interned_sv& o) const { return m_ptr == o.m_ptr; }
    bool operator!=(const interned_sv& o) const { return m_ptr != o.m_ptr; }

    /* string_viewとの文字列比較 */
    bool operator==(string_view sv) const {
        if (m_len != sv.size()) return false;
        for (size_t i = 0; i < m_len; i++) {
            if (m_ptr[i] != sv[i]) return false;
        }
        return true;
    }
    bool operator!=(string_view sv) const { return !(*this == sv); }

    /* const char*との文字列比較 */
    bool operator==(const char* s) const {
        if (!s) return m_len == 0;
        size_t i = 0;
        for (; i < m_len && s[i]; i++) {
            if (m_ptr[i] != s[i]) return false;
        }
        return i == m_len && s[i] == '\0';
    }
    bool operator!=(const char* s) const { return !(*this == s); }

    const char* data() const { return m_ptr; }
    size_t size() const { return m_len; }
    size_t length() const { return m_len; }
    bool empty() const { return m_len == 0 || m_ptr == nullptr; }

    /* std::stringへの明示的変換 */
    operator string_view() const { return string_view(m_ptr, m_len); }
};

#endif /* __cplusplus >= 201703L */

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_INTERN_H */
