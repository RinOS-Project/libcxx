/*
 * RinOS C++ <fstream> ✿
 * ファイルストリーム
 */

#ifndef RINCXX_FSTREAM_H
#define RINCXX_FSTREAM_H

#include "rincxx.h"
#include "ios.h"
#include "streambuf.h"
#include "ostream.h"
#include "istream.h"
#include "string.h"
#include "type_traits.h"

/* C stdio */
extern "C" {
    int rin_open(const char* path, int flags);
    int rin_close(int fd);
    long rin_read(int fd, void* buf, unsigned long count);
    long rin_write(int fd, const void* buf, unsigned long count);
    long rin_seek(int fd, long offset, int whence);
#if defined(__GNUC__) || defined(__clang__)
    /* The legacy seam returns `long`, which is 32-bit on MinGW.  Keep it for
     * existing products, but allow a 64-bit provider to be discovered at
     * runtime so large-file positions never narrow in filebuf. */
    long long rin_seek64(int fd, long long offset, int whence)
        __attribute__((weak));
#endif
}

/* ファイルフラグ */
#ifndef O_RDONLY
#define O_RDONLY    0x01
#define O_WRONLY    0x02
#define O_RDWR      0x03
#define O_CREATE    0x04
#define O_APPEND    0x08
#define O_TRUNC     0x10
#endif

namespace std {

#if defined(__cplusplus) && __cplusplus >= 201703L
namespace filesystem { class path; }
#endif

/* ═══════════════════════════════════════════════════════════════
 * basic_filebuf
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename Traits = char_traits<CharT>>
class basic_filebuf : public basic_streambuf<CharT, Traits> {
public:
    using char_type = CharT;
    using traits_type = Traits;
    using int_type = typename Traits::int_type;
    using pos_type = typename Traits::pos_type;
    using off_type = typename Traits::off_type;

private:
    static constexpr unsigned int buffer_capacity = 16u;
    static constexpr unsigned int external_capacity = buffer_capacity * 4u;

    int fd_;
    ios_base::openmode mode_;
    char_type* configured_buffer_;
    unsigned int configured_capacity_;
    char_type input_buffer_[buffer_capacity];
    char_type output_buffer_[buffer_capacity];
    char external_buffer_[external_capacity];
    unsigned char input_byte_width_[buffer_capacity];
    unsigned char decode_pending_[external_capacity];
    unsigned int decode_pending_size_;
    unsigned int conversion_pending_bytes_;
    mbstate_t conversion_state_;
    mbstate_t conversion_state_rewind_;
    mbstate_t input_state_before_buffer_;
    mbstate_t input_state_after_[buffer_capacity];
    mbstate_t previous_boundary_state_;
    char_type previous_boundary_char_;
    unsigned int previous_boundary_bytes_;
    bool has_previous_boundary_;
    unsigned int external_pending_size_;

    char_type* storage_buffer() {
        return configured_buffer_ ? configured_buffer_ : input_buffer_;
    }

    const char_type* storage_buffer() const {
        return configured_buffer_ ? configured_buffer_ : input_buffer_;
    }

    unsigned int storage_capacity() const {
        return configured_buffer_ ? configured_capacity_ : buffer_capacity;
    }

    char_type* output_storage() {
        return configured_buffer_ ? configured_buffer_ : output_buffer_;
    }

    const char_type* output_storage() const {
        return configured_buffer_ ? configured_buffer_ : output_buffer_;
    }

    bool has_unread_input() const {
        return this->gptr() && this->egptr() &&
               this->gptr() < this->egptr();
    }

    streamsize unread_input_count() const {
        return has_unread_input() ? this->egptr() - this->gptr() : 0;
    }

    streamsize unread_input_external_bytes() const {
        if (is_same<char_type, char>::value)
            return unread_input_count();
        if (!has_unread_input())
            return static_cast<streamsize>(decode_pending_size_) +
                   static_cast<streamsize>(conversion_pending_bytes_);
        const int first = input_buffer_position();
        const int last = input_buffer_size();
        streamsize bytes = static_cast<streamsize>(decode_pending_size_) +
                           static_cast<streamsize>(conversion_pending_bytes_);
        for (int index = first; index < last; ++index)
            bytes += static_cast<streamsize>(input_byte_width_[index]);
        return bytes;
    }

    int input_buffer_size() const {
        return this->eback() && this->egptr()
                   ? static_cast<int>(this->egptr() - this->eback())
                   : 0;
    }

    int input_buffer_position() const {
        return this->eback() && this->gptr()
                   ? static_cast<int>(this->gptr() - this->eback())
                   : 0;
    }

    int output_buffer_size() const {
        return this->pbase() && this->pptr()
                   ? static_cast<int>(this->pptr() - this->pbase())
                   : 0;
    }

    void clear_input_buffer() {
        this->setg(nullptr, nullptr, nullptr);
    }

    void clear_decode_pending() {
        decode_pending_size_ = 0u;
    }

    void clear_conversion_state() {
        __builtin_memset(&conversion_state_, 0, sizeof(conversion_state_));
        __builtin_memset(&conversion_state_rewind_, 0,
                         sizeof(conversion_state_rewind_));
        __builtin_memset(&input_state_before_buffer_, 0,
                         sizeof(input_state_before_buffer_));
        __builtin_memset(input_state_after_, 0, sizeof(input_state_after_));
        __builtin_memset(&previous_boundary_state_, 0,
                         sizeof(previous_boundary_state_));
        previous_boundary_char_ = char_type();
        previous_boundary_bytes_ = 0u;
        has_previous_boundary_ = false;
        conversion_pending_bytes_ = 0u;
    }

    void capture_previous_boundary() {
        if (is_same<char_type, char>::value || has_unread_input()) return;
        const int size = input_buffer_size();
        if (size <= 0 || !this->gptr() || this->gptr() != this->egptr())
            return;
        const unsigned int width = input_byte_width_[size - 1];
        if (width == 0u) return;
        previous_boundary_char_ = storage_buffer()[size - 1];
        previous_boundary_bytes_ = width;
        previous_boundary_state_ = input_state_after_[size - 1];
        has_previous_boundary_ = true;
    }

    void disable_output_buffer() {
        this->setp(nullptr, nullptr);
    }

    void reset_output_buffer(int used = 0) {
        if (!(mode_ & ios_base::out)) {
            disable_output_buffer();
            return;
        }
        char_type* storage = output_storage();
        this->setp(storage, storage + storage_capacity());
        if (used > 0) this->pbump(used);
    }

    void reset_closed_buffers() {
        clear_input_buffer();
        disable_output_buffer();
        clear_decode_pending();
        clear_conversion_state();
        external_pending_size_ = 0u;
    }

    void retain_unwritten_output(int transferred, int pending) {
        const int remaining = pending - transferred;
        if (remaining > 0) {
            Traits::move(output_storage(), output_storage() + transferred,
                         static_cast<size_t>(remaining));
        }
        reset_output_buffer(remaining);
    }

    bool convert_output(char* destination, int count, int& written) {
        const locale selected = this->getloc();
        const codecvt<char_type, char, mbstate_t>& conversion =
            use_facet<codecvt<char_type, char, mbstate_t>>(selected);
        const char_type* output = output_storage();
        const char_type* from_next = output;
        char* to_next = destination;
        const codecvt_base::result result = conversion.out(
            conversion_state_, output, output + count,
            from_next, destination, destination + external_capacity, to_next);
        written = static_cast<int>(to_next - destination);
        /* A filebuf flush owns the complete character buffer.  Do not
         * publish a partial conversion (which could otherwise lose the
         * codecvt state or duplicate a stateful shift on retry). */
        return (result == codecvt_base::ok || result == codecvt_base::noconv) &&
               from_next == output + count;
    }

    bool flush_output() {
        while (external_pending_size_ != 0u) {
            const long written = rin_write(
                fd_, external_buffer_,
                static_cast<unsigned long>(external_pending_size_));
            if (written <= 0 ||
                written > static_cast<long>(external_pending_size_)) {
                if (written > 0 &&
                    written < static_cast<long>(external_pending_size_)) {
                    const unsigned int remaining =
                        external_pending_size_ - static_cast<unsigned int>(written);
                    for (unsigned int i = 0; i != remaining; ++i)
                        external_buffer_[i] = external_buffer_[written + i];
                    external_pending_size_ = remaining;
                }
                return false;
            }
            external_pending_size_ -= static_cast<unsigned int>(written);
            if (external_pending_size_ != 0u) {
                for (unsigned int i = 0; i != external_pending_size_; ++i)
                    external_buffer_[i] = external_buffer_[written + i];
            }
        }

        const int pending = output_buffer_size();
        if (pending == 0) return true;

        const bool narrow = is_same<char_type, char>::value;
        int external_size = pending;
        if (!narrow && !convert_output(external_buffer_, pending, external_size))
            return false;

        int transferred = 0;
        while (transferred < external_size) {
            const int remaining = external_size - transferred;
            const long written = rin_write(
                fd_, narrow ? static_cast<const void*>(output_storage() + transferred)
                             : static_cast<const void*>(external_buffer_ + transferred),
                static_cast<unsigned long>(remaining));
            if (written <= 0 || written > remaining) {
                if (!narrow) {
                    const unsigned int left =
                        external_size - static_cast<unsigned int>(transferred);
                    for (unsigned int i = 0; i != left; ++i)
                        external_buffer_[i] = external_buffer_[transferred + i];
                    external_pending_size_ = left;
                    reset_output_buffer();
                } else {
                    retain_unwritten_output(transferred, pending);
                }
                return false;
            }
            transferred += static_cast<int>(written);
        }

        reset_output_buffer();
        return true;
    }

    bool rewind_unread_input() {
        const streamsize unread = unread_input_external_bytes();
        if (unread == 0) {
            clear_input_buffer();
            clear_decode_pending();
            return true;
        }
        /* The current descriptor position may be above LONG_MAX on LLP64
         * hosts.  Rewinding a read-ahead buffer is still a seek operation,
         * so it must use the same 64-bit provider and representability check
         * as public seekoff/seekpos instead of narrowing the small negative
         * delta through the legacy rin_seek ABI. */
        off_type rewind = -static_cast<off_type>(unread);
        off_type position = 0;
        if (!seek_backend(rewind, ios_base::cur, position)) return false;
        clear_input_buffer();
        clear_decode_pending();
        conversion_state_ = input_state_before_buffer_;
        conversion_pending_bytes_ = 0u;
        return true;
    }

    bool prepare_for_output() {
        if (!is_open() || !(mode_ & ios_base::out)) return false;
        if (!rewind_unread_input()) return false;
        if (!this->pbase() || !this->pptr()) reset_output_buffer();
        return true;
    }

    void restore_buffers(int input_size, int input_position, int output_size) {
        if (input_size > 0) {
            char_type* storage = storage_buffer();
            this->setg(storage, storage + input_position,
                       storage + input_size);
        } else {
            clear_input_buffer();
        }

        if ((mode_ & ios_base::out) && input_position >= input_size) {
            reset_output_buffer(output_size);
        } else {
            disable_output_buffer();
        }
    }

    static bool decode_utf8(const unsigned char* bytes, unsigned int available,
                            char_type values[2], unsigned int& value_count,
                            unsigned int& consumed) {
        if (available == 0u) return false;
        const unsigned int first = bytes[0];
        unsigned long codepoint = 0u;
        unsigned int needed = 0u;
        unsigned long minimum = 0u;
        if (first <= 0x7fu) {
            codepoint = first;
            needed = 1u;
            minimum = 0u;
        } else if (first >= 0xc2u && first <= 0xdfu) {
            codepoint = first & 0x1fu;
            needed = 2u;
            minimum = 0x80u;
        } else if (first >= 0xe0u && first <= 0xefu) {
            codepoint = first & 0x0fu;
            needed = 3u;
            minimum = 0x800u;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            codepoint = first & 0x07u;
            needed = 4u;
            minimum = 0x10000u;
        } else {
            consumed = 0u;
            return false;
        }
        if (available < needed) {
            consumed = needed;
            return false;
        }
        for (unsigned int index = 1u; index != needed; ++index) {
            const unsigned int byte = bytes[index];
            if ((byte & 0xc0u) != 0x80u) {
                consumed = 0u;
                return false;
            }
            codepoint = (codepoint << 6) | (byte & 0x3fu);
        }
        if (codepoint < minimum || codepoint > 0x10ffffu ||
            (codepoint >= 0xd800u && codepoint <= 0xdfffu)) {
            consumed = 0u;
            return false;
        }
        if (sizeof(char_type) == 2u && codepoint > 0xffffu) {
            const unsigned long scalar = codepoint - 0x10000u;
            values[0] = static_cast<char_type>(0xd800u | (scalar >> 10u));
            values[1] = static_cast<char_type>(0xdc00u | (scalar & 0x3ffu));
            value_count = 2u;
        } else {
            values[0] = static_cast<char_type>(codepoint);
            value_count = 1u;
        }
        consumed = needed;
        return true;
    }

    bool seek_backend(off_type offset, int whence, off_type& position) {
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
        if (rin_seek64 != nullptr) {
            const long long result = rin_seek64(
                fd_, static_cast<long long>(offset), whence);
            if (result < 0) return false;
            position = static_cast<off_type>(result);
            return static_cast<long long>(position) == result;
        }
#pragma GCC diagnostic pop
#endif
        const off_type long_min = static_cast<off_type>(-__LONG_MAX__ - 1L);
        const off_type long_max = static_cast<off_type>(__LONG_MAX__);
        if (offset < long_min || offset > long_max) return false;
        const long result = rin_seek(fd_, static_cast<long>(offset), whence);
        if (result < 0) return false;
        position = static_cast<off_type>(result);
        return true;
    }

    bool rewind_external_bytes(unsigned int bytes) {
        if (bytes == 0u) return true;
        /* Read-ahead rollback is a position mutation just like public seek.
         * Route it through the 64-bit provider whenever available; narrowing
         * this bounded byte count through the legacy long ABI corrupts a
         * wide stream positioned above LONG_MAX. */
        const off_type amount = static_cast<off_type>(bytes);
        if (amount < 0) return false;
        off_type ignored = 0;
        return seek_backend(-amount, ios_base::cur, ignored);
    }

    void take_from(basic_filebuf& rhs) {
        const int input_size = rhs.input_buffer_size();
        const int input_position = rhs.input_buffer_position();
        const int output_size = rhs.output_buffer_size();
        configured_buffer_ = rhs.configured_buffer_;
        configured_capacity_ = rhs.configured_capacity_;
        for (int index = 0; index != input_size; ++index) {
            storage_buffer()[index] = rhs.storage_buffer()[index];
        }
        for (int index = 0; index != output_size; ++index) {
            output_storage()[index] = rhs.output_storage()[index];
        }
        for (unsigned int index = 0u; index != buffer_capacity; ++index)
            input_byte_width_[index] = rhs.input_byte_width_[index];
        for (unsigned int index = 0u; index != buffer_capacity; ++index)
            input_state_after_[index] = rhs.input_state_after_[index];
        for (unsigned int index = 0u; index != external_capacity; ++index)
            decode_pending_[index] = rhs.decode_pending_[index];
        decode_pending_size_ = rhs.decode_pending_size_;
        conversion_pending_bytes_ = rhs.conversion_pending_bytes_;
        conversion_state_ = rhs.conversion_state_;
        conversion_state_rewind_ = rhs.conversion_state_rewind_;
        input_state_before_buffer_ = rhs.input_state_before_buffer_;
        previous_boundary_state_ = rhs.previous_boundary_state_;
        previous_boundary_char_ = rhs.previous_boundary_char_;
        previous_boundary_bytes_ = rhs.previous_boundary_bytes_;
        has_previous_boundary_ = rhs.has_previous_boundary_;
        external_pending_size_ = rhs.external_pending_size_;
        for (unsigned int index = 0u; index != external_pending_size_; ++index)
            external_buffer_[index] = rhs.external_buffer_[index];

        fd_ = rhs.fd_;
        mode_ = rhs.mode_;
        restore_buffers(input_size, input_position, output_size);

        rhs.fd_ = -1;
        rhs.mode_ = 0;
        rhs.configured_buffer_ = nullptr;
        rhs.configured_capacity_ = 0u;
        rhs.reset_closed_buffers();
    }

public:
    basic_filebuf()
        : fd_(-1), mode_(0), configured_buffer_(nullptr),
          configured_capacity_(0u), input_buffer_{}, output_buffer_{},
          external_buffer_{}, input_byte_width_{}, decode_pending_{},
          decode_pending_size_(0u), conversion_pending_bytes_(0u),
          conversion_state_{}, conversion_state_rewind_{},
          input_state_before_buffer_{}, input_state_after_{},
          previous_boundary_state_{}, previous_boundary_char_{},
          previous_boundary_bytes_(0u), has_previous_boundary_(false),
          external_pending_size_(0u) {
        reset_closed_buffers();
    }

    basic_filebuf(basic_filebuf&& rhs) noexcept
        : fd_(-1), mode_(0), configured_buffer_(nullptr),
          configured_capacity_(0u), input_buffer_{}, output_buffer_{},
          external_buffer_{}, input_byte_width_{}, decode_pending_{},
          decode_pending_size_(0u), conversion_pending_bytes_(0u),
          conversion_state_{}, conversion_state_rewind_{},
          input_state_before_buffer_{}, input_state_after_{},
          previous_boundary_state_{}, previous_boundary_char_{},
          previous_boundary_bytes_(0u), has_previous_boundary_(false),
          external_pending_size_(0u) {
        take_from(rhs);
    }

    ~basic_filebuf() {
        close();
    }

    basic_filebuf& operator=(basic_filebuf&& rhs) noexcept {
        if (this != &rhs) {
            close();
            take_from(rhs);
        }
        return *this;
    }

    /* コピー禁止 */
    basic_filebuf(const basic_filebuf&) = delete;
    basic_filebuf& operator=(const basic_filebuf&) = delete;

    /* ファイル操作 */
    bool is_open() const noexcept { return fd_ >= 0; }

    /* Adopt caller-owned storage before open.  The implementation keeps the
     * same fixed bound as its stateful codecvt bookkeeping, so a wide stream
     * can never overrun the per-code-unit width table.  Passing nullptr/0
     * restores the internal storage. */
    basic_filebuf* setbuf(char_type* buffer, streamsize count) override {
        if (is_open() || count < 0 ||
            (buffer == nullptr && count != 0) ||
            (buffer != nullptr && count == 0) ||
            static_cast<unsigned long>(count) > buffer_capacity) {
            return nullptr;
        }
        configured_buffer_ = buffer;
        configured_capacity_ = buffer == nullptr
            ? 0u : static_cast<unsigned int>(count);
        reset_closed_buffers();
        return this;
    }

    basic_filebuf* open(const char* filename, ios_base::openmode mode) {
        if (is_open() || !filename) return nullptr;

        constexpr ios_base::openmode known_modes =
            ios_base::app | ios_base::ate | ios_base::binary |
            ios_base::in | ios_base::out | ios_base::trunc;
        if ((mode & ~known_modes) != 0u) return nullptr;

        const bool input = (mode & ios_base::in) != 0u;
        const bool append = (mode & ios_base::app) != 0u;
        const bool output = (mode & ios_base::out) != 0u || append;
        const bool truncate = (mode & ios_base::trunc) != 0u;
        if ((!input && !output) || (truncate && !output) ||
            (append && truncate)) {
            return nullptr;
        }

        int flags;
        if (input && output) {
            flags = O_RDWR;
        } else if (input) {
            flags = O_RDONLY;
        } else {
            flags = O_WRONLY;
        }

        if (append) flags |= O_APPEND | O_CREATE;
        if (truncate || (output && !input && !append)) {
            flags |= O_TRUNC | O_CREATE;
        }

        fd_ = rin_open(filename, flags);
        if (fd_ < 0) return nullptr;

        mode_ = mode | (output ? ios_base::out : 0u);
        clear_input_buffer();
        clear_decode_pending();
        external_pending_size_ = 0u;
        reset_output_buffer();

        if (mode & ios_base::ate) {
            off_type position = 0;
            if (!seek_backend(0, 2, position)) {  /* SEEK_END */
                (void)rin_close(fd_);
                fd_ = -1;
                mode_ = 0;
                reset_closed_buffers();
                return nullptr;
            }
        }

        return this;
    }

    basic_filebuf* open(const string& filename, ios_base::openmode mode) {
        return open(filename.c_str(), mode);
    }

#if defined(__cplusplus) && __cplusplus >= 201703L
    template<typename Path,
             typename enable_if<
                 is_same<typename decay<Path>::type, filesystem::path>::value,
                 int>::type = 0>
    basic_filebuf* open(const Path& filename, ios_base::openmode mode) {
        return open(filename.c_str(), mode);
    }
#endif

    basic_filebuf* close() {
        if (!is_open()) return nullptr;

        bool okay = sync() == 0;
        if (okay && !is_same<char_type, char>::value) {
            const locale selected = this->getloc();
            const codecvt<char_type, char, mbstate_t>& conversion =
                use_facet<codecvt<char_type, char, mbstate_t>>(selected);
            /* Stateful facets may need several unshift calls.  Publish each
             * produced byte only through the existing pending-output owner,
             * then retry while the facet reports progress.  A partial result
             * with no bytes is an explicit no-progress failure rather than a
             * successful close that leaves the external encoding state live. */
            for (;;) {
                char* next = external_buffer_;
                const codecvt_base::result result = conversion.unshift(
                    conversion_state_, external_buffer_,
                    external_buffer_ + external_capacity, next);
                const int pending = static_cast<int>(next - external_buffer_);
                if (result == codecvt_base::error ||
                    (result == codecvt_base::partial && pending == 0)) {
                    okay = false;
                    break;
                }
                if (pending != 0) {
                    external_pending_size_ = static_cast<unsigned int>(pending);
                    if (!flush_output()) {
                        okay = false;
                        break;
                    }
                }
                if (result != codecvt_base::partial) break;
            }
        }
        if (rin_close(fd_) != 0) okay = false;
        fd_ = -1;
        mode_ = 0;
        reset_closed_buffers();

        return okay ? this : nullptr;
    }

    void swap(basic_filebuf& rhs) noexcept {
        if (this == &rhs) return;

        const int input_size = input_buffer_size();
        const int input_position = input_buffer_position();
        const int output_size = output_buffer_size();
        const int rhs_input_size = rhs.input_buffer_size();
        const int rhs_input_position = rhs.input_buffer_position();
        const int rhs_output_size = rhs.output_buffer_size();

        /* Keep each object's caller-owned buffer association, but transfer the
         * staged bytes with the descriptor state.  The old implementation
         * only exchanged external bytes when both objects had configured
         * buffers; swapping a configured buffer with an internal one then
         * restored the right-hand state from stale memory.  Snapshot both
         * bounded areas before touching either destination so external and
         * internal storage use the same failure-atomic path. */
        char_type left_input[buffer_capacity] = {};
        char_type right_input[buffer_capacity] = {};
        char_type left_output[buffer_capacity] = {};
        char_type right_output[buffer_capacity] = {};
        const unsigned int left_capacity = configured_buffer_
            ? configured_capacity_ : buffer_capacity;
        const unsigned int right_capacity = rhs.configured_buffer_
            ? rhs.configured_capacity_ : buffer_capacity;
        const unsigned int left_active = static_cast<unsigned int>(
            input_size > output_size ? input_size : output_size);
        const unsigned int right_active = static_cast<unsigned int>(
            rhs_input_size > rhs_output_size ? rhs_input_size : rhs_output_size);
        /* A smaller caller buffer cannot represent the other's currently
         * staged state.  Leave both streams untouched instead of publishing a
         * truncated pending write or read-ahead area.  Identical external
         * buffers cannot hold two independent snapshots safely either. */
        if (left_active > right_capacity || right_active > left_capacity ||
            (configured_buffer_ && rhs.configured_buffer_ &&
             configured_buffer_ == rhs.configured_buffer_ &&
             (left_active != 0u || right_active != 0u)))
            return;
        for (int index = 0; index != input_size; ++index)
            left_input[index] = storage_buffer()[index];
        for (int index = 0; index != rhs_input_size; ++index)
            right_input[index] = rhs.storage_buffer()[index];
        for (int index = 0; index != output_size; ++index)
            left_output[index] = output_storage()[index];
        for (int index = 0; index != rhs_output_size; ++index)
            right_output[index] = rhs.output_storage()[index];

        using std::swap;
        mbstate_t state_temp = {};
        swap(fd_, rhs.fd_);
        swap(mode_, rhs.mode_);
        for (unsigned int index = 0; index != buffer_capacity; ++index) {
            swap(input_buffer_[index], rhs.input_buffer_[index]);
            swap(output_buffer_[index], rhs.output_buffer_[index]);
            swap(input_byte_width_[index], rhs.input_byte_width_[index]);
            state_temp = input_state_after_[index];
            input_state_after_[index] = rhs.input_state_after_[index];
            rhs.input_state_after_[index] = state_temp;
        }
        for (unsigned int index = 0u; index != external_capacity; ++index)
            swap(decode_pending_[index], rhs.decode_pending_[index]);
        swap(decode_pending_size_, rhs.decode_pending_size_);
        swap(external_pending_size_, rhs.external_pending_size_);
        for (unsigned int index = 0u; index != external_capacity; ++index)
            swap(external_buffer_[index], rhs.external_buffer_[index]);
        swap(conversion_pending_bytes_, rhs.conversion_pending_bytes_);
        state_temp = conversion_state_;
        conversion_state_ = rhs.conversion_state_;
        rhs.conversion_state_ = state_temp;
        state_temp = conversion_state_rewind_;
        conversion_state_rewind_ = rhs.conversion_state_rewind_;
        rhs.conversion_state_rewind_ = state_temp;
        state_temp = input_state_before_buffer_;
        input_state_before_buffer_ = rhs.input_state_before_buffer_;
        rhs.input_state_before_buffer_ = state_temp;
        state_temp = previous_boundary_state_;
        previous_boundary_state_ = rhs.previous_boundary_state_;
        rhs.previous_boundary_state_ = state_temp;
        swap(previous_boundary_char_, rhs.previous_boundary_char_);
        swap(previous_boundary_bytes_, rhs.previous_boundary_bytes_);
        swap(has_previous_boundary_, rhs.has_previous_boundary_);
        for (int index = 0; index != rhs_input_size; ++index)
            storage_buffer()[index] = right_input[index];
        for (int index = 0; index != input_size; ++index)
            rhs.storage_buffer()[index] = left_input[index];
        for (int index = 0; index != rhs_output_size; ++index)
            output_storage()[index] = right_output[index];
        for (int index = 0; index != output_size; ++index)
            rhs.output_storage()[index] = left_output[index];
        restore_buffers(rhs_input_size, rhs_input_position, rhs_output_size);
        rhs.restore_buffers(input_size, input_position, output_size);
    }

protected:
    streamsize showmanyc() override {
        /* `basic_streambuf::in_avail()` already reports the get-area span
         * directly when it is populated. Once that span is exhausted it calls
         * this hook; never probe the backend here because a probe would
         * consume bytes and violate showmanyc's non-consuming contract. */
        return unread_input_count();
    }

    void imbue(const locale&) override {
        /* A locale is normally selected before open.  When a caller changes
         * it while no buffered character is pending, reset the stateful
         * conversion token; otherwise retain the old state until sync/close
         * so already-read bytes are not silently discarded. */
        if (!has_unread_input() && output_buffer_size() == 0 &&
            decode_pending_size_ == 0u) {
            clear_conversion_state();
        }
    }

    int_type overflow(int_type c = Traits::eof()) override {
        if (!prepare_for_output() || !flush_output()) return Traits::eof();
        if (Traits::eq_int_type(c, Traits::eof())) {
            return Traits::not_eof(c);
        }
        *this->pptr() = Traits::to_char_type(c);
        this->pbump(1);
        return c;
    }

    /* The base streambuf can only put back a character while gptr() still
     * has room inside the get area.  A filebuf must also support one
     * character at a read-ahead boundary.  For a wide buffer retain the
     * codecvt state after the last produced code unit; the descriptor is
     * rewound over the unread suffix and the encoded bytes of that unit, then
     * advanced by the unit width without running the codecvt a second time.
     * This keeps stateful facets from observing a synthetic input transition
     * while still making the next underflow resume at the exact byte. */
    int_type pbackfail(int_type c = Traits::eof()) override {
        if (!is_open() || !(mode_ & ios_base::in) ||
            !this->eback() || !this->gptr() || !this->egptr() ||
            (this->gptr() != this->eback() &&
             this->gptr() != this->egptr())) {
            return Traits::eof();
        }

        if (!is_same<char_type, char>::value) {
            const int input_size = input_buffer_size();
            const bool at_end = this->gptr() == this->egptr();
            char_type previous = char_type();
            unsigned int width = 0u;
            mbstate_t state_after = {};
            if (at_end && input_size > 0) {
                const int last = input_size - 1;
                width = input_byte_width_[last];
                if (width == 0u) return Traits::eof();
                previous = storage_buffer()[last];
                state_after = input_state_after_[last];
            } else if (this->gptr() == this->eback() &&
                       has_previous_boundary_) {
                width = previous_boundary_bytes_;
                if (width == 0u) return Traits::eof();
                previous = previous_boundary_char_;
                state_after = previous_boundary_state_;
            } else {
                return Traits::eof();
            }

            const streamsize unread = unread_input_external_bytes();
            const unsigned long long maximum =
                static_cast<unsigned long long>(
                    numeric_limits<off_type>::max());
            if (unread < 0 || static_cast<unsigned long long>(unread) >
                                  maximum - static_cast<unsigned long long>(width))
                return Traits::eof();
            const off_type rewind = static_cast<off_type>(unread) +
                                    static_cast<off_type>(width);
            off_type ignored = 0;
            if (!seek_backend(-rewind, ios_base::cur, ignored))
                return Traits::eof();

            /* Discard exactly the previous encoded unit.  Reading it through
             * the provider, rather than decoding it, leaves the facet state
             * at the captured post-unit snapshot. */
            unsigned int remaining = width;
            char discarded[16] = {};
            bool read_ok = true;
            while (remaining != 0u) {
                const unsigned int chunk = remaining < sizeof(discarded)
                    ? remaining : static_cast<unsigned int>(sizeof(discarded));
                const long count = rin_read(
                    fd_, discarded, static_cast<unsigned long>(chunk));
                if (count != static_cast<long>(chunk)) {
                    read_ok = false;
                    break;
                }
                remaining -= chunk;
            }
            if (!read_ok) {
                if (!seek_backend(rewind, ios_base::cur, ignored)) {
                    (void)rin_close(fd_);
                    fd_ = -1;
                    mode_ = 0;
                    reset_closed_buffers();
                }
                return Traits::eof();
            }

            if (!Traits::eq_int_type(c, Traits::eof()) &&
                !Traits::eq(previous, Traits::to_char_type(c))) {
                if (!seek_backend(rewind, ios_base::cur, ignored)) {
                    (void)rin_close(fd_);
                    fd_ = -1;
                    mode_ = 0;
                    reset_closed_buffers();
                }
                return Traits::eof();
            }

            this->storage_buffer()[0] = previous;
            this->setg(this->storage_buffer(), this->storage_buffer(),
                       this->storage_buffer() + 1);
            input_byte_width_[0] = static_cast<unsigned char>(
                width > 255u ? 255u : width);
            input_state_after_[0] = state_after;
            conversion_state_ = state_after;
            conversion_state_rewind_ = state_after;
            input_state_before_buffer_ = state_after;
            clear_decode_pending();
            conversion_pending_bytes_ = 0u;
            has_previous_boundary_ = false;
            return Traits::to_int_type(previous);
        }

        const streamsize unread = unread_input_count();
        if (unread < 0 || static_cast<unsigned long long>(unread) >=
                              static_cast<unsigned long long>(
                                  numeric_limits<off_type>::max())) {
            return Traits::eof();
        }
        const off_type rewind = static_cast<off_type>(unread) + 1;
        off_type ignored = 0;
        if (!seek_backend(-rewind, ios_base::cur, ignored))
            return Traits::eof();

        char previous = 0;
        const long read_count = rin_read(
            fd_, &previous, static_cast<unsigned long>(1));
        if (read_count != 1) {
            if (!seek_backend(static_cast<off_type>(unread),
                              ios_base::cur, ignored)) {
                /* A short read normally rolls the descriptor back.  If that
                 * compensating seek fails, keeping the file open would
                 * expose an unknown logical position to the caller.  Close
                 * the owner and publish only the terminal failure instead. */
                (void)rin_close(fd_);
                fd_ = -1;
                mode_ = 0;
                reset_closed_buffers();
            }
            return Traits::eof();
        }

        if (!Traits::eq_int_type(c, Traits::eof()) &&
            Traits::to_char_type(c) != previous) {
            /* A mismatched putback is not allowed to rewrite the file.  Put
             * the descriptor back at its exact pre-call position and leave
             * the get area untouched. */
            if (!seek_backend(static_cast<off_type>(unread),
                              ios_base::cur, ignored)) {
                (void)rin_close(fd_);
                fd_ = -1;
                mode_ = 0;
                reset_closed_buffers();
            }
            return Traits::eof();
        }

        this->storage_buffer()[0] = static_cast<char_type>(previous);
        this->setg(this->storage_buffer(), this->storage_buffer(),
                   this->storage_buffer() + 1);
        return Traits::to_int_type(static_cast<char_type>(previous));
    }

    streamsize xsgetn(char_type* s, streamsize n) override {
        if (!is_open() || !(mode_ & ios_base::in) || !s || n <= 0) {
            return 0;
        }
        return basic_streambuf<CharT, Traits>::xsgetn(s, n);
    }

    streamsize xsputn(const char_type* s, streamsize n) override {
        if (!is_open() || !(mode_ & ios_base::out) || !s || n <= 0) {
            return 0;
        }
        if (!prepare_for_output()) return 0;
        return basic_streambuf<CharT, Traits>::xsputn(s, n);
    }

    int_type underflow() override {
        if (!is_open() || !(mode_ & ios_base::in)) {
            return Traits::eof();
        }
        if (has_unread_input()) {
            return Traits::to_int_type(*this->gptr());
        }
        if (!flush_output()) return Traits::eof();

        capture_previous_boundary();
        clear_input_buffer();
        disable_output_buffer();
        if (is_same<char_type, char>::value) {
            char_type* storage = storage_buffer();
            const long read_count = rin_read(fd_, static_cast<void*>(storage),
                                             static_cast<unsigned long>(storage_capacity()));
            if (read_count <= 0 ||
                read_count > static_cast<long>(storage_capacity())) return Traits::eof();
            this->setg(storage, storage,
                       storage + static_cast<int>(read_count));
            return Traits::to_int_type(*this->gptr());
        }

        const locale selected = this->getloc();
        const codecvt<char_type, char, mbstate_t>& conversion =
            use_facet<codecvt<char_type, char, mbstate_t>>(selected);
        input_state_before_buffer_ = conversion_state_;
        for (;;) {
            const unsigned int pending_before = decode_pending_size_;
            const long read_count = rin_read(
                fd_, static_cast<void*>(external_buffer_),
                static_cast<unsigned long>(storage_capacity()));
            if (read_count <= 0 ||
                read_count > static_cast<long>(storage_capacity())) {
                const unsigned int rewind =
                    pending_before + conversion_pending_bytes_;
                if (rewind != 0u)
                    (void)rewind_external_bytes(rewind);
                clear_decode_pending();
                clear_conversion_state();
                return Traits::eof();
            }

            unsigned char bytes[external_capacity + buffer_capacity] = {};
            for (unsigned int index = 0u; index != pending_before; ++index)
                bytes[index] = decode_pending_[index];
            for (long index = 0; index != read_count; ++index)
                bytes[pending_before + static_cast<unsigned int>(index)] =
                    static_cast<unsigned char>(external_buffer_[index]);
            const unsigned int total =
                pending_before + static_cast<unsigned int>(read_count);
            clear_decode_pending();

            unsigned int byte_index = 0u;
            unsigned int output_count = 0u;
            char_type* storage = storage_buffer();
            while (byte_index < total && output_count < storage_capacity()) {
                const mbstate_t state_before = conversion_state_;
                const char* from = reinterpret_cast<const char*>(
                    bytes + byte_index);
                const char* from_next = from;
                char_type* to = storage + output_count;
                char_type* to_next = to;
                codecvt_base::result result = conversion.in(
                            conversion_state_, from,
                            reinterpret_cast<const char*>(bytes + total),
                            from_next, to, to + 1,
                            to_next);
                unsigned int consumed = static_cast<unsigned int>(
                    from_next - from);
                unsigned int produced = static_cast<unsigned int>(
                    to_next - to);

                /* A multicode-unit character (for example a UTF-16
                 * surrogate pair) may report partial solely because the
                 * one-unit probe is too small. Retry with the remaining
                 * input-buffer capacity without changing the conversion
                 * state. */
                if (result == codecvt_base::partial && produced == 0u &&
                    output_count + 1u < storage_capacity()) {
                    conversion_state_ = state_before;
                    from_next = from;
                    to_next = to;
                    result = conversion.in(
                                conversion_state_, from,
                                reinterpret_cast<const char*>(bytes + total),
                                from_next, to,
                                storage + storage_capacity(), to_next);
                    consumed = static_cast<unsigned int>(from_next - from);
                    produced = static_cast<unsigned int>(to_next - to);
                }

                if (result == codecvt_base::error) {
                    const unsigned int rewind =
                        total + conversion_pending_bytes_;
                    (void)rewind_external_bytes(rewind);
                    conversion_state_ = conversion_state_rewind_;
                    clear_decode_pending();
                    conversion_pending_bytes_ = 0u;
                    return Traits::eof();
                }

                if (produced == 0u) {
                    if (consumed != 0u) {
                        if (conversion_pending_bytes_ == 0u)
                            conversion_state_rewind_ = state_before;
                        if (conversion_pending_bytes_ >
                            0xffffffffu - consumed) {
                            return Traits::eof();
                        }
                        conversion_pending_bytes_ += consumed;
                        byte_index += consumed;
                    }
                    if (result != codecvt_base::partial || consumed == 0u)
                        break;
                    continue;
                }

                const unsigned int width = conversion_pending_bytes_ + consumed;
                for (unsigned int index = 0u; index != produced; ++index)
                    input_byte_width_[output_count + index] =
                        static_cast<unsigned char>(index + 1u == produced
                            ? (width > 255u ? 255u : width) : 0u);
                for (unsigned int index = 0u; index != produced; ++index)
                    input_state_after_[output_count + index] =
                        conversion_state_;
                conversion_pending_bytes_ = 0u;
                output_count += produced;
                byte_index += consumed;
                if (result == codecvt_base::partial && consumed == 0u)
                    break;
            }

            if (byte_index < total) {
                decode_pending_size_ = total - byte_index;
                if (decode_pending_size_ > external_capacity) {
                    const unsigned int rewind =
                        total + conversion_pending_bytes_;
                    (void)rewind_external_bytes(rewind);
                    clear_decode_pending();
                    clear_conversion_state();
                    return Traits::eof();
                }
                for (unsigned int index = 0u;
                     index != decode_pending_size_; ++index)
                    decode_pending_[index] = bytes[byte_index + index];
            }

            if (output_count != 0u) {
                char_type* storage = storage_buffer();
                this->setg(storage, storage,
                           storage + static_cast<int>(output_count));
                return Traits::to_int_type(*this->gptr());
            }
            /* A stateful codecvt may consume a prefix into mbstate_t without
             * producing a code unit. Read another bounded chunk before
             * exposing a character. */
        }
    }

    pos_type seekoff(off_type off, ios_base::seekdir way,
                     ios_base::openmode which =
                         ios_base::in | ios_base::out) override {
        if (!is_open()) return pos_type(off_type(-1));
        const ios_base::openmode requested =
            which & (ios_base::in | ios_base::out);
        const ios_base::openmode available =
            mode_ & (ios_base::in | ios_base::out);
        if (requested == 0u || (requested & available) == 0u) {
            return pos_type(off_type(-1));
        }

        int whence;
        switch (way) {
            case ios_base::beg: whence = 0; break;
            case ios_base::cur: whence = 1; break;
            case ios_base::end: whence = 2; break;
            default: return pos_type(off_type(-1));
        }

        if (!flush_output()) return pos_type(off_type(-1));

        const streamsize unread = unread_input_external_bytes();
        if (way == ios_base::cur && unread > 0) {
            const off_type minimum = numeric_limits<off_type>::min();
            if (off < minimum + unread) {
                return pos_type(off_type(-1));
            }
            off -= unread;
        }
        off_type position = 0;
        if (!seek_backend(off, whence, position))
            return pos_type(off_type(-1));
        clear_input_buffer();
        reset_output_buffer();
        return pos_type(position);
    }

    pos_type seekpos(pos_type pos,
                     ios_base::openmode which = ios_base::in | ios_base::out) override {
        return seekoff(off_type(pos), ios_base::beg, which);
    }

    int sync() override {
        if (!is_open()) return -1;
        if (!flush_output() || !rewind_unread_input()) return -1;
        reset_output_buffer();
        return 0;
    }
};

/* 型エイリアス */
using filebuf = basic_filebuf<char>;
using wfilebuf = basic_filebuf<wchar_t>;

/* ═══════════════════════════════════════════════════════════════
 * basic_ifstream
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename Traits = char_traits<CharT>>
class basic_ifstream : public basic_istream<CharT, Traits> {
    basic_filebuf<CharT, Traits> buf_;

public:
    using char_type = CharT;
    using traits_type = Traits;

    basic_ifstream() : basic_istream<CharT, Traits>(&buf_) {}

    explicit basic_ifstream(const char* filename,
                            ios_base::openmode mode = ios_base::in)
        : basic_istream<CharT, Traits>(&buf_) {
        open(filename, mode);
    }

    explicit basic_ifstream(const string& filename,
                            ios_base::openmode mode = ios_base::in)
        : basic_ifstream(filename.c_str(), mode) {}

#if defined(__cplusplus) && __cplusplus >= 201703L
    template<typename Path,
             typename enable_if<
                 is_same<typename decay<Path>::type, filesystem::path>::value,
                 int>::type = 0>
    explicit basic_ifstream(const Path& filename,
                            ios_base::openmode mode = ios_base::in)
        : basic_istream<CharT, Traits>(&buf_) {
        open(filename, mode);
    }
#endif

    basic_ifstream(basic_ifstream&& other) noexcept
        : basic_istream<CharT, Traits>(std::move(other)),
          buf_(std::move(other.buf_)) {
        this->set_rdbuf(&buf_);
    }

    ~basic_ifstream() = default;

    basic_ifstream& operator=(basic_ifstream&& other) noexcept {
        basic_istream<CharT, Traits>::operator=(std::move(other));
        buf_ = std::move(other.buf_);
        this->set_rdbuf(&buf_);
        return *this;
    }

    /* コピー禁止 */
    basic_ifstream(const basic_ifstream&) = delete;
    basic_ifstream& operator=(const basic_ifstream&) = delete;

    /* ファイル操作 */
    basic_filebuf<CharT, Traits>* rdbuf() const noexcept {
        return const_cast<basic_filebuf<CharT, Traits>*>(&buf_);
    }

    bool is_open() const noexcept { return buf_.is_open(); }

    void open(const char* filename, ios_base::openmode mode = ios_base::in) {
        if (buf_.open(filename, mode | ios_base::in)) {
            this->clear();
        } else {
            this->setstate(ios_base::failbit);
        }
    }

    void open(const string& filename, ios_base::openmode mode = ios_base::in) {
        open(filename.c_str(), mode);
    }

#if defined(__cplusplus) && __cplusplus >= 201703L
    template<typename Path,
             typename enable_if<
                 is_same<typename decay<Path>::type, filesystem::path>::value,
                 int>::type = 0>
    void open(const Path& filename, ios_base::openmode mode = ios_base::in) {
        open(filename.c_str(), mode);
    }
#endif

    void close() {
        if (!buf_.close()) {
            this->setstate(ios_base::failbit);
        }
    }

    void swap(basic_ifstream& other) noexcept {
        basic_istream<CharT, Traits>::swap(other);
        buf_.swap(other.buf_);
    }
};

/* 型エイリアス */
using ifstream = basic_ifstream<char>;
using wifstream = basic_ifstream<wchar_t>;

/* ═══════════════════════════════════════════════════════════════
 * basic_ofstream
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename Traits = char_traits<CharT>>
class basic_ofstream : public basic_ostream<CharT, Traits> {
    basic_filebuf<CharT, Traits> buf_;

public:
    using char_type = CharT;
    using traits_type = Traits;

    basic_ofstream() : basic_ostream<CharT, Traits>(&buf_) {}

    explicit basic_ofstream(const char* filename,
                            ios_base::openmode mode = ios_base::out)
        : basic_ostream<CharT, Traits>(&buf_) {
        open(filename, mode);
    }

    explicit basic_ofstream(const string& filename,
                            ios_base::openmode mode = ios_base::out)
        : basic_ofstream(filename.c_str(), mode) {}

#if defined(__cplusplus) && __cplusplus >= 201703L
    template<typename Path,
             typename enable_if<
                 is_same<typename decay<Path>::type, filesystem::path>::value,
                 int>::type = 0>
    explicit basic_ofstream(const Path& filename,
                            ios_base::openmode mode = ios_base::out)
        : basic_ostream<CharT, Traits>(&buf_) {
        open(filename, mode);
    }
#endif

    basic_ofstream(basic_ofstream&& other) noexcept
        : basic_ostream<CharT, Traits>(std::move(other)),
          buf_(std::move(other.buf_)) {
        this->set_rdbuf(&buf_);
    }

    ~basic_ofstream() = default;

    basic_ofstream& operator=(basic_ofstream&& other) noexcept {
        basic_ostream<CharT, Traits>::operator=(std::move(other));
        buf_ = std::move(other.buf_);
        this->set_rdbuf(&buf_);
        return *this;
    }

    /* コピー禁止 */
    basic_ofstream(const basic_ofstream&) = delete;
    basic_ofstream& operator=(const basic_ofstream&) = delete;

    /* ファイル操作 */
    basic_filebuf<CharT, Traits>* rdbuf() const noexcept {
        return const_cast<basic_filebuf<CharT, Traits>*>(&buf_);
    }

    bool is_open() const noexcept { return buf_.is_open(); }

    void open(const char* filename, ios_base::openmode mode = ios_base::out) {
        if (buf_.open(filename, mode | ios_base::out)) {
            this->clear();
        } else {
            this->setstate(ios_base::failbit);
        }
    }

    void open(const string& filename, ios_base::openmode mode = ios_base::out) {
        open(filename.c_str(), mode);
    }

#if defined(__cplusplus) && __cplusplus >= 201703L
    template<typename Path,
             typename enable_if<
                 is_same<typename decay<Path>::type, filesystem::path>::value,
                 int>::type = 0>
    void open(const Path& filename, ios_base::openmode mode = ios_base::out) {
        open(filename.c_str(), mode);
    }
#endif

    void close() {
        if (!buf_.close()) {
            this->setstate(ios_base::failbit);
        }
    }

    void swap(basic_ofstream& other) noexcept {
        basic_ostream<CharT, Traits>::swap(other);
        buf_.swap(other.buf_);
    }
};

/* 型エイリアス */
using ofstream = basic_ofstream<char>;
using wofstream = basic_ofstream<wchar_t>;

/* ═══════════════════════════════════════════════════════════════
 * basic_fstream
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename Traits = char_traits<CharT>>
class basic_fstream : public basic_istream<CharT, Traits>,
                      public basic_ostream<CharT, Traits> {
    basic_filebuf<CharT, Traits> buf_;

public:
    using char_type = CharT;
    using traits_type = Traits;

    basic_fstream() : basic_istream<CharT, Traits>(&buf_),
                      basic_ostream<CharT, Traits>(&buf_) {}

    explicit basic_fstream(const char* filename,
                           ios_base::openmode mode = ios_base::in | ios_base::out)
        : basic_istream<CharT, Traits>(&buf_),
          basic_ostream<CharT, Traits>(&buf_) {
        open(filename, mode);
    }

    explicit basic_fstream(const string& filename,
                           ios_base::openmode mode = ios_base::in | ios_base::out)
        : basic_fstream(filename.c_str(), mode) {}

#if defined(__cplusplus) && __cplusplus >= 201703L
    template<typename Path,
             typename enable_if<
                 is_same<typename decay<Path>::type, filesystem::path>::value,
                 int>::type = 0>
    explicit basic_fstream(const Path& filename,
                           ios_base::openmode mode = ios_base::in | ios_base::out)
        : basic_istream<CharT, Traits>(&buf_),
          basic_ostream<CharT, Traits>(&buf_) {
        open(filename, mode);
    }
#endif

    basic_fstream(basic_fstream&& other) noexcept
        : basic_istream<CharT, Traits>(std::move(other)),
          basic_ostream<CharT, Traits>(std::move(other)),
          buf_(std::move(other.buf_)) {
        this->set_rdbuf(&buf_);
    }

    ~basic_fstream() = default;

    basic_fstream& operator=(basic_fstream&& other) noexcept {
        basic_istream<CharT, Traits>::operator=(std::move(other));
        buf_ = std::move(other.buf_);
        basic_istream<CharT, Traits>::set_rdbuf(&buf_);
        return *this;
    }

    /* コピー禁止 */
    basic_fstream(const basic_fstream&) = delete;
    basic_fstream& operator=(const basic_fstream&) = delete;

    /* ファイル操作 */
    basic_filebuf<CharT, Traits>* rdbuf() const noexcept {
        return const_cast<basic_filebuf<CharT, Traits>*>(&buf_);
    }

    bool is_open() const noexcept { return buf_.is_open(); }

    void open(const char* filename,
              ios_base::openmode mode = ios_base::in | ios_base::out) {
        if (buf_.open(filename, mode)) {
            this->clear();
        } else {
            this->setstate(ios_base::failbit);
        }
    }

    void open(const string& filename,
              ios_base::openmode mode = ios_base::in | ios_base::out) {
        open(filename.c_str(), mode);
    }

#if defined(__cplusplus) && __cplusplus >= 201703L
    template<typename Path,
             typename enable_if<
                 is_same<typename decay<Path>::type, filesystem::path>::value,
                 int>::type = 0>
    void open(const Path& filename,
              ios_base::openmode mode = ios_base::in | ios_base::out) {
        open(filename.c_str(), mode);
    }
#endif

    void close() {
        if (!buf_.close()) {
            this->setstate(ios_base::failbit);
        }
    }

    void swap(basic_fstream& other) noexcept {
        /* basic_istream and basic_ostream virtually share one basic_ios
         * state.  Swap it exactly once; calling both base helpers would
         * exchange the same formatting state twice. */
        basic_istream<CharT, Traits>::swap(other);
        buf_.swap(other.buf_);
        basic_istream<CharT, Traits>::set_rdbuf(&buf_);
        other.basic_istream<CharT, Traits>::set_rdbuf(&other.buf_);
    }
};

/* 型エイリアス */
using fstream = basic_fstream<char>;
using wfstream = basic_fstream<wchar_t>;
template<typename CharT, typename Traits>
void swap(basic_filebuf<CharT, Traits>& lhs,
          basic_filebuf<CharT, Traits>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

template<typename CharT, typename Traits>
void swap(basic_ifstream<CharT, Traits>& lhs,
          basic_ifstream<CharT, Traits>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

template<typename CharT, typename Traits>
void swap(basic_ofstream<CharT, Traits>& lhs,
          basic_ofstream<CharT, Traits>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

template<typename CharT, typename Traits>
void swap(basic_fstream<CharT, Traits>& lhs,
          basic_fstream<CharT, Traits>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

} /* namespace std */

#endif /* RINCXX_FSTREAM_H */
