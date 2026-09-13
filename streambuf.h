/*
 * RinOS C++ <streambuf> ✿
 * ストリームバッファ
 */

#ifndef RINCXX_STREAMBUF_H
#define RINCXX_STREAMBUF_H

#include "rincxx.h"
#include "ios.h"

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * basic_streambuf
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename Traits = char_traits<CharT>>
class basic_streambuf {
public:
    using char_type = CharT;
    using traits_type = Traits;
    using int_type = typename Traits::int_type;
    using pos_type = typename Traits::pos_type;
    using off_type = typename Traits::off_type;

protected:
    /* The conversion facet belongs to the stream buffer, not to a concrete
     * file stream.  Keeping the locale here makes `pubimbue()` observable by
     * every derived buffer and gives filebuf a stable state owner for
     * stateful codecvt implementations. */
    locale locale_;

    /* 入力バッファポインタ */
    char_type* eback_;  /* 開始 */
    char_type* gptr_;   /* 現在 */
    char_type* egptr_;  /* 終了 */

    /* 出力バッファポインタ */
    char_type* pbase_;  /* 開始 */
    char_type* pptr_;   /* 現在 */
    char_type* epptr_;  /* 終了 */

public:
    basic_streambuf()
        : locale_(), eback_(nullptr), gptr_(nullptr), egptr_(nullptr),
          pbase_(nullptr), pptr_(nullptr), epptr_(nullptr) {}

    virtual ~basic_streambuf() = default;

    /* ═══════════════════════════════════════════════════════════
     * 入力操作
     * ═══════════════════════════════════════════════════════════*/

    /* 読み取り可能な文字数 */
    streamsize in_avail() {
        if (gptr_ && egptr_) {
            return egptr_ - gptr_;
        }
        return showmanyc();
    }

    /* 1文字読み取り (バッファから削除) */
    int_type sbumpc() {
        if (gptr_ && gptr_ < egptr_) {
            return traits_type::to_int_type(*gptr_++);
        }
        return uflow();
    }

    /* 1文字読み取り (バッファに残す) */
    int_type sgetc() {
        if (gptr_ && gptr_ < egptr_) {
            return traits_type::to_int_type(*gptr_);
        }
        return underflow();
    }

    /* n文字読み取り */
    streamsize sgetn(char_type* s, streamsize n) {
        if (!s || n <= 0) return 0;
        return xsgetn(s, n);
    }

    /* ═══════════════════════════════════════════════════════════
     * 出力操作
     * ═══════════════════════════════════════════════════════════*/

    /* 1文字書き込み */
    int_type sputc(char_type c) {
        if (pptr_ && pptr_ < epptr_) {
            *pptr_++ = c;
            return traits_type::to_int_type(c);
        }
        return overflow(traits_type::to_int_type(c));
    }

    /* n文字書き込み */
    streamsize sputn(const char_type* s, streamsize n) {
        if (!s || n <= 0) return 0;
        return xsputn(s, n);
    }

    /* ═══════════════════════════════════════════════════════════
     * プットバック
     * ═══════════════════════════════════════════════════════════*/

    int_type sputbackc(char_type c) {
        if (gptr_ && gptr_ > eback_ && traits_type::eq(c, gptr_[-1])) {
            return traits_type::to_int_type(*--gptr_);
        }
        return pbackfail(traits_type::to_int_type(c));
    }

    int_type sungetc() {
        if (gptr_ && gptr_ > eback_) {
            return traits_type::to_int_type(*--gptr_);
        }
        return pbackfail();
    }

    /* ═══════════════════════════════════════════════════════════
     * シーク
     * ═══════════════════════════════════════════════════════════*/

    pos_type pubseekoff(off_type off, ios_base::seekdir way,
                        ios_base::openmode which = ios_base::in | ios_base::out) {
        return seekoff(off, way, which);
    }

    pos_type pubseekpos(pos_type pos,
                        ios_base::openmode which = ios_base::in | ios_base::out) {
        return seekpos(pos, which);
    }

    int pubsync() {
        return sync();
    }

    /* The public setbuf hook is intentionally a thin boundary.  Concrete
     * buffers decide whether the caller-owned storage can be adopted without
     * changing the stream's active state. */
    basic_streambuf* pubsetbuf(char_type* buffer, streamsize count) {
        return setbuf(buffer, count);
    }

    locale pubimbue(const locale& loc) {
        locale old = locale_;
        locale_ = loc;
        imbue(loc);
        return old;
    }

    locale getloc() const { return locale_; }

protected:
    /* ═══════════════════════════════════════════════════════════
     * 入力バッファ管理
     * ═══════════════════════════════════════════════════════════*/

    char_type* eback() const { return eback_; }
    char_type* gptr() const { return gptr_; }
    char_type* egptr() const { return egptr_; }

    void setg(char_type* gbeg, char_type* gnext, char_type* gend) {
        eback_ = gbeg;
        gptr_ = gnext;
        egptr_ = gend;
    }

    void gbump(int n) { gptr_ += n; }

    /* ═══════════════════════════════════════════════════════════
     * 出力バッファ管理
     * ═══════════════════════════════════════════════════════════*/

    char_type* pbase() const { return pbase_; }
    char_type* pptr() const { return pptr_; }
    char_type* epptr() const { return epptr_; }

    void setp(char_type* pbeg, char_type* pend) {
        pbase_ = pbeg;
        pptr_ = pbeg;
        epptr_ = pend;
    }

    void pbump(int n) { pptr_ += n; }

    /* ═══════════════════════════════════════════════════════════
     * 仮想関数 (派生クラスでオーバーライド)
     * ═══════════════════════════════════════════════════════════*/

    virtual streamsize showmanyc() { return 0; }

    virtual int_type underflow() { return traits_type::eof(); }

    virtual int_type uflow() {
        int_type c = underflow();
        if (!traits_type::eq_int_type(c, traits_type::eof())) {
            ++gptr_;
        }
        return c;
    }

    virtual streamsize xsgetn(char_type* s, streamsize n) {
        if (!s || n <= 0) return 0;
        streamsize count = 0;
        while (count < n) {
            if (gptr_ && gptr_ < egptr_) {
                streamsize avail = egptr_ - gptr_;
                streamsize to_copy = (n - count < avail) ? (n - count) : avail;
                traits_type::copy(s + count, gptr_, to_copy);
                gptr_ += to_copy;
                count += to_copy;
            } else {
                int_type c = uflow();
                if (traits_type::eq_int_type(c, traits_type::eof())) {
                    break;
                }
                s[count++] = traits_type::to_char_type(c);
            }
        }
        return count;
    }

    virtual int_type pbackfail(int_type c = Traits::eof()) {
        (void)c;
        return traits_type::eof();
    }

    virtual int_type overflow(int_type c = Traits::eof()) {
        (void)c;
        return traits_type::eof();
    }

    virtual streamsize xsputn(const char_type* s, streamsize n) {
        if (!s || n <= 0) return 0;
        streamsize count = 0;
        while (count < n) {
            if (pptr_ && pptr_ < epptr_) {
                streamsize avail = epptr_ - pptr_;
                streamsize to_copy = (n - count < avail) ? (n - count) : avail;
                traits_type::copy(pptr_, s + count, to_copy);
                pptr_ += to_copy;
                count += to_copy;
            } else {
                int_type result = overflow(traits_type::to_int_type(s[count]));
                if (traits_type::eq_int_type(result, traits_type::eof())) {
                    break;
                }
                ++count;
            }
        }
        return count;
    }

    virtual pos_type seekoff(off_type, ios_base::seekdir,
                             ios_base::openmode = ios_base::in | ios_base::out) {
        return pos_type(off_type(-1));
    }

    virtual pos_type seekpos(pos_type,
                             ios_base::openmode = ios_base::in | ios_base::out) {
        return pos_type(off_type(-1));
    }

    virtual int sync() { return 0; }

    virtual basic_streambuf* setbuf(char_type*, streamsize) { return this; }

    virtual void imbue(const locale&) {}
};

template<typename CharT, typename Traits>
inline void istreambuf_iterator<CharT, Traits>::read_char() {
    if (sbuf_) {
        current_ = sbuf_->sgetc();
        if (Traits::eq_int_type(current_, Traits::eof())) {
            sbuf_ = nullptr;
        }
    }
}

template<typename CharT, typename Traits>
inline istreambuf_iterator<CharT, Traits>&
istreambuf_iterator<CharT, Traits>::operator++() {
    if (sbuf_) {
        sbuf_->sbumpc();
        read_char();
    }
    return *this;
}

template<typename CharT, typename Traits>
inline ostreambuf_iterator<CharT, Traits>&
ostreambuf_iterator<CharT, Traits>::operator=(CharT c) {
    if (!failed_ && sbuf_) {
        if (Traits::eq_int_type(sbuf_->sputc(c), Traits::eof())) {
            failed_ = true;
        }
    }
    return *this;
}

template<typename CharT, typename Traits>
inline locale basic_ios<CharT, Traits>::imbue(const locale& loc) {
    locale old = ios_base::imbue(loc);
    if (rdbuf_ != nullptr) rdbuf_->pubimbue(loc);
    return old;
}

/* 型エイリアス */
using streambuf = basic_streambuf<char>;

} /* namespace std */

#endif /* RINCXX_STREAMBUF_H */
