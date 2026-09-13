/*
 * RinOS C++ <iostream> ✿
 * 入出力ストリーム
 */

#ifndef RINCXX_IOSTREAM_H
#define RINCXX_IOSTREAM_H

#include "rincxx.h"
#include "string.h"
#include "ios.h"
#include "streambuf.h"
#include "ostream.h"
#include "istream.h"
#include "cstdlib.h"

/* C stdio */
extern "C" {
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && __STDC_HOSTED__
    /* Keep FILE and the standard streams owned by the host C runtime in a
     * hosted C++ consumer.  Rin's FILE layout is a target ABI and cannot be
     * safely overlaid on a compiler-provided FILE. */
    #include <stdio.h>
#else
    #include "../libc/stdio.h"
#endif
}

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * basic_iostream - combines istream and ostream
 * Both use virtual inheritance from basic_ios, resolving diamond
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename Traits = char_traits<CharT>>
class basic_iostream : public basic_istream<CharT, Traits>,
                       public basic_ostream<CharT, Traits> {
public:
    using char_type = CharT;
    using traits_type = Traits;
    using int_type = typename Traits::int_type;
    using pos_type = typename Traits::pos_type;
    using off_type = typename Traits::off_type;

    explicit basic_iostream(basic_streambuf<CharT, Traits>* sb)
        : basic_ios<CharT, Traits>(),
          basic_istream<CharT, Traits>(),
          basic_ostream<CharT, Traits>() {
        this->init(sb);
    }

    virtual ~basic_iostream() = default;

    basic_iostream(basic_iostream&& other) = default;
    basic_iostream& operator=(basic_iostream&& other) = default;

protected:
    basic_iostream()
        : basic_ios<CharT, Traits>(),
          basic_istream<CharT, Traits>(),
          basic_ostream<CharT, Traits>() {}
};

using iostream = basic_iostream<char>;
using wiostream = basic_iostream<wchar_t>;

/* ═══════════════════════════════════════════════════════════════
 * 標準ストリームオブジェクト
 * ═══════════════════════════════════════════════════════════════*/

/* cin - 標準入力 */
extern istream cin;

/* cout - 標準出力 */
extern ostream cout;

/* cerr - 標準エラー (バッファリングなし) */
extern ostream cerr;

/* clog - 標準ログ (バッファリングあり) */
extern ostream clog;

/* ═══════════════════════════════════════════════════════════════
 * 標準ストリームの初期化
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

/* コンソール用streambuf */
class console_streambuf : public streambuf {
    int fd_;
    bool is_input_;
    char input_buffer_;

public:
    console_streambuf(int fd, bool is_input)
        : fd_(fd), is_input_(is_input), input_buffer_(0) {}

protected:
    int_type overflow(int_type c) override {
        if (c != traits_type::eof()) {
            char ch = traits_type::to_char_type(c);
            if (write(fd_, &ch, 1) != 1) {
                return traits_type::eof();
            }
        }
        return c;
    }

    streamsize xsputn(const char_type* s, streamsize n) override {
        return write(fd_, s, n);
    }

    int_type underflow() override {
        if (!is_input_) return traits_type::eof();

        if (read(fd_, &input_buffer_, 1) == 1) {
            /* The get area must outlive underflow(); it also provides the
             * one-character putback buffer required by streambuf. */
            setg(&input_buffer_, &input_buffer_, &input_buffer_ + 1);
            return traits_type::to_int_type(input_buffer_);
        }
        return traits_type::eof();
    }

    int_type uflow() override {
        int_type c = underflow();
        if (c != traits_type::eof()) {
            gbump(1);
        }
        return c;
    }

private:
    static long write(int fd, const void* buf, size_t count) {
        return fwrite(buf, 1, count, fd == 1 ? stdout : stderr);
    }

    static long read(int fd, void* buf, size_t count) {
        (void)fd;
        return fread(buf, 1, count, stdin);
    }
};

/* 静的ストリームバッファ */
#if __cplusplus >= 201703L
inline console_streambuf cin_buf(0, true);
inline console_streambuf cout_buf(1, false);
inline console_streambuf cerr_buf(2, false);
inline console_streambuf clog_buf(2, false);
#endif

} /* namespace detail */

/* 標準ストリームの定義 */
#if __cplusplus >= 201703L
inline istream cin(&detail::cin_buf);
inline ostream cout(&detail::cout_buf);
inline ostream cerr(&detail::cerr_buf);
inline ostream clog(&detail::clog_buf);
#endif

/* ═══════════════════════════════════════════════════════════════
 * endl, ends, flush マニピュレータ
 * ═══════════════════════════════════════════════════════════════*/

inline ostream& endl(ostream& os) {
    os.put('\n');
    os.flush();
    return os;
}

inline ostream& ends(ostream& os) {
    os.put('\0');
    return os;
}

inline ostream& flush(ostream& os) {
    os.flush();
    return os;
}

} /* namespace std */

#endif /* RINCXX_IOSTREAM_H */
