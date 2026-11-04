/*
 * Integral std::hash specializations available before functional's higher
 * level dependencies. thread.h needs these when algorithm.h reaches it
 * through an include cycle.
 */
#ifndef RINCXX_HASH_INTEGRAL_H
#define RINCXX_HASH_INTEGRAL_H

#include "cstddef.h"

#ifdef __cplusplus
namespace std {

template<typename T>
struct hash;

template<> struct hash<bool> {
    size_t operator()(bool value) const noexcept {
        return static_cast<size_t>(value);
    }
};
template<> struct hash<char> {
    size_t operator()(char value) const noexcept {
        return static_cast<size_t>(value);
    }
};
template<> struct hash<signed char> {
    size_t operator()(signed char value) const noexcept {
        return static_cast<size_t>(value);
    }
};
template<> struct hash<unsigned char> {
    size_t operator()(unsigned char value) const noexcept {
        return static_cast<size_t>(value);
    }
};
template<> struct hash<wchar_t> {
    size_t operator()(wchar_t value) const noexcept {
        return static_cast<size_t>(value);
    }
};
#if __cplusplus >= 201103L
template<> struct hash<char16_t> {
    size_t operator()(char16_t value) const noexcept {
        return static_cast<size_t>(value);
    }
};
template<> struct hash<char32_t> {
    size_t operator()(char32_t value) const noexcept {
        return static_cast<size_t>(value);
    }
};
#endif
#if defined(__cpp_char8_t)
template<> struct hash<char8_t> {
    size_t operator()(char8_t value) const noexcept {
        return static_cast<size_t>(value);
    }
};
#endif
template<> struct hash<short> {
    size_t operator()(short value) const noexcept {
        return static_cast<size_t>(value);
    }
};
template<> struct hash<unsigned short> {
    size_t operator()(unsigned short value) const noexcept {
        return static_cast<size_t>(value);
    }
};
template<> struct hash<int> {
    size_t operator()(int value) const noexcept {
        return static_cast<size_t>(value);
    }
};
template<> struct hash<unsigned int> {
    size_t operator()(unsigned int value) const noexcept {
        return static_cast<size_t>(value);
    }
};
template<> struct hash<long> {
    size_t operator()(long value) const noexcept {
        return static_cast<size_t>(value);
    }
};
template<> struct hash<unsigned long> {
    size_t operator()(unsigned long value) const noexcept {
        return static_cast<size_t>(value);
    }
};
template<> struct hash<long long> {
    size_t operator()(long long value) const noexcept {
        const unsigned long long bits = static_cast<unsigned long long>(value);
        return static_cast<size_t>(bits ^ (bits >> 32));
    }
};
template<> struct hash<unsigned long long> {
    size_t operator()(unsigned long long value) const noexcept {
        return static_cast<size_t>(value ^ (value >> 32));
    }
};

} /* namespace std */
#endif /* __cplusplus */

#endif /* RINCXX_HASH_INTEGRAL_H */
