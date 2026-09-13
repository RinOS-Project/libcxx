/*
 * RinOS C++ <typeinfo>
 * Runtime type information
 */

#ifndef RINCXX_TYPEINFO_H
#define RINCXX_TYPEINFO_H

#include "rincxx.h"
#include "cstddef.h"
#include "exception.h"

#ifdef __cplusplus

namespace std {

/* ===================================================================
 * type_info - Runtime type information
 * ===================================================================*/

class type_info {
protected:
    const char* __name;

    explicit type_info(const char* name) : __name(name) {}

private:
    static const char* __canonical_name(const char* name) noexcept {
        if (!name) return "";
        return name[0] == '*' ? name + 1 : name;
    }

    static int __compare_names(const char* lhs, const char* rhs) noexcept {
        const unsigned char* left = reinterpret_cast<const unsigned char*>(
            __canonical_name(lhs));
        const unsigned char* right = reinterpret_cast<const unsigned char*>(
            __canonical_name(rhs));
        while (*left != 0u && *left == *right) {
            ++left;
            ++right;
        }
        return *left < *right ? -1 : *left > *right ? 1 : 0;
    }

public:
    virtual ~type_info();

    type_info(const type_info&) = delete;
    type_info& operator=(const type_info&) = delete;

    /* GCC's Itanium ABI prefixes non-unique names with '*'; name() exposes
       the implementation name without that internal uniqueness marker. */
    const char* name() const noexcept { return __canonical_name(__name); }

    bool before(const type_info& rhs) const noexcept {
        if (__name == rhs.__name) return false;
        return __compare_names(__name, rhs.__name) < 0;
    }

    size_t hash_code() const noexcept {
        const unsigned char* cursor = reinterpret_cast<const unsigned char*>(
            __canonical_name(__name));
        size_t hash = sizeof(size_t) == 8u
            ? static_cast<size_t>(14695981039346656037ULL)
            : static_cast<size_t>(2166136261u);
        const size_t prime = sizeof(size_t) == 8u
            ? static_cast<size_t>(1099511628211ULL)
            : static_cast<size_t>(16777619u);
        while (*cursor != 0u) {
            hash ^= static_cast<size_t>(*cursor++);
            hash *= prime;
        }
        return hash;
    }

    bool operator==(const type_info& rhs) const noexcept {
        return __name == rhs.__name || __compare_names(__name, rhs.__name) == 0;
    }

    bool operator!=(const type_info& rhs) const noexcept {
        return !(*this == rhs);
    }
};

/* `any::type()` and `function::target_type()` remain part of the standard
 * surface even for Rin builds compiled with `-fno-rtti`.  Compiler RTTI
 * objects cannot be named in that mode, so provide a small type_info carrier
 * whose name is the compiler's stable template signature.  Equality still
 * uses type_info's canonical string comparison, which keeps equivalent types
 * comparable across translation units without image-local token addresses. */
namespace detail {

class rin_named_type_info final : public type_info {
public:
    explicit rin_named_type_info(const char* name) : type_info(name) {}
};

template<class T>
inline const type_info& rin_no_rtti_type_info() noexcept {
#if defined(_MSC_VER)
    static rin_named_type_info value(__FUNCSIG__);
#elif defined(__clang__) || defined(__GNUC__)
    static rin_named_type_info value(__PRETTY_FUNCTION__);
#else
    static rin_named_type_info value("rin.type.unknown");
#endif
    return value;
}

} /* namespace detail */

/* bad_cast と bad_typeid は exception.h で定義済み */

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_TYPEINFO_H */
