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

/* Syncbufs for one wrapped buffer share a gate. The registry lock is held
 * only while looking up or releasing that gate; emitting to unrelated
 * buffers never serializes their writes. */
struct emission_gate {
    const void* wrapped;
    size_t references;
    recursive_mutex mutex;
    emission_gate* next;

    emission_gate(const void* buffer, emission_gate* next_gate)
        : wrapped(buffer), references(1u), next(next_gate) {}
};

inline emission_gate*& emission_gate_list() {
    static emission_gate* value = nullptr;
    return value;
}

inline mutex& emission_gate_registry_mutex() {
    static mutex value;
    return value;
}

inline emission_gate* acquire_emission_gate(const void* wrapped) {
    if (wrapped == nullptr) return nullptr;
    lock_guard<mutex> lock(emission_gate_registry_mutex());
    emission_gate*& gates = emission_gate_list();
    for (emission_gate* gate = gates; gate != nullptr; gate = gate->next) {
        if (gate->wrapped == wrapped) {
            ++gate->references;
            return gate;
        }
    }
    emission_gate* gate = new emission_gate(wrapped, gates);
    gates = gate;
    return gate;
}

inline void release_emission_gate(emission_gate* gate) {
    if (gate == nullptr) return;
    bool destroy = false;
    {
        lock_guard<mutex> lock(emission_gate_registry_mutex());
        if (gate->references > 1u) {
            --gate->references;
        } else {
            emission_gate** link = &emission_gate_list();
            while (*link != nullptr && *link != gate)
                link = &(*link)->next;
            if (*link == gate) {
                *link = gate->next;
                destroy = true;
            }
        }
    }
    if (destroy) delete gate;
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
    using pos_type = typename traits_type::pos_type;
    using off_type = typename traits_type::off_type;
    using allocator_type = Allocator;
    using streambuf_type = basic_streambuf<CharT, Traits>;
    using ostream_type = basic_ostream<CharT, Traits>;
    using string_type = basic_string<CharT, Traits, Allocator>;
    using int_type = typename traits_type::int_type;

private:
    streambuf_type* wrapped_;
    string_type pending_;
    bool emit_on_sync_;
    bool sync_pending_;
    __syncstream_detail::emission_gate* emission_gate_;

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
        sync_pending_ = true;
        if (!emit_on_sync_) return 0;
        return emit() ? 0 : -1;
    }

public:
    basic_syncbuf()
        : basic_syncbuf(static_cast<streambuf_type*>(nullptr)) {}

    explicit basic_syncbuf(streambuf_type* wrapped)
        : basic_syncbuf(wrapped, allocator_type()) {}

    basic_syncbuf(streambuf_type* wrapped,
                  const allocator_type& allocator)
        : wrapped_(wrapped), pending_(allocator), emit_on_sync_(false),
          sync_pending_(false),
          emission_gate_(
              __syncstream_detail::acquire_emission_gate(wrapped)) {}

    explicit basic_syncbuf(ostream_type& wrapped,
                           const allocator_type& allocator = allocator_type())
        : basic_syncbuf(wrapped.rdbuf(), allocator) {}

    basic_syncbuf(const basic_syncbuf&) = delete;
    basic_syncbuf& operator=(const basic_syncbuf&) = delete;

    basic_syncbuf(basic_syncbuf&& other)
        : streambuf_type(), wrapped_(other.wrapped_),
          pending_(std::move(other.pending_)),
          emit_on_sync_(other.emit_on_sync_),
          sync_pending_(other.sync_pending_),
          emission_gate_(other.emission_gate_) {
        other.pending_.clear();
        other.wrapped_ = nullptr;
        other.emit_on_sync_ = false;
        other.sync_pending_ = false;
        other.emission_gate_ = nullptr;
    }

    basic_syncbuf(basic_syncbuf&& other, const allocator_type& allocator)
        : streambuf_type(), wrapped_(other.wrapped_),
          pending_(std::move(other.pending_), allocator),
          emit_on_sync_(other.emit_on_sync_),
          sync_pending_(other.sync_pending_),
          emission_gate_(other.emission_gate_) {
        other.pending_.clear();
        other.wrapped_ = nullptr;
        other.emit_on_sync_ = false;
        other.sync_pending_ = false;
        other.emission_gate_ = nullptr;
    }

    basic_syncbuf& operator=(basic_syncbuf&& other) {
        if (this == &other) return *this;
        (void)emit();
        streambuf_type* wrapped = other.wrapped_;
        const bool emit_on_sync = other.emit_on_sync_;
        const bool sync_pending = other.sync_pending_;
        __syncstream_detail::emission_gate* gate = other.emission_gate_;
        pending_ = std::move(other.pending_);
        __syncstream_detail::release_emission_gate(emission_gate_);
        wrapped_ = wrapped;
        emit_on_sync_ = emit_on_sync;
        sync_pending_ = sync_pending;
        emission_gate_ = gate;
        other.pending_.clear();
        other.wrapped_ = nullptr;
        other.emit_on_sync_ = false;
        other.sync_pending_ = false;
        other.emission_gate_ = nullptr;
        return *this;
    }

    ~basic_syncbuf() override {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
        (void)emit();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
        }
#endif
        __syncstream_detail::release_emission_gate(emission_gate_);
    }

    void swap(basic_syncbuf& other) {
        if (this == &other) return;
        pending_.swap(other.pending_);
        streambuf_type* wrapped = wrapped_;
        wrapped_ = other.wrapped_;
        other.wrapped_ = wrapped;
        const bool emit_on_sync = emit_on_sync_;
        emit_on_sync_ = other.emit_on_sync_;
        other.emit_on_sync_ = emit_on_sync;
        const bool sync_pending = sync_pending_;
        sync_pending_ = other.sync_pending_;
        other.sync_pending_ = sync_pending;
        __syncstream_detail::emission_gate* gate = emission_gate_;
        emission_gate_ = other.emission_gate_;
        other.emission_gate_ = gate;
    }

    streambuf_type* get_wrapped() const noexcept { return wrapped_; }

    allocator_type get_allocator() const noexcept {
        return pending_.get_allocator();
    }

    bool get_emit_on_sync() const noexcept { return emit_on_sync_; }

    void set_emit_on_sync(bool value) noexcept { emit_on_sync_ = value; }

    bool emit() { return emit_controlled(); }

    bool emit_controlled() override {
        if (wrapped_ == nullptr) {
            sync_pending_ = false;
            return false;
        }
        if (emission_gate_ == nullptr) return false;
        lock_guard<recursive_mutex> lock(
            emission_gate_->mutex);
        const bool flush_wrapped = sync_pending_;
        sync_pending_ = false;
        bool success = true;
        if (!pending_.empty()) {
            const streamsize requested =
                static_cast<streamsize>(pending_.size());
            streamsize written = wrapped_->sputn(pending_.data(), requested);
            if (written < 0) written = 0;
            if (written > requested) written = requested;
            if (written != 0)
                pending_.erase(0u, static_cast<size_t>(written));
            success = written == requested;
        }
        if (flush_wrapped && wrapped_->pubsync() == -1) success = false;
        return success;
    }

    void set_emit_on_sync_controlled(bool value) override {
        set_emit_on_sync(value);
    }
};

template<typename CharT, typename Traits, typename Allocator>
void swap(basic_syncbuf<CharT, Traits, Allocator>& left,
          basic_syncbuf<CharT, Traits, Allocator>& right) {
    left.swap(right);
}

template<typename CharT, typename Traits = char_traits<CharT>,
         typename Allocator = allocator<CharT>>
class basic_osyncstream : public basic_ostream<CharT, Traits> {
public:
    using char_type = CharT;
    using traits_type = Traits;
    using pos_type = typename traits_type::pos_type;
    using off_type = typename traits_type::off_type;
    using allocator_type = Allocator;
    using ostream_type = basic_ostream<CharT, Traits>;
    using streambuf_type = basic_streambuf<CharT, Traits>;
    using syncbuf_type = basic_syncbuf<CharT, Traits, Allocator>;

private:
    syncbuf_type buffer_;

public:
    basic_osyncstream(streambuf_type* wrapped,
                      const allocator_type& allocator)
        : ostream_type(&buffer_), buffer_(wrapped, allocator) {}

    explicit basic_osyncstream(streambuf_type* wrapped)
        : basic_osyncstream(wrapped, allocator_type()) {}

    explicit basic_osyncstream(ostream_type& wrapped,
                               const allocator_type& allocator = allocator_type())
        : basic_osyncstream(wrapped.rdbuf(), allocator) {}

    basic_osyncstream(const basic_osyncstream&) = delete;
    basic_osyncstream& operator=(const basic_osyncstream&) = delete;

    basic_osyncstream(basic_osyncstream&& other) noexcept
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

    void emit() {
        typename ostream_type::sentry sentry(*this);
        if (!sentry) return;
        bool success = false;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            success = buffer_.emit();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return;
        }
#endif
        if (!success) this->setstate(ios_base::badbit);
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
