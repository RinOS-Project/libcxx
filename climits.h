/*
 * RinOS C++ <climits> ✿
 * 整数型限界値
 */

#ifndef RINCXX_CLIMITS_H
#define RINCXX_CLIMITS_H

/* Signed char limits */
#ifndef SCHAR_MIN
#define SCHAR_MIN (-128)
#endif
#ifndef SCHAR_MAX
#define SCHAR_MAX 127
#endif

/* Unsigned char limit */
#ifndef UCHAR_MAX
#define UCHAR_MAX 255
#endif

/* Char limits (assuming signed) */
#ifndef CHAR_MIN
#define CHAR_MIN SCHAR_MIN
#endif
#ifndef CHAR_MAX
#define CHAR_MAX SCHAR_MAX
#endif
#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

/* Signed short limits */
#ifndef SHRT_MIN
#define SHRT_MIN (-32768)
#endif
#ifndef SHRT_MAX
#define SHRT_MAX 32767
#endif

/* Unsigned short limit */
#ifndef USHRT_MAX
#define USHRT_MAX 65535
#endif

/* Signed int limits */
#ifndef INT_MIN
#define INT_MIN (-2147483647 - 1)
#endif
#ifndef INT_MAX
#define INT_MAX 2147483647
#endif

/* Unsigned int limit */
#ifndef UINT_MAX
#define UINT_MAX 4294967295U
#endif

/* Signed long limits follow the target ABI, not its CPU family. */
#if defined(__SIZEOF_LONG__) && __SIZEOF_LONG__ == 8
/* LP64: long is 64-bit. */
#ifndef LONG_MIN
#define LONG_MIN  (-9223372036854775807L - 1L)
#endif
#ifndef LONG_MAX
#define LONG_MAX  9223372036854775807L
#endif
#ifndef ULONG_MAX
#define ULONG_MAX 18446744073709551615UL
#endif
#else
/* ILP32: long is 32-bit */
#ifndef LONG_MIN
#define LONG_MIN  (-2147483647L - 1L)
#endif
#ifndef LONG_MAX
#define LONG_MAX  2147483647L
#endif
#ifndef ULONG_MAX
#define ULONG_MAX 4294967295UL
#endif
#endif

/* Signed long long limits */
#ifndef LLONG_MIN
#define LLONG_MIN (-9223372036854775807LL - 1)
#endif
#ifndef LLONG_MAX
#define LLONG_MAX 9223372036854775807LL
#endif

/* Unsigned long long limit */
#ifndef ULLONG_MAX
#define ULLONG_MAX 18446744073709551615ULL
#endif

/* Multi-byte character max */
#ifndef MB_LEN_MAX
#define MB_LEN_MAX 4
#endif

#endif /* RINCXX_CLIMITS_H */
