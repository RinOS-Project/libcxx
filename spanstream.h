/*
 * RinOS C++ <spanstream>
 * Fixed-storage stream buffers and streams (C++23).
 */

#ifndef RINCXX_SPANSTREAM_H
#define RINCXX_SPANSTREAM_H

#include "version.h"

#if defined(RIN_LIBCXX_CXX23_DIALECT)

#include "iostream.h"
#include "limits.h"
#include "span.h"
#include "utility.h"

namespace std {

template<class CharT, class Traits = char_traits<CharT>>
class basic_spanbuf : public basic_streambuf<CharT, Traits> {
public:
    using char_type = CharT;
    using traits_type = Traits;
    using int_type = typename Traits::int_type;
    using pos_type = typename Traits::pos_type;
    using off_type = typename Traits::off_type;

private:
    ios_base::openmode mode_;
    std::span<CharT> buffer_;

    static pos_type failure() noexcept { return pos_type(off_type(-1)); }

    void reset_sequences() noexcept {
        CharT* first = buffer_.data();
        CharT* last = first ? first + buffer_.size() : nullptr;
        if (mode_ & ios_base::in) this->setg(first, first, last);
        else this->setg(nullptr, nullptr, nullptr);

        if (mode_ & ios_base::out) {
            this->setp(first, last);
            if ((mode_ & ios_base::ate) && first)
                this->pptr_ = last;
        } else {
            this->setp(nullptr, nullptr);
        }
    }

protected:
    basic_streambuf<CharT, Traits>* setbuf(CharT* data,
                                            streamsize count) override {
        if (count < 0 || (!data && count != 0)) return nullptr;
        std::span<CharT> replacement(data, static_cast<size_t>(count));
        span(replacement);
        return this;
    }

    pos_type seekoff(off_type offset, ios_base::seekdir direction,
                     ios_base::openmode which = ios_base::in |
                                               ios_base::out) override {
        const bool in = (which & ios_base::in) && (mode_ & ios_base::in);
        const bool out = (which & ios_base::out) && (mode_ & ios_base::out);
        if ((!in && !out) || (in && out && direction == ios_base::cur))
            return failure();

        const CharT* next = out ? this->pptr_ : this->gptr_;
        const CharT* begin = out ? this->pbase_ : this->eback_;
        const bool has_current = next && begin;
        if (!has_current && offset != 0) return failure();

        off_type base = 0;
        if (direction == ios_base::cur) {
            if (has_current) base = static_cast<off_type>(next - begin);
        } else if (direction == ios_base::end) {
            if ((mode_ & ios_base::out) && !(mode_ & ios_base::in)) {
                base = this->pbase_ && this->pptr_
                    ? static_cast<off_type>(this->pptr_ - this->pbase_)
                    : 0;
            } else {
                if (buffer_.size() >
                    static_cast<size_t>(numeric_limits<off_type>::max()))
                    return failure();
                base = static_cast<off_type>(buffer_.size());
            }
        } else if (direction != ios_base::beg) {
            return failure();
        }

        const off_type maximum = numeric_limits<off_type>::max();
        const off_type minimum = numeric_limits<off_type>::min();
        if ((offset > 0 && base > maximum - offset) ||
            (offset < 0 && base < minimum - offset))
            return failure();
        const off_type position = base + offset;
        if (position < 0 || static_cast<size_t>(position) > buffer_.size())
            return failure();
        if ((!this->gptr_ && in && position != 0) ||
            (!this->pptr_ && out && position != 0))
            return failure();

        CharT* first = buffer_.data();
        if (in && first) this->gptr_ = first + position;
        if (out && first) this->pptr_ = first + position;
        return pos_type(position);
    }

    pos_type seekpos(pos_type position,
                     ios_base::openmode which = ios_base::in |
                                               ios_base::out) override {
        return seekoff(static_cast<off_type>(position), ios_base::beg, which);
    }

public:
    basic_spanbuf() : basic_spanbuf(ios_base::in | ios_base::out) {}

    explicit basic_spanbuf(ios_base::openmode which)
        : basic_spanbuf(std::span<CharT>(), which) {}

    explicit basic_spanbuf(std::span<CharT> storage,
                           ios_base::openmode which = ios_base::in |
                                                     ios_base::out)
        : basic_streambuf<CharT, Traits>(), mode_(which), buffer_(storage) {
        reset_sequences();
    }

    basic_spanbuf(const basic_spanbuf&) = delete;
    basic_spanbuf& operator=(const basic_spanbuf&) = delete;

    basic_spanbuf(basic_spanbuf&& other)
        : basic_streambuf<CharT, Traits>(std::move(other)),
          mode_(std::move(other.mode_)), buffer_(std::move(other.buffer_)) {}

    basic_spanbuf& operator=(basic_spanbuf&& other) {
        if (this != &other) {
            basic_spanbuf temporary(std::move(other));
            swap(temporary);
        }
        return *this;
    }

    void swap(basic_spanbuf& other) noexcept {
        basic_streambuf<CharT, Traits>::swap(other);
        using std::swap;
        swap(mode_, other.mode_);
        swap(buffer_, other.buffer_);
    }

    std::span<CharT> span() const noexcept {
        if (mode_ & ios_base::out) {
            CharT* first = this->pbase_;
            if (!first) return std::span<CharT>();
            return std::span<CharT>(first,
                static_cast<size_t>(this->pptr_ - this->pbase_));
        }
        return buffer_;
    }

    void span(std::span<CharT> storage) noexcept {
        buffer_ = storage;
        reset_sequences();
    }
};

template<class CharT, class Traits>
void swap(basic_spanbuf<CharT, Traits>& left,
          basic_spanbuf<CharT, Traits>& right) noexcept {
    left.swap(right);
}

template<class CharT, class Traits = char_traits<CharT>>
class basic_ispanstream : public basic_istream<CharT, Traits> {
public:
    using char_type = CharT;
    using traits_type = Traits;
    using int_type = typename Traits::int_type;
    using pos_type = typename Traits::pos_type;
    using off_type = typename Traits::off_type;

private:
    basic_spanbuf<CharT, Traits> buffer_;

public:
    explicit basic_ispanstream(std::span<CharT> storage,
                               ios_base::openmode which = ios_base::in)
        : basic_istream<CharT, Traits>(),
          buffer_(storage, which | ios_base::in) {
        this->init(&buffer_);
    }

    basic_ispanstream(const basic_ispanstream&) = delete;
    basic_ispanstream& operator=(const basic_ispanstream&) = delete;

    basic_ispanstream(basic_ispanstream&& other) noexcept
        : basic_istream<CharT, Traits>(std::move(other)),
          buffer_(std::move(other.buffer_)) {
        this->set_rdbuf(&buffer_);
    }

    basic_ispanstream& operator=(basic_ispanstream&& other) noexcept {
        if (this != &other) {
            basic_istream<CharT, Traits>::operator=(std::move(other));
            buffer_ = std::move(other.buffer_);
            this->set_rdbuf(&buffer_);
        }
        return *this;
    }

    void swap(basic_ispanstream& other) noexcept {
        basic_istream<CharT, Traits>::swap(other);
        buffer_.swap(other.buffer_);
        this->set_rdbuf(&buffer_);
        other.set_rdbuf(&other.buffer_);
    }

    basic_spanbuf<CharT, Traits>* rdbuf() const noexcept {
        return const_cast<basic_spanbuf<CharT, Traits>*>(&buffer_);
    }

    std::span<CharT> span() const noexcept { return buffer_.span(); }
    void span(std::span<CharT> storage) noexcept { buffer_.span(storage); }
};

template<class CharT, class Traits>
void swap(basic_ispanstream<CharT, Traits>& left,
          basic_ispanstream<CharT, Traits>& right) noexcept {
    left.swap(right);
}

template<class CharT, class Traits = char_traits<CharT>>
class basic_ospanstream : public basic_ostream<CharT, Traits> {
public:
    using char_type = CharT;
    using traits_type = Traits;
    using int_type = typename Traits::int_type;
    using pos_type = typename Traits::pos_type;
    using off_type = typename Traits::off_type;

private:
    basic_spanbuf<CharT, Traits> buffer_;

public:
    explicit basic_ospanstream(std::span<CharT> storage,
                               ios_base::openmode which = ios_base::out)
        : basic_ostream<CharT, Traits>(),
          buffer_(storage, which | ios_base::out) {
        this->init(&buffer_);
    }

    basic_ospanstream(const basic_ospanstream&) = delete;
    basic_ospanstream& operator=(const basic_ospanstream&) = delete;

    basic_ospanstream(basic_ospanstream&& other) noexcept
        : basic_ostream<CharT, Traits>(std::move(other)),
          buffer_(std::move(other.buffer_)) {
        this->set_rdbuf(&buffer_);
    }

    basic_ospanstream& operator=(basic_ospanstream&& other) noexcept {
        if (this != &other) {
            basic_ostream<CharT, Traits>::operator=(std::move(other));
            buffer_ = std::move(other.buffer_);
            this->set_rdbuf(&buffer_);
        }
        return *this;
    }

    void swap(basic_ospanstream& other) noexcept {
        basic_ostream<CharT, Traits>::swap(other);
        buffer_.swap(other.buffer_);
        this->set_rdbuf(&buffer_);
        other.set_rdbuf(&other.buffer_);
    }

    basic_spanbuf<CharT, Traits>* rdbuf() const noexcept {
        return const_cast<basic_spanbuf<CharT, Traits>*>(&buffer_);
    }

    std::span<CharT> span() const noexcept { return buffer_.span(); }
    void span(std::span<CharT> storage) noexcept { buffer_.span(storage); }
};

template<class CharT, class Traits>
void swap(basic_ospanstream<CharT, Traits>& left,
          basic_ospanstream<CharT, Traits>& right) noexcept {
    left.swap(right);
}

template<class CharT, class Traits = char_traits<CharT>>
class basic_spanstream : public basic_iostream<CharT, Traits> {
public:
    using char_type = CharT;
    using traits_type = Traits;
    using int_type = typename Traits::int_type;
    using pos_type = typename Traits::pos_type;
    using off_type = typename Traits::off_type;

private:
    basic_spanbuf<CharT, Traits> buffer_;

public:
    explicit basic_spanstream(std::span<CharT> storage,
                              ios_base::openmode which = ios_base::in |
                                                        ios_base::out)
        : basic_iostream<CharT, Traits>(), buffer_(storage, which) {
        this->init(&buffer_);
    }

    basic_spanstream(const basic_spanstream&) = delete;
    basic_spanstream& operator=(const basic_spanstream&) = delete;

    basic_spanstream(basic_spanstream&& other)
        : basic_iostream<CharT, Traits>(std::move(other)),
          buffer_(std::move(other.buffer_)) {
        this->set_rdbuf(&buffer_);
    }

    basic_spanstream& operator=(basic_spanstream&& other) {
        if (this != &other) {
            basic_iostream<CharT, Traits>::operator=(std::move(other));
            buffer_ = std::move(other.buffer_);
            this->set_rdbuf(&buffer_);
        }
        return *this;
    }

    void swap(basic_spanstream& other) noexcept {
        basic_iostream<CharT, Traits>::swap(other);
        buffer_.swap(other.buffer_);
        this->set_rdbuf(&buffer_);
        other.set_rdbuf(&other.buffer_);
    }

    basic_spanbuf<CharT, Traits>* rdbuf() const noexcept {
        return const_cast<basic_spanbuf<CharT, Traits>*>(&buffer_);
    }

    std::span<CharT> span() const noexcept { return buffer_.span(); }
    void span(std::span<CharT> storage) noexcept { buffer_.span(storage); }
};

template<class CharT, class Traits>
void swap(basic_spanstream<CharT, Traits>& left,
          basic_spanstream<CharT, Traits>& right) noexcept {
    left.swap(right);
}

using spanbuf = basic_spanbuf<char>;
using wspanbuf = basic_spanbuf<wchar_t>;
using ispanstream = basic_ispanstream<char>;
using wispanstream = basic_ispanstream<wchar_t>;
using ospanstream = basic_ospanstream<char>;
using wospanstream = basic_ospanstream<wchar_t>;
using spanstream = basic_spanstream<char>;
using wspanstream = basic_spanstream<wchar_t>;

} // namespace std

#endif // RIN_LIBCXX_CXX23_DIALECT

#endif // RINCXX_SPANSTREAM_H
