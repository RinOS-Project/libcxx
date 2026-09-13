/*
 * RinOS C++ <source_location> ✿
 * ソース位置情報 (C++20)
 */

#ifndef RINCXX_SOURCE_LOCATION_H
#define RINCXX_SOURCE_LOCATION_H

#include "rincxx.h"
#include "cstdint.h"
#include "version.h"

#if defined(__cplusplus) && __cplusplus >= 202002L
#ifndef RIN_LIBCXX_HAS_SOURCE_LOCATION_BACKEND
#error "RinOS source_location requires a supported call-site builtin backend"
#endif
namespace std {

/* ═══════════════════════════════════════════════════════════════
 * source_location
 * コンパイル時にソースコードの位置情報を取得
 * ═══════════════════════════════════════════════════════════════*/

class source_location {
    const char* file_;
    const char* func_;
    uint_least32_t line_;
    uint_least32_t column_;

    constexpr source_location(const char* file, const char* func,
                              uint_least32_t line, uint_least32_t column) noexcept
        : file_(file), func_(func), line_(line), column_(column) {}

public:
    /* デフォルトコンストラクタ */
    constexpr source_location() noexcept
        : file_(""), func_(""), line_(0), column_(0) {}

    /* current() - 呼び出し位置の情報を取得 */
/* GCC 11--13 has no column builtin.  Its standard zero sentinel is carried
 * by the backend capability selected in <version>, never synthesized here. */
#define RIN_SOURCE_LOCATION_HAS_COLUMN RIN_LIBCXX_SOURCE_LOCATION_HAS_COLUMN

    static constexpr source_location current(
        const char* file = __builtin_FILE(),
        const char* func = __builtin_FUNCTION(),
        uint_least32_t line = __builtin_LINE(),
#if RIN_SOURCE_LOCATION_HAS_COLUMN
        uint_least32_t column = __builtin_COLUMN()
#else
        uint_least32_t column = 0
#endif
    ) noexcept {
        return source_location(file, func, line, column);
    }

    /* アクセサ */
    constexpr uint_least32_t line() const noexcept {
        return line_;
    }

    constexpr uint_least32_t column() const noexcept {
        return column_;
    }

    constexpr const char* file_name() const noexcept {
        return file_;
    }

    constexpr const char* function_name() const noexcept {
        return func_;
    }
};

} /* namespace std */

/* ═══════════════════════════════════════════════════════════════
 * 便利マクロ (非標準)
 * ═══════════════════════════════════════════════════════════════*/

/* Keep the compiler-owned default arguments so this also captures columns. */
#define RIN_SOURCE_LOCATION() ::std::source_location::current()

#endif /* C++20 */
#endif /* RINCXX_SOURCE_LOCATION_H */
