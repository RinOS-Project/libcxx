/* SPDX-License-Identifier: MIT */
#ifndef RINCXX_RANDOM_BOUNDED_H
#define RINCXX_RANDOM_BOUNDED_H

#include "rincxx.h"

#ifdef __cplusplus

namespace std {
namespace __detail {

/*
 * Draw uniformly from [0, bound).  URBG digits are combined when one digit
 * cannot cover the requested interval.  Rejecting the incomplete high bucket
 * and then the incomplete requested interval avoids modulo bias.  A zero bound
 * represents the complete 64-bit interval, whose size cannot be represented
 * in an unsigned long long.
 */
template<typename URBG>
unsigned long long __bounded_random(URBG& generator,
                                    unsigned long long bound) {
    if (bound == 1u) return 0u;

    using generator_type = typename remove_reference<URBG>::type;
    using result_type = typename generator_type::result_type;
    static_assert(sizeof(result_type) <= sizeof(unsigned long long),
                  "URBG result_type is wider than supported integers");
    static_assert(generator_type::min() < generator_type::max(),
                  "URBG must have a non-empty result interval");

    const unsigned long long minimum =
        static_cast<unsigned long long>(generator_type::min());
    const unsigned long long maximum =
        static_cast<unsigned long long>(generator_type::max());
    const unsigned long long range = maximum - minimum;

    unsigned int digit_bits = 0u;
    if (range == ~0ull) {
        digit_bits = 64u;
    } else {
        unsigned long long base = range + 1u;
        while (base > 1u) {
            ++digit_bits;
            base >>= 1u;
        }
    }

    for (;;) {
        unsigned int needed_bits = 64u;
        if (bound != 0u) {
            needed_bits = 0u;
            unsigned long long ceiling = bound - 1u;
            while (ceiling != 0u) {
                ++needed_bits;
                ceiling >>= 1u;
            }
        }

        unsigned long long value = 0u;
        unsigned int produced_bits = 0u;
        while (produced_bits < needed_bits) {
            unsigned long long digit;
            if (digit_bits == 64u) {
                digit = static_cast<unsigned long long>(generator()) - minimum;
            } else {
                const unsigned long long accepted = 1ull << digit_bits;
                do {
                    digit = static_cast<unsigned long long>(generator()) -
                            minimum;
                } while (digit >= accepted);
            }

            unsigned int take = digit_bits;
            const unsigned int remaining = needed_bits - produced_bits;
            if (take > remaining) take = remaining;
            if (take == 64u) {
                value = digit;
            } else {
                value <<= take;
                value |= digit & ((1ull << take) - 1u);
            }
            produced_bits += take;
        }

        if (bound == 0u || value < bound) return value;
    }
}

} /* namespace __detail */
} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_RANDOM_BOUNDED_H */
