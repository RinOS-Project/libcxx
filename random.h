/*
 * RinOS C++ <random> ✿
 * 乱数ライブラリ
 */

#ifndef RINCXX_RANDOM_H
#define RINCXX_RANDOM_H

#include "rincxx.h"
#include "limits.h"
#include "type_traits.h"
#include "cstdint.h"
#include "__nextafter.h"
#include "cstdlib.h"
#include "string.h"
#include "vector.h"
#include "iosfwd.h"
#include "pointer_order.h"
#include "__random_bounded.h"
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && \
    __STDC_HOSTED__ && defined(SYS_OPEN)
#pragma push_macro("SYS_OPEN")
#undef SYS_OPEN
#define RINCXX_RANDOM_RESTORE_HOST_SYS_OPEN 1
#endif
#include "../libc/sys/syscall.h"
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && __STDC_HOSTED__
#ifdef RINCXX_RANDOM_RESTORE_HOST_SYS_OPEN
#undef SYS_OPEN
#pragma pop_macro("SYS_OPEN")
#undef RINCXX_RANDOM_RESTORE_HOST_SYS_OPEN
#else
/* Do not leak the Rin target-only uppercase alias into a hosted CRT. */
#undef SYS_OPEN
#endif
#endif

#if __cplusplus >= 201402L
#define RIN_RANDOM_CONSTEXPR14 constexpr
#else
#define RIN_RANDOM_CONSTEXPR14 inline
#endif

/* カーネル乱数・エントロピー関数 */
extern "C" {
    unsigned long long rin_get_ticks(void);
    /* エントロピープールからの真の乱数 (RDRAND/RDSEED + TSC) */
#if defined(__GNUC__) || defined(__clang__)
    unsigned int platform_random32(void) __attribute__((weak));
    unsigned long long platform_random64(void) __attribute__((weak));
    int platform_has_rdrand(void) __attribute__((weak));
#else
    unsigned int platform_random32(void);
    unsigned long long platform_random64(void);
    int platform_has_rdrand(void);
#endif
    /* RDRANDが利用可能かどうか */
    /* 外部エントロピーを追加 */
    void platform_add_entropy(unsigned long long value);
}

namespace std {

namespace __detail {

#ifndef RIN_RANDOM_DEVICE_SYSCALL3
#define RIN_RANDOM_DEVICE_SYSCALL3(number, buffer, length, flags) \
    _syscall3((uintptr_t)(number), (uintptr_t)(buffer), \
              (uintptr_t)(length), (uintptr_t)(flags))
#define RIN_RANDOM_DEVICE_SYSCALL3_INTERNAL 1
#endif

inline bool __random_device_getentropy(void* buffer, size_t size) noexcept {
    if (!buffer || size == 0u) return false;
    const intptr_t result = RIN_RANDOM_DEVICE_SYSCALL3(
        __NR_getrandom, buffer, size, 0u);
    return result == (intptr_t)size;
}

inline bool __random_device_platform32_available() noexcept {
    return detail::function_pointer_hash(&platform_random32) != 0u;
}

inline bool __random_device_rdrand_available() noexcept {
    return detail::function_pointer_hash(&platform_has_rdrand) != 0u;
}

inline bool __random_parameter_finite(double value) noexcept {
    return value == value &&
           value <= numeric_limits<double>::max() &&
           value >= -numeric_limits<double>::max();
}

template<typename RealType>
inline bool __random_real_parameter_finite(RealType value) noexcept {
    return value == value &&
           value <= numeric_limits<RealType>::max() &&
           value >= -numeric_limits<RealType>::max();
}

[[noreturn]] inline void __random_invalid_parameter() noexcept {
    /* Distribution preconditions are not recoverable in a freestanding
     * runtime.  Never turn NaN/range violations into an infinite retry loop
     * or a deterministic sample. */
    __builtin_trap();
}

} /* namespace __detail */

#ifdef RIN_RANDOM_DEVICE_SYSCALL3_INTERNAL
#undef RIN_RANDOM_DEVICE_SYSCALL3_INTERNAL
#undef RIN_RANDOM_DEVICE_SYSCALL3
#endif

/* ═══════════════════════════════════════════════════════════════
 * random_device - 非決定論的乱数生成器
 * エントロピープール (RDRAND/RDSEED + TSC) を使用
 * ═══════════════════════════════════════════════════════════════*/

class random_device {
public:
    using result_type = unsigned int;

    random_device() {}
    explicit random_device(const string&) {}

    random_device(const random_device&) = delete;
    random_device& operator=(const random_device&) = delete;

    result_type operator()() {
        /* Prefer the authenticated kernel/platform owner.  Hosted tools may
         * not link that owner, so use the same bounded getrandom ABI rather
         * than leaving an unresolved symbol or manufacturing a value. */
        if (__detail::__random_device_platform32_available()) {
            return platform_random32();
        }
        result_type value = 0;
        if (__detail::__random_device_getentropy(&value, sizeof(value))) {
            return value;
        }
        /* A random_device failure must never become deterministic output. */
        __builtin_trap();
        return 0;
    }

    double entropy() const noexcept {
        if (__detail::__random_device_rdrand_available() &&
            platform_has_rdrand()) {
            return 32.0;
        }
        /* getentropy is an authenticated CSPRNG boundary, but without a
         * provider health query no conservative estimate can be claimed. */
        return 0.0;
    }

    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return numeric_limits<result_type>::max(); }
};

namespace __detail {

template<typename Candidate, typename = void>
struct __is_seed_sequence : false_type {};

template<typename Candidate>
struct __is_seed_sequence<Candidate, void_t<decltype(
    declval<Candidate&>().generate(
        declval<uint_least32_t*>(), declval<uint_least32_t*>()))>>
    : true_type {};

template<unsigned long long Value>
struct __random_bit_count {
    static constexpr size_t value =
        1u + __random_bit_count<(Value >> 1u)>::value;
};

template<>
struct __random_bit_count<0u> {
    static constexpr size_t value = 0u;
};

template<typename UIntType, UIntType Modulus, bool IsZero = Modulus == 0u>
struct __random_seed_word_count;

template<typename UIntType, UIntType Modulus>
struct __random_seed_word_count<UIntType, Modulus, true> {
    static constexpr size_t value =
        (numeric_limits<UIntType>::digits + 31u) / 32u;
};

template<typename UIntType, UIntType Modulus>
struct __random_seed_word_count<UIntType, Modulus, false> {
    static constexpr size_t value =
        (__random_bit_count<
            static_cast<unsigned long long>(Modulus) - 1u>::value + 31u) /
        32u;
};

template<typename RealType>
inline RealType __random_log(RealType value)
{
#if defined(__i386__) || defined(__x86_64__)
    RealType result;
    __asm__ volatile(
        "fldln2\n\t"
        "fxch %%st(1)\n\t"
        "fyl2x"
        : "=t"(result)
        : "0"(value));
    return result;
#else
    return __builtin_log(value);
#endif
}

template<typename RealType>
inline RealType __random_log1p(RealType value)
{
#if defined(__i386__) || defined(__x86_64__)
    RealType result;
    __asm__ volatile(
        "fldln2\n\t"
        "fxch %%st(1)\n\t"
        "fyl2xp1"
        : "=t"(result)
        : "0"(value));
    return result;
#else
    return __builtin_log1p(value);
#endif
}

template<typename RealType>
inline RealType __random_sqrt(RealType value)
{
#if defined(__i386__) || defined(__x86_64__)
    RealType result;
    __asm__ volatile("fsqrt" : "=t"(result) : "0"(value));
    return result;
#else
    return __builtin_sqrt(value);
#endif
}

template<typename RealType>
inline RealType __random_exp(RealType value)
{
    if (value == -numeric_limits<RealType>::infinity()) return RealType(0);
    if (value == numeric_limits<RealType>::infinity()) return value;
#if defined(__i386__) || defined(__x86_64__)
    RealType result;
    __asm__ volatile(
        "fldl2e\n\t"
        "fmulp\n\t"
        "fld %%st(0)\n\t"
        "frndint\n\t"
        "fxch %%st(1)\n\t"
        "fsub %%st(1), %%st(0)\n\t"
        "f2xm1\n\t"
        "fld1\n\t"
        "faddp\n\t"
        "fscale\n\t"
        "fstp %%st(1)"
        : "=t"(result)
        : "0"(value));
    return result;
#else
    return __builtin_exp(value);
#endif
}

template<typename RealType>
inline RealType __random_tan(RealType value)
{
#if defined(__i386__) || defined(__x86_64__)
    const long double extended = static_cast<long double>(value);
    long double result;
    __asm__ volatile(
        "fldt %1\n\t"
        "fptan\n\t"
        "fstp %%st(0)\n\t"
        "fstpt %0"
        : "=m"(result)
        : "m"(extended)
        : "st");
    return static_cast<RealType>(result);
#else
    return __builtin_tan(value);
#endif
}

inline double __random_log_gamma_integer(double value)
{
    if (value == 1.0 || value == 2.0) return 0.0;
    int shifts = value < 7.0 ? static_cast<int>(7.0 - value) : 0;
    double augmented = value + static_cast<double>(shifts);
    const double inverse = 1.0 / augmented;
    const double inverse_squared = inverse * inverse;
    double series = -1.39243221690590;
    series = series * inverse_squared + 1.796443723688307e-1;
    series = series * inverse_squared - 2.955065359477124e-2;
    series = series * inverse_squared + 6.410256410256410e-3;
    series = series * inverse_squared - 1.917526917526918e-3;
    series = series * inverse_squared + 8.417508417508418e-4;
    series = series * inverse_squared - 5.952380952380952e-4;
    series = series * inverse_squared + 7.936507936507937e-4;
    series = series * inverse_squared - 2.777777777777778e-3;
    series = series * inverse_squared + 8.333333333333333e-2;
    double result = series * inverse + 0.91893853320467274178 +
        (augmented - 0.5) * __random_log(augmented) - augmented;
    while (shifts != 0) {
        augmented -= 1.0;
        result -= __random_log(augmented);
        --shifts;
    }
    return result;
}

template<typename CharT, typename Traits, typename UIntType>
bool __random_read_decimal(basic_istream<CharT, Traits>& stream,
                           UIntType maximum,
                           UIntType& output)
{
    static_assert(is_unsigned<UIntType>::value,
                  "random state tokens must be unsigned");
    if (stream.fail()) return false;

    using int_type = typename Traits::int_type;
    int_type current;
    for (;;) {
        current = stream.peek();
        if (Traits::eq_int_type(current, Traits::eof())) {
            stream.setstate(basic_istream<CharT, Traits>::failbit);
            return false;
        }
        const CharT character = Traits::to_char_type(current);
        if (character != static_cast<CharT>(' ') &&
            character != static_cast<CharT>('\t') &&
            character != static_cast<CharT>('\n') &&
            character != static_cast<CharT>('\r') &&
            character != static_cast<CharT>('\f') &&
            character != static_cast<CharT>('\v')) {
            break;
        }
        (void)stream.get();
        if (stream.fail()) return false;
    }

    if (Traits::to_char_type(current) == static_cast<CharT>('+')) {
        (void)stream.get();
    }

    UIntType candidate = 0u;
    bool has_digit = false;
    bool overflow = false;
    for (;;) {
        current = stream.peek();
        if (Traits::eq_int_type(current, Traits::eof())) break;
        const CharT character = Traits::to_char_type(current);
        if (character < static_cast<CharT>('0') ||
            character > static_cast<CharT>('9')) {
            break;
        }
        const UIntType digit = static_cast<UIntType>(
            character - static_cast<CharT>('0'));
        has_digit = true;
        if (!overflow) {
            if (digit > maximum ||
                candidate > static_cast<UIntType>(
                    (maximum - digit) / static_cast<UIntType>(10u))) {
                overflow = true;
            } else {
                candidate = static_cast<UIntType>(
                    candidate * static_cast<UIntType>(10u) + digit);
            }
        }
        (void)stream.get();
        if (stream.fail()) return false;
    }

    if (!has_digit || overflow) {
        stream.setstate(basic_istream<CharT, Traits>::failbit);
        return false;
    }
    output = candidate;
    return true;
}

/* A formatted extraction that reaches EOF after its last token sets only
 * eofbit.  That is a successful extraction when no more fields are needed,
 * but it must become a failure before a distribution attempts to read a
 * required subsequent field.  Keep that distinction in one owner so every
 * distribution stream parser remains transactional at truncated input. */
template<typename CharT, typename Traits, typename Value>
bool __random_read_required(basic_istream<CharT, Traits>& stream,
                            Value& output)
{
    using stream_type = basic_istream<CharT, Traits>;
    if (!stream.good()) {
        stream.setstate(stream_type::failbit);
        return false;
    }
    stream >> output;
    return !stream.fail();
}

template<typename CharT, typename Traits, typename IntType>
bool __random_read_integer(basic_istream<CharT, Traits>& stream,
                           IntType& output)
{
    static_assert(is_integral<IntType>::value,
                  "random integer tokens require an integral type");
    using unsigned_type = typename make_unsigned<IntType>::type;
    static_assert(sizeof(unsigned_type) <= sizeof(unsigned long long),
                  "random integer token is wider than supported storage");
    if (!stream.good()) {
        stream.setstate(basic_istream<CharT, Traits>::failbit);
        return false;
    }

    using int_type = typename Traits::int_type;
    int_type current;
    for (;;) {
        current = stream.peek();
        if (Traits::eq_int_type(current, Traits::eof())) {
            stream.setstate(basic_istream<CharT, Traits>::failbit);
            return false;
        }
        const CharT character = Traits::to_char_type(current);
        if (character != static_cast<CharT>(' ') &&
            character != static_cast<CharT>('\t') &&
            character != static_cast<CharT>('\n') &&
            character != static_cast<CharT>('\r') &&
            character != static_cast<CharT>('\f') &&
            character != static_cast<CharT>('\v')) {
            break;
        }
        (void)stream.get();
        if (stream.fail()) return false;
    }

    bool negative = false;
    if (Traits::to_char_type(current) == static_cast<CharT>('-')) {
        if (!is_signed<IntType>::value) {
            stream.setstate(basic_istream<CharT, Traits>::failbit);
            return false;
        }
        negative = true;
        (void)stream.get();
    } else if (Traits::to_char_type(current) == static_cast<CharT>('+')) {
        (void)stream.get();
    }

    const unsigned_type positive_maximum = static_cast<unsigned_type>(
        numeric_limits<IntType>::max());
    unsigned_type magnitude_maximum = positive_maximum;
    if (is_signed<IntType>::value) {
        if (negative) {
            magnitude_maximum = static_cast<unsigned_type>(
                positive_maximum + static_cast<unsigned_type>(1u));
        }
    }

    unsigned_type magnitude = 0u;
    if (!__random_read_decimal(stream, magnitude_maximum, magnitude)) {
        return false;
    }
    if (is_signed<IntType>::value) {
        if (negative) {
            if (magnitude == magnitude_maximum) {
                output = numeric_limits<IntType>::min();
            } else {
                output = static_cast<IntType>(-
                    static_cast<IntType>(magnitude));
            }
        } else {
            output = static_cast<IntType>(magnitude);
        }
    } else {
        output = static_cast<IntType>(magnitude);
    }
    return true;
}

template<typename CharT, typename Traits, typename IntType>
void __random_write_integer(basic_ostream<CharT, Traits>& stream,
                            IntType value)
{
    if (is_signed<IntType>::value) {
        stream << static_cast<long long>(value);
    } else {
        stream << static_cast<unsigned long long>(value);
    }
}

} /* namespace __detail */

/* ═══════════════════════════════════════════════════════════════
 * linear_congruential_engine
 * ═══════════════════════════════════════════════════════════════*/

template<typename UIntType, UIntType a, UIntType c, UIntType m>
class linear_congruential_engine {
    static_assert(is_unsigned<UIntType>::value, "UIntType must be unsigned");
    static_assert(m == 0u || (a < m && c < m),
                  "multiplier and increment must be below the modulus");

    UIntType state_;

    template<typename SeedSequence>
    using __enable_seed_sequence = enable_if_t<
        __detail::__is_seed_sequence<SeedSequence>::value &&
        !is_same<remove_cvref_t<SeedSequence>,
                 linear_congruential_engine>::value &&
        !is_convertible<SeedSequence&, UIntType>::value>;

    static constexpr UIntType __add_mod(UIntType left, UIntType right)
    {
        return left >= m - right ? left - (m - right) : left + right;
    }

    static RIN_RANDOM_CONSTEXPR14 UIntType __transition(UIntType state)
    {
        if (m == 0u) {
            return static_cast<UIntType>(a * state + c);
        } else {
            UIntType product = 0u;
            UIntType factor = state;
            UIntType multiplier_value = a;
            while (multiplier_value != 0u) {
                if ((multiplier_value & 1u) != 0u) {
                    product = __add_mod(product, factor);
                }
                multiplier_value >>= 1u;
                if (multiplier_value != 0u) {
                    factor = __add_mod(factor, factor);
                }
            }
            return __add_mod(product, c);
        }
    }

    static constexpr size_t __seed_word_count()
    {
        return __detail::__random_seed_word_count<UIntType, m>::value;
    }

    static UIntType __seed_state(UIntType value, true_type)
    {
        return value;
    }

    static UIntType __seed_state(UIntType value, false_type)
    {
        return value % m;
    }

public:
    using result_type = UIntType;

    static constexpr result_type multiplier = a;
    static constexpr result_type increment = c;
    static constexpr result_type modulus = m;
    static constexpr result_type default_seed = 1u;

    static constexpr result_type min() { return c == 0u ? 1u : 0u; }
    static constexpr result_type max() { return m - 1u; }

    linear_congruential_engine()
    {
        seed(default_seed);
    }

    explicit linear_congruential_engine(result_type s)
    {
        seed(s);
    }

    template<typename SeedSequence,
             typename = __enable_seed_sequence<SeedSequence>>
    explicit linear_congruential_engine(SeedSequence& sequence)
    {
        seed(sequence);
    }

    void seed(result_type s = default_seed)
    {
        state_ = __seed_state(s, integral_constant<bool, m == 0u>());
        if (state_ == 0u && c == 0u) state_ = 1u;
    }

    template<typename SeedSequence>
    __enable_seed_sequence<SeedSequence>
    seed(SeedSequence& sequence)
    {
        constexpr size_t count = __seed_word_count();
        uint_least32_t words[count + 3u];
        sequence.generate(words, words + count + 3u);
        UIntType value = 0u;
        UIntType factor = 1u;
        for (size_t index = 0u; index < count; ++index) {
            value += static_cast<UIntType>(words[index + 3u]) * factor;
            factor *= static_cast<UIntType>(4294967296ull);
        }
        seed(value);
    }

    result_type operator()()
    {
        state_ = __transition(state_);
        return state_;
    }

    void discard(unsigned long long z) {
        for (unsigned long long i = 0; i < z; ++i) {
            (*this)();
        }
    }

    bool operator==(const linear_congruential_engine& other) const {
        return state_ == other.state_;
    }

    bool operator!=(const linear_congruential_engine& other) const {
        return !(*this == other);
    }

    template<typename CharT, typename Traits>
    friend basic_ostream<CharT, Traits>&
    operator<<(basic_ostream<CharT, Traits>& stream,
               const linear_congruential_engine& engine)
    {
        using stream_type = basic_ostream<CharT, Traits>;
        const typename stream_type::fmtflags flags = stream.flags();
        const CharT fill = stream.fill();
        stream.flags(stream_type::dec | stream_type::fixed |
                     stream_type::left);
        stream.fill(static_cast<CharT>(' '));
        stream << static_cast<unsigned long long>(engine.state_);
        stream.flags(flags);
        stream.fill(fill);
        return stream;
    }

    template<typename CharT, typename Traits>
    friend basic_istream<CharT, Traits>&
    operator>>(basic_istream<CharT, Traits>& stream,
               linear_congruential_engine& engine)
    {
        using stream_type = basic_istream<CharT, Traits>;
        const typename stream_type::fmtflags flags = stream.flags();
        stream.flags(stream_type::dec | stream_type::skipws);
        UIntType candidate = 0u;
        const bool parsed = __detail::__random_read_decimal(
            stream, numeric_limits<UIntType>::max(), candidate);
        if (parsed && (candidate < min() || candidate > max())) {
            stream.setstate(stream_type::failbit);
        } else if (parsed && !stream.fail()) {
            engine.state_ = candidate;
        }
        stream.flags(flags);
        return stream;
    }
};

/* 標準的なLCG */
using minstd_rand0 = linear_congruential_engine<
    uint_fast32_t, 16807u, 0u, 2147483647u>;
using minstd_rand = linear_congruential_engine<
    uint_fast32_t, 48271u, 0u, 2147483647u>;

/* ═══════════════════════════════════════════════════════════════
 * mersenne_twister_engine
 * ═══════════════════════════════════════════════════════════════*/

template<typename UIntType, size_t w, size_t n, size_t m, size_t r,
         UIntType a, size_t u, UIntType d, size_t s,
         UIntType b, size_t t, UIntType c, size_t l, UIntType f>
class mersenne_twister_engine {
    static_assert(is_unsigned<UIntType>::value, "UIntType must be unsigned");
    static_assert(1u <= m && m <= n, "Invalid m");
    static_assert(2u < w, "Invalid w");
    static_assert(r <= w, "Invalid r");
    static_assert(u <= w && s <= w && t <= w && l <= w,
                  "Invalid tempering shift");
    static_assert(w <= numeric_limits<UIntType>::digits, "Invalid w");

    UIntType mt_[n];
    size_t index_;

    template<typename SeedSequence>
    using __enable_seed_sequence = enable_if_t<
        __detail::__is_seed_sequence<SeedSequence>::value &&
        !is_same<remove_cvref_t<SeedSequence>,
                 mersenne_twister_engine>::value &&
        !is_convertible<SeedSequence&, UIntType>::value>;

    static constexpr UIntType __low_mask(size_t bits)
    {
        return bits == 0u ? 0u
             : bits >= static_cast<size_t>(numeric_limits<UIntType>::digits)
                   ? ~static_cast<UIntType>(0)
                   : static_cast<UIntType>(
                         (static_cast<UIntType>(1) << bits) - 1u);
    }

    static constexpr UIntType word_mask = __low_mask(w);
    static constexpr UIntType lower_mask = __low_mask(r);
    static constexpr UIntType upper_mask =
        word_mask & ~lower_mask;

    static_assert(a <= word_mask && b <= word_mask && c <= word_mask &&
                  d <= word_mask && f <= word_mask,
                  "mask parameters exceed the word width");

    static constexpr UIntType __right_shift(UIntType value, size_t amount)
    {
        return amount >=
                   static_cast<size_t>(numeric_limits<UIntType>::digits)
             ? 0u : value >> amount;
    }

    static constexpr UIntType __left_shift(UIntType value, size_t amount)
    {
        return amount >=
                   static_cast<size_t>(numeric_limits<UIntType>::digits)
             ? 0u
             : static_cast<UIntType>(value << amount) & word_mask;
    }

    void generate()
    {
        for (size_t i = 0; i < n; ++i) {
            UIntType y = (mt_[i] & upper_mask) | (mt_[(i + 1) % n] & lower_mask);
            mt_[i] = (mt_[(i + m) % n] ^ (y >> 1u)) & word_mask;
            if ((y & 1u) != 0u) {
                mt_[i] ^= a;
            }
        }
        index_ = 0;
    }

public:
    using result_type = UIntType;

    static constexpr size_t word_size = w;
    static constexpr size_t state_size = n;
    static constexpr size_t shift_size = m;
    static constexpr size_t mask_bits = r;
    static constexpr UIntType xor_mask = a;
    static constexpr size_t tempering_u = u;
    static constexpr UIntType tempering_d = d;
    static constexpr size_t tempering_s = s;
    static constexpr UIntType tempering_b = b;
    static constexpr size_t tempering_t = t;
    static constexpr UIntType tempering_c = c;
    static constexpr size_t tempering_l = l;
    static constexpr UIntType initialization_multiplier = f;
    /* Narrow UIntType engines are valid extensions; mask the canonical seed
     * before conversion so the intentional modulo value is warning-free. */
    static constexpr result_type default_seed =
        static_cast<result_type>(5489u &
                                 static_cast<unsigned int>(word_mask));

    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return word_mask; }

    mersenne_twister_engine()
    {
        seed(default_seed);
    }

    explicit mersenne_twister_engine(result_type seed_value)
    {
        seed(seed_value);
    }

    template<typename SeedSequence,
             typename = __enable_seed_sequence<SeedSequence>>
    explicit mersenne_twister_engine(SeedSequence& sequence)
    {
        seed(sequence);
    }

    void seed(result_type seed_value = default_seed)
    {
        mt_[0] = seed_value & word_mask;
        for (size_t i = 1; i < n; ++i) {
            const UIntType previous = mt_[i - 1u];
            mt_[i] = static_cast<UIntType>(
                f * (previous ^ (previous >> (w - 2u))) +
                static_cast<UIntType>(i)) & word_mask;
        }
        index_ = n;
    }

    template<typename SeedSequence>
    __enable_seed_sequence<SeedSequence>
    seed(SeedSequence& sequence)
    {
        constexpr size_t words_per_state = (w + 31u) / 32u;
        uint_least32_t words[n * words_per_state];
        sequence.generate(words, words + n * words_per_state);

        bool all_zero = true;
        for (size_t state_index = 0u; state_index < n; ++state_index) {
            UIntType value = 0u;
            UIntType factor = 1u;
            for (size_t word_index = 0u;
                 word_index < words_per_state; ++word_index) {
                value += static_cast<UIntType>(
                    words[state_index * words_per_state + word_index]) * factor;
                factor *= static_cast<UIntType>(4294967296ull);
            }
            mt_[state_index] = value & word_mask;
            if (state_index == 0u) {
                if ((mt_[0] & upper_mask) != 0u) all_zero = false;
            } else if (mt_[state_index] != 0u) {
                all_zero = false;
            }
        }
        if (all_zero) {
            mt_[0] = static_cast<UIntType>(1) << (w - 1u);
        }
        index_ = n;
    }

    result_type operator()()
    {
        if (index_ >= n) {
            generate();
        }

        UIntType y = mt_[index_++];

        /* テンパリング */
        y ^= __right_shift(y, u) & d;
        y ^= __left_shift(y, s) & b;
        y ^= __left_shift(y, t) & c;
        y ^= __right_shift(y, l);

        return y & word_mask;
    }

    void discard(unsigned long long z) {
        for (unsigned long long i = 0; i < z; ++i) {
            (*this)();
        }
    }

    bool operator==(const mersenne_twister_engine& other) const {
        if (index_ != other.index_) return false;
        for (size_t i = 0; i < n; ++i) {
            if (mt_[i] != other.mt_[i]) return false;
        }
        return true;
    }

    bool operator!=(const mersenne_twister_engine& other) const {
        return !(*this == other);
    }

    template<typename CharT, typename Traits>
    friend basic_ostream<CharT, Traits>&
    operator<<(basic_ostream<CharT, Traits>& stream,
               const mersenne_twister_engine& engine)
    {
        using stream_type = basic_ostream<CharT, Traits>;
        const typename stream_type::fmtflags flags = stream.flags();
        const CharT fill = stream.fill();
        const CharT space = static_cast<CharT>(' ');
        stream.flags(stream_type::dec | stream_type::fixed |
                     stream_type::left);
        stream.fill(space);
        for (size_t state_index = 0u; state_index < n; ++state_index) {
            stream << static_cast<unsigned long long>(
                engine.mt_[state_index]) << space;
        }
        stream << engine.index_;
        stream.flags(flags);
        stream.fill(fill);
        return stream;
    }

    template<typename CharT, typename Traits>
    friend basic_istream<CharT, Traits>&
    operator>>(basic_istream<CharT, Traits>& stream,
               mersenne_twister_engine& engine)
    {
        using stream_type = basic_istream<CharT, Traits>;
        const typename stream_type::fmtflags flags = stream.flags();
        stream.flags(stream_type::dec | stream_type::skipws);
        UIntType candidate_state[n];
        size_t candidate_index = 0u;
        bool parsed = true;
        for (size_t state_index = 0u; state_index < n; ++state_index) {
            if (!__detail::__random_read_decimal(
                    stream, word_mask, candidate_state[state_index])) {
                parsed = false;
                break;
            }
        }
        if (parsed && !__detail::__random_read_decimal(
                stream, n, candidate_index)) {
            parsed = false;
        }
        if (parsed && !stream.fail()) {
            for (size_t state_index = 0u; state_index < n; ++state_index) {
                engine.mt_[state_index] = candidate_state[state_index];
            }
            engine.index_ = candidate_index;
        }
        stream.flags(flags);
        return stream;
    }
};

/* 標準的なMersenne Twister */
using mt19937 = mersenne_twister_engine<
    uint_fast32_t, 32, 624, 397, 31,
    0x9908b0dfUL, 11, 0xffffffffUL, 7,
    0x9d2c5680UL, 15, 0xefc60000UL, 18, 1812433253UL>;

using mt19937_64 = mersenne_twister_engine<
    unsigned long long, 64, 312, 156, 31,
    0xb5026f5aa96619e9ULL, 29, 0x5555555555555555ULL, 17,
    0x71d67fffeda60000ULL, 37, 0xfff7eee000000000ULL, 43, 6364136223846793005ULL>;

/* ═══════════════════════════════════════════════════════════════
 * subtract_with_carry_engine
 * ═══════════════════════════════════════════════════════════════*/

template<typename UIntType, size_t w, size_t s, size_t r>
class subtract_with_carry_engine {
    static_assert(is_unsigned<UIntType>::value, "UIntType must be unsigned");
    static_assert(0u < s && s < r, "Invalid lag parameters");
    static_assert(0u < w && w <= numeric_limits<UIntType>::digits,
                  "Invalid word width");

    UIntType state_[r];
    UIntType carry_;
    size_t index_;

    template<typename SeedSequence>
    using __enable_seed_sequence = enable_if_t<
        __detail::__is_seed_sequence<SeedSequence>::value &&
        !is_same<remove_cvref_t<SeedSequence>,
                 subtract_with_carry_engine>::value &&
        !is_convertible<SeedSequence&, UIntType>::value>;

    static constexpr UIntType __word_mask()
    {
        return w == numeric_limits<UIntType>::digits
             ? ~static_cast<UIntType>(0)
             : static_cast<UIntType>((static_cast<UIntType>(1) << w) - 1u);
    }

    template<typename WordSource>
    void __seed_words(WordSource& source)
    {
        constexpr size_t words_per_state = (w + 31u) / 32u;
        for (size_t state_index = 0u; state_index < r; ++state_index) {
            UIntType value = 0u;
            UIntType factor = 1u;
            for (size_t word_index = 0u;
                 word_index < words_per_state; ++word_index) {
                value += static_cast<UIntType>(source()) * factor;
                factor *= static_cast<UIntType>(4294967296ull);
            }
            state_[state_index] = value & __word_mask();
        }
        carry_ = state_[r - 1u] == 0u ? 1u : 0u;
        index_ = 0u;
    }

public:
    using result_type = UIntType;

    static constexpr size_t word_size = w;
    static constexpr size_t short_lag = s;
    static constexpr size_t long_lag = r;
    static constexpr uint_least32_t default_seed = 19780503u;

    subtract_with_carry_engine()
    {
        seed();
    }

    explicit subtract_with_carry_engine(result_type value)
    {
        seed(value);
    }

    template<typename SeedSequence,
             typename = __enable_seed_sequence<SeedSequence>>
    explicit subtract_with_carry_engine(SeedSequence& sequence)
    {
        seed(sequence);
    }

    void seed(result_type value = 0u)
    {
        using seed_engine = linear_congruential_engine<
            uint_least32_t, 40014u, 0u, 2147483563u>;
        seed_engine engine(value == 0u
            ? default_seed
            : static_cast<uint_least32_t>(value % 2147483563u));
        __seed_words(engine);
    }

    template<typename SeedSequence>
    __enable_seed_sequence<SeedSequence>
    seed(SeedSequence& sequence)
    {
        constexpr size_t words_per_state = (w + 31u) / 32u;
        uint_least32_t words[r * words_per_state];
        sequence.generate(words, words + r * words_per_state);
        size_t next = 0u;
        struct word_source {
            uint_least32_t* words;
            size_t* next;
            uint_least32_t operator()() { return words[(*next)++]; }
        } source{words, &next};
        __seed_words(source);
    }

    static constexpr result_type min() { return 0u; }
    static constexpr result_type max() { return __word_mask(); }

    result_type operator()()
    {
        const size_t short_index = index_ >= s
            ? index_ - s : index_ + r - s;
        const UIntType left = state_[short_index];
        const UIntType right = state_[index_];
        const bool borrow = left < right ||
            (left == right && carry_ != 0u);
        const UIntType result = static_cast<UIntType>(
            left - right - carry_) & __word_mask();
        carry_ = borrow ? 1u : 0u;
        state_[index_] = result;
        if (++index_ == r) index_ = 0u;
        return result;
    }

    void discard(unsigned long long count)
    {
        while (count-- != 0u) (void)(*this)();
    }

    bool operator==(const subtract_with_carry_engine& other) const
    {
        if (carry_ != other.carry_ || index_ != other.index_) return false;
        for (size_t index = 0u; index < r; ++index) {
            if (state_[index] != other.state_[index]) return false;
        }
        return true;
    }

    bool operator!=(const subtract_with_carry_engine& other) const
    {
        return !(*this == other);
    }

    template<typename CharT, typename Traits>
    friend basic_ostream<CharT, Traits>&
    operator<<(basic_ostream<CharT, Traits>& stream,
               const subtract_with_carry_engine& engine)
    {
        using stream_type = basic_ostream<CharT, Traits>;
        const typename stream_type::fmtflags flags = stream.flags();
        const CharT fill = stream.fill();
        const CharT space = static_cast<CharT>(' ');
        stream.flags(stream_type::dec | stream_type::fixed |
                     stream_type::left);
        stream.fill(space);
        for (size_t state_index = 0u; state_index < r; ++state_index) {
            stream << static_cast<unsigned long long>(
                engine.state_[state_index]) << space;
        }
        stream << static_cast<unsigned long long>(engine.carry_) << space
               << engine.index_;
        stream.flags(flags);
        stream.fill(fill);
        return stream;
    }

    template<typename CharT, typename Traits>
    friend basic_istream<CharT, Traits>&
    operator>>(basic_istream<CharT, Traits>& stream,
               subtract_with_carry_engine& engine)
    {
        using stream_type = basic_istream<CharT, Traits>;
        const typename stream_type::fmtflags flags = stream.flags();
        stream.flags(stream_type::dec | stream_type::skipws);
        UIntType candidate_state[r];
        UIntType candidate_carry = 0u;
        size_t candidate_index = 0u;
        bool parsed = true;
        for (size_t state_index = 0u; state_index < r; ++state_index) {
            if (!__detail::__random_read_decimal(
                    stream, max(), candidate_state[state_index])) {
                parsed = false;
                break;
            }
        }
        if (parsed && !__detail::__random_read_decimal(
                stream, static_cast<UIntType>(1u), candidate_carry)) {
            parsed = false;
        }
        if (parsed && !__detail::__random_read_decimal(
                stream, r - 1u, candidate_index)) {
            parsed = false;
        }
        if (parsed && !stream.fail()) {
            for (size_t state_index = 0u; state_index < r; ++state_index) {
                engine.state_[state_index] = candidate_state[state_index];
            }
            engine.carry_ = candidate_carry;
            engine.index_ = candidate_index;
        }
        stream.flags(flags);
        return stream;
    }
};

using ranlux24_base = subtract_with_carry_engine<
    uint_fast32_t, 24, 10, 24>;
using ranlux48_base = subtract_with_carry_engine<
    uint_fast64_t, 48, 5, 12>;

/* ═══════════════════════════════════════════════════════════════
 * discard_block_engine
 * ═══════════════════════════════════════════════════════════════*/

template<typename Engine, size_t p, size_t r>
class discard_block_engine {
    static_assert(1u <= r && r <= p, "Invalid block parameters");

    Engine engine_;
    size_t used_;

    template<typename SeedSequence>
    using __enable_seed_sequence = enable_if_t<
        __detail::__is_seed_sequence<SeedSequence>::value &&
        !is_same<remove_cvref_t<SeedSequence>,
                 discard_block_engine>::value &&
        !is_convertible<SeedSequence&, typename Engine::result_type>::value>;

public:
    using result_type = typename Engine::result_type;

    static constexpr size_t block_size = p;
    static constexpr size_t used_block = r;

    discard_block_engine() : engine_(), used_(0u) {}

    explicit discard_block_engine(const Engine& engine)
        : engine_(engine), used_(0u) {}

    explicit discard_block_engine(Engine&& engine)
        : engine_(std::move(engine)), used_(0u) {}

    explicit discard_block_engine(result_type value)
        : engine_(value), used_(0u) {}

    template<typename SeedSequence,
             typename = __enable_seed_sequence<SeedSequence>>
    explicit discard_block_engine(SeedSequence& sequence)
        : engine_(sequence), used_(0u) {}

    void seed()
    {
        engine_.seed();
        used_ = 0u;
    }

    void seed(result_type value)
    {
        engine_.seed(value);
        used_ = 0u;
    }

    template<typename SeedSequence>
    __enable_seed_sequence<SeedSequence>
    seed(SeedSequence& sequence)
    {
        engine_.seed(sequence);
        used_ = 0u;
    }

    const Engine& base() const noexcept { return engine_; }

    static constexpr result_type min() { return Engine::min(); }
    static constexpr result_type max() { return Engine::max(); }

    result_type operator()()
    {
        if (used_ >= r) {
            engine_.discard(p - used_);
            used_ = 0u;
        }
        ++used_;
        return engine_();
    }

    void discard(unsigned long long count)
    {
        while (count-- != 0u) (void)(*this)();
    }

    bool operator==(const discard_block_engine& other) const
    {
        return engine_ == other.engine_ && used_ == other.used_;
    }

    bool operator!=(const discard_block_engine& other) const
    {
        return !(*this == other);
    }

    template<typename CharT, typename Traits>
    friend basic_ostream<CharT, Traits>&
    operator<<(basic_ostream<CharT, Traits>& stream,
               const discard_block_engine& engine)
    {
        using stream_type = basic_ostream<CharT, Traits>;
        const typename stream_type::fmtflags flags = stream.flags();
        const CharT fill = stream.fill();
        const CharT space = static_cast<CharT>(' ');
        stream.flags(stream_type::dec | stream_type::fixed |
                     stream_type::left);
        stream.fill(space);
        stream << engine.engine_ << space << engine.used_;
        stream.flags(flags);
        stream.fill(fill);
        return stream;
    }

    template<typename CharT, typename Traits>
    friend basic_istream<CharT, Traits>&
    operator>>(basic_istream<CharT, Traits>& stream,
               discard_block_engine& engine)
    {
        using stream_type = basic_istream<CharT, Traits>;
        const typename stream_type::fmtflags flags = stream.flags();
        stream.flags(stream_type::dec | stream_type::skipws);
        Engine candidate_engine = engine.engine_;
        size_t candidate_used = 0u;
        stream >> candidate_engine;
        bool parsed = !stream.fail();
        if (parsed && !__detail::__random_read_decimal(
                stream, r, candidate_used)) {
            parsed = false;
        }
        if (parsed && !stream.fail()) {
            engine.engine_ = candidate_engine;
            engine.used_ = candidate_used;
        }
        stream.flags(flags);
        return stream;
    }
};

using ranlux24 = discard_block_engine<ranlux24_base, 223, 23>;
using ranlux48 = discard_block_engine<ranlux48_base, 389, 11>;

/* ═══════════════════════════════════════════════════════════════
 * independent_bits_engine
 * ═══════════════════════════════════════════════════════════════*/

template<typename Engine, size_t w, typename UIntType>
class independent_bits_engine {
    static_assert(is_unsigned<UIntType>::value,
                  "result_type must be unsigned");
    static_assert(w > 0u && w <= numeric_limits<UIntType>::digits,
                  "Invalid output width");
    static_assert(is_unsigned<typename Engine::result_type>::value,
                  "base engine result_type must be unsigned");
    static_assert(Engine::min() < Engine::max(),
                  "base engine range must contain multiple values");

    Engine engine_;

    template<typename SeedSequence>
    using __enable_seed_sequence = enable_if_t<
        __detail::__is_seed_sequence<SeedSequence>::value &&
        !is_same<remove_cvref_t<SeedSequence>,
                 independent_bits_engine>::value &&
        !is_convertible<SeedSequence&, UIntType>::value>;

    static RIN_RANDOM_CONSTEXPR14 size_t
    __floor_log2(typename Engine::result_type value)
    {
        size_t result = 0u;
        while (value > 1u) {
            value >>= 1u;
            ++result;
        }
        return result;
    }

public:
    using result_type = UIntType;

    independent_bits_engine() : engine_() {}

    explicit independent_bits_engine(const Engine& engine)
        : engine_(engine) {}

    explicit independent_bits_engine(Engine&& engine)
        : engine_(std::move(engine)) {}

    explicit independent_bits_engine(result_type value)
        : engine_(value) {}

    template<typename SeedSequence,
             typename = __enable_seed_sequence<SeedSequence>>
    explicit independent_bits_engine(SeedSequence& sequence)
        : engine_(sequence) {}

    void seed() { engine_.seed(); }

    void seed(result_type value) { engine_.seed(value); }

    template<typename SeedSequence>
    __enable_seed_sequence<SeedSequence>
    seed(SeedSequence& sequence)
    {
        engine_.seed(sequence);
    }

    const Engine& base() const noexcept { return engine_; }

    static constexpr result_type min() { return 0u; }

    static constexpr result_type max()
    {
        return w == numeric_limits<result_type>::digits
             ? numeric_limits<result_type>::max()
             : static_cast<result_type>((result_type(1u) << w) - 1u);
    }

    result_type operator()()
    {
        using engine_result = typename Engine::result_type;
        using common_result = common_type_t<engine_result, result_type>;
        static_assert(is_unsigned<common_result>::value,
                      "common result type must be unsigned");

        constexpr size_t engine_digits =
            numeric_limits<engine_result>::digits;
        constexpr size_t common_digits =
            numeric_limits<common_result>::digits;
        const engine_result range = static_cast<engine_result>(
            Engine::max() - Engine::min());
        const engine_result outcomes =
            range < numeric_limits<engine_result>::max()
                ? static_cast<engine_result>(range + 1u)
                : 0u;
        const size_t source_bits = outcomes != 0u
            ? __floor_log2(outcomes) : engine_digits;

        size_t draws = 0u;
        size_t short_draws = 0u;
        common_result short_scale = 0u;
        common_result long_scale = 0u;
        common_result short_limit = 0u;
        common_result long_limit = 0u;
        const common_result common_outcomes =
            static_cast<common_result>(outcomes);

        for (size_t adjustment = 0u; adjustment < 2u; ++adjustment) {
            draws = (w + source_bits - 1u) / source_bits + adjustment;
            short_draws = draws - w % draws;
            const size_t short_width = w / draws;

            short_scale = 0u;
            long_scale = 0u;
            if (short_width < common_digits) {
                short_scale = common_result(1u) << short_width;
                long_scale = short_scale << 1u;
            }

            short_limit = 0u;
            long_limit = 0u;
            if (outcomes != 0u) {
                short_limit = short_scale *
                    (common_outcomes / short_scale);
                if (long_scale != 0u) {
                    long_limit = long_scale *
                        (common_outcomes / long_scale);
                }
                if (common_outcomes - short_limit <=
                    short_limit / draws) {
                    break;
                }
            } else {
                break;
            }
        }

        result_type result = 0u;
        for (size_t index = 0u; index < short_draws; ++index) {
            common_result value;
            do {
                value = static_cast<common_result>(engine_()) -
                        static_cast<common_result>(Engine::min());
            } while (short_limit != 0u && value >= short_limit);
            result = static_cast<result_type>(
                short_scale * static_cast<common_result>(result) +
                (short_scale != 0u ? value % short_scale : value));
        }
        for (size_t index = short_draws; index < draws; ++index) {
            common_result value;
            do {
                value = static_cast<common_result>(engine_()) -
                        static_cast<common_result>(Engine::min());
            } while (long_limit != 0u && value >= long_limit);
            result = static_cast<result_type>(
                long_scale * static_cast<common_result>(result) +
                (long_scale != 0u ? value % long_scale : value));
        }
        return result;
    }

    void discard(unsigned long long count)
    {
        while (count-- != 0u) (void)(*this)();
    }

    bool operator==(const independent_bits_engine& other) const
    {
        return engine_ == other.engine_;
    }

    bool operator!=(const independent_bits_engine& other) const
    {
        return !(*this == other);
    }

    template<typename CharT, typename Traits>
    friend basic_ostream<CharT, Traits>&
    operator<<(basic_ostream<CharT, Traits>& stream,
               const independent_bits_engine& engine)
    {
        stream << engine.engine_;
        return stream;
    }

    template<typename CharT, typename Traits>
    friend basic_istream<CharT, Traits>&
    operator>>(basic_istream<CharT, Traits>& stream,
               independent_bits_engine& engine)
    {
        Engine candidate_engine = engine.engine_;
        stream >> candidate_engine;
        if (!stream.fail()) engine.engine_ = candidate_engine;
        return stream;
    }
};

/* ═══════════════════════════════════════════════════════════════
 * shuffle_order_engine
 * ═══════════════════════════════════════════════════════════════*/

template<typename Engine, size_t k>
class shuffle_order_engine {
    static_assert(k >= 1u, "Invalid table size");

    Engine engine_;
    typename Engine::result_type table_[k];
    typename Engine::result_type current_;

    template<typename SeedSequence>
    using __enable_seed_sequence = enable_if_t<
        __detail::__is_seed_sequence<SeedSequence>::value &&
        !is_same<remove_cvref_t<SeedSequence>,
                 shuffle_order_engine>::value &&
        !is_convertible<SeedSequence&, typename Engine::result_type>::value>;

    void __initialize()
    {
        for (size_t index = 0u; index < k; ++index) {
            table_[index] = engine_();
        }
        current_ = engine_();
    }

public:
    using result_type = typename Engine::result_type;

    static constexpr size_t table_size = k;

    shuffle_order_engine() : engine_(), table_{}, current_(0u)
    {
        __initialize();
    }

    explicit shuffle_order_engine(const Engine& engine)
        : engine_(engine), table_{}, current_(0u)
    {
        __initialize();
    }

    explicit shuffle_order_engine(Engine&& engine)
        : engine_(std::move(engine)), table_{}, current_(0u)
    {
        __initialize();
    }

    explicit shuffle_order_engine(result_type value)
        : engine_(value), table_{}, current_(0u)
    {
        __initialize();
    }

    template<typename SeedSequence,
             typename = __enable_seed_sequence<SeedSequence>>
    explicit shuffle_order_engine(SeedSequence& sequence)
        : engine_(sequence), table_{}, current_(0u)
    {
        __initialize();
    }

    void seed()
    {
        engine_.seed();
        __initialize();
    }

    void seed(result_type value)
    {
        engine_.seed(value);
        __initialize();
    }

    template<typename SeedSequence>
    __enable_seed_sequence<SeedSequence>
    seed(SeedSequence& sequence)
    {
        engine_.seed(sequence);
        __initialize();
    }

    const Engine& base() const noexcept { return engine_; }

    static constexpr result_type min() { return Engine::min(); }
    static constexpr result_type max() { return Engine::max(); }

    result_type operator()()
    {
        const long double offset = static_cast<long double>(current_) -
                                   static_cast<long double>(min());
        const long double outcomes = static_cast<long double>(max()) -
                                     static_cast<long double>(min()) + 1.0L;
        const size_t index = static_cast<size_t>(
            static_cast<long double>(k) * offset / outcomes);
        current_ = table_[index];
        table_[index] = engine_();
        return current_;
    }

    void discard(unsigned long long count)
    {
        while (count-- != 0u) (void)(*this)();
    }

    bool operator==(const shuffle_order_engine& other) const
    {
        if (!(engine_ == other.engine_) || current_ != other.current_) {
            return false;
        }
        for (size_t index = 0u; index < k; ++index) {
            if (table_[index] != other.table_[index]) return false;
        }
        return true;
    }

    bool operator!=(const shuffle_order_engine& other) const
    {
        return !(*this == other);
    }

    template<typename CharT, typename Traits>
    friend basic_ostream<CharT, Traits>&
    operator<<(basic_ostream<CharT, Traits>& stream,
               const shuffle_order_engine& engine)
    {
        using stream_type = basic_ostream<CharT, Traits>;
        const typename stream_type::fmtflags flags = stream.flags();
        const CharT fill = stream.fill();
        const CharT space = static_cast<CharT>(' ');
        stream.flags(stream_type::dec | stream_type::fixed |
                     stream_type::left);
        stream.fill(space);
        stream << engine.engine_;
        for (size_t table_index = 0u; table_index < k; ++table_index) {
            stream << space << static_cast<unsigned long long>(
                engine.table_[table_index]);
        }
        stream << space << static_cast<unsigned long long>(engine.current_);
        stream.flags(flags);
        stream.fill(fill);
        return stream;
    }

    template<typename CharT, typename Traits>
    friend basic_istream<CharT, Traits>&
    operator>>(basic_istream<CharT, Traits>& stream,
               shuffle_order_engine& engine)
    {
        using stream_type = basic_istream<CharT, Traits>;
        using engine_result = typename Engine::result_type;
        static_assert(is_unsigned<engine_result>::value,
                      "base engine result_type must be unsigned");
        const typename stream_type::fmtflags flags = stream.flags();
        stream.flags(stream_type::dec | stream_type::skipws);
        Engine candidate_engine = engine.engine_;
        engine_result candidate_table[k];
        engine_result candidate_current = 0u;
        stream >> candidate_engine;
        bool parsed = !stream.fail();
        for (size_t table_index = 0u; parsed && table_index < k;
             ++table_index) {
            if (!__detail::__random_read_decimal(
                    stream, numeric_limits<engine_result>::max(),
                    candidate_table[table_index]) ||
                candidate_table[table_index] < min() ||
                candidate_table[table_index] > max()) {
                if (!stream.fail()) stream.setstate(stream_type::failbit);
                parsed = false;
            }
        }
        if (parsed &&
            (!__detail::__random_read_decimal(
                 stream, numeric_limits<engine_result>::max(),
                 candidate_current) ||
             candidate_current < min() || candidate_current > max())) {
            if (!stream.fail()) stream.setstate(stream_type::failbit);
            parsed = false;
        }
        if (parsed && !stream.fail()) {
            engine.engine_ = candidate_engine;
            for (size_t table_index = 0u; table_index < k; ++table_index) {
                engine.table_[table_index] = candidate_table[table_index];
            }
            engine.current_ = candidate_current;
        }
        stream.flags(flags);
        return stream;
    }
};

using knuth_b = shuffle_order_engine<minstd_rand0, 256>;

/* ═══════════════════════════════════════════════════════════════
 * xorshift (RinOS標準)
 * ═══════════════════════════════════════════════════════════════*/

class xorshift32 {
    unsigned int state_;

public:
    using result_type = unsigned int;

    static constexpr result_type default_seed = 1u;
    static constexpr result_type min() { return 1; }
    static constexpr result_type max() { return numeric_limits<result_type>::max(); }

    explicit xorshift32(result_type s = default_seed) : state_(s ? s : 1) {}

    void seed(result_type s = default_seed) { state_ = s ? s : 1; }

    result_type operator()() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

    void discard(unsigned long long z) {
        for (unsigned long long i = 0; i < z; ++i) (*this)();
    }
};

/* デフォルトエンジン */
using default_random_engine = mt19937;

/* ═══════════════════════════════════════════════════════════════
 * uniform_int_distribution
 * ═══════════════════════════════════════════════════════════════*/

template<typename IntType = int>
class uniform_int_distribution {
    static_assert(is_integral<IntType>::value, "IntType must be integral");

    IntType a_, b_;

public:
    using result_type = IntType;

    struct param_type {
        using distribution_type = uniform_int_distribution;

        IntType a_, b_;
        param_type(IntType a = 0, IntType b = numeric_limits<IntType>::max())
            : a_(a), b_(b) {}
        IntType a() const { return a_; }
        IntType b() const { return b_; }

        friend bool operator==(const param_type& lhs, const param_type& rhs)
        {
            return lhs.a_ == rhs.a_ && lhs.b_ == rhs.b_;
        }

        friend bool operator!=(const param_type& lhs, const param_type& rhs)
        {
            return !(lhs == rhs);
        }
    };

    uniform_int_distribution() : a_(0), b_(numeric_limits<IntType>::max()) {}
    explicit uniform_int_distribution(IntType a, IntType b = numeric_limits<IntType>::max())
        : a_(a), b_(b) {}
    explicit uniform_int_distribution(const param_type& p) : a_(p.a_), b_(p.b_) {}

    void reset() {}

    IntType a() const { return a_; }
    IntType b() const { return b_; }

    param_type param() const { return param_type(a_, b_); }
    void param(const param_type& p) { a_ = p.a_; b_ = p.b_; }

    result_type min() const { return a_; }
    result_type max() const { return b_; }

    template<typename Generator>
    result_type operator()(Generator& g) {
        if (a_ > b_) __detail::__random_invalid_parameter();
        using UType = typename make_unsigned<IntType>::type;
        const UType range =
            static_cast<UType>(b_) - static_cast<UType>(a_);
        const unsigned long long outcomes =
            static_cast<unsigned long long>(range) + 1u;
        const UType offset = static_cast<UType>(
            __detail::__bounded_random(g, outcomes));
        UType encoded = static_cast<UType>(a_);
        encoded += offset;
        return static_cast<result_type>(encoded);
    }

    template<typename Generator>
    result_type operator()(Generator& g, const param_type& p) {
        uniform_int_distribution d(p);
        return d(g);
    }

    friend bool operator==(const uniform_int_distribution& lhs,
                           const uniform_int_distribution& rhs)
    {
        return lhs.a_ == rhs.a_ && lhs.b_ == rhs.b_;
    }

    friend bool operator!=(const uniform_int_distribution& lhs,
                           const uniform_int_distribution& rhs)
    {
        return !(lhs == rhs);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * uniform_real_distribution
 * ═══════════════════════════════════════════════════════════════*/

template<typename RealType, size_t bits, typename Generator>
RealType generate_canonical(Generator& g);

template<typename RealType = double>
class uniform_real_distribution {
    static_assert(is_floating_point<RealType>::value, "RealType must be floating point");

    RealType a_, b_;

public:
    using result_type = RealType;

    struct param_type {
        using distribution_type = uniform_real_distribution;

        RealType a_, b_;
        param_type(RealType a = 0.0, RealType b = 1.0) : a_(a), b_(b) {}
        RealType a() const { return a_; }
        RealType b() const { return b_; }

        friend bool operator==(const param_type& lhs, const param_type& rhs)
        {
            return lhs.a_ == rhs.a_ && lhs.b_ == rhs.b_;
        }

        friend bool operator!=(const param_type& lhs, const param_type& rhs)
        {
            return !(lhs == rhs);
        }
    };

    uniform_real_distribution() : a_(0.0), b_(1.0) {}
    explicit uniform_real_distribution(RealType a, RealType b = 1.0)
        : a_(a), b_(b) {}
    explicit uniform_real_distribution(const param_type& p) : a_(p.a_), b_(p.b_) {}

    void reset() {}

    RealType a() const { return a_; }
    RealType b() const { return b_; }

    param_type param() const { return param_type(a_, b_); }
    void param(const param_type& p) { a_ = p.a_; b_ = p.b_; }

    result_type min() const { return a_; }
    result_type max() const { return b_; }

    template<typename Generator>
    result_type operator()(Generator& g) {
        if (!__detail::__random_real_parameter_finite(a_) ||
            !__detail::__random_real_parameter_finite(b_) || a_ > b_) {
            __detail::__random_invalid_parameter();
        }
        const RealType unit = generate_canonical<
            RealType, static_cast<size_t>(numeric_limits<RealType>::digits)>(g);
        RealType result = (RealType(1) - unit) * a_ + unit * b_;
        if (result < a_) result = a_;
        if (!(result < b_)) result = __detail::__nextafter(b_, a_);
        return result;
    }

    template<typename Generator>
    result_type operator()(Generator& g, const param_type& p) {
        uniform_real_distribution d(p);
        return d(g);
    }

    friend bool operator==(const uniform_real_distribution& lhs,
                           const uniform_real_distribution& rhs)
    {
        return lhs.a_ == rhs.a_ && lhs.b_ == rhs.b_;
    }

    friend bool operator!=(const uniform_real_distribution& lhs,
                           const uniform_real_distribution& rhs)
    {
        return !(lhs == rhs);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * bernoulli_distribution
 * ═══════════════════════════════════════════════════════════════*/

class bernoulli_distribution {
    double p_;

public:
    using result_type = bool;

    struct param_type {
        using distribution_type = bernoulli_distribution;

        double p_;
        param_type(double p = 0.5) : p_(p) {}
        double p() const { return p_; }

        friend bool operator==(const param_type& lhs, const param_type& rhs)
        {
            return lhs.p_ == rhs.p_;
        }

        friend bool operator!=(const param_type& lhs, const param_type& rhs)
        {
            return !(lhs == rhs);
        }
    };

    bernoulli_distribution() : p_(0.5) {}
    explicit bernoulli_distribution(double p) : p_(p) {}
    explicit bernoulli_distribution(const param_type& params) : p_(params.p_) {}

    void reset() {}

    double p() const { return p_; }
    param_type param() const { return param_type(p_); }
    void param(const param_type& params) { p_ = params.p_; }

    result_type min() const { return false; }
    result_type max() const { return true; }

    template<typename Generator>
    result_type operator()(Generator& g) {
        if (!__detail::__random_parameter_finite(p_) ||
            p_ < 0.0 || p_ > 1.0) {
            __detail::__random_invalid_parameter();
        }
        if (p_ <= 0.0) return false;
        if (p_ >= 1.0) return true;
        return generate_canonical<
            double, static_cast<size_t>(numeric_limits<double>::digits)>(g) < p_;
    }

    template<typename Generator>
    result_type operator()(Generator& g, const param_type& params) {
        bernoulli_distribution d(params);
        return d(g);
    }

    friend bool operator==(const bernoulli_distribution& lhs,
                           const bernoulli_distribution& rhs)
    {
        return lhs.p_ == rhs.p_;
    }

    friend bool operator!=(const bernoulli_distribution& lhs,
                           const bernoulli_distribution& rhs)
    {
        return !(lhs == rhs);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * geometric_distribution
 * ═══════════════════════════════════════════════════════════════*/

template<typename IntType = int>
class geometric_distribution {
    static_assert(is_integral<IntType>::value, "IntType must be integral");

    double p_;

public:
    using result_type = IntType;

    struct param_type {
        using distribution_type = geometric_distribution;

        double p_;
        explicit param_type(double p = 0.5) : p_(p) {}
        double p() const { return p_; }

        friend bool operator==(const param_type& lhs, const param_type& rhs)
        {
            return lhs.p_ == rhs.p_;
        }

        friend bool operator!=(const param_type& lhs, const param_type& rhs)
        {
            return !(lhs == rhs);
        }
    };

    geometric_distribution() : p_(0.5) {}
    explicit geometric_distribution(double p) : p_(p) {}
    explicit geometric_distribution(const param_type& params) : p_(params.p_) {}

    void reset() {}

    double p() const { return p_; }
    param_type param() const { return param_type(p_); }
    void param(const param_type& params) { p_ = params.p_; }

    result_type min() const { return result_type(0); }
    result_type max() const { return numeric_limits<result_type>::max(); }

    template<typename Generator>
    result_type operator()(Generator& generator)
    {
        return (*this)(generator, param());
    }

    template<typename Generator>
    result_type operator()(Generator& generator, const param_type& params)
    {
        if (!__detail::__random_parameter_finite(params.p_) ||
            params.p_ <= 0.0 || params.p_ > 1.0) {
            __detail::__random_invalid_parameter();
        }
        if (params.p_ >= 1.0) return result_type(0);

        const double denominator = __detail::__random_log1p(-params.p_);
        const long double maximum =
            static_cast<long double>(numeric_limits<result_type>::max());
        for (;;) {
            const double unit = generate_canonical<
                double, static_cast<size_t>(numeric_limits<double>::digits)>(
                    generator);
            const double candidate =
                __detail::__random_log1p(-unit) / denominator;
            if (candidate >= 0.0 &&
                static_cast<long double>(candidate) <= maximum) {
                return static_cast<result_type>(candidate);
            }
        }
    }

    friend bool operator==(const geometric_distribution& lhs,
                           const geometric_distribution& rhs)
    {
        return lhs.p_ == rhs.p_;
    }

    friend bool operator!=(const geometric_distribution& lhs,
                           const geometric_distribution& rhs)
    {
        return !(lhs == rhs);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * negative_binomial_distribution
 * ═══════════════════════════════════════════════════════════════*/

template<typename IntType>
class poisson_distribution;

template<typename IntType = int>
class negative_binomial_distribution {
    static_assert(is_integral<IntType>::value, "IntType must be integral");

    IntType k_;
    double p_;

public:
    using result_type = IntType;

    struct param_type {
        using distribution_type = negative_binomial_distribution;

        IntType k_;
        double p_;
        explicit param_type(IntType k = 1, double p = 0.5) : k_(k), p_(p) {}
        IntType k() const { return k_; }
        double p() const { return p_; }

        friend bool operator==(const param_type& lhs, const param_type& rhs)
        {
            return lhs.k_ == rhs.k_ && lhs.p_ == rhs.p_;
        }

        friend bool operator!=(const param_type& lhs, const param_type& rhs)
        {
            return !(lhs == rhs);
        }
    };

    negative_binomial_distribution() : k_(1), p_(0.5) {}
    explicit negative_binomial_distribution(IntType k, double p = 0.5)
        : k_(k), p_(p) {}
    explicit negative_binomial_distribution(const param_type& params)
        : k_(params.k_), p_(params.p_) {}

    void reset() {}

    IntType k() const { return k_; }
    double p() const { return p_; }
    param_type param() const { return param_type(k_, p_); }
    void param(const param_type& params) { k_ = params.k_; p_ = params.p_; }

    result_type min() const { return result_type(0); }
    result_type max() const { return numeric_limits<result_type>::max(); }

    template<typename Generator>
    result_type operator()(Generator& generator)
    {
        return (*this)(generator, param());
    }

    template<typename Generator>
    result_type operator()(Generator& generator, const param_type& params)
    {
        if (params.k_ <= IntType(0) ||
            !__detail::__random_parameter_finite(params.p_) ||
            params.p_ <= 0.0 || params.p_ > 1.0) {
            __detail::__random_invalid_parameter();
        }
        if (params.p_ >= 1.0) return result_type(0);

        using unsigned_type = typename make_unsigned<result_type>::type;
        const unsigned_type successes =
            static_cast<unsigned_type>(params.k_);
        if (successes > 32u) {
            return sample_gamma_poisson(generator, successes, params.p_);
        }

        const unsigned_type maximum = static_cast<unsigned_type>(
            numeric_limits<result_type>::max());
        geometric_distribution<result_type> geometric(params.p_);

        for (;;) {
            unsigned_type total = 0u;
            bool overflow = false;
            for (unsigned_type remaining = successes;
                 remaining != 0u; --remaining) {
                const unsigned_type failures =
                    static_cast<unsigned_type>(geometric(generator));
                if (failures > maximum - total) {
                    overflow = true;
                    break;
                }
                total = static_cast<unsigned_type>(total + failures);
            }
            if (!overflow) return static_cast<result_type>(total);
        }
    }

private:
    template<typename Generator>
    static double sample_standard_normal(Generator& generator)
    {
        for (;;) {
            const double first = 2.0 * generate_canonical<
                double, static_cast<size_t>(numeric_limits<double>::digits)>(
                    generator) - 1.0;
            const double second = 2.0 * generate_canonical<
                double, static_cast<size_t>(numeric_limits<double>::digits)>(
                    generator) - 1.0;
            const double radius = first * first + second * second;
            if (radius > 0.0 && radius < 1.0) {
                return first * __detail::__random_sqrt(
                    -2.0 * __detail::__random_log(radius) / radius);
            }
        }
    }

    template<typename Generator, typename UnsignedType>
    static result_type sample_gamma_poisson(Generator& generator,
                                             UnsignedType successes,
                                             double probability)
    {
        const double shape = static_cast<double>(successes);
        const double adjusted_shape = shape - 1.0 / 3.0;
        const double coefficient =
            1.0 / __detail::__random_sqrt(9.0 * adjusted_shape);
        const double scale = (1.0 - probability) / probability;
        const long double maximum = static_cast<long double>(
            numeric_limits<result_type>::max());

        for (;;) {
            const double normal = sample_standard_normal(generator);
            const double root = 1.0 + coefficient * normal;
            if (!(root > 0.0)) continue;
            const double root_squared = root * root;
            const double volume = root_squared * root;
            const double uniform = generate_canonical<
                double, static_cast<size_t>(numeric_limits<double>::digits)>(
                    generator);
            const double fourth_power = normal * normal * normal * normal;
            if (!(uniform < 1.0 - 0.0331 * fourth_power) &&
                !(__detail::__random_log(uniform) <
                  0.5 * normal * normal + adjusted_shape *
                      (1.0 - volume + __detail::__random_log(volume)))) {
                continue;
            }

            const double rate = adjusted_shape * volume * scale;
            if (!(rate >= 0.0) ||
                static_cast<long double>(rate) > maximum) {
                continue;
            }
            if (rate == 0.0) return result_type(0);
            poisson_distribution<result_type> poisson(rate);
            return poisson(generator);
        }
    }

public:

    friend bool operator==(const negative_binomial_distribution& lhs,
                           const negative_binomial_distribution& rhs)
    {
        return lhs.k_ == rhs.k_ && lhs.p_ == rhs.p_;
    }

    friend bool operator!=(const negative_binomial_distribution& lhs,
                           const negative_binomial_distribution& rhs)
    {
        return !(lhs == rhs);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * binomial_distribution
 * ═══════════════════════════════════════════════════════════════*/

template<typename IntType = int>
class binomial_distribution {
    static_assert(is_integral<IntType>::value, "IntType must be integral");

    IntType t_;
    double p_;

public:
    using result_type = IntType;

    struct param_type {
        using distribution_type = binomial_distribution;

        IntType t_;
        double p_;
        explicit param_type(IntType t = 1, double p = 0.5) : t_(t), p_(p) {}
        IntType t() const { return t_; }
        double p() const { return p_; }

        friend bool operator==(const param_type& lhs, const param_type& rhs)
        {
            return lhs.t_ == rhs.t_ && lhs.p_ == rhs.p_;
        }

        friend bool operator!=(const param_type& lhs, const param_type& rhs)
        {
            return !(lhs == rhs);
        }
    };

    binomial_distribution() : t_(1), p_(0.5) {}
    explicit binomial_distribution(IntType t, double p = 0.5)
        : t_(t), p_(p) {}
    explicit binomial_distribution(const param_type& params)
        : t_(params.t_), p_(params.p_) {}

    void reset() {}

    IntType t() const { return t_; }
    double p() const { return p_; }
    param_type param() const { return param_type(t_, p_); }
    void param(const param_type& params) { t_ = params.t_; p_ = params.p_; }

    result_type min() const { return result_type(0); }
    result_type max() const { return t_; }

    template<typename Generator>
    result_type operator()(Generator& generator)
    {
        return (*this)(generator, param());
    }

    template<typename Generator>
    result_type operator()(Generator& generator, const param_type& params)
    {
        if (params.t_ < IntType(0) ||
            !__detail::__random_parameter_finite(params.p_) ||
            params.p_ < 0.0 || params.p_ > 1.0) {
            __detail::__random_invalid_parameter();
        }
        if (params.p_ <= 0.0 || params.t_ == result_type(0)) {
            return result_type(0);
        }
        if (params.p_ >= 1.0) return params.t_;

        using unsigned_type = typename make_unsigned<result_type>::type;
        const unsigned_type trials = static_cast<unsigned_type>(params.t_);
        const bool complement = params.p_ > 0.5;
        const double smaller_probability =
            complement ? 1.0 - params.p_ : params.p_;
        const double expected = static_cast<double>(trials) *
            smaller_probability;
        if (expected <= 30.0) {
            return sample_inversion(generator, trials, smaller_probability,
                                    complement);
        }
        return sample_transformed_rejection(
            generator, trials, smaller_probability, complement);
    }

private:
    template<typename Generator, typename UnsignedType>
    static result_type sample_inversion(Generator& generator,
                                        UnsignedType trials,
                                        double probability,
                                        bool complement)
    {
        const double opposite = 1.0 - probability;
        const double expected = static_cast<double>(trials) * probability;
        const double initial_mass = __detail::__random_exp(
            static_cast<double>(trials) *
            __detail::__random_log1p(-probability));
        const double raw_bound = expected + 10.0 * __detail::__random_sqrt(
            expected * opposite + 1.0);
        UnsignedType bound = static_cast<UnsignedType>(raw_bound);
        if (bound > trials) bound = trials;

        for (;;) {
            double uniform = generate_canonical<
                double, static_cast<size_t>(numeric_limits<double>::digits)>(
                    generator);
            double mass = initial_mass;
            UnsignedType value = 0u;
            for (;;) {
                if (uniform <= mass) {
                    return static_cast<result_type>(
                        complement ? trials - value : value);
                }
                if (value == bound) break;
                uniform -= mass;
                ++value;
                mass *= static_cast<double>(trials - value + 1u) *
                    probability / (static_cast<double>(value) * opposite);
            }
        }
    }

    template<typename Generator, typename UnsignedType>
    static result_type sample_transformed_rejection(Generator& generator,
                                                     UnsignedType trials,
                                                     double probability,
                                                     bool complement)
    {
        const double opposite = 1.0 - probability;
        const double expected = static_cast<double>(trials) * probability;
        const double center = expected + probability;
        const UnsignedType mode = static_cast<UnsignedType>(center);
        const double half_width = static_cast<double>(mode) + 0.5;
        const double region_one = static_cast<double>(static_cast<long long>(
            2.195 * __detail::__random_sqrt(expected * opposite) -
            4.6 * opposite)) + 0.5;
        const double left = half_width - region_one;
        const double right = half_width + region_one;
        const double center_scale = 0.134 +
            20.5 / (15.3 + static_cast<double>(mode));
        double tail = (center - left) /
            (center - left * probability);
        const double left_rate = tail * (1.0 + 0.5 * tail);
        tail = (right - center) / (right * opposite);
        const double right_rate = tail * (1.0 + 0.5 * tail);
        const double region_two = region_one * (1.0 + 2.0 * center_scale);
        const double region_three = region_two + center_scale / left_rate;
        const double region_four = region_three + center_scale / right_rate;
        const long double trial_limit = static_cast<long double>(trials);

        for (;;) {
            const double region = generate_canonical<
                double, static_cast<size_t>(numeric_limits<double>::digits)>(
                    generator) * region_four;
            double uniform = generate_canonical<
                double, static_cast<size_t>(numeric_limits<double>::digits)>(
                    generator);
            double candidate;
            if (region <= region_one) {
                candidate = half_width - region_one * uniform + region;
                if (!(candidate >= 0.0) ||
                    static_cast<long double>(candidate) > trial_limit) {
                    continue;
                }
                const UnsignedType value =
                    static_cast<UnsignedType>(candidate);
                return static_cast<result_type>(
                    complement ? trials - value : value);
            }
            if (region <= region_two) {
                const double position = left +
                    (region - region_one) / center_scale;
                const double offset =
                    static_cast<double>(mode) - position + 0.5;
                const double absolute_offset = offset < 0.0 ? -offset : offset;
                uniform = uniform * center_scale + 1.0 -
                    absolute_offset / region_one;
                if (!(uniform > 0.0 && uniform <= 1.0)) continue;
                candidate = position;
            } else if (region <= region_three) {
                if (!(uniform > 0.0)) continue;
                candidate = left + __detail::__random_log(uniform) /
                    left_rate;
                if (!(candidate >= 0.0)) continue;
                uniform *= (region - region_two) * left_rate;
            } else {
                if (!(uniform > 0.0)) continue;
                candidate = right - __detail::__random_log(uniform) /
                    right_rate;
                if (static_cast<long double>(candidate) > trial_limit)
                    continue;
                uniform *= (region - region_three) * right_rate;
            }

            if (!(candidate >= 0.0) ||
                static_cast<long double>(candidate) > trial_limit) {
                continue;
            }
            const UnsignedType value = static_cast<UnsignedType>(candidate);
            const UnsignedType distance = value < mode
                ? mode - value : value - mode;
            if (distance <= 20u) {
                const double odds = probability / opposite;
                const double scaled_trials =
                    odds * (static_cast<double>(trials) + 1.0);
                double mass_ratio = 1.0;
                UnsignedType position;
                if (mode < value) {
                    position = mode;
                    while (position < value) {
                        ++position;
                        mass_ratio *= scaled_trials /
                            static_cast<double>(position) - odds;
                    }
                } else {
                    position = value;
                    while (position < mode) {
                        ++position;
                        mass_ratio /= scaled_trials /
                            static_cast<double>(position) - odds;
                    }
                }
                if (uniform <= mass_ratio) {
                    return static_cast<result_type>(
                        complement ? trials - value : value);
                }
                continue;
            }

            const double value_plus_one = static_cast<double>(value) + 1.0;
            const double mode_plus_one = static_cast<double>(mode) + 1.0;
            const double failures_at_mode =
                static_cast<double>(trials - mode) + 1.0;
            const double failures_at_value =
                static_cast<double>(trials - value) + 1.0;
            const double signed_distance = value < mode
                ? -static_cast<double>(mode - value)
                : static_cast<double>(value - mode);
            const double log_ratio =
                half_width * __detail::__random_log(
                    mode_plus_one / value_plus_one) +
                (static_cast<double>(trials - mode) + 0.5) *
                    __detail::__random_log(
                        failures_at_mode / failures_at_value) +
                signed_distance * __detail::__random_log(
                    failures_at_value * probability /
                    (value_plus_one * opposite)) +
                stirling_correction(mode_plus_one) +
                stirling_correction(failures_at_mode) -
                stirling_correction(value_plus_one) -
                stirling_correction(failures_at_value);
            if (__detail::__random_log(uniform) <= log_ratio) {
                return static_cast<result_type>(
                    complement ? trials - value : value);
            }
        }
    }

    static double stirling_correction(double value)
    {
        const double squared = value * value;
        return (13860.0 - (462.0 - (132.0 -
            (99.0 - 140.0 / squared) / squared) / squared) / squared) /
            value / 166320.0;
    }

public:

    friend bool operator==(const binomial_distribution& lhs,
                           const binomial_distribution& rhs)
    {
        return lhs.t_ == rhs.t_ && lhs.p_ == rhs.p_;
    }

    friend bool operator!=(const binomial_distribution& lhs,
                           const binomial_distribution& rhs)
    {
        return !(lhs == rhs);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * poisson_distribution
 * ═══════════════════════════════════════════════════════════════*/

template<typename IntType = int>
class poisson_distribution {
    static_assert(is_integral<IntType>::value,
                  "IntType must be integral");

    double mean_;

public:
    using result_type = IntType;

    struct param_type {
        using distribution_type = poisson_distribution;

        double mean_;
        explicit param_type(double mean = 1.0) : mean_(mean) {}
        double mean() const { return mean_; }

        friend bool operator==(const param_type& lhs, const param_type& rhs)
        { return lhs.mean_ == rhs.mean_; }
        friend bool operator!=(const param_type& lhs, const param_type& rhs)
        { return !(lhs == rhs); }
    };

    explicit poisson_distribution(double mean = 1.0) : mean_(mean) {}
    explicit poisson_distribution(const param_type& params)
        : mean_(params.mean_) {}

    void reset() {}
    double mean() const { return mean_; }
    param_type param() const { return param_type(mean_); }
    void param(const param_type& params) { mean_ = params.mean_; }
    result_type min() const { return result_type(0); }
    result_type max() const { return numeric_limits<result_type>::max(); }

    template<typename Generator>
    result_type operator()(Generator& generator)
    { return (*this)(generator, param()); }

    template<typename Generator>
    result_type operator()(Generator& generator, const param_type& params) {
        if (!__detail::__random_parameter_finite(params.mean_) ||
            params.mean_ <= 0.0) {
            __detail::__random_invalid_parameter();
        }
        if (params.mean_ < 10.0) return sample_product(generator, params.mean_);
        return sample_transformed_rejection(generator, params.mean_);
    }

    friend bool operator==(const poisson_distribution& lhs,
                           const poisson_distribution& rhs)
    { return lhs.mean_ == rhs.mean_; }
    friend bool operator!=(const poisson_distribution& lhs,
                           const poisson_distribution& rhs)
    { return !(lhs == rhs); }

private:
    template<typename Generator>
    static result_type sample_product(Generator& generator, double mean) {
        const double threshold = __detail::__random_exp(-mean);
        const unsigned long long result_max =
            static_cast<unsigned long long>(numeric_limits<result_type>::max());
        for (;;) {
            double product = 1.0;
            unsigned long long value = 0;
            for (;;) {
                const double unit = generate_canonical<
                    double, static_cast<size_t>(numeric_limits<double>::digits)>(
                        generator);
                product *= unit;
                if (!(product > threshold))
                    return static_cast<result_type>(value);
                if (value == result_max) break;
                ++value;
            }
        }
    }

    template<typename Generator>
    static result_type sample_transformed_rejection(Generator& generator,
                                                     double mean) {
        const double root = __detail::__random_sqrt(mean);
        const double log_mean = __detail::__random_log(mean);
        const double b = 0.931 + 2.53 * root;
        const double a = -0.059 + 0.02483 * b;
        const double inverse_alpha = 1.1239 + 1.1328 / (b - 3.4);
        const double fast_threshold = 0.9277 - 3.6224 / (b - 2.0);
        const long double result_max = static_cast<long double>(
            numeric_limits<result_type>::max());
        for (;;) {
            const double centered = generate_canonical<
                double, static_cast<size_t>(numeric_limits<double>::digits)>(
                    generator) - 0.5;
            const double uniform = generate_canonical<
                double, static_cast<size_t>(numeric_limits<double>::digits)>(
                    generator);
            const double absolute = centered < 0.0 ? -centered : centered;
            const double squeeze = 0.5 - absolute;
            if (!(squeeze > 0.0)) continue;
            const double candidate =
                (2.0 * a / squeeze + b) * centered + mean + 0.43;
            if (!(candidate >= 0.0) ||
                static_cast<long double>(candidate) > result_max)
                continue;
            const result_type value = static_cast<result_type>(candidate);
            if (squeeze >= 0.07 && uniform <= fast_threshold) return value;
            if (squeeze < 0.013 && uniform > squeeze) continue;
            const double integer = static_cast<double>(value);
            const double left = __detail::__random_log(uniform) +
                __detail::__random_log(inverse_alpha) -
                __detail::__random_log(a / (squeeze * squeeze) + b);
            const double right = -mean + integer * log_mean -
                __detail::__random_log_gamma_integer(integer + 1.0);
            if (left <= right) return value;
        }
    }
};

/* ═══════════════════════════════════════════════════════════════
 * exponential_distribution
 * ═══════════════════════════════════════════════════════════════*/

template<typename RealType = double>
class exponential_distribution {
    static_assert(is_floating_point<RealType>::value,
                  "RealType must be floating point");

    RealType lambda_;

public:
    using result_type = RealType;

    struct param_type {
        using distribution_type = exponential_distribution;

        RealType lambda_;
        explicit param_type(RealType lambda = RealType(1))
            : lambda_(lambda) {}
        RealType lambda() const { return lambda_; }

        friend bool operator==(const param_type& lhs, const param_type& rhs)
        {
            return lhs.lambda_ == rhs.lambda_;
        }

        friend bool operator!=(const param_type& lhs, const param_type& rhs)
        {
            return !(lhs == rhs);
        }
    };

    explicit exponential_distribution(RealType lambda = RealType(1))
        : lambda_(lambda) {}
    explicit exponential_distribution(const param_type& params)
        : lambda_(params.lambda_) {}

    void reset() {}

    RealType lambda() const { return lambda_; }
    param_type param() const { return param_type(lambda_); }
    void param(const param_type& params) { lambda_ = params.lambda_; }

    result_type min() const { return RealType(0); }
    result_type max() const { return numeric_limits<RealType>::max(); }

    template<typename Generator>
    result_type operator()(Generator& generator)
    {
        return (*this)(generator, param());
    }

    template<typename Generator>
    result_type operator()(Generator& generator, const param_type& params)
    {
        if (!__detail::__random_real_parameter_finite(params.lambda_) ||
            params.lambda_ <= RealType(0)) {
            __detail::__random_invalid_parameter();
        }
        const RealType unit = generate_canonical<
            RealType, static_cast<size_t>(numeric_limits<RealType>::digits)>(
                generator);
        return -__detail::__random_log1p(-unit) / params.lambda_;
    }

    friend bool operator==(const exponential_distribution& lhs,
                           const exponential_distribution& rhs)
    {
        return lhs.lambda_ == rhs.lambda_;
    }

    friend bool operator!=(const exponential_distribution& lhs,
                           const exponential_distribution& rhs)
    {
        return !(lhs == rhs);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * weibull_distribution
 * ═══════════════════════════════════════════════════════════════*/

template<typename RealType = double>
class weibull_distribution {
    static_assert(is_floating_point<RealType>::value,
                  "RealType must be floating point");

    RealType a_;
    RealType b_;

public:
    using result_type = RealType;

    struct param_type {
        using distribution_type = weibull_distribution;

        RealType a_;
        RealType b_;
        explicit param_type(RealType a = RealType(1),
                            RealType b = RealType(1))
            : a_(a), b_(b) {}
        RealType a() const { return a_; }
        RealType b() const { return b_; }

        friend bool operator==(const param_type& lhs, const param_type& rhs)
        {
            return lhs.a_ == rhs.a_ && lhs.b_ == rhs.b_;
        }

        friend bool operator!=(const param_type& lhs, const param_type& rhs)
        {
            return !(lhs == rhs);
        }
    };

    explicit weibull_distribution(RealType a = RealType(1),
                                  RealType b = RealType(1))
        : a_(a), b_(b) {}
    explicit weibull_distribution(const param_type& params)
        : a_(params.a_), b_(params.b_) {}

    void reset() {}

    RealType a() const { return a_; }
    RealType b() const { return b_; }
    param_type param() const { return param_type(a_, b_); }
    void param(const param_type& params) { a_ = params.a_; b_ = params.b_; }

    result_type min() const { return RealType(0); }
    result_type max() const { return numeric_limits<RealType>::max(); }

    template<typename Generator>
    result_type operator()(Generator& generator)
    {
        return (*this)(generator, param());
    }

    template<typename Generator>
    result_type operator()(Generator& generator, const param_type& params)
    {
        if (!__detail::__random_real_parameter_finite(params.a_) ||
            !__detail::__random_real_parameter_finite(params.b_) ||
            params.a_ <= RealType(0) || params.b_ <= RealType(0)) {
            __detail::__random_invalid_parameter();
        }
        const RealType unit = generate_canonical<
            RealType, static_cast<size_t>(numeric_limits<RealType>::digits)>(
                generator);
        const RealType exponential = -__detail::__random_log1p(-unit);
        if (params.a_ == RealType(1)) return params.b_ * exponential;
        if (exponential == RealType(0)) return RealType(0);
        return params.b_ * __detail::__random_exp(
            __detail::__random_log(exponential) / params.a_);
    }

    friend bool operator==(const weibull_distribution& lhs,
                           const weibull_distribution& rhs)
    {
        return lhs.a_ == rhs.a_ && lhs.b_ == rhs.b_;
    }

    friend bool operator!=(const weibull_distribution& lhs,
                           const weibull_distribution& rhs)
    {
        return !(lhs == rhs);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * extreme_value_distribution
 * ═══════════════════════════════════════════════════════════════*/

template<typename RealType = double>
class extreme_value_distribution {
    static_assert(is_floating_point<RealType>::value,
                  "RealType must be floating point");

    RealType a_;
    RealType b_;

public:
    using result_type = RealType;

    struct param_type {
        using distribution_type = extreme_value_distribution;

        RealType a_;
        RealType b_;
        explicit param_type(RealType a = RealType(0),
                            RealType b = RealType(1))
            : a_(a), b_(b) {}
        RealType a() const { return a_; }
        RealType b() const { return b_; }

        friend bool operator==(const param_type& lhs, const param_type& rhs)
        {
            return lhs.a_ == rhs.a_ && lhs.b_ == rhs.b_;
        }

        friend bool operator!=(const param_type& lhs, const param_type& rhs)
        {
            return !(lhs == rhs);
        }
    };

    explicit extreme_value_distribution(RealType a = RealType(0),
                                         RealType b = RealType(1))
        : a_(a), b_(b) {}
    explicit extreme_value_distribution(const param_type& params)
        : a_(params.a_), b_(params.b_) {}

    void reset() {}

    RealType a() const { return a_; }
    RealType b() const { return b_; }
    param_type param() const { return param_type(a_, b_); }
    void param(const param_type& params) { a_ = params.a_; b_ = params.b_; }

    result_type min() const { return numeric_limits<RealType>::lowest(); }
    result_type max() const { return numeric_limits<RealType>::max(); }

    template<typename Generator>
    result_type operator()(Generator& generator)
    {
        return (*this)(generator, param());
    }

    template<typename Generator>
    result_type operator()(Generator& generator, const param_type& params)
    {
        if (!__detail::__random_real_parameter_finite(params.a_) ||
            !__detail::__random_real_parameter_finite(params.b_) ||
            params.b_ <= RealType(0)) {
            __detail::__random_invalid_parameter();
        }
        RealType unit;
        do {
            unit = generate_canonical<
                RealType,
                static_cast<size_t>(numeric_limits<RealType>::digits)>(
                    generator);
        } while (unit == RealType(0));
        const RealType exponential = -__detail::__random_log1p(-unit);
        return params.a_ - params.b_ * __detail::__random_log(exponential);
    }

    friend bool operator==(const extreme_value_distribution& lhs,
                           const extreme_value_distribution& rhs)
    {
        return lhs.a_ == rhs.a_ && lhs.b_ == rhs.b_;
    }

    friend bool operator!=(const extreme_value_distribution& lhs,
                           const extreme_value_distribution& rhs)
    {
        return !(lhs == rhs);
    }
};

template<typename RealType = double>
class cauchy_distribution {
    static_assert(is_floating_point<RealType>::value,
                  "RealType must be floating point");
    RealType a_, b_;
public:
    using result_type = RealType;
    struct param_type {
        using distribution_type = cauchy_distribution;
        RealType a_, b_;
        explicit param_type(RealType a = RealType(0), RealType b = RealType(1))
            : a_(a), b_(b) {}
        RealType a() const { return a_; } RealType b() const { return b_; }
        friend bool operator==(const param_type& x, const param_type& y)
        { return x.a_ == y.a_ && x.b_ == y.b_; }
        friend bool operator!=(const param_type& x, const param_type& y)
        { return !(x == y); }
    };
    explicit cauchy_distribution(RealType a = RealType(0),
                                 RealType b = RealType(1)) : a_(a), b_(b) {}
    explicit cauchy_distribution(const param_type& p) : a_(p.a_), b_(p.b_) {}
    void reset() {} RealType a() const { return a_; } RealType b() const { return b_; }
    param_type param() const { return param_type(a_, b_); }
    void param(const param_type& p) { a_ = p.a_; b_ = p.b_; }
    result_type min() const { return numeric_limits<RealType>::lowest(); }
    result_type max() const { return numeric_limits<RealType>::max(); }
    template<typename G> result_type operator()(G& g) { return (*this)(g,param()); }
    template<typename G> result_type operator()(G& g,const param_type& p) {
        if (!__detail::__random_real_parameter_finite(p.a_) ||
            !__detail::__random_real_parameter_finite(p.b_) ||
            p.b_ <= RealType(0)) {
            __detail::__random_invalid_parameter();
        }
        RealType u; do { u=generate_canonical<RealType,
            static_cast<size_t>(numeric_limits<RealType>::digits)>(g);
        } while(u==RealType(0.5));
        const RealType pi=RealType(3.1415926535897932384626433832795029L);
        return p.a_+p.b_*__detail::__random_tan(pi*u);
    }
    friend bool operator==(const cauchy_distribution& x,const cauchy_distribution& y)
    { return x.a_==y.a_&&x.b_==y.b_; }
    friend bool operator!=(const cauchy_distribution& x,const cauchy_distribution& y)
    { return !(x==y); }
    template<typename C,typename T> friend basic_ostream<C,T>& operator<<(
        basic_ostream<C,T>&s,const cauchy_distribution&d){using S=basic_ostream<C,T>;
        auto f=s.flags();C fill=s.fill();streamsize pr=s.precision();C sp=static_cast<C>(' ');
        s.flags(S::scientific|S::left);s.fill(sp);s.precision(numeric_limits<RealType>::max_digits10);
        s<<d.a_<<sp<<d.b_;s.flags(f);s.fill(fill);s.precision(pr);return s;}
    template<typename C,typename T> friend basic_istream<C,T>& operator>>(
        basic_istream<C,T>&s,cauchy_distribution&d){using S=basic_istream<C,T>;
        auto f=s.flags();s.flags(S::skipws);RealType a=0,b=0;
        const bool parsed_a = __detail::__random_read_required(s, a);
        const bool parsed_b = parsed_a &&
            __detail::__random_read_required(s, b);
        if(parsed_b&&!s.fail()&&b>0){d.a_=a;d.b_=b;}else if(!s.fail())s.setstate(S::failbit);
        s.flags(f);return s;}
};

/* ═══════════════════════════════════════════════════════════════
 * normal_distribution (Box-Muller)
 * ═══════════════════════════════════════════════════════════════*/

template<typename RealType = double>
class normal_distribution {
    static_assert(is_floating_point<RealType>::value,
                  "RealType must be floating point");

    RealType mean_, stddev_;
    bool has_spare_;
    RealType spare_;

    template<typename Generator>
    RealType __generate(Generator& g, RealType mean, RealType stddev)
    {
        if (!__detail::__random_real_parameter_finite(mean) ||
            !__detail::__random_real_parameter_finite(stddev) ||
            stddev <= RealType(0)) {
            __detail::__random_invalid_parameter();
        }
        if (has_spare_) {
            has_spare_ = false;
            return spare_ * stddev + mean;
        }

        uniform_real_distribution<RealType> dist(0.0, 1.0);
        RealType u;
        RealType v;
        RealType radius_squared;
        do {
            u = dist(g) * RealType(2) - RealType(1);
            v = dist(g) * RealType(2) - RealType(1);
            radius_squared = u * u + v * v;
        } while (radius_squared >= RealType(1) ||
                 radius_squared == RealType(0));

        const RealType multiplier = __detail::__random_sqrt(
            RealType(-2) * __detail::__random_log(radius_squared) /
            radius_squared);
        spare_ = v * multiplier;
        has_spare_ = true;
        return mean + stddev * u * multiplier;
    }

public:
    using result_type = RealType;

    struct param_type {
        using distribution_type = normal_distribution;

        RealType mean_, stddev_;
        param_type(RealType mean = 0.0, RealType stddev = 1.0)
            : mean_(mean), stddev_(stddev) {}
        RealType mean() const { return mean_; }
        RealType stddev() const { return stddev_; }

        friend bool operator==(const param_type& lhs, const param_type& rhs)
        {
            return lhs.mean_ == rhs.mean_ && lhs.stddev_ == rhs.stddev_;
        }

        friend bool operator!=(const param_type& lhs, const param_type& rhs)
        {
            return !(lhs == rhs);
        }
    };

    normal_distribution() : mean_(0.0), stddev_(1.0), has_spare_(false), spare_(0) {}
    explicit normal_distribution(RealType mean, RealType stddev = 1.0)
        : mean_(mean), stddev_(stddev), has_spare_(false), spare_(0) {}
    explicit normal_distribution(const param_type& p)
        : mean_(p.mean_), stddev_(p.stddev_), has_spare_(false), spare_(0) {}

    void reset() { has_spare_ = false; }

    RealType mean() const { return mean_; }
    RealType stddev() const { return stddev_; }

    param_type param() const { return param_type(mean_, stddev_); }
    void param(const param_type& p) { mean_ = p.mean_; stddev_ = p.stddev_; }

    result_type min() const { return -numeric_limits<RealType>::infinity(); }
    result_type max() const { return numeric_limits<RealType>::infinity(); }

    template<typename Generator>
    result_type operator()(Generator& g)
    {
        return __generate(g, mean_, stddev_);
    }

    template<typename Generator>
    result_type operator()(Generator& g, const param_type& p)
    {
        if (!__detail::__random_real_parameter_finite(p.mean_) ||
            !__detail::__random_real_parameter_finite(p.stddev_) ||
            p.stddev_ <= RealType(0)) {
            __detail::__random_invalid_parameter();
        }
        return __generate(g, p.mean_, p.stddev_);
    }

    friend bool operator==(const normal_distribution& lhs,
                           const normal_distribution& rhs)
    {
        return lhs.mean_ == rhs.mean_ &&
               lhs.stddev_ == rhs.stddev_ &&
               lhs.has_spare_ == rhs.has_spare_ &&
               (!lhs.has_spare_ || lhs.spare_ == rhs.spare_);
    }

    friend bool operator!=(const normal_distribution& lhs,
                           const normal_distribution& rhs)
    {
        return !(lhs == rhs);
    }

    template<typename CharT, typename Traits>
    friend basic_ostream<CharT, Traits>&
    operator<<(basic_ostream<CharT, Traits>& stream,
               const normal_distribution& distribution)
    {
        using stream_type = basic_ostream<CharT, Traits>;
        const typename stream_type::fmtflags flags = stream.flags();
        const CharT fill = stream.fill();
        const streamsize precision = stream.precision();
        const CharT space = static_cast<CharT>(' ');
        stream.flags(stream_type::scientific | stream_type::left);
        stream.fill(space);
        stream.precision(numeric_limits<RealType>::max_digits10);
        stream << distribution.mean_ << space << distribution.stddev_
               << space << distribution.has_spare_;
        if (distribution.has_spare_) {
            stream << space << distribution.spare_;
        }
        stream.flags(flags);
        stream.fill(fill);
        stream.precision(precision);
        return stream;
    }

    template<typename CharT, typename Traits>
    friend basic_istream<CharT, Traits>&
    operator>>(basic_istream<CharT, Traits>& stream,
               normal_distribution& distribution)
    {
        using stream_type = basic_istream<CharT, Traits>;
        const typename stream_type::fmtflags flags = stream.flags();
        stream.flags(stream_type::dec | stream_type::skipws);
        RealType candidate_mean = RealType(0);
        RealType candidate_stddev = RealType(0);
        bool candidate_has_spare = false;
        RealType candidate_spare = RealType(0);
        const bool parsed_mean = __detail::__random_read_required(
            stream, candidate_mean);
        const bool parsed_stddev = parsed_mean &&
            __detail::__random_read_required(stream, candidate_stddev);
        const bool parsed_has_spare = parsed_stddev &&
            __detail::__random_read_required(stream, candidate_has_spare);
        if (parsed_has_spare && candidate_has_spare) {
            (void)__detail::__random_read_required(stream, candidate_spare);
        }
        if (!stream.fail() && parsed_has_spare && candidate_stddev > RealType(0)) {
            distribution.mean_ = candidate_mean;
            distribution.stddev_ = candidate_stddev;
            distribution.has_spare_ = candidate_has_spare;
            distribution.spare_ = candidate_spare;
        } else if (!stream.fail()) {
            stream.setstate(stream_type::failbit);
        }
        stream.flags(flags);
        return stream;
    }
};

/* ═══════════════════════════════════════════════════════════════
 * lognormal_distribution
 * ═══════════════════════════════════════════════════════════════*/

template<typename RealType = double>
class lognormal_distribution {
    static_assert(is_floating_point<RealType>::value,
                  "RealType must be floating point");

    normal_distribution<RealType> normal_;

public:
    using result_type = RealType;

    struct param_type {
        using distribution_type = lognormal_distribution;

        RealType m_;
        RealType s_;
        explicit param_type(RealType m = RealType(0),
                            RealType s = RealType(1))
            : m_(m), s_(s) {}
        RealType m() const { return m_; }
        RealType s() const { return s_; }

        friend bool operator==(const param_type& lhs, const param_type& rhs)
        {
            return lhs.m_ == rhs.m_ && lhs.s_ == rhs.s_;
        }

        friend bool operator!=(const param_type& lhs, const param_type& rhs)
        {
            return !(lhs == rhs);
        }
    };

    explicit lognormal_distribution(RealType m = RealType(0),
                                    RealType s = RealType(1))
        : normal_(m, s) {}
    explicit lognormal_distribution(const param_type& params)
        : normal_(params.m_, params.s_) {}

    void reset() { normal_.reset(); }

    RealType m() const { return normal_.mean(); }
    RealType s() const { return normal_.stddev(); }
    param_type param() const { return param_type(m(), s()); }
    void param(const param_type& params)
    {
        normal_.param(typename normal_distribution<RealType>::param_type(
            params.m_, params.s_));
    }

    result_type min() const { return RealType(0); }
    result_type max() const { return numeric_limits<RealType>::max(); }

    template<typename Generator>
    result_type operator()(Generator& generator)
    {
        return __detail::__random_exp(normal_(generator));
    }

    template<typename Generator>
    result_type operator()(Generator& generator, const param_type& params)
    {
        return __detail::__random_exp(normal_(
            generator,
            typename normal_distribution<RealType>::param_type(
                params.m_, params.s_)));
    }

    friend bool operator==(const lognormal_distribution& lhs,
                           const lognormal_distribution& rhs)
    {
        return lhs.normal_ == rhs.normal_;
    }

    friend bool operator!=(const lognormal_distribution& lhs,
                           const lognormal_distribution& rhs)
    {
        return !(lhs == rhs);
    }

    template<typename CharT, typename Traits>
    friend basic_ostream<CharT, Traits>&
    operator<<(basic_ostream<CharT, Traits>& stream,
               const lognormal_distribution& distribution)
    {
        return stream << distribution.normal_;
    }

    template<typename CharT, typename Traits>
    friend basic_istream<CharT, Traits>&
    operator>>(basic_istream<CharT, Traits>& stream,
               lognormal_distribution& distribution)
    {
        normal_distribution<RealType> candidate;
        stream >> candidate;
        if (!stream.fail()) distribution.normal_ = candidate;
        return stream;
    }
};

/* ═══════════════════════════════════════════════════════════════
 * gamma_distribution (Marsaglia-Tsang)
 * ═══════════════════════════════════════════════════════════════*/

template<typename RealType = double>
class gamma_distribution {
    static_assert(is_floating_point<RealType>::value,
                  "RealType must be floating point");

    RealType alpha_;
    RealType beta_;
    normal_distribution<RealType> normal_;

public:
    using result_type = RealType;

    struct param_type {
        using distribution_type = gamma_distribution;

        RealType alpha_;
        RealType beta_;
        explicit param_type(RealType alpha = RealType(1),
                            RealType beta = RealType(1))
            : alpha_(alpha), beta_(beta) {}
        RealType alpha() const { return alpha_; }
        RealType beta() const { return beta_; }

        friend bool operator==(const param_type& lhs, const param_type& rhs)
        {
            return lhs.alpha_ == rhs.alpha_ && lhs.beta_ == rhs.beta_;
        }

        friend bool operator!=(const param_type& lhs, const param_type& rhs)
        {
            return !(lhs == rhs);
        }
    };

    explicit gamma_distribution(RealType alpha = RealType(1),
                                RealType beta = RealType(1))
        : alpha_(alpha), beta_(beta), normal_() {}
    explicit gamma_distribution(const param_type& params)
        : alpha_(params.alpha_), beta_(params.beta_), normal_() {}

    void reset() { normal_.reset(); }

    RealType alpha() const { return alpha_; }
    RealType beta() const { return beta_; }
    param_type param() const { return param_type(alpha_, beta_); }
    void param(const param_type& params)
    {
        alpha_ = params.alpha_;
        beta_ = params.beta_;
    }

    result_type min() const { return RealType(0); }
    result_type max() const { return numeric_limits<RealType>::max(); }

    template<typename Generator>
    result_type operator()(Generator& generator)
    {
        return (*this)(generator, param());
    }

    template<typename Generator>
    result_type operator()(Generator& generator, const param_type& params)
    {
        if (!__detail::__random_real_parameter_finite(params.alpha_) ||
            !__detail::__random_real_parameter_finite(params.beta_) ||
            params.alpha_ <= RealType(0) || params.beta_ <= RealType(0)) {
            __detail::__random_invalid_parameter();
        }
        const RealType modified_alpha = params.alpha_ < RealType(1)
            ? params.alpha_ + RealType(1) : params.alpha_;
        const RealType a1 = modified_alpha - RealType(1) / RealType(3);
        const RealType a2 = RealType(1) /
            __detail::__random_sqrt(RealType(9) * a1);
        RealType unit;
        RealType normal;
        RealType value;
        do {
            do {
                normal = normal_(generator);
                value = RealType(1) + a2 * normal;
            } while (value <= RealType(0));
            value = value * value * value;
            unit = generate_canonical<
                RealType,
                static_cast<size_t>(numeric_limits<RealType>::digits)>(
                    generator);
        } while (unit > RealType(1) - RealType(0.0331) * normal * normal *
                                      normal * normal &&
                 __detail::__random_log(unit) >
                     RealType(0.5) * normal * normal +
                     a1 * (RealType(1) - value +
                           __detail::__random_log(value)));

        RealType result = a1 * value * params.beta_;
        if (params.alpha_ < RealType(1)) {
            do {
                unit = generate_canonical<
                    RealType,
                    static_cast<size_t>(numeric_limits<RealType>::digits)>(
                        generator);
            } while (unit == RealType(0));
            result *= __detail::__random_exp(
                __detail::__random_log(unit) / params.alpha_);
        }
        return result;
    }

    friend bool operator==(const gamma_distribution& lhs,
                           const gamma_distribution& rhs)
    {
        return lhs.alpha_ == rhs.alpha_ && lhs.beta_ == rhs.beta_ &&
               lhs.normal_ == rhs.normal_;
    }

    friend bool operator!=(const gamma_distribution& lhs,
                           const gamma_distribution& rhs)
    {
        return !(lhs == rhs);
    }

    template<typename CharT, typename Traits>
    friend basic_ostream<CharT, Traits>&
    operator<<(basic_ostream<CharT, Traits>& stream,
               const gamma_distribution& distribution)
    {
        using stream_type = basic_ostream<CharT, Traits>;
        const typename stream_type::fmtflags flags = stream.flags();
        const CharT fill = stream.fill();
        const streamsize precision = stream.precision();
        const CharT space = static_cast<CharT>(' ');
        stream.flags(stream_type::scientific | stream_type::left);
        stream.fill(space);
        stream.precision(numeric_limits<RealType>::max_digits10);
        stream << distribution.alpha_ << space << distribution.beta_
               << space << distribution.normal_;
        stream.flags(flags);
        stream.fill(fill);
        stream.precision(precision);
        return stream;
    }

    template<typename CharT, typename Traits>
    friend basic_istream<CharT, Traits>&
    operator>>(basic_istream<CharT, Traits>& stream,
               gamma_distribution& distribution)
    {
        using stream_type = basic_istream<CharT, Traits>;
        const typename stream_type::fmtflags flags = stream.flags();
        stream.flags(stream_type::dec | stream_type::skipws);
        RealType candidate_alpha = RealType(0);
        RealType candidate_beta = RealType(0);
        normal_distribution<RealType> candidate_normal;
        const bool parsed_alpha = __detail::__random_read_required(
            stream, candidate_alpha);
        const bool parsed_beta = parsed_alpha &&
            __detail::__random_read_required(stream, candidate_beta);
        const bool parsed_normal = parsed_beta &&
            __detail::__random_read_required(stream, candidate_normal);
        if (!stream.fail() && parsed_normal && candidate_alpha > RealType(0) &&
            candidate_beta > RealType(0)) {
            distribution.alpha_ = candidate_alpha;
            distribution.beta_ = candidate_beta;
            distribution.normal_ = candidate_normal;
        } else if (!stream.fail()) {
            stream.setstate(stream_type::failbit);
        }
        stream.flags(flags);
        return stream;
    }
};

template<typename RealType = double>
class chi_squared_distribution {
    static_assert(is_floating_point<RealType>::value,
                  "RealType must be floating point");
    RealType n_;
    gamma_distribution<RealType> gamma_;
public:
    using result_type = RealType;
    struct param_type {
        using distribution_type = chi_squared_distribution;
        RealType n_;
        explicit param_type(RealType n = RealType(1)) : n_(n) {}
        RealType n() const { return n_; }
        friend bool operator==(const param_type& a, const param_type& b)
        { return a.n_ == b.n_; }
        friend bool operator!=(const param_type& a, const param_type& b)
        { return !(a == b); }
    };
    explicit chi_squared_distribution(RealType n = RealType(1))
        : n_(n), gamma_(n / RealType(2)) {}
    explicit chi_squared_distribution(const param_type& p)
        : n_(p.n_), gamma_(p.n_ / RealType(2)) {}
    void reset() { gamma_.reset(); }
    RealType n() const { return n_; }
    param_type param() const { return param_type(n_); }
    void param(const param_type& p) {
        n_ = p.n_;
        gamma_.param(typename gamma_distribution<RealType>::param_type(
            n_ / RealType(2)));
    }
    result_type min() const { return RealType(0); }
    result_type max() const { return numeric_limits<RealType>::max(); }
    template<typename Generator> result_type operator()(Generator& g)
    { return RealType(2) * gamma_(g); }
    template<typename Generator>
    result_type operator()(Generator& g, const param_type& p) {
        return RealType(2) * gamma_(g,
            typename gamma_distribution<RealType>::param_type(
                p.n_ / RealType(2)));
    }
    friend bool operator==(const chi_squared_distribution& a,
                           const chi_squared_distribution& b)
    { return a.n_ == b.n_ && a.gamma_ == b.gamma_; }
    friend bool operator!=(const chi_squared_distribution& a,
                           const chi_squared_distribution& b)
    { return !(a == b); }
    template<typename CharT, typename Traits>
    friend basic_ostream<CharT, Traits>& operator<<(
        basic_ostream<CharT, Traits>& s, const chi_squared_distribution& d) {
        using st = basic_ostream<CharT, Traits>;
        const typename st::fmtflags f = s.flags();
        const CharT fill = s.fill(); const streamsize precision = s.precision();
        const CharT space = static_cast<CharT>(' ');
        s.flags(st::scientific | st::left); s.fill(space);
        s.precision(numeric_limits<RealType>::max_digits10);
        s << d.n_ << space << d.gamma_;
        s.flags(f); s.fill(fill); s.precision(precision); return s;
    }
    template<typename CharT, typename Traits>
    friend basic_istream<CharT, Traits>& operator>>(
        basic_istream<CharT, Traits>& s, chi_squared_distribution& d) {
        using st = basic_istream<CharT, Traits>;
        const typename st::fmtflags f = s.flags(); s.flags(st::dec | st::skipws);
        RealType n = RealType(0); gamma_distribution<RealType> gamma;
        const bool parsed_n = __detail::__random_read_required(s, n);
        const bool parsed_gamma = parsed_n &&
            __detail::__random_read_required(s, gamma);
        if (!s.fail() && parsed_gamma && n > RealType(0) &&
            gamma.alpha() == n / RealType(2) && gamma.beta() == RealType(1)) {
            d.n_ = n; d.gamma_ = gamma;
        } else if (!s.fail()) s.setstate(st::failbit);
        s.flags(f); return s;
    }
};

template<typename RealType = double>
class fisher_f_distribution {
    static_assert(is_floating_point<RealType>::value,
                  "RealType must be floating point");
    RealType m_, n_;
    gamma_distribution<RealType> x_, y_;
public:
    using result_type = RealType;
    struct param_type {
        using distribution_type = fisher_f_distribution;
        RealType m_, n_;
        explicit param_type(RealType m = RealType(1), RealType n = RealType(1))
            : m_(m), n_(n) {}
        RealType m() const { return m_; } RealType n() const { return n_; }
        friend bool operator==(const param_type& a, const param_type& b)
        { return a.m_ == b.m_ && a.n_ == b.n_; }
        friend bool operator!=(const param_type& a, const param_type& b)
        { return !(a == b); }
    };
    explicit fisher_f_distribution(RealType m = RealType(1),
                                   RealType n = RealType(1))
        : m_(m), n_(n), x_(m / RealType(2)), y_(n / RealType(2)) {}
    explicit fisher_f_distribution(const param_type& p)
        : fisher_f_distribution(p.m_, p.n_) {}
    void reset() { x_.reset(); y_.reset(); }
    RealType m() const { return m_; } RealType n() const { return n_; }
    param_type param() const { return param_type(m_, n_); }
    void param(const param_type& p) {
        m_ = p.m_; n_ = p.n_;
        x_.param(typename gamma_distribution<RealType>::param_type(m_/2));
        y_.param(typename gamma_distribution<RealType>::param_type(n_/2));
    }
    result_type min() const { return RealType(0); }
    result_type max() const { return numeric_limits<RealType>::max(); }
    template<typename G> result_type operator()(G& g) { return (*this)(g,param()); }
    template<typename G> result_type operator()(G& g, const param_type& p) {
        const RealType xv=x_(g,typename gamma_distribution<RealType>::param_type(p.m_/2));
        const RealType yv=y_(g,typename gamma_distribution<RealType>::param_type(p.n_/2));
        return (xv*p.n_)/(yv*p.m_);
    }
    friend bool operator==(const fisher_f_distribution& a,
                           const fisher_f_distribution& b)
    { return a.m_==b.m_ && a.n_==b.n_ && a.x_==b.x_ && a.y_==b.y_; }
    friend bool operator!=(const fisher_f_distribution& a,
                           const fisher_f_distribution& b) { return !(a==b); }
    template<typename C,typename T> friend basic_ostream<C,T>& operator<<(
        basic_ostream<C,T>& s,const fisher_f_distribution& d) {
        using S=basic_ostream<C,T>; auto f=s.flags(); C fill=s.fill();
        streamsize pr=s.precision(); C sp=static_cast<C>(' ');
        s.flags(S::scientific|S::left);s.fill(sp);
        s.precision(numeric_limits<RealType>::max_digits10);
        s<<d.m_<<sp<<d.n_<<sp<<d.x_<<sp<<d.y_;
        s.flags(f);s.fill(fill);s.precision(pr);return s;
    }
    template<typename C,typename T> friend basic_istream<C,T>& operator>>(
        basic_istream<C,T>& s,fisher_f_distribution& d) {
        using S=basic_istream<C,T>; auto f=s.flags();s.flags(S::dec|S::skipws);
        RealType m=0,n=0;gamma_distribution<RealType>x,y;
        const bool parsed_m = __detail::__random_read_required(s, m);
        const bool parsed_n = parsed_m &&
            __detail::__random_read_required(s, n);
        const bool parsed_x = parsed_n &&
            __detail::__random_read_required(s, x);
        const bool parsed_y = parsed_x &&
            __detail::__random_read_required(s, y);
        if(!s.fail()&&parsed_y&&m>0&&n>0&&x.alpha()==m/2&&x.beta()==1&&
           y.alpha()==n/2&&y.beta()==1){d.m_=m;d.n_=n;d.x_=x;d.y_=y;}
        else if(!s.fail()) s.setstate(S::failbit);
        s.flags(f);
        return s;
    }
};

template<typename RealType = double>
class student_t_distribution {
    static_assert(is_floating_point<RealType>::value,
                  "RealType must be floating point");
    RealType n_; normal_distribution<RealType> normal_;
    gamma_distribution<RealType> gamma_;
public:
    using result_type=RealType;
    struct param_type { using distribution_type=student_t_distribution;
        RealType n_; explicit param_type(RealType n=RealType(1)):n_(n){}
        RealType n()const{return n_;}
        friend bool operator==(const param_type&a,const param_type&b){return a.n_==b.n_;}
        friend bool operator!=(const param_type&a,const param_type&b){return !(a==b);}
    };
    explicit student_t_distribution(RealType n=RealType(1))
        :n_(n),normal_(),gamma_(n/RealType(2),RealType(2)){}
    explicit student_t_distribution(const param_type&p):student_t_distribution(p.n_){}
    void reset(){normal_.reset();gamma_.reset();} RealType n()const{return n_;}
    param_type param()const{return param_type(n_);} void param(const param_type&p){
        n_=p.n_;gamma_.param(typename gamma_distribution<RealType>::param_type(n_/2,2));}
    result_type min()const{return numeric_limits<RealType>::lowest();}
    result_type max()const{return numeric_limits<RealType>::max();}
    template<typename G>result_type operator()(G&g){return (*this)(g,param());}
    template<typename G>result_type operator()(G&g,const param_type&p){
        RealType v=gamma_(g,typename gamma_distribution<RealType>::param_type(p.n_/2,2));
        return normal_(g)*__detail::__random_sqrt(p.n_/v);}
    friend bool operator==(const student_t_distribution&a,const student_t_distribution&b)
    {return a.n_==b.n_&&a.normal_==b.normal_&&a.gamma_==b.gamma_;}
    friend bool operator!=(const student_t_distribution&a,const student_t_distribution&b){return !(a==b);}
    template<typename C,typename T>friend basic_ostream<C,T>&operator<<(
        basic_ostream<C,T>&s,const student_t_distribution&d){using S=basic_ostream<C,T>;
        auto f=s.flags();C fill=s.fill();streamsize pr=s.precision();C sp=static_cast<C>(' ');
        s.flags(S::scientific|S::left);s.fill(sp);s.precision(numeric_limits<RealType>::max_digits10);
        s<<d.n_<<sp<<d.normal_<<sp<<d.gamma_;s.flags(f);s.fill(fill);s.precision(pr);return s;}
    template<typename C,typename T>friend basic_istream<C,T>&operator>>(
        basic_istream<C,T>&s,student_t_distribution&d){using S=basic_istream<C,T>;
        auto f=s.flags();s.flags(S::dec|S::skipws);RealType n=0;
        normal_distribution<RealType>nd;gamma_distribution<RealType>gd;
        const bool parsed_n = __detail::__random_read_required(s, n);
        const bool parsed_nd = parsed_n &&
            __detail::__random_read_required(s, nd);
        const bool parsed_gd = parsed_nd &&
            __detail::__random_read_required(s, gd);
        if(!s.fail()&&parsed_gd&&n>0&&gd.alpha()==n/2&&gd.beta()==2){d.n_=n;d.normal_=nd;d.gamma_=gd;}
        else if(!s.fail()) s.setstate(S::failbit);
        s.flags(f);
        return s;}
};

template<typename IntType = int>
class discrete_distribution {
    static_assert(is_integral<IntType>::value,
                  "IntType must be integral");
public:
    using result_type = IntType;
    struct param_type {
        using distribution_type = discrete_distribution;
        vector<double> probabilities_;
        vector<double> cumulative_;

        param_type() {}
        template<typename InputIt>
        param_type(InputIt first, InputIt last) {
            while (first != last) { probabilities_.push_back(*first); ++first; }
            initialize();
        }
        param_type(initializer_list<double> weights)
            : probabilities_(weights) { initialize(); }
        template<typename Function>
        param_type(size_t count, double minimum, double maximum,
                   Function function) {
            const size_t divisor = count == 0 ? 1 : count;
            const double delta = (maximum - minimum) /
                                 static_cast<double>(divisor);
            probabilities_.reserve(count);
            for (size_t index = 0; index < count; ++index)
                probabilities_.push_back(function(
                    minimum + (static_cast<double>(index) + 0.5) * delta));
            initialize();
        }
        vector<double> probabilities() const {
            return probabilities_.empty() ? vector<double>(1, 1.0)
                                          : probabilities_;
        }
        friend bool operator==(const param_type& a, const param_type& b)
        { return a.probabilities_ == b.probabilities_; }
        friend bool operator!=(const param_type& a, const param_type& b)
        { return !(a == b); }
    private:
        void initialize() {
            cumulative_.clear();
            if (probabilities_.size() < 2) { probabilities_.clear(); return; }
            double total = 0.0;
            for (size_t i = 0; i < probabilities_.size(); ++i)
                total += probabilities_[i];
            if (!(total > 0.0) || total > numeric_limits<double>::max()) {
                probabilities_.clear(); return;
            }
            cumulative_.reserve(probabilities_.size());
            double cumulative = 0.0;
            for (size_t i = 0; i < probabilities_.size(); ++i) {
                probabilities_[i] /= total;
                cumulative += probabilities_[i];
                cumulative_.push_back(cumulative);
            }
            cumulative_[cumulative_.size() - 1] = 1.0;
        }
        friend class discrete_distribution;
    };

private:
    param_type parameter_;
public:
    discrete_distribution() {}
    template<typename InputIt> discrete_distribution(InputIt first, InputIt last)
        : parameter_(first, last) {}
    discrete_distribution(initializer_list<double> weights) : parameter_(weights) {}
    template<typename Function>
    discrete_distribution(size_t count, double minimum, double maximum,
                          Function function)
        : parameter_(count, minimum, maximum, function) {}
    explicit discrete_distribution(const param_type& parameter)
        : parameter_(parameter) {}
    void reset() {}
    vector<double> probabilities() const { return parameter_.probabilities(); }
    param_type param() const { return parameter_; }
    void param(const param_type& parameter) { parameter_ = parameter; }
    result_type min() const { return result_type(0); }
    result_type max() const { return parameter_.probabilities_.empty()
        ? result_type(0)
        : static_cast<result_type>(parameter_.probabilities_.size() - 1); }
    template<typename Generator> result_type operator()(Generator& generator)
    { return (*this)(generator, parameter_); }
    template<typename Generator>
    result_type operator()(Generator& generator, const param_type& parameter) {
        if (parameter.cumulative_.empty()) return result_type(0);
        const double unit = generate_canonical<
            double, static_cast<size_t>(numeric_limits<double>::digits)>(generator);
        size_t index = 0;
        while (index + 1 < parameter.cumulative_.size() &&
               parameter.cumulative_[index] < unit) ++index;
        return static_cast<result_type>(index);
    }
    friend bool operator==(const discrete_distribution& a,
                           const discrete_distribution& b)
    { return a.parameter_ == b.parameter_; }
    friend bool operator!=(const discrete_distribution& a,
                           const discrete_distribution& b) { return !(a == b); }
    template<typename C, typename T> friend basic_ostream<C,T>& operator<<(
        basic_ostream<C,T>& stream, const discrete_distribution& distribution) {
        using S = basic_ostream<C,T>; auto flags = stream.flags();
        C fill = stream.fill(); streamsize precision = stream.precision();
        C space = static_cast<C>(' '); stream.flags(S::scientific | S::left);
        stream.fill(space); stream.precision(numeric_limits<double>::max_digits10);
        vector<double> values = distribution.probabilities();
        stream << values.size();
        for (size_t i = 0; i < values.size(); ++i) stream << space << values[i];
        stream.flags(flags); stream.fill(fill); stream.precision(precision);
        return stream;
    }
    template<typename C, typename T> friend basic_istream<C,T>& operator>>(
        basic_istream<C,T>& stream, discrete_distribution& distribution) {
        using S = basic_istream<C,T>; auto flags = stream.flags();
        stream.flags(S::dec | S::skipws); size_t count = 0;
        const bool parsed_count = __detail::__random_read_required(
            stream, count);
        vector<double> values;
        bool valid = parsed_count && !stream.fail() && count > 0;
        if (valid) values.reserve(count);
        double total = 0.0;
        for (size_t i = 0; valid && i < count; ++i) {
            double value = 0.0;
            const bool parsed_value = __detail::__random_read_required(
                stream, value);
            valid = parsed_value && !stream.fail() && value >= 0.0 &&
                    value <= numeric_limits<double>::max();
            if (valid) { values.push_back(value); total += value; }
        }
        valid = valid && total > 0.0 && total <= numeric_limits<double>::max();
        if (valid) distribution.parameter_ = param_type(values.begin(), values.end());
        else if (!stream.fail()) stream.setstate(S::failbit);
        stream.flags(flags); return stream;
    }
};

template<typename RealType = double>
class piecewise_constant_distribution {
    static_assert(is_floating_point<RealType>::value,
                  "RealType must be floating point");
public:
    using result_type = RealType;
    struct param_type {
        using distribution_type = piecewise_constant_distribution;
        vector<RealType> intervals_;
        vector<double> densities_;
        vector<double> cumulative_;
        param_type() {}
        template<typename BoundaryIt, typename WeightIt>
        param_type(BoundaryIt first, BoundaryIt last, WeightIt weights) {
            if (first != last) {
                for (;;) {
                    intervals_.push_back(*first); ++first;
                    if (first == last) break;
                    densities_.push_back(*weights); ++weights;
                }
            }
            initialize();
        }
        template<typename Function>
        param_type(initializer_list<RealType> boundaries, Function function)
            : intervals_(boundaries) {
            if (intervals_.size() >= 2) {
                densities_.reserve(intervals_.size() - 1);
                for (size_t i = 0; i + 1 < intervals_.size(); ++i)
                    densities_.push_back(function(
                        (intervals_[i] + intervals_[i + 1]) / RealType(2)));
            }
            initialize();
        }
        template<typename Function>
        param_type(size_t count, RealType minimum, RealType maximum,
                   Function function) {
            const size_t interval_count = count == 0 ? 1 : count;
            const RealType delta = (maximum - minimum) /
                                   RealType(interval_count);
            intervals_.reserve(interval_count + 1);
            densities_.reserve(interval_count);
            for (size_t i = 0; i <= interval_count; ++i)
                intervals_.push_back(minimum + RealType(i) * delta);
            for (size_t i = 0; i < interval_count; ++i)
                densities_.push_back(function(intervals_[i] + delta / RealType(2)));
            initialize();
        }
        vector<RealType> intervals() const {
            if (!intervals_.empty()) return intervals_;
            vector<RealType> value(2); value[1] = RealType(1); return value;
        }
        vector<double> densities() const {
            return densities_.empty() ? vector<double>(1, 1.0) : densities_;
        }
        friend bool operator==(const param_type& a, const param_type& b)
        { return a.intervals_ == b.intervals_ && a.densities_ == b.densities_; }
        friend bool operator!=(const param_type& a, const param_type& b)
        { return !(a == b); }
    private:
        void initialize() {
            cumulative_.clear();
            if (intervals_.size() < 2 ||
                densities_.size() + 1 != intervals_.size()) {
                intervals_.clear(); densities_.clear(); return;
            }
            const RealType real_max = numeric_limits<RealType>::max();
            const double double_max = numeric_limits<double>::max();
            for (size_t i = 0; i < intervals_.size(); ++i) {
                if (!(intervals_[i] >= -real_max && intervals_[i] <= real_max) ||
                    (i != 0 && !(intervals_[i - 1] < intervals_[i]))) {
                    intervals_.clear(); densities_.clear(); return;
                }
            }
            double total = 0.0;
            for (size_t i = 0; i < densities_.size(); ++i) {
                const double value = densities_[i];
                const double width = static_cast<double>(
                    intervals_[i + 1] - intervals_[i]);
                if (!(value >= 0.0 && value <= double_max &&
                      width > 0.0 && width <= double_max)) {
                    intervals_.clear(); densities_.clear(); return;
                }
                total += value;
            }
            if (!(total > 0.0 && total <= double_max)) {
                intervals_.clear(); densities_.clear(); return;
            }
            cumulative_.reserve(densities_.size());
            double cumulative = 0.0;
            for (size_t i = 0; i < densities_.size(); ++i) {
                const double probability = densities_[i] / total;
                const double normalized = probability / static_cast<double>(
                    intervals_[i + 1] - intervals_[i]);
                if (!(normalized >= 0.0 && normalized <= double_max)) {
                    intervals_.clear(); densities_.clear();
                    cumulative_.clear(); return;
                }
                cumulative += probability;
                cumulative_.push_back(cumulative);
                densities_[i] = normalized;
            }
            cumulative_[cumulative_.size() - 1] = 1.0;
            if (intervals_.size() == 2 && intervals_[0] == RealType(0) &&
                intervals_[1] == RealType(1)) {
                intervals_.clear(); densities_.clear(); cumulative_.clear();
            }
        }
        friend class piecewise_constant_distribution;
    };
private:
    param_type parameter_;
public:
    piecewise_constant_distribution() {}
    template<typename BoundaryIt, typename WeightIt>
    piecewise_constant_distribution(BoundaryIt first, BoundaryIt last,
                                    WeightIt weights)
        : parameter_(first, last, weights) {}
    template<typename Function>
    piecewise_constant_distribution(initializer_list<RealType> boundaries,
                                    Function function)
        : parameter_(boundaries, function) {}
    template<typename Function>
    piecewise_constant_distribution(size_t count, RealType minimum,
                                    RealType maximum, Function function)
        : parameter_(count, minimum, maximum, function) {}
    explicit piecewise_constant_distribution(const param_type& parameter)
        : parameter_(parameter) {}
    void reset() {}
    vector<RealType> intervals() const { return parameter_.intervals(); }
    vector<double> densities() const { return parameter_.densities(); }
    param_type param() const { return parameter_; }
    void param(const param_type& parameter) { parameter_ = parameter; }
    result_type min() const { return parameter_.intervals_.empty()
        ? RealType(0) : parameter_.intervals_.front(); }
    result_type max() const { return parameter_.intervals_.empty()
        ? RealType(1) : parameter_.intervals_.back(); }
    template<typename Generator> result_type operator()(Generator& generator)
    { return (*this)(generator, parameter_); }
    template<typename Generator>
    result_type operator()(Generator& generator, const param_type& parameter) {
        const double unit = generate_canonical<
            double, static_cast<size_t>(numeric_limits<double>::digits)>(generator);
        if (parameter.cumulative_.empty()) return static_cast<RealType>(unit);
        size_t index = 0;
        while (index + 1 < parameter.cumulative_.size() &&
               parameter.cumulative_[index] <= unit) ++index;
        const double previous = index == 0 ? 0.0
                                           : parameter.cumulative_[index - 1];
        return parameter.intervals_[index] + static_cast<RealType>(
            (unit - previous) / parameter.densities_[index]);
    }
    friend bool operator==(const piecewise_constant_distribution& a,
                           const piecewise_constant_distribution& b)
    { return a.parameter_ == b.parameter_; }
    friend bool operator!=(const piecewise_constant_distribution& a,
                           const piecewise_constant_distribution& b)
    { return !(a == b); }
    template<typename C, typename T> friend basic_ostream<C,T>& operator<<(
        basic_ostream<C,T>& stream,
        const piecewise_constant_distribution& distribution) {
        using S = basic_ostream<C,T>; auto flags = stream.flags();
        C fill = stream.fill(); streamsize precision = stream.precision();
        C space = static_cast<C>(' '); stream.flags(S::scientific | S::left);
        stream.fill(space); stream.precision(numeric_limits<RealType>::max_digits10);
        vector<RealType> boundaries = distribution.intervals();
        vector<double> density = distribution.densities();
        stream << boundaries.size() - 1;
        for (size_t i = 0; i < boundaries.size(); ++i)
            stream << space << boundaries[i];
        for (size_t i = 0; i < density.size(); ++i)
            stream << space << density[i];
        stream.flags(flags); stream.fill(fill); stream.precision(precision);
        return stream;
    }
    template<typename C, typename T> friend basic_istream<C,T>& operator>>(
        basic_istream<C,T>& stream,
        piecewise_constant_distribution& distribution) {
        using S = basic_istream<C,T>; auto flags = stream.flags();
        stream.flags(S::dec | S::skipws); size_t count = 0;
        const bool parsed_count = __detail::__random_read_required(
            stream, count);
        vector<RealType> boundaries; vector<double> weights;
        bool valid = parsed_count && !stream.fail() && count > 0;
        if (valid) { boundaries.reserve(count + 1); weights.reserve(count); }
        for (size_t i = 0; valid && i <= count; ++i) {
            RealType value = 0;
            const bool parsed_value = __detail::__random_read_required(
                stream, value);
            valid = parsed_value && !stream.fail() &&
                    value >= -numeric_limits<RealType>::max() &&
                    value <= numeric_limits<RealType>::max() &&
                    (i == 0 || boundaries.back() < value);
            if (valid) boundaries.push_back(value);
        }
        double total = 0.0;
        for (size_t i = 0; valid && i < count; ++i) {
            double value = 0.0;
            const bool parsed_value = __detail::__random_read_required(
                stream, value);
            valid = parsed_value && !stream.fail() && value >= 0.0 &&
                    value <= numeric_limits<double>::max();
            if (valid) {
                const double area = value * static_cast<double>(
                    boundaries[i + 1] - boundaries[i]);
                valid = area >= 0.0 && area <= numeric_limits<double>::max();
                if (valid) { weights.push_back(area); total += area; }
            }
        }
        valid = valid && total > 0.0 &&
                total <= numeric_limits<double>::max();
        if (valid) distribution.parameter_ = param_type(
            boundaries.begin(), boundaries.end(), weights.begin());
        else if (!stream.fail()) stream.setstate(S::failbit);
        stream.flags(flags); return stream;
    }
};

template<typename RealType = double>
class piecewise_linear_distribution {
    static_assert(is_floating_point<RealType>::value,
                  "RealType must be floating point");
public:
    using result_type = RealType;
    struct param_type {
        using distribution_type = piecewise_linear_distribution;
        vector<RealType> intervals_;
        vector<double> densities_;
        vector<double> cumulative_;
        vector<double> slopes_;
        param_type() {}
        template<typename BoundaryIt, typename WeightIt>
        param_type(BoundaryIt first, BoundaryIt last, WeightIt weights) {
            if (first != last) {
                intervals_.push_back(*first); ++first;
                if (first != last) {
                    densities_.push_back(*weights); ++weights;
                    while (first != last) {
                        intervals_.push_back(*first);
                        densities_.push_back(*weights);
                        ++first; ++weights;
                    }
                }
            }
            initialize();
        }
        template<typename Function>
        param_type(initializer_list<RealType> boundaries, Function function)
            : intervals_(boundaries) {
            if (intervals_.size() >= 2) {
                densities_.reserve(intervals_.size());
                for (size_t i = 0; i < intervals_.size(); ++i)
                    densities_.push_back(function(intervals_[i]));
            }
            initialize();
        }
        template<typename Function>
        param_type(size_t count, RealType minimum, RealType maximum,
                   Function function) {
            const size_t interval_count = count == 0 ? 1 : count;
            const RealType delta = (maximum - minimum) /
                                   RealType(interval_count);
            intervals_.reserve(interval_count + 1);
            densities_.reserve(interval_count + 1);
            for (size_t i = 0; i <= interval_count; ++i) {
                const RealType point = minimum + RealType(i) * delta;
                intervals_.push_back(point);
                densities_.push_back(function(point));
            }
            initialize();
        }
        vector<RealType> intervals() const {
            if (!intervals_.empty()) return intervals_;
            vector<RealType> value(2); value[1] = RealType(1); return value;
        }
        vector<double> densities() const {
            return densities_.empty() ? vector<double>(2, 1.0) : densities_;
        }
        friend bool operator==(const param_type& a, const param_type& b)
        { return a.intervals_ == b.intervals_ && a.densities_ == b.densities_; }
        friend bool operator!=(const param_type& a, const param_type& b)
        { return !(a == b); }
    private:
        void initialize() {
            cumulative_.clear(); slopes_.clear();
            if (intervals_.size() < 2 ||
                densities_.size() != intervals_.size()) {
                intervals_.clear(); densities_.clear(); return;
            }
            const RealType real_max = numeric_limits<RealType>::max();
            const double double_max = numeric_limits<double>::max();
            for (size_t i = 0; i < intervals_.size(); ++i) {
                if (!(intervals_[i] >= -real_max && intervals_[i] <= real_max) ||
                    !(densities_[i] >= 0.0 && densities_[i] <= double_max) ||
                    (i != 0 && !(intervals_[i - 1] < intervals_[i]))) {
                    intervals_.clear(); densities_.clear(); return;
                }
            }
            double total = 0.0;
            cumulative_.reserve(intervals_.size() - 1);
            for (size_t i = 0; i + 1 < intervals_.size(); ++i) {
                const double width = static_cast<double>(
                    intervals_[i + 1] - intervals_[i]);
                const double area = 0.5 * densities_[i] * width +
                                    0.5 * densities_[i + 1] * width;
                if (!(width > 0.0 && width <= double_max &&
                      area >= 0.0 && area <= double_max)) {
                    intervals_.clear(); densities_.clear();
                    cumulative_.clear(); return;
                }
                total += area; cumulative_.push_back(total);
            }
            if (!(total > 0.0 && total <= double_max)) {
                intervals_.clear(); densities_.clear();
                cumulative_.clear(); return;
            }
            for (size_t i = 0; i < densities_.size(); ++i)
                densities_[i] /= total;
            slopes_.reserve(intervals_.size() - 1);
            for (size_t i = 0; i < cumulative_.size(); ++i) {
                cumulative_[i] /= total;
                const double slope = (densities_[i + 1] - densities_[i]) /
                    static_cast<double>(intervals_[i + 1] - intervals_[i]);
                if (!(slope >= -double_max && slope <= double_max)) {
                    intervals_.clear(); densities_.clear();
                    cumulative_.clear(); slopes_.clear(); return;
                }
                slopes_.push_back(slope);
            }
            cumulative_[cumulative_.size() - 1] = 1.0;
            if (intervals_.size() == 2 && intervals_[0] == RealType(0) &&
                intervals_[1] == RealType(1) &&
                densities_[0] == densities_[1]) {
                intervals_.clear(); densities_.clear();
                cumulative_.clear(); slopes_.clear();
            }
        }
        friend class piecewise_linear_distribution;
    };
private:
    param_type parameter_;
public:
    piecewise_linear_distribution() {}
    template<typename BoundaryIt, typename WeightIt>
    piecewise_linear_distribution(BoundaryIt first, BoundaryIt last,
                                  WeightIt weights)
        : parameter_(first, last, weights) {}
    template<typename Function>
    piecewise_linear_distribution(initializer_list<RealType> boundaries,
                                  Function function)
        : parameter_(boundaries, function) {}
    template<typename Function>
    piecewise_linear_distribution(size_t count, RealType minimum,
                                  RealType maximum, Function function)
        : parameter_(count, minimum, maximum, function) {}
    explicit piecewise_linear_distribution(const param_type& parameter)
        : parameter_(parameter) {}
    void reset() {}
    vector<RealType> intervals() const { return parameter_.intervals(); }
    vector<double> densities() const { return parameter_.densities(); }
    param_type param() const { return parameter_; }
    void param(const param_type& parameter) { parameter_ = parameter; }
    result_type min() const { return parameter_.intervals_.empty()
        ? RealType(0) : parameter_.intervals_.front(); }
    result_type max() const { return parameter_.intervals_.empty()
        ? RealType(1) : parameter_.intervals_.back(); }
    template<typename Generator> result_type operator()(Generator& generator)
    { return (*this)(generator, parameter_); }
    template<typename Generator>
    result_type operator()(Generator& generator, const param_type& parameter) {
        const double unit = generate_canonical<
            double, static_cast<size_t>(numeric_limits<double>::digits)>(generator);
        if (parameter.cumulative_.empty()) return static_cast<RealType>(unit);
        size_t index = 0;
        while (index + 1 < parameter.cumulative_.size() &&
               parameter.cumulative_[index] <= unit) ++index;
        const double previous = index == 0 ? 0.0
                                           : parameter.cumulative_[index - 1];
        const double mass = unit - previous;
        const double half_slope = 0.5 * parameter.slopes_[index];
        const double density = parameter.densities_[index];
        const double offset = half_slope == 0.0 ? mass / density
            : 0.5 * (__detail::__random_sqrt(
                density * density + 4.0 * half_slope * mass) - density) /
                half_slope;
        return parameter.intervals_[index] + static_cast<RealType>(offset);
    }
    friend bool operator==(const piecewise_linear_distribution& a,
                           const piecewise_linear_distribution& b)
    { return a.parameter_ == b.parameter_; }
    friend bool operator!=(const piecewise_linear_distribution& a,
                           const piecewise_linear_distribution& b)
    { return !(a == b); }
    template<typename C, typename T> friend basic_ostream<C,T>& operator<<(
        basic_ostream<C,T>& stream,
        const piecewise_linear_distribution& distribution) {
        using S = basic_ostream<C,T>; auto flags = stream.flags();
        C fill = stream.fill(); streamsize precision = stream.precision();
        C space = static_cast<C>(' '); stream.flags(S::scientific | S::left);
        stream.fill(space); stream.precision(numeric_limits<RealType>::max_digits10);
        vector<RealType> boundaries = distribution.intervals();
        vector<double> density = distribution.densities();
        stream << boundaries.size() - 1;
        for (size_t i = 0; i < boundaries.size(); ++i)
            stream << space << boundaries[i];
        for (size_t i = 0; i < density.size(); ++i)
            stream << space << density[i];
        stream.flags(flags); stream.fill(fill); stream.precision(precision);
        return stream;
    }
    template<typename C, typename T> friend basic_istream<C,T>& operator>>(
        basic_istream<C,T>& stream,
        piecewise_linear_distribution& distribution) {
        using S = basic_istream<C,T>; auto flags = stream.flags();
        stream.flags(S::dec | S::skipws); size_t count = 0;
        const bool parsed_count = __detail::__random_read_required(
            stream, count);
        vector<RealType> boundaries; vector<double> density;
        bool valid = parsed_count && !stream.fail() && count > 0;
        if (valid) {
            boundaries.reserve(count + 1); density.reserve(count + 1);
        }
        for (size_t i = 0; valid && i <= count; ++i) {
            RealType value = 0;
            const bool parsed_value = __detail::__random_read_required(
                stream, value);
            valid = parsed_value && !stream.fail() &&
                    value >= -numeric_limits<RealType>::max() &&
                    value <= numeric_limits<RealType>::max() &&
                    (i == 0 || boundaries.back() < value);
            if (valid) boundaries.push_back(value);
        }
        for (size_t i = 0; valid && i <= count; ++i) {
            double value = 0.0;
            const bool parsed_value = __detail::__random_read_required(
                stream, value);
            valid = parsed_value && !stream.fail() && value >= 0.0 &&
                    value <= numeric_limits<double>::max();
            if (valid) density.push_back(value);
        }
        double total = 0.0;
        for (size_t i = 0; valid && i < count; ++i) {
            const double width = static_cast<double>(
                boundaries[i + 1] - boundaries[i]);
            const double area = 0.5 * density[i] * width +
                                0.5 * density[i + 1] * width;
            valid = area >= 0.0 && area <= numeric_limits<double>::max();
            if (valid) total += area;
        }
        valid = valid && total > 0.0 &&
                total <= numeric_limits<double>::max();
        if (valid) distribution.parameter_ = param_type(
            boundaries.begin(), boundaries.end(), density.begin());
        else if (!stream.fail()) stream.setstate(S::failbit);
        stream.flags(flags); return stream;
    }
};

template<typename CharT, typename Traits, typename IntType>
basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& stream,
           const uniform_int_distribution<IntType>& distribution)
{
    using stream_type = basic_ostream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    const CharT fill = stream.fill();
    const CharT space = static_cast<CharT>(' ');
    stream.flags(stream_type::dec | stream_type::left);
    stream.fill(space);
    __detail::__random_write_integer(stream, distribution.a());
    stream << space;
    __detail::__random_write_integer(stream, distribution.b());
    stream.flags(flags);
    stream.fill(fill);
    return stream;
}

template<typename CharT, typename Traits, typename IntType>
basic_istream<CharT, Traits>&
operator>>(basic_istream<CharT, Traits>& stream,
           uniform_int_distribution<IntType>& distribution)
{
    using stream_type = basic_istream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    stream.flags(stream_type::dec | stream_type::skipws);
    IntType candidate_a = IntType(0);
    IntType candidate_b = IntType(0);
    const bool parsed_a = __detail::__random_read_integer(stream, candidate_a);
    const bool parsed_b = parsed_a &&
        __detail::__random_read_integer(stream, candidate_b);
    if (parsed_b && !stream.fail() && candidate_a <= candidate_b) {
        distribution.param(
            typename uniform_int_distribution<IntType>::param_type(
                candidate_a, candidate_b));
    } else if (!stream.fail()) {
        stream.setstate(stream_type::failbit);
    }
    stream.flags(flags);
    return stream;
}

template<typename CharT, typename Traits, typename RealType>
basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& stream,
           const uniform_real_distribution<RealType>& distribution)
{
    using stream_type = basic_ostream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    const CharT fill = stream.fill();
    const streamsize precision = stream.precision();
    const CharT space = static_cast<CharT>(' ');
    stream.flags(stream_type::scientific | stream_type::left);
    stream.fill(space);
    stream.precision(numeric_limits<RealType>::max_digits10);
    stream << distribution.a() << space << distribution.b();
    stream.flags(flags);
    stream.fill(fill);
    stream.precision(precision);
    return stream;
}

template<typename CharT, typename Traits, typename RealType>
basic_istream<CharT, Traits>&
operator>>(basic_istream<CharT, Traits>& stream,
           uniform_real_distribution<RealType>& distribution)
{
    using stream_type = basic_istream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    stream.flags(stream_type::skipws);
    RealType candidate_a = RealType(0);
    RealType candidate_b = RealType(0);
    const bool parsed_a = __detail::__random_read_required(
        stream, candidate_a);
    const bool parsed_b = parsed_a &&
        __detail::__random_read_required(stream, candidate_b);
    if (parsed_b && !stream.fail() && candidate_a <= candidate_b) {
        distribution.param(
            typename uniform_real_distribution<RealType>::param_type(
                candidate_a, candidate_b));
    } else if (!stream.fail()) {
        stream.setstate(stream_type::failbit);
    }
    stream.flags(flags);
    return stream;
}

template<typename CharT, typename Traits>
basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& stream,
           const bernoulli_distribution& distribution)
{
    using stream_type = basic_ostream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    const CharT fill = stream.fill();
    const streamsize precision = stream.precision();
    stream.flags(stream_type::scientific | stream_type::left);
    stream.fill(static_cast<CharT>(' '));
    stream.precision(numeric_limits<double>::max_digits10);
    stream << distribution.p();
    stream.flags(flags);
    stream.fill(fill);
    stream.precision(precision);
    return stream;
}

template<typename CharT, typename Traits>
basic_istream<CharT, Traits>&
operator>>(basic_istream<CharT, Traits>& stream,
           bernoulli_distribution& distribution)
{
    using stream_type = basic_istream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    stream.flags(stream_type::skipws);
    double candidate_p = 0.0;
    const bool parsed = __detail::__random_read_required(stream, candidate_p);
    if (parsed && !stream.fail() && candidate_p >= 0.0 && candidate_p <= 1.0) {
        distribution.param(bernoulli_distribution::param_type(candidate_p));
    } else if (!stream.fail()) {
        stream.setstate(stream_type::failbit);
    }
    stream.flags(flags);
    return stream;
}

template<typename CharT, typename Traits, typename IntType>
basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& stream,
           const geometric_distribution<IntType>& distribution)
{
    using stream_type = basic_ostream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    const CharT fill = stream.fill();
    const streamsize precision = stream.precision();
    stream.flags(stream_type::scientific | stream_type::left);
    stream.fill(static_cast<CharT>(' '));
    stream.precision(numeric_limits<double>::max_digits10);
    stream << distribution.p();
    stream.flags(flags);
    stream.fill(fill);
    stream.precision(precision);
    return stream;
}

template<typename CharT, typename Traits, typename IntType>
basic_istream<CharT, Traits>&
operator>>(basic_istream<CharT, Traits>& stream,
           geometric_distribution<IntType>& distribution)
{
    using stream_type = basic_istream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    stream.flags(stream_type::skipws);
    double candidate_p = 0.0;
    const bool parsed = __detail::__random_read_required(stream, candidate_p);
    if (parsed && !stream.fail() && candidate_p > 0.0 && candidate_p <= 1.0) {
        distribution.param(
            typename geometric_distribution<IntType>::param_type(
                candidate_p));
    } else if (!stream.fail()) {
        stream.setstate(stream_type::failbit);
    }
    stream.flags(flags);
    return stream;
}

template<typename CharT, typename Traits, typename IntType>
basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& stream,
           const negative_binomial_distribution<IntType>& distribution)
{
    using stream_type = basic_ostream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    const CharT fill = stream.fill();
    const streamsize precision = stream.precision();
    const CharT space = static_cast<CharT>(' ');
    stream.flags(stream_type::scientific | stream_type::left);
    stream.fill(space);
    stream.precision(numeric_limits<double>::max_digits10);
    __detail::__random_write_integer(stream, distribution.k());
    stream << space << distribution.p();
    stream.flags(flags);
    stream.fill(fill);
    stream.precision(precision);
    return stream;
}

template<typename CharT, typename Traits, typename IntType>
basic_istream<CharT, Traits>&
operator>>(basic_istream<CharT, Traits>& stream,
           negative_binomial_distribution<IntType>& distribution)
{
    using stream_type = basic_istream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    stream.flags(stream_type::dec | stream_type::skipws);
    IntType candidate_k = IntType(0);
    double candidate_p = 0.0;
    const bool parsed_k = __detail::__random_read_integer(stream, candidate_k);
    const bool parsed_p = parsed_k &&
        __detail::__random_read_required(stream, candidate_p);
    if (!stream.fail() && parsed_p && candidate_k > IntType(0) &&
        candidate_p > 0.0 && candidate_p <= 1.0) {
        distribution.param(
            typename negative_binomial_distribution<IntType>::param_type(
                candidate_k, candidate_p));
    } else if (!stream.fail()) {
        stream.setstate(stream_type::failbit);
    }
    stream.flags(flags);
    return stream;
}

template<typename CharT, typename Traits, typename IntType>
basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& stream,
           const binomial_distribution<IntType>& distribution)
{
    using stream_type = basic_ostream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    const CharT fill = stream.fill();
    const streamsize precision = stream.precision();
    const CharT space = static_cast<CharT>(' ');
    stream.flags(stream_type::scientific | stream_type::left);
    stream.fill(space);
    stream.precision(numeric_limits<double>::max_digits10);
    __detail::__random_write_integer(stream, distribution.t());
    stream << space << distribution.p();
    stream.flags(flags);
    stream.fill(fill);
    stream.precision(precision);
    return stream;
}

template<typename CharT, typename Traits, typename IntType>
basic_istream<CharT, Traits>&
operator>>(basic_istream<CharT, Traits>& stream,
           binomial_distribution<IntType>& distribution)
{
    using stream_type = basic_istream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    stream.flags(stream_type::dec | stream_type::skipws);
    IntType candidate_t = IntType(0);
    double candidate_p = 0.0;
    const bool parsed_t = __detail::__random_read_integer(stream, candidate_t);
    const bool parsed_p = parsed_t &&
        __detail::__random_read_required(stream, candidate_p);
    if (!stream.fail() && parsed_p && candidate_t >= IntType(0) &&
        candidate_p >= 0.0 && candidate_p <= 1.0) {
        distribution.param(
            typename binomial_distribution<IntType>::param_type(
                candidate_t, candidate_p));
    } else if (!stream.fail()) {
        stream.setstate(stream_type::failbit);
    }
    stream.flags(flags);
    return stream;
}

template<typename CharT, typename Traits, typename IntType>
basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& stream,
           const poisson_distribution<IntType>& distribution)
{
    using stream_type = basic_ostream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    const CharT fill = stream.fill();
    const streamsize precision = stream.precision();
    stream.flags(stream_type::scientific | stream_type::left);
    stream.fill(static_cast<CharT>(' '));
    stream.precision(numeric_limits<double>::max_digits10);
    stream << distribution.mean();
    stream.flags(flags);
    stream.fill(fill);
    stream.precision(precision);
    return stream;
}

template<typename CharT, typename Traits, typename IntType>
basic_istream<CharT, Traits>&
operator>>(basic_istream<CharT, Traits>& stream,
           poisson_distribution<IntType>& distribution)
{
    using stream_type = basic_istream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    stream.flags(stream_type::skipws);
    double candidate_mean = 0.0;
    const bool parsed = __detail::__random_read_required(
        stream, candidate_mean);
    if (parsed && !stream.fail() && candidate_mean > 0.0 &&
        candidate_mean <= numeric_limits<double>::max()) {
        distribution.param(
            typename poisson_distribution<IntType>::param_type(
                candidate_mean));
    } else if (!stream.fail()) {
        stream.setstate(stream_type::failbit);
    }
    stream.flags(flags);
    return stream;
}

template<typename CharT, typename Traits, typename RealType>
basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& stream,
           const exponential_distribution<RealType>& distribution)
{
    using stream_type = basic_ostream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    const CharT fill = stream.fill();
    const streamsize precision = stream.precision();
    stream.flags(stream_type::scientific | stream_type::left);
    stream.fill(static_cast<CharT>(' '));
    stream.precision(numeric_limits<RealType>::max_digits10);
    stream << distribution.lambda();
    stream.flags(flags);
    stream.fill(fill);
    stream.precision(precision);
    return stream;
}

template<typename CharT, typename Traits, typename RealType>
basic_istream<CharT, Traits>&
operator>>(basic_istream<CharT, Traits>& stream,
           exponential_distribution<RealType>& distribution)
{
    using stream_type = basic_istream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    stream.flags(stream_type::skipws);
    RealType candidate_lambda = RealType(0);
    const bool parsed = __detail::__random_read_required(
        stream, candidate_lambda);
    if (parsed && !stream.fail() && candidate_lambda > RealType(0)) {
        distribution.param(
            typename exponential_distribution<RealType>::param_type(
                candidate_lambda));
    } else if (!stream.fail()) {
        stream.setstate(stream_type::failbit);
    }
    stream.flags(flags);
    return stream;
}

template<typename CharT, typename Traits, typename RealType>
basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& stream,
           const weibull_distribution<RealType>& distribution)
{
    using stream_type = basic_ostream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    const CharT fill = stream.fill();
    const streamsize precision = stream.precision();
    const CharT space = static_cast<CharT>(' ');
    stream.flags(stream_type::scientific | stream_type::left);
    stream.fill(space);
    stream.precision(numeric_limits<RealType>::max_digits10);
    stream << distribution.a() << space << distribution.b();
    stream.flags(flags);
    stream.fill(fill);
    stream.precision(precision);
    return stream;
}

template<typename CharT, typename Traits, typename RealType>
basic_istream<CharT, Traits>&
operator>>(basic_istream<CharT, Traits>& stream,
           weibull_distribution<RealType>& distribution)
{
    using stream_type = basic_istream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    stream.flags(stream_type::skipws);
    RealType candidate_a = RealType(0);
    RealType candidate_b = RealType(0);
    const bool parsed_a = __detail::__random_read_required(
        stream, candidate_a);
    const bool parsed_b = parsed_a &&
        __detail::__random_read_required(stream, candidate_b);
    if (parsed_b && !stream.fail() && candidate_a > RealType(0) &&
        candidate_b > RealType(0)) {
        distribution.param(
            typename weibull_distribution<RealType>::param_type(
                candidate_a, candidate_b));
    } else if (!stream.fail()) {
        stream.setstate(stream_type::failbit);
    }
    stream.flags(flags);
    return stream;
}

template<typename CharT, typename Traits, typename RealType>
basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& stream,
           const extreme_value_distribution<RealType>& distribution)
{
    using stream_type = basic_ostream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    const CharT fill = stream.fill();
    const streamsize precision = stream.precision();
    const CharT space = static_cast<CharT>(' ');
    stream.flags(stream_type::scientific | stream_type::left);
    stream.fill(space);
    stream.precision(numeric_limits<RealType>::max_digits10);
    stream << distribution.a() << space << distribution.b();
    stream.flags(flags);
    stream.fill(fill);
    stream.precision(precision);
    return stream;
}

template<typename CharT, typename Traits, typename RealType>
basic_istream<CharT, Traits>&
operator>>(basic_istream<CharT, Traits>& stream,
           extreme_value_distribution<RealType>& distribution)
{
    using stream_type = basic_istream<CharT, Traits>;
    const typename stream_type::fmtflags flags = stream.flags();
    stream.flags(stream_type::skipws);
    RealType candidate_a = RealType(0);
    RealType candidate_b = RealType(0);
    const bool parsed_a = __detail::__random_read_required(
        stream, candidate_a);
    const bool parsed_b = parsed_a &&
        __detail::__random_read_required(stream, candidate_b);
    if (parsed_b && !stream.fail() && candidate_b > RealType(0)) {
        distribution.param(
            typename extreme_value_distribution<RealType>::param_type(
                candidate_a, candidate_b));
    } else if (!stream.fail()) {
        stream.setstate(stream_type::failbit);
    }
    stream.flags(flags);
    return stream;
}

/* ═══════════════════════════════════════════════════════════════
 * seed_seq
 * ═══════════════════════════════════════════════════════════════*/

class seed_seq {
    vector<uint_least32_t> v_;

public:
    using result_type = uint_least32_t;

    seed_seq() noexcept = default;

    template<typename InputIt>
    seed_seq(InputIt begin, InputIt end) {
        for (; begin != end; ++begin) {
            v_.push_back(static_cast<result_type>(*begin));
        }
    }

    template<typename IntType,
             enable_if_t<is_integral<IntType>::value, int> = 0>
    seed_seq(initializer_list<IntType> values) {
        v_.reserve(values.size());
        for (const auto value : values) {
            v_.push_back(static_cast<result_type>(value));
        }
    }

    seed_seq(const seed_seq&) = delete;
    seed_seq& operator=(const seed_seq&) = delete;

    template<typename RandomIt>
    void generate(RandomIt begin, RandomIt end) {
        if (begin == end) return;

        using word_type = uint32_t;
        const size_t output_size = static_cast<size_t>(end - begin);
        const size_t seed_size = v_.size();
        const size_t t = output_size >= 623 ? 11
                       : output_size >= 68  ? 7
                       : output_size >= 39  ? 5
                       : output_size >= 7   ? 3
                                            : (output_size - 1) / 2;
        const size_t p = (output_size - t) / 2;
        const size_t q = p + t;
        const size_t mix_count = seed_size + 1 > output_size
                               ? seed_size + 1 : output_size;

        for (RandomIt it = begin; it != end; ++it) {
            *it = static_cast<result_type>(0x8b8b8b8bu);
        }

        const auto read_word = [&](size_t index) -> word_type {
            return static_cast<word_type>(begin[index]);
        };
        const auto write_word = [&](size_t index, word_type value) {
            begin[index] = static_cast<result_type>(value);
        };

        word_type r1 = 1371501266u;
        word_type r2 = static_cast<word_type>(r1 + seed_size);
        write_word(p, static_cast<word_type>(read_word(p) + r1));
        write_word(q, static_cast<word_type>(read_word(q) + r2));
        write_word(0, r2);

        size_t k = 1;
        for (; k <= seed_size; ++k) {
            const size_t kn = k % output_size;
            const size_t kpn = (k + p) % output_size;
            const size_t kqn = (k + q) % output_size;
            const word_type argument = static_cast<word_type>(
                read_word(kn) ^ read_word(kpn) ^
                read_word((k - 1) % output_size));
            r1 = static_cast<word_type>(
                1664525u * static_cast<word_type>(
                    argument ^ (argument >> 27)));
            r2 = static_cast<word_type>(
                r1 + static_cast<word_type>(kn) +
                static_cast<word_type>(v_[k - 1]));
            write_word(kpn, static_cast<word_type>(read_word(kpn) + r1));
            write_word(kqn, static_cast<word_type>(read_word(kqn) + r2));
            write_word(kn, r2);
        }

        for (; k < mix_count; ++k) {
            const size_t kn = k % output_size;
            const size_t kpn = (k + p) % output_size;
            const size_t kqn = (k + q) % output_size;
            const word_type argument = static_cast<word_type>(
                read_word(kn) ^ read_word(kpn) ^
                read_word((k - 1) % output_size));
            r1 = static_cast<word_type>(
                1664525u * static_cast<word_type>(
                    argument ^ (argument >> 27)));
            r2 = static_cast<word_type>(r1 + static_cast<word_type>(kn));
            write_word(kpn, static_cast<word_type>(read_word(kpn) + r1));
            write_word(kqn, static_cast<word_type>(read_word(kqn) + r2));
            write_word(kn, r2);
        }

        for (; k < mix_count + output_size; ++k) {
            const size_t kn = k % output_size;
            const size_t kpn = (k + p) % output_size;
            const size_t kqn = (k + q) % output_size;
            const word_type argument = static_cast<word_type>(
                read_word(kn) + read_word(kpn) +
                read_word((k - 1) % output_size));
            const word_type r3 = static_cast<word_type>(
                1566083941u * static_cast<word_type>(
                    argument ^ (argument >> 27)));
            const word_type r4 = static_cast<word_type>(
                r3 - static_cast<word_type>(kn));
            write_word(kpn, static_cast<word_type>(read_word(kpn) ^ r3));
            write_word(kqn, static_cast<word_type>(read_word(kqn) ^ r4));
            write_word(kn, r4);
        }
    }

    size_t size() const noexcept { return v_.size(); }

    template<typename OutputIt>
    void param(OutputIt dest) const {
        for (const auto& x : v_) {
            *dest++ = x;
        }
    }
};

/* ═══════════════════════════════════════════════════════════════
 * generate_canonical
 * ═══════════════════════════════════════════════════════════════*/

template<typename RealType, size_t bits, typename Generator>
RealType generate_canonical(Generator& g) {
    static_assert(is_floating_point<RealType>::value,
                  "RealType must be floating point");
    static_assert(numeric_limits<RealType>::digits <= 64,
                  "floating-point precision wider than 64 bits is unsupported");
    using generator_type = typename remove_reference<Generator>::type;
    using generator_result = typename generator_type::result_type;
    static_assert(is_unsigned<generator_result>::value,
                  "URBG result_type must be unsigned");
    static_assert(sizeof(generator_result) <= sizeof(unsigned long long),
                  "URBG result_type is wider than supported integers");
    static_assert(generator_type::min() < generator_type::max(),
                  "URBG must have a non-empty result interval");

    constexpr size_t requested_bits =
        bits < static_cast<size_t>(numeric_limits<RealType>::digits)
            ? bits : static_cast<size_t>(numeric_limits<RealType>::digits);
    constexpr unsigned long long minimum =
        static_cast<unsigned long long>(generator_type::min());
    constexpr unsigned long long maximum =
        static_cast<unsigned long long>(generator_type::max());
    constexpr unsigned long long outcomes = maximum - minimum + 1u;
    /* Keep arithmetic in both ordinary-C++11 branches defined even when a
     * full-width URBG represents 2^64 outcomes as zero modulo uint64_t. */
    const unsigned long long finite_outcomes =
        outcomes == 0u ? 1u : outcomes;
    const size_t bounded_requested_bits =
        requested_bits == 64u ? 63u : requested_bits;

    size_t draws = 1u;
    if (requested_bits != 0u && outcomes != 0u) {
        unsigned long long product = 1u;
        draws = 0u;
        for (;;) {
            ++draws;
            bool enough;
            if (requested_bits == 64u) {
                enough = product > (~0ull / finite_outcomes);
            } else {
                const unsigned long long target =
                    1ull << bounded_requested_bits;
                const unsigned long long quotient = target / finite_outcomes;
                const unsigned long long remainder = target % finite_outcomes;
                const unsigned long long ceiling =
                    quotient + (remainder != 0u ? 1u : 0u);
                enough = product >= ceiling;
            }
            if (enough) break;
            product *= finite_outcomes;
        }
    }

    const RealType base = outcomes == 0u
                        ? static_cast<RealType>(18446744073709551616.0L)
                        : static_cast<RealType>(outcomes);
    RealType sum = RealType(0);
    RealType factor = RealType(1);
    for (size_t i = 0; i < draws; ++i) {
        const unsigned long long digit =
            static_cast<unsigned long long>(g()) - minimum;
        sum += static_cast<RealType>(digit) * factor;
        factor *= base;
    }

    RealType result = sum / factor;
    if (result < RealType(0)) result = RealType(0);
    if (!(result < RealType(1))) {
        result = RealType(1) - numeric_limits<RealType>::epsilon() / RealType(2);
    }
    return result;
}

} /* namespace std */

#undef RIN_RANDOM_CONSTEXPR14

#endif /* RINCXX_RANDOM_H */
