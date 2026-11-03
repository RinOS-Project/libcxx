/*
 * RinOS C++20 synchronized output streams.
 */

#ifndef RINCXX_SYNCSTREAM_H
#define RINCXX_SYNCSTREAM_H

#include "rincxx.h"

#if defined(__cplusplus) && __cplusplus >= 202002L

#include "ostream.h"
#include "streambuf.h"
#include "string.h"
#include "utility.h"
#include "mutex.h"

namespace std {

namespace __syncstream_detail {

/* A single emission gate makes writes from syncbuf objects that wrap the
 * same streambuf indivisible. Serializing unrelated streambufs as well is a
 * conservative fallback that avoids a global registry with its own lifetime
 * and allocation failure rules. */
inline recursive_mutex& emission_mutex() {
    static recursive_mutex value;
    return value;
}

template<typename CharT, typename Traits>
class syncbuf_control {
public:
    virtual ~syncbuf_control() = default;
    virtual bool emit_controlled() = 0;
    virtual void set_emit_on_sync_controlled(bool value) = 0;
};

template<typename CharT, typename Traits>
syncbuf_control<CharT, Traits>* get_syncbuf_control(
    basic_ostream<CharT, Traits>& stream) {
    return dynamic_cast<syncbuf_control<CharT, Traits>*>(stream.rdbuf());
}

} // namespace __syncstream_detail

template<typename CharT, typename Traits = char_traits<CharT>,
         typename Allocator = allocator<CharT>>
class basic_syncbuf : public basic_streambuf<CharT, Traits>,
                      public __syncstream_detail::syncbuf_control<CharT, Traits> {
public:
    using char_type = CharT;
    using traits_type = Traits;
    using allocator_type = Allocator;
    using streambuf_type = basic_streambuf<CharT, Traits>;
    using ostream_type = basic_ostream<CharT, Traits>;
    using string_type = basic_string<CharT, Traits, Allocator>;
    using int_type = typename traits_type::int_type;

private:
    streambuf_type* wrapped_;
    string_type pending_;
    bool emit_on_sync_;

protected:
    int_type overflow(int_type value = traits_type::eof()) override {
        if (traits_type::eq_int_type(value, traits_type::eof()))
            return traits_type::not_eof(value);
        pending_.push_back(traits_type::to_char_type(value));
        return value;
    }

    streamsize xsputn(const char_type* source, streamsize count) override {
        if (source == nullptr || count <= 0) return 0;
        pending_.append(source, static_cast<size_t>(count));
        return count;
    }

    int sync() override {
        if (!emit_on_sync_) return 0;
        return emit() ? 0 : -1;
    }

public:
    explicit basic_syncbuf(streambuf_type* wrapped,
                           const allocator_type& allocator = allocator_type())
        : wrapped_(wrapped), pending_(allocator), emit_on_sync_(false) {}

    explicit basic_syncbuf(ostream_type& wrapped,
                           const allocator_type& allocator = allocator_type())
        : basic_syncbuf(wrapped.rdbuf(), allocator) {}

    basic_syncbuf(const basic_syncbuf&) = delete;
    basic_syncbuf& operator=(const basic_syncbuf&) = delete;

    basic_syncbuf(basic_syncbuf&& other)
        : streambuf_type(), wrapped_(other.wrapped_),
          pending_(std::move(other.pending_)),
          emit_on_sync_(other.emit_on_sync_) {
        other.wrapped_ = nullptr;
        other.emit_on_sync_ = false;
    }

    basic_syncbuf(basic_syncbuf&& other, const allocator_type& allocator)
        : streambuf_type(), wrapped_(other.wrapped_),
          pending_(std::move(other.pending_), allocator),
          emit_on_sync_(other.emit_on_sync_) {
        other.wrapped_ = nullptr;
        other.emit_on_sync_ = false;
    }

    basic_syncbuf& operator=(basic_syncbuf&& other) {
        if (this == &other) return *this;
        (void)emit();
        wrapped_ = other.wrapped_;
        pending_ = std::move(other.pending_);
        emit_on_sync_ = other.emit_on_sync_;
        other.wrapped_ = nullptr;
        other.emit_on_sync_ = false;
        return *this;
    }

    ~basic_syncbuf() override { (void)emit(); }

    streambuf_type* get_wrapped() const noexcept { return wrapped_; }

    bool get_emit_on_sync() const noexcept { return emit_on_sync_; }

    void set_emit_on_sync(bool value) noexcept { emit_on_sync_ = value; }

    bool emit() { return emit_controlled(); }

    bool emit_controlled() override {
        if (wrapped_ == nullptr) return pending_.empty();
        lock_guard<recursive_mutex> lock(
            __syncstream_detail::emission_mutex());
        if (!pending_.empty()) {
            const streamsize requested =
                static_cast<streamsize>(pending_.size());
            streamsize written = wrapped_->sputn(pending_.data(), requested);
            if (written < 0) written = 0;
            if (written > requested) written = requested;
            if (written != 0)
                pending_.erase(0u, static_cast<size_t>(written));
            if (written != requested) return false;
        }
        if (emit_on_sync_ && wrapped_->pubsync() == -1) return false;
        return true;
    }

    void set_emit_on_sync_controlled(bool value) override {
        set_emit_on_sync(value);
    }
};

template<typename CharT, typename Traits = char_traits<CharT>,
         typename Allocator = allocator<CharT>>
class basic_osyncstream : public basic_ostream<CharT, Traits> {
public:
    using char_type = CharT;
    using traits_type = Traits;
    using allocator_type = Allocator;
    using ostream_type = basic_ostream<CharT, Traits>;
    using streambuf_type = basic_streambuf<CharT, Traits>;
    using syncbuf_type = basic_syncbuf<CharT, Traits, Allocator>;

private:
    syncbuf_type buffer_;

public:
    explicit basic_osyncstream(streambuf_type* wrapped,
                               const allocator_type& allocator = allocator_type())
        : ostream_type(&buffer_), buffer_(wrapped, allocator) {}

    explicit basic_osyncstream(ostream_type& wrapped,
                               const allocator_type& allocator = allocator_type())
        : basic_osyncstream(wrapped.rdbuf(), allocator) {}

    basic_osyncstream(const basic_osyncstream&) = delete;
    basic_osyncstream& operator=(const basic_osyncstream&) = delete;

    basic_osyncstream(basic_osyncstream&& other)
        : ostream_type(std::move(other)), buffer_(std::move(other.buffer_)) {
        this->set_rdbuf(&buffer_);
    }

    basic_osyncstream& operator=(basic_osyncstream&& other) {
        if (this == &other) return *this;
        ostream_type::operator=(std::move(other));
        buffer_ = std::move(other.buffer_);
        this->set_rdbuf(&buffer_);
        return *this;
    }

    syncbuf_type* rdbuf() const noexcept {
        return const_cast<syncbuf_type*>(&buffer_);
    }

    streambuf_type* get_wrapped() const noexcept {
        return buffer_.get_wrapped();
    }

    bool emit() {
        const bool result = buffer_.emit();
        if (!result) this->setstate(ios_base::badbit);
        return result;
    }

    bool get_emit_on_sync() const noexcept {
        return buffer_.get_emit_on_sync();
    }

    void set_emit_on_sync(bool value) noexcept {
        buffer_.set_emit_on_sync(value);
    }
};

using syncbuf = basic_syncbuf<char>;
using wsyncbuf = basic_syncbuf<wchar_t>;
using osyncstream = basic_osyncstream<char>;
using wosyncstream = basic_osyncstream<wchar_t>;

struct emit_on_flush_t { explicit emit_on_flush_t() = default; };
struct noemit_on_flush_t { explicit noemit_on_flush_t() = default; };
struct flush_emit_t { explicit flush_emit_t() = default; };

inline constexpr emit_on_flush_t emit_on_flush{};
inline constexpr noemit_on_flush_t noemit_on_flush{};
inline constexpr flush_emit_t flush_emit{};

template<typename CharT, typename Traits>
basic_ostream<CharT, Traits>& operator<<(basic_ostream<CharT, Traits>& stream,
                                          emit_on_flush_t) {
    if (auto* control =
            __syncstream_detail::get_syncbuf_control(stream))
        control->set_emit_on_sync_controlled(true);
    return stream;
}

template<typename CharT, typename Traits>
basic_ostream<CharT, Traits>& operator<<(basic_ostream<CharT, Traits>& stream,
                                          noemit_on_flush_t) {
    if (auto* control =
            __syncstream_detail::get_syncbuf_control(stream))
        control->set_emit_on_sync_controlled(false);
    return stream;
}

template<typename CharT, typename Traits>
basic_ostream<CharT, Traits>& operator<<(basic_ostream<CharT, Traits>& stream,
                                          flush_emit_t) {
    stream.flush();
    if (auto* control =
            __syncstream_detail::get_syncbuf_control(stream)) {
        if (!control->emit_controlled()) stream.setstate(ios_base::badbit);
    }
    return stream;
}

} // namespace std

#endif // C++20
#endif // RINCXX_SYNCSTREAM_H
