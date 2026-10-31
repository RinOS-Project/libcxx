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

inline void print_write(FILE* stream, string_view output) {
    if (!stream) print_output_failed(EINVAL);
    if (output.empty()) return;

    /* One fwrite keeps each formatted result within one stream-lock interval. */
    errno = 0;
    const size_t written = fwrite(output.data(), 1, output.size(), stream);
    if (written != output.size()) print_output_failed(errno);
}

inline void print_vformat(FILE* stream, string_view format,
                          format_args args) {
    const string output = vformat(format, args);
    print_write(stream, string_view(output.data(), output.size()));
}

} /* namespace detail */

inline void vprint_unicode(FILE* stream, string_view format,
                           format_args args) {
    /* The current RinOS stdio backend forwards the formatted UTF-8 bytes. */
    detail::print_vformat(stream, format, args);
}

inline void vprint_unicode(string_view format, format_args args) {
    vprint_unicode(stdout, format, args);
}

inline void vprint_nonunicode(FILE* stream, string_view format,
                              format_args args) {
    detail::print_vformat(stream, format, args);
}

inline void vprint_nonunicode(string_view format, format_args args) {
    vprint_nonunicode(stdout, format, args);
}

template<class... Args>
void print(FILE* stream, format_string<Args...> format, Args&&... args) {
    const string output = std::format(format, std::forward<Args>(args)...);
    detail::print_write(stream, string_view(output.data(), output.size()));
}

template<class... Args>
void print(format_string<Args...> format, Args&&... args) {
    print(stdout, format, std::forward<Args>(args)...);
}

template<class... Args>
void println(FILE* stream, format_string<Args...> format, Args&&... args) {
    string output = std::format(format, std::forward<Args>(args)...);
    output.push_back('\n');
    detail::print_write(stream, string_view(output.data(), output.size()));
}

inline void println(FILE* stream) {
    detail::print_write(stream, string_view("\n", 1));
}

template<class... Args>
void println(format_string<Args...> format, Args&&... args) {
    println(stdout, format, std::forward<Args>(args)...);
}

inline void println() { println(stdout); }

} /* namespace std */

#endif /* defined(RIN_LIBCXX_CXX23_DIALECT) */

#endif /* RINCXX_PRINT_H */
