/*
 * RinOS C++ Initializer List ✿
 * std::initializer_list 互換実装
 */

#ifndef RINCXX_INITIALIZER_LIST_H
#define RINCXX_INITIALIZER_LIST_H

#ifdef __cplusplus

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * initializer_list<T>
 * 
 * 注意: このクラスはコンパイラが特別に扱います。
 * メンバーのレイアウトはABIで固定されています。
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
class initializer_list {
public:
    using value_type = T;
    using reference = const T&;
    using const_reference = const T&;
    using size_type = __SIZE_TYPE__;
    using iterator = const T*;
    using const_iterator = const T*;
    
private:
    /* コンパイラが設定する - 順序とサイズは固定！ */
    const T* begin_;
    size_type size_;
    
    /* コンパイラ用のプライベートコンストラクタ */
    constexpr initializer_list(const T* begin, size_type size) noexcept
        : begin_(begin), size_(size) {}
    
public:
    /* デフォルトコンストラクタ */
    constexpr initializer_list() noexcept : begin_(nullptr), size_(0) {}
    
    /* サイズ */
    constexpr size_type size() const noexcept { return size_; }
    
    /* イテレータ */
    constexpr const_iterator begin() const noexcept { return begin_; }
    constexpr const_iterator end() const noexcept {
        return size_ == 0 ? begin_ : begin_ + size_;
    }
};

/* begin/end 非メンバ関数 */
template<typename T>
constexpr const T* begin(initializer_list<T> il) noexcept {
    return il.begin();
}

template<typename T>
constexpr const T* end(initializer_list<T> il) noexcept {
    return il.end();
}

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_INITIALIZER_LIST_H */
