/*
 * RinOS C++ <typeindex>
 * type_index wrapper for type_info
 */

#ifndef RINCXX_TYPEINDEX_H
#define RINCXX_TYPEINDEX_H

#include "rincxx.h"
#include "typeinfo.h"
#include "functional.h"
#if defined(__cplusplus) && __cplusplus >= 202002L
#include "compare.h"
#endif

#ifdef __cplusplus

namespace std {

/* ===================================================================
 * type_index - Wrapper around type_info for use as container key
 * ===================================================================*/

class type_index {
public:
    type_index(const type_info& ti) noexcept : __target(&ti) {}

    bool operator==(const type_index& rhs) const noexcept {
        return *__target == *rhs.__target;
    }

    bool operator!=(const type_index& rhs) const noexcept {
        return *__target != *rhs.__target;
    }

    bool operator<(const type_index& rhs) const noexcept {
        return __target->before(*rhs.__target);
    }

    bool operator<=(const type_index& rhs) const noexcept {
        return !rhs.__target->before(*__target);
    }

    bool operator>(const type_index& rhs) const noexcept {
        return rhs.__target->before(*__target);
    }

    bool operator>=(const type_index& rhs) const noexcept {
        return !__target->before(*rhs.__target);
    }

#if __cplusplus >= 202002L
    strong_ordering operator<=>(const type_index& rhs) const noexcept {
        if (*__target == *rhs.__target) return strong_ordering::equal;
        return __target->before(*rhs.__target)
            ? strong_ordering::less
            : strong_ordering::greater;
    }
#endif

    size_t hash_code() const noexcept {
        return __target->hash_code();
    }

    const char* name() const noexcept {
        return __target->name();
    }

private:
    const type_info* __target;
};

/* ===================================================================
 * hash specialization for type_index
 * ===================================================================*/

template<>
struct hash<type_index> {
    size_t operator()(const type_index& ti) const noexcept {
        return ti.hash_code();
    }
};

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_TYPEINDEX_H */
