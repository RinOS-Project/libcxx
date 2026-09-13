/*
 * RinOS C++ <istream> ✿
 * 入力ストリーム
 */

#ifndef RINCXX_ISTREAM_H
#define RINCXX_ISTREAM_H

#include "rincxx.h"
#include "ios.h"
#include "streambuf.h"
#include "ostream.h"
#include "string.h"
#include "cstdlib.h"
#if __cplusplus >= 201703L
#include "charconv.h"
#endif

namespace std {

namespace __istream_detail {

/* The C++11 floating compatibility parser needs a source-index map while it
 * normalizes locale separators.  Keep that map off the call stack just like
 * the numeric candidate itself. */
struct numeric_source_index_buffer {
    size_t* data;

    explicit numeric_source_index_buffer(size_t count)
        : data(static_cast<size_t*>(rin_malloc(count * sizeof(size_t)))) {}

    numeric_source_index_buffer(const numeric_source_index_buffer&) = delete;
    numeric_source_index_buffer& operator=(
        const numeric_source_index_buffer&) = delete;

    ~numeric_source_index_buffer() {
        if (data) rin_free(data);
    }

    bool valid() const noexcept { return data != nullptr; }
};

} /* namespace __istream_detail */

/* ═══════════════════════════════════════════════════════════════
 * basic_istream
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename Traits = char_traits<CharT>>
class basic_istream : virtual public basic_ios<CharT, Traits> {
    streamsize gcount_;
    bool tie_flush_active_;

    bool is_space(CharT value) const {
        const locale current_locale = this->getloc();
        return use_facet<ctype<CharT>>(current_locale).is(
            ctype_base::space, value);
    }

    bool consume_observed(basic_streambuf<CharT, Traits>* buffer,
                          typename Traits::int_type expected) {
        typename Traits::int_type consumed;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            consumed = buffer->sbumpc();
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return false;
        }
#else
        consumed = buffer->sbumpc();
#endif
        if (Traits::eq_int_type(consumed, Traits::eof()) ||
            !Traits::eq_int_type(consumed, expected)) {
            this->setstate(ios_base::badbit);
            return false;
        }
        return true;
    }

    bool observe_current(basic_streambuf<CharT, Traits>* buffer,
                         typename Traits::int_type& observed) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            observed = buffer->sgetc();
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return false;
        }
#else
        observed = buffer->sgetc();
#endif
        return true;
    }

    bool consume_next(basic_streambuf<CharT, Traits>* buffer,
                      typename Traits::int_type& consumed) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            consumed = buffer->sbumpc();
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return false;
        }
#else
        consumed = buffer->sbumpc();
#endif
        return true;
    }

    bool putback_observed(basic_streambuf<CharT, Traits>* buffer,
                          CharT value,
                          typename Traits::int_type& result) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            result = buffer->sputbackc(value);
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return false;
        }
#else
        result = buffer->sputbackc(value);
#endif
        return true;
    }

    bool write_observed(basic_streambuf<CharT, Traits>* buffer,
                        CharT value,
                        typename Traits::int_type& result) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            result = buffer->sputc(value);
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return false;
        }
#else
        result = buffer->sputc(value);
#endif
        return true;
    }

    bool restore_observed(basic_streambuf<CharT, Traits>* buffer,
                          CharT value,
                          typename Traits::int_type& result) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            result = buffer->sputbackc(value);
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return false;
        }
#else
        result = buffer->sputbackc(value);
#endif
        return true;
    }

    bool unget_observed(basic_streambuf<CharT, Traits>* buffer,
                        typename Traits::int_type& result) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            result = buffer->sungetc();
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return false;
        }
#else
        result = buffer->sungetc();
#endif
        return true;
    }

    bool read_block(basic_streambuf<CharT, Traits>* buffer,
                    CharT* destination, streamsize requested,
                    streamsize& transferred) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            transferred = buffer->sgetn(destination, requested);
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return false;
        }
#else
        transferred = buffer->sgetn(destination, requested);
#endif
        return true;
    }

    bool available_count(basic_streambuf<CharT, Traits>* buffer,
                         streamsize& available) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            available = buffer->in_avail();
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return false;
        }
#else
        available = buffer->in_avail();
#endif
        return true;
    }

    bool seek_position_observed(basic_streambuf<CharT, Traits>* buffer,
                                typename Traits::pos_type position,
                                typename Traits::pos_type& result) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            result = buffer->pubseekpos(position, ios_base::in);
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return false;
        }
#else
        result = buffer->pubseekpos(position, ios_base::in);
#endif
        return true;
    }

    bool seek_offset_observed(basic_streambuf<CharT, Traits>* buffer,
                              typename Traits::off_type offset,
                              ios_base::seekdir direction,
                              typename Traits::pos_type& result) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            result = buffer->pubseekoff(offset, direction, ios_base::in);
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return false;
        }
#else
        result = buffer->pubseekoff(offset, direction, ios_base::in);
#endif
        return true;
    }

    bool sync_observed(basic_streambuf<CharT, Traits>* buffer, int& result) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            result = buffer->pubsync();
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return false;
        }
#else
        result = buffer->pubsync();
#endif
        return true;
    }

    class tie_flush_guard {
        basic_istream& stream_;
        bool owns_active_;
    public:
        explicit tie_flush_guard(basic_istream& stream)
            : stream_(stream), owns_active_(!stream.tie_flush_active_) {
            if (owns_active_) {
                stream_.tie_flush_active_ = true;
                if (stream_.good()) {
                    basic_ostream<CharT, Traits>* tied = stream_.tie();
                    if (tied) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                        try {
                            tied->flush();
                        } catch (...) {
                            if (stream_.__setstate_from_callback(
                                    ios_base::badbit)) {
                                throw;
                            }
                        }
#else
                        tied->flush();
#endif
                    }
                }
            }
        }

        ~tie_flush_guard() {
            if (owns_active_) stream_.tie_flush_active_ = false;
        }

        tie_flush_guard(const tie_flush_guard&) = delete;
        tie_flush_guard& operator=(const tie_flush_guard&) = delete;
    };

    class gcount_guard {
        basic_istream& stream_;
        streamsize saved_;
    public:
        explicit gcount_guard(basic_istream& stream)
            : stream_(stream), saved_(stream.gcount_) {}
        ~gcount_guard() { stream_.gcount_ = saved_; }
    };

    template<typename OtherCharT, typename OtherTraits>
    friend basic_istream<OtherCharT, OtherTraits>& getline(
        basic_istream<OtherCharT, OtherTraits>&,
        basic_string<OtherCharT>&, OtherCharT);

public:
    using char_type = CharT;
    using traits_type = Traits;
    using int_type = typename Traits::int_type;
    using pos_type = typename Traits::pos_type;
    using off_type = typename Traits::off_type;

    class sentry {
        bool ok_;

    public:
        explicit sentry(basic_istream& stream, bool noskipws = false)
            : ok_(false) {
            if (!stream.good()) {
                // An already-failed stream is a sentry precondition failure.
                // Preserve its state and avoid re-evaluating the exception mask.
                return;
            }
            tie_flush_guard tie_guard(stream);
            basic_streambuf<CharT, Traits>* buffer = stream.rdbuf();
            if (!stream.good()) {
                stream.setstate(ios_base::failbit);
                return;
            }
            if (!buffer) {
                stream.setstate(ios_base::badbit | ios_base::failbit);
                return;
            }
            if (!noskipws && (stream.flags() & ios_base::skipws)) {
                for (;;) {
                    int_type current;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                    try {
                        current = buffer->sgetc();
                    } catch (...) {
                        if (stream.__setstate_from_callback(
                                ios_base::badbit | ios_base::failbit)) {
                            throw;
                        }
                        return;
                    }
#else
                    current = buffer->sgetc();
#endif
                    if (Traits::eq_int_type(current, Traits::eof())) {
                        stream.setstate(ios_base::eofbit | ios_base::failbit);
                        return;
                    }
                    bool space;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                    try {
                        space = stream.is_space(Traits::to_char_type(current));
                    } catch (...) {
                        if (stream.__setstate_from_callback(
                                ios_base::badbit | ios_base::failbit)) {
                            throw;
                        }
                        return;
                    }
#else
                    space = stream.is_space(Traits::to_char_type(current));
#endif
                    if (!space) break;
                    if (!stream.consume_observed(buffer, current)) {
                        stream.setstate(ios_base::failbit);
                        return;
                    }
                }
            }
            ok_ = true;
        }

        explicit operator bool() const noexcept { return ok_; }
    };

    explicit basic_istream(basic_streambuf<CharT, Traits>* sb)
        : gcount_(0), tie_flush_active_(false) {
        this->init(sb);
    }

    virtual ~basic_istream() = default;

    /* ═══════════════════════════════════════════════════════════
     * 書式なし入力
     * ═══════════════════════════════════════════════════════════*/

    int_type get() {
        tie_flush_guard tie_guard(*this);
        gcount_ = 0;
        sentry gate(*this, true);
        if (!gate) return Traits::eof();
        int_type c = Traits::eof();
        if (!consume_next(this->rdbuf(), c)) return c;
        if (Traits::eq_int_type(c, Traits::eof())) {
            this->setstate(ios_base::eofbit | ios_base::failbit);
        } else {
            gcount_ = 1;
        }
        return c;
    }

    basic_istream& get(char_type& c) {
        tie_flush_guard tie_guard(*this);
        int_type result = get();
        if (result != Traits::eof()) {
            c = Traits::to_char_type(result);
        }
        return *this;
    }

    basic_istream& get(char_type* s, streamsize n) {
        tie_flush_guard tie_guard(*this);
        return get(s, n, '\n');
    }

    basic_istream& get(char_type* s, streamsize n, char_type delim) {
        tie_flush_guard tie_guard(*this);
        gcount_ = 0;
        sentry gate(*this, true);
        if (!gate) return *this;
        if (!s || n <= 0 || !this->rdbuf()) {
            this->setstate(ios_base::failbit);
            return *this;
        }
        while (gcount_ < n - 1) {
            int_type c;
            if (!observe_current(this->rdbuf(), c)) break;
            if (Traits::eq_int_type(c, Traits::eof())) {
                this->setstate(ios_base::eofbit);
                break;
            }
            if (Traits::eq(Traits::to_char_type(c), delim)) break;
            if (!consume_observed(this->rdbuf(), c)) break;
            s[gcount_++] = Traits::to_char_type(c);
        }
        s[gcount_] = char_type();
        if (gcount_ == 0) this->setstate(ios_base::failbit);
        return *this;
    }

    basic_istream& getline(char_type* s, streamsize n) {
        tie_flush_guard tie_guard(*this);
        return getline(s, n, '\n');
    }

    basic_istream& getline(char_type* s, streamsize n, char_type delim) {
        tie_flush_guard tie_guard(*this);
        gcount_ = 0;
        sentry gate(*this, true);
        if (!gate) return *this;
        streamsize stored = 0;
        if (!s || n <= 0 || !this->rdbuf()) {
            this->setstate(ios_base::failbit);
            return *this;
        }
        for (;;) {
            int_type c;
            if (!observe_current(this->rdbuf(), c)) break;
            if (Traits::eq_int_type(c, Traits::eof())) {
                this->setstate(ios_base::eofbit);
                break;
            }
            if (Traits::eq(Traits::to_char_type(c), delim)) {
                if (!consume_observed(this->rdbuf(), c)) break;
                ++gcount_;
                break;
            }
            if (gcount_ >= n - 1) {
                this->setstate(ios_base::failbit);
                break;
            }
            if (!consume_observed(this->rdbuf(), c)) break;
            s[stored++] = Traits::to_char_type(c);
            ++gcount_;
        }
        s[stored] = char_type();
        if (gcount_ == 0) this->setstate(ios_base::failbit);
        return *this;
    }

    basic_istream& ignore(streamsize n = 1, int_type delim = Traits::eof()) {
        tie_flush_guard tie_guard(*this);
        gcount_ = 0;
        sentry gate(*this, true);
        if (!gate) return *this;
        if (!this->rdbuf()) {
            this->setstate(ios_base::badbit);
            return *this;
        }
        while (gcount_ < n) {
            int_type c;
            if (!consume_next(this->rdbuf(), c)) break;
            if (Traits::eq_int_type(c, Traits::eof())) {
                this->setstate(ios_base::eofbit);
                break;
            }
            ++gcount_;
            if (Traits::eq_int_type(c, delim)) break;
        }
        return *this;
    }

    int_type peek() {
        tie_flush_guard tie_guard(*this);
        gcount_ = 0;
        sentry gate(*this, true);
        if (!gate) return Traits::eof();
        int_type c = Traits::eof();
        if (!observe_current(this->rdbuf(), c)) return Traits::eof();
        if (Traits::eq_int_type(c, Traits::eof())) {
            this->setstate(ios_base::eofbit);
        }
        return c;
    }

    basic_istream& read(char_type* s, streamsize n) {
        tie_flush_guard tie_guard(*this);
        gcount_ = 0;
        sentry gate(*this, true);
        if (!gate) return *this;
        if (!this->rdbuf() || n < 0 || (!s && n != 0)) {
            this->setstate(ios_base::failbit);
            return *this;
        }
        if (!read_block(this->rdbuf(), s, n, gcount_)) return *this;
        if (gcount_ != n) {
            this->setstate(ios_base::eofbit | ios_base::failbit);
        }
        return *this;
    }

    streamsize readsome(char_type* s, streamsize n) {
        tie_flush_guard tie_guard(*this);
        gcount_ = 0;
        sentry gate(*this, true);
        if (!gate) return 0;
        if (!this->rdbuf() || n < 0 || (!s && n != 0)) {
            this->setstate(ios_base::failbit);
            return 0;
        }
        streamsize avail = 0;
        if (!available_count(this->rdbuf(), avail)) return 0;
        if (avail < 0) {
            this->setstate(ios_base::eofbit);
            return 0;
        }
        if (avail == 0 || n == 0) return 0;
        streamsize to_read = (avail < n) ? avail : n;
        if (!read_block(this->rdbuf(), s, to_read, gcount_)) return 0;
        return gcount_;
    }

    basic_istream& putback(char_type c) {
        tie_flush_guard tie_guard(*this);
        this->clear(this->rdstate() & ~ios_base::eofbit);
        gcount_ = 0;
        sentry gate(*this, true);
        if (!gate) return *this;
        int_type result = Traits::eof();
        if (!putback_observed(this->rdbuf(), c, result) ||
            Traits::eq_int_type(result, Traits::eof())) {
            this->setstate(ios_base::failbit);
        }
        return *this;
    }

    basic_istream& unget() {
        tie_flush_guard tie_guard(*this);
        this->clear(this->rdstate() & ~ios_base::eofbit);
        gcount_ = 0;
        sentry gate(*this, true);
        if (!gate) return *this;
        int_type result = Traits::eof();
        if (!unget_observed(this->rdbuf(), result) ||
            Traits::eq_int_type(result, Traits::eof())) {
            this->setstate(ios_base::failbit);
        }
        return *this;
    }

    streamsize gcount() const {
        return gcount_;
    }

    /* ═══════════════════════════════════════════════════════════
     * シーク
     * ═══════════════════════════════════════════════════════════*/

    pos_type tellg() {
        tie_flush_guard tie_guard(*this);
        if (this->fail() || !this->rdbuf()) return pos_type(-1);
        pos_type result = pos_type(off_type(-1));
        if (!seek_offset_observed(this->rdbuf(), 0, ios_base::cur, result))
            return pos_type(off_type(-1));
        return result;
    }

    basic_istream& seekg(pos_type pos) {
        tie_flush_guard tie_guard(*this);
        this->clear(this->rdstate() & ~ios_base::eofbit);
        if (!this->fail()) {
            pos_type result = pos_type(off_type(-1));
            if (!this->rdbuf() ||
                !seek_position_observed(this->rdbuf(), pos, result) ||
                result == pos_type(off_type(-1))) {
                if (!this->bad()) this->setstate(ios_base::failbit);
            }
        }
        return *this;
    }

    basic_istream& seekg(off_type off, ios_base::seekdir dir) {
        tie_flush_guard tie_guard(*this);
        this->clear(this->rdstate() & ~ios_base::eofbit);
        if (!this->fail()) {
            pos_type result = pos_type(off_type(-1));
            if (!this->rdbuf() ||
                !seek_offset_observed(this->rdbuf(), off, dir, result) ||
                result == pos_type(off_type(-1))) {
                if (!this->bad()) this->setstate(ios_base::failbit);
            }
        }
        return *this;
    }

    int sync() {
        tie_flush_guard tie_guard(*this);
        if (!this->rdbuf()) return -1;
        int result = -1;
        if (!sync_observed(this->rdbuf(), result)) return -1;
        if (result == -1) {
            /* [istream.unformatted] exposes a failed pubsync as badbit so
             * callers can observe the failed input sequence and an enabled
             * exception mask receives the same ios_base::failure path as
             * other unformatted operations. */
            this->setstate(ios_base::badbit);
            return -1;
        }
        return 0;
    }

    /* ═══════════════════════════════════════════════════════════
     * 書式付き入力 (operator>>)
     * ═══════════════════════════════════════════════════════════*/

    /* 空白をスキップ */
    bool skip_ws() {
        gcount_guard preserve(*this);
        tie_flush_guard tie_guard(*this);
        if (!this->good()) {
            this->setstate(ios_base::failbit);
            return false;
        }
        basic_streambuf<CharT, Traits>* buffer = this->rdbuf();
        if (!buffer) {
            this->setstate(ios_base::badbit | ios_base::failbit);
            return false;
        }
        if (this->flags() & ios_base::skipws) {
            int_type c;
            for (;;) {
                if (!observe_current(buffer, c)) return false;
                if (Traits::eq_int_type(c, Traits::eof())) {
                    this->setstate(ios_base::eofbit | ios_base::failbit);
                    return false;
                }
                char_type ch = Traits::to_char_type(c);
                if (!is_space(ch)) break;
                if (!consume_observed(buffer, c)) return false;
            }
        }
        return true;
    }

    basic_istream& __consume_ws() {
        gcount_guard preserve(*this);
        tie_flush_guard tie_guard(*this);
        if (!this->good()) {
            this->setstate(ios_base::failbit);
            return *this;
        }
        basic_streambuf<CharT, Traits>* buffer = this->rdbuf();
        if (!buffer) {
            this->setstate(ios_base::badbit | ios_base::failbit);
            return *this;
        }
        for (;;) {
            int_type current;
            if (!observe_current(buffer, current)) return *this;
            if (Traits::eq_int_type(current, Traits::eof())) {
                this->setstate(ios_base::eofbit);
                return *this;
            }
            if (!is_space(Traits::to_char_type(current))) return *this;
            if (!consume_observed(buffer, current)) return *this;
        }
    }

    /* bool */
    basic_istream& operator>>(bool& val) {
        gcount_guard preserve(*this);
        /* A boolalpha field may have names where one is a prefix of the
         * other.  The stream-owned matcher can look ahead and roll back on a
         * mismatch; the single-pass num_get iterator cannot, so preserve the
         * established longest-name behavior here. */
        if (this->flags() & ios_base::boolalpha)
            return input_boolalpha(val);
        return input_num_get(val);
    }

    /* 整数 */
    basic_istream& operator>>(short& val) {
        gcount_guard preserve(*this); return input_num_get_small(val);
    }
    basic_istream& operator>>(unsigned short& val) {
        gcount_guard preserve(*this); return input_num_get_small(val);
    }
    basic_istream& operator>>(int& val) {
        gcount_guard preserve(*this); return input_num_get_small(val);
    }
    basic_istream& operator>>(unsigned int& val) {
        gcount_guard preserve(*this); return input_num_get_small(val);
    }
    basic_istream& operator>>(long& val) {
        gcount_guard preserve(*this); return input_num_get(val);
    }
    basic_istream& operator>>(unsigned long& val) {
        gcount_guard preserve(*this); return input_num_get(val);
    }
    basic_istream& operator>>(long long& val) {
        gcount_guard preserve(*this); return input_num_get(val);
    }
    basic_istream& operator>>(unsigned long long& val) {
        gcount_guard preserve(*this); return input_num_get(val);
    }

    /* 浮動小数点 */
    basic_istream& operator>>(float& val) {
        gcount_guard preserve(*this);
        return input_num_get(val);
    }

    basic_istream& operator>>(double& val) {
        gcount_guard preserve(*this);
        return input_num_get(val);
    }

    basic_istream& operator>>(long double& val) {
        gcount_guard preserve(*this);
        return input_num_get(val);
    }

    /* Pointer extraction is owned by num_get just like the other numeric
     * fields.  Keep the destination unchanged when the facet rejects the
     * bounded hexadecimal address field. */
    basic_istream& operator>>(void*& val) {
        gcount_guard preserve(*this);
        return input_num_get(val);
    }

    /* 文字 */
    basic_istream& operator>>(char_type& c) {
        gcount_guard preserve(*this);
        tie_flush_guard tie_guard(*this);
        if (!skip_ws()) return *this;
        int_type result = get();
        if (result != Traits::eof()) {
            c = Traits::to_char_type(result);
        }
        return *this;
    }

    basic_istream& operator>>(signed char& c) {
        return *this >> reinterpret_cast<char&>(c);
    }

    basic_istream& operator>>(unsigned char& c) {
        return *this >> reinterpret_cast<char&>(c);
    }

    /* C文字列 (C++20: 配列長を型から取得して終端領域を保証する) */
    template<size_t N>
    basic_istream& operator>>(char_type (&s)[N]) {
        gcount_guard preserve(*this);
        tie_flush_guard tie_guard(*this);
        if (!skip_ws()) return *this;
        streamsize requested = this->width();
        this->width(0);
        s[0] = char_type();

        int_type c;
        if (!observe_current(this->rdbuf(), c)) return *this;
        if (Traits::eq_int_type(c, Traits::eof())) {
            this->setstate(ios_base::eofbit | ios_base::failbit);
            return *this;
        }

        size_t limit = N - 1u;
        if (requested > 0) {
            size_t requested_limit = static_cast<size_t>(requested - 1);
            if (requested_limit < limit) limit = requested_limit;
        }

        size_t stored = 0u;
        while (stored < limit) {
            if (!observe_current(this->rdbuf(), c)) break;
            if (Traits::eq_int_type(c, Traits::eof())) {
                this->setstate(ios_base::eofbit);
                break;
            }
            char_type ch = Traits::to_char_type(c);
            if (is_space(ch)) break;
            if (!consume_observed(this->rdbuf(), c)) break;
            s[stored++] = ch;
        }
        s[stored] = char_type();
        if (stored == 0u) this->setstate(ios_base::failbit);
        return *this;
    }

    /* std::string */
    basic_istream& operator>>(basic_string<CharT>& s) {
        gcount_guard preserve(*this);
        tie_flush_guard tie_guard(*this);
        if (!skip_ws()) return *this;
        streamsize requested = this->width();
        this->width(0);
        s.clear();
        size_t limit = s.max_size();
        if (requested > 0 && static_cast<size_t>(requested) < limit) {
            limit = static_cast<size_t>(requested);
        }
        while (s.size() < limit) {
            int_type c;
            if (!observe_current(this->rdbuf(), c)) break;
            if (Traits::eq_int_type(c, Traits::eof())) {
                this->setstate(ios_base::eofbit);
                break;
            }
            char_type ch = Traits::to_char_type(c);
            if (is_space(ch)) break;
            size_t previous = s.size();
            s.push_back(ch);
            if (s.size() == previous) {
                this->setstate(ios_base::badbit);
                break;
            }
            if (!consume_observed(this->rdbuf(), c)) {
                s.pop_back();
                break;
            }
        }
        if (s.empty()) this->setstate(ios_base::failbit);
        return *this;
    }

    /* Match the standard inserter/extractor family for strings with custom
     * traits or allocators.  Keep the implementation in one path so width,
     * skipws, stream state, and bounded growth have identical behavior for
     * the default alias and for user-provided basic_string specializations. */
    template<typename StringTraits, typename Allocator>
    basic_istream& operator>>(
        basic_string<CharT, StringTraits, Allocator>& s) {
        gcount_guard preserve(*this);
        tie_flush_guard tie_guard(*this);
        if (!skip_ws()) return *this;
        streamsize requested = this->width();
        this->width(0);
        s.clear();
        size_t limit = s.max_size();
        if (requested > 0 && static_cast<size_t>(requested) < limit) {
            limit = static_cast<size_t>(requested);
        }
        while (s.size() < limit) {
            int_type c;
            if (!observe_current(this->rdbuf(), c)) break;
            if (Traits::eq_int_type(c, Traits::eof())) {
                this->setstate(ios_base::eofbit);
                break;
            }
            char_type ch = Traits::to_char_type(c);
            if (is_space(ch)) break;
            size_t previous = s.size();
            s.push_back(ch);
            if (s.size() == previous) {
                this->setstate(ios_base::badbit);
                break;
            }
            if (!consume_observed(this->rdbuf(), c)) {
                s.pop_back();
                break;
            }
        }
        if (s.empty()) this->setstate(ios_base::failbit);
        return *this;
    }

    /* streambuf */
    basic_istream& operator>>(basic_streambuf<CharT, Traits>* sb) {
        gcount_guard preserve(*this);
        tie_flush_guard tie_guard(*this);
        if (!this->good()) {
            this->setstate(ios_base::failbit);
            return *this;
        }
        if (!sb) {
            this->setstate(ios_base::failbit);
            return *this;
        }
        basic_streambuf<CharT, Traits>* source = this->rdbuf();
        if (!source) {
            this->setstate(ios_base::badbit | ios_base::failbit);
            return *this;
        }

        streamsize transferred = 0;
        for (;;) {
            int_type current;
            if (!observe_current(source, current)) break;
            if (Traits::eq_int_type(current, Traits::eof())) {
                this->setstate(ios_base::eofbit);
                break;
            }
            if (!consume_observed(source, current)) break;
            const char_type character = Traits::to_char_type(current);
            int_type written;
            if (!write_observed(sb, character, written) ||
                Traits::eq_int_type(written, Traits::eof())) {
                int_type restored;
                const bool restore_completed =
                    restore_observed(source, character, restored);
                if (!restore_completed ||
                    Traits::eq_int_type(restored, Traits::eof()) ||
                    !Traits::eq_int_type(restored, current)) {
                    this->setstate(ios_base::badbit);
                }
                break;
            }
            ++transferred;
        }
        if (transferred == 0) this->setstate(ios_base::failbit);
        return *this;
    }

    /* マニピュレータ */
    basic_istream& operator>>(basic_istream& (*manip)(basic_istream&)) {
        return manip(*this);
    }

    basic_istream& operator>>(basic_ios<CharT, Traits>& (*manip)(basic_ios<CharT, Traits>&)) {
        manip(*this);
        return *this;
    }

    basic_istream& operator>>(ios_base& (*manip)(ios_base&)) {
        manip(*this);
        return *this;
    }

private:
    basic_istream& input_num_get_fallback(bool& value) {
        long numeric = 0;
        if (!input_integral_value(numeric)) return *this;
        value = numeric != 0;
        if (numeric != 0 && numeric != 1)
            this->setstate(ios_base::failbit);
        return *this;
    }
    basic_istream& input_num_get_fallback(long& value) {
        return input_int(value);
    }
    basic_istream& input_num_get_fallback(unsigned long& value) {
        return input_uint(value);
    }
    basic_istream& input_num_get_fallback(long long& value) {
        return input_int(value);
    }
    basic_istream& input_num_get_fallback(unsigned long long& value) {
        return input_uint(value);
    }
    basic_istream& input_num_get_fallback(float& value) {
        return input_float(value);
    }
    basic_istream& input_num_get_fallback(double& value) {
        return input_float(value);
    }
    basic_istream& input_num_get_fallback(long double& value) {
        return input_float(value);
    }
    basic_istream& input_num_get_fallback(void*& value) {
        (void)value;
        this->setstate(ios_base::failbit);
        return *this;
    }

    template<typename T>
    basic_istream& input_num_get(T& value) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
        tie_flush_guard tie_guard(*this);
        sentry input_sentry(*this);
        if (!input_sentry) return *this;
        using iterator = istreambuf_iterator<CharT, Traits>;
        iterator first(this->rdbuf());
        iterator last;
        unsigned int error = 0u;
        const locale current_locale = this->getloc();
        if (current_locale.name() != string("*") ||
            current_locale.template _find_facet<num_get<CharT, iterator>>() ==
            nullptr)
            return input_num_get_fallback(value);
        const auto& facet = use_facet<num_get<CharT, iterator>>(
            current_locale);
        (void)facet.get(first, last, *this, error, value);
        if ((error & __locale_eofbit()) != 0u)
            this->setstate(ios_base::eofbit);
        if ((error & __locale_failbit()) != 0u)
            this->setstate(ios_base::failbit);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            /* Numeric extraction, including the classic-locale fallback,
             * reads through the streambuf.  A callback exception is a
             * badbit event; preserve the original exception only when
             * badbit is masked. */
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
        }
#endif
        return *this;
    }

    template<typename T>
    basic_istream& input_num_get_small(T& value) {
        if (is_signed<T>::value) {
            long long candidate = 0;
            const ios_base::iostate before = this->rdstate();
            input_num_get(candidate);
            if ((before & ios_base::failbit) != 0u)
                return *this;
            if (this->bad()) return *this;
            if ((this->rdstate() & ios_base::failbit) != 0u) {
                value = static_cast<T>(candidate);
                return *this;
            }
            if (candidate < static_cast<long long>(numeric_limits<T>::min())) {
                value = numeric_limits<T>::min();
                this->setstate(ios_base::failbit);
            } else if (candidate > static_cast<long long>(numeric_limits<T>::max())) {
                value = numeric_limits<T>::max();
                this->setstate(ios_base::failbit);
            } else {
                value = static_cast<T>(candidate);
            }
        } else {
            long long candidate = 0;
            const ios_base::iostate before = this->rdstate();
            input_num_get(candidate);
            if ((before & ios_base::failbit) != 0u)
                return *this;
            if (this->bad()) return *this;
            if ((this->rdstate() & ios_base::failbit) != 0u) {
                value = static_cast<T>(candidate);
                return *this;
            }
            if (candidate < 0) {
                const unsigned long long magnitude =
                    0u - static_cast<unsigned long long>(candidate);
                if (magnitude <= static_cast<unsigned long long>(
                                     numeric_limits<T>::max())) {
                    value = static_cast<T>(candidate);
                } else {
                    value = numeric_limits<T>::max();
                    this->setstate(ios_base::failbit);
                }
            } else if (static_cast<unsigned long long>(candidate) >
                       static_cast<unsigned long long>(
                           numeric_limits<T>::max())) {
                value = numeric_limits<T>::max();
                this->setstate(ios_base::failbit);
            } else {
                value = static_cast<T>(candidate);
            }
        }
        return *this;
    }

    template<typename T>
    basic_istream& input_int(T& val) {
        (void)input_integral_value(val);
        return *this;
    }

    template<typename T>
    basic_istream& input_uint(T& val) {
        (void)input_integral_value(val);
        return *this;
    }

    int numeric_digit(char_type value) const {
        const locale current_locale = this->getloc();
        const ctype<char_type>& facet =
            use_facet<ctype<char_type>>(current_locale);
        for (int digit = 0; digit != 10; ++digit) {
            if (Traits::eq(value, facet.widen(static_cast<char>('0' + digit)))) {
                return digit;
            }
        }
        for (int digit = 0; digit != 6; ++digit) {
            if (Traits::eq(value, facet.widen(static_cast<char>('a' + digit))) ||
                Traits::eq(value, facet.widen(static_cast<char>('A' + digit)))) {
                return digit + 10;
            }
        }
        return -1;
    }

    bool numeric_is(char_type value, char expected) const {
        const locale current_locale = this->getloc();
        return Traits::eq(value,
            use_facet<ctype<char_type>>(current_locale).widen(expected));
    }

    template<typename T>
    bool input_integral_value(T& val) {
        tie_flush_guard tie_guard(*this);
        if (!skip_ws()) return false;

        basic_streambuf<CharT, Traits>* buffer = this->rdbuf();
        int_type current = buffer->sgetc();
        bool negative = false;
        if (!Traits::eq_int_type(current, Traits::eof())) {
            char_type character = Traits::to_char_type(current);
            if (numeric_is(character, '-') || numeric_is(character, '+')) {
                negative = numeric_is(character, '-');
                if (!consume_observed(buffer, current)) return false;
                current = buffer->sgetc();
            }
        }

        ios_base::fmtflags base_flags = this->flags() & ios_base::basefield;
        int base = base_flags == ios_base::oct ? 8 :
                   base_flags == ios_base::hex ? 16 :
                   base_flags == ios_base::dec ? 10 : 0;

        using unsigned_type = make_unsigned_t<T>;
        const unsigned_type maximum = numeric_limits<T>::is_signed
            ? static_cast<unsigned_type>(numeric_limits<T>::max())
            : numeric_limits<T>::max();
        const unsigned_type magnitude_limit =
            numeric_limits<T>::is_signed && negative
                ? static_cast<unsigned_type>(maximum + unsigned_type(1))
                : maximum;
        unsigned_type magnitude = 0;
        bool got_digit = false;
        bool overflow = false;
        const locale current_locale = this->getloc();
        const numpunct<char_type>& punctuation =
            use_facet<numpunct<char_type>>(current_locale);
        const basic_string<char> grouping = punctuation.grouping();
        const char_type thousands_separator = punctuation.thousands_sep();
        const bool grouping_enabled = !grouping.empty();
        /* Keep grouping validation bounded, but admit long valid numbers
         * within the surrounding token limits.  The former 128-entry cap
         * rejected a valid 130-group value before checking its grammar. */
        /* Keep grouping validation bounded while admitting fields well beyond
         * the historical 128-group limit.  One spare slot is reserved for
         * the final rightmost group. */
        static constexpr size_t maximum_group_count = 512u;
        static constexpr unsigned int oversized_group = 256u;
        unsigned int groups[maximum_group_count];
        size_t group_count = 0u;
        unsigned int current_group_digits = 0u;
        bool saw_separator = false;
        bool group_count_overflow = false;
        bool repeated_separator = false;

        const auto consume_digit = [&](unsigned int digit) {
            got_digit = true;
            if (current_group_digits < oversized_group) {
                ++current_group_digits;
            }
            if (!overflow) {
                const unsigned_type converted =
                    static_cast<unsigned_type>(digit);
                const unsigned_type converted_base =
                    static_cast<unsigned_type>(base);
                if (magnitude >
                    static_cast<unsigned_type>((magnitude_limit - converted) /
                                               converted_base)) {
                    overflow = true;
                } else {
                    magnitude = static_cast<unsigned_type>(
                        magnitude * converted_base + converted);
                }
            }
            if (!consume_observed(buffer, current)) return false;
            current = buffer->sgetc();
            return true;
        };

        int first_digit = Traits::eq_int_type(current, Traits::eof())
            ? -1 : numeric_digit(Traits::to_char_type(current));
        if (base == 0) {
            base = 10;
            if (first_digit == 0) {
                base = 8;
                if (!consume_digit(0)) return false;
                if (!Traits::eq_int_type(current, Traits::eof()) &&
                    (numeric_is(Traits::to_char_type(current), 'x') ||
                     numeric_is(Traits::to_char_type(current), 'X'))) {
                    base = 16;
                    got_digit = false;
                    current_group_digits = 0u;
                    if (!consume_observed(buffer, current)) return false;
                    current = buffer->sgetc();
                }
            }
        } else if (base == 16 && first_digit == 0) {
            if (!consume_digit(0)) return false;
            if (!Traits::eq_int_type(current, Traits::eof()) &&
                (numeric_is(Traits::to_char_type(current), 'x') ||
                 numeric_is(Traits::to_char_type(current), 'X'))) {
                got_digit = false;
                current_group_digits = 0u;
                if (!consume_observed(buffer, current)) return false;
                current = buffer->sgetc();
            }
        }

        for (;;) {
            if (Traits::eq_int_type(current, Traits::eof())) {
                this->setstate(ios_base::eofbit);
                break;
            }
            int digit = numeric_digit(Traits::to_char_type(current));
            if (digit >= 0 && digit < base) {
                if (!consume_digit(static_cast<unsigned int>(digit))) {
                    return false;
                }
                continue;
            }
            if (grouping_enabled && got_digit &&
                Traits::eq(Traits::to_char_type(current),
                           thousands_separator)) {
                if (current_group_digits == 0u) {
                    repeated_separator = true;
                    break;
                }
                saw_separator = true;
                if (group_count < maximum_group_count - 1u) {
                    groups[group_count++] = current_group_digits;
                } else {
                    group_count_overflow = true;
                }
                current_group_digits = 0u;
                if (!consume_observed(buffer, current)) return false;
                current = buffer->sgetc();
                continue;
            }
            break;
        }

        bool invalid_grouping = repeated_separator || group_count_overflow;
        if (saw_separator && !group_count_overflow) {
            groups[group_count++] = current_group_digits;
            const auto expected_group = [&](size_t position_from_right) {
                int previous = 0;
                int expected = 0;
                for (size_t index = 0u; index <= position_from_right; ++index) {
                    size_t pattern_index = index < grouping.size()
                        ? index : grouping.size() - 1u;
                    unsigned int encoded = static_cast<unsigned char>(
                        grouping[pattern_index]);
                    if (encoded == 0u) {
                        expected = previous;
                    } else if (encoded ==
                               static_cast<unsigned int>(CHAR_MAX)) {
                        expected = -1;
                    } else {
                        expected = static_cast<int>(encoded);
                        previous = expected;
                    }
                }
                return expected;
            };
            for (size_t index = group_count; index != 0u; --index) {
                const size_t group_index = index - 1u;
                const size_t position_from_right =
                    group_count - 1u - group_index;
                const int expected = expected_group(position_from_right);
                const unsigned int actual = groups[group_index];
                if (group_index == 0u) {
                    if (actual == 0u ||
                        (expected >= 0 &&
                         actual > static_cast<unsigned int>(expected))) {
                        invalid_grouping = true;
                    }
                } else if (expected <= 0 ||
                           actual != static_cast<unsigned int>(expected)) {
                    invalid_grouping = true;
                }
            }
        }

        if (!got_digit || repeated_separator) {
            val = T(0);
            this->setstate(ios_base::failbit);
        } else if (overflow) {
            val = numeric_limits<T>::is_signed && negative
                ? numeric_limits<T>::min() : numeric_limits<T>::max();
            this->setstate(ios_base::failbit);
        } else if (is_signed<T>::value) {
            if (negative) {
                if (magnitude == magnitude_limit) {
                    val = numeric_limits<T>::min();
                } else {
                    val = static_cast<T>(-static_cast<T>(magnitude));
                }
            } else {
                val = static_cast<T>(magnitude);
            }
        } else {
            val = negative
                ? static_cast<T>(unsigned_type(0) - magnitude)
                : static_cast<T>(magnitude);
        }
        if (invalid_grouping) this->setstate(ios_base::failbit);
        return true;
    }

    basic_istream& input_boolalpha(bool& val) {
        tie_flush_guard tie_guard(*this);
        if (!skip_ws()) return *this;

        const locale current_locale = this->getloc();
        const numpunct<char_type>& punctuation =
            use_facet<numpunct<char_type>>(current_locale);
        basic_string<char_type> true_name = punctuation.truename();
        basic_string<char_type> false_name = punctuation.falsename();
        size_t true_index = 0;
        size_t false_index = 0;
        bool true_possible = !true_name.empty();
        bool false_possible = !false_name.empty();
        basic_streambuf<CharT, Traits>* buffer = this->rdbuf();

        val = false;
        while (true_possible || false_possible) {
            const bool true_complete = true_possible &&
                true_index == true_name.size();
            const bool false_complete = false_possible &&
                false_index == false_name.size();
            if (true_complete || false_complete) {
                if (true_complete && false_complete) break;
                bool& longer_possible = true_complete
                    ? false_possible : true_possible;
                size_t& longer_index = true_complete
                    ? false_index : true_index;
                const basic_string<char_type>& longer_name = true_complete
                    ? false_name : true_name;
                if (!longer_possible || longer_index == longer_name.size()) {
                    val = true_complete;
                    return *this;
                }

                int_type next = buffer->sgetc();
                if (Traits::eq_int_type(next, Traits::eof())) {
                    this->setstate(ios_base::eofbit);
                    val = true_complete;
                    return *this;
                }
                if (!Traits::eq(Traits::to_char_type(next),
                                longer_name[longer_index])) {
                    val = true_complete;
                    return *this;
                }
                if (!consume_observed(buffer, next)) return *this;
                ++longer_index;
                if (true_complete) true_possible = false;
                else false_possible = false;
                continue;
            }

            int_type current = buffer->sgetc();
            if (Traits::eq_int_type(current, Traits::eof())) {
                this->setstate(ios_base::eofbit);
                break;
            }
            char_type character = Traits::to_char_type(current);
            bool true_matches = true_possible &&
                true_index < true_name.size() &&
                Traits::eq(character, true_name[true_index]);
            bool false_matches = false_possible &&
                false_index < false_name.size() &&
                Traits::eq(character, false_name[false_index]);
            if (!true_matches && !false_matches) break;

            if (!consume_observed(buffer, current)) return *this;
            if (true_matches) ++true_index;
            else true_possible = false;
            if (false_matches) ++false_index;
            else false_possible = false;
        }

        this->setstate(ios_base::failbit);
        return *this;
    }

#if __cplusplus >= 201703L
    template<typename FloatType>
    basic_istream& input_float(FloatType& val) {
        tie_flush_guard tie_guard(*this);
        if (!skip_ws()) return *this;

        /* Keep numeric fields bounded, but admit fields larger than the old
         * 4 KiB and 16 KiB stack ceilings.  The conversion owner still
         * validates the complete token before publishing a value. */
        constexpr size_t maximum_token_size =
            __num_get_detail::numeric_field_capacity;
        /* Match integral extraction's bounded grouping workspace. */
        /* Match integral extraction: 512 bounded groups, including the final
         * rightmost group, so valid fields no longer stop at 128 groups. */
        constexpr size_t maximum_group_count = 512u;
        constexpr unsigned int oversized_group = 4097u;
        __num_get_detail::numeric_token_buffer token_storage(
            maximum_token_size + 1u);
        if (!token_storage.valid()) {
            this->setstate(ios_base::failbit);
            return *this;
        }
        char* token = token_storage.data;
        unsigned int groups[maximum_group_count];
        size_t token_size = 0u;
        size_t group_count = 0u;
        unsigned int current_group_digits = 0u;
        bool token_overflow = false;
        bool significand_digit = false;
        bool integer_digit = false;
        bool saw_separator = false;
        bool group_count_overflow = false;
        bool repeated_separator = false;

        basic_streambuf<CharT, Traits>* buffer = this->rdbuf();
        int_type current = buffer->sgetc();
        const locale current_locale = this->getloc();
        const numpunct<char_type>& punctuation =
            use_facet<numpunct<char_type>>(current_locale);
        const basic_string<char> grouping = punctuation.grouping();
        const char_type decimal_point = punctuation.decimal_point();
        const char_type thousands_separator = punctuation.thousands_sep();
        const bool grouping_enabled = !grouping.empty();

        const auto append = [&](char character) {
            if (token_size < maximum_token_size) {
                token[token_size++] = character;
            } else {
                token_overflow = true;
            }
        };
        const auto advance = [&]() {
            if (!consume_observed(buffer, current)) return false;
            current = buffer->sgetc();
            return true;
        };
        const auto current_is = [&](char expected) {
            return !Traits::eq_int_type(current, Traits::eof()) &&
                   numeric_is(Traits::to_char_type(current), expected);
        };
        const auto current_is_ci = [&](char lower) {
            return current_is(lower) ||
                   current_is(static_cast<char>(lower - ('a' - 'A')));
        };
        const auto payload_ascii = [&]() {
            if (Traits::eq_int_type(current, Traits::eof())) return -1;
            const char_type character = Traits::to_char_type(current);
            if (numeric_is(character, '_')) return static_cast<int>('_');
            for (int digit = 0; digit != 10; ++digit) {
                const char expected = static_cast<char>('0' + digit);
                if (numeric_is(character, expected)) {
                    return static_cast<int>(expected);
                }
            }
            for (int letter = 0; letter != 26; ++letter) {
                const char lower = static_cast<char>('a' + letter);
                const char upper = static_cast<char>('A' + letter);
                if (numeric_is(character, lower)) {
                    return static_cast<int>(lower);
                }
                if (numeric_is(character, upper)) {
                    return static_cast<int>(upper);
                }
            }
            return -1;
        };
        const auto current_digit = [&]() {
            if (Traits::eq_int_type(current, Traits::eof())) return -1;
            return numeric_digit(Traits::to_char_type(current));
        };
        const auto consume_digit = [&](int digit, bool integer_part) {
            append(digit < 10
                ? static_cast<char>('0' + digit)
                : static_cast<char>('a' + (digit - 10)));
            significand_digit = true;
            if (integer_part) {
                integer_digit = true;
                if (current_group_digits < oversized_group) {
                    ++current_group_digits;
                }
            }
            return advance();
        };

        if (current_is('-') || current_is('+')) {
            append(current_is('-') ? '-' : '+');
            if (!advance()) return *this;
        }

        const size_t sign_size = token_size;
        bool consume_failed = false;
        const auto consume_word = [&](const char* word) {
            while (*word != '\0') {
                if (!current_is_ci(*word)) return false;
                append(*word++);
                if (!advance()) {
                    consume_failed = true;
                    return false;
                }
            }
            return true;
        };
        bool special = false;
        bool special_valid = true;
        if (current_is_ci('i')) {
            special = true;
            special_valid = consume_word("inf");
            if (special_valid && current_is_ci('i')) {
                special_valid = consume_word("inity");
            }
        } else if (current_is_ci('n')) {
            special = true;
            special_valid = consume_word("nan");
            if (special_valid && current_is('(')) {
                append('(');
                if (!advance()) return *this;
                for (;;) {
                    const int character = payload_ascii();
                    if (character < 0) break;
                    append(static_cast<char>(character));
                    if (!advance()) return *this;
                }
                if (current_is(')')) {
                    append(')');
                    if (!advance()) return *this;
                } else {
                    special_valid = false;
                }
            }
        }
        if (consume_failed) return *this;
        if (special) {
            if (Traits::eq_int_type(current, Traits::eof())) {
                this->setstate(ios_base::eofbit);
            }
            if (!special_valid || token_overflow) {
                this->setstate(ios_base::failbit);
                return *this;
            }
            const char* first = token;
            const char* last = token + token_size;
            if (first != last && *first == '+') ++first;
            FloatType candidate = FloatType(0);
            const from_chars_result result = from_chars(
                first, last, candidate, chars_format::general);
            if (result.ec != errc{} || result.ptr != last) {
                this->setstate(ios_base::failbit);
                return *this;
            }
            val = candidate;
            return *this;
        }

        bool hexadecimal = false;
        if (current_digit() == 0) {
            if (!consume_digit(0, true)) return *this;
            if (current_is_ci('x')) {
                hexadecimal = true;
                token_size = sign_size;
                significand_digit = false;
                integer_digit = false;
                current_group_digits = 0u;
                if (!advance()) return *this;
            }
        }

        for (;;) {
            const int digit = current_digit();
            if (digit >= 0 && digit < (hexadecimal ? 16 : 10)) {
                if (!consume_digit(digit, true)) return *this;
                continue;
            }
            if (grouping_enabled && integer_digit &&
                !Traits::eq_int_type(current, Traits::eof()) &&
                Traits::eq(Traits::to_char_type(current),
                           thousands_separator)) {
                if (current_group_digits == 0u) {
                    repeated_separator = true;
                    break;
                }
                saw_separator = true;
                if (group_count < maximum_group_count - 1u) {
                    groups[group_count++] = current_group_digits;
                } else {
                    group_count_overflow = true;
                }
                current_group_digits = 0u;
                if (!advance()) return *this;
                continue;
            }
            break;
        }

        bool invalid_grouping = repeated_separator || group_count_overflow;
        if (saw_separator && !group_count_overflow) {
            groups[group_count++] = current_group_digits;
            const auto expected_group = [&](size_t position_from_right) {
                int previous = 0;
                int expected = 0;
                for (size_t index = 0u; index <= position_from_right; ++index) {
                    const size_t pattern_index = index < grouping.size()
                        ? index : grouping.size() - 1u;
                    const unsigned int encoded = static_cast<unsigned char>(
                        grouping[pattern_index]);
                    if (encoded == 0u) {
                        expected = previous;
                    } else if (encoded ==
                               static_cast<unsigned int>(CHAR_MAX)) {
                        expected = -1;
                    } else {
                        expected = static_cast<int>(encoded);
                        previous = expected;
                    }
                }
                return expected;
            };
            for (size_t index = group_count; index != 0u; --index) {
                const size_t group_index = index - 1u;
                const size_t position_from_right =
                    group_count - 1u - group_index;
                const int expected = expected_group(position_from_right);
                const unsigned int actual = groups[group_index];
                if (group_index == 0u) {
                    if (actual == 0u ||
                        (expected >= 0 &&
                         actual > static_cast<unsigned int>(expected))) {
                        invalid_grouping = true;
                    }
                } else if (expected <= 0 ||
                           actual != static_cast<unsigned int>(expected)) {
                    invalid_grouping = true;
                }
            }
        }

        if (!Traits::eq_int_type(current, Traits::eof()) &&
            Traits::eq(Traits::to_char_type(current), decimal_point)) {
            append('.');
            if (!advance()) return *this;
            for (;;) {
                const int digit = current_digit();
                if (digit < 0 || digit >= (hexadecimal ? 16 : 10)) break;
                if (!consume_digit(digit, false)) return *this;
            }
        }

        bool exponent_digit = !hexadecimal;
        if ((!hexadecimal && current_is_ci('e')) ||
            (hexadecimal && current_is_ci('p'))) {
            exponent_digit = false;
            append(hexadecimal ? 'p' : 'e');
            if (!advance()) return *this;
            if (current_is('-') || current_is('+')) {
                append(current_is('-') ? '-' : '+');
                if (!advance()) return *this;
            }
            for (;;) {
                const int digit = current_digit();
                if (digit < 0 || digit >= 10) break;
                append(static_cast<char>('0' + digit));
                exponent_digit = true;
                if (!advance()) return *this;
            }
        }

        if (Traits::eq_int_type(current, Traits::eof())) {
            this->setstate(ios_base::eofbit);
        }
        if (!significand_digit || !exponent_digit || token_overflow) {
            this->setstate(ios_base::failbit);
            return *this;
        }

        const char* first = token;
        const char* last = token + token_size;
        if (first != last && *first == '+') ++first;
        FloatType candidate = FloatType(0);
        const from_chars_result result = from_chars(
            first, last, candidate,
            hexadecimal ? chars_format::hex : chars_format::general);
        if (result.ec != errc{} || result.ptr != last) {
            this->setstate(ios_base::failbit);
            return *this;
        }
        val = candidate;
        if (invalid_grouping) this->setstate(ios_base::failbit);

        return *this;
    }
#else
    /* C++11/14 float extraction uses the allocation-free libc parser.  Read a
     * bounded token first, normalize the active numpunct separators, then
     * publish only a complete in-range conversion.  Any suffix which is not
     * part of the conversion is put back in reverse order so `1.2tail` leaves
     * `tail` for the next extraction. */
    template<typename FloatType>
    basic_istream& input_float(FloatType& value) {
        tie_flush_guard tie_guard(*this);
        if (!skip_ws()) return *this;

        /* Match the C++17 path so C++11/14 extraction has the same field
         * contract instead of rejecting a valid long token early. */
        constexpr size_t maximum_token_size =
            __num_get_detail::numeric_field_capacity;
        __num_get_detail::numeric_token_buffer token_storage(
            maximum_token_size + 1u);
        if (!token_storage.valid()) {
            this->setstate(ios_base::failbit);
            return *this;
        }
        char* token = token_storage.data;
        __num_get_detail::numeric_token_buffer normalized_storage(
            maximum_token_size + 1u);
        __istream_detail::numeric_source_index_buffer source_storage(
            maximum_token_size);
        if (!normalized_storage.valid() || !source_storage.valid()) {
            this->setstate(ios_base::failbit);
            return *this;
        }
        char* normalized = normalized_storage.data;
        size_t* source_for_normalized = source_storage.data;
        size_t token_size = 0u;
        size_t normalized_size = 0u;
        bool token_overflow = false;
        basic_streambuf<CharT, Traits>* buffer = this->rdbuf();
        int_type current = buffer->sgetc();
        const locale current_locale = this->getloc();
        const numpunct<char_type>& punctuation =
            use_facet<numpunct<char_type>>(current_locale);
        const char_type decimal_point = punctuation.decimal_point();
        const char_type thousands_separator = punctuation.thousands_sep();
        const basic_string<char> grouping = punctuation.grouping();
        const bool grouping_enabled = !grouping.empty();
        static constexpr size_t maximum_group_count = 512u;
        unsigned int groups[maximum_group_count];
        size_t group_count = 0u;
        unsigned int current_group_digits = 0u;
        bool saw_separator = false;
        bool group_count_overflow = false;
        bool repeated_separator = false;

        while (!Traits::eq_int_type(current, Traits::eof())) {
            const char_type character = Traits::to_char_type(current);
            if (is_space(character)) break;
            if (token_size >= maximum_token_size) {
                token_overflow = true;
            } else {
                if (static_cast<unsigned long long>(character) > 0x7fu) {
                    token_overflow = true;
                } else {
                    token[token_size++] = static_cast<char>(character);
                }
            }
            if (!consume_observed(buffer, current)) return *this;
            current = buffer->sgetc();
        }
        token[token_size] = '\0';
        if (Traits::eq_int_type(current, Traits::eof())) {
            this->setstate(ios_base::eofbit);
        }
        if (token_overflow || token_size == 0u) {
            this->setstate(ios_base::failbit);
            return *this;
        }

        const bool hexadecimal_prefix = token_size >= 2u &&
            token[0] == '0' && (token[1] == 'x' || token[1] == 'X');
        bool decimal_seen = false;
        bool exponent_seen = false;
        for (size_t index = 0u; index < token_size; ++index) {
            const char_type character = static_cast<char_type>(token[index]);
            const bool in_integer = !decimal_seen && !exponent_seen;
            if (grouping_enabled && in_integer &&
                Traits::eq(character, thousands_separator)) {
                if (current_group_digits == 0u) {
                    repeated_separator = true;
                } else {
                    saw_separator = true;
                    if (group_count < maximum_group_count - 1u) {
                        groups[group_count++] = current_group_digits;
                    } else {
                        group_count_overflow = true;
                    }
                    current_group_digits = 0u;
                }
                continue;
            }
            if (in_integer && character >= static_cast<char_type>('0') &&
                character <= static_cast<char_type>('9')) {
                ++current_group_digits;
            }
            if (in_integer && Traits::eq(character, decimal_point)) {
                decimal_seen = true;
                normalized[normalized_size] = '.';
            } else if (!exponent_seen &&
                       (character == static_cast<char_type>('e') ||
                        character == static_cast<char_type>('E') ||
                        (hexadecimal_prefix &&
                         (character == static_cast<char_type>('p') ||
                          character == static_cast<char_type>('P'))))) {
                exponent_seen = true;
                normalized[normalized_size] = token[index];
            } else if (static_cast<unsigned int>(character) <= 0x7fu) {
                normalized[normalized_size] = token[index];
            } else {
                this->setstate(ios_base::failbit);
                return *this;
            }
            source_for_normalized[normalized_size++] = index;
        }
        if (saw_separator && !group_count_overflow) {
            groups[group_count++] = current_group_digits;
        }
        bool invalid_grouping = repeated_separator || group_count_overflow;
        if (saw_separator && !group_count_overflow) {
            const auto expected_group = [&](size_t position_from_right) {
                int previous = 0;
                int expected = 0;
                for (size_t pattern = 0u;
                     pattern <= position_from_right; ++pattern) {
                    const size_t pattern_index = pattern < grouping.size()
                        ? pattern : grouping.size() - 1u;
                    const unsigned int encoded = static_cast<unsigned char>(
                        grouping[pattern_index]);
                    if (encoded == 0u) {
                        expected = previous;
                    } else if (encoded == static_cast<unsigned int>(CHAR_MAX)) {
                        expected = -1;
                    } else {
                        expected = static_cast<int>(encoded);
                        previous = expected;
                    }
                }
                return expected;
            };
            for (size_t index = group_count; index != 0u; --index) {
                const size_t group_index = index - 1u;
                const size_t position_from_right =
                    group_count - 1u - group_index;
                const int expected = expected_group(position_from_right);
                const unsigned int actual = groups[group_index];
                if (group_index == 0u) {
                    if (actual == 0u ||
                        (expected >= 0 &&
                         actual > static_cast<unsigned int>(expected))) {
                        invalid_grouping = true;
                    }
                } else if (expected <= 0 ||
                           actual != static_cast<unsigned int>(expected)) {
                    invalid_grouping = true;
                }
            }
        }
        normalized[normalized_size] = '\0';

        char* parsed_end = nullptr;
        errno = 0;
        const long double parsed = strtold(normalized, &parsed_end);
        const size_t parsed_size = parsed_end == nullptr
            ? 0u : static_cast<size_t>(parsed_end - normalized);
        const bool hexadecimal = normalized_size >= 2u &&
            normalized[0] == '0' &&
            (normalized[1] == 'x' || normalized[1] == 'X');
        bool incomplete_exponent = false;
        if (parsed_size < normalized_size && parsed_size != 0u) {
            const char next = normalized[parsed_size];
            incomplete_exponent = next == 'e' || next == 'E' ||
                                  (hexadecimal && (next == 'p' || next == 'P'));
        }
        bool missing_hex_exponent = false;
        if (hexadecimal && parsed_size != 0u) {
            bool exponent_seen = false;
            for (size_t index = 2u; index < parsed_size; ++index) {
                if (normalized[index] == 'p' || normalized[index] == 'P') {
                    exponent_seen = true;
                    break;
                }
            }
            missing_hex_exponent = !exponent_seen;
        }
        if (parsed_size == 0u || parsed_size > normalized_size ||
            incomplete_exponent || missing_hex_exponent || errno == ERANGE) {
            this->setstate(ios_base::failbit);
            return *this;
        }

        size_t source_size = parsed_size == normalized_size
            ? token_size : source_for_normalized[parsed_size];
        while (source_size < token_size) {
            --token_size;
            const int_type restored = buffer->sputbackc(
                static_cast<char_type>(token[token_size]));
            if (Traits::eq_int_type(restored, Traits::eof())) {
                this->setstate(ios_base::badbit);
                return *this;
            }
        }
        value = static_cast<FloatType>(parsed);
        if (invalid_grouping) this->setstate(ios_base::failbit);
        return *this;
    }
#endif

protected:
    basic_istream(basic_istream&& rhs) noexcept
        : gcount_(rhs.gcount_), tie_flush_active_(false) {
        this->copyfmt(rhs);
        this->set_rdbuf(rhs.rdbuf());
        /* Move construction is noexcept.  Copy the already-recorded state
         * without re-running the destination exception mask: a source that
         * is failed with that bit masked must still be movable. */
        this->state_ = rhs.rdstate();
        rhs.gcount_ = 0;
    }

    basic_istream& operator=(basic_istream&& rhs) noexcept {
        if (this != &rhs) {
            this->copyfmt(rhs);
            this->set_rdbuf(rhs.rdbuf());
            this->state_ = rhs.rdstate();
            gcount_ = rhs.gcount_;
            tie_flush_active_ = false;
            rhs.gcount_ = 0;
        }
        return *this;
    }

    void swap(basic_istream& rhs) noexcept {
        basic_ios<CharT, Traits>::swap(rhs);
        using std::swap;
        swap(gcount_, rhs.gcount_);
        tie_flush_active_ = false;
        rhs.tie_flush_active_ = false;
    }

    basic_istream() : gcount_(0), tie_flush_active_(false) {
        this->init(nullptr);
    }
};

/* 型エイリアス */
using istream = basic_istream<char>;

template<typename CharT, typename Traits>
basic_istream<CharT, Traits>& ws(basic_istream<CharT, Traits>& input) {
    return input.__consume_ws();
}

/* ═══════════════════════════════════════════════════════════════
 * getline (string用)
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename Traits>
basic_istream<CharT, Traits>&
getline(basic_istream<CharT, Traits>& is, basic_string<CharT>& str, CharT delim) {
    streamsize saved_gcount = is.gcount_;
    typename basic_istream<CharT, Traits>::tie_flush_guard tie_guard(is);
    basic_string<CharT> candidate;
    bool allocation_failed = false;
    bool callback_failed = false;
    typename Traits::int_type c = Traits::eof();
    basic_streambuf<CharT, Traits>* buffer = is.rdbuf();
    if (!buffer) {
        is.setstate(ios_base::badbit | ios_base::failbit);
        is.gcount_ = saved_gcount;
        return is;
    }
    for (;;) {
        if (!is.observe_current(buffer, c)) {
            callback_failed = true;
            break;
        }
        if (Traits::eq_int_type(c, Traits::eof())) {
            is.setstate(ios_base::eofbit);
            break;
        }
        if (!is.consume_observed(buffer, c)) break;
        if (Traits::eq(Traits::to_char_type(c), delim)) break;
        const size_t before = candidate.size();
        candidate += Traits::to_char_type(c);
        if (candidate.size() != before + 1u) {
            /* In no-exception builds basic_string reports a failed growth by
             * retaining its old value.  Stop before publishing a partial
             * destination and leave the stream at badbit. */
            allocation_failed = true;
            is.setstate(ios_base::badbit);
            break;
        }
    }
    if (!callback_failed && candidate.empty() &&
        Traits::eq_int_type(c, Traits::eof())) {
        is.setstate(ios_base::failbit);
    }
    if (!allocation_failed && !callback_failed) {
        using std::swap;
        swap(str, candidate);
    }
    is.gcount_ = saved_gcount;
    return is;
}

template<typename CharT, typename Traits>
basic_istream<CharT, Traits>&
getline(basic_istream<CharT, Traits>& is, basic_string<CharT>& str) {
    return getline<CharT, Traits>(is, str, CharT('\n'));
}

} /* namespace std */

#endif /* RINCXX_ISTREAM_H */
