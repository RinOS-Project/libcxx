/*
 * RinOS C++ <iomanip>
 * I/O manipulators
 */

#ifndef RINCXX_IOMANIP_H
#define RINCXX_IOMANIP_H

#include "ios.h"
#include "ostream.h"
#include "istream.h"
#include "locale.h"
#include "iterator.h"
#include "ctime.h"

#ifdef __cplusplus

namespace std {

/* ===================================================================
 * Manipulator helpers
 * ===================================================================*/

struct _Setw { int width; };

struct _Setprecision { int prec; };

template<typename CharT>
struct _Setfill { CharT ch; };

struct _Setbase { int base; };

/* ===================================================================
 * Manipulator creators
 * ===================================================================*/

inline _Setw setw(int n) {
    return _Setw{n};
}

inline _Setprecision setprecision(int n) {
    return _Setprecision{n};
}

template<typename CharT>
inline _Setfill<CharT> setfill(CharT c) {
    return _Setfill<CharT>{c};
}

inline _Setbase setbase(int base) {
    return _Setbase{base};
}

/* ===================================================================
 * Manipulator operators
 * ===================================================================*/

template<typename CharT, typename Traits>
inline basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& os, _Setw w) {
    os.width(w.width);
    return os;
}

template<typename CharT, typename Traits>
inline basic_istream<CharT, Traits>&
operator>>(basic_istream<CharT, Traits>& is, _Setw w) {
    is.width(w.width);
    return is;
}

template<typename CharT, typename Traits>
inline basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& os, _Setprecision p) {
    os.precision(p.prec);
    return os;
}

template<typename CharT, typename Traits>
inline basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& os, _Setfill<CharT> f) {
    os.fill(f.ch);
    return os;
}

template<typename CharT, typename Traits>
inline basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& os, _Setbase b) {
    switch (b.base) {
        case 8:
            os.setf(ios_base::oct, ios_base::basefield);
            break;
        case 16:
            os.setf(ios_base::hex, ios_base::basefield);
            break;
        default:
            os.unsetf(ios_base::basefield);
            break;
    }
    return os;
}

template<typename CharT, typename Traits>
inline basic_istream<CharT, Traits>&
operator>>(basic_istream<CharT, Traits>& is, _Setbase b) {
    switch (b.base) {
        case 8:
            is.setf(ios_base::oct, ios_base::basefield);
            break;
        case 10:
            is.setf(ios_base::dec, ios_base::basefield);
            break;
        case 16:
            is.setf(ios_base::hex, ios_base::basefield);
            break;
        default:
            is.unsetf(ios_base::basefield);
            break;
    }
    return is;
}

/* ===================================================================
 * Quoted strings (C++14)
 * ===================================================================*/

#if __cplusplus >= 201402L
template<typename CharT>
struct _Quoted {
    const CharT* str;
    CharT delim;
    CharT escape;
};

template<typename CharT>
struct _Quoted_string {
    basic_string<CharT>& str;
    CharT delim;
    CharT escape;
};

namespace __iomanip_detail {

/* Output still uses a bounded C-string scan so a malformed/non-terminated
 * producer cannot make the stream operation walk unbounded memory.  Input is
 * different: the destination basic_string already owns its allocator and
 * max_size(), so quoted_append can grow transactionally up to that owner
 * limit instead of imposing an unrelated 4 KiB ceiling. */
static constexpr size_t quoted_output_max_units = 65536u;

template<typename CharT>
inline bool quoted_measure(const CharT* value, CharT delim, CharT escape,
                           size_t& input_length,
                           size_t& output_length) noexcept {
    input_length = 0u;
    output_length = 0u;
    if (value == nullptr) return false;
    size_t escapes = 0u;
    while (value[input_length] != CharT()) {
        if (input_length == quoted_output_max_units) return false;
        if (value[input_length] == delim || value[input_length] == escape)
            ++escapes;
        ++input_length;
    }
    output_length = input_length + escapes + 2u;
    return true;
}

template<typename CharT, typename Traits>
inline basic_ostream<CharT, Traits>& quoted_output(
    basic_ostream<CharT, Traits>& output, const CharT* value,
    CharT delim, CharT escape) {
    if (output.fail()) return output;
    size_t input_length = 0u;
    size_t output_length = 0u;
    if (!quoted_measure(value, delim, escape, input_length, output_length)) {
        output.width(0);
        output.setstate(ios_base::failbit);
        return output;
    }
    streamsize requested = output.width(0);
    size_t padding = requested > 0 &&
            static_cast<size_t>(requested) > output_length
        ? static_cast<size_t>(requested) - output_length : 0u;
    bool left_adjusted =
        (output.flags() & ios_base::adjustfield) == ios_base::left;
    if (!left_adjusted) {
        while (padding != 0u && !output.fail()) {
            output.put(output.fill());
            --padding;
        }
    }
    output.put(delim);
    for (size_t index = 0u; index < input_length && !output.fail(); ++index) {
        if (value[index] == delim || value[index] == escape)
            output.put(escape);
        if (!output.fail()) output.put(value[index]);
    }
    if (!output.fail()) output.put(delim);
    if (left_adjusted) {
        while (padding != 0u && !output.fail()) {
            output.put(output.fill());
            --padding;
        }
    }
    return output;
}

template<typename CharT, typename Traits>
inline bool quoted_append(basic_istream<CharT, Traits>& input,
                          basic_string<CharT>& value, CharT character) {
    if (value.size() >= value.max_size()) {
        input.setstate(ios_base::failbit);
        return false;
    }
    size_t previous_size = value.size();
    value.push_back(character);
    if (value.size() != previous_size + 1u) {
        input.setstate(ios_base::badbit);
        return false;
    }
    return true;
}

template<typename CharT>
inline bool quoted_space(CharT character) noexcept {
    return character == static_cast<CharT>(' ') ||
           character == static_cast<CharT>('\t') ||
           character == static_cast<CharT>('\n') ||
           character == static_cast<CharT>('\r');
}

} /* namespace __iomanip_detail */

template<typename CharT>
inline _Quoted<CharT> quoted(const CharT* s, CharT delim = CharT('"'), CharT escape = CharT('\\')) {
    return _Quoted<CharT>{s, delim, escape};
}

template<typename CharT>
inline _Quoted<CharT> quoted(const basic_string<CharT>& s, CharT delim = CharT('"'), CharT escape = CharT('\\')) {
    return _Quoted<CharT>{s.c_str(), delim, escape};
}

template<typename CharT>
inline _Quoted_string<CharT> quoted(basic_string<CharT>& s,
                                    CharT delim = CharT('"'),
                                    CharT escape = CharT('\\')) {
    return _Quoted_string<CharT>{s, delim, escape};
}

template<typename CharT, typename Traits>
inline basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& os, const _Quoted<CharT>& q) {
    return __iomanip_detail::quoted_output(
        os, q.str, q.delim, q.escape);
}

template<typename CharT, typename Traits>
inline basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& os,
           const _Quoted_string<CharT>& q) {
    return __iomanip_detail::quoted_output(
        os, q.str.c_str(), q.delim, q.escape);
}

template<typename CharT, typename Traits>
inline basic_istream<CharT, Traits>&
operator>>(basic_istream<CharT, Traits>& is,
           const _Quoted_string<CharT>& q) {
    using int_type = typename Traits::int_type;
    if (is.fail()) return is;
    is.skip_ws();
    basic_streambuf<CharT, Traits>* buffer = is.rdbuf();
    if (buffer == nullptr) {
        is.setstate(ios_base::badbit | ios_base::failbit);
        return is;
    }
    int_type current = buffer->sgetc();
    if (Traits::eq_int_type(current, Traits::eof())) {
        is.setstate(ios_base::eofbit | ios_base::failbit);
        return is;
    }

    basic_string<CharT> candidate;
    if (!Traits::eq(Traits::to_char_type(current), q.delim)) {
        for (;;) {
            current = buffer->sgetc();
            if (Traits::eq_int_type(current, Traits::eof())) {
                is.setstate(ios_base::eofbit);
                break;
            }
            CharT character = Traits::to_char_type(current);
            if (__iomanip_detail::quoted_space(character)) break;
            (void)buffer->sbumpc();
            if (!__iomanip_detail::quoted_append(is, candidate, character))
                return is;
        }
        if (candidate.empty()) {
            is.setstate(ios_base::failbit);
            return is;
        }
        q.str.swap(candidate);
        return is;
    }

    (void)buffer->sbumpc();
    for (;;) {
        current = buffer->sgetc();
        if (Traits::eq_int_type(current, Traits::eof())) {
            is.setstate(ios_base::eofbit | ios_base::failbit);
            return is;
        }
        CharT character = Traits::to_char_type(buffer->sbumpc());
        if (character == q.delim) {
            q.str.swap(candidate);
            return is;
        }
        if (character == q.escape) {
            current = buffer->sgetc();
            if (Traits::eq_int_type(current, Traits::eof())) {
                is.setstate(ios_base::eofbit | ios_base::failbit);
                return is;
            }
            character = Traits::to_char_type(buffer->sbumpc());
        }
        if (!__iomanip_detail::quoted_append(is, candidate, character))
            return is;
    }
}
#endif

/* ===================================================================
 * put_money, get_money
 * ===================================================================*/

template<typename MoneyT>
struct _Put_money { MoneyT val; bool intl; };

template<typename MoneyT>
inline _Put_money<MoneyT> put_money(const MoneyT& val, bool intl = false) {
    return _Put_money<MoneyT>{val, intl};
}

template<typename MoneyT>
struct _Get_money { MoneyT& val; bool intl; };

template<typename MoneyT>
inline _Get_money<MoneyT> get_money(MoneyT& val, bool intl = false) {
    return _Get_money<MoneyT>{val, intl};
}

template<typename CharT, typename Traits, typename MoneyT>
inline basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& os, const _Put_money<MoneyT>& money) {
    if (os.fail()) return os;
    using iterator = ostreambuf_iterator<CharT, Traits>;
    using facet = money_put<CharT, iterator>;
    const locale loc = os.getloc();
    const facet& formatter = use_facet<facet>(loc);
    bool show_symbol = (os.flags() & ios_base::showbase) != 0;
    if (!formatter.valid(money.val, money.intl, show_symbol, loc)) {
        os.width(0);
        os.setstate(ios_base::failbit);
        return os;
    }
    iterator output(os.rdbuf());
    output = formatter.put(output, money.intl, os, os.fill(), money.val);
    if (output.failed()) os.setstate(ios_base::badbit);
    return os;
}

template<typename CharT, typename Traits, typename MoneyT>
inline basic_istream<CharT, Traits>&
operator>>(basic_istream<CharT, Traits>& is, const _Get_money<MoneyT>& money) {
    if (is.fail()) return is;
    is.skip_ws();
    using iterator = istreambuf_iterator<CharT, Traits>;
    using facet = money_get<CharT, iterator>;
    unsigned int error = ios_base::goodbit;
    iterator input(is.rdbuf());
    iterator end;
    use_facet<facet>(is.getloc()).get(input, end, money.intl, is, error,
                                      money.val);
    if (error != ios_base::goodbit) is.setstate(error);
    return is;
}

/* ===================================================================
 * put_time, get_time
 * ===================================================================*/

/* tm は <time.h> で定義される。ここでは struct ::tm を直接使用 */
template<typename CharT>
struct _Put_time { const struct ::tm* t; const CharT* fmt; };

template<typename CharT>
inline _Put_time<CharT> put_time(const struct ::tm* t, const CharT* fmt) {
    return _Put_time<CharT>{t, fmt};
}

template<typename CharT>
struct _Get_time { struct ::tm* t; const CharT* fmt; };

template<typename CharT>
inline _Get_time<CharT> get_time(struct ::tm* t, const CharT* fmt) {
    return _Get_time<CharT>{t, fmt};
}

template<typename CharT, typename Traits>
inline basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& os, const _Put_time<CharT>& time) {
    if (os.fail()) return os;
    if (time.t == nullptr || time.fmt == nullptr) {
        os.width(0);
        os.setstate(ios_base::failbit);
        return os;
    }
    const CharT* format_end = time.fmt;
    while (*format_end != CharT()) ++format_end;
    using iterator = ostreambuf_iterator<CharT, Traits>;
    using facet = time_put<CharT, iterator>;
    const locale loc = os.getloc();
    const facet& formatter = use_facet<facet>(loc);
    if (!formatter.valid(time.t, time.fmt, format_end)) {
        os.width(0);
        os.setstate(ios_base::failbit);
        return os;
    }
    iterator output(os.rdbuf());
    output = formatter.put(output, os, os.fill(), time.t, time.fmt,
                           format_end);
    if (output.failed()) os.setstate(ios_base::badbit);
    return os;
}

template<typename CharT, typename Traits>
inline basic_istream<CharT, Traits>&
operator>>(basic_istream<CharT, Traits>& is, const _Get_time<CharT>& time) {
    if (is.fail()) return is;
    is.skip_ws();
    if (time.t == nullptr || time.fmt == nullptr) {
        is.setstate(ios_base::failbit);
        return is;
    }
    const CharT* format_end = time.fmt;
    while (*format_end != CharT()) ++format_end;
    using iterator = istreambuf_iterator<CharT, Traits>;
    using facet = time_get<CharT, iterator>;
    unsigned int error = ios_base::goodbit;
    iterator input(is.rdbuf());
    iterator end;
    use_facet<facet>(is.getloc()).get(input, end, is, error, time.t,
                                      time.fmt, format_end);
    if (error != ios_base::goodbit) is.setstate(error);
    return is;
}

/* ===================================================================
 * resetiosflags, setiosflags
 * ===================================================================*/

struct _Resetiosflags { ios_base::fmtflags mask; };
struct _Setiosflags { ios_base::fmtflags mask; };

inline _Resetiosflags resetiosflags(ios_base::fmtflags mask) {
    return _Resetiosflags{mask};
}

inline _Setiosflags setiosflags(ios_base::fmtflags mask) {
    return _Setiosflags{mask};
}

template<typename CharT, typename Traits>
inline basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& os, _Resetiosflags r) {
    os.unsetf(r.mask);
    return os;
}

template<typename CharT, typename Traits>
inline basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& os, _Setiosflags s) {
    os.setf(s.mask);
    return os;
}

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_IOMANIP_H */
