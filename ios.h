/*
 * RinOS C++ <ios> ✿
 * I/O基底クラス
 */

#ifndef RINCXX_IOS_H
#define RINCXX_IOS_H

#include "rincxx.h"
#include "system_error.h"
#include "string.h"
#include "iosfwd.h"  /* char_traits, streamoff, streamsize */
#include "locale.h"

namespace std {

/* char_traits is now defined in iosfwd.h to avoid instantiation issues */

/* ═══════════════════════════════════════════════════════════════
 * ios_base
 * ═══════════════════════════════════════════════════════════════*/

class ios_base {
public:
    /* フォーマットフラグ */
    using fmtflags = unsigned int;
    static constexpr fmtflags boolalpha   = 0x0001;
    static constexpr fmtflags dec         = 0x0002;
    static constexpr fmtflags fixed       = 0x0004;
    static constexpr fmtflags hex         = 0x0008;
    static constexpr fmtflags internal    = 0x0010;
    static constexpr fmtflags left        = 0x0020;
    static constexpr fmtflags oct         = 0x0040;
    static constexpr fmtflags right       = 0x0080;
    static constexpr fmtflags scientific  = 0x0100;
    static constexpr fmtflags showbase    = 0x0200;
    static constexpr fmtflags showpoint   = 0x0400;
    static constexpr fmtflags showpos     = 0x0800;
    static constexpr fmtflags skipws      = 0x1000;
    static constexpr fmtflags unitbuf     = 0x2000;
    static constexpr fmtflags uppercase   = 0x4000;

    static constexpr fmtflags adjustfield = left | right | internal;
    static constexpr fmtflags basefield   = dec | oct | hex;
    static constexpr fmtflags floatfield  = scientific | fixed;

    /* 状態フラグ */
    using iostate = unsigned int;
    static constexpr iostate goodbit = 0x0;
    static constexpr iostate badbit  = 0x1;
    static constexpr iostate eofbit  = 0x2;
    static constexpr iostate failbit = 0x4;

    /* Keep the exception's error_code observable to callers.  In particular,
     * state-mask failures are in the standard iostream domain rather than the
     * generic runtime_error domain. */
    class failure : public system_error {
    public:
        explicit failure(const string& message,
                         const error_code& code =
                             make_error_code(io_errc::stream))
            : system_error(code, message) {}

        explicit failure(const char* message,
                         const error_code& code =
                             make_error_code(io_errc::stream))
            : system_error(code, message) {}
    };

    /* シーク方向 */
    using seekdir = int;
    static constexpr seekdir beg = 0;
    static constexpr seekdir cur = 1;
    static constexpr seekdir end = 2;

    /* オープンモード */
    using openmode = unsigned int;
    static constexpr openmode app    = 0x01;
    static constexpr openmode ate    = 0x02;
    static constexpr openmode binary = 0x04;
    static constexpr openmode in     = 0x08;
    static constexpr openmode out    = 0x10;
    static constexpr openmode trunc  = 0x20;

protected:
    fmtflags flags_ = dec | skipws;
    streamsize width_ = 0;
    streamsize precision_ = 6;
    iostate state_ = goodbit;
    iostate exceptions_ = goodbit;
    locale locale_{};

    void throw_if_masked() {
        if ((state_ & exceptions_) == 0u) return;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        throw failure("ios_base state matches exception mask");
#else
        __builtin_trap();
#endif
    }

public:
    ios_base() = default;
    virtual ~ios_base() = default;

    /* フォーマットフラグ */
    fmtflags flags() const { return flags_; }
    fmtflags flags(fmtflags f) { fmtflags old = flags_; flags_ = f; return old; }
    fmtflags setf(fmtflags f) { fmtflags old = flags_; flags_ |= f; return old; }
    fmtflags setf(fmtflags f, fmtflags mask) {
        fmtflags old = flags_;
        flags_ = (flags_ & ~mask) | (f & mask);
        return old;
    }
    void unsetf(fmtflags f) { flags_ &= ~f; }

    /* 幅と精度 */
    streamsize width() const { return width_; }
    streamsize width(streamsize w) { streamsize old = width_; width_ = w; return old; }
    streamsize precision() const { return precision_; }
    streamsize precision(streamsize p) { streamsize old = precision_; precision_ = p; return old; }

    /* ロケール */
    locale imbue(const locale& loc) {
        locale old = locale_;
        locale_ = loc;
        return old;
    }
    locale getloc() const { return locale_; }

    /* 状態テスト */
    bool good() const { return state_ == goodbit; }
    bool eof() const { return (state_ & eofbit) != 0; }
    bool fail() const { return (state_ & (failbit | badbit)) != 0; }
    bool bad() const { return (state_ & badbit) != 0; }

    /* operator bool - ストリームが正常かどうか */
    explicit operator bool() const { return !fail(); }
    bool operator!() const { return fail(); }

    /* 状態操作 */
    iostate rdstate() const { return state_; }
    void clear(iostate state = goodbit) {
        state_ = state;
        throw_if_masked();
    }
    void setstate(iostate state) { clear(state_ | state); }

    iostate exceptions() const noexcept { return exceptions_; }
    void exceptions(iostate mask) {
        exceptions_ = mask;
        throw_if_masked();
    }

    /* Stream callbacks are special: [iostream] requires their original
     * exception to escape when badbit is masked, while still recording the
     * stream state. The caller invokes this from its catch block and rethrows
     * when the return value is true. Other exception masks keep the ordinary
     * setstate() failure path. */
    bool __setstate_from_callback(iostate state) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        if ((state & badbit) != 0u && (exceptions_ & badbit) != 0u) {
            state_ |= state;
            return true;
        }
#endif
        setstate(state);
        return false;
    }
};

/* C++11/14 constexpr static data members need an out-of-class definition
 * when a caller odr-uses a flag (for example, by taking `&ios_base::hex`).
 * Since C++17 the in-class declarations are implicitly inline variables. */
#if __cplusplus < 201703L
#if defined(_MSC_VER)
#define RIN_CXX11_CONSTEXPR_STATIC_DEF __declspec(selectany)
#elif defined(__GNUC__) || defined(__clang__)
/* Header-only C++11/14 consumers need a definition in every translation unit,
 * while the linker must coalesce those definitions just like C++17 inline
 * variables.  `selectany` provides that COMDAT-like owner on the COFF/MinGW
 * linker used by host regressions; other GCC/Clang targets use weak linkage. */
#if defined(__MINGW32__) || defined(_WIN32)
#define RIN_CXX11_CONSTEXPR_STATIC_DEF __attribute__((selectany))
#else
#define RIN_CXX11_CONSTEXPR_STATIC_DEF __attribute__((weak))
#endif
#else
#define RIN_CXX11_CONSTEXPR_STATIC_DEF
#endif
constexpr ios_base::fmtflags ios_base::boolalpha RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::dec RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::fixed RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::hex RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::internal RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::left RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::oct RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::right RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::scientific RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::showbase RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::showpoint RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::showpos RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::skipws RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::unitbuf RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::uppercase RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::adjustfield RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::basefield RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::fmtflags ios_base::floatfield RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::iostate ios_base::goodbit RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::iostate ios_base::badbit RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::iostate ios_base::eofbit RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::iostate ios_base::failbit RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::seekdir ios_base::beg RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::seekdir ios_base::cur RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::seekdir ios_base::end RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::openmode ios_base::app RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::openmode ios_base::ate RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::openmode ios_base::binary RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::openmode ios_base::in RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::openmode ios_base::out RIN_CXX11_CONSTEXPR_STATIC_DEF;
constexpr ios_base::openmode ios_base::trunc RIN_CXX11_CONSTEXPR_STATIC_DEF;
#undef RIN_CXX11_CONSTEXPR_STATIC_DEF
#endif

inline unsigned int __locale_stream_flags(const ios_base& stream) noexcept {
    return stream.flags();
}

inline long __locale_stream_width(const ios_base& stream) noexcept {
    return static_cast<long>(stream.width());
}

inline long __locale_stream_precision(const ios_base& stream) noexcept {
    return static_cast<long>(stream.precision());
}

inline void __locale_stream_width_reset(ios_base& stream) noexcept {
    stream.width(0);
}

inline void __locale_stream_fail(ios_base& stream) noexcept {
    stream.setstate(ios_base::failbit);
}

inline locale __locale_stream_locale(const ios_base& stream) {
    return stream.getloc();
}

inline unsigned int __locale_eofbit() noexcept { return ios_base::eofbit; }
inline unsigned int __locale_failbit() noexcept { return ios_base::failbit; }

/* ═══════════════════════════════════════════════════════════════
 * basic_ios
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename Traits = char_traits<CharT>>
class basic_ios : public ios_base {
public:
    using char_type = CharT;
    using traits_type = Traits;
    using int_type = typename Traits::int_type;
    using pos_type = typename Traits::pos_type;
    using off_type = typename Traits::off_type;

protected:
    basic_streambuf<CharT, Traits>* rdbuf_;
    basic_ostream<CharT, Traits>* tie_;
    char_type fill_;

public:
    explicit basic_ios(basic_streambuf<CharT, Traits>* sb)
        : rdbuf_(sb), tie_(nullptr), fill_(' ') {}

    virtual ~basic_ios() = default;

    /* 状態 */
    bool good() const { return state_ == goodbit; }
    bool eof() const { return state_ & eofbit; }
    bool fail() const { return state_ & (failbit | badbit); }
    bool bad() const { return state_ & badbit; }

    bool operator!() const { return fail(); }
    explicit operator bool() const { return !fail(); }

    iostate rdstate() const { return state_; }
    void clear(iostate state = goodbit) {
        /* [basic.ios.members] keeps a stream with no associated buffer in
         * bad state.  This matters after rdbuf(nullptr): clearing the old
         * error bits must not make an unusable stream look good. */
        if (rdbuf_ == nullptr) state |= badbit;
        ios_base::clear(state);
    }
    void setstate(iostate state) { clear(state_ | state); }

    /* streambuf */
    basic_streambuf<CharT, Traits>* rdbuf() const { return rdbuf_; }
    basic_streambuf<CharT, Traits>* rdbuf(basic_streambuf<CharT, Traits>* sb) {
        basic_streambuf<CharT, Traits>* old = rdbuf_;
        rdbuf_ = sb;
        clear();
        return old;
    }

    /* `basic_ios::imbue` has to update both the formatting locale and the
     * associated stream buffer.  The definition lives in streambuf.h, after
     * basic_streambuf is complete, so this header remains safe to include on
     * its own. */
    locale imbue(const locale& loc);

    /* fill */
    char_type fill() const { return fill_; }
    char_type fill(char_type c) { char_type old = fill_; fill_ = c; return old; }

    /* tied output stream */
    basic_ostream<CharT, Traits>* tie() const { return tie_; }
    basic_ostream<CharT, Traits>* tie(basic_ostream<CharT, Traits>* stream) {
        basic_ostream<CharT, Traits>* old = tie_;
        tie_ = stream;
        return old;
    }

    /* copyfmt - 別のストリームからフォーマット設定をコピー */
    basic_ios& copyfmt(const basic_ios& rhs) {
        if (this != &rhs) {
            flags_ = rhs.flags_;
            width_ = rhs.width_;
            precision_ = rhs.precision_;
            locale_ = rhs.locale_;
            tie_ = rhs.tie_;
            fill_ = rhs.fill_;
            exceptions_ = rhs.exceptions_;
            /* 状態はコピーしない（標準準拠） */
            throw_if_masked();
        }
        return *this;
    }

protected:
    basic_ios() : rdbuf_(nullptr), tie_(nullptr), fill_(' ') {}

    void init(basic_streambuf<CharT, Traits>* sb) {
        rdbuf_ = sb;
        tie_ = nullptr;
        fill_ = ' ';
        state_ = sb ? goodbit : badbit;
    }

    void set_rdbuf(basic_streambuf<CharT, Traits>* sb) noexcept {
        rdbuf_ = sb;
    }

    void swap(basic_ios& rhs) noexcept {
        using std::swap;
        swap(flags_, rhs.flags_);
        swap(width_, rhs.width_);
        swap(precision_, rhs.precision_);
        swap(state_, rhs.state_);
        swap(exceptions_, rhs.exceptions_);
        swap(locale_, rhs.locale_);
        swap(tie_, rhs.tie_);
        swap(fill_, rhs.fill_);
        /* The owning derived stream keeps its own stream buffer object. */
    }
};

/* 型エイリアス */
using ios = basic_ios<char>;

/* ═══════════════════════════════════════════════════════════════
 * I/Oマニピュレータ
 * ═══════════════════════════════════════════════════════════════*/

/* 基数マニピュレータ */
inline ios_base& dec(ios_base& os) {
    os.setf(ios_base::dec, ios_base::basefield);
    return os;
}

inline ios_base& hex(ios_base& os) {
    os.setf(ios_base::hex, ios_base::basefield);
    return os;
}

inline ios_base& oct(ios_base& os) {
    os.setf(ios_base::oct, ios_base::basefield);
    return os;
}

/* 表示フラグマニピュレータ */
inline ios_base& boolalpha(ios_base& os) {
    os.setf(ios_base::boolalpha);
    return os;
}

inline ios_base& noboolalpha(ios_base& os) {
    os.unsetf(ios_base::boolalpha);
    return os;
}

inline ios_base& showbase(ios_base& os) {
    os.setf(ios_base::showbase);
    return os;
}

inline ios_base& noshowbase(ios_base& os) {
    os.unsetf(ios_base::showbase);
    return os;
}

inline ios_base& showpos(ios_base& os) {
    os.setf(ios_base::showpos);
    return os;
}

inline ios_base& noshowpos(ios_base& os) {
    os.unsetf(ios_base::showpos);
    return os;
}

inline ios_base& showpoint(ios_base& os) {
    os.setf(ios_base::showpoint);
    return os;
}

inline ios_base& noshowpoint(ios_base& os) {
    os.unsetf(ios_base::showpoint);
    return os;
}

inline ios_base& skipws(ios_base& os) {
    os.setf(ios_base::skipws);
    return os;
}

inline ios_base& noskipws(ios_base& os) {
    os.unsetf(ios_base::skipws);
    return os;
}

inline ios_base& uppercase(ios_base& os) {
    os.setf(ios_base::uppercase);
    return os;
}

inline ios_base& nouppercase(ios_base& os) {
    os.unsetf(ios_base::uppercase);
    return os;
}

inline ios_base& unitbuf(ios_base& os) {
    os.setf(ios_base::unitbuf);
    return os;
}

inline ios_base& nounitbuf(ios_base& os) {
    os.unsetf(ios_base::unitbuf);
    return os;
}

/* 配置マニピュレータ */
inline ios_base& left(ios_base& os) {
    os.setf(ios_base::left, ios_base::adjustfield);
    return os;
}

inline ios_base& right(ios_base& os) {
    os.setf(ios_base::right, ios_base::adjustfield);
    return os;
}

inline ios_base& internal(ios_base& os) {
    os.setf(ios_base::internal, ios_base::adjustfield);
    return os;
}

/* 浮動小数点マニピュレータ */
inline ios_base& fixed(ios_base& os) {
    os.setf(ios_base::fixed, ios_base::floatfield);
    return os;
}

inline ios_base& scientific(ios_base& os) {
    os.setf(ios_base::scientific, ios_base::floatfield);
    return os;
}

inline ios_base& hexfloat(ios_base& os) {
    os.setf(ios_base::fixed | ios_base::scientific,
            ios_base::floatfield);
    return os;
}

inline ios_base& defaultfloat(ios_base& os) {
    os.unsetf(ios_base::floatfield);
    return os;
}

} /* namespace std */

#endif /* RINCXX_IOS_H */
