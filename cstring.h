/*
 * RinOS C++ <cstring> ✿
 * 文字列操作関数
 */

#ifndef RINCXX_CSTRING_H
#define RINCXX_CSTRING_H

#include "rincxx.h"
#include "version.h"

/* Include the RinOS C string API rather than the sibling C++ string.h. */
#include "../libc/string.h"

#ifdef __cplusplus

/* ═══════════════════════════════════════════════════════════════
 * 文字列操作 - std名前空間版
 * C関数をstd名前空間に持ち込む
 * ═══════════════════════════════════════════════════════════════*/

namespace std {

/* Import C functions into std namespace */
using ::memcpy;
using ::memmove;
using ::memset;
using ::memcmp;
using ::memchr;
using ::strlen;
using ::strcpy;
using ::strncpy;
using ::strcat;
using ::strncat;
using ::strcmp;
using ::strncmp;
using ::strchr;
using ::strrchr;
using ::strstr;
using ::strspn;
using ::strcspn;
using ::strpbrk;

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_CSTRING_H */
