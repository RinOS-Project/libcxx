/* SPDX-License-Identifier: MIT */
#ifndef RINCXX_INTERNAL_STRING_HASH_H
#define RINCXX_INTERNAL_STRING_HASH_H

#include "cstddef.h"
#include "type_traits.h"

#ifdef __cplusplus
namespace std {
namespace detail {

template<typename Sequence>
inline size_t libcxx_hash_character_sequence(const Sequence& sequence) noexcept
{
    using character_type = typename Sequence::value_type;
    using unsigned_character = typename make_unsigned<character_type>::type;
    size_t value = static_cast<size_t>(2166136261u);
    for (size_t index = 0; index < sequence.size(); ++index) {
        value ^= static_cast<size_t>(
            static_cast<unsigned_character>(sequence[index]));
        value *= static_cast<size_t>(16777619u);
    }
    return value;
}

} /* namespace detail */
} /* namespace std */
#endif

#endif /* RINCXX_INTERNAL_STRING_HASH_H */
