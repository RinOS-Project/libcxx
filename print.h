/*
 * RinOS C++ <print> ✿
 * C++23 formatted output for C streams.
 */

#ifndef RINCXX_PRINT_H
#define RINCXX_PRINT_H

#include "rincxx.h"
#include "version.h"

#if defined(RIN_LIBCXX_CXX23_DIALECT)

#include "cerrno.h"
#include "cstdio.h"
#include "format.h"
#include "system_error.h"
#include "utility.h"

namespace std {

namespace detail {

[[noreturn]] inline void print_output_failed(int error) {
    if (error == 0) error = EIO;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw system_error(error, generic_category(), "std::print");
#else
    (void)error;
    __builtin_trap();
#endif
}

class print_file_lock {
    FILE* stream_;

public:
    explicit print_file_lock(FILE* stream) : stream_(stream) {
        if (!stream_) print_output_failed(EINVAL);
#if defined(_MSC_VER) && !defined(RIN_FREESTANDING)
        _lock_file(stream_);
#else
        ::flockfile(stream_);
#endif
    }

    print_file_lock(const print_file_lock&) = delete;
    print_file_lock& operator=(const print_file_lock&) = delete;

    ~print_file_lock() {
#if defined(_MSC_VER) && !defined(RIN_FREESTANDING)
        _unlock_file(stream_);
#else
        ::funlockfile(stream_);
#endif
    }
};

#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_PRINT)
class print_file_output_iterator {
    FILE* stream_;

public:
    using iterator_category = output_iterator_tag;
    using value_type = void;
    using difference_type = ptrdiff_t;
    using pointer = void;
    using reference = void;

    explicit print_file_output_iterator(FILE* stream) : stream_(stream) {}

    print_file_output_iterator& operator=(char character) {
        errno = 0;
#if defined(_MSC_VER) && !defined(RIN_FREESTANDING)
        if (_putc_nolock(static_cast<unsigned char>(character), stream_) == EOF)
#else
        if (::putc_unlocked(static_cast<unsigned char>(character), stream_) == EOF)
#endif
            print_output_failed(errno);
        return *this;
    }

    print_file_output_iterator& operator*() { return *this; }
    print_file_output_iterator& operator++() { return *this; }
    print_file_output_iterator operator++(int) { return *this; }
};

inline void print_vformat_direct(FILE* stream, string_view format,
                                 format_args args) {
    print_file_lock lock(stream);
    print_file_output_iterator output(stream);
    (void)std::vformat_to(output, format, args);
}

template<class... Args>
inline constexpr bool print_args_nonlocking =
    (enable_nonlocking_formatter_optimization<remove_cvref_t<Args>> && ...);

template<class... Args>
inline void print_format_direct(FILE* stream,
                                format_string<Args...> format,
                                Args&&... args) {
    auto arg_array = make_format_args<format_context>(
        std::forward<Args>(args)...);
    print_vformat_direct(
        stream, format.get(),
        format_args(arg_array.data(), arg_array.size()));
}

template<class... Args>
inline void println_format_direct(FILE* stream,
                                  format_string<Args...> format,
                                  Args&&... args) {
    print_file_lock lock(stream);
    auto arg_array = make_format_args<format_context>(
        std::forward<Args>(args)...);
    print_file_output_iterator output(stream);
    (void)std::vformat_to(
        output, format.get(), format_args(arg_array.data(), arg_array.size()));
    output = '\n';
}
#endif

inline void print_write(FILE* stream, string_view output) {
    print_file_lock lock(stream);
    if (output.empty()) return;
    errno = 0;
    if (::fwrite(output.data(), sizeof(char), output.size(), stream) !=
        output.size())
        print_output_failed(errno);
}

inline void print_vformat(FILE* stream, string_view format,
                          format_args args) {
    auto output = std::vformat(format, args);
    print_write(stream, string_view(output.data(), output.size()));
}

template<class... Args>
inline void print_format(FILE* stream, format_string<Args...> format,
                         Args&&... args) {
    auto output = std::format(format, std::forward<Args>(args)...);
    print_write(stream, string_view(output.data(), output.size()));
}

template<class... Args>
inline void println_format(FILE* stream, format_string<Args...> format,
                           Args&&... args) {
    auto output = std::format(format, std::forward<Args>(args)...);
    output.push_back('\n');
    print_write(stream, string_view(output.data(), output.size()));
}

inline void println_newline(FILE* stream) {
    print_write(stream, string_view("\n", 1u));
}

} /* namespace detail */

inline void vprint_unicode(FILE* stream, string_view format,
                           format_args args) {
    /* The current RinOS stdio backend forwards the formatted UTF-8 bytes. */
#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_PRINT)
    detail::print_vformat_direct(stream, format, args);
#else
    detail::print_vformat(stream, format, args);
#endif
}

inline void vprint_unicode(string_view format, format_args args) {
    vprint_unicode(stdout, format, args);
}

inline void vprint_nonunicode(FILE* stream, string_view format,
                              format_args args) {
#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_PRINT)
    detail::print_vformat_direct(stream, format, args);
#else
    detail::print_vformat(stream, format, args);
#endif
}

inline void vprint_nonunicode(string_view format, format_args args) {
    vprint_nonunicode(stdout, format, args);
}

template<class... Args>
void print(FILE* stream, format_string<Args...> format, Args&&... args) {
#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_PRINT)
    if constexpr (detail::print_args_nonlocking<Args...>) {
        detail::print_format_direct(stream, format,
                                    std::forward<Args>(args)...);
    } else {
        detail::print_format(stream, format, std::forward<Args>(args)...);
    }
#else
    detail::print_format(stream, format, std::forward<Args>(args)...);
#endif
}

template<class... Args>
void print(format_string<Args...> format, Args&&... args) {
    print(stdout, format, std::forward<Args>(args)...);
}

template<class... Args>
void println(FILE* stream, format_string<Args...> format, Args&&... args) {
#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_PRINT)
    if constexpr (detail::print_args_nonlocking<Args...>) {
        detail::println_format_direct(stream, format,
                                      std::forward<Args>(args)...);
    } else {
        detail::println_format(stream, format,
                               std::forward<Args>(args)...);
    }
#else
    detail::println_format(stream, format, std::forward<Args>(args)...);
#endif
}

#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_PRINT)
inline void vprint_unicode_buffered(FILE* stream, string_view format,
                                    format_args args) {
    detail::print_vformat(stream, format, args);
}

inline void vprint_nonunicode_buffered(FILE* stream, string_view format,
                                       format_args args) {
    detail::print_vformat(stream, format, args);
}
#endif

inline void println(FILE* stream) {
    detail::println_newline(stream);
}

template<class... Args>
void println(format_string<Args...> format, Args&&... args) {
    println(stdout, format, std::forward<Args>(args)...);
}

inline void println() { println(stdout); }

} /* namespace std */

#endif /* defined(RIN_LIBCXX_CXX23_DIALECT) */

#endif /* RINCXX_PRINT_H */
