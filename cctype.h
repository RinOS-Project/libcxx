/*
 * RinOS libcxx - cctype.h ✿
 * 文字分類関数 (ctype.h inline実装を使用)
 */
#ifndef RINCXX_CCTYPE_H
#define RINCXX_CCTYPE_H

/* ctype.hからrin_is*のinline実装を取得 */
#include "../libc/ctype.h"

#ifdef __cplusplus

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * ロケール対応文字分類関数 (カーネルAPIラッパー)
 * ═══════════════════════════════════════════════════════════════*/

inline int isdigit(int c)  { return rin_isdigit(c); }
inline int isalpha(int c)  { return rin_isalpha(c); }
inline int isalnum(int c)  { return rin_isalnum(c); }
inline int isspace(int c)  { return rin_isspace(c); }
inline int isupper(int c)  { return rin_isupper(c); }
inline int islower(int c)  { return rin_islower(c); }
inline int isprint(int c)  { return rin_isprint(c); }
inline int ispunct(int c)  { return rin_ispunct(c); }
inline int isxdigit(int c) { return rin_isxdigit(c); }
inline int iscntrl(int c)  { return rin_iscntrl(c); }
inline int isgraph(int c)  { return rin_isgraph(c); }
inline int isblank(int c)  { return rin_isblank(c); }

inline int toupper(int c) { return rin_toupper(c); }
inline int tolower(int c) { return rin_tolower(c); }

/* C99: isascii/toascii */
inline int isascii(int c) { return (unsigned int)c <= 127; }
inline int toascii(int c) { return c & 0x7F; }

} /* namespace std */

/* ═══════════════════════════════════════════════════════════════
 * C互換 (グローバルスコープ)
 * ═══════════════════════════════════════════════════════════════*/

#ifndef __RIN_NO_CCOMPAT
inline int isdigit(int c)  { return std::isdigit(c); }
inline int isalpha(int c)  { return std::isalpha(c); }
inline int isalnum(int c)  { return std::isalnum(c); }
inline int isspace(int c)  { return std::isspace(c); }
inline int isupper(int c)  { return std::isupper(c); }
inline int islower(int c)  { return std::islower(c); }
inline int isprint(int c)  { return std::isprint(c); }
inline int ispunct(int c)  { return std::ispunct(c); }
inline int isxdigit(int c) { return std::isxdigit(c); }
inline int iscntrl(int c)  { return std::iscntrl(c); }
inline int isgraph(int c)  { return std::isgraph(c); }
inline int isblank(int c)  { return std::isblank(c); }
inline int toupper(int c)  { return std::toupper(c); }
inline int tolower(int c)  { return std::tolower(c); }
inline int isascii(int c)  { return std::isascii(c); }
inline int toascii(int c)  { return std::toascii(c); }
#endif

#endif /* __cplusplus */
#endif /* RINCXX_CCTYPE_H */
