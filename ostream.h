/*
 * RinOS C++ <ostream> ✿
 * 出力ストリーム
 */

#ifndef RINCXX_OSTREAM_H
#define RINCXX_OSTREAM_H

#include "rincxx.h"
#include "ios.h"
#include "streambuf.h"
#include "string.h"
#include "cstdio.h"
#if __cplusplus >= 201703L
#include "string_view.h"
#include "charconv.h"
#endif
#include "cstdint.h"

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * basic_ostream
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename Traits = char_traits<CharT>>
class basic_ostream : virtual public basic_ios<CharT, Traits> {
public:
    using char_type = CharT;
    using traits_type = Traits;
    using int_type = typename Traits::int_type;
    using pos_type = typename Traits::pos_type;
    using off_type = typename Traits::off_type;

    class sentry {
        basic_ostream& stream_;
        bool ok_;

    public:
        explicit sentry(basic_ostream& stream) : stream_(stream), ok_(false) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            try {
#endif
            if (!stream.good()) return;
            basic_ostream<CharT, Traits>* tied = stream.tie();
            if (tied && tied != &stream) tied->flush();
            ok_ = stream.good();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            } catch (...) {
                if (stream.__setstate_from_callback(ios_base::badbit)) throw;
            }
#endif
        }

        ~sentry() {
            if (!ok_ || !(stream_.flags() & ios_base::unitbuf)) return;

            /* A formatted operation can leave this scope while an exception
             * is already unwinding (for example, a locale facet callback).
             * A unitbuf flush must not run then: a second callback failure
             * would mask the primary exception or terminate the process. */
            if (uncaught_exceptions() != 0) return;
            stream_.flush();
        }

        explicit operator bool() const noexcept { return ok_; }
    };

    explicit basic_ostream(basic_streambuf<CharT, Traits>* sb) {
        this->init(sb);
    }

    virtual ~basic_ostream() = default;

    /* NOTE: 標準の std::basic_ostream には str() メソッドは存在しません。
     * (ostringstream{} << x).str() パターンが必要な場合は、
     * ostringstream を明示的に使用してください。
     *
     * 非標準の virtual str() は Abseil 等のライブラリと衝突するため削除しました。
     */

    /* ═══════════════════════════════════════════════════════════
     * 書式なし出力
     * ═══════════════════════════════════════════════════════════*/

    basic_ostream& put(char_type c) {
        sentry stream_sentry(*this);
        if (!stream_sentry) return *this;
        put_direct(c);
        return *this;
    }

    basic_ostream& write(const char_type* s, streamsize n) {
        sentry stream_sentry(*this);
        if (!stream_sentry) return *this;
        write_direct(s, n);
        return *this;
    }

    basic_ostream& flush() {
        sentry stream_sentry(*this);
        if (!stream_sentry) return *this;
        flush_direct();
        return *this;
    }

    /* ═══════════════════════════════════════════════════════════
     * シーク
     * ═══════════════════════════════════════════════════════════*/

    pos_type tellp() {
        if (this->fail() || !this->rdbuf()) return pos_type(-1);
        pos_type position;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            position = this->rdbuf()->pubseekoff(0, ios_base::cur, ios_base::out);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return pos_type(-1);
        }
#endif
        return position;
    }

    basic_ostream& seekp(pos_type pos) {
        if (!this->fail()) {
            bool failed = !this->rdbuf();
            if (!failed) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                try {
#endif
                    failed = this->rdbuf()->pubseekpos(pos, ios_base::out) ==
                             pos_type(off_type(-1));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                } catch (...) {
                    if (this->__setstate_from_callback(ios_base::badbit)) throw;
                    return *this;
                }
#endif
            }
            if (failed) {
                this->setstate(ios_base::failbit);
            }
        }
        return *this;
    }

    basic_ostream& seekp(off_type off, ios_base::seekdir dir) {
        if (!this->fail()) {
            bool failed = !this->rdbuf();
            if (!failed) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                try {
#endif
                    failed = this->rdbuf()->pubseekoff(off, dir, ios_base::out) ==
                             pos_type(off_type(-1));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                } catch (...) {
                    if (this->__setstate_from_callback(ios_base::badbit)) throw;
                    return *this;
                }
#endif
            }
            if (failed) {
                this->setstate(ios_base::failbit);
            }
        }
        return *this;
    }

    /* ═══════════════════════════════════════════════════════════
     * 書式付き出力 (operator<<)
     * ═══════════════════════════════════════════════════════════*/

    /* bool */
    basic_ostream& operator<<(bool val) {
        return output_num_put(val);
    }

    /* 整数 */
    basic_ostream& operator<<(short val) {
        return output_num_put(static_cast<long>(val));
    }
    basic_ostream& operator<<(unsigned short val) {
        return output_num_put(static_cast<unsigned long>(val));
    }
    basic_ostream& operator<<(int val) {
        return output_num_put(static_cast<long>(val));
    }
    basic_ostream& operator<<(unsigned int val) {
        return output_num_put(static_cast<unsigned long>(val));
    }
    basic_ostream& operator<<(long val) { return output_num_put(val); }
    basic_ostream& operator<<(unsigned long val) {
        return output_num_put(val);
    }
    basic_ostream& operator<<(long long val) {
        return output_num_put(val);
    }
    basic_ostream& operator<<(unsigned long long val) {
        return output_num_put(val);
    }

    /* 浮動小数点 */
    basic_ostream& operator<<(float val) { return output_num_put(val); }
    basic_ostream& operator<<(double val) { return output_num_put(val); }
    basic_ostream& operator<<(long double val) {
        return output_num_put(val);
    }

    /* ポインタ */
    basic_ostream& operator<<(const void* val) {
        return output_num_put(val);
    }

    /* 文字 */
    basic_ostream& operator<<(char c) {
        const CharT value = static_cast<CharT>(c);
        return output_field(&value, 1u);
    }

    template<typename C = CharT>
    typename enable_if<!is_same<C, char>::value, basic_ostream&>::type
    operator<<(CharT c) {
        return output_field(&c, 1u);
    }

    basic_ostream& operator<<(signed char c) {
        const CharT value = static_cast<CharT>(c);
        return output_field(&value, 1u);
    }

    basic_ostream& operator<<(unsigned char c) {
        const CharT value = static_cast<CharT>(c);
        return output_field(&value, 1u);
    }

    /* C文字列 */
    basic_ostream& operator<<(const CharT* s) {
        if (!s) return output_ascii("(null)");
        const CharT* end = s;
        while (*end) ++end;
        return output_field(s, static_cast<size_t>(end - s));
    }

    template<typename C = CharT>
    typename enable_if<!is_same<C, char>::value, basic_ostream&>::type
    operator<<(const char* s) {
        return output_ascii(s ? s : "(null)");
    }

    basic_ostream& operator<<(const signed char* s) {
        return output_ascii(s ? reinterpret_cast<const char*>(s) : "(null)");
    }

    basic_ostream& operator<<(const unsigned char* s) {
        return output_ascii(s ? reinterpret_cast<const char*>(s) : "(null)");
    }

    /* std::string */
    basic_ostream& operator<<(const basic_string<CharT>& s) {
        return output_field(s.data(), s.size());
    }

    /* The standard string inserter accepts every basic_string whose
     * character type matches the stream, including custom traits and
     * allocator instances.  The old non-template overload only admitted the
     * default std::string alias and silently routed other strings to the
     * generic inserter (which is intentionally unavailable in freestanding
     * mode). */
    template<typename StringTraits, typename Allocator>
    basic_ostream& operator<<(
        const basic_string<CharT, StringTraits, Allocator>& s) {
        return output_field(s.data(), s.size());
    }

    /* string_view */
#if __cplusplus >= 201703L
    template<typename ViewTraits>
    basic_ostream& operator<<(basic_string_view<CharT, ViewTraits> sv) {
        return output_field(sv.data(), sv.size());
    }
#endif

    /* streambuf */
    basic_ostream& operator<<(basic_streambuf<CharT, Traits>* sb) {
        sentry stream_sentry(*this);
        if (!stream_sentry) return *this;
        if (!sb) {
            this->setstate(ios_base::failbit);
            return *this;
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
        int_type c;
        bool transferred = false;
        while ((c = sb->sbumpc()) != Traits::eof()) {
            put_direct(Traits::to_char_type(c));
            if (this->bad()) break;
            transferred = true;
        }
        if (!transferred && this->good()) this->setstate(ios_base::failbit);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
        }
#endif
        return *this;
    }

    /* マニピュレータ */
    basic_ostream& operator<<(basic_ostream& (*manip)(basic_ostream&)) {
        return manip(*this);
    }

    basic_ostream& operator<<(basic_ios<CharT, Traits>& (*manip)(basic_ios<CharT, Traits>&)) {
        manip(*this);
        return *this;
    }

    basic_ostream& operator<<(ios_base& (*manip)(ios_base&)) {
        manip(*this);
        return *this;
    }

private:
    /* Sink operations invoked by an already active formatted/unformatted
     * operation must not construct another sentry: doing so would flush a
     * tied stream once per character.  The public members above provide the
     * standard one-sentry boundary and delegate to these raw callbacks. */
    bool put_direct(char_type c) {
        if (!this->rdbuf()) {
            this->setstate(ios_base::badbit);
            return false;
        }
        bool failed = false;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            failed = this->rdbuf()->sputc(c) == Traits::eof();
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return false;
        }
#else
        failed = this->rdbuf()->sputc(c) == Traits::eof();
#endif
        if (failed) this->setstate(ios_base::badbit);
        return !failed;
    }

    bool write_direct(const char_type* s, streamsize n) {
        if (!this->rdbuf()) {
            this->setstate(ios_base::badbit);
            return false;
        }
        bool failed = false;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            failed = this->rdbuf()->sputn(s, n) != n;
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return false;
        }
#else
        failed = this->rdbuf()->sputn(s, n) != n;
#endif
        if (failed) this->setstate(ios_base::badbit);
        return !failed;
    }

    bool flush_direct() {
        if (!this->rdbuf()) {
            this->setstate(ios_base::badbit);
            return false;
        }
        bool failed = false;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            failed = this->rdbuf()->pubsync() == -1;
        } catch (...) {
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
            return false;
        }
#else
        failed = this->rdbuf()->pubsync() == -1;
#endif
        if (failed) this->setstate(ios_base::badbit);
        return !failed;
    }

    template<typename T>
    basic_ostream& output_num_put(T value) {
        sentry stream_sentry(*this);
        if (!stream_sentry) {
            this->width(0);
            return *this;
        }
        using iterator = ostreambuf_iterator<CharT, Traits>;
        iterator output(this->rdbuf());
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            const locale current_locale = this->getloc();
            const auto& facet = use_facet<num_put<CharT, iterator>>(
                current_locale);
            iterator result = facet.put(output, *this, this->fill(), value);
            if (result.failed()) this->setstate(ios_base::badbit);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            /* num_put writes through ostreambuf_iterator.  A streambuf
             * callback exception must be recorded as badbit, and the
             * original exception is rethrown only when badbit is masked.
             * Formatted insertion still consumes width on this path. */
            this->width(0);
            if (this->__setstate_from_callback(ios_base::badbit)) throw;
        }
#endif
        return *this;
    }

    /* Apply the standard width/fill/alignment contract to character fields.
     * Numeric formatters call output_ascii() and therefore share this owner;
     * keeping the transaction here also ensures width is reset when the
     * destination reports a short write. */
    basic_ostream& output_field(const CharT* data, size_t count) {
        const streamsize requested_width = this->width(0);
        sentry stream_sentry(*this);
        if (!stream_sentry) return *this;

        size_t padding = 0u;
        if (requested_width > 0 &&
            static_cast<unsigned long long>(requested_width) > count) {
            padding = static_cast<size_t>(requested_width) - count;
        }
        const bool left = (this->flags() & ios_base::adjustfield) ==
                          ios_base::left;
        const auto write_fill = [&](size_t amount) {
            for (size_t index = 0u; index < amount; ++index) {
                put_direct(this->fill());
                if (this->bad()) break;
            }
        };
        const auto write_data = [&]() {
            if (data && count != 0u)
                write_direct(data, static_cast<streamsize>(count));
        };
        if (!left) write_fill(padding);
        if (this->good()) write_data();
        if (left && this->good()) write_fill(padding);
        return *this;
    }

    basic_ostream& output_ascii(const char* text) {
        if (!text) text = "(null)";
        const char* end = text;
        while (*end) ++end;
        const streamsize requested_width = this->width(0);
        sentry stream_sentry(*this);
        if (!stream_sentry) return *this;
        const size_t count = static_cast<size_t>(end - text);
        size_t padding = requested_width > 0 &&
                         static_cast<unsigned long long>(requested_width) > count
            ? static_cast<size_t>(requested_width) - count : 0u;
        const bool left = (this->flags() & ios_base::adjustfield) ==
                          ios_base::left;
        const auto write_fill = [&](size_t amount) {
            for (size_t index = 0u; index < amount; ++index) {
                put_direct(this->fill());
                if (this->bad()) break;
            }
        };
        if (!left) write_fill(padding);
        if (this->good()) {
            for (const char* cursor = text; cursor != end && this->good();
                 ++cursor)
                put_direct(static_cast<CharT>(*cursor));
        }
        if (left && this->good()) write_fill(padding);
        return *this;
    }

    template<typename T>
    basic_ostream& output_int(T val) {
        char buf[32];
        char* p = buf + sizeof(buf) - 1;
        *p = '\0';

        bool neg = false;
        unsigned long long magnitude;
        if (val < 0) {
            neg = true;
            magnitude = 0u - static_cast<unsigned long long>(val);
        } else {
            magnitude = static_cast<unsigned long long>(val);
        }

        int base = 10;
        if (this->flags() & ios_base::hex) base = 16;
        else if (this->flags() & ios_base::oct) base = 8;

        const char* digits = (this->flags() & ios_base::uppercase)
            ? "0123456789ABCDEF" : "0123456789abcdef";

        do {
            *--p = digits[magnitude % static_cast<unsigned>(base)];
            magnitude /= static_cast<unsigned>(base);
        } while (magnitude);

        if (this->flags() & ios_base::showbase) {
            if (base == 16) { *--p = 'x'; *--p = '0'; }
            else if (base == 8 && *p != '0') { *--p = '0'; }
        }

        if (neg) *--p = '-';
        else if (this->flags() & ios_base::showpos) *--p = '+';

        return output_ascii(p);
    }

    template<typename T>
    basic_ostream& output_uint(T val) {
        char buf[32];
        char* p = buf + sizeof(buf) - 1;
        *p = '\0';

        int base = 10;
        if (this->flags() & ios_base::hex) base = 16;
        else if (this->flags() & ios_base::oct) base = 8;

        const char* digits = (this->flags() & ios_base::uppercase)
            ? "0123456789ABCDEF" : "0123456789abcdef";

        do {
            *--p = digits[val % base];
            val /= base;
        } while (val);

        if (this->flags() & ios_base::showbase) {
            if (base == 16) { *--p = 'x'; *--p = '0'; }
            else if (base == 8 && *p != '0') { *--p = '0'; }
        }

        if (this->flags() & ios_base::showpos) *--p = '+';

        return output_ascii(p);
    }

#if __cplusplus >= 201703L
    template<typename FloatType>
    basic_ostream& output_float(FloatType val) {
        basic_string<char> raw;
        basic_string<char> formatted;
        const ios_base::fmtflags float_flags =
            this->flags() & ios_base::floatfield;
        chars_format format = chars_format::general;
        bool hexadecimal = false;
        if (float_flags == ios_base::fixed) {
            format = chars_format::fixed;
        } else if (float_flags == ios_base::scientific) {
            format = chars_format::scientific;
        } else if (float_flags == ios_base::floatfield) {
            format = chars_format::hex;
            hexadecimal = true;
        }

        const streamsize requested_precision = this->precision();
        if (requested_precision >
            static_cast<streamsize>(numeric_limits<int>::max())) {
            this->width(0);
            this->setstate(ios_base::failbit);
            return *this;
        }
        int precision = requested_precision < 0
                      ? 6 : static_cast<int>(requested_precision);
        if (format == chars_format::general && precision == 0) precision = 1;

        constexpr size_t minimum_raw_capacity = 64u;
        constexpr size_t raw_precision_slack = 64u;
        if (static_cast<size_t>(precision) >
            raw.max_size() - raw_precision_slack) {
            this->width(0);
            this->setstate(ios_base::failbit);
            return *this;
        }
        size_t raw_capacity = hexadecimal ? minimum_raw_capacity
            : static_cast<size_t>(precision) + raw_precision_slack;
        if (raw_capacity < minimum_raw_capacity) {
            raw_capacity = minimum_raw_capacity;
        }

        to_chars_result conversion{};
        for (;;) {
            raw.resize(raw_capacity);
            if (raw.size() != raw_capacity) {
                this->width(0);
                this->setstate(ios_base::failbit);
                return *this;
            }
            if (hexadecimal) {
                conversion = to_chars(raw.data(), raw.data() + raw.size(),
                                      val, format);
            } else {
                conversion = to_chars(raw.data(), raw.data() + raw.size(),
                                      val, format, precision);
            }
            if (conversion.ec == errc{}) break;
            if (conversion.ec != errc::value_too_large ||
                raw_capacity > raw.max_size() / 2u) {
                this->width(0);
                this->setstate(ios_base::failbit);
                return *this;
            }
            raw_capacity *= 2u;
        }

        const size_t raw_size = static_cast<size_t>(conversion.ptr - raw.data());
        raw.resize(raw_size);
        size_t input_offset = 0u;
        size_t output_size = 0u;
        const auto append_character = [&](char character) {
            const size_t previous_size = formatted.size();
            formatted.push_back(character);
            return formatted.size() == previous_size + 1u;
        };
        const auto append_range = [&](const char* first, size_t count) {
            const size_t previous_size = formatted.size();
            if (count > formatted.max_size() - previous_size) return false;
            formatted.append(first, count);
            return formatted.size() == previous_size + count;
        };
        if (raw_size != 0u && raw[0] == '-') {
            if (!append_character('-')) {
                this->width(0);
                this->setstate(ios_base::failbit);
                return *this;
            }
            ++output_size;
            input_offset = 1u;
        } else if ((this->flags() & ios_base::showpos) != 0u) {
            if (!append_character('+')) {
                this->width(0);
                this->setstate(ios_base::failbit);
                return *this;
            }
            ++output_size;
        }
        const bool raw_special = input_offset < raw_size &&
            (raw[input_offset] == 'i' || raw[input_offset] == 'n');
        const bool hexadecimal_prefix = hexadecimal && !raw_special;
        if (hexadecimal_prefix) {
            if (!append_character('0') || !append_character('x')) {
                this->width(0);
                this->setstate(ios_base::failbit);
                return *this;
            }
            output_size += 2u;
        }
        if (!append_range(raw.data() + input_offset,
                          raw_size - input_offset)) {
            this->width(0);
            this->setstate(ios_base::failbit);
            return *this;
        }
        output_size = formatted.size();

        size_t mantissa_begin = 0u;
        if (output_size != 0u &&
            (formatted[0] == '-' || formatted[0] == '+')) {
            mantissa_begin = 1u;
        }
        if (hexadecimal_prefix) mantissa_begin += 2u;
        const bool special = mantissa_begin < output_size &&
            (formatted[mantissa_begin] == 'i' ||
             formatted[mantissa_begin] == 'n');

        const auto insert_characters = [&](size_t position, size_t count,
                                           char character) -> bool {
            if (count > formatted.max_size() - output_size) return false;
            const size_t previous_size = formatted.size();
            formatted.insert(position, count, character);
            if (formatted.size() != previous_size + count) return false;
            output_size = formatted.size();
            return true;
        };

        if (!special && (this->flags() & ios_base::showpoint) != 0u) {
            size_t exponent = output_size;
            size_t point = output_size;
            for (size_t index = mantissa_begin; index < output_size; ++index) {
                if (formatted[index] == '.') point = index;
                if (formatted[index] == 'e' || formatted[index] == 'E' ||
                    formatted[index] == 'p' || formatted[index] == 'P') {
                    exponent = index;
                    break;
                }
            }
            if (point == output_size) {
                if (!insert_characters(exponent, 1u, '.')) {
                    this->width(0);
                    this->setstate(ios_base::failbit);
                    return *this;
                }
                point = exponent;
                ++exponent;
            }

            if (format == chars_format::general) {
                size_t significant_digits = 0u;
                bool found_nonzero = false;
                for (size_t index = mantissa_begin; index < exponent; ++index) {
                    const char character = formatted[index];
                    if (character < '0' || character > '9') continue;
                    if (character != '0') found_nonzero = true;
                    if (found_nonzero) ++significant_digits;
                }
                if (!found_nonzero) significant_digits = 1u;
                const size_t desired = static_cast<size_t>(precision);
                if (significant_digits < desired &&
                    !insert_characters(exponent, desired - significant_digits,
                                       '0')) {
                    this->width(0);
                    this->setstate(ios_base::failbit);
                    return *this;
                }
            }
        }

        /* The numeric conversion owner emits the classic '.' radix.  Apply
         * the active numpunct radix and decimal grouping before width/padding
         * is calculated so the published field follows the stream locale.
         * The local formatter stores one-byte candidates; a non-ASCII wchar
         * radix/separator is rejected rather than silently narrowing it. */
        if (!special && !hexadecimal) {
            const numpunct<char_type>& punctuation =
                use_facet<numpunct<char_type>>(this->getloc());
            const char_type decimal = punctuation.decimal_point();
            const char_type separator = punctuation.thousands_sep();
            const unsigned int decimal_value =
                static_cast<unsigned int>(decimal);
            const unsigned int separator_value =
                static_cast<unsigned int>(separator);
            if (decimal_value > 0x7fu || separator_value > 0x7fu) {
                this->width(0);
                this->setstate(ios_base::failbit);
                return *this;
            }

            size_t exponent = output_size;
            size_t point = output_size;
            for (size_t index = mantissa_begin; index < output_size; ++index) {
                if (formatted[index] == '.') point = index;
                if (formatted[index] == 'e' || formatted[index] == 'E') {
                    exponent = index;
                    break;
                }
            }
            const size_t integer_end = point < exponent ? point : exponent;
            if (decimal != static_cast<char_type>('.')) {
                for (size_t index = mantissa_begin; index < output_size;
                     ++index) {
                    if (formatted[index] == '.') {
                        formatted[index] = static_cast<char>(decimal_value);
                    }
                }
            }
            const basic_string<char> grouping = punctuation.grouping();
            if (!grouping.empty() && integer_end > mantissa_begin) {
                basic_string<char> marks;
                marks.assign(output_size, '\0');
                if (marks.size() != output_size) {
                    this->width(0);
                    this->setstate(ios_base::failbit);
                    return *this;
                }
                size_t cursor = integer_end;
                size_t pattern_index = 0u;
                unsigned int previous_group = 0u;
                size_t boundary_count = 0u;
                while (cursor > mantissa_begin && pattern_index <
                       grouping.size()) {
                    const unsigned int encoded = static_cast<unsigned char>(
                        grouping[pattern_index]);
                    if (encoded == static_cast<unsigned int>(CHAR_MAX)) break;
                    const unsigned int group = encoded == 0u
                        ? previous_group : encoded;
                    if (group == 0u || cursor - mantissa_begin <= group) break;
                    cursor -= group;
                    marks[cursor] = 1;
                    ++boundary_count;
                    if (encoded != 0u) previous_group = encoded;
                    ++pattern_index;
                    if (pattern_index == grouping.size() &&
                        previous_group != 0u &&
                        grouping[grouping.size() - 1u] !=
                            static_cast<char>(CHAR_MAX)) {
                        pattern_index = grouping.size() - 1u;
                    }
                }
                if (boundary_count != 0u) {
                    if (boundary_count > formatted.max_size() - output_size) {
                        this->width(0);
                        this->setstate(ios_base::failbit);
                        return *this;
                    }
                    basic_string<char> grouped;
                    grouped.reserve(output_size + boundary_count);
                    if (grouped.capacity() < output_size + boundary_count) {
                        this->width(0);
                        this->setstate(ios_base::failbit);
                        return *this;
                    }
                    for (size_t index = 0u; index < output_size; ++index) {
                        if (marks[index] != '\0') grouped.push_back(
                            static_cast<char>(separator_value));
                        grouped.push_back(formatted[index]);
                    }
                    if (grouped.size() != output_size + boundary_count) {
                        this->width(0);
                        this->setstate(ios_base::failbit);
                        return *this;
                    }
                    formatted.swap(grouped);
                    output_size = formatted.size();
                }
            }
        }

        if ((this->flags() & ios_base::uppercase) != 0u) {
            for (size_t index = 0u; index < output_size; ++index) {
                if (formatted[index] >= 'a' && formatted[index] <= 'z') {
                    formatted[index] = static_cast<char>(
                        formatted[index] - 'a' + 'A');
                }
            }
        }

        const streamsize requested_width = this->width(0);
        const size_t padding = requested_width > 0 &&
            static_cast<unsigned long long>(requested_width) > output_size
            ? static_cast<size_t>(requested_width -
                                  static_cast<streamsize>(output_size))
            : 0u;
        const ios_base::fmtflags adjustment =
            this->flags() & ios_base::adjustfield;
        const auto write_fill = [&](size_t count) {
            for (size_t index = 0u; index < count; ++index)
                put_direct(this->fill());
        };
        const auto write_range = [&](size_t begin, size_t end) {
            for (size_t index = begin; index < end; ++index) {
                put_direct(static_cast<CharT>(formatted[index]));
            }
        };

        if (adjustment == ios_base::left) {
            write_range(0u, output_size);
            write_fill(padding);
        } else if (adjustment == ios_base::internal && padding != 0u) {
            size_t prefix = 0u;
            if (output_size != 0u &&
                (formatted[0] == '-' || formatted[0] == '+')) {
                prefix = 1u;
            }
            if (hexadecimal_prefix) prefix += 2u;
            write_range(0u, prefix);
            write_fill(padding);
            write_range(prefix, output_size);
        } else {
            write_fill(padding);
            write_range(0u, output_size);
        }
        return *this;
    }
#else
    /* C++11/14 uses the allocation-free C formatter as its conversion
     * backend.  Formatting is performed into a bounded scratch buffer before
     * any character reaches the stream, so a short conversion cannot publish
     * a partial value. */
    template<typename FloatType>
    basic_ostream& output_float(FloatType value) {
        char format[16];
        size_t format_size = 0u;
        const ios_base::fmtflags float_flags =
            this->flags() & ios_base::floatfield;
        const bool uppercase = (this->flags() & ios_base::uppercase) != 0u;
        const char conversion = float_flags == ios_base::fixed ? 'f'
            : float_flags == ios_base::scientific ? 'e'
            : float_flags == ios_base::floatfield ? 'a' : 'g';
        format[format_size++] = '%';
        if ((this->flags() & ios_base::showpos) != 0u)
            format[format_size++] = '+';
        if ((this->flags() & ios_base::showpoint) != 0u)
            format[format_size++] = '#';
        format[format_size++] = '.';
        format[format_size++] = '*';
        format[format_size++] = 'L';
        format[format_size++] = uppercase
            ? static_cast<char>(conversion - 'a' + 'A') : conversion;
        format[format_size] = '\0';

        streamsize requested_precision = this->precision();
        if (requested_precision < 0) requested_precision = 6;
        if (requested_precision > 16382) {
            this->width(0);
            this->setstate(ios_base::failbit);
            return *this;
        }
        char rendered[16384];
        size_t output_size = 0u;
        bool conversion_ok = true;
        if (conversion == 'a') {
            const long double original = static_cast<long double>(value);
            const bool negative = original < 0.0L ||
                (original == 0.0L && (1.0L / original) < 0.0L);
            long double magnitude = negative ? -original : original;
            const bool special_nan = magnitude != magnitude;
            const long double infinity =
                numeric_limits<long double>::infinity();
            const bool special_infinity = magnitude == infinity;
            const auto append = [&](char character) {
                if (output_size + 1u >= sizeof(rendered)) {
                    conversion_ok = false;
                    return;
                }
                rendered[output_size++] = character;
            };
            if (negative) append('-');
            else if ((this->flags() & ios_base::showpos) != 0u) append('+');
            if (special_nan || special_infinity) {
                const char* text = special_nan ? "nan" : "inf";
                for (const char* cursor = text; *cursor != '\0'; ++cursor)
                    append(uppercase ? static_cast<char>(*cursor - 'a' + 'A')
                                     : *cursor);
            } else if (magnitude == 0.0L) {
                append('0');
                append('x');
                append('0');
                append('p');
                append('+');
                append('0');
            } else {
                int exponent = 0;
                while (magnitude >= 2.0L && exponent < 20000) {
                    magnitude *= 0.5L;
                    ++exponent;
                }
                while (magnitude < 1.0L && exponent > -20000) {
                    magnitude *= 2.0L;
                    --exponent;
                }
                append('0');
                append(uppercase ? 'X' : 'x');
                append('1');
                char fraction[16384];
                size_t fraction_size = 0u;
                long double fractional = magnitude - 1.0L;
                for (int index = 0; index < static_cast<int>(requested_precision);
                     ++index) {
                    fractional *= 16.0L;
                    int digit = static_cast<int>(fractional);
                    if (digit < 0) digit = 0;
                    if (digit > 15) digit = 15;
                    fraction[fraction_size++] = digit < 10
                        ? static_cast<char>('0' + digit)
                        : static_cast<char>((uppercase ? 'A' : 'a') +
                                            (digit - 10));
                    fractional -= static_cast<long double>(digit);
                }
                while (fraction_size != 0u &&
                       fraction[fraction_size - 1u] == '0') {
                    --fraction_size;
                }
                if (fraction_size != 0u ||
                    (this->flags() & ios_base::showpoint) != 0u) {
                    append('.');
                    for (size_t index = 0u; index < fraction_size; ++index)
                        append(fraction[index]);
                }
                append(uppercase ? 'P' : 'p');
                append(exponent < 0 ? '-' : '+');
                unsigned int magnitude_exponent = static_cast<unsigned int>(
                    exponent < 0 ? -exponent : exponent);
                char exponent_digits[16];
                size_t exponent_size = 0u;
                do {
                    exponent_digits[exponent_size++] = static_cast<char>(
                        '0' + magnitude_exponent % 10u);
                    magnitude_exponent /= 10u;
                } while (magnitude_exponent != 0u);
                while (exponent_size != 0u)
                    append(exponent_digits[--exponent_size]);
            }
        } else {
            const int written = ::snprintf(
                rendered, sizeof(rendered), format,
                static_cast<int>(requested_precision),
                static_cast<long double>(value));
            if (written < 0 || static_cast<size_t>(written) >= sizeof(rendered)) {
                conversion_ok = false;
            } else {
                output_size = static_cast<size_t>(written);
            }
        }
        if (!conversion_ok) {
            this->width(0);
            this->setstate(ios_base::failbit);
            return *this;
        }

        if (conversion != 'a') {
            const numpunct<char_type>& punctuation =
                use_facet<numpunct<char_type>>(this->getloc());
            const char_type decimal_point = punctuation.decimal_point();
            const char_type thousands_separator = punctuation.thousands_sep();
            const unsigned int decimal_value =
                static_cast<unsigned int>(decimal_point);
            const unsigned int separator_value =
                static_cast<unsigned int>(thousands_separator);
            if (decimal_value > 0x7fu || separator_value > 0x7fu) {
                this->width(0);
                this->setstate(ios_base::failbit);
                return *this;
            }
            size_t exponent = output_size;
            size_t point = output_size;
            for (size_t index = 0u; index < output_size; ++index) {
                if (rendered[index] == '.') point = index;
                if (rendered[index] == 'e' || rendered[index] == 'E') {
                    exponent = index;
                    break;
                }
            }
            const size_t mantissa_begin = output_size != 0u &&
                (rendered[0] == '-' || rendered[0] == '+') ? 1u : 0u;
            const size_t integer_end = point < exponent ? point : exponent;
            if (decimal_point != static_cast<char_type>('.')) {
                for (size_t index = 0u; index < output_size; ++index) {
                    if (rendered[index] == '.') {
                        rendered[index] = static_cast<char>(decimal_value);
                    }
                }
            }
            const basic_string<char> grouping = punctuation.grouping();
            if (!grouping.empty() && integer_end > mantissa_begin) {
                static constexpr size_t maximum_group_count = 512u;
                size_t boundaries[maximum_group_count];
                size_t boundary_count = 0u;
                size_t cursor = integer_end;
                size_t pattern_index = 0u;
                unsigned int previous_group = 0u;
                while (cursor > mantissa_begin && pattern_index <
                       grouping.size()) {
                    const unsigned int encoded = static_cast<unsigned char>(
                        grouping[pattern_index]);
                    if (encoded == static_cast<unsigned int>(CHAR_MAX)) break;
                    const unsigned int group = encoded == 0u
                        ? previous_group : encoded;
                    if (group == 0u || cursor - mantissa_begin <= group) break;
                    cursor -= group;
                    if (boundary_count == maximum_group_count) {
                        this->width(0);
                        this->setstate(ios_base::failbit);
                        return *this;
                    }
                    boundaries[boundary_count++] = cursor;
                    if (encoded != 0u) previous_group = encoded;
                    ++pattern_index;
                    if (pattern_index == grouping.size() &&
                        previous_group != 0u &&
                        grouping[grouping.size() - 1u] !=
                            static_cast<char>(CHAR_MAX)) {
                        pattern_index = grouping.size() - 1u;
                    }
                }
                for (size_t boundary_index = 0u;
                     boundary_index < boundary_count; ++boundary_index) {
                    const size_t position = boundaries[boundary_index];
                    if (output_size >= sizeof(rendered) - 1u) {
                        this->width(0);
                        this->setstate(ios_base::failbit);
                        return *this;
                    }
                    for (size_t index = output_size; index > position; --index) {
                        rendered[index] = rendered[index - 1u];
                    }
                    rendered[position] = static_cast<char>(separator_value);
                    ++output_size;
                }
            }
        }

        const streamsize requested_width = this->width(0);
        const size_t padding = requested_width > 0 &&
            static_cast<unsigned long long>(requested_width) > output_size
            ? static_cast<size_t>(requested_width -
                                  static_cast<streamsize>(output_size))
            : 0u;
        const ios_base::fmtflags adjustment =
            this->flags() & ios_base::adjustfield;
        const auto write_fill = [&](size_t count) {
            for (size_t index = 0u; index < count; ++index)
                put_direct(this->fill());
        };
        const auto write_range = [&](size_t begin, size_t end) {
            for (size_t index = begin; index < end; ++index)
                put_direct(static_cast<CharT>(rendered[index]));
        };
        if (adjustment == ios_base::left) {
            write_range(0u, output_size);
            write_fill(padding);
        } else if (adjustment == ios_base::internal && padding != 0u) {
            size_t prefix = output_size != 0u &&
                (rendered[0] == '-' || rendered[0] == '+') ? 1u : 0u;
            write_range(0u, prefix);
            write_fill(padding);
            write_range(prefix, output_size);
        } else {
            write_fill(padding);
            write_range(0u, output_size);
        }
        return *this;
    }
#endif

protected:
    basic_ostream(basic_ostream&& rhs) noexcept {
        this->copyfmt(rhs);
        this->set_rdbuf(rhs.rdbuf());
        /* Preserve a failed source without invoking its exception mask from
         * this noexcept move operation. */
        this->state_ = rhs.rdstate();
    }

    basic_ostream& operator=(basic_ostream&& rhs) noexcept {
        if (this != &rhs) {
            this->copyfmt(rhs);
            this->set_rdbuf(rhs.rdbuf());
            this->state_ = rhs.rdstate();
        }
        return *this;
    }

    void swap(basic_ostream& rhs) noexcept {
        basic_ios<CharT, Traits>::swap(rhs);
    }

    basic_ostream() { this->init(nullptr); }
};

/* 型エイリアス */
using ostream = basic_ostream<char>;

/* ═══════════════════════════════════════════════════════════════════
 * C++11: rvalue stream insertion
 * (ostringstream{} << x).str() パターンをサポートするため
 * ═══════════════════════════════════════════════════════════════════*/

template<typename Ostream, typename T,
         typename = typename enable_if<
             is_base_of<ios_base, typename remove_reference<Ostream>::type>::value &&
             !is_lvalue_reference<Ostream>::value
         >::type>
Ostream&& operator<<(Ostream&& os, const T& value) {
    os << value;
    return static_cast<Ostream&&>(os);
}

/* const char* 特殊化（テンプレート競合回避） */
template<typename CharT, typename Traits>
basic_ostream<CharT, Traits>&&
operator<<(basic_ostream<CharT, Traits>&& os, const CharT* s) {
    os << s;
    return static_cast<basic_ostream<CharT, Traits>&&>(os);
}

template<typename CharT, typename Traits>
basic_ostream<CharT, Traits>&&
operator<<(basic_ostream<CharT, Traits>&& os, CharT c) {
    os << c;
    return static_cast<basic_ostream<CharT, Traits>&&>(os);
}

} /* namespace std */

#endif /* RINCXX_OSTREAM_H */
