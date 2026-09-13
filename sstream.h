/*
 * RinOS C++ String Stream ✿
 * std::stringstream - proper inheritance from basic_ostream/basic_istream
 */

#ifndef RINCXX_SSTREAM_H
#define RINCXX_SSTREAM_H

#include "string.h"
#include "string_view.h"
#include "cctype.h"
#include "ios.h"
#include "streambuf.h"
#include "iostream.h"

#ifdef __cplusplus

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * basic_stringbuf - inherits from basic_streambuf
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename Traits = char_traits<CharT>, typename Allocator = void>
class basic_stringbuf : public basic_streambuf<CharT, Traits> {
public:
    using char_type = CharT;
    using traits_type = Traits;
    using int_type = typename Traits::int_type;
    using pos_type = typename Traits::pos_type;
    using off_type = typename Traits::off_type;
    using allocator_type = Allocator;
    using string_type = basic_string<CharT>;

private:
    string_type str_;
    ios_base::openmode mode_;

    struct pointer_state {
        bool has_get;
        size_t get_pos;
        size_t get_limit;
        bool has_put;
        size_t put_pos;
    };

    static pointer_state capture_pointers(const basic_stringbuf& source) noexcept {
        pointer_state state = { false, 0, 0, false, 0 };
        if (source.eback() && source.gptr() && source.egptr()) {
            state.has_get = true;
            state.get_pos = static_cast<size_t>(source.gptr() - source.eback());
            state.get_limit = static_cast<size_t>(source.egptr() - source.eback());
        }
        if (source.pbase() && source.pptr() && source.epptr()) {
            state.has_put = true;
            state.put_pos = static_cast<size_t>(source.pptr() - source.pbase());
        }
        return state;
    }

    void restore_pointers(const pointer_state& state) noexcept {
        update_pointers();
        CharT* begin = str_.empty()
            ? nullptr : const_cast<CharT*>(str_.data());
        if (begin && (mode_ & ios_base::in) && state.has_get) {
            size_t limit = state.get_limit;
            if (limit > str_.size()) limit = str_.size();
            size_t position = state.get_pos;
            if (position > limit) position = limit;
            this->setg(begin, begin + position, begin + limit);
        }
        if (begin && (mode_ & ios_base::out) && state.has_put) {
            size_t position = state.put_pos;
            if (position > str_.size()) position = str_.size();
            this->setp(begin, begin + str_.size());
            this->pbump(static_cast<int>(position));
        }
    }

    void reset_moved_from() noexcept {
        mode_ = static_cast<ios_base::openmode>(0);
        str_.clear();
        update_pointers();
    }

    void update_pointers() {
        if (str_.empty()) {
            this->setg(nullptr, nullptr, nullptr);
            this->setp(nullptr, nullptr);
        } else {
            CharT* begin = const_cast<CharT*>(str_.data());
            CharT* end = begin + str_.size();

            if (mode_ & ios_base::in) {
                this->setg(begin, begin, end);
            }
            if (mode_ & ios_base::out) {
                this->setp(begin, end);
                if (mode_ & ios_base::ate) {
                    this->pbump(static_cast<int>(str_.size()));
                }
            }
        }
    }

public:
    explicit basic_stringbuf(ios_base::openmode mode = ios_base::in | ios_base::out)
        : str_(), mode_(mode) {
        update_pointers();
    }

    explicit basic_stringbuf(const string_type& s,
                             ios_base::openmode mode = ios_base::in | ios_base::out)
        : str_(s), mode_(mode) {
        update_pointers();
    }

    basic_stringbuf(basic_stringbuf&& other) noexcept
        : str_(std::move(other.str_)), mode_(other.mode_) {
        /* basic_string transfers its allocation but the base streambuf's
         * raw pointers are not part of that move.  Capture their offsets
         * after the transfer (the source pointers still address the moved
         * allocation), then rebase them onto this object's string. */
        pointer_state state = capture_pointers(other);
        restore_pointers(state);
        other.reset_moved_from();
    }

    basic_stringbuf& operator=(basic_stringbuf&& other) noexcept {
        if (this != &other) {
            pointer_state state = capture_pointers(other);
            str_ = std::move(other.str_);
            mode_ = other.mode_;
            restore_pointers(state);
            other.reset_moved_from();
        }
        return *this;
    }

    /* str() accessors */
    string_type str() const {
        if (mode_ & ios_base::out) {
            /* Return up to pptr position */
            if (this->pptr()) {
                return string_type(this->pbase(),
                    static_cast<typename string_type::size_type>(this->pptr() - this->pbase()));
            }
        }
        return str_;
    }

    void str(const string_type& s) {
        str_ = s;
        update_pointers();
    }

    /* view() for C++20 */
#if __cplusplus >= 202002L
    basic_string_view<CharT, Traits> view() const noexcept {
        if (mode_ & ios_base::out) {
            if (this->pptr()) {
                return basic_string_view<CharT, Traits>(this->pbase(),
                    static_cast<size_t>(this->pptr() - this->pbase()));
            }
        }
        return basic_string_view<CharT, Traits>(str_);
    }
#endif

protected:
    /* Override underflow for input */
    int_type underflow() override {
        if (!(mode_ & ios_base::in)) {
            return Traits::eof();
        }
        if (this->gptr() && this->gptr() < this->egptr()) {
            return Traits::to_int_type(*this->gptr());
        }
        /* Sync with output if writing to same buffer */
        if ((mode_ & ios_base::out) && this->pptr() > this->egptr()) {
            this->setg(this->eback(), this->gptr(), this->pptr());
            if (this->gptr() < this->egptr()) {
                return Traits::to_int_type(*this->gptr());
            }
        }
        return Traits::eof();
    }

    /* Override overflow for output */
    int_type overflow(int_type c = Traits::eof()) override {
        if (!(mode_ & ios_base::out)) {
            return Traits::eof();
        }
        if (Traits::eq_int_type(c, Traits::eof())) {
            return Traits::not_eof(c);
        }

        /* Need to grow the string */
        size_t cur_pos = this->pptr() ? (this->pptr() - this->pbase()) : 0;

        str_ += Traits::to_char_type(c);

        CharT* begin = const_cast<CharT*>(str_.data());
        CharT* end = begin + str_.size();

        if (mode_ & ios_base::in) {
            size_t gpos = this->gptr() ? (this->gptr() - this->eback()) : 0;
            this->setg(begin, begin + gpos, end);
        }
        this->setp(begin, end);
        this->pbump(static_cast<int>(cur_pos + 1));

        return c;
    }

    /* Override seekoff for seeking */
    pos_type seekoff(off_type off, ios_base::seekdir way,
                     ios_base::openmode which = ios_base::in | ios_base::out) override {
        pos_type ret = pos_type(off_type(-1));

        bool do_in = (which & ios_base::in) && (mode_ & ios_base::in);
        bool do_out = (which & ios_base::out) && (mode_ & ios_base::out);

        if (!do_in && !do_out) return ret;
        if (do_in && do_out && way == ios_base::cur) return ret;

        off_type newoff;
        if (way == ios_base::beg) {
            newoff = 0;
        } else if (way == ios_base::cur) {
            if (do_in) newoff = this->gptr() - this->eback();
            else newoff = this->pptr() - this->pbase();
        } else {
            newoff = str_.size();
        }
        newoff += off;

        if (newoff < 0 || static_cast<size_t>(newoff) > str_.size()) {
            return ret;
        }

        if (do_in) {
            this->setg(this->eback(), this->eback() + newoff, this->egptr());
        }
        if (do_out) {
            this->setp(this->pbase(), this->epptr());
            this->pbump(static_cast<int>(newoff));
        }

        return pos_type(newoff);
    }

    pos_type seekpos(pos_type pos,
                     ios_base::openmode which = ios_base::in | ios_base::out) override {
        return seekoff(off_type(pos), ios_base::beg, which);
    }
};

using stringbuf = basic_stringbuf<char>;
using wstringbuf = basic_stringbuf<wchar_t>;

/* ═══════════════════════════════════════════════════════════════
 * basic_istringstream - inherits from basic_istream
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename Traits = char_traits<CharT>, typename Allocator = void>
class basic_istringstream : public basic_istream<CharT, Traits> {
public:
    using char_type = CharT;
    using traits_type = Traits;
    using int_type = typename Traits::int_type;
    using pos_type = typename Traits::pos_type;
    using off_type = typename Traits::off_type;
    using allocator_type = Allocator;
    using string_type = basic_string<CharT>;

private:
    basic_stringbuf<CharT, Traits, Allocator> buf_;

public:
    explicit basic_istringstream(ios_base::openmode mode = ios_base::in)
        : basic_istream<CharT, Traits>(), buf_(mode | ios_base::in) {
        this->init(&buf_);
    }

    explicit basic_istringstream(const string_type& s,
                                  ios_base::openmode mode = ios_base::in)
        : basic_istream<CharT, Traits>(), buf_(s, mode | ios_base::in) {
        this->init(&buf_);
    }

    basic_istringstream(basic_istringstream&& other) noexcept
        : basic_istream<CharT, Traits>(std::move(other)),
          buf_(std::move(other.buf_)) {
        this->set_rdbuf(&buf_);
    }

    basic_istringstream& operator=(basic_istringstream&& other) noexcept {
        if (this != &other) {
            basic_istream<CharT, Traits>::operator=(std::move(other));
            buf_ = std::move(other.buf_);
            this->set_rdbuf(&buf_);
        }
        return *this;
    }

    /* Buffer access */
    basic_stringbuf<CharT, Traits, Allocator>* rdbuf() const {
        return const_cast<basic_stringbuf<CharT, Traits, Allocator>*>(&buf_);
    }

    /* String access */
    string_type str() const { return buf_.str(); }
    void str(const string_type& s) {
        buf_.str(s);
        this->clear();
    }

#if __cplusplus >= 202002L
    basic_string_view<CharT, Traits> view() const noexcept {
        return buf_.view();
    }
#endif
};

using istringstream = basic_istringstream<char>;
using wistringstream = basic_istringstream<wchar_t>;

/* ═══════════════════════════════════════════════════════════════
 * basic_ostringstream - inherits from basic_ostream
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename Traits = char_traits<CharT>, typename Allocator = void>
class basic_ostringstream : public basic_ostream<CharT, Traits> {
public:
    using char_type = CharT;
    using traits_type = Traits;
    using int_type = typename Traits::int_type;
    using pos_type = typename Traits::pos_type;
    using off_type = typename Traits::off_type;
    using allocator_type = Allocator;
    using string_type = basic_string<CharT>;

private:
    basic_stringbuf<CharT, Traits, Allocator> buf_;

public:
    explicit basic_ostringstream(ios_base::openmode mode = ios_base::out)
        : basic_ostream<CharT, Traits>(), buf_(mode | ios_base::out) {
        this->init(&buf_);
    }

    explicit basic_ostringstream(const string_type& s,
                                  ios_base::openmode mode = ios_base::out)
        : basic_ostream<CharT, Traits>(), buf_(s, mode | ios_base::out) {
        this->init(&buf_);
    }

    basic_ostringstream(basic_ostringstream&& other) noexcept
        : basic_ostream<CharT, Traits>(std::move(other)),
          buf_(std::move(other.buf_)) {
        this->set_rdbuf(&buf_);
    }

    basic_ostringstream& operator=(basic_ostringstream&& other) noexcept {
        if (this != &other) {
            basic_ostream<CharT, Traits>::operator=(std::move(other));
            buf_ = std::move(other.buf_);
            this->set_rdbuf(&buf_);
        }
        return *this;
    }

    /* Buffer access */
    basic_stringbuf<CharT, Traits, Allocator>* rdbuf() const {
        return const_cast<basic_stringbuf<CharT, Traits, Allocator>*>(&buf_);
    }

    /* String access */
    string_type str() const { return buf_.str(); }
    void str(const string_type& s) {
        buf_.str(s);
        this->clear();
    }

#if __cplusplus >= 202002L
    basic_string_view<CharT, Traits> view() const noexcept {
        return buf_.view();
    }
#endif
};

using ostringstream = basic_ostringstream<char>;
using wostringstream = basic_ostringstream<wchar_t>;

/* ═══════════════════════════════════════════════════════════════
 * basic_stringstream - inherits from basic_iostream
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename Traits = char_traits<CharT>, typename Allocator = void>
class basic_stringstream : public basic_iostream<CharT, Traits> {
public:
    using char_type = CharT;
    using traits_type = Traits;
    using int_type = typename Traits::int_type;
    using pos_type = typename Traits::pos_type;
    using off_type = typename Traits::off_type;
    using allocator_type = Allocator;
    using string_type = basic_string<CharT>;

private:
    basic_stringbuf<CharT, Traits, Allocator> buf_;

public:
    explicit basic_stringstream(ios_base::openmode mode = ios_base::in | ios_base::out)
        : basic_iostream<CharT, Traits>(), buf_(mode) {
        this->init(&buf_);
    }

    explicit basic_stringstream(const string_type& s,
                                 ios_base::openmode mode = ios_base::in | ios_base::out)
        : basic_iostream<CharT, Traits>(), buf_(s, mode) {
        this->init(&buf_);
    }

    basic_stringstream(basic_stringstream&& other) noexcept
        : basic_iostream<CharT, Traits>(std::move(other)),
          buf_(std::move(other.buf_)) {
        this->set_rdbuf(&buf_);
    }

    basic_stringstream& operator=(basic_stringstream&& other) noexcept {
        if (this != &other) {
            basic_iostream<CharT, Traits>::operator=(std::move(other));
            buf_ = std::move(other.buf_);
            this->set_rdbuf(&buf_);
        }
        return *this;
    }

    /* Buffer access */
    basic_stringbuf<CharT, Traits, Allocator>* rdbuf() const {
        return const_cast<basic_stringbuf<CharT, Traits, Allocator>*>(&buf_);
    }

    /* String access */
    string_type str() const { return buf_.str(); }
    void str(const string_type& s) {
        buf_.str(s);
        this->clear();
    }

#if __cplusplus >= 202002L
    basic_string_view<CharT, Traits> view() const noexcept {
        return buf_.view();
    }
#endif
};

using stringstream = basic_stringstream<char>;
using wstringstream = basic_stringstream<wchar_t>;

/* ═══════════════════════════════════════════════════════════════
 * getline functions
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename Traits, typename Allocator>
basic_istream<CharT, Traits>&
getline(basic_istringstream<CharT, Traits, Allocator>& is,
        basic_string<CharT>& str, CharT delim) {
    return getline(static_cast<basic_istream<CharT, Traits>&>(is), str, delim);
}

template<typename CharT, typename Traits, typename Allocator>
basic_istream<CharT, Traits>&
getline(basic_istringstream<CharT, Traits, Allocator>& is,
        basic_string<CharT>& str) {
    return getline(static_cast<basic_istream<CharT, Traits>&>(is), str, CharT('\n'));
}

/* getline for basic_stringstream */
template<typename CharT, typename Traits, typename Allocator>
basic_istream<CharT, Traits>&
getline(basic_stringstream<CharT, Traits, Allocator>& ss,
        basic_string<CharT>& str, CharT delim) {
    return getline(static_cast<basic_istream<CharT, Traits>&>(ss), str, delim);
}

template<typename CharT, typename Traits, typename Allocator>
basic_istream<CharT, Traits>&
getline(basic_stringstream<CharT, Traits, Allocator>& ss,
        basic_string<CharT>& str) {
    return getline(static_cast<basic_istream<CharT, Traits>&>(ss), str, CharT('\n'));
}

} /* namespace std */

/* ═══════════════════════════════════════════════════════════════
 * RinOS 独自ヘルパー関数
 * 標準を壊さずに (ostringstream{} << x).str() 相当の機能を提供
 * ═══════════════════════════════════════════════════════════════*/

namespace rincxx {

/**
 * stream_str - ostringstream から文字列を取得
 * @param oss ostringstream への参照
 * @return 蓄積された文字列
 *
 * Usage:
 *   std::ostringstream oss;
 *   oss << "Hello " << 42;
 *   std::string s = rincxx::stream_str(oss);
 */
template<typename CharT, typename Traits, typename Allocator>
inline std::basic_string<CharT>
stream_str(std::basic_ostringstream<CharT, Traits, Allocator>& oss) {
    return oss.str();
}

template<typename CharT, typename Traits, typename Allocator>
inline std::basic_string<CharT>
stream_str(const std::basic_ostringstream<CharT, Traits, Allocator>& oss) {
    return oss.str();
}

template<typename CharT, typename Traits, typename Allocator>
inline std::basic_string<CharT>
stream_str(std::basic_stringstream<CharT, Traits, Allocator>& ss) {
    return ss.str();
}

template<typename CharT, typename Traits, typename Allocator>
inline std::basic_string<CharT>
stream_str(const std::basic_stringstream<CharT, Traits, Allocator>& ss) {
    return ss.str();
}

/**
 * to_string_via_stream - 任意の型を ostringstream 経由で文字列に変換
 * @param value ストリーム出力可能な値
 * @return 文字列化された値
 *
 * Usage:
 *   std::string s = rincxx::to_string_via_stream(3.14159);
 */
template<typename T>
inline std::string to_string_via_stream(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

} /* namespace rincxx */

#endif /* __cplusplus */
#endif /* RINCXX_SSTREAM_H */
