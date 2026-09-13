/*
 * RinOS C++ <bitset> ✿
 * ビットセット - fixed-width algebra and validated conversion subset
 */

#ifndef RINCXX_BITSET_H
#define RINCXX_BITSET_H

#include "rincxx.h"
#include "exception.h"
#include "functional.h"
#include "istream.h"
#include "string.h"
#include "cstring.h"

namespace std {

#if __cplusplus >= 201402L
#define RIN_BITSET_CONSTEXPR14 constexpr
#else
#define RIN_BITSET_CONSTEXPR14 inline
#endif

/* P2417R2 makes the bitset object model constexpr in C++23.  Keep this
 * separate from the C++14 spelling above: the string-taking construction and
 * to_string() still depend on the library string's allocation model and must
 * not make the C++23 feature-test promise prematurely. */
#if __cplusplus > 202002L
#define RIN_BITSET_CONSTEXPR23 constexpr
#else
#define RIN_BITSET_CONSTEXPR23 inline
#endif

namespace detail {

[[noreturn]] inline void bitset_invalid_argument_fail() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw invalid_argument("bitset string contains an invalid character");
#else
    __builtin_trap();
#endif
}

[[noreturn]] inline void bitset_out_of_range_fail() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw out_of_range("bitset position is out of range");
#else
    __builtin_trap();
#endif
}

[[noreturn]] inline void bitset_overflow_fail() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw overflow_error("bitset value does not fit the destination type");
#else
    __builtin_trap();
#endif
}

[[noreturn]] inline void bitset_allocation_fail() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_alloc();
#else
    __builtin_trap();
#endif
}

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * bitset クラス
 * ═══════════════════════════════════════════════════════════════*/

template<size_t N>
class bitset {
public:
    /* ═══════════════════════════════════════════════════════════
     * reference クラス（ビットへの参照）
     * ═══════════════════════════════════════════════════════════*/
    
    class reference {
        friend class bitset;
        bitset* bitset_;
        size_t pos_;
        
        RIN_BITSET_CONSTEXPR23 reference(bitset* bs, size_t pos)
            : bitset_(bs), pos_(pos) {}
        
    public:
        reference(const reference&) noexcept = default;

        RIN_BITSET_CONSTEXPR23 reference& operator=(bool value) noexcept {
            bitset_->set(pos_, value);
            return *this;
        }
        
        RIN_BITSET_CONSTEXPR23 reference& operator=(
            const reference& other) noexcept {
            bitset_->set(pos_, static_cast<bool>(other));
            return *this;
        }
        
        RIN_BITSET_CONSTEXPR23 operator bool() const noexcept {
            return bitset_->test(pos_);
        }
        
        RIN_BITSET_CONSTEXPR23 bool operator~() const noexcept {
            return !bitset_->test(pos_);
        }
        
        RIN_BITSET_CONSTEXPR23 reference& flip() noexcept {
            bitset_->flip(pos_);
            return *this;
        }
    };
    
private:
    /* 内部ストレージ */
    using word_type = unsigned long;
    static constexpr size_t bits_per_word = sizeof(word_type) * 8;
    static constexpr size_t num_words = (N + bits_per_word - 1) / bits_per_word;
    
    word_type data_[num_words > 0 ? num_words : 1];
    
    /* 余分なビットをクリア */
    RIN_BITSET_CONSTEXPR14 void sanitize() {
        if (N % bits_per_word != 0) {
            data_[num_words - 1] &= (static_cast<word_type>(1) << (N % bits_per_word)) - 1;
        }
    }

    /* ワードとビット位置を計算 */
    static constexpr size_t word_index(size_t pos) { return pos / bits_per_word; }
    static constexpr size_t bit_index(size_t pos) { return pos % bits_per_word; }
    static constexpr word_type bit_mask(size_t pos) {
        return static_cast<word_type>(1) << bit_index(pos);
    }
    
public:
    /* ═══════════════════════════════════════════════════════════
     * コンストラクタ
     * ═══════════════════════════════════════════════════════════*/
    
    RIN_BITSET_CONSTEXPR14 bitset() noexcept {
        for (size_t i = 0; i < num_words; ++i)
            data_[i] = 0;
    }
    
    RIN_BITSET_CONSTEXPR14 bitset(unsigned long long val) noexcept : bitset() {
        if (N > 0) {
            if (sizeof(unsigned long long) <= sizeof(word_type)) {
                data_[0] = static_cast<word_type>(val);
            } else {
                for (size_t i = 0; i < num_words && val != 0; ++i) {
                    data_[i] = static_cast<word_type>(val);
                    constexpr size_t source_word_bits =
                        sizeof(unsigned long long) * 8u;
                    constexpr size_t shift_amount =
                        bits_per_word < source_word_bits ? bits_per_word : 0u;
                    val >>= shift_amount;
                }
            }
            sanitize();
        }
    }
    
    template<typename CharT, typename Traits, typename Allocator>
    explicit bitset(const basic_string<CharT, Traits, Allocator>& str,
                    size_t pos = 0,
                    size_t n = basic_string<CharT, Traits, Allocator>::npos,
                    CharT zero = CharT('0'), CharT one = CharT('1'))
        : bitset() {
        if (pos > str.size()) {
            detail::bitset_out_of_range_fail();
        }
        const size_t available = str.size() - pos;
        const size_t selected = n < available ? n : available;
        const size_t used = N < selected ? N : selected;

        /* The standard maps the first min(N, selected) characters. */
        for (size_t offset = 0; offset < used; ++offset) {
            const CharT c = str[pos + offset];
            if (c == one) {
                set(used - offset - 1);
            } else if (c != zero) {
                detail::bitset_invalid_argument_fail();
            }
        }
    }

      template<typename CharT>
      RIN_BITSET_CONSTEXPR23 explicit bitset(
          const CharT* str, size_t n = string::npos,
          CharT zero = CharT('0'), CharT one = CharT('1'))
          : bitset() {
          if (!str) {
              detail::bitset_invalid_argument_fail();
          }

          /* Only code units that can affect this bitset are observed.  The
           * default npos form stops at NUL, while a finite count treats NUL
           * as an input character and validates it.  In both cases the
           * standard's leading-substring rule limits the work to N units. */
          size_t selected = 0u;
          if (n == string::npos) {
              while (selected < N && str[selected] != CharT(0)) ++selected;
          } else {
              selected = n < N ? n : N;
          }
          for (size_t offset = 0u; offset < selected; ++offset) {
              const CharT c = str[offset];
              if (c == one) {
                  set(selected - offset - 1u);
              } else if (c != zero) {
                  detail::bitset_invalid_argument_fail();
              }
          }
      }

    /* ═══════════════════════════════════════════════════════════
     * 要素アクセス
     * ═══════════════════════════════════════════════════════════*/
    
    RIN_BITSET_CONSTEXPR23 bool operator[](size_t pos) const noexcept {
        return test(pos);
    }
    
    RIN_BITSET_CONSTEXPR23 reference operator[](size_t pos) noexcept {
        return reference(this, pos);
    }
    
    RIN_BITSET_CONSTEXPR23 bool test(size_t pos) const {
        if (pos >= N) detail::bitset_out_of_range_fail();
        return (data_[word_index(pos)] & bit_mask(pos)) != 0;
    }
    
    /* ═══════════════════════════════════════════════════════════
     * 容量
     * ═══════════════════════════════════════════════════════════*/
    
    constexpr size_t size() const noexcept { return N; }
    
    /* セットされたビット数をカウント */
    RIN_BITSET_CONSTEXPR23 size_t count() const noexcept {
        size_t result = 0;
        for (size_t i = 0; i < num_words; ++i) {
            word_type w = data_[i];
            /* ポップカウント */
            while (w) {
                result += w & 1;
                w >>= 1;
            }
        }
        return result;
    }
    
    RIN_BITSET_CONSTEXPR23 bool all() const noexcept {
        if (N == 0) return true;
        
        /* 完全なワードをチェック */
        for (size_t i = 0; i < num_words - 1; ++i) {
            if (data_[i] != ~static_cast<word_type>(0))
                return false;
        }
        
        /* 最後のワード */
        if (N % bits_per_word == 0) {
            return data_[num_words - 1] == ~static_cast<word_type>(0);
        } else {
            word_type mask = (static_cast<word_type>(1) << (N % bits_per_word)) - 1;
            return data_[num_words - 1] == mask;
        }
    }
    
    RIN_BITSET_CONSTEXPR23 bool any() const noexcept {
        for (size_t i = 0; i < num_words; ++i) {
            if (data_[i] != 0) return true;
        }
        return false;
    }
    
    RIN_BITSET_CONSTEXPR23 bool none() const noexcept {
        return !any();
    }
    
    /* ═══════════════════════════════════════════════════════════
     * 変更
     * ═══════════════════════════════════════════════════════════*/
    
    RIN_BITSET_CONSTEXPR23 bitset& set() noexcept {
        for (size_t i = 0; i < num_words; ++i)
            data_[i] = ~static_cast<word_type>(0);
        sanitize();
        return *this;
    }
    
    RIN_BITSET_CONSTEXPR23 bitset& set(size_t pos, bool value = true) {
        if (pos >= N) detail::bitset_out_of_range_fail();
        if (value)
            data_[word_index(pos)] |= bit_mask(pos);
        else
            data_[word_index(pos)] &= ~bit_mask(pos);
        return *this;
    }
    
    RIN_BITSET_CONSTEXPR23 bitset& reset() noexcept {
        for (size_t i = 0; i < num_words; ++i)
            data_[i] = 0;
        return *this;
    }
    
    RIN_BITSET_CONSTEXPR23 bitset& reset(size_t pos) {
        return set(pos, false);
    }
    
    RIN_BITSET_CONSTEXPR23 bitset& flip() noexcept {
        for (size_t i = 0; i < num_words; ++i)
            data_[i] = ~data_[i];
        sanitize();
        return *this;
    }
    
    RIN_BITSET_CONSTEXPR23 bitset& flip(size_t pos) {
        if (pos >= N) detail::bitset_out_of_range_fail();
        data_[word_index(pos)] ^= bit_mask(pos);
        return *this;
    }
    
    /* ═══════════════════════════════════════════════════════════
     * 変換
     * ═══════════════════════════════════════════════════════════*/
    
    RIN_BITSET_CONSTEXPR23 unsigned long to_ulong() const {
        if (num_words == 0) return 0;
        
        /* オーバーフローチェック */
        for (size_t i = 1; i < num_words; ++i) {
            if (data_[i] != 0) {
                detail::bitset_overflow_fail();
            }
        }
        
        return static_cast<unsigned long>(data_[0]);
    }
    
    RIN_BITSET_CONSTEXPR23 unsigned long long to_ullong() const {
        if (num_words == 0) return 0;

        constexpr size_t value_bits = sizeof(unsigned long long) * 8;
        constexpr size_t value_words =
            (value_bits + bits_per_word - 1) / bits_per_word;
        for (size_t i = value_words; i < num_words; ++i) {
            if (data_[i] != 0) {
                detail::bitset_overflow_fail();
            }
        }

        unsigned long long result = 0;
        size_t words_to_use = value_words;
        if (words_to_use > num_words) words_to_use = num_words;
        
        for (size_t i = 0; i < words_to_use; ++i) {
            result |= static_cast<unsigned long long>(data_[i]) 
                      << (i * bits_per_word);
        }
        
        return result;
    }
    
    template<typename CharT = char,
             typename Traits = char_traits<CharT>,
             typename Allocator = default_string_allocator<CharT>>
    basic_string<CharT, Traits, Allocator>
    to_string(CharT zero = CharT('0'), CharT one = CharT('1')) const {
        basic_string<CharT, Traits, Allocator> result;
        result.reserve(N);
        /* basic_string's no-exception owner retains the old value when an
         * allocation fails.  A freshly-created result would otherwise look
         * like a successful empty conversion, so close that path explicitly
         * instead of publishing a truncated bit string. */
        if (result.capacity() < N) {
            detail::bitset_allocation_fail();
        }
        for (size_t i = N; i > 0; --i) {
            result.push_back(test(i - 1) ? one : zero);
        }
        return result;
    }
    
    /* ═══════════════════════════════════════════════════════════
     * ビット演算
     * ═══════════════════════════════════════════════════════════*/
    
    RIN_BITSET_CONSTEXPR23 bitset operator~() const noexcept {
        bitset result = *this;
        result.flip();
        return result;
    }
    
    RIN_BITSET_CONSTEXPR23 bitset& operator&=(const bitset& other) noexcept {
        for (size_t i = 0; i < num_words; ++i)
            data_[i] &= other.data_[i];
        return *this;
    }
    
    RIN_BITSET_CONSTEXPR23 bitset& operator|=(const bitset& other) noexcept {
        for (size_t i = 0; i < num_words; ++i)
            data_[i] |= other.data_[i];
        return *this;
    }
    
    RIN_BITSET_CONSTEXPR23 bitset& operator^=(const bitset& other) noexcept {
        for (size_t i = 0; i < num_words; ++i)
            data_[i] ^= other.data_[i];
        return *this;
    }
    
    RIN_BITSET_CONSTEXPR23 bitset operator<<(size_t pos) const noexcept {
        bitset result;
        if (N > 0) {
            if (pos >= N) return result;

            size_t word_shift = pos / bits_per_word;
            size_t bit_shift = pos % bits_per_word;

            if (bit_shift == 0) {
                for (size_t i = num_words - 1; i >= word_shift; --i) {
                    result.data_[i] = data_[i - word_shift];
                    if (i == word_shift) break;
                }
            } else {
                for (size_t i = num_words - 1; i > word_shift; --i) {
                    result.data_[i] = (data_[i - word_shift] << bit_shift) |
                                      (data_[i - word_shift - 1] >> (bits_per_word - bit_shift));
                }
                result.data_[word_shift] = data_[0] << bit_shift;
            }

            result.sanitize();
        }
        return result;
    }
    
    RIN_BITSET_CONSTEXPR23 bitset& operator<<=(size_t pos) noexcept {
        *this = *this << pos;
        return *this;
    }
    
    RIN_BITSET_CONSTEXPR23 bitset operator>>(size_t pos) const noexcept {
        bitset result;
        if (N > 0) {
            if (pos >= N) return result;

            size_t word_shift = pos / bits_per_word;
            size_t bit_shift = pos % bits_per_word;

            if (bit_shift == 0) {
                for (size_t i = 0; i < num_words - word_shift; ++i) {
                    result.data_[i] = data_[i + word_shift];
                }
            } else {
                for (size_t i = 0; i < num_words - word_shift - 1; ++i) {
                    result.data_[i] = (data_[i + word_shift] >> bit_shift) |
                                      (data_[i + word_shift + 1] << (bits_per_word - bit_shift));
                }
                result.data_[num_words - word_shift - 1] =
                    data_[num_words - 1] >> bit_shift;
            }
        }
        return result;
    }
    
    RIN_BITSET_CONSTEXPR23 bitset& operator>>=(size_t pos) noexcept {
        *this = *this >> pos;
        return *this;
    }
    
    /* ═══════════════════════════════════════════════════════════
     * 比較
     * ═══════════════════════════════════════════════════════════*/
    
    RIN_BITSET_CONSTEXPR23 bool operator==(const bitset& other) const noexcept {
        for (size_t i = 0; i < num_words; ++i) {
            if (data_[i] != other.data_[i]) return false;
        }
        return true;
    }
    
    RIN_BITSET_CONSTEXPR23 bool operator!=(const bitset& other) const noexcept {
        return !(*this == other);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * 非メンバ関数
 * ═══════════════════════════════════════════════════════════════*/

template<size_t N>
RIN_BITSET_CONSTEXPR23 bitset<N> operator&(
    const bitset<N>& lhs, const bitset<N>& rhs) noexcept {
    bitset<N> result = lhs;
    result &= rhs;
    return result;
}

template<size_t N>
RIN_BITSET_CONSTEXPR23 bitset<N> operator|(
    const bitset<N>& lhs, const bitset<N>& rhs) noexcept {
    bitset<N> result = lhs;
    result |= rhs;
    return result;
}

template<size_t N>
RIN_BITSET_CONSTEXPR23 bitset<N> operator^(
    const bitset<N>& lhs, const bitset<N>& rhs) noexcept {
    bitset<N> result = lhs;
    result ^= rhs;
    return result;
}

/*
 * Formatted bitset output follows the same width/fill ownership as the
 * arithmetic inserters.  A bitset has no sign or base prefix, so `internal`
 * has the standard right-adjusted result.
 */
template<typename CharT, typename Traits, size_t N>
basic_ostream<CharT, Traits>& operator<<(
    basic_ostream<CharT, Traits>& stream, const bitset<N>& value)
{
    typename basic_ostream<CharT, Traits>::sentry gate(stream);
    if (!gate) return stream;
    if (!stream.rdbuf()) {
        stream.setstate(ios_base::badbit);
        return stream;
    }
    const streamsize requested_width = stream.width(0);
    const size_t padding = requested_width > 0 &&
        static_cast<unsigned long long>(requested_width) > N
        ? static_cast<size_t>(requested_width - static_cast<streamsize>(N))
        : 0u;
    const ios_base::fmtflags adjustment =
        stream.flags() & ios_base::adjustfield;
    const locale selected_locale = stream.getloc();
    const ctype<CharT>& characters =
        use_facet<ctype<CharT>>(selected_locale);
    const CharT zero = characters.widen('0');
    const CharT one = characters.widen('1');
    const auto write_fill = [&](size_t count) {
        for (size_t index = 0u; index < count; ++index) {
            stream.put(stream.fill());
            if (!stream.good()) return false;
        }
        return true;
    };
    const auto write_bits = [&] {
        for (size_t index = N; index != 0u; --index) {
            stream.put(value.test(index - 1u)
                ? one : zero);
            if (!stream.good()) return false;
        }
        return true;
    };

    if (adjustment == ios_base::left) {
        if (!write_bits()) return stream;
        (void)write_fill(padding);
    } else {
        if (!write_fill(padding)) return stream;
        (void)write_bits();
    }
    return stream;
}

template<typename CharT, typename Traits, size_t N>
basic_istream<CharT, Traits>& operator>>(
    basic_istream<CharT, Traits>& stream, bitset<N>& value)
{
    typename basic_istream<CharT, Traits>::sentry gate(stream);
    if (!gate) return stream;

    bitset<N> candidate;
    size_t extracted = 0u;
    /* Formatted extraction honors the stream width as a maximum number of
     * code units, then resets it just like the other string-like inserters.
     * A zero (or negative) width means the bitset's full N-character limit. */
    const streamsize requested_width = stream.width(0);
    size_t extraction_limit = N;
    if (requested_width > 0 &&
        static_cast<unsigned long long>(requested_width) <
            static_cast<unsigned long long>(extraction_limit)) {
        extraction_limit = static_cast<size_t>(requested_width);
    }
    basic_streambuf<CharT, Traits>* buffer = stream.rdbuf();
    if (!buffer) {
        stream.setstate(ios_base::badbit);
        return stream;
    }
    const locale selected_locale = stream.getloc();
    const ctype<CharT>& characters =
        use_facet<ctype<CharT>>(selected_locale);
    const CharT zero = characters.widen('0');
    const CharT one = characters.widen('1');
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
    while (extracted < extraction_limit) {
        const typename Traits::int_type current = buffer->sgetc();
        if (Traits::eq_int_type(current, Traits::eof())) {
            stream.setstate(ios_base::eofbit);
            break;
        }
        const CharT character = Traits::to_char_type(current);
        if (!Traits::eq(character, zero) && !Traits::eq(character, one)) {
            break;
        }
        const typename Traits::int_type consumed = buffer->sbumpc();
        if (Traits::eq_int_type(consumed, Traits::eof()) ||
            !Traits::eq_int_type(consumed, current)) {
            stream.setstate(ios_base::badbit);
            return stream;
        }
        candidate <<= 1u;
        if (Traits::eq(character, one)) candidate.set(0u);
        ++extracted;
    }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    } catch (...) {
        if (stream.__setstate_from_callback(ios_base::badbit)) throw;
        return stream;
    }
#endif

    if (extracted == 0u && N != 0u) {
        stream.setstate(ios_base::failbit);
    } else {
        value = candidate;
    }
    return stream;
}

template<size_t N>
struct hash<bitset<N>> {
    size_t operator()(const bitset<N>& value) const noexcept {
        size_t result = sizeof(size_t) == 8
            ? static_cast<size_t>(14695981039346656037ull)
            : static_cast<size_t>(2166136261u);
        const size_t prime = sizeof(size_t) == 8
            ? static_cast<size_t>(1099511628211ull)
            : static_cast<size_t>(16777619u);
        for (size_t index = 0; index < N; ++index) {
            result ^= static_cast<size_t>(value[index]);
            result *= prime;
        }
        return result;
    }
};

} /* namespace std */

#undef RIN_BITSET_CONSTEXPR14
#undef RIN_BITSET_CONSTEXPR23

#endif /* RINCXX_BITSET_H */
