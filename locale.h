/*
 * RinOS C++ <locale> ✿
 * ロケールライブラリ
 */

#ifndef RINCXX_LOCALE_H
#define RINCXX_LOCALE_H

#include "rincxx.h"
#include "string.h"
#include "memory.h"
#include "stdexcept.h"
#include "iterator.h"
#include "cstdlib.h"
#include "cstdio.h"
#if __cplusplus >= 201703L
#include "charconv.h"
#include "pointer_order.h"
#endif
#if defined(__cplusplus) && !defined(RIN_FREESTANDING) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__
/* Pull the hosted C wide-character limits in before Rin's target limits
 * wrapper.  glibc's fortify wchar helpers validate MB_LEN_MAX at declaration
 * time, and the target default must not shadow that host ABI. */
#include <wchar.h>
#endif
#include "limits.h"
#if defined(__cplusplus) && !defined(RIN_FREESTANDING) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__
/* A hosted consumer owns the C time ABI.  In particular, MinGW's `tm` has
 * no GNU extension fields, so including Rin's target-only definition after a
 * host C header would produce an incompatible aggregate. */
#include <time.h>
#include <wchar.h>
/* This header itself is named locale.h; skip the Rin include directory so a
 * hosted consumer reaches its C runtime's locale declarations. */
#include_next <locale.h>
#else
#include "../libc/time.h"
#include "../libc/wchar.h"
#include "../libc/locale.h"
#endif

#if defined(RIN_TM_HAS_GNU_EXTENSIONS) || \
    (defined(__GLIBC__) && defined(__USE_MISC))
#define RINCXX_TM_HAS_GNU_EXTENSIONS 1
#else
#define RINCXX_TM_HAS_GNU_EXTENSIONS 0
#endif

/* A hosted POSIX `struct tm` has no standard storage for a numeric UTC
 * offset or a zone name.  Keep the standard layout intact and let a runtime
 * owner provide a bounded sidecar keyed by the caller's `tm` address.  The
 * hook is weak so freestanding/header-only users can keep the historical
 * failure-atomic rejection when no owner is linked. */
#if defined(__GNUC__) || defined(__clang__)
#define RINCXX_TM_TIMEZONE_WEAK __attribute__((weak))
#else
#define RINCXX_TM_TIMEZONE_WEAK
#endif
extern "C" int rin_cxx_tm_timezone_get(const void* value, long* offset,
                                        char* zone, size_t zone_capacity,
                                        unsigned* flags)
    RINCXX_TM_TIMEZONE_WEAK;
extern "C" int rin_cxx_tm_timezone_set(const void* value, unsigned flags,
                                        long offset, const char* zone)
    RINCXX_TM_TIMEZONE_WEAK;
/* Optional product owner for the immutable C timezone catalog.  When linked,
 * `%Z` can publish a named zone only after this complete token is resolved;
 * header-only consumers retain the historical bounded sidecar behavior. */
extern "C" int rin_time_zone_resolve_name(const char* input, size_t length,
                                           long* offset)
    RINCXX_TM_TIMEZONE_WEAK;
enum {
    RINCXX_TM_TIMEZONE_HAS_OFFSET = 1u,
    RINCXX_TM_TIMEZONE_HAS_NAME = 2u
};

/* C locale.  Freestanding builds use the one Rin C declaration owner rather
 * than reproducing `lconv` and the setlocale functions here.  Besides keeping
 * the ABI in one place, this lets either <locale> or <clocale> be included
 * first.  Hosted headers that do not use the conventional _LOCALE_H guard
 * retain the compatible declaration fallback below. */
#if !defined(_LOCALE_H)
extern "C" {
    char* setlocale(int category, const char* locale);

#if defined(RIN_FREESTANDING) || !defined(__STDC_HOSTED__) || !__STDC_HOSTED__
    struct lconv {
        char* decimal_point;
        char* thousands_sep;
        char* grouping;
        char* int_curr_symbol;
        char* currency_symbol;
        char* mon_decimal_point;
        char* mon_thousands_sep;
        char* mon_grouping;
        char* positive_sign;
        char* negative_sign;
        char  int_frac_digits;
        char  frac_digits;
        char  p_cs_precedes;
        char  p_sep_by_space;
        char  n_cs_precedes;
        char  n_sep_by_space;
        char  p_sign_posn;
        char  n_sign_posn;
        char  int_p_cs_precedes;
        char  int_p_sep_by_space;
        char  int_n_cs_precedes;
        char  int_n_sep_by_space;
        char  int_p_sign_posn;
        char  int_n_sign_posn;
    };

    struct lconv* localeconv(void);
#endif
}
#endif

/* ロケールカテゴリ */
#ifndef LC_CTYPE
#define LC_CTYPE    0
#define LC_NUMERIC  1
#define LC_TIME     2
#define LC_COLLATE  3
#define LC_MONETARY 4
#define LC_MESSAGES 5
#define LC_ALL      6
#endif

/* Microsoft-family C runtimes expose no LC_MESSAGES category.  The C++
 * locale category still needs a unique facet-composition bit; it is never
 * passed to the hosted C setlocale API. */
#ifndef LC_MESSAGES
#define LC_MESSAGES 6
#endif

namespace std {

#if __cplusplus >= 201703L
#define RIN_LOCALE_SPECIALIZATION_DEFINITION inline
#elif defined(_WIN32) && (defined(__GNUC__) || defined(__clang__))
/* C++11 has no inline variables.  MinGW's selectany gives each header
 * definition one coalesced data symbol without the aliasing that weak data
 * symbols can acquire in PE/COFF links. */
#define RIN_LOCALE_SPECIALIZATION_DEFINITION __attribute__((selectany))
#elif defined(__GNUC__) || defined(__clang__)
/* ELF linkers coalesce weak data definitions reliably. */
#define RIN_LOCALE_SPECIALIZATION_DEFINITION __attribute__((weak))
#else
#define RIN_LOCALE_SPECIALIZATION_DEFINITION
#endif

class ios_base;
class locale;

template<typename Facet>
const Facet& use_facet(const locale& value);

/* Defined after ios_base is complete in ios.h. Locale facets use these
 * narrow accessors instead of depending on the full stream hierarchy. */
unsigned int __locale_stream_flags(const ios_base&) noexcept;
long __locale_stream_width(const ios_base&) noexcept;
long __locale_stream_precision(const ios_base&) noexcept;
void __locale_stream_width_reset(ios_base&) noexcept;
void __locale_stream_fail(ios_base&) noexcept;
locale __locale_stream_locale(const ios_base&);
unsigned int __locale_eofbit() noexcept;
unsigned int __locale_failbit() noexcept;

/* ═══════════════════════════════════════════════════════════════
 * locale::facet 基底クラス
 * ═══════════════════════════════════════════════════════════════*/

class locale_facet {
protected:
    explicit locale_facet(size_t refs = 0)
        : refs_(0), immortal_(refs != 0) {}
    virtual ~locale_facet() = default;

    locale_facet(const locale_facet&) = delete;
    locale_facet& operator=(const locale_facet&) = delete;

private:
    friend class locale;
    mutable size_t refs_;
    bool immortal_;

    void _add_ref() const { if (!immortal_) ++refs_; }
    void _release() const {
        if (!immortal_ && refs_ != 0 && --refs_ == 0) delete this;
    }
};

/* ═══════════════════════════════════════════════════════════════
 * locale::id
 * ═══════════════════════════════════════════════════════════════*/

class locale_id {
public:
    locale_id() : id_(0) {}

    locale_id(const locale_id&) = delete;
    locale_id& operator=(const locale_id&) = delete;

private:
    friend class locale;
    mutable size_t id_;

    static size_t _next_id() {
        static size_t next = 0;
        /* C++11 consumers intentionally disable thread-safe statics.  The
         * counter is zero-initialized and published with an atomic increment
         * so the first concurrent facet lookup still receives one process-wide
         * monotonic ID. */
        return __atomic_add_fetch(&next, static_cast<size_t>(1),
                                  __ATOMIC_RELAXED);
    }

    size_t _get_id() const {
        size_t current = __atomic_load_n(&id_, __ATOMIC_ACQUIRE);
        if (current != 0) return current;
        const size_t candidate = _next_id();
        size_t expected = 0;
        if (__atomic_compare_exchange_n(&id_, &expected, candidate, false,
                                        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            return candidate;
        }
        return expected;
    }
};

/* ═══════════════════════════════════════════════════════════════
 * locale クラス
 * ═══════════════════════════════════════════════════════════════*/

class locale {
public:
    using facet = locale_facet;
    using id = locale_id;

    /* カテゴリ */
    using category = int;
    static constexpr category none     = 0;
    static constexpr category collate  = (1 << LC_COLLATE);
    static constexpr category ctype    = (1 << LC_CTYPE);
    static constexpr category monetary = (1 << LC_MONETARY);
    static constexpr category numeric  = (1 << LC_NUMERIC);
    static constexpr category time     = (1 << LC_TIME);
    static constexpr category messages = (1 << LC_MESSAGES);
    static constexpr category all      = collate | ctype | monetary | numeric | time | messages;

    /* コンストラクタ */
    /* A default-constructed locale observes one coherent global snapshot.
     * `global()` can replace both the name storage and the immutable facet
     * chain, so reading the global object without the same lock used by the
     * writer can otherwise retain a node while another thread releases it. */
    locale() noexcept : locale(_global_snapshot()) {}

    locale(const locale& other) noexcept
        : name_(other.name_), facets_(other.facets_) {
        _add_node_ref(facets_);
    }

    explicit locale(const char* name) : name_("C"), facets_(nullptr) {
        if (!_supported_name(name)) _invalid_locale();
        name_ = name;
    }

    explicit locale(const string& name) : locale(name.c_str()) {}

    template<typename Facet>
    locale(const locale& other, Facet* facet)
        : name_(), facets_(other.facets_) {
        _add_node_ref(facets_);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        bool installed = false;
        try {
#endif
            _facet_node* node = nullptr;
            if (facet != nullptr) {
                node = _allocate_facet_node(
                    Facet::id._get_id(),
                    _category_for_id(Facet::id._get_id()),
                    facet, facets_);
                _release_node(facets_);
                facets_ = node;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                installed = true;
#endif
            }
            name_ = facet != nullptr ? string("*") : other.name_;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            /* Facet ownership transfers at the call boundary.  If the node
             * or the name allocation fails, undo the exact state already
             * installed before propagating the original exception. */
            if (installed) {
                _release_node(facets_);
            } else {
                if (facet != nullptr) delete facet;
                _release_node(facets_);
            }
            facets_ = nullptr;
            throw;
        }
#endif
    }

    locale(const locale& other, const char* name, category cat)
        : name_(other.name_), facets_(nullptr) {
        if (!_valid_category(cat)) _invalid_category();
        locale named(name);
        facets_ = _compose_facets(other, named, cat);
        name_ = _composed_name(other, named, cat);
    }

    locale(const locale& other, const locale& one, category cat)
        : name_(other.name_), facets_(nullptr) {
        if (!_valid_category(cat)) _invalid_category();
        facets_ = _compose_facets(other, one, cat);
        name_ = _composed_name(other, one, cat);
    }

    ~locale() { _release_node(facets_); }

    /* 代入 */
    const locale& operator=(const locale& other) noexcept {
        if (this == &other) return *this;
        _facet_node* replacement = other.facets_;
        _add_node_ref(replacement);
        _release_node(facets_);
        facets_ = replacement;
        name_ = other.name_;
        return *this;
    }

    /* 比較 */
    bool operator==(const locale& other) const {
        if (name_ != "*" && name_ == other.name_) return true;
        return _facets_equal(facets_, other.facets_);
    }

    bool operator!=(const locale& other) const {
        return !(*this == other);
    }

    /* 名前 */
    string name() const { return name_; }

    const char* _name_c_str() const noexcept { return name_.c_str(); }

    template<typename Facet>
    const Facet* _find_facet() const noexcept {
        size_t wanted = Facet::id._get_id();
        for (_facet_node* node = facets_; node != nullptr; node = node->previous) {
            if (node->id == wanted) return static_cast<const Facet*>(node->value);
        }
        return nullptr;
    }

    /* グローバルロケール */
    static locale global(const locale& loc) {
        _global_lock_guard guard;
        if (loc.name_ != "*" &&
            setlocale(_c_locale_category(),
                      _c_locale_name(loc.name_.c_str())) == nullptr)
            _invalid_locale();
        locale old = _global();
        _global() = loc;
        return old;
    }

    /* クラシック(C)ロケール */
    static const locale& classic() {
        static locale c_locale(_classic_init{});
        return c_locale;
    }

private:
    struct _facet_node {
        size_t references;
        size_t id;
        category facet_category;
        const locale_facet* value;
        _facet_node* previous;

        _facet_node(size_t facet_id, category selected_category,
                    const locale_facet* facet,
                    _facet_node* inherited)
            : references(1), id(facet_id),
              facet_category(selected_category), value(facet),
              previous(inherited) {
            value->_add_ref();
            _add_node_ref(previous);
        }
    };

    static _facet_node* _allocate_facet_node(
        size_t facet_id, category selected_category,
        const locale_facet* facet, _facet_node* inherited) {
        void* storage = rin_malloc(sizeof(_facet_node));
        if (!storage) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            throw bad_alloc();
#else
            rin_panic("std::locale facet allocation");
#endif
        }
        return ::new (storage) _facet_node(
            facet_id, selected_category, facet, inherited);
    }

    string name_;
    _facet_node* facets_;

    struct _classic_init {};

    explicit locale(_classic_init) noexcept : name_("C"), facets_(nullptr) {}

    class _global_lock_guard {
    public:
        _global_lock_guard() noexcept {
            while (__atomic_exchange_n(&_global_lock(), 1,
                                       __ATOMIC_ACQUIRE) != 0) {
                /* This lock only protects a short pointer/name snapshot.
                 * Do not introduce a scheduler dependency into a header
                 * facility that is also used during target bootstrap. */
                __atomic_signal_fence(__ATOMIC_SEQ_CST);
            }
        }

        ~_global_lock_guard() {
            __atomic_store_n(&_global_lock(), 0, __ATOMIC_RELEASE);
        }

        _global_lock_guard(const _global_lock_guard&) = delete;
        _global_lock_guard& operator=(const _global_lock_guard&) = delete;
    };

    static int& _global_lock() noexcept {
        static int lock = 0;
        return lock;
    }

    static locale _global_snapshot() noexcept {
        _global_lock_guard guard;
        return _global();
    }

    static void _add_node_ref(_facet_node* node) noexcept {
        if (node != nullptr) {
            __atomic_add_fetch(&node->references, 1u, __ATOMIC_RELAXED);
        }
    }

    static void _release_node(_facet_node* node) noexcept {
        while (node != nullptr &&
               __atomic_sub_fetch(&node->references, 1u,
                                  __ATOMIC_ACQ_REL) == 0u) {
            _facet_node* previous = node->previous;
            node->value->_release();
            delete node;
            node = previous;
        }
    }

    static bool _text_equal(const char* left, const char* right) noexcept {
        if (left == nullptr || right == nullptr) return left == right;
        while (*left != '\0' && *left == *right) {
            ++left;
            ++right;
        }
        return *left == *right;
    }

    static bool _regional_utf8_name(const char* name) noexcept {
        static constexpr const char* regions[] = {
            "en_US", "de_DE", "fr_FR", "es_ES", "it_IT", "pt_BR",
            "ja_JP"};
        static constexpr const char* suffixes[] = {
            ".UTF-8", ".UTF8", ".utf8"};
        for (const char* region : regions) {
            for (const char* suffix : suffixes) {
                char candidate[32] = {};
                size_t index = 0u;
                while (region[index] != '\0' && index + 1u <
                       sizeof(candidate)) {
                    candidate[index] = region[index];
                    ++index;
                }
                if (region[index] != '\0') return false;
                size_t suffix_index = 0u;
                while (suffix[suffix_index] != '\0' &&
                       index + suffix_index + 1u < sizeof(candidate)) {
                    candidate[index + suffix_index] = suffix[suffix_index];
                    ++suffix_index;
                }
                if (suffix[suffix_index] != '\0') return false;
                candidate[index + suffix_index] = '\0';
                if (_text_equal(name, candidate)) return true;
            }
        }
        return false;
    }

    static bool _supported_name(const char* name) noexcept {
        return _text_equal(name, "C") || _text_equal(name, "POSIX") ||
               _text_equal(name, "C.UTF-8") ||
               _text_equal(name, "POSIX.UTF-8") ||
               _text_equal(name, "C.UTF8") ||
               _text_equal(name, "POSIX.UTF8") ||
               _text_equal(name, "C.utf8") ||
               _text_equal(name, "POSIX.utf8") ||
               _regional_utf8_name(name);
    }

    static const char* _c_locale_name(const char* name) noexcept {
        return _text_equal(name, "C.UTF-8") ||
               _text_equal(name, "POSIX.UTF-8") ||
               _text_equal(name, "C.UTF8") ||
               _text_equal(name, "POSIX.UTF8") ||
               _text_equal(name, "C.utf8") ||
               _text_equal(name, "POSIX.utf8") ||
               _regional_utf8_name(name) ? "C" : name;
    }

    static int _c_locale_category() noexcept {
#if defined(_WIN32) && !defined(RIN_FREESTANDING) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__
        /* UCRT uses LC_ALL == 0.  The fallback Rin declarations may have
         * supplied the target-only value 6 when their C header guard was
         * seen first, so never pass that category into the host ABI. */
        return 0;
#else
        return LC_ALL;
#endif
    }

    [[noreturn]] static void _invalid_locale() {
#if defined(__cpp_exceptions)
        throw runtime_error("std::locale: unsupported locale name");
#else
        rin_panic("std::locale: unsupported locale name");
#endif
    }

    [[noreturn]] static void _invalid_category() {
#if defined(__cpp_exceptions)
        throw runtime_error("std::locale: invalid category mask");
#else
        rin_panic("std::locale: invalid category mask");
#endif
    }

    static bool _valid_category(category value) noexcept {
        return (value & ~all) == 0;
    }

    static string _composed_name(const locale& base, const locale& source,
                                 category selected) {
        if (selected == none) return base.name_;
        if (selected == all) return source.name_;
        if (base.name_ == source.name_) return base.name_;
        return string("*");
    }

    static const locale_facet* _find_node_value(_facet_node* node,
                                                 size_t id) noexcept {
        for (; node != nullptr; node = node->previous) {
            if (node->id == id) return node->value;
        }
        return nullptr;
    }

    static bool _effective_node(_facet_node* head,
                                _facet_node* candidate) noexcept {
        return _find_node_value(head, candidate->id) == candidate->value;
    }

    static bool _facets_equal(_facet_node* left, _facet_node* right) noexcept {
        for (_facet_node* node = left; node != nullptr;
             node = node->previous) {
            if (_effective_node(left, node) &&
                _find_node_value(right, node->id) != node->value)
                return false;
        }
        for (_facet_node* node = right; node != nullptr;
             node = node->previous) {
            if (_effective_node(right, node) &&
                _find_node_value(left, node->id) != node->value)
                return false;
        }
        return true;
    }

    static void _copy_facets(_facet_node*& output, _facet_node* source,
                             category selected, bool copy_selected) {
        for (_facet_node* node = source; node != nullptr;
             node = node->previous) {
            bool matches = node->facet_category != none &&
                           (node->facet_category & selected) != 0;
            if (matches != copy_selected ||
                _find_node_value(output, node->id) != nullptr)
                continue;
            _facet_node* copy = _allocate_facet_node(
                node->id, node->facet_category, node->value, output);
            _release_node(output);
            output = copy;
        }
    }

    static _facet_node* _compose_facets(const locale& base,
                                        const locale& source,
                                        category selected) {
        _facet_node* output = nullptr;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
        _copy_facets(output, base.facets_, selected, false);
        _copy_facets(output, source.facets_, selected, true);
        return output;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            _release_node(output);
            throw;
        }
#endif
    }

    static category _category_for_id(size_t id) noexcept;

    static locale& _global() {
        static locale global_locale(_classic_init{});
        return global_locale;
    }
};

/* ═══════════════════════════════════════════════════════════════
 * ctype_base
 * ═══════════════════════════════════════════════════════════════*/

class ctype_base {
public:
    using mask = unsigned short;

    static constexpr mask space  = 1 << 0;
    static constexpr mask print  = 1 << 1;
    static constexpr mask cntrl  = 1 << 2;
    static constexpr mask upper  = 1 << 3;
    static constexpr mask lower  = 1 << 4;
    static constexpr mask alpha  = 1 << 5;
    static constexpr mask digit  = 1 << 6;
    static constexpr mask punct  = 1 << 7;
    static constexpr mask xdigit = 1 << 8;
    static constexpr mask blank  = 1 << 9;
    static constexpr mask alnum  = alpha | digit;
    static constexpr mask graph  = alnum | punct;
};

/* ═══════════════════════════════════════════════════════════════
 * ctype<char> - 文字分類
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT>
class ctype : public locale::facet, public ctype_base {
public:
    using char_type = CharT;

    static locale::id id;

    explicit ctype(size_t refs = 0) : locale::facet(refs) {}

    bool is(mask m, char_type c) const { return do_is(m, c); }

    const char_type* is(const char_type* lo, const char_type* hi, mask* vec) const {
        return do_is(lo, hi, vec);
    }

    const char_type* scan_is(mask m, const char_type* lo, const char_type* hi) const {
        return do_scan_is(m, lo, hi);
    }

    const char_type* scan_not(mask m, const char_type* lo, const char_type* hi) const {
        return do_scan_not(m, lo, hi);
    }

    char_type toupper(char_type c) const { return do_toupper(c); }

    const char_type* toupper(char_type* lo, const char_type* hi) const {
        return do_toupper(lo, hi);
    }

    char_type tolower(char_type c) const { return do_tolower(c); }

    const char_type* tolower(char_type* lo, const char_type* hi) const {
        return do_tolower(lo, hi);
    }

    char_type widen(char c) const { return do_widen(c); }

    const char* widen(const char* lo, const char* hi, char_type* to) const {
        return do_widen(lo, hi, to);
    }

    char narrow(char_type c, char dfault) const { return do_narrow(c, dfault); }

    const char_type* narrow(const char_type* lo, const char_type* hi,
                            char dfault, char* to) const {
        return do_narrow(lo, hi, dfault, to);
    }

protected:
    virtual ~ctype() = default;

    virtual bool do_is(mask m, char_type c) const {
        unsigned char uc = static_cast<unsigned char>(c);
        if ((m & space) && (uc == ' ' || (uc >= '\t' && uc <= '\r'))) return true;
        if ((m & upper) && (uc >= 'A' && uc <= 'Z')) return true;
        if ((m & lower) && (uc >= 'a' && uc <= 'z')) return true;
        if ((m & digit) && (uc >= '0' && uc <= '9')) return true;
        if ((m & xdigit) && ((uc >= '0' && uc <= '9') ||
            (uc >= 'a' && uc <= 'f') || (uc >= 'A' && uc <= 'F'))) return true;
        if ((m & alpha) && ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z'))) return true;
        if ((m & punct) && ((uc >= '!' && uc <= '/') || (uc >= ':' && uc <= '@') ||
            (uc >= '[' && uc <= '`') || (uc >= '{' && uc <= '~'))) return true;
        if ((m & cntrl) && (uc < 0x20 || uc == 0x7F)) return true;
        if ((m & print) && (uc >= 0x20 && uc < 0x7F)) return true;
        if ((m & blank) && (uc == ' ' || uc == '\t')) return true;
        return false;
    }

    virtual const char_type* do_is(const char_type* lo, const char_type* hi, mask* vec) const {
        for (; lo != hi; ++lo, ++vec) {
            *vec = 0;
            unsigned char uc = static_cast<unsigned char>(*lo);
            if (uc == ' ' || (uc >= '\t' && uc <= '\r')) *vec |= space;
            if (uc >= 'A' && uc <= 'Z') *vec |= upper;
            if (uc >= 'a' && uc <= 'z') *vec |= lower;
            if (uc >= '0' && uc <= '9') *vec |= digit;
            if ((uc >= '0' && uc <= '9') || (uc >= 'a' && uc <= 'f') ||
                (uc >= 'A' && uc <= 'F')) *vec |= xdigit;
            if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z')) *vec |= alpha;
            if (uc < 0x20 || uc == 0x7F) *vec |= cntrl;
            if (uc >= 0x20 && uc < 0x7F) *vec |= print;
            if (uc == ' ' || uc == '\t') *vec |= blank;
        }
        return hi;
    }

    virtual const char_type* do_scan_is(mask m, const char_type* lo, const char_type* hi) const {
        for (; lo != hi; ++lo) {
            if (do_is(m, *lo)) return lo;
        }
        return hi;
    }

    virtual const char_type* do_scan_not(mask m, const char_type* lo, const char_type* hi) const {
        for (; lo != hi; ++lo) {
            if (!do_is(m, *lo)) return lo;
        }
        return hi;
    }

    virtual char_type do_toupper(char_type c) const {
        if (c >= 'a' && c <= 'z') return c - 'a' + 'A';
        return c;
    }

    virtual const char_type* do_toupper(char_type* lo, const char_type* hi) const {
        for (; lo != hi; ++lo) *lo = do_toupper(*lo);
        return hi;
    }

    virtual char_type do_tolower(char_type c) const {
        if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
        return c;
    }

    virtual const char_type* do_tolower(char_type* lo, const char_type* hi) const {
        for (; lo != hi; ++lo) *lo = do_tolower(*lo);
        return hi;
    }

    virtual char_type do_widen(char c) const { return c; }

    virtual const char* do_widen(const char* lo, const char* hi, char_type* to) const {
        for (; lo != hi; ++lo, ++to) *to = *lo;
        return hi;
    }

    virtual char do_narrow(char_type c, char dfault) const {
        (void)dfault;
        return c;
    }

    virtual const char_type* do_narrow(const char_type* lo, const char_type* hi,
                                       char dfault, char* to) const {
        (void)dfault;
        for (; lo != hi; ++lo, ++to) *to = *lo;
        return hi;
    }
};

template<typename CharT>
locale::id ctype<CharT>::id;

/* ctype<char>特殊化 */
template<>
class ctype<char> : public locale::facet, public ctype_base {
public:
    using char_type = char;

    static locale::id id;

    explicit ctype(const mask* table = nullptr, bool del = false, size_t refs = 0)
        : locale::facet(refs), table_(table ? table : classic_table()), del_(del) {}

    bool is(mask m, char c) const {
        return (table_[static_cast<unsigned char>(c)] & m) != 0;
    }

    const char* is(const char* lo, const char* hi, mask* vec) const {
        for (; lo != hi; ++lo, ++vec) {
            *vec = table_[static_cast<unsigned char>(*lo)];
        }
        return hi;
    }

    const char* scan_is(mask m, const char* lo, const char* hi) const {
        for (; lo != hi; ++lo) {
            if ((table_[static_cast<unsigned char>(*lo)] & m) != 0) return lo;
        }
        return hi;
    }

    const char* scan_not(mask m, const char* lo, const char* hi) const {
        for (; lo != hi; ++lo) {
            if ((table_[static_cast<unsigned char>(*lo)] & m) == 0) return lo;
        }
        return hi;
    }

    char toupper(char c) const { return do_toupper(c); }
    const char* toupper(char* lo, const char* hi) const { return do_toupper(lo, hi); }

    char tolower(char c) const { return do_tolower(c); }
    const char* tolower(char* lo, const char* hi) const { return do_tolower(lo, hi); }

    char widen(char c) const { return c; }
    const char* widen(const char* lo, const char* hi, char* to) const {
        for (; lo != hi; ++lo, ++to) *to = *lo;
        return hi;
    }

    char narrow(char c, char) const { return c; }
    const char* narrow(const char* lo, const char* hi, char, char* to) const {
        for (; lo != hi; ++lo, ++to) *to = *lo;
        return hi;
    }

    const mask* table() const noexcept { return table_; }

    static const mask* classic_table() noexcept {
        static mask table[256];
        static bool initialized = false;
        if (!initialized) {
            for (int i = 0; i < 256; ++i) {
                table[i] = 0;
                if (i == ' ' || (i >= '\t' && i <= '\r')) table[i] |= space;
                if (i >= 'A' && i <= 'Z') table[i] |= upper;
                if (i >= 'a' && i <= 'z') table[i] |= lower;
                if (i >= '0' && i <= '9') table[i] |= digit;
                if ((i >= '0' && i <= '9') || (i >= 'a' && i <= 'f') ||
                    (i >= 'A' && i <= 'F')) table[i] |= xdigit;
                if ((i >= 'A' && i <= 'Z') || (i >= 'a' && i <= 'z')) table[i] |= alpha;
                if ((i >= '!' && i <= '/') || (i >= ':' && i <= '@') ||
                    (i >= '[' && i <= '`') || (i >= '{' && i <= '~')) table[i] |= punct;
                if (i < 0x20 || i == 0x7F) table[i] |= cntrl;
                if (i >= 0x20 && i < 0x7F) table[i] |= print;
                if (i == ' ' || i == '\t') table[i] |= blank;
            }
            initialized = true;
        }
        return table;
    }

    static constexpr size_t table_size = 256;

protected:
    ~ctype() override {
        if (del_) delete[] table_;
    }

    virtual char do_toupper(char c) const {
        if (c >= 'a' && c <= 'z') return c - 'a' + 'A';
        return c;
    }

    virtual const char* do_toupper(char* lo, const char* hi) const {
        for (; lo != hi; ++lo) *lo = do_toupper(*lo);
        return hi;
    }

    virtual char do_tolower(char c) const {
        if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
        return c;
    }

    virtual const char* do_tolower(char* lo, const char* hi) const {
        for (; lo != hi; ++lo) *lo = do_tolower(*lo);
        return hi;
    }

private:
    const mask* table_;
    bool del_;
};

RIN_LOCALE_SPECIALIZATION_DEFINITION locale::id ctype<char>::id;

/* ═══════════════════════════════════════════════════════════════
 * numpunct - 数値書式句読点
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT>
class numpunct : public locale::facet {
public:
    using char_type = CharT;
    using string_type = basic_string<CharT>;

    static locale::id id;

    explicit numpunct(size_t refs = 0) : locale::facet(refs) {}

    char_type decimal_point() const { return do_decimal_point(); }
    char_type thousands_sep() const { return do_thousands_sep(); }
    string grouping() const { return do_grouping(); }
    string_type truename() const { return do_truename(); }
    string_type falsename() const { return do_falsename(); }

protected:
    virtual ~numpunct() = default;

    virtual char_type do_decimal_point() const { return '.'; }
    virtual char_type do_thousands_sep() const { return ','; }
    virtual string do_grouping() const { return ""; }
    virtual string_type do_truename() const {
        string_type value;
        value += static_cast<CharT>('t');
        value += static_cast<CharT>('r');
        value += static_cast<CharT>('u');
        value += static_cast<CharT>('e');
        return value;
    }
    virtual string_type do_falsename() const {
        string_type value;
        value += static_cast<CharT>('f');
        value += static_cast<CharT>('a');
        value += static_cast<CharT>('l');
        value += static_cast<CharT>('s');
        value += static_cast<CharT>('e');
        return value;
    }
};

template<typename CharT>
locale::id numpunct<CharT>::id;

/* The named facet providers below are defined before the common classic
 * facet availability table.  Keep the character-domain trait visible to
 * those providers; its full definition remains in the shared facet section
 * below. */
template<typename CharT>
struct __classic_locale_character;

namespace __num_punct_detail {

enum {
    profile_classic = 0,
    profile_de = 1,
    profile_fr = 2,
    profile_dot_decimal = 3
};

inline bool name_prefix(const char* name, const char* prefix) noexcept {
    if (!name || !prefix) return false;
    while (*prefix) {
        if (*name++ != *prefix++) return false;
    }
    return true;
}

inline int profile_for_name(const char* name) noexcept {
    if (name && ((name[0] == 'C' &&
                  (name[1] == '\0' || name[1] == '.')) ||
                 (name[0] == 'P' && name[1] == 'O' &&
                  name[2] == 'S' && name[3] == 'I' && name[4] == 'X' &&
                  (name[5] == '\0' || name[5] == '.'))))
        return profile_classic;
    if (name_prefix(name, "de_DE")) return profile_de;
    if (name_prefix(name, "fr_FR")) return profile_fr;
    if (name_prefix(name, "es_ES") || name_prefix(name, "it_IT") ||
        name_prefix(name, "pt_BR"))
        return profile_dot_decimal;
    return -1;
}

[[noreturn]] inline void invalid_name() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw runtime_error("std::numpunct_byname: unsupported locale name");
#else
    rin_panic("std::numpunct_byname: unsupported locale name");
#endif
}

} /* namespace __num_punct_detail */

template<typename CharT>
class numpunct_byname : public numpunct<CharT> {
public:
    using char_type = CharT;
    using string_type = basic_string<CharT>;

    explicit numpunct_byname(const char* name, size_t refs = 0)
        : numpunct<CharT>(refs),
          profile_(__num_punct_detail::profile_for_name(name)) {
        if (profile_ < 0) __num_punct_detail::invalid_name();
    }

protected:
    ~numpunct_byname() override = default;

    char_type do_decimal_point() const override {
        return static_cast<CharT>(
            profile_ == __num_punct_detail::profile_classic ||
            profile_ == __num_punct_detail::profile_fr
                ? (profile_ == __num_punct_detail::profile_fr ? ',' : '.')
                : ',');
    }

    char_type do_thousands_sep() const override {
        if (profile_ == __num_punct_detail::profile_de ||
            profile_ == __num_punct_detail::profile_dot_decimal)
            return static_cast<CharT>('.');
        if (profile_ == __num_punct_detail::profile_fr)
            return static_cast<CharT>(' ');
        return static_cast<CharT>(',');
    }

    string do_grouping() const override {
        string grouping;
        if (profile_ != __num_punct_detail::profile_classic)
            grouping += static_cast<char>(3);
        return grouping;
    }

private:
    int profile_;
};

template<typename CharT>
struct __named_numpunct_holder final : numpunct_byname<CharT> {
    explicit __named_numpunct_holder(const char* name)
        : numpunct_byname<CharT>(name, 1) {}
    ~__named_numpunct_holder() override = default;
};

template<typename Facet>
struct __named_facet_provider {
    static const Facet* get(const locale&) noexcept { return nullptr; }
};

template<typename CharT>
struct __named_facet_provider<numpunct<CharT>> {
    static const numpunct<CharT>* get(const locale& value) noexcept {
        const char* name = value._name_c_str();
        if (!__classic_locale_character<CharT>::value ||
            !name || name[0] == '\0' || name[0] == '*')
            return nullptr;
        if (__num_punct_detail::name_prefix(name, "de_DE")) {
            static __named_numpunct_holder<CharT> facet("de_DE.UTF-8");
            return &facet;
        }
        if (__num_punct_detail::name_prefix(name, "fr_FR")) {
            static __named_numpunct_holder<CharT> facet("fr_FR.UTF-8");
            return &facet;
        }
        if (__num_punct_detail::name_prefix(name, "es_ES")) {
            static __named_numpunct_holder<CharT> facet("es_ES.UTF-8");
            return &facet;
        }
        if (__num_punct_detail::name_prefix(name, "it_IT")) {
            static __named_numpunct_holder<CharT> facet("it_IT.UTF-8");
            return &facet;
        }
        if (__num_punct_detail::name_prefix(name, "pt_BR")) {
            static __named_numpunct_holder<CharT> facet("pt_BR.UTF-8");
            return &facet;
        }
        return nullptr;
    }
};

/* ═══════════════════════════════════════════════════════════════
 * num_get - bounded integral input facet
 * ═══════════════════════════════════════════════════════════════
 *
 * The stream extractors have their own streambuf transaction owner, but a
 * locale consumer also needs the standard num_get facet surface.  Keep this
 * first slice allocation-free and bounded: it accepts ASCII integer fields,
 * the active numpunct separator/grouping policy, and boolalpha names.  The
 * floating-point and arbitrary external-locale paths remain separate TODOs.
 */

namespace __num_get_detail {

/* Formatted numeric extraction must not use an unbounded stack array.  The
 * old 4 KiB ceiling was raised once, but a fixed 16 KiB stack buffer still
 * rejected otherwise valid fields and duplicated storage for normalization.
 * Keep one bounded heap owner per extraction instead.  The extra byte is for
 * the NUL terminator used by the C floating parser; integral parsing remains
 * length-based. */
static constexpr size_t numeric_field_capacity = 65536u;

struct numeric_token_buffer {
    char* data;
    size_t capacity;

    explicit numeric_token_buffer(size_t requested)
        : data(static_cast<char*>(rin_malloc(requested))),
          capacity(data ? requested : 0u) {}

    numeric_token_buffer(const numeric_token_buffer&) = delete;
    numeric_token_buffer& operator=(const numeric_token_buffer&) = delete;

    ~numeric_token_buffer() {
        if (data) rin_free(data);
    }

    bool valid() const noexcept { return data != nullptr; }
};

template<typename CharT>
inline bool ascii_digit(CharT value) noexcept {
    return value >= static_cast<CharT>('0') &&
           value <= static_cast<CharT>('9');
}

template<typename CharT>
inline int ascii_digit_value(CharT value) noexcept {
    if (value >= static_cast<CharT>('0') &&
        value <= static_cast<CharT>('9'))
        return static_cast<int>(value - static_cast<CharT>('0'));
    if (value >= static_cast<CharT>('a') &&
        value <= static_cast<CharT>('f'))
        return static_cast<int>(value - static_cast<CharT>('a')) + 10;
    if (value >= static_cast<CharT>('A') &&
        value <= static_cast<CharT>('F'))
        return static_cast<int>(value - static_cast<CharT>('A')) + 10;
    return -1;
}

template<typename CharT>
inline bool ascii_hex_letter(CharT value) noexcept {
    return (value >= static_cast<CharT>('a') &&
            value <= static_cast<CharT>('f')) ||
           (value >= static_cast<CharT>('A') &&
            value <= static_cast<CharT>('F'));
}

template<typename CharT>
inline bool ascii_equal(CharT value, char expected) noexcept {
    return value == static_cast<CharT>(expected);
}

template<typename CharT>
inline bool grouping_valid(const char* token, size_t length,
                           CharT separator, const string& grouping) noexcept {
    if (grouping.empty()) return false;

    size_t groups[512];
    size_t group_count = 0;
    size_t current = 0;
    bool separated = false;
    for (size_t i = 0; i < length; ++i) {
        if (token[i] == static_cast<char>(separator)) {
            if (current == 0 || group_count >= 512) return false;
            groups[group_count++] = current;
            current = 0;
            separated = true;
        } else {
            ++current;
        }
    }
    if (!separated || current == 0 || group_count >= 512) return false;
    groups[group_count++] = current;

    size_t pattern = 0;
    unsigned int expected = static_cast<unsigned char>(grouping[pattern]);
    if (expected == 0u || expected == 0xffu) return false;
    for (size_t right = group_count; right-- > 0;) {
        const bool leftmost = right == 0;
        if (expected != 0xffu &&
            (leftmost ? (groups[right] == 0 || groups[right] > expected)
                      : groups[right] != expected))
            return false;
        if (pattern + 1u < grouping.size()) {
            const unsigned int next =
                static_cast<unsigned char>(grouping[pattern + 1u]);
            if (next == 0u) {
                /* A zero repeats the preceding grouping size. */
            } else if (next == 0xffu) {
                expected = 0xffu;
            } else {
                expected = next;
                ++pattern;
            }
        }
    }
    return true;
}

template<typename T>
inline bool parse_integral(const char* token, size_t length, int base,
                           T& value) noexcept {
    size_t begin = 0;
    bool negative = false;
    if (begin < length && (token[begin] == '+' || token[begin] == '-')) {
        negative = token[begin] == '-';
        ++begin;
    }
    if (begin + 1u < length && token[begin] == '0' &&
        (token[begin + 1u] == 'x' || token[begin + 1u] == 'X')) {
        if (base == 16) begin += 2u;
    }
    if (begin >= length) return false;

    unsigned long long magnitude = 0;
    unsigned long long limit = static_cast<unsigned long long>(
        numeric_limits<T>::max());
    if (is_signed<T>::value && negative) ++limit;
    bool digit_seen = false;
    for (size_t i = begin; i < length; ++i) {
        if (token[i] == ',') continue;
        const int digit =
            token[i] >= '0' && token[i] <= '9'
                ? token[i] - '0'
                : (token[i] >= 'a' && token[i] <= 'f'
                       ? token[i] - 'a' + 10
                       : (token[i] >= 'A' && token[i] <= 'F'
                              ? token[i] - 'A' + 10 : -1));
        if (digit < 0 || digit >= base) return false;
        digit_seen = true;
        if (magnitude > (limit - static_cast<unsigned long long>(digit)) /
                           static_cast<unsigned long long>(base)) {
            return false;
        }
        magnitude = magnitude * static_cast<unsigned long long>(base) +
                    static_cast<unsigned long long>(digit);
    }
    if (!digit_seen) return false;
    if (negative) {
        value = static_cast<T>(0ull - magnitude);
    } else {
        value = static_cast<T>(magnitude);
    }
    return true;
}

} /* namespace __num_get_detail */

template<typename CharT, typename InputIterator = istreambuf_iterator<CharT>>
class num_get : public locale::facet {
public:
    using char_type = CharT;
    using iter_type = InputIterator;

    static locale::id id;

    explicit num_get(size_t refs = 0) : locale::facet(refs) {}

    iter_type get(iter_type input, iter_type end, ios_base& stream,
                  unsigned int& error, bool& value) const {
        return do_get(input, end, stream, error, value);
    }
    iter_type get(iter_type input, iter_type end, ios_base& stream,
                  unsigned int& error, long& value) const {
        return do_get(input, end, stream, error, value);
    }
    iter_type get(iter_type input, iter_type end, ios_base& stream,
                  unsigned int& error, unsigned long& value) const {
        return do_get(input, end, stream, error, value);
    }
    iter_type get(iter_type input, iter_type end, ios_base& stream,
                  unsigned int& error, long long& value) const {
        return do_get(input, end, stream, error, value);
    }
    iter_type get(iter_type input, iter_type end, ios_base& stream,
                  unsigned int& error, unsigned long long& value) const {
        return do_get(input, end, stream, error, value);
    }
    iter_type get(iter_type input, iter_type end, ios_base& stream,
                  unsigned int& error, float& value) const {
        return do_get(input, end, stream, error, value);
    }
    iter_type get(iter_type input, iter_type end, ios_base& stream,
                  unsigned int& error, double& value) const {
        return do_get(input, end, stream, error, value);
    }
    iter_type get(iter_type input, iter_type end, ios_base& stream,
                  unsigned int& error, long double& value) const {
        return do_get(input, end, stream, error, value);
    }
    iter_type get(iter_type input, iter_type end, ios_base& stream,
                  unsigned int& error, void*& value) const {
        return do_get(input, end, stream, error, value);
    }

protected:
    virtual ~num_get() = default;

    template<typename T>
    iter_type get_integral(iter_type input, iter_type end, ios_base& stream,
                           unsigned int& error, T& value) const {
        const locale selected = __locale_stream_locale(stream);
        const auto& punctuation = use_facet<numpunct<CharT>>(selected);
        const CharT separator = punctuation.thousands_sep();
        const unsigned int flags = __locale_stream_flags(stream);
        const bool hexadecimal = (flags & 0x0008u) != 0u;
        const bool octal = (flags & 0x0040u) != 0u;
        const bool auto_base = !hexadecimal && !octal &&
            (flags & 0x0002u) == 0u;
        const int base = hexadecimal ? 16 :
            (octal ? 8 : ((flags & 0x0002u) != 0u ? 10 : 0));

        __num_get_detail::numeric_token_buffer storage(
            __num_get_detail::numeric_field_capacity + 1u);
        if (!storage.valid()) {
            error |= __locale_failbit();
            return input;
        }
        char* token = storage.data;
        size_t length = 0;
        iter_type cursor = input;
        bool token_overflow = false;
        bool repeated_separator = false;
        while (cursor != end) {
            const CharT current = static_cast<CharT>(*cursor);
            const bool sign = length == 0u &&
                (current == static_cast<CharT>('+') ||
                 current == static_cast<CharT>('-'));
            const size_t sign_offset = length != 0u &&
                (token[0] == '+' || token[0] == '-') ? 1u : 0u;
            const bool auto_hex = auto_base &&
                length >= sign_offset + 2u &&
                token[sign_offset] == '0' &&
                (token[sign_offset + 1u] == 'x' ||
                 token[sign_offset + 1u] == 'X');
            const bool prefix = length == sign_offset + 1u &&
                token[sign_offset] == '0' &&
                (current == static_cast<CharT>('x') ||
                 current == static_cast<CharT>('X')) &&
                (hexadecimal || auto_base);
            const bool digit = __num_get_detail::ascii_digit(current) ||
                ((hexadecimal || auto_hex) &&
                 __num_get_detail::ascii_hex_letter(current));
            const bool grouped = current == separator &&
                !punctuation.grouping().empty();
            if (grouped && length != 0u &&
                token[length - 1u] == static_cast<char>(separator)) {
                repeated_separator = true;
                break;
            }
            if (!(sign || prefix || digit || grouped)) break;
            if (length < __num_get_detail::numeric_field_capacity)
                token[length++] = static_cast<char>(current);
            else
                token_overflow = true;
            ++cursor;
        }
        if (token_overflow) {
            error |= __locale_failbit();
            return cursor;
        }
        if (length == 0u) {
            error |= __locale_failbit();
            if (cursor == end) error |= __locale_eofbit();
            return cursor;
        }

        const string grouping = punctuation.grouping();
        bool invalid_grouping = repeated_separator;
        for (size_t i = 0; i < length; ++i) {
            if (token[i] == static_cast<char>(separator) &&
                !__num_get_detail::grouping_valid(
                    token, length, separator, grouping)) {
                invalid_grouping = true;
            }
            if (token[i] == static_cast<char>(separator) &&
                (i == 0u || token[i - 1u] == static_cast<char>(separator)))
                repeated_separator = true;
        }

        size_t normalized_length = 0;
        for (size_t i = 0; i < length; ++i) {
            if (token[i] != static_cast<char>(separator))
                token[normalized_length++] = token[i];
        }
        int parse_base = base;
        if (parse_base == 0) {
            parse_base = 10;
            if (normalized_length >= 2u && token[0] == '0' &&
                (token[1] == 'x' || token[1] == 'X')) {
                parse_base = 16;
            } else if (normalized_length > 1u && token[0] == '0') {
                parse_base = 8;
            }
        }
        if (!__num_get_detail::parse_integral(
                token, normalized_length, parse_base, value)) {
            error |= __locale_failbit();
            if (cursor == end) error |= __locale_eofbit();
            return cursor;
        }
        if (repeated_separator) value = T(0);
        if (invalid_grouping) error |= __locale_failbit();
        if (cursor == end) error |= __locale_eofbit();
        return cursor;
    }

    virtual iter_type do_get(iter_type input, iter_type end, ios_base& stream,
                             unsigned int& error, long& value) const {
        return get_integral(input, end, stream, error, value);
    }
    virtual iter_type do_get(iter_type input, iter_type end, ios_base& stream,
                             unsigned int& error, unsigned long& value) const {
        return get_integral(input, end, stream, error, value);
    }
    virtual iter_type do_get(iter_type input, iter_type end, ios_base& stream,
                             unsigned int& error, long long& value) const {
        return get_integral(input, end, stream, error, value);
    }
    virtual iter_type do_get(iter_type input, iter_type end, ios_base& stream,
                             unsigned int& error,
                             unsigned long long& value) const {
        return get_integral(input, end, stream, error, value);
    }

    template<typename FloatType>
    iter_type get_floating(iter_type input, iter_type end, ios_base& stream,
                           unsigned int& error, FloatType& value) const {
        const locale selected = __locale_stream_locale(stream);
        const auto& punctuation = use_facet<numpunct<CharT>>(selected);
        const auto& classification = use_facet<ctype<CharT>>(selected);
        const auto matches = [&](CharT value, char expected) {
            return value == classification.widen(expected);
        };
        const auto digit_value = [&](CharT value) {
            for (int digit = 0; digit != 10; ++digit)
                if (matches(value, static_cast<char>('0' + digit)))
                    return digit;
            return -1;
        };
        const auto hex_value = [&](CharT value) {
            const int decimal_digit = digit_value(value);
            if (decimal_digit >= 0) return decimal_digit;
            for (int digit = 0; digit != 6; ++digit)
                if (matches(value, static_cast<char>('a' + digit)) ||
                    matches(value, static_cast<char>('A' + digit)))
                    return digit + 10;
            return -1;
        };
        const CharT decimal = punctuation.decimal_point();
        const CharT separator = punctuation.thousands_sep();
        __num_get_detail::numeric_token_buffer storage(
            __num_get_detail::numeric_field_capacity + 1u);
        if (!storage.valid()) {
            error |= __locale_failbit();
            return input;
        }
        char* token = storage.data;
        size_t length = 0;
        iter_type cursor = input;
        bool token_overflow = false;
        bool decimal_seen = false;
        bool hexadecimal = false;
        bool special_payload_closed = false;
        bool special_word = false;
        bool infinity_suffix_valid = false;
        bool exponent_seen = false;
        while (cursor != end) {
            const CharT current = static_cast<CharT>(*cursor);
            if (special_payload_closed &&
                (current != static_cast<CharT>(' ') &&
                 current != static_cast<CharT>('\t') &&
                 current != static_cast<CharT>('\n') &&
                 current != static_cast<CharT>('\r'))) {
                break;
            }
            const bool sign = length == 0u &&
                (matches(current, '+') || matches(current, '-'));
            const size_t sign_offset = length != 0u &&
                (token[0] == '+' || token[0] == '-') ? 1u : 0u;
            if (length == sign_offset &&
                (matches(current, 'i') || matches(current, 'I') ||
                 matches(current, 'n') || matches(current, 'N')))
                special_word = true;
            const bool prefix = length == sign_offset + 1u &&
                token[sign_offset] == '0' &&
                (matches(current, 'x') || matches(current, 'X'));
            if (prefix) hexadecimal = true;
            const int value_digit = digit_value(current);
            const int value_hex = hex_value(current);
            const bool hex_digit = hexadecimal && value_hex >= 10;
            const bool digit = value_digit >= 0 || hex_digit;
            const bool point = !decimal_seen &&
                (current == decimal ||
                 (hexadecimal && current == static_cast<CharT>('.') &&
                  current != separator));
            const bool exponent = matches(current, 'e') ||
                matches(current, 'E') ||
                (hexadecimal && (matches(current, 'p') ||
                                 matches(current, 'P')));
            bool alpha = false;
            bool special_punctuation = false;
            if (!hexadecimal && special_word) {
                const size_t word_offset = sign_offset;
                const size_t word_length = length - word_offset;
                const char first = word_length == 0u
                    ? static_cast<char>(current) : token[word_offset];
                if (first == 'i' || first == 'I') {
                    /* `strtold` accepts the complete `inf` spelling and the
                     * optional `inity` suffix.  Look ahead before consuming
                     * the first suffix character so an invalid extension
                     * such as `infx` remains the caller's delimiter. */
                    static const char infinity[] = "infinity";
                    if (word_length < sizeof(infinity) - 1u) {
                        if (word_length < 3u) {
                            alpha = matches(current, infinity[word_length]);
                        } else if (word_length == 3u && matches(current, 'i')) {
                            iter_type probe = cursor;
                            bool complete = true;
                            for (size_t suffix = 3u;
                                 suffix != sizeof(infinity) - 1u;
                                 ++suffix) {
                                if (probe == end ||
                                    !matches(static_cast<CharT>(*probe),
                                             infinity[suffix])) {
                                    complete = false;
                                    break;
                                }
                                ++probe;
                            }
                            alpha = complete;
                            infinity_suffix_valid = complete;
                        } else if (infinity_suffix_valid) {
                            alpha = matches(current, infinity[word_length]);
                        }
                    }
                } else if (first == 'n' || first == 'N') {
                    static const char nan[] = "nan";
                    if (word_length < 3u) {
                        alpha = matches(current, nan[word_length]);
                    } else if (word_length == 3u && current == static_cast<CharT>('(')) {
                        special_punctuation = true;
                    } else if (word_length >= 4u && token[word_offset + 3u] == '(') {
                        alpha = (current >= static_cast<CharT>('a') &&
                                 current <= static_cast<CharT>('z')) ||
                                (current >= static_cast<CharT>('A') &&
                                 current <= static_cast<CharT>('Z'));
                        special_punctuation =
                            current == static_cast<CharT>(')') ||
                            current == static_cast<CharT>('_');
                    }
                }
            }
            const bool exponent_sign = length != 0u &&
                (token[length - 1u] == 'e' || token[length - 1u] == 'E' ||
                 token[length - 1u] == 'p' || token[length - 1u] == 'P') &&
                (matches(current, '+') || matches(current, '-'));
            const bool grouped = current == separator &&
                !decimal_seen && !exponent_seen &&
                !punctuation.grouping().empty();
            if (!(sign || prefix || digit || point || exponent ||
                  alpha || special_punctuation ||
                  exponent_sign || grouped)) break;
            char normalized_character;
            if (value_digit >= 0)
                normalized_character = static_cast<char>('0' + value_digit);
            else if (hex_digit)
                normalized_character = static_cast<char>('a' + value_hex - 10);
            else if (matches(current, '+'))
                normalized_character = '+';
            else if (matches(current, '-'))
                normalized_character = '-';
            else if (matches(current, 'e') || matches(current, 'E'))
                normalized_character = 'e';
            else if (matches(current, 'p') || matches(current, 'P'))
                normalized_character = 'p';
            else
                normalized_character = static_cast<char>(current);
            if (length < __num_get_detail::numeric_field_capacity)
                token[length++] = normalized_character;
            else
                token_overflow = true;
            if (current == static_cast<CharT>(')'))
                special_payload_closed = true;
            if (point) decimal_seen = true;
            if (exponent) exponent_seen = true;
            ++cursor;
        }
        if (token_overflow) {
            error |= __locale_failbit();
            return cursor;
        }
        if (length == 0u) {
            error |= __locale_failbit();
            if (cursor == end) error |= __locale_eofbit();
            return cursor;
        }

        const string grouping = punctuation.grouping();
        bool invalid_grouping = false;
        if (!hexadecimal) {
            size_t integer_end = length;
            for (size_t i = 0u; i < length; ++i) {
                if (token[i] == static_cast<char>(decimal) ||
                    token[i] == 'e' || token[i] == 'E') {
                    integer_end = i;
                    break;
                }
            }
            for (size_t i = 0; i < integer_end; ++i) {
                if (token[i] == static_cast<char>(separator) &&
                    !__num_get_detail::grouping_valid(
                        token, integer_end, separator, grouping)) {
                    invalid_grouping = true;
                    break;
                }
            }
        } else {
            size_t integer_end = length;
            for (size_t i = 2u; i < length; ++i) {
                if (token[i] == static_cast<char>(decimal)) {
                    integer_end = i;
                    break;
                }
            }
            if (integer_end < length) {
                size_t integer_length = integer_end - 2u;
                for (size_t i = 0u; i < integer_length; ++i) {
                    if (token[i + 2u] == static_cast<char>(separator) &&
                        !__num_get_detail::grouping_valid(
                            token + 2u, integer_length, separator,
                            grouping)) {
                        invalid_grouping = true;
                        break;
                    }
                }
            }
        }

        size_t normalized_length = 0;
        for (size_t i = 0; i < length; ++i) {
            if (token[i] == static_cast<char>(separator)) continue;
            token[normalized_length++] =
                (token[i] == static_cast<char>(decimal) ||
                 (hexadecimal && token[i] == '.') ? '.' : token[i]);
        }
        token[normalized_length] = '\0';
        char* parsed_end = nullptr;
        errno = 0;
        const long double parsed = strtold(token, &parsed_end);
        const bool special =
            (token[0] == 'i' || token[0] == 'I' ||
             token[0] == 'n' || token[0] == 'N' ||
            ((token[0] == '+' || token[0] == '-') &&
             (token[1] == 'i' || token[1] == 'I' ||
              token[1] == 'n' || token[1] == 'N')));
        bool hexadecimal_exponent = false;
        if (hexadecimal) {
            for (size_t i = 2u; i < normalized_length; ++i) {
                if (token[i] == 'p' || token[i] == 'P') {
                    hexadecimal_exponent = true;
                    break;
                }
            }
        }
        if (parsed_end == token ||
            *parsed_end != '\0' ||
            (hexadecimal && !hexadecimal_exponent) ||
            (!special && (errno == ERANGE ||
                          parsed > static_cast<long double>(
                                      numeric_limits<FloatType>::max()) ||
                          parsed < -static_cast<long double>(
                                      numeric_limits<FloatType>::max())))) {
            error |= __locale_failbit();
            return cursor;
        }
        value = static_cast<FloatType>(parsed);
        if (invalid_grouping) error |= __locale_failbit();
        if (cursor == end) error |= __locale_eofbit();
        return cursor;
    }

    virtual iter_type do_get(iter_type input, iter_type end, ios_base& stream,
                             unsigned int& error, float& value) const {
        return get_floating(input, end, stream, error, value);
    }
    virtual iter_type do_get(iter_type input, iter_type end, ios_base& stream,
                             unsigned int& error, double& value) const {
        return get_floating(input, end, stream, error, value);
    }
    virtual iter_type do_get(iter_type input, iter_type end, ios_base& stream,
                             unsigned int& error, long double& value) const {
        return get_floating(input, end, stream, error, value);
    }

    virtual iter_type do_get(iter_type input, iter_type end, ios_base&,
                             unsigned int& error, void*& value) const {
        char token[2u + 2u * sizeof(uintptr_t) + 1u];
        size_t length = 0;
        iter_type cursor = input;
        while (cursor != end && length < sizeof(token) - 1u) {
            const CharT current = static_cast<CharT>(*cursor);
            const bool prefix = length == 1u && token[0] == '0' &&
                (current == static_cast<CharT>('x') ||
                 current == static_cast<CharT>('X'));
            const bool digit = __num_get_detail::ascii_digit(current) ||
                __num_get_detail::ascii_hex_letter(current);
            if (!(prefix || digit)) break;
            token[length++] = static_cast<char>(current);
            ++cursor;
        }
        if (length == 0u) {
            error |= __locale_failbit();
            if (cursor == end) error |= __locale_eofbit();
            return cursor;
        }
        unsigned long long parsed = 0;
        if (!__num_get_detail::parse_integral(token, length, 16, parsed) ||
            parsed > static_cast<unsigned long long>(
                         numeric_limits<uintptr_t>::max())) {
            error |= __locale_failbit();
            return cursor;
        }
        value = reinterpret_cast<void*>(static_cast<uintptr_t>(parsed));
        if (cursor == end) error |= __locale_eofbit();
        return cursor;
    }

    iter_type match_bool_name(iter_type input, iter_type end,
                              const basic_string<CharT>& name,
                              bool matched_value, bool& value,
                              bool& matched) const {
        iter_type cursor = input;
        matched = false;
        for (size_t i = 0; i < name.size(); ++i) {
            if (cursor == end || static_cast<CharT>(*cursor) != name[i])
                return input;
            ++cursor;
        }
        value = matched_value;
        matched = true;
        return cursor;
    }

    virtual iter_type do_get(iter_type input, iter_type end, ios_base& stream,
                             unsigned int& error, bool& value) const {
        const unsigned int flags = __locale_stream_flags(stream);
        if ((flags & 0x0001u) == 0u) {
            long parsed = 0;
            iter_type result = get_integral(input, end, stream, error, parsed);
            if ((error & __locale_failbit()) == 0u) {
                value = parsed != 0;
                if (parsed != 0 && parsed != 1) error |= __locale_failbit();
            }
            return result;
        }

        const locale selected = __locale_stream_locale(stream);
        const auto& punctuation = use_facet<numpunct<CharT>>(selected);
        value = false;
        bool matched = false;
        iter_type cursor = match_bool_name(
            input, end, punctuation.truename(), true, value, matched);
        if (matched) {
            if (cursor == end) error |= __locale_eofbit();
            return cursor;
        }
        cursor = match_bool_name(
            input, end, punctuation.falsename(), false, value, matched);
        if (matched) {
            if (cursor == end) error |= __locale_eofbit();
            return cursor;
        }
        error |= __locale_failbit();
        if (input == end) error |= __locale_eofbit();
        return input;
    }
};

template<typename CharT, typename InputIterator>
locale::id num_get<CharT, InputIterator>::id;

/* ═══════════════════════════════════════════════════════════════
 * num_put - bounded integral output facet
 * ═══════════════════════════════════════════════════════════════ */

namespace __num_put_detail {

template<typename CharT, typename OutputIterator>
inline OutputIterator emit(OutputIterator output, const char* text,
                           size_t length) {
    for (size_t i = 0; i < length; ++i)
        *output++ = static_cast<CharT>(text[i]);
    return output;
}

template<typename CharT, typename OutputIterator>
inline OutputIterator emit_repeated(OutputIterator output, CharT value,
                                    size_t count) {
    for (size_t i = 0; i < count; ++i) *output++ = value;
    return output;
}

template<typename CharT, typename OutputIterator, typename T>
inline OutputIterator put_integral(OutputIterator output, ios_base& stream,
                                   CharT fill, T value) {
    const unsigned int flags = __locale_stream_flags(stream);
    const locale selected = __locale_stream_locale(stream);
    const auto& punctuation = use_facet<numpunct<CharT>>(selected);
    const bool negative = is_signed<T>::value && value < 0;
    unsigned long long magnitude = 0;
    if (negative)
        magnitude = 0ull - static_cast<unsigned long long>(value);
    else
        magnitude = static_cast<unsigned long long>(value);

    const int base = (flags & 0x0008u) != 0u
        ? 16 : ((flags & 0x0040u) != 0u ? 8 : 10);
    const char* alphabet = (flags & 0x4000u) != 0u
        ? "0123456789ABCDEF" : "0123456789abcdef";
    char reversed[128];
    size_t raw_length = 0;
    do {
        reversed[raw_length++] = alphabet[magnitude %
                                           static_cast<unsigned long long>(base)];
        magnitude /= static_cast<unsigned long long>(base);
    } while (magnitude != 0 && raw_length < sizeof(reversed));

    char digits[4097];
    size_t digit_length = 0;
    const string grouping = punctuation.grouping();
    const CharT separator = punctuation.thousands_sep();
    size_t group_size = 0;
    if (base == 10 && !grouping.empty()) {
        group_size = static_cast<unsigned char>(grouping[0]);
        if (group_size == 0u || group_size == 0xffu) group_size = 0u;
    }
    char grouped_reverse[4097];
    size_t grouped_length = 0;
    for (size_t index = 0; index < raw_length; ++index) {
        if (group_size != 0u && index != 0u && index % group_size == 0u)
            grouped_reverse[grouped_length++] = static_cast<char>(separator);
        grouped_reverse[grouped_length++] = reversed[index];
    }
    for (size_t index = grouped_length; index-- > 0;)
        digits[digit_length++] = grouped_reverse[index];

    char field[4097];
    size_t field_length = 0;
    if (negative) field[field_length++] = '-';
    else if ((flags & 0x0800u) != 0u) field[field_length++] = '+';
    if ((flags & 0x0200u) != 0u && base == 16) {
        field[field_length++] = '0';
        field[field_length++] = (flags & 0x4000u) != 0u ? 'X' : 'x';
    } else if ((flags & 0x0200u) != 0u && base == 8 &&
               field_length < sizeof(field)) {
        if (raw_length != 1u || reversed[0] != '0') field[field_length++] = '0';
    }
    for (size_t i = 0; i < digit_length && field_length < sizeof(field); ++i)
        field[field_length++] = digits[i];

    long requested = __locale_stream_width(stream);
    if (requested < 0) requested = 0;
    size_t width = static_cast<size_t>(requested);
    const size_t padding = width > field_length ? width - field_length : 0u;
    const bool left = (flags & 0x0020u) != 0u;
    const bool internal = (flags & 0x0010u) != 0u;
    if (!left && !internal)
        output = emit_repeated<CharT>(output, fill, padding);
    if (internal && padding != 0u) {
        size_t prefix_length = (negative || (flags & 0x0800u) != 0u) ? 1u : 0u;
        if ((flags & 0x0200u) != 0u && base == 16) prefix_length += 2u;
        if ((flags & 0x0200u) != 0u && base == 8 &&
            (raw_length != 1u || reversed[0] != '0')) ++prefix_length;
        output = emit<CharT>(output, field, prefix_length);
        output = emit_repeated<CharT>(output, fill, padding);
        output = emit<CharT>(output, field + prefix_length,
                             field_length - prefix_length);
    } else {
        output = emit<CharT>(output, field, field_length);
    }
    if (left) output = emit_repeated<CharT>(output, fill, padding);
    __locale_stream_width_reset(stream);
    return output;
}

template<typename CharT, typename OutputIterator>
inline OutputIterator put_pointer(OutputIterator output, ios_base& stream,
                                  CharT fill, const void* value) {
    const unsigned long long address = static_cast<unsigned long long>(
        detail::object_pointer_hash(value));
    const char* alphabet = "0123456789abcdef";
    char reversed[2u * sizeof(uintptr_t)];
    size_t raw_length = 0;
    unsigned long long remaining = address;
    do {
        reversed[raw_length++] = alphabet[remaining & 0xfu];
        remaining >>= 4u;
    } while (remaining != 0u && raw_length < sizeof(reversed));

    char field[2u + 2u * sizeof(uintptr_t)];
    size_t field_length = 0;
    field[field_length++] = '0';
    field[field_length++] = 'x';
    while (raw_length != 0u) field[field_length++] = reversed[--raw_length];
    long requested = __locale_stream_width(stream);
    if (requested < 0) requested = 0;
    size_t width = static_cast<size_t>(requested);
    const size_t padding = width > field_length ? width - field_length : 0u;
    const bool left = (__locale_stream_flags(stream) & 0x0020u) != 0u;
    const bool internal = (__locale_stream_flags(stream) & 0x0010u) != 0u;
    if (!left && !internal)
        output = emit_repeated<CharT>(output, fill, padding);
    if (internal && padding != 0u) {
        output = emit<CharT>(output, field, 2u);
        output = emit_repeated<CharT>(output, fill, padding);
        output = emit<CharT>(output, field + 2u, field_length - 2u);
    } else {
        output = emit<CharT>(output, field, field_length);
    }
    if (left) output = emit_repeated<CharT>(output, fill, padding);
    __locale_stream_width_reset(stream);
    return output;
}

template<typename CharT, typename OutputIterator, typename FloatType>
inline OutputIterator put_floating(OutputIterator output, ios_base& stream,
                                   CharT fill, FloatType value) {
    __num_get_detail::numeric_token_buffer raw_storage(
        __num_get_detail::numeric_field_capacity + 1u);
    if (!raw_storage.valid()) {
        __locale_stream_width_reset(stream);
        __locale_stream_fail(stream);
        return output;
    }
    char* raw = raw_storage.data;
    size_t raw_length = 0;
    long precision = __locale_stream_precision(stream);
    if (precision < 0) precision = 6;
    if (precision > static_cast<long>(
                       __num_get_detail::numeric_field_capacity - 2u)) {
        __locale_stream_width_reset(stream);
        __locale_stream_fail(stream);
        return output;
    }
    const unsigned int flags = __locale_stream_flags(stream);
#if __cplusplus >= 201703L
    chars_format format = chars_format::general;
    if ((flags & 0x0104u) == 0x0104u) format = chars_format::hex;
    else if ((flags & 0x0100u) != 0u) format = chars_format::scientific;
    else if ((flags & 0x0004u) != 0u) format = chars_format::fixed;
    to_chars_result converted = format == chars_format::hex
        ? to_chars(raw, raw + __num_get_detail::numeric_field_capacity,
                   value, format)
        : to_chars(raw, raw + __num_get_detail::numeric_field_capacity,
                   value, format,
                   static_cast<int>(precision));
    if (converted.ec != errc{}) {
        __locale_stream_width_reset(stream);
        __locale_stream_fail(stream);
        return output;
    }
    raw_length = static_cast<size_t>(converted.ptr - raw);
#else
    char format[16];
    size_t format_length = 0;
    format[format_length++] = '%';
    if ((flags & 0x0400u) != 0u) format[format_length++] = '#';
    format[format_length++] = '.';
    format[format_length++] = '*';
#if defined(_WIN32) && !defined(RIN_FREESTANDING)
    /* MSVCRT/UCRT treats `long double` as the same ABI as `double`, but its
     * printf parser still consumes `%f` for that argument.  Passing `%Lf`
     * therefore reads the varargs slot with the wrong width and silently
     * formats values such as 1.25 as 0.00.  Keep the freestanding target's
     * native long-double spelling below while using the hosted Windows ABI. */
    const bool msvc_long_double_alias = true;
#else
    const bool msvc_long_double_alias = false;
#endif
    if (!msvc_long_double_alias) format[format_length++] = 'L';
    format[format_length++] = (flags & 0x0104u) == 0x0104u ? 'a'
        : (flags & 0x0100u) != 0u ? 'e'
        : (flags & 0x0004u) != 0u ? 'f' : 'g';
    format[format_length] = '\0';
    const int written = msvc_long_double_alias
        ? ::snprintf(raw, __num_get_detail::numeric_field_capacity + 1u, format,
                     static_cast<int>(precision), static_cast<double>(value))
        : ::snprintf(raw, __num_get_detail::numeric_field_capacity + 1u, format,
                     static_cast<int>(precision),
                     static_cast<long double>(value));
    if (written < 0 || static_cast<size_t>(written) >=
                           __num_get_detail::numeric_field_capacity) {
        __locale_stream_width_reset(stream);
        __locale_stream_fail(stream);
        return output;
    }
    raw_length = static_cast<size_t>(written);
#endif

    /* C++11 has no Rin charconv float backend in the public surface, so the
     * compatibility path above uses snprintf.  Its %a spelling is valid but
     * not the canonical shortest hexfloat form required by iostreams (for
     * example, 1.25 becomes 0xa.000000p-3).  Normalize the binary significand
     * without converting through a narrower floating type. */
#if __cplusplus < 201703L
    if ((flags & 0x0104u) == 0x0104u && raw_length != 0u) {
        const size_t sign_offset = raw[0] == '-' ? 1u : 0u;
        size_t prefix = sign_offset;
        if (prefix + 2u <= raw_length && raw[prefix] == '0' &&
            (raw[prefix + 1u] == 'x' || raw[prefix + 1u] == 'X'))
            prefix += 2u;
        size_t point = prefix;
        while (point < raw_length && raw[point] != '.') ++point;
        size_t exponent_marker = prefix;
        while (exponent_marker < raw_length && raw[exponent_marker] != 'p' &&
               raw[exponent_marker] != 'P') ++exponent_marker;
        if (prefix != sign_offset + 2u || point >= raw_length ||
            exponent_marker >= raw_length || point > exponent_marker) {
            __locale_stream_width_reset(stream);
            __locale_stream_fail(stream);
            return output;
        }
        int exponent = 0;
        size_t exponent_cursor = exponent_marker + 1u;
        bool exponent_negative = false;
        if (exponent_cursor < raw_length &&
            (raw[exponent_cursor] == '+' || raw[exponent_cursor] == '-')) {
            exponent_negative = raw[exponent_cursor] == '-';
            ++exponent_cursor;
        }
        if (exponent_cursor == raw_length) {
            __locale_stream_width_reset(stream);
            __locale_stream_fail(stream);
            return output;
        }
        for (; exponent_cursor < raw_length; ++exponent_cursor) {
            if (raw[exponent_cursor] < '0' || raw[exponent_cursor] > '9' ||
                exponent > 100000000) {
                __locale_stream_width_reset(stream);
                __locale_stream_fail(stream);
                return output;
            }
            exponent = exponent * 10 + (raw[exponent_cursor] - '0');
        }
        if (exponent_negative) exponent = -exponent;

        __num_get_detail::numeric_token_buffer digits_storage(
            __num_get_detail::numeric_field_capacity + 1u);
        if (!digits_storage.valid()) {
            __locale_stream_width_reset(stream);
            __locale_stream_fail(stream);
            return output;
        }
        char* digits = digits_storage.data;
        size_t digit_count = 0u;
        for (size_t cursor = prefix; cursor < exponent_marker; ++cursor) {
            if (raw[cursor] == '.') continue;
            const char value_digit = raw[cursor];
            if (!((value_digit >= '0' && value_digit <= '9') ||
                  (value_digit >= 'a' && value_digit <= 'f') ||
                  (value_digit >= 'A' && value_digit <= 'F')) ||
                digit_count >= __num_get_detail::numeric_field_capacity) {
                __locale_stream_width_reset(stream);
                __locale_stream_fail(stream);
                return output;
            }
            digits[digit_count++] = value_digit;
        }
        const size_t decimal_digit_count = point - prefix;
        size_t first_digit = 0u;
        while (first_digit < digit_count) {
            const char value_digit = digits[first_digit];
            const unsigned int numeric =
                value_digit <= '9' ? static_cast<unsigned int>(value_digit - '0')
                : value_digit <= 'F' ? static_cast<unsigned int>(value_digit - 'A' + 10)
                : static_cast<unsigned int>(value_digit - 'a' + 10);
            if (numeric != 0u) break;
            ++first_digit;
        }
        if (first_digit == digit_count) {
            /* Preserve a signed zero while dropping snprintf's padding. */
            char normalized[8];
            size_t normalized_length = sign_offset;
            if (sign_offset != 0u) normalized[0] = '-';
            normalized[normalized_length++] = '0';
            normalized[normalized_length++] = 'x';
            normalized[normalized_length++] = '0';
            normalized[normalized_length++] = 'p';
            normalized[normalized_length++] = '+';
            normalized[normalized_length++] = '0';
            for (size_t cursor = 0u; cursor < normalized_length; ++cursor)
                raw[cursor] = normalized[cursor];
            raw_length = normalized_length;
        } else {
            const char value_digit = digits[first_digit];
            const unsigned int numeric =
                value_digit <= '9' ? static_cast<unsigned int>(value_digit - '0')
                : value_digit <= 'F' ? static_cast<unsigned int>(value_digit - 'A' + 10)
                : static_cast<unsigned int>(value_digit - 'a' + 10);
            unsigned int high_bit = 0u;
            if (numeric >= 8u) high_bit = 3u;
            else if (numeric >= 4u) high_bit = 2u;
            else if (numeric >= 2u) high_bit = 1u;
            const int normalized_exponent = exponent +
                static_cast<int>(high_bit) -
                4 * (static_cast<int>(first_digit) -
                     static_cast<int>(decimal_digit_count) + 1);
            const size_t fraction_capacity =
                __num_get_detail::numeric_field_capacity * 4u;
            __num_get_detail::numeric_token_buffer fraction_storage(
                fraction_capacity + 1u);
            if (!fraction_storage.valid()) {
                __locale_stream_width_reset(stream);
                __locale_stream_fail(stream);
                return output;
            }
            unsigned char* fraction_bits =
                reinterpret_cast<unsigned char*>(fraction_storage.data);
            size_t fraction_count = 0u;
            for (size_t digit_index = first_digit; digit_index < digit_count;
                 ++digit_index) {
                const char current = digits[digit_index];
                const unsigned int value =
                    current <= '9' ? static_cast<unsigned int>(current - '0')
                    : current <= 'F' ? static_cast<unsigned int>(current - 'A' + 10)
                    : static_cast<unsigned int>(current - 'a' + 10);
                const unsigned int bit_count = digit_index == first_digit
                    ? (high_bit == 0u ? 0u : high_bit - 1u) : 4u;
                for (int bit = static_cast<int>(bit_count) - 1; bit >= 0; --bit) {
                    if (fraction_count >= fraction_capacity) {
                        __locale_stream_width_reset(stream);
                        __locale_stream_fail(stream);
                        return output;
                    }
                    fraction_bits[fraction_count++] =
                        static_cast<unsigned char>((value >> bit) & 1u);
                }
            }
            size_t fraction_digits = (fraction_count + 3u) / 4u;
            while (fraction_digits != 0u) {
                const size_t begin = (fraction_digits - 1u) * 4u;
                bool nonzero = false;
                for (size_t bit = begin; bit < fraction_count; ++bit)
                    if (fraction_bits[bit] != 0u) nonzero = true;
                if (nonzero) break;
                --fraction_digits;
            }
            __num_get_detail::numeric_token_buffer normalized_storage(
                __num_get_detail::numeric_field_capacity + 1u);
            if (!normalized_storage.valid()) {
                __locale_stream_width_reset(stream);
                __locale_stream_fail(stream);
                return output;
            }
            char* normalized = normalized_storage.data;
            size_t normalized_length = sign_offset;
            if (sign_offset != 0u) normalized[0] = '-';
            normalized[normalized_length++] = '0';
            normalized[normalized_length++] = 'x';
            normalized[normalized_length++] = '1';
            if (fraction_digits != 0u) {
                normalized[normalized_length++] = '.';
                for (size_t digit = 0u; digit < fraction_digits; ++digit) {
                    unsigned int value = 0u;
                    for (size_t bit = 0u; bit < 4u; ++bit) {
                        const size_t index = digit * 4u + bit;
                        value = (value << 1u) |
                            (index < fraction_count ? fraction_bits[index] : 0u);
                    }
                    normalized[normalized_length++] =
                        value < 10u ? static_cast<char>('0' + value)
                                    : static_cast<char>('a' + value - 10u);
                }
            }
            normalized[normalized_length++] = 'p';
            normalized[normalized_length++] = normalized_exponent < 0 ? '-' : '+';
            unsigned int magnitude = static_cast<unsigned int>(
                normalized_exponent < 0 ? -normalized_exponent : normalized_exponent);
            char reversed[16];
            size_t reversed_length = 0u;
            do {
                reversed[reversed_length++] =
                    static_cast<char>('0' + magnitude % 10u);
                magnitude /= 10u;
            } while (magnitude != 0u);
            while (reversed_length != 0u)
                normalized[normalized_length++] = reversed[--reversed_length];
            for (size_t cursor = 0u; cursor < normalized_length; ++cursor)
                raw[cursor] = normalized[cursor];
            raw_length = normalized_length;
        }
    }
#endif

    __num_get_detail::numeric_token_buffer field_storage(
        __num_get_detail::numeric_field_capacity + 1u);
    if (!field_storage.valid()) {
        __locale_stream_width_reset(stream);
        __locale_stream_fail(stream);
        return output;
    }
    char* field = field_storage.data;
    size_t field_length = 0;
    if (raw_length != 0u && raw[0] != '-' &&
        (flags & 0x0800u) != 0u)
        field[field_length++] = '+';
    const bool hexadecimal = (flags & 0x0104u) == 0x0104u;
    const bool raw_special = raw_length != 0u &&
        (raw[0] == 'i' || raw[0] == 'I' || raw[0] == 'n' || raw[0] == 'N');
    if (hexadecimal && !raw_special &&
        !(raw_length >= 2u && raw[0] == '0' &&
          (raw[1] == 'x' || raw[1] == 'X'))) {
        field[field_length++] = '0';
        field[field_length++] = (flags & 0x4000u) != 0u ? 'X' : 'x';
    }
    if (field_length > __num_get_detail::numeric_field_capacity ||
        raw_length > __num_get_detail::numeric_field_capacity - field_length) {
        __locale_stream_width_reset(stream);
        __locale_stream_fail(stream);
        return output;
    }
    for (size_t i = 0; i < raw_length; ++i) {
        char current = raw[i];
        if ((flags & 0x4000u) != 0u && current >= 'a' && current <= 'z')
            current = static_cast<char>(current - 'a' + 'A');
        field[field_length++] = current;
    }
    const locale selected = __locale_stream_locale(stream);
    const auto& punctuation = use_facet<numpunct<CharT>>(selected);
    const CharT decimal = punctuation.decimal_point();
    if (static_cast<unsigned int>(decimal) > 0x7fu) {
        __locale_stream_width_reset(stream);
        __locale_stream_fail(stream);
        return output;
    }
    if (decimal != static_cast<CharT>('.') &&
        field_length < __num_get_detail::numeric_field_capacity) {
        for (size_t i = 0; i < field_length; ++i)
            if (field[i] == '.') field[i] = static_cast<char>(decimal);
    }
    if ((flags & 0x0400u) != 0u) {
        size_t marker = field_length;
        for (size_t i = 0; i < field_length; ++i) {
            if (field[i] == 'e' || field[i] == 'E' ||
                field[i] == 'p' || field[i] == 'P') {
                marker = i;
                break;
            }
        }
        bool point = false;
        for (size_t i = 0; i < marker; ++i)
            point = point || field[i] == static_cast<char>(decimal);
        if (!point && marker + 2u <
                         __num_get_detail::numeric_field_capacity) {
            for (size_t i = field_length + 1u; i > marker; --i)
                field[i - 1u] = field[i - 2u];
            field[marker] = static_cast<char>(decimal);
            field[marker + 1u] = '0';
            ++field_length;
            ++marker;
            point = true;
        }
        if ((flags & 0x0104u) == 0u &&
            (flags & 0x0100u) == 0u &&
            marker <= field_length) {
            size_t significant = 0u;
            bool found_nonzero = false;
            for (size_t i = 0u; i < marker; ++i) {
                const char current = field[i];
                if (current == static_cast<char>(decimal)) continue;
                if (current < '0' || current > '9') continue;
                if (current != '0') found_nonzero = true;
                if (found_nonzero) ++significant;
            }
            const size_t desired = precision < 0
                ? 6u : static_cast<size_t>(precision);
            if (significant < desired &&
                field_length + desired - significant <
                    __num_get_detail::numeric_field_capacity) {
                const size_t extra = desired - significant;
                for (size_t i = field_length + extra; i > marker; --i)
                    field[i - 1u] = field[i - extra - 1u];
                for (size_t i = 0u; i < extra; ++i)
                    field[marker + i] = '0';
                field_length += extra;
            }
        }
    }
    if (!hexadecimal) {
        const string grouping = punctuation.grouping();
        const CharT separator = punctuation.thousands_sep();
        if (!grouping.empty() && static_cast<unsigned int>(separator) <= 0x7fu) {
            size_t mantissa_begin = 0u;
            if (field_length != 0u &&
                (field[0] == '+' || field[0] == '-'))
                mantissa_begin = 1u;
            size_t integer_end = field_length;
            for (size_t i = mantissa_begin; i < field_length; ++i) {
                if (field[i] == static_cast<char>(decimal) ||
                    field[i] == 'e' || field[i] == 'E') {
                    integer_end = i;
                    break;
                }
            }
            if (integer_end > mantissa_begin) {
                __num_get_detail::numeric_token_buffer marks_storage(
                    __num_get_detail::numeric_field_capacity + 1u);
                if (!marks_storage.valid()) {
                    __locale_stream_width_reset(stream);
                    __locale_stream_fail(stream);
                    return output;
                }
                char* marks = marks_storage.data;
                for (size_t i = 0u; i <
                         __num_get_detail::numeric_field_capacity + 1u; ++i)
                    marks[i] = 0;
                size_t cursor = integer_end;
                size_t pattern = 0u;
                unsigned int previous_group = 0u;
                size_t boundary_count = 0u;
                while (cursor > mantissa_begin && pattern < grouping.size()) {
                    const unsigned int encoded = static_cast<unsigned char>(
                        grouping[pattern]);
                    if (encoded == static_cast<unsigned int>(CHAR_MAX)) break;
                    const unsigned int group = encoded == 0u
                        ? previous_group : encoded;
                    if (group == 0u || cursor - mantissa_begin <= group) break;
                    cursor -= group;
                    marks[cursor] = 1;
                    ++boundary_count;
                    if (encoded != 0u) previous_group = encoded;
                    ++pattern;
                    if (pattern == grouping.size() && previous_group != 0u &&
                        grouping[grouping.size() - 1u] !=
                            static_cast<char>(CHAR_MAX))
                        pattern = grouping.size() - 1u;
                }
                if (boundary_count != 0u &&
                    field_length + boundary_count <
                        __num_get_detail::numeric_field_capacity) {
                    __num_get_detail::numeric_token_buffer grouped_storage(
                        __num_get_detail::numeric_field_capacity + 1u);
                    if (!grouped_storage.valid()) {
                        __locale_stream_width_reset(stream);
                        __locale_stream_fail(stream);
                        return output;
                    }
                    char* grouped = grouped_storage.data;
                    size_t grouped_length = 0u;
                    for (size_t i = 0u; i < field_length; ++i) {
                        if (marks[i] != 0)
                            grouped[grouped_length++] = static_cast<char>(separator);
                        grouped[grouped_length++] = field[i];
                    }
                    for (size_t i = 0u; i < grouped_length; ++i)
                        field[i] = grouped[i];
                    field_length = grouped_length;
                }
            }
        }
    }
    long requested = __locale_stream_width(stream);
    if (requested < 0) requested = 0;
    size_t width = static_cast<size_t>(requested);
    const size_t padding = width > field_length ? width - field_length : 0u;
    const bool left = (flags & 0x0020u) != 0u;
    const bool internal = (flags & 0x0010u) != 0u;
    if (!left && !internal)
        output = emit_repeated<CharT>(output, fill, padding);
    if (internal && padding != 0u) {
        size_t prefix = field_length != 0u &&
            (field[0] == '+' || field[0] == '-') ? 1u : 0u;
        if (field_length >= prefix + 2u && field[prefix] == '0' &&
            (field[prefix + 1u] == 'x' || field[prefix + 1u] == 'X'))
            prefix += 2u;
        output = emit<CharT>(output, field, prefix);
        output = emit_repeated<CharT>(output, fill, padding);
        output = emit<CharT>(output, field + prefix, field_length - prefix);
    } else {
        output = emit<CharT>(output, field, field_length);
    }
    if (left) output = emit_repeated<CharT>(output, fill, padding);
    __locale_stream_width_reset(stream);
    return output;
}

} /* namespace __num_put_detail */

template<typename CharT, typename OutputIterator = ostreambuf_iterator<CharT>>
class num_put : public locale::facet {
public:
    using char_type = CharT;
    using iter_type = OutputIterator;

    static locale::id id;

    explicit num_put(size_t refs = 0) : locale::facet(refs) {}

    iter_type put(iter_type output, ios_base& stream, CharT fill,
                  bool value) const { return do_put(output, stream, fill, value); }
    iter_type put(iter_type output, ios_base& stream, CharT fill,
                  long value) const { return do_put(output, stream, fill, value); }
    iter_type put(iter_type output, ios_base& stream, CharT fill,
                  unsigned long value) const {
        return do_put(output, stream, fill, value);
    }
    iter_type put(iter_type output, ios_base& stream, CharT fill,
                  long long value) const {
        return do_put(output, stream, fill, value);
    }
    iter_type put(iter_type output, ios_base& stream, CharT fill,
                  unsigned long long value) const {
        return do_put(output, stream, fill, value);
    }
    iter_type put(iter_type output, ios_base& stream, CharT fill,
                  const void* value) const {
        return do_put(output, stream, fill, value);
    }
    iter_type put(iter_type output, ios_base& stream, CharT fill,
                  float value) const { return do_put(output, stream, fill, value); }
    iter_type put(iter_type output, ios_base& stream, CharT fill,
                  double value) const { return do_put(output, stream, fill, value); }
    iter_type put(iter_type output, ios_base& stream, CharT fill,
                  long double value) const {
        return do_put(output, stream, fill, value);
    }

protected:
    virtual ~num_put() = default;

    virtual iter_type do_put(iter_type output, ios_base& stream, CharT fill,
                             bool value) const {
        if ((__locale_stream_flags(stream) & 0x0001u) != 0u) {
            const locale selected = __locale_stream_locale(stream);
            const auto& punctuation = use_facet<numpunct<CharT>>(selected);
            const auto& text = value ? punctuation.truename()
                                     : punctuation.falsename();
            long requested = __locale_stream_width(stream);
            if (requested < 0) requested = 0;
            size_t width = static_cast<size_t>(requested);
            const size_t padding = width > text.size()
                ? width - text.size() : 0u;
            if ((__locale_stream_flags(stream) & 0x0020u) == 0u)
                output = __num_put_detail::emit_repeated<CharT>(
                    output, fill, padding);
            for (size_t i = 0; i < text.size(); ++i) *output++ = text[i];
            if ((__locale_stream_flags(stream) & 0x0020u) != 0u)
                output = __num_put_detail::emit_repeated<CharT>(
                    output, fill, padding);
            __locale_stream_width_reset(stream);
            return output;
        }
        return __num_put_detail::put_integral(output, stream, fill,
                                              static_cast<long>(value));
    }
    virtual iter_type do_put(iter_type output, ios_base& stream, CharT fill,
                             long value) const {
        return __num_put_detail::put_integral(output, stream, fill, value);
    }
    virtual iter_type do_put(iter_type output, ios_base& stream, CharT fill,
                             unsigned long value) const {
        return __num_put_detail::put_integral(output, stream, fill, value);
    }
    virtual iter_type do_put(iter_type output, ios_base& stream, CharT fill,
                             long long value) const {
        return __num_put_detail::put_integral(output, stream, fill, value);
    }
    virtual iter_type do_put(iter_type output, ios_base& stream, CharT fill,
                             unsigned long long value) const {
        return __num_put_detail::put_integral(output, stream, fill, value);
    }
    virtual iter_type do_put(iter_type output, ios_base& stream, CharT fill,
                             const void* value) const {
        return __num_put_detail::put_pointer(output, stream, fill, value);
    }
    virtual iter_type do_put(iter_type output, ios_base& stream, CharT fill,
                             float value) const {
        return __num_put_detail::put_floating(output, stream, fill, value);
    }
    virtual iter_type do_put(iter_type output, ios_base& stream, CharT fill,
                             double value) const {
        return __num_put_detail::put_floating(output, stream, fill, value);
    }
    virtual iter_type do_put(iter_type output, ios_base& stream, CharT fill,
                             long double value) const {
        return __num_put_detail::put_floating(output, stream, fill, value);
    }
};

template<typename CharT, typename OutputIterator>
locale::id num_put<CharT, OutputIterator>::id;

/* ═══════════════════════════════════════════════════════════════
 * collate - 文字列照合
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT>
class collate : public locale::facet {
public:
    using char_type = CharT;
    using string_type = basic_string<CharT>;

    static locale::id id;

    explicit collate(size_t refs = 0) : locale::facet(refs) {}

    int compare(const CharT* lo1, const CharT* hi1,
                const CharT* lo2, const CharT* hi2) const {
        return do_compare(lo1, hi1, lo2, hi2);
    }

    string_type transform(const CharT* lo, const CharT* hi) const {
        return do_transform(lo, hi);
    }

    long hash(const CharT* lo, const CharT* hi) const {
        return do_hash(lo, hi);
    }

protected:
    virtual ~collate() = default;

    virtual int do_compare(const CharT* lo1, const CharT* hi1,
                           const CharT* lo2, const CharT* hi2) const {
        for (; lo1 != hi1 && lo2 != hi2; ++lo1, ++lo2) {
            if (*lo1 < *lo2) return -1;
            if (*lo1 > *lo2) return 1;
        }
        if (lo1 == hi1 && lo2 == hi2) return 0;
        return (lo1 == hi1) ? -1 : 1;
    }

    virtual string_type do_transform(const CharT* lo, const CharT* hi) const {
        return string_type(lo, hi - lo);
    }

    virtual long do_hash(const CharT* lo, const CharT* hi) const {
        long h = 0;
        for (; lo != hi; ++lo) {
            h = h * 31 + static_cast<long>(*lo);
        }
        return h;
    }
};

template<typename CharT>
locale::id collate<CharT>::id;

/* ═══════════════════════════════════════════════════════════════
 * moneypunct - 通貨書式句読点
 * ═══════════════════════════════════════════════════════════════*/

class money_base {
public:
    enum part { none, space, symbol, sign, value };
    struct pattern { char field[4]; };
};

template<typename CharT, bool International = false>
class moneypunct : public locale::facet, public money_base {
public:
    using char_type = CharT;
    using string_type = basic_string<CharT>;

    static locale::id id;
    static constexpr bool intl = International;

    explicit moneypunct(size_t refs = 0) : locale::facet(refs) {}

    char_type decimal_point() const { return do_decimal_point(); }
    char_type thousands_sep() const { return do_thousands_sep(); }
    string grouping() const { return do_grouping(); }
    string_type curr_symbol() const { return do_curr_symbol(); }
    string_type positive_sign() const { return do_positive_sign(); }
    string_type negative_sign() const { return do_negative_sign(); }
    int frac_digits() const { return do_frac_digits(); }
    pattern pos_format() const { return do_pos_format(); }
    pattern neg_format() const { return do_neg_format(); }

protected:
    virtual ~moneypunct() = default;

    virtual char_type do_decimal_point() const { return '.'; }
    virtual char_type do_thousands_sep() const { return ','; }
    virtual string do_grouping() const { return ""; }
    virtual string_type do_curr_symbol() const { return string_type(); }
    virtual string_type do_positive_sign() const { return string_type(); }
    virtual string_type do_negative_sign() const { return string_type(); }
    virtual int do_frac_digits() const { return 0; }
    virtual pattern do_pos_format() const {
        pattern p;
        p.field[0] = symbol;
        p.field[1] = sign;
        p.field[2] = none;
        p.field[3] = value;
        return p;
    }
    virtual pattern do_neg_format() const { return do_pos_format(); }
};

template<typename CharT, bool Intl>
locale::id moneypunct<CharT, Intl>::id;

template<typename CharT, bool Intl = false>
class moneypunct_byname : public moneypunct<CharT, Intl> {
public:
    using char_type = CharT;
    using string_type = basic_string<CharT>;
    using pattern = typename moneypunct<CharT, Intl>::pattern;

    explicit moneypunct_byname(const char* name, size_t refs = 0)
        : moneypunct<CharT, Intl>(refs),
          profile_(__num_punct_detail::profile_for_name(name)) {
        if (profile_ < 0) __num_punct_detail::invalid_name();
    }

protected:
    ~moneypunct_byname() override = default;

    char_type do_decimal_point() const override {
        return static_cast<CharT>(
            profile_ == __num_punct_detail::profile_classic ? '.' : ',');
    }

    char_type do_thousands_sep() const override {
        if (profile_ == __num_punct_detail::profile_fr)
            return static_cast<CharT>(' ');
        if (profile_ == __num_punct_detail::profile_classic)
            return static_cast<CharT>(',');
        return static_cast<CharT>('.');
    }

    string do_grouping() const override {
        string grouping;
        if (profile_ != __num_punct_detail::profile_classic)
            grouping += static_cast<char>(3);
        return grouping;
    }

    string_type do_curr_symbol() const override {
        string_type symbol;
        symbol += static_cast<CharT>('E');
        symbol += static_cast<CharT>('U');
        symbol += static_cast<CharT>('R');
        return symbol;
    }

    string_type do_positive_sign() const override {
        return string_type();
    }

    string_type do_negative_sign() const override {
        string_type sign;
        sign += static_cast<CharT>('-');
        return sign;
    }

    int do_frac_digits() const override { return 2; }

    pattern do_pos_format() const override {
        pattern value = {{moneypunct<CharT, Intl>::symbol,
                          moneypunct<CharT, Intl>::sign,
                          moneypunct<CharT, Intl>::none,
                          moneypunct<CharT, Intl>::value}};
        return value;
    }

    pattern do_neg_format() const override {
        pattern value = {{moneypunct<CharT, Intl>::sign,
                          moneypunct<CharT, Intl>::symbol,
                          moneypunct<CharT, Intl>::none,
                          moneypunct<CharT, Intl>::value}};
        return value;
    }

private:
    int profile_;
};

template<typename CharT, bool Intl>
struct __named_moneypunct_holder final : moneypunct_byname<CharT, Intl> {
    explicit __named_moneypunct_holder(const char* name)
        : moneypunct_byname<CharT, Intl>(name, 1) {}
    ~__named_moneypunct_holder() override = default;
};

template<typename CharT, bool Intl>
struct __named_facet_provider<moneypunct<CharT, Intl>> {
    static const moneypunct<CharT, Intl>* get(const locale& value) noexcept {
        const char* name = value._name_c_str();
        if (!__classic_locale_character<CharT>::value ||
            !name || name[0] == '\0' || name[0] == '*')
            return nullptr;
        if (__num_punct_detail::name_prefix(name, "de_DE")) {
            static __named_moneypunct_holder<CharT, Intl> facet(
                "de_DE.UTF-8");
            return &facet;
        }
        if (__num_punct_detail::name_prefix(name, "fr_FR")) {
            static __named_moneypunct_holder<CharT, Intl> facet(
                "fr_FR.UTF-8");
            return &facet;
        }
        if (__num_punct_detail::name_prefix(name, "es_ES")) {
            static __named_moneypunct_holder<CharT, Intl> facet(
                "es_ES.UTF-8");
            return &facet;
        }
        if (__num_punct_detail::name_prefix(name, "it_IT")) {
            static __named_moneypunct_holder<CharT, Intl> facet(
                "it_IT.UTF-8");
            return &facet;
        }
        if (__num_punct_detail::name_prefix(name, "pt_BR")) {
            static __named_moneypunct_holder<CharT, Intl> facet(
                "pt_BR.UTF-8");
            return &facet;
        }
        return nullptr;
    }
};

/* ═══════════════════════════════════════════════════════════════
 * codecvt_base
 * ═══════════════════════════════════════════════════════════════*/

class codecvt_base {
public:
    enum result { ok, partial, error, noconv };
};

/* std::mbstate_t is the C wide conversion state in both hosted and target
 * modes.  Keeping one type prevents <cwchar> and <locale> include order from
 * creating distinct, incompatible state objects. */
using ::mbstate_t;

namespace __detail {

/* The C standard intentionally leaves mbstate_t opaque.  The UTF-8 codecvt
 * state needs one 32-bit private token, so preserve the existing target
 * encoding in the first four bytes and clear all remaining host-owned state.
 * No layout field of a compiler C runtime type is inspected. */
inline unsigned mbstate_load(const mbstate_t& state) noexcept {
    unsigned value = 0u;
    __builtin_memcpy(&value, &state, sizeof(value));
    return value;
}

inline void mbstate_store(mbstate_t& state, unsigned value) noexcept {
    static_assert(sizeof(mbstate_t) >= sizeof(value),
                  "mbstate_t must hold a 32-bit conversion state");
    __builtin_memset(&state, 0, sizeof(state));
    __builtin_memcpy(&state, &value, sizeof(value));
}

inline bool codecvt_byname_supported(const char* name) noexcept {
    if (name == nullptr) return false;
    auto equal = [](const char* left, const char* right) noexcept {
        while (*left != '\0' && *left == *right) {
            ++left;
            ++right;
        }
        return *left == *right;
    };
    if (equal(name, "C") || equal(name, "POSIX") ||
        equal(name, "C.UTF-8") || equal(name, "POSIX.UTF-8") ||
        equal(name, "C.UTF8") || equal(name, "POSIX.UTF8") ||
        equal(name, "C.utf8") || equal(name, "POSIX.utf8"))
        return true;
    static constexpr const char* regions[] = {
        "en_US", "de_DE", "fr_FR", "es_ES", "it_IT", "pt_BR", "ja_JP"};
    for (const char* region : regions) {
        char candidate[32] = {};
        size_t length = 0u;
        while (region[length] != '\0' && length + 1u < sizeof(candidate)) {
            candidate[length] = region[length];
            ++length;
        }
        if (region[length] != '\0') return false;
        const char* suffixes[] = {".UTF-8", ".UTF8", ".utf8"};
        for (const char* suffix : suffixes) {
            size_t suffix_length = 0u;
            while (suffix[suffix_length] != '\0') ++suffix_length;
            if (length + suffix_length >= sizeof(candidate)) continue;
            for (size_t index = 0u; index <= suffix_length; ++index)
                candidate[length + index] = suffix[index];
            if (equal(name, candidate)) return true;
        }
    }
    return false;
}

[[noreturn]] inline void codecvt_byname_invalid() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw runtime_error("unsupported codecvt locale name");
#else
    __builtin_trap();
#endif
}

} // namespace __detail

/* ═══════════════════════════════════════════════════════════════
 * codecvt - 文字コード変換
 * ═══════════════════════════════════════════════════════════════*/

template<typename InternT, typename ExternT, typename StateT>
class codecvt : public locale::facet, public codecvt_base {
public:
    using intern_type = InternT;
    using extern_type = ExternT;
    using state_type = StateT;

    static locale::id id;

    explicit codecvt(size_t refs = 0) : locale::facet(refs) {}

    result out(StateT& state,
               const InternT* from, const InternT* from_end, const InternT*& from_next,
               ExternT* to, ExternT* to_end, ExternT*& to_next) const {
        return do_out(state, from, from_end, from_next, to, to_end, to_next);
    }

    result in(StateT& state,
              const ExternT* from, const ExternT* from_end, const ExternT*& from_next,
              InternT* to, InternT* to_end, InternT*& to_next) const {
        return do_in(state, from, from_end, from_next, to, to_end, to_next);
    }

    result unshift(StateT& state, ExternT* to, ExternT* to_end, ExternT*& to_next) const {
        return do_unshift(state, to, to_end, to_next);
    }

    int encoding() const noexcept { return do_encoding(); }
    bool always_noconv() const noexcept { return do_always_noconv(); }
    int length(StateT& state, const ExternT* from, const ExternT* from_end, size_t max) const {
        return do_length(state, from, from_end, max);
    }
    int max_length() const noexcept { return do_max_length(); }

protected:
    virtual ~codecvt() = default;

    virtual result do_out(StateT&,
                          const InternT* from, const InternT*, const InternT*& from_next,
                          ExternT* to, ExternT*, ExternT*& to_next) const {
        from_next = from;
        to_next = to;
        return noconv;
    }

    virtual result do_in(StateT&,
                         const ExternT* from, const ExternT*, const ExternT*& from_next,
                         InternT* to, InternT*, InternT*& to_next) const {
        from_next = from;
        to_next = to;
        return noconv;
    }

    virtual result do_unshift(StateT&, ExternT* to, ExternT*, ExternT*& to_next) const {
        to_next = to;
        return noconv;
    }

    virtual int do_encoding() const noexcept { return 1; }
    virtual bool do_always_noconv() const noexcept { return true; }
    virtual int do_length(StateT&, const ExternT* from, const ExternT* from_end, size_t max) const {
        size_t n = from_end - from;
        return static_cast<int>(n < max ? n : max);
    }
    virtual int do_max_length() const noexcept { return 1; }
};

template<typename InternT, typename ExternT, typename StateT>
locale::id codecvt<InternT, ExternT, StateT>::id;

template<typename InternT>
class codecvt<InternT, char, mbstate_t>
    : public locale::facet, public codecvt_base {
public:
    using intern_type = InternT;
    using extern_type = char;
    using state_type = mbstate_t;

    static locale::id id;

    explicit codecvt(size_t refs = 0) : locale::facet(refs) {}

    result out(mbstate_t& state, const InternT* from, const InternT* from_end,
               const InternT*& from_next, char* to, char* to_end,
               char*& to_next) const {
        return do_out(state, from, from_end, from_next, to, to_end, to_next);
    }

    result in(mbstate_t& state, const char* from, const char* from_end,
              const char*& from_next, InternT* to, InternT* to_end,
              InternT*& to_next) const {
        return do_in(state, from, from_end, from_next, to, to_end, to_next);
    }

    result unshift(mbstate_t& state, char* to, char* to_end,
                   char*& to_next) const {
        return do_unshift(state, to, to_end, to_next);
    }

    int encoding() const noexcept { return do_encoding(); }
    bool always_noconv() const noexcept { return do_always_noconv(); }
    int length(mbstate_t& state, const char* from, const char* from_end,
               size_t maximum) const {
        return do_length(state, from, from_end, maximum);
    }
    int max_length() const noexcept { return do_max_length(); }

protected:
    virtual ~codecvt() = default;

    virtual result do_out(mbstate_t& state, const InternT* from,
                          const InternT* from_end, const InternT*& from_next,
                          char* to, char* to_end, char*& to_next) const {
        from_next = from;
        to_next = to;
        if (__detail::mbstate_load(state) != 0u) return error;
        while (from_next != from_end) {
            const InternT* character_start = from_next;
            unsigned code_point = static_cast<unsigned>(*from_next);
            size_t input_units = 1u;
            if (sizeof(InternT) == 2u) {
                if (code_point >= 0xD800u && code_point <= 0xDBFFu) {
                    if (from_end - from_next < 2) return partial;
                    unsigned low = static_cast<unsigned>(from_next[1]);
                    if (low < 0xDC00u || low > 0xDFFFu) return error;
                    code_point = 0x10000u + ((code_point - 0xD800u) << 10u) +
                        (low - 0xDC00u);
                    input_units = 2u;
                } else if (code_point >= 0xDC00u && code_point <= 0xDFFFu) {
                    return error;
                }
            } else if ((code_point >= 0xD800u && code_point <= 0xDFFFu) ||
                       code_point > 0x10FFFFu) {
                return error;
            }

            unsigned byte_count = code_point < 0x80u ? 1u
                : code_point < 0x800u ? 2u
                : code_point < 0x10000u ? 3u : 4u;
            if (static_cast<size_t>(to_end - to_next) < byte_count) {
                from_next = character_start;
                return partial;
            }
            if (byte_count == 1u) {
                *to_next++ = static_cast<char>(code_point);
            } else if (byte_count == 2u) {
                *to_next++ = static_cast<char>(0xC0u | (code_point >> 6u));
                *to_next++ = static_cast<char>(0x80u | (code_point & 0x3Fu));
            } else if (byte_count == 3u) {
                *to_next++ = static_cast<char>(0xE0u | (code_point >> 12u));
                *to_next++ = static_cast<char>(
                    0x80u | ((code_point >> 6u) & 0x3Fu));
                *to_next++ = static_cast<char>(0x80u | (code_point & 0x3Fu));
            } else {
                *to_next++ = static_cast<char>(0xF0u | (code_point >> 18u));
                *to_next++ = static_cast<char>(
                    0x80u | ((code_point >> 12u) & 0x3Fu));
                *to_next++ = static_cast<char>(
                    0x80u | ((code_point >> 6u) & 0x3Fu));
                *to_next++ = static_cast<char>(0x80u | (code_point & 0x3Fu));
            }
            from_next += input_units;
        }
        return ok;
    }

    virtual result do_in(mbstate_t& state, const char* from,
                         const char* from_end, const char*& from_next,
                         InternT* to, InternT* to_end,
                         InternT*& to_next) const {
        from_next = from;
        to_next = to;
        unsigned code_point = 0u;
        unsigned remaining = 0u;
        unsigned total = 0u;
        if (!unpack_state(state, code_point, remaining, total)) {
            __detail::mbstate_store(state, 0u);
            return error;
        }
        while (true) {
            if (remaining == 0u) {
                if (from_next == from_end) return ok;
                unsigned lead = static_cast<unsigned char>(*from_next);
                if (lead < 0x80u) {
                    if (to_next == to_end) return partial;
                    *to_next++ = static_cast<InternT>(lead);
                    ++from_next;
                    continue;
                }
                if (lead >= 0xC2u && lead <= 0xDFu) {
                    code_point = lead & 0x1Fu;
                    remaining = total = 1u;
                } else if (lead >= 0xE0u && lead <= 0xEFu) {
                    code_point = lead & 0x0Fu;
                    remaining = total = 2u;
                } else if (lead >= 0xF0u && lead <= 0xF4u) {
                    code_point = lead & 0x07u;
                    remaining = total = 3u;
                } else {
                    return error;
                }
                size_t output_units = sizeof(InternT) == 2u && total == 3u
                    ? 2u : 1u;
                if (static_cast<size_t>(to_end - to_next) < output_units) {
                    remaining = total = 0u;
                    code_point = 0u;
                    return partial;
                }
                ++from_next;
            } else {
                size_t output_units = sizeof(InternT) == 2u && total == 3u
                    ? 2u : 1u;
                if (static_cast<size_t>(to_end - to_next) < output_units)
                    return partial;
            }

            while (remaining != 0u) {
                if (from_next == from_end) {
                    __detail::mbstate_store(
                        state, pack_state(code_point, remaining, total));
                    return partial;
                }
                unsigned continuation =
                    static_cast<unsigned char>(*from_next);
                if ((continuation & 0xC0u) != 0x80u) {
                    __detail::mbstate_store(state, 0u);
                    return error;
                }
                code_point = (code_point << 6u) | (continuation & 0x3Fu);
                --remaining;
                ++from_next;
            }

            unsigned minimum = total == 1u ? 0x80u
                : total == 2u ? 0x800u : 0x10000u;
            if (code_point < minimum || code_point > 0x10FFFFu ||
                (code_point >= 0xD800u && code_point <= 0xDFFFu)) {
                __detail::mbstate_store(state, 0u);
                return error;
            }
            if (sizeof(InternT) == 2u) {
                if (code_point >= 0x10000u) {
                    unsigned value = code_point - 0x10000u;
                    *to_next++ = static_cast<InternT>(
                        0xD800u + (value >> 10u));
                    *to_next++ = static_cast<InternT>(
                        0xDC00u + (value & 0x3FFu));
                } else {
                    *to_next++ = static_cast<InternT>(code_point);
                }
            } else {
                *to_next++ = static_cast<InternT>(code_point);
            }
            __detail::mbstate_store(state, 0u);
            code_point = remaining = total = 0u;
        }
    }

    virtual result do_unshift(mbstate_t& state, char* to, char*,
                              char*& to_next) const {
        to_next = to;
        return __detail::mbstate_load(state) == 0u ? noconv : error;
    }

    virtual int do_encoding() const noexcept { return 0; }
    virtual bool do_always_noconv() const noexcept { return false; }

    virtual int do_length(mbstate_t& state, const char* from,
                          const char* from_end, size_t maximum) const {
        unsigned code_point = 0u;
        unsigned remaining = 0u;
        unsigned total = 0u;
        if (!unpack_state(state, code_point, remaining, total)) return 0;
        const char* scan = from;
        const char* accepted = from;
        while (scan != from_end) {
            unsigned current_total = total;
            if (remaining == 0u) {
                unsigned lead = static_cast<unsigned char>(*scan);
                if (lead < 0x80u) {
                    if (maximum == 0u) break;
                    --maximum;
                    accepted = ++scan;
                    continue;
                }
                if (lead >= 0xC2u && lead <= 0xDFu) {
                    code_point = lead & 0x1Fu;
                    remaining = current_total = 1u;
                } else if (lead >= 0xE0u && lead <= 0xEFu) {
                    code_point = lead & 0x0Fu;
                    remaining = current_total = 2u;
                } else if (lead >= 0xF0u && lead <= 0xF4u) {
                    code_point = lead & 0x07u;
                    remaining = current_total = 3u;
                } else {
                    break;
                }
                size_t units = sizeof(InternT) == 2u && current_total == 3u
                    ? 2u : 1u;
                if (maximum < units) break;
                ++scan;
            } else {
                size_t units = sizeof(InternT) == 2u && current_total == 3u
                    ? 2u : 1u;
                if (maximum < units) break;
            }
            while (remaining != 0u && scan != from_end) {
                unsigned continuation = static_cast<unsigned char>(*scan);
                if ((continuation & 0xC0u) != 0x80u) break;
                code_point = (code_point << 6u) | (continuation & 0x3Fu);
                --remaining;
                ++scan;
            }
            if (remaining != 0u) break;
            unsigned minimum = current_total == 1u ? 0x80u
                : current_total == 2u ? 0x800u : 0x10000u;
            if (code_point < minimum || code_point > 0x10FFFFu ||
                (code_point >= 0xD800u && code_point <= 0xDFFFu)) break;
            size_t units = sizeof(InternT) == 2u && code_point >= 0x10000u
                ? 2u : 1u;
            maximum -= units;
            accepted = scan;
            code_point = 0u;
            total = 0u;
        }
        size_t consumed = static_cast<size_t>(accepted - from);
        size_t int_max = static_cast<size_t>(numeric_limits<int>::max());
        return static_cast<int>(consumed > int_max ? int_max : consumed);
    }

    virtual int do_max_length() const noexcept { return 4; }

private:
    static constexpr unsigned state_magic = 0xA4000000u;
    static constexpr unsigned state_magic_mask = 0xFE000000u;
    static constexpr unsigned code_point_mask = 0x001FFFFFu;

    static unsigned pack_state(unsigned code_point, unsigned remaining,
                               unsigned total) noexcept {
        unsigned packed = state_magic | (code_point & code_point_mask) |
            ((remaining & 3u) << 21u) | ((total & 3u) << 23u);
        return packed;
    }

    static bool unpack_state(const mbstate_t& state, unsigned& code_point,
                             unsigned& remaining, unsigned& total) noexcept {
        unsigned packed = __detail::mbstate_load(state);
        if (packed == 0u) {
            code_point = remaining = total = 0u;
            return true;
        }
        if ((packed & state_magic_mask) != state_magic) return false;
        code_point = packed & code_point_mask;
        remaining = (packed >> 21u) & 3u;
        total = (packed >> 23u) & 3u;
        if (remaining == 0u || total == 0u || remaining > total) return false;
        unsigned consumed = total - remaining;
        unsigned payload_bits = total == 1u ? 5u : total == 2u ? 4u : 3u;
        unsigned used_bits = payload_bits + consumed * 6u;
        if (code_point >= (1u << used_bits)) return false;
        if (total == 1u && consumed == 0u && code_point < 2u) return false;
        if (total == 3u && consumed == 0u && code_point > 4u) return false;
        return true;
    }
};

template<typename InternT>
locale::id codecvt<InternT, char, mbstate_t>::id;

/* codecvt<char, char, mbstate_t>特殊化 */
template<>
class codecvt<char, char, mbstate_t> : public locale::facet, public codecvt_base {
public:
    using intern_type = char;
    using extern_type = char;
    using state_type = mbstate_t;

    static locale::id id;

    explicit codecvt(size_t refs = 0) : locale::facet(refs) {}

    result out(mbstate_t&, const char* from, const char*, const char*& from_next,
               char* to, char*, char*& to_next) const {
        from_next = from;
        to_next = to;
        return noconv;
    }

    result in(mbstate_t&, const char* from, const char*, const char*& from_next,
              char* to, char*, char*& to_next) const {
        from_next = from;
        to_next = to;
        return noconv;
    }

    result unshift(mbstate_t&, char* to, char*, char*& to_next) const {
        to_next = to;
        return noconv;
    }

    int encoding() const noexcept { return 1; }
    bool always_noconv() const noexcept { return true; }
    int length(mbstate_t&, const char* from, const char* from_end, size_t max) const {
        size_t n = from_end - from;
        return static_cast<int>(n < max ? n : max);
    }
    int max_length() const noexcept { return 1; }

protected:
    ~codecvt() override = default;
};

RIN_LOCALE_SPECIALIZATION_DEFINITION locale::id codecvt<char, char, mbstate_t>::id;

/* codecvt_byname is a named-facet adapter.  RinOS has no external locale
 * catalog yet, but the four classic names are stable and all map to the
 * built-in UTF-8/identity facets above.  Rejecting every other name before
 * publishing a facet keeps an arbitrary catalog name from silently selecting
 * the wrong encoding. */
template<typename InternT, typename ExternT, typename StateT>
class codecvt_byname : public codecvt<InternT, ExternT, StateT> {
public:
    using base_type = codecvt<InternT, ExternT, StateT>;
    using intern_type = InternT;
    using extern_type = ExternT;
    using state_type = StateT;

    static locale::id id;

    explicit codecvt_byname(const char* name, size_t refs = 0)
        : base_type(refs) {
        if (!__detail::codecvt_byname_supported(name))
            __detail::codecvt_byname_invalid();
    }

protected:
    ~codecvt_byname() override = default;
};

template<typename InternT, typename ExternT, typename StateT>
locale::id codecvt_byname<InternT, ExternT, StateT>::id;

/* ═══════════════════════════════════════════════════════════════
 * time_base
 * ═══════════════════════════════════════════════════════════════*/

class time_base {
public:
    enum dateorder { no_order, dmy, mdy, ymd, ydm };
};

/* ═══════════════════════════════════════════════════════════════
 * messages_base
 * ═══════════════════════════════════════════════════════════════*/

class messages_base {
public:
    using catalog = int;
};

/* ═══════════════════════════════════════════════════════════════
 * messages
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT>
class messages : public locale::facet, public messages_base {
public:
    using char_type = CharT;
    using string_type = basic_string<CharT>;

    static locale::id id;

    explicit messages(size_t refs = 0) : locale::facet(refs) {}

    catalog open(const basic_string<char>& name, const locale& loc) const {
        return do_open(name, loc);
    }

    string_type get(catalog cat, int set, int msgid, const string_type& dfault) const {
        return do_get(cat, set, msgid, dfault);
    }

    void close(catalog cat) const { do_close(cat); }

protected:
    virtual ~messages() = default;

    virtual catalog do_open(const basic_string<char>&, const locale&) const { return -1; }
    virtual string_type do_get(catalog, int, int, const string_type& dfault) const {
        return dfault;
    }
    virtual void do_close(catalog) const {}
};

template<typename CharT>
locale::id messages<CharT>::id;

/* ═══════════════════════════════════════════════════════════════
 * use_facet / has_facet
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT, typename OutputIterator> class time_put;
template<typename CharT, typename InputIterator> class time_get;
template<typename CharT, typename InputIterator> class num_get;
template<typename CharT, typename OutputIterator> class num_put;
template<typename CharT, typename OutputIterator> class money_put;
template<typename CharT, typename InputIterator> class money_get;

template<typename CharT>
struct __classic_locale_character : integral_constant<bool,
    is_same<CharT, char>::value || is_same<CharT, wchar_t>::value> {};

template<typename Facet>
struct __classic_facet_available : false_type {};

template<typename CharT>
struct __classic_facet_available<ctype<CharT>>
    : __classic_locale_character<CharT> {};

template<typename CharT>
struct __classic_facet_available<numpunct<CharT>>
    : __classic_locale_character<CharT> {};

template<typename CharT, typename InputIterator>
struct __classic_facet_available<num_get<CharT, InputIterator>>
    : integral_constant<bool, __classic_locale_character<CharT>::value &&
        is_same<InputIterator, istreambuf_iterator<CharT>>::value> {};

template<typename CharT, typename OutputIterator>
struct __classic_facet_available<num_put<CharT, OutputIterator>>
    : integral_constant<bool, __classic_locale_character<CharT>::value &&
        is_same<OutputIterator, ostreambuf_iterator<CharT>>::value> {};

template<typename CharT>
struct __classic_facet_available<collate<CharT>>
    : __classic_locale_character<CharT> {};

template<typename CharT, bool International>
struct __classic_facet_available<moneypunct<CharT, International>>
    : __classic_locale_character<CharT> {};

template<typename InternT, typename ExternT, typename StateT>
struct __classic_facet_available<codecvt<InternT, ExternT, StateT>>
    : integral_constant<bool,
        (is_same<InternT, char>::value || is_same<InternT, wchar_t>::value ||
         is_same<InternT, char16_t>::value ||
         is_same<InternT, char32_t>::value) &&
        is_same<ExternT, char>::value &&
        is_same<StateT, mbstate_t>::value> {};

template<typename CharT>
struct __classic_facet_available<messages<CharT>>
    : __classic_locale_character<CharT> {};

template<typename CharT, typename OutputIterator>
struct __classic_facet_available<time_put<CharT, OutputIterator>>
    : integral_constant<bool, __classic_locale_character<CharT>::value &&
        is_same<OutputIterator, ostreambuf_iterator<CharT>>::value> {};

template<typename CharT, typename InputIterator>
struct __classic_facet_available<time_get<CharT, InputIterator>>
    : integral_constant<bool, __classic_locale_character<CharT>::value &&
        is_same<InputIterator, istreambuf_iterator<CharT>>::value> {};

template<typename CharT, typename OutputIterator>
struct __classic_facet_available<money_put<CharT, OutputIterator>>
    : integral_constant<bool, __classic_locale_character<CharT>::value &&
        is_same<OutputIterator, ostreambuf_iterator<CharT>>::value> {};

template<typename CharT, typename InputIterator>
struct __classic_facet_available<money_get<CharT, InputIterator>>
    : integral_constant<bool, __classic_locale_character<CharT>::value &&
        is_same<InputIterator, istreambuf_iterator<CharT>>::value> {};

template<typename Facet>
class __classic_facet_holder final : public Facet {
public:
    __classic_facet_holder() : Facet(1) {}
    ~__classic_facet_holder() override = default;
};

template<>
class __classic_facet_holder<ctype<char>> final : public ctype<char> {
public:
    __classic_facet_holder() : ctype<char>(nullptr, false, 1u) {}
    ~__classic_facet_holder() override = default;
};

template<typename Facet>
const Facet& __classic_facet_or_fail(true_type) {
    static __classic_facet_holder<Facet> facet;
    return facet;
}

template<typename Facet>
const Facet& __classic_facet_or_fail(false_type) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_cast();
#else
    rin_panic("std::use_facet: facet is not present");
#endif
}

template<typename Facet>
const Facet& use_facet(const locale& value) {
    if (const Facet* installed = value.template _find_facet<Facet>()) {
        return *installed;
    }
    if (const Facet* named = __named_facet_provider<Facet>::get(value)) {
        return *named;
    }
    return __classic_facet_or_fail<Facet>(
        integral_constant<bool, __classic_facet_available<Facet>::value>());
}

template<typename Facet>
bool has_facet(const locale& value) noexcept {
    return value.template _find_facet<Facet>() != nullptr ||
           __named_facet_provider<Facet>::get(value) != nullptr ||
           __classic_facet_available<Facet>::value;
}

/* ═══════════════════════════════════════════════════════════════
 * C-locale money/time facets
 * ═══════════════════════════════════════════════════════════════*/

namespace __locale_detail {

template<typename CharT>
inline bool ascii_equal(CharT value, char expected) noexcept {
    return value == static_cast<CharT>(expected);
}

template<typename CharT>
inline char ascii_lower(CharT value) noexcept {
    char narrowed = static_cast<char>(value);
    return narrowed >= 'A' && narrowed <= 'Z'
        ? static_cast<char>(narrowed + ('a' - 'A')) : narrowed;
}

template<typename CharT>
inline bool append(CharT* output, unsigned capacity, unsigned& length,
                   CharT value) noexcept {
    if (length == static_cast<unsigned>(-1)) return false;
    if (output != nullptr) {
        if (length >= capacity) return false;
        output[length] = value;
    }
    ++length;
    return true;
}

template<typename CharT>
inline bool append_ascii(CharT* output, unsigned capacity, unsigned& length,
                         const char* text) noexcept {
    while (*text != '\0') {
        if (!append(output, capacity, length,
                    static_cast<CharT>(*text++))) return false;
    }
    return true;
}

template<typename CharT>
inline bool append_number(CharT* output, unsigned capacity, unsigned& length,
                          unsigned value, unsigned width,
                          char padding = '0') noexcept {
    char digits[16];
    unsigned count = 0;
    do {
        digits[count++] = static_cast<char>('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    while (count < width) digits[count++] = padding;
    while (count != 0u) {
        if (!append(output, capacity, length,
                    static_cast<CharT>(digits[--count]))) return false;
    }
    return true;
}

inline bool timezone_offset_valid(long offset) noexcept {
    return offset >= -86340L && offset <= 86340L && offset % 60L == 0L;
}

inline bool timezone_offset_from_tm(const struct tm& value,
                                    long& offset) noexcept {
#if RINCXX_TM_HAS_GNU_EXTENSIONS
    offset = value.tm_gmtoff;
    return true;
#else
    unsigned flags = 0u;
    if (!rin_cxx_tm_timezone_get ||
        !rin_cxx_tm_timezone_get(&value, &offset, nullptr, 0u, &flags))
        return false;
    return (flags & RINCXX_TM_TIMEZONE_HAS_OFFSET) != 0u;
#endif
}

inline bool timezone_name_from_tm(const struct tm& value,
                                  const char*& zone, char* scratch,
                                  size_t scratch_capacity) noexcept {
#if RINCXX_TM_HAS_GNU_EXTENSIONS
    zone = value.tm_zone;
    return true;
#else
    unsigned flags = 0u;
    if (!scratch || scratch_capacity == 0u || !rin_cxx_tm_timezone_get ||
        !rin_cxx_tm_timezone_get(&value, nullptr, scratch, scratch_capacity,
                                 &flags) ||
        (flags & RINCXX_TM_TIMEZONE_HAS_NAME) == 0u)
        return false;
    zone = scratch;
    return true;
#endif
}

template<typename CharT>
inline bool append_timezone_offset(CharT* output, unsigned capacity,
                                   unsigned& length, long offset) noexcept {
    if (!timezone_offset_valid(offset)) return false;
    unsigned minutes = static_cast<unsigned>(offset < 0 ? -offset : offset) /
                       60u;
    return append(output, capacity, length,
                  static_cast<CharT>(offset < 0 ? '-' : '+')) &&
           append_number(output, capacity, length, minutes / 60u, 2u) &&
           append_number(output, capacity, length, minutes % 60u, 2u);
}

template<typename CharT>
inline bool append_timezone_name(CharT* output, unsigned capacity,
                                 unsigned& length, const char* zone) noexcept {
    if (zone == nullptr || zone[0] == '\0') return false;
    for (unsigned index = 0u; index < 64u; ++index) {
        unsigned char current = static_cast<unsigned char>(zone[index]);
        if (current == 0u) return true;
        if (current < 0x21u || current > 0x7eu ||
            !append(output, capacity, length,
                    static_cast<CharT>(current))) return false;
    }
    return false;
}

inline bool time_modifier_valid(char modifier, char directive) noexcept {
    if (modifier == 'E') {
        return directive == 'c' || directive == 'C' || directive == 'x' ||
               directive == 'X' || directive == 'y' || directive == 'Y';
    }
    if (modifier == 'O') {
        return directive == 'd' || directive == 'e' || directive == 'H' ||
               directive == 'I' || directive == 'm' || directive == 'M' ||
               directive == 'S' || directive == 'u' || directive == 'U' ||
               directive == 'V' || directive == 'w' || directive == 'W' ||
               directive == 'y';
    }
    return false;
}

inline bool leap_year(long long year) noexcept {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

inline int month_days(long long year, int month) noexcept {
    static constexpr unsigned char days[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if (month < 0 || month >= 12) return 0;
    return month == 1 && leap_year(year)
        ? 29 : static_cast<int>(days[month]);
}

inline int year_day(long long year, int month, int day) noexcept {
    int result = 0;
    for (int current = 0; current < month; ++current) {
        result += month_days(year, current);
    }
    return result + day - 1;
}

inline int week_day(long long year, int month, int day) noexcept {
    /* Proleptic Gregorian calendar; 1970-01-01 was Thursday. */
    long long adjusted_year = year;
    unsigned adjusted_month = static_cast<unsigned>(month + 1);
    adjusted_year -= adjusted_month <= 2u;
    const long long era = (adjusted_year >= 0 ? adjusted_year
                                                : adjusted_year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(adjusted_year - era * 400);
    const unsigned shifted_month = adjusted_month > 2u
        ? adjusted_month - 3u : adjusted_month + 9u;
    const unsigned day_of_year = (153u * shifted_month + 2u) / 5u +
        static_cast<unsigned>(day - 1);
    const unsigned day_of_era = year_of_era * 365u + year_of_era / 4u -
        year_of_era / 100u + day_of_year;
    long long days = era * 146097 + static_cast<long long>(day_of_era) - 719468;
    int result = static_cast<int>((days + 4) % 7);
    return result < 0 ? result + 7 : result;
}

inline bool calendar_week_fields_valid(long long year, int yday,
                                       int wday) noexcept {
    if (year < 0 || year > 9999 || wday < 0 || wday > 6 || yday < 0 ||
        yday >= (leap_year(year) ? 366 : 365)) return false;
    return (week_day(year, 0, 1) + yday) % 7 == wday;
}

inline int sunday_week_number(int yday, int wday) noexcept {
    return (yday + 7 - wday) / 7;
}

inline int monday_week_number(int yday, int wday) noexcept {
    int monday_based = wday == 0 ? 6 : wday - 1;
    return (yday + 7 - monday_based) / 7;
}

inline int iso_weeks_in_year(long long year) noexcept {
    int january_first = week_day(year, 0, 1);
    int iso_weekday = january_first == 0 ? 7 : january_first;
    return iso_weekday == 4 || (iso_weekday == 3 && leap_year(year))
        ? 53 : 52;
}

inline bool iso_week_fields(long long year, int yday, int wday,
                            long long& iso_year, int& iso_week) noexcept {
    if (!calendar_week_fields_valid(year, yday, wday)) return false;
    int iso_weekday = wday == 0 ? 7 : wday;
    int week = (yday + 11 - iso_weekday) / 7;
    iso_year = year;
    if (week < 1) {
        --iso_year;
        week = iso_weeks_in_year(iso_year);
    } else if (week > iso_weeks_in_year(year)) {
        ++iso_year;
        week = 1;
    }
    iso_week = week;
    return true;
}

inline bool month_day_from_year_day(long long year, int yday,
                                    int& month, int& day) noexcept {
    if (year < 0 || year > 9999 || yday < 0 ||
        yday >= (leap_year(year) ? 366 : 365)) return false;
    month = 0;
    while (month < 12) {
        int days = month_days(year, month);
        if (yday < days) {
            day = yday + 1;
            return true;
        }
        yday -= days;
        ++month;
    }
    return false;
}

inline bool year_day_from_week(long long year, int week, int wday,
                               bool monday_first, int& yday) noexcept {
    if (year < 0 || year > 9999 || week < 0 || week > 53 ||
        wday < 0 || wday > 6) return false;
    int days = leap_year(year) ? 366 : 365;
    int january_first = week_day(year, 0, 1);
    for (int candidate = 0; candidate < days; ++candidate) {
        int candidate_wday = (january_first + candidate) % 7;
        int candidate_week = monday_first
            ? monday_week_number(candidate, candidate_wday)
            : sunday_week_number(candidate, candidate_wday);
        if (candidate_week == week && candidate_wday == wday) {
            yday = candidate;
            return true;
        }
    }
    return false;
}

inline bool date_from_iso_week(long long requested_iso_year,
                               int requested_week, int requested_wday,
                               long long& year, int& month, int& day,
                               int& yday) noexcept {
    if (requested_iso_year < 0 || requested_iso_year > 9999 ||
        requested_week < 1 || requested_week > 53 ||
        requested_wday < 1 || requested_wday > 7) return false;
    for (long long candidate_year = requested_iso_year - 1;
         candidate_year <= requested_iso_year + 1; ++candidate_year) {
        if (candidate_year < 0 || candidate_year > 9999) continue;
        int days = leap_year(candidate_year) ? 366 : 365;
        int january_first = week_day(candidate_year, 0, 1);
        for (int candidate_yday = 0; candidate_yday < days;
             ++candidate_yday) {
            int candidate_wday = (january_first + candidate_yday) % 7;
            long long candidate_iso_year = 0;
            int candidate_iso_week = 0;
            if (!iso_week_fields(candidate_year, candidate_yday,
                                 candidate_wday, candidate_iso_year,
                                 candidate_iso_week)) return false;
            int candidate_iso_wday = candidate_wday == 0
                ? 7 : candidate_wday;
            if (candidate_iso_year == requested_iso_year &&
                candidate_iso_week == requested_week &&
                candidate_iso_wday == requested_wday) {
                year = candidate_year;
                yday = candidate_yday;
                return month_day_from_year_day(year, yday, month, day);
            }
        }
    }
    return false;
}

#if __cplusplus >= 201703L
inline constexpr const char* short_weekdays[7] = {
#else
static const char* const short_weekdays[7] = {
#endif
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
#if __cplusplus >= 201703L
inline constexpr const char* long_weekdays[7] = {
#else
static const char* const long_weekdays[7] = {
#endif
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
    "Saturday"
};
#if __cplusplus >= 201703L
inline constexpr const char* short_months[12] = {
#else
static const char* const short_months[12] = {
#endif
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
    "Oct", "Nov", "Dec"
};
#if __cplusplus >= 201703L
inline constexpr const char* long_months[12] = {
#else
static const char* const long_months[12] = {
#endif
    "January", "February", "March", "April", "May", "June", "July",
    "August", "September", "October", "November", "December"
};

template<typename CharT>
inline bool format_time(CharT* output, unsigned capacity, unsigned& length,
                        const struct ::tm& value, const CharT* first,
                        const CharT* last, unsigned depth = 0) noexcept {
    if (depth > 4u) return false;
    while (first != last) {
        if (!ascii_equal(*first, '%')) {
            if (!append(output, capacity, length, *first++)) return false;
            continue;
        }
        if (++first == last) return false;
        char directive = static_cast<char>(*first++);
        if (directive == 'E' || directive == 'O') {
            char modifier = directive;
            if (first == last) return false;
            directive = static_cast<char>(*first++);
            if (!time_modifier_valid(modifier, directive)) return false;
        }
        long long year = static_cast<long long>(value.tm_year) + 1900LL;
        const CharT* expansion = nullptr;
        static constexpr CharT c_format[] = {
            '%','a',' ','%','b',' ','%','e',' ','%','T',' ','%','Y',0
        };
        static constexpr CharT x_format[] = {
            '%','m','/','%','d','/','%','y',0
        };
        static constexpr CharT time_format[] = {'%','H',':','%','M',':','%','S',0};
        static constexpr CharT date_format[] = {'%','Y','-','%','m','-','%','d',0};
        static constexpr CharT short_date_format[] = {
            '%','m','/','%','d','/','%','y',0
        };
        static constexpr CharT short_time_format[] = {'%','H',':','%','M',0};
        static constexpr CharT am_time_format[] = {
            '%','I',':','%','M',':','%','S',' ','%','p',0
        };
        switch (directive) {
            case '%':
                if (!append(output, capacity, length, static_cast<CharT>('%'))) return false;
                break;
            case 'n':
                if (!append(output, capacity, length, static_cast<CharT>('\n'))) return false;
                break;
            case 't':
                if (!append(output, capacity, length, static_cast<CharT>('\t'))) return false;
                break;
            case 'Y':
                if (year < 0 || year > 9999 ||
                    !append_number(output, capacity, length,
                                   static_cast<unsigned>(year), 4u)) return false;
                break;
            case 'C':
                if (year < 0 || year > 9999 ||
                    !append_number(output, capacity, length,
                                   static_cast<unsigned>(year / 100), 2u)) {
                    return false;
                }
                break;
            case 'y':
                if (year < 0 ||
                    !append_number(output, capacity, length,
                                   static_cast<unsigned>(year % 100), 2u)) return false;
                break;
            case 'm':
                if (value.tm_mon < 0 || value.tm_mon > 11 ||
                    !append_number(output, capacity, length,
                                   static_cast<unsigned>(value.tm_mon + 1), 2u)) return false;
                break;
            case 'd':
            case 'e': {
                if (value.tm_mon < 0 || value.tm_mon > 11 || value.tm_mday < 1 ||
                    value.tm_mday > month_days(year, value.tm_mon)) return false;
                char pad = directive == 'e' ? ' ' : '0';
                if (!append_number(output, capacity, length,
                                   static_cast<unsigned>(value.tm_mday), 2u, pad)) return false;
                break;
            }
            case 'H':
                if (value.tm_hour < 0 || value.tm_hour > 23 ||
                    !append_number(output, capacity, length,
                                   static_cast<unsigned>(value.tm_hour), 2u)) return false;
                break;
            case 'I': {
                if (value.tm_hour < 0 || value.tm_hour > 23) return false;
                unsigned hour = static_cast<unsigned>(value.tm_hour % 12);
                if (hour == 0u) hour = 12u;
                if (!append_number(output, capacity, length, hour, 2u)) return false;
                break;
            }
            case 'M':
                if (value.tm_min < 0 || value.tm_min > 59 ||
                    !append_number(output, capacity, length,
                                   static_cast<unsigned>(value.tm_min), 2u)) return false;
                break;
            case 'S':
                if (value.tm_sec < 0 || value.tm_sec > 60 ||
                    !append_number(output, capacity, length,
                                   static_cast<unsigned>(value.tm_sec), 2u)) return false;
                break;
            case 'p':
                if (value.tm_hour < 0 || value.tm_hour > 23 ||
                    !append_ascii(output, capacity, length,
                                  value.tm_hour < 12 ? "AM" : "PM")) return false;
                break;
            case 'z':
                {
                    long offset = 0;
                    if (!timezone_offset_from_tm(value, offset) ||
                        !append_timezone_offset(output, capacity, length,
                                                offset)) return false;
                }
                break;
            case 'Z':
                {
                    const char* zone = nullptr;
                    char zone_scratch[64] = {};
                    if (!timezone_name_from_tm(value, zone, zone_scratch,
                                               sizeof(zone_scratch)) ||
                        !append_timezone_name(output, capacity, length,
                                              zone)) return false;
                }
                break;
            case 'j':
                if (value.tm_yday < 0 || value.tm_yday >=
                    (leap_year(year) ? 366 : 365) ||
                    !append_number(output, capacity, length,
                                   static_cast<unsigned>(value.tm_yday + 1), 3u)) return false;
                break;
            case 'w':
                if (value.tm_wday < 0 || value.tm_wday > 6 ||
                    !append_number(output, capacity, length,
                                   static_cast<unsigned>(value.tm_wday), 1u)) return false;
                break;
            case 'u':
                if (value.tm_wday < 0 || value.tm_wday > 6 ||
                    !append_number(output, capacity, length,
                                   static_cast<unsigned>(value.tm_wday == 0 ? 7 : value.tm_wday), 1u)) return false;
                break;
            case 'U':
            case 'W': {
                if (!calendar_week_fields_valid(year, value.tm_yday,
                                                value.tm_wday)) return false;
                int week = directive == 'U'
                    ? sunday_week_number(value.tm_yday, value.tm_wday)
                    : monday_week_number(value.tm_yday, value.tm_wday);
                if (!append_number(output, capacity, length,
                                   static_cast<unsigned>(week), 2u)) {
                    return false;
                }
                break;
            }
            case 'G':
            case 'g':
            case 'V': {
                long long iso_year = 0;
                int iso_week = 0;
                if (!iso_week_fields(year, value.tm_yday, value.tm_wday,
                                     iso_year, iso_week)) return false;
                if (directive == 'V') {
                    if (!append_number(output, capacity, length,
                                       static_cast<unsigned>(iso_week), 2u)) {
                        return false;
                    }
                } else if (directive == 'G') {
                    if (iso_year < 0 || iso_year > 9999 ||
                        !append_number(output, capacity, length,
                            static_cast<unsigned>(iso_year), 4u)) return false;
                } else {
                    if (iso_year < 0 ||
                        !append_number(output, capacity, length,
                            static_cast<unsigned>(iso_year % 100), 2u)) {
                        return false;
                    }
                }
                break;
            }
            case 'a':
            case 'A':
                if (value.tm_wday < 0 || value.tm_wday > 6 ||
                    !append_ascii(output, capacity, length,
                        directive == 'a' ? short_weekdays[value.tm_wday]
                                         : long_weekdays[value.tm_wday])) return false;
                break;
            case 'b':
            case 'h':
            case 'B':
                if (value.tm_mon < 0 || value.tm_mon > 11 ||
                    !append_ascii(output, capacity, length,
                        directive == 'B' ? long_months[value.tm_mon]
                                         : short_months[value.tm_mon])) return false;
                break;
            case 'c': expansion = c_format; break;
            case 'x': expansion = x_format; break;
            case 'X':
            case 'T': expansion = time_format; break;
            case 'F': expansion = date_format; break;
            case 'D': expansion = short_date_format; break;
            case 'R': expansion = short_time_format; break;
            case 'r': expansion = am_time_format; break;
            default: return false;
        }
        if (expansion != nullptr) {
            const CharT* expansion_end = expansion;
            while (*expansion_end != CharT()) ++expansion_end;
            if (!format_time(output, capacity, length, value, expansion,
                             expansion_end, depth + 1u)) return false;
        }
    }
    return true;
}

template<typename Iterator, typename CharT>
inline bool parse_digits(Iterator& input, const Iterator& end,
                         unsigned width, int minimum, int maximum,
                         int& result) noexcept {
    int value = 0;
    for (unsigned index = 0; index < width; ++index) {
        if (input == end) return false;
        CharT current = *input;
        if (current < static_cast<CharT>('0') ||
            current > static_cast<CharT>('9')) return false;
        value = value * 10 + static_cast<int>(current - static_cast<CharT>('0'));
        ++input;
    }
    if (value < minimum || value > maximum) return false;
    result = value;
    return true;
}

template<typename Iterator, typename CharT>
inline bool parse_name(Iterator& input, const Iterator& end,
                       const char* const* names, unsigned count,
                       int& result) noexcept {
    char text[16];
    unsigned length = 0;
    while (input != end && length < sizeof(text) - 1u) {
        char current = ascii_lower(*input);
        if (current < 'a' || current > 'z') break;
        text[length++] = current;
        ++input;
    }
    text[length] = '\0';
    for (unsigned index = 0; index < count; ++index) {
        const char* candidate = names[index];
        unsigned candidate_length = 0;
        while (candidate[candidate_length] != '\0') ++candidate_length;
        if (length != 3u && length != candidate_length) continue;
        bool same = true;
        for (unsigned character = 0; character < length; ++character) {
            char expected = candidate[character];
            if (expected >= 'A' && expected <= 'Z') expected += 'a' - 'A';
            if (text[character] != expected) { same = false; break; }
        }
        if (same) { result = static_cast<int>(index); return true; }
    }
    return false;
}

struct time_fields {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    int yday = 0, wday = 0, hour12 = 0, pm = 0;
    int century = 0, year_of_century = 0;
    int iso_year = 0, iso_year_of_century = 0;
    int sunday_week = 0, monday_week = 0, iso_week = 0;
    long gmtoff = 0;
    const char* zone = nullptr;
    char zone_storage[64] = {};
    bool has_year = false, has_month = false, has_day = false;
    bool has_hour = false, has_minute = false, has_second = false;
    bool has_yday = false, has_wday = false, has_hour12 = false, has_pm = false;
    bool has_gmtoff = false, has_zone = false;
    bool has_century = false, has_year_of_century = false;
    bool has_iso_year = false, has_iso_year_of_century = false;
    bool has_sunday_week = false, has_monday_week = false;
    bool has_iso_week = false;
};

inline bool assign_field(int& target, bool& present, int value) noexcept {
    if (present) return target == value;
    target = value;
    present = true;
    return true;
}

inline bool assign_timezone_offset(time_fields& fields, long offset) noexcept {
    if (!timezone_offset_valid(offset)) return false;
    if (fields.has_gmtoff && fields.gmtoff != offset) return false;
    if (fields.has_zone && !fields.has_gmtoff) return false;
    fields.gmtoff = offset;
    fields.has_gmtoff = true;
    return true;
}

template<typename Iterator, typename CharT>
inline bool parse_timezone_offset(Iterator& input, const Iterator& end,
                                  long& offset) noexcept {
    if (input == end) return false;
    CharT sign = *input++;
    if (!ascii_equal(sign, '+') && !ascii_equal(sign, '-')) return false;
    int hours = 0;
    int minutes = 0;
    if (!parse_digits<Iterator, CharT>(input, end, 2u, 0, 23, hours) ||
        !parse_digits<Iterator, CharT>(input, end, 2u, 0, 59, minutes)) {
        return false;
    }
    long parsed = static_cast<long>(hours * 60 + minutes) * 60L;
    offset = ascii_equal(sign, '-') ? -parsed : parsed;
    return true;
}

template<typename Iterator, typename CharT>
inline bool parse_timezone_name(Iterator& input, const Iterator& end,
                                time_fields& fields) noexcept {
    unsigned length = 0u;
    while (input != end) {
        /* Do not narrow a wide input code unit before validating the ASCII
         * zone-token grammar.  For example U+0141 used to become 'A' after
         * the unsigned-char cast and could be published as a zone name. */
        if (static_cast<unsigned long long>(*input) > 0x7full) break;
        unsigned char current = static_cast<unsigned char>(*input);
        bool accepted = (current >= 'A' && current <= 'Z') ||
                        (current >= 'a' && current <= 'z') ||
                        (current >= '0' && current <= '9') ||
                        current == '_' || current == '/' || current == '+' ||
                        current == '-' || current == ':' || current == '.';
        if (!accepted) break;
        if (length + 1u >= sizeof(fields.zone_storage)) return false;
        fields.zone_storage[length++] = static_cast<char>(current);
        ++input;
    }
    if (length == 0u) return false;
    fields.zone_storage[length] = '\0';
    auto resolver = rin_time_zone_resolve_name;
    if (resolver) {
        long resolved_offset = 0L;
        if (!resolver(fields.zone_storage, length, &resolved_offset) ||
            (fields.has_gmtoff && fields.gmtoff != resolved_offset)) {
            return false;
        }
        fields.gmtoff = resolved_offset;
        fields.has_gmtoff = true;
    }
    fields.zone = fields.zone_storage;
    return true;
}

template<typename Iterator, typename CharT>
inline bool parse_time(Iterator& input, const Iterator& end,
                       const CharT* first, const CharT* last,
                       time_fields& fields, unsigned depth = 0) noexcept {
    if (depth > 4u) return false;
    while (first != last) {
        if (!ascii_equal(*first, '%')) {
            if (*first == static_cast<CharT>(' ') || *first == static_cast<CharT>('\t') ||
                *first == static_cast<CharT>('\n')) {
                if (input == end || !(*input == static_cast<CharT>(' ') ||
                    *input == static_cast<CharT>('\t') || *input == static_cast<CharT>('\n') ||
                    *input == static_cast<CharT>('\r'))) return false;
                do { ++input; } while (input != end &&
                    (*input == static_cast<CharT>(' ') || *input == static_cast<CharT>('\t') ||
                     *input == static_cast<CharT>('\n') || *input == static_cast<CharT>('\r')));
                ++first;
                continue;
            }
            if (input == end || *input != *first) return false;
            ++input;
            ++first;
            continue;
        }
        if (++first == last) return false;
        char directive = static_cast<char>(*first++);
        if (directive == 'E' || directive == 'O') {
            char modifier = directive;
            if (first == last) return false;
            directive = static_cast<char>(*first++);
            if (!time_modifier_valid(modifier, directive)) return false;
        }
        int parsed = 0;
        const CharT* expansion = nullptr;
        static constexpr CharT c_format[] = {
            '%','a',' ','%','b',' ','%','e',' ','%','T',' ','%','Y',0
        };
        static constexpr CharT x_format[] = {'%','m','/','%','d','/','%','y',0};
        static constexpr CharT time_format[] = {'%','H',':','%','M',':','%','S',0};
        static constexpr CharT date_format[] = {'%','Y','-','%','m','-','%','d',0};
        static constexpr CharT short_date_format[] = {
            '%','m','/','%','d','/','%','y',0
        };
        static constexpr CharT short_time_format[] = {'%','H',':','%','M',0};
        static constexpr CharT am_time_format[] = {
            '%','I',':','%','M',':','%','S',' ','%','p',0
        };
        switch (directive) {
            case '%':
                if (input == end || !ascii_equal(*input, '%')) return false;
                ++input;
                break;
            case 'n':
            case 't':
                if (input == end || !(*input == static_cast<CharT>(' ') ||
                    *input == static_cast<CharT>('\t') || *input == static_cast<CharT>('\n') ||
                    *input == static_cast<CharT>('\r'))) return false;
                do { ++input; } while (input != end &&
                    (*input == static_cast<CharT>(' ') || *input == static_cast<CharT>('\t') ||
                     *input == static_cast<CharT>('\n') || *input == static_cast<CharT>('\r')));
                break;
            case 'Y':
                if (!parse_digits<Iterator, CharT>(input, end, 4u, 0, 9999, parsed) ||
                    !assign_field(fields.year, fields.has_year, parsed)) return false;
                break;
            case 'C':
                if (!parse_digits<Iterator, CharT>(input, end, 2u, 0, 99,
                                                    parsed) ||
                    !assign_field(fields.century, fields.has_century,
                                  parsed)) return false;
                break;
            case 'y':
                if (!parse_digits<Iterator, CharT>(input, end, 2u, 0, 99, parsed)) return false;
                if (!assign_field(fields.year_of_century,
                                  fields.has_year_of_century,
                                  parsed)) return false;
                break;
            case 'm':
                if (!parse_digits<Iterator, CharT>(input, end, 2u, 1, 12, parsed) ||
                    !assign_field(fields.month, fields.has_month, parsed - 1)) return false;
                break;
            case 'd':
                if (!parse_digits<Iterator, CharT>(input, end, 2u, 1, 31, parsed) ||
                    !assign_field(fields.day, fields.has_day, parsed)) return false;
                break;
            case 'e': {
                if (input == end) return false;
                if (ascii_equal(*input, ' ')) {
                    ++input;
                    if (!parse_digits<Iterator, CharT>(input, end, 1u, 1, 9, parsed)) return false;
                } else if (!parse_digits<Iterator, CharT>(input, end, 2u, 1, 31, parsed)) {
                    return false;
                }
                if (!assign_field(fields.day, fields.has_day, parsed)) return false;
                break;
            }
            case 'H':
                if (!parse_digits<Iterator, CharT>(input, end, 2u, 0, 23, parsed) ||
                    !assign_field(fields.hour, fields.has_hour, parsed)) return false;
                break;
            case 'I':
                if (!parse_digits<Iterator, CharT>(input, end, 2u, 1, 12, parsed) ||
                    !assign_field(fields.hour12, fields.has_hour12, parsed)) return false;
                break;
            case 'M':
                if (!parse_digits<Iterator, CharT>(input, end, 2u, 0, 59, parsed) ||
                    !assign_field(fields.minute, fields.has_minute, parsed)) return false;
                break;
            case 'S':
                if (!parse_digits<Iterator, CharT>(input, end, 2u, 0, 60, parsed) ||
                    !assign_field(fields.second, fields.has_second, parsed)) return false;
                break;
            case 'p': {
                if (input == end) return false;
                char first_marker = ascii_lower(*input++);
                if (input == end || ascii_lower(*input++) != 'm' ||
                    (first_marker != 'a' && first_marker != 'p')) return false;
                if (!assign_field(fields.pm, fields.has_pm,
                                  first_marker == 'p' ? 1 : 0)) return false;
                break;
            }
            case 'z': {
                long offset = 0;
                if (!parse_timezone_offset<Iterator, CharT>(input, end,
                                                             offset) ||
                    !assign_timezone_offset(fields, offset)) return false;
                break;
            }
            case 'Z': {
                if (fields.has_zone ||
                    !parse_timezone_name<Iterator, CharT>(input, end, fields))
                    return false;
                fields.has_zone = true;
                break;
            }
            case 'j':
                if (!parse_digits<Iterator, CharT>(input, end, 3u, 1, 366, parsed) ||
                    !assign_field(fields.yday, fields.has_yday, parsed - 1)) return false;
                break;
            case 'w':
                if (!parse_digits<Iterator, CharT>(input, end, 1u, 0, 6, parsed) ||
                    !assign_field(fields.wday, fields.has_wday, parsed)) return false;
                break;
            case 'u':
                if (!parse_digits<Iterator, CharT>(input, end, 1u, 1, 7, parsed) ||
                    !assign_field(fields.wday, fields.has_wday, parsed == 7 ? 0 : parsed)) return false;
                break;
            case 'U':
                if (!parse_digits<Iterator, CharT>(input, end, 2u, 0, 53,
                                                    parsed) ||
                    !assign_field(fields.sunday_week,
                                  fields.has_sunday_week, parsed)) return false;
                break;
            case 'W':
                if (!parse_digits<Iterator, CharT>(input, end, 2u, 0, 53,
                                                    parsed) ||
                    !assign_field(fields.monday_week,
                                  fields.has_monday_week, parsed)) return false;
                break;
            case 'G':
                if (!parse_digits<Iterator, CharT>(input, end, 4u, 0, 9999,
                                                    parsed) ||
                    !assign_field(fields.iso_year, fields.has_iso_year,
                                  parsed)) return false;
                break;
            case 'g':
                if (!parse_digits<Iterator, CharT>(input, end, 2u, 0, 99,
                                                    parsed) ||
                    !assign_field(fields.iso_year_of_century,
                        fields.has_iso_year_of_century, parsed)) return false;
                break;
            case 'V':
                if (!parse_digits<Iterator, CharT>(input, end, 2u, 1, 53,
                                                    parsed) ||
                    !assign_field(fields.iso_week, fields.has_iso_week,
                                  parsed)) return false;
                break;
            case 'a':
            case 'A':
                if (!parse_name<Iterator, CharT>(input, end,
                                                  long_weekdays, 7u, parsed) ||
                    !assign_field(fields.wday, fields.has_wday, parsed)) return false;
                break;
            case 'b':
            case 'h':
            case 'B':
                if (!parse_name<Iterator, CharT>(input, end,
                        long_months, 12u, parsed) ||
                    !assign_field(fields.month, fields.has_month, parsed)) return false;
                break;
            case 'c': expansion = c_format; break;
            case 'x': expansion = x_format; break;
            case 'X':
            case 'T': expansion = time_format; break;
            case 'F': expansion = date_format; break;
            case 'D': expansion = short_date_format; break;
            case 'R': expansion = short_time_format; break;
            case 'r': expansion = am_time_format; break;
            default: return false;
        }
        if (expansion != nullptr) {
            const CharT* expansion_end = expansion;
            while (*expansion_end != CharT()) ++expansion_end;
            if (!parse_time(input, end, expansion, expansion_end, fields,
                            depth + 1u)) return false;
        }
    }
    return true;
}

inline bool commit_time(const time_fields& fields, struct ::tm& value) noexcept {
    if (fields.has_hour12 != fields.has_pm) return false;
    if (fields.has_hour && fields.has_hour12) {
        int converted = fields.hour12 % 12 + (fields.pm ? 12 : 0);
        if (converted != fields.hour) return false;
    }

    long long current_year = static_cast<long long>(value.tm_year) + 1900LL;
    long long year = fields.has_year
        ? static_cast<long long>(fields.year) : current_year;
    bool set_year = fields.has_year;
    if (fields.has_year && fields.has_century &&
        fields.year / 100 != fields.century) return false;
    if (fields.has_year && fields.has_year_of_century &&
        fields.year % 100 != fields.year_of_century) return false;
    if (!fields.has_year && fields.has_century) {
        int suffix = fields.has_year_of_century
            ? fields.year_of_century
            : (current_year >= 0 ? static_cast<int>(current_year % 100) : -1);
        if (suffix < 0) return false;
        year = static_cast<long long>(fields.century) * 100LL + suffix;
        set_year = true;
    } else if (!fields.has_year && fields.has_year_of_century) {
        year = fields.year_of_century >= 69
            ? fields.year_of_century + 1900LL
            : fields.year_of_century + 2000LL;
        set_year = true;
    }
    bool set_month = fields.has_month;
    bool set_day = fields.has_day;
    bool set_yday = fields.has_yday;
    bool set_wday = fields.has_wday;
    int month = set_month ? fields.month : value.tm_mon;
    int day = set_day ? fields.day : value.tm_mday;
    int yday = set_yday ? fields.yday : value.tm_yday;
    int wday = set_wday ? fields.wday : value.tm_wday;
    bool has_derived_date = false;
    long long derived_year = year;
    int derived_month = 0;
    int derived_day = 0;
    int derived_yday = 0;
    int derived_wday = wday;

    if (fields.has_sunday_week || fields.has_monday_week) {
        if (!fields.has_wday) return false;
        int sunday_yday = -1;
        int monday_yday = -1;
        if (fields.has_sunday_week &&
            !year_day_from_week(year, fields.sunday_week, fields.wday,
                                false, sunday_yday)) return false;
        if (fields.has_monday_week &&
            !year_day_from_week(year, fields.monday_week, fields.wday,
                                true, monday_yday)) return false;
        if (fields.has_sunday_week && fields.has_monday_week &&
            sunday_yday != monday_yday) return false;
        derived_yday = fields.has_sunday_week ? sunday_yday : monday_yday;
        derived_wday = fields.wday;
        if (!month_day_from_year_day(year, derived_yday, derived_month,
                                     derived_day)) return false;
        has_derived_date = true;
    }

    if (fields.has_iso_week || fields.has_iso_year ||
        fields.has_iso_year_of_century) {
        if (!fields.has_iso_week || !fields.has_wday ||
            (!fields.has_iso_year && !fields.has_iso_year_of_century)) {
            return false;
        }
        long long requested_iso_year = fields.has_iso_year
            ? fields.iso_year
            : (fields.iso_year_of_century >= 69
                ? fields.iso_year_of_century + 1900LL
                : fields.iso_year_of_century + 2000LL);
        if (fields.has_iso_year && fields.has_iso_year_of_century &&
            fields.iso_year % 100 != fields.iso_year_of_century) return false;
        long long iso_derived_year = 0;
        int iso_derived_month = 0;
        int iso_derived_day = 0;
        int iso_derived_yday = 0;
        int iso_wday = fields.wday == 0 ? 7 : fields.wday;
        if (!date_from_iso_week(requested_iso_year, fields.iso_week,
                                iso_wday, iso_derived_year,
                                iso_derived_month, iso_derived_day,
                                iso_derived_yday)) return false;
        if (has_derived_date &&
            (derived_year != iso_derived_year ||
             derived_yday != iso_derived_yday)) return false;
        derived_year = iso_derived_year;
        derived_month = iso_derived_month;
        derived_day = iso_derived_day;
        derived_yday = iso_derived_yday;
        derived_wday = fields.wday;
        has_derived_date = true;
    }

    if (has_derived_date) {
        if ((set_year && year != derived_year) ||
            (set_month && month != derived_month) ||
            (set_day && day != derived_day) ||
            (set_yday && yday != derived_yday) ||
            (set_wday && wday != derived_wday)) return false;
        year = derived_year;
        month = derived_month;
        day = derived_day;
        yday = derived_yday;
        wday = derived_wday;
        set_year = set_month = set_day = set_yday = set_wday = true;
    }

    if (set_month && set_day &&
        (day < 1 || day > month_days(year, month))) return false;
    if (set_yday) {
        if (yday < 0 || yday >= (leap_year(year) ? 366 : 365)) return false;
        if (set_month && set_day && yday != year_day(year, month, day))
            return false;
    }
    if (set_wday && set_year && set_month && set_day &&
        wday != week_day(year, month, day)) return false;

#if !RINCXX_TM_HAS_GNU_EXTENSIONS
    struct ::tm original = value;
#endif
    struct ::tm candidate = value;
    if (set_year) candidate.tm_year = static_cast<int>(year) - 1900;
    if (set_month) candidate.tm_mon = month;
    if (set_day) candidate.tm_mday = day;
    if (fields.has_hour) candidate.tm_hour = fields.hour;
    else if (fields.has_hour12) candidate.tm_hour = fields.hour12 % 12 +
                                                    (fields.pm ? 12 : 0);
    if (fields.has_minute) candidate.tm_min = fields.minute;
    if (fields.has_second) candidate.tm_sec = fields.second;
#if RINCXX_TM_HAS_GNU_EXTENSIONS
    if (fields.has_gmtoff) candidate.tm_gmtoff = fields.gmtoff;
    if (fields.has_zone) candidate.tm_zone = fields.zone;
#else
    if (fields.has_gmtoff || fields.has_zone) {
        unsigned flags = 0u;
        long offset = 0L;
        char zone[64] = {};
        if (!rin_cxx_tm_timezone_get ||
            (rin_cxx_tm_timezone_get(&value, &offset, zone, sizeof(zone),
                                     &flags) == 0)) {
            flags = 0u;
            offset = 0L;
            zone[0] = '\0';
        }
        if (fields.has_gmtoff) {
            offset = fields.gmtoff;
            flags |= RINCXX_TM_TIMEZONE_HAS_OFFSET;
        }
        if (fields.has_zone) {
            size_t index = 0u;
            while (fields.zone[index] != '\0' && index + 1u < sizeof(zone)) {
                zone[index] = fields.zone[index];
                ++index;
            }
            if (fields.zone[index] != '\0') return false;
            zone[index] = '\0';
            flags |= RINCXX_TM_TIMEZONE_HAS_NAME;
        }
        if (!rin_cxx_tm_timezone_set) return false;
        value = candidate;
        if (!rin_cxx_tm_timezone_set(&value, flags, offset, zone)) {
            value = original;
            return false;
        }
        return true;
    }
#endif
    if (set_yday) candidate.tm_yday = yday;
    else if (set_year && set_month && set_day) {
        candidate.tm_yday = year_day(year, month, day);
    }
    if (set_wday) candidate.tm_wday = wday;
    else if (set_year && set_month && set_day) {
        candidate.tm_wday = week_day(year, month, day);
    }
    value = candidate;
#if !RINCXX_TM_HAS_GNU_EXTENSIONS
    /* A hosted standard `tm` has no timezone fields of its own.  The optional
     * sidecar is therefore part of the same logical object and must not
     * survive a later parse that did not contain `%z` or `%Z`: otherwise a
     * reused `tm` formats a stale zone unrelated to its current value.  Clear
     * the owner only after the calendar candidate is complete; a bound owner
     * that cannot clear its record makes the whole commit fail atomically. */
    if (rin_cxx_tm_timezone_get && rin_cxx_tm_timezone_set) {
        unsigned prior_flags = 0u;
        long prior_offset = 0L;
        /* Query first: a calendar-only parse of a fresh `tm` must not reserve
         * an empty sidecar slot merely to represent absence of `%z`/`%Z`. */
        if (rin_cxx_tm_timezone_get(&value, &prior_offset, nullptr, 0u,
                                    &prior_flags) && prior_flags != 0u &&
            !rin_cxx_tm_timezone_set(&value, 0u, 0L, nullptr)) {
            value = original;
            return false;
        }
    }
#endif
    return true;
}

template<typename Iterator, typename CharT>
inline Iterator emit_padded(Iterator output, ios_base& stream, CharT fill,
                            const CharT* text, size_t length) {
    long requested = __locale_stream_width(stream);
    __locale_stream_width_reset(stream);
    unsigned long long padding = requested > 0 &&
            static_cast<unsigned long long>(requested) > length
        ? static_cast<unsigned long long>(requested) - length : 0u;
    bool left_adjusted = (__locale_stream_flags(stream) & 0x0020u) != 0u;
    if (!left_adjusted) while (padding != 0u) {
        *output++ = fill;
        --padding;
    }
    for (unsigned index = 0; index < length; ++index) *output++ = text[index];
    if (left_adjusted) while (padding != 0u) {
        *output++ = fill;
        --padding;
    }
    return output;
}

template<typename CharT>
inline const CharT* time_token_end(const CharT* first,
                                   const CharT* last) noexcept {
    if (first == last) return last;
    const CharT* result = first + 1;
    if (!ascii_equal(*first, '%') || result == last) return result;
    char directive = static_cast<char>(*result++);
    if ((directive == 'E' || directive == 'O') && result != last) ++result;
    return result;
}

template<typename CharT>
inline bool locale_name_has_prefix(const char* name, const char* prefix) noexcept {
    if (name == nullptr || prefix == nullptr) return false;
    while (*prefix != '\0') {
        if (*name++ != *prefix++) return false;
    }
    return true;
}
template<typename CharT>
inline bool localized_date_token(const locale& selected,
                                 const CharT* first, const CharT* last,
                                 CharT* scratch, unsigned capacity,
                                 const CharT*& effective_first,
                                 const CharT*& effective_last) noexcept {
    effective_first = first;
    effective_last = last;
    if (first == nullptr || last == nullptr || first == last ||
        static_cast<unsigned>(last - first) != 2u ||
        !ascii_equal(first[0], '%') || !ascii_equal(first[1], 'x') ||
        scratch == nullptr || capacity < 2u) return false;
    const char* name = selected._name_c_str();
    if (name == nullptr || name[0] == '\0' || name[0] == '*') return false;
    const char* profile = nullptr;
    if (locale_name_has_prefix<CharT>(name, "de_DE")) {
        profile = "%d.%m.%Y";
    } else if (locale_name_has_prefix<CharT>(name, "fr_FR") ||
               locale_name_has_prefix<CharT>(name, "es_ES") ||
               locale_name_has_prefix<CharT>(name, "it_IT") ||
               locale_name_has_prefix<CharT>(name, "pt_BR")) {
        profile = "%d/%m/%Y";
    } else if (locale_name_has_prefix<CharT>(name, "ja_JP")) {
        profile = "%Y/%m/%d";
    } else {
        return false;
    }
    unsigned length = 0u;
    while (profile[length] != '\0') {
        if (length + 1u >= capacity) return false;
        scratch[length] = static_cast<CharT>(profile[length]);
        ++length;
    }
    scratch[length] = CharT();
    effective_first = scratch;
    effective_last = scratch + length;
    return true;
}

template<typename CharT>
inline bool time_tokens_fit_buffer(const struct ::tm& value,
                                   const CharT* first,
                                   const CharT* last) noexcept {
    while (first != last) {
        const CharT* token_end = time_token_end(first, last);
        unsigned token_length = 0u;
        if (!format_time(static_cast<CharT*>(nullptr), 0u, token_length,
                         value, first, token_end) || token_length > 512u) {
            return false;
        }
        first = token_end;
    }
    return true;
}

template<typename Iterator, typename CharT>
inline Iterator emit_time_tokens(Iterator output, const struct ::tm& value,
                                 const CharT* first, const CharT* last) {
    while (first != last) {
        const CharT* token_end = time_token_end(first, last);
        CharT token[512];
        unsigned token_length = 0u;
        if (!format_time(token, 512u, token_length, value, first, token_end))
            return output;
        for (unsigned index = 0u; index < token_length; ++index)
            *output++ = token[index];
        first = token_end;
    }
    return output;
}

} /* namespace __locale_detail */

template<typename CharT, typename OutputIterator = ostreambuf_iterator<CharT>>
class time_put : public locale::facet {
public:
    using char_type = CharT;
    using iter_type = OutputIterator;
    static locale::id id;

    explicit time_put(size_t refs = 0) : locale::facet(refs) {}

    bool valid(const struct ::tm* value, const CharT* first,
               const CharT* last) const noexcept {
        unsigned length = 0u;
        return value != nullptr && first != nullptr && last != nullptr &&
               __locale_detail::format_time(
                   static_cast<CharT*>(nullptr), 0u, length,
                   *value, first, last) &&
               __locale_detail::time_tokens_fit_buffer(
                   *value, first, last);
    }

    iter_type put(iter_type output, ios_base& stream, CharT fill,
                  const struct ::tm* value, const CharT* first,
                  const CharT* last) const {
        return do_put(output, stream, fill, value, first, last);
    }

protected:
    virtual ~time_put() = default;

    virtual iter_type do_put(iter_type output, ios_base& stream, CharT fill,
                             const struct ::tm* value, const CharT* first,
                             const CharT* last) const {
        CharT localized_date[16];
        const CharT* effective_first = first;
        const CharT* effective_last = last;
        __locale_detail::localized_date_token(
            __locale_stream_locale(stream), first, last, localized_date,
            static_cast<unsigned>(sizeof(localized_date) /
                                  sizeof(localized_date[0])),
            effective_first, effective_last);
        unsigned length = 0u;
        if (value == nullptr || first == nullptr || last == nullptr ||
            !__locale_detail::format_time(
                static_cast<CharT*>(nullptr), 0u, length, *value,
                effective_first, effective_last) ||
            !__locale_detail::time_tokens_fit_buffer(
                *value, effective_first, effective_last)) {
            return output;
        }
        long requested = __locale_stream_width(stream);
        __locale_stream_width_reset(stream);
        unsigned long long padding = requested > 0 &&
                static_cast<unsigned long long>(requested) > length
            ? static_cast<unsigned long long>(requested) - length : 0u;
        bool left_adjusted =
            (__locale_stream_flags(stream) & 0x0020u) != 0u;
        if (!left_adjusted) while (padding != 0u) {
            *output++ = fill;
            --padding;
        }
        output = __locale_detail::emit_time_tokens(
            output, *value, effective_first, effective_last);
        if (left_adjusted) while (padding != 0u) {
            *output++ = fill;
            --padding;
        }
        return output;
    }
};

template<typename CharT, typename OutputIterator>
locale::id time_put<CharT, OutputIterator>::id;

template<typename CharT, typename InputIterator = istreambuf_iterator<CharT>>
class time_get : public locale::facet, public time_base {
public:
    using char_type = CharT;
    using iter_type = InputIterator;
    static locale::id id;

    explicit time_get(size_t refs = 0) : locale::facet(refs) {}

    dateorder date_order() const { return do_date_order(); }

    iter_type get_time(iter_type input, iter_type end, ios_base& stream,
                       unsigned int& error, struct ::tm* value) const {
        return do_get_time(input, end, stream, error, value);
    }

    iter_type get_date(iter_type input, iter_type end, ios_base& stream,
                       unsigned int& error, struct ::tm* value) const {
        return do_get_date(input, end, stream, error, value);
    }

    iter_type get_weekday(iter_type input, iter_type end, ios_base& stream,
                          unsigned int& error, struct ::tm* value) const {
        return do_get_weekday(input, end, stream, error, value);
    }

    iter_type get_monthname(iter_type input, iter_type end, ios_base& stream,
                            unsigned int& error, struct ::tm* value) const {
        return do_get_monthname(input, end, stream, error, value);
    }

    iter_type get_year(iter_type input, iter_type end, ios_base& stream,
                       unsigned int& error, struct ::tm* value) const {
        return do_get_year(input, end, stream, error, value);
    }

    iter_type get(iter_type input, iter_type end, ios_base& stream,
                  unsigned int& error, struct ::tm* value, char format,
                  char modifier = 0) const {
        CharT selected[3];
        selected[0] = static_cast<CharT>('%');
        const CharT* selected_end;
        if (modifier == 0) {
            selected[1] = static_cast<CharT>(format);
            selected_end = selected + 2;
        } else if (modifier == 'E' || modifier == 'O') {
            selected[1] = static_cast<CharT>(modifier);
            selected[2] = static_cast<CharT>(format);
            selected_end = selected + 3;
        } else {
            error |= __locale_failbit();
            if (input == end) error |= __locale_eofbit();
            return input;
        }
        return do_get(input, end, stream, error, value, selected,
                      selected_end);
    }

    iter_type get(iter_type input, iter_type end, ios_base& stream,
                  unsigned int& error, struct ::tm* value,
                  const CharT* first, const CharT* last) const {
        return do_get(input, end, stream, error, value, first, last);
    }

protected:
    virtual ~time_get() = default;

    virtual dateorder do_date_order() const { return mdy; }

    virtual iter_type do_get_time(iter_type input, iter_type end, ios_base&,
                                  unsigned int& error,
                                  struct ::tm* value) const {
        static constexpr CharT format[] = {'%','X'};
        return parse_format(input, end, error, value, format, format + 2);
    }

    virtual iter_type do_get_date(iter_type input, iter_type end,
                                  ios_base& stream, unsigned int& error,
                                  struct ::tm* value) const {
        static constexpr CharT format[] = {'%','x'};
        CharT localized_date[16];
        const CharT* effective_first = format;
        const CharT* effective_last = format + 2;
        __locale_detail::localized_date_token(
            __locale_stream_locale(stream), format, format + 2,
            localized_date, static_cast<unsigned>(sizeof(localized_date) /
                                                  sizeof(localized_date[0])),
            effective_first, effective_last);
        return parse_format(input, end, error, value,
                            effective_first, effective_last);
    }

    virtual iter_type do_get_weekday(iter_type input, iter_type end,
                                     ios_base&, unsigned int& error,
                                     struct ::tm* value) const {
        static constexpr CharT format[] = {'%','a'};
        return parse_format(input, end, error, value, format, format + 2);
    }

    virtual iter_type do_get_monthname(iter_type input, iter_type end,
                                       ios_base&, unsigned int& error,
                                       struct ::tm* value) const {
        static constexpr CharT format[] = {'%','b'};
        return parse_format(input, end, error, value, format, format + 2);
    }

    virtual iter_type do_get_year(iter_type input, iter_type end, ios_base&,
                                  unsigned int& error,
                                  struct ::tm* value) const {
        static constexpr CharT format[] = {'%','Y'};
        return parse_format(input, end, error, value, format, format + 2);
    }

    virtual iter_type do_get(iter_type input, iter_type end, ios_base& stream,
                             unsigned int& error, struct ::tm* value,
                             const CharT* first, const CharT* last) const {
        CharT localized_date[16];
        const CharT* effective_first = first;
        const CharT* effective_last = last;
        __locale_detail::localized_date_token(
            __locale_stream_locale(stream), first, last, localized_date,
            static_cast<unsigned>(sizeof(localized_date) /
                                  sizeof(localized_date[0])),
            effective_first, effective_last);
        return parse_format(input, end, error, value,
                            effective_first, effective_last);
    }

private:
    static iter_type parse_format(iter_type input, iter_type end,
                                  unsigned int& error, struct ::tm* value,
                                  const CharT* first,
                                  const CharT* last) {
        __locale_detail::time_fields fields;
        if (value == nullptr || first == nullptr || last == nullptr ||
            !__locale_detail::parse_time(input, end, first, last, fields) ||
            !__locale_detail::commit_time(fields, *value)) {
            error |= __locale_failbit();
        }
        if (input == end) error |= __locale_eofbit();
        return input;
    }
};

template<typename CharT, typename InputIterator>
locale::id time_get<CharT, InputIterator>::id;

namespace __locale_detail {

template<typename CharT>
inline bool append_money_character(basic_string<CharT>& output,
                                   CharT value) {
    if (output.size() == output.max_size()) return false;
    size_t previous = output.size();
    output.push_back(value);
    return output.size() == previous + 1u;
}

template<typename CharT>
inline bool append_money_text(basic_string<CharT>& output,
                              const basic_string<CharT>& text) {
    for (CharT value : text) {
        if (!append_money_character(output, value)) return false;
    }
    return true;
}

inline bool valid_money_pattern(money_base::pattern pattern) noexcept {
    unsigned symbols = 0u;
    unsigned signs = 0u;
    unsigned values = 0u;
    unsigned separators = 0u;
    for (unsigned index = 0u; index < 4u; ++index) {
        switch (pattern.field[index]) {
            case money_base::symbol: ++symbols; break;
            case money_base::sign: ++signs; break;
            case money_base::value: ++values; break;
            case money_base::none:
                if (index == 0u) return false;
                ++separators;
                break;
            case money_base::space:
                if (index == 0u || index == 3u) return false;
                ++separators;
                break;
            default: return false;
        }
    }
    return symbols == 1u && signs == 1u && values == 1u &&
           separators == 1u;
}

inline bool money_pattern_has_space(money_base::pattern pattern) noexcept {
    for (unsigned index = 0u; index < 4u; ++index) {
        if (pattern.field[index] == money_base::space) return true;
    }
    return false;
}

template<typename CharT, bool International>
inline bool format_money_digits(basic_string<CharT>& output,
                                const CharT* digits, size_t digit_count,
                                bool negative,
                                bool show_symbol, const locale& selected,
                                bool internal_adjust = false,
                                size_t internal_fill = 0u,
                                CharT fill = static_cast<CharT>(' ')) {
    output.clear();
    if (digits == nullptr || digit_count == 0u) return false;
    const auto& punctuation = use_facet<moneypunct<CharT, International>>(
        selected);
    int fraction_digits = punctuation.frac_digits();
    if (fraction_digits < 0) return false;
    size_t required = static_cast<size_t>(fraction_digits) + 1u;
    size_t padded_count = digit_count < required ? required : digit_count;
    size_t leading = padded_count - digit_count;
    for (size_t index = 0; index < digit_count; ++index) {
        CharT current = digits[index];
        if (current < static_cast<CharT>('0') ||
            current > static_cast<CharT>('9')) return false;
    }

    size_t integer_count = padded_count - static_cast<size_t>(fraction_digits);
    string grouping = punctuation.grouping();
    string group_widths;
    if (!grouping.empty() && integer_count > 1u) {
        size_t group_index = 0u;
        unsigned group_size = static_cast<unsigned char>(grouping[0]);
        if (group_size == 0u || group_size >= 127u) return false;
        size_t grouped_digits = 0u;
        bool repeat_group = false;
        while (integer_count - grouped_digits > group_size) {
            if (!append_money_character(
                    group_widths, static_cast<char>(group_size))) return false;
            grouped_digits += group_size;
            if (!repeat_group && group_index + 1u < grouping.size()) {
                unsigned next = static_cast<unsigned char>(
                    grouping[++group_index]);
                if (next == 0u) {
                    repeat_group = true;
                } else if (next >= 127u) {
                    break;
                } else {
                    group_size = next;
                }
            } else {
                repeat_group = true;
            }
        }
    }

    auto padded_digit = [&](size_t index) -> CharT {
        return index < leading ? static_cast<CharT>('0')
                               : digits[index - leading];
    };
    auto append_numeric = [&]() -> bool {
        size_t grouped_digits = 0u;
        for (char width : group_widths) {
            grouped_digits += static_cast<unsigned char>(width);
        }
        size_t digit_index = 0u;
        size_t leftmost = integer_count - grouped_digits;
        for (; digit_index < leftmost; ++digit_index) {
            if (!append_money_character(output,
                                        padded_digit(digit_index))) return false;
        }
        for (size_t group = group_widths.size(); group != 0u; --group) {
            if (!append_money_character(output,
                                        punctuation.thousands_sep())) return false;
            unsigned width = static_cast<unsigned char>(
                group_widths[group - 1u]);
            for (unsigned index = 0u; index < width; ++index, ++digit_index) {
                if (!append_money_character(output,
                                            padded_digit(digit_index))) return false;
            }
        }
        if (fraction_digits != 0) {
            if (!append_money_character(output,
                                        punctuation.decimal_point())) return false;
            for (; digit_index < padded_count; ++digit_index) {
                if (!append_money_character(output,
                                            padded_digit(digit_index))) return false;
            }
        }
        return true;
    };

    basic_string<CharT> symbol = punctuation.curr_symbol();
    basic_string<CharT> sign = negative ? punctuation.negative_sign()
                                        : punctuation.positive_sign();
    money_base::pattern pattern = negative ? punctuation.neg_format()
                                           : punctuation.pos_format();
    if (!valid_money_pattern(pattern)) return false;
    for (unsigned field = 0; field < 4u; ++field) {
        switch (pattern.field[field]) {
            case money_base::none:
                if (internal_adjust) {
                    for (size_t index = 0u; index < internal_fill; ++index) {
                        if (!append_money_character(output, fill)) return false;
                    }
                }
                break;
            case money_base::space: {
                size_t count = internal_adjust && internal_fill != 0u
                    ? internal_fill : 1u;
                for (size_t index = 0u; index < count; ++index) {
                    if (!append_money_character(output, fill)) return false;
                }
                break;
            }
            case money_base::symbol:
                if (show_symbol && !append_money_text(output, symbol)) return false;
                break;
            case money_base::sign:
                if (!sign.empty() && !append_money_character(
                        output, sign[0])) return false;
                break;
            case money_base::value:
                if (!append_numeric()) return false;
                break;
            default: return false;
        }
    }
    for (size_t index = 1u; index < sign.size(); ++index) {
        if (!append_money_character(output, sign[index])) return false;
    }
    return true;
}

template<typename CharT, bool International>
inline bool format_money_string(basic_string<CharT>& output,
                                const basic_string<CharT>& input,
                                bool show_symbol, const locale& selected,
                                bool internal_adjust = false,
                                size_t internal_fill = 0u,
                                CharT fill = static_cast<CharT>(' ')) {
    if (input.empty()) return false;
    unsigned offset = 0;
    bool negative = false;
    if (input[0] == static_cast<CharT>('-') ||
        input[0] == static_cast<CharT>('+')) {
        negative = input[0] == static_cast<CharT>('-');
        offset = 1;
    }
    if (offset == input.size()) return false;
    return format_money_digits<CharT, International>(
        output, input.data() + offset, input.size() - offset, negative,
        show_symbol, selected, internal_adjust, internal_fill, fill);
}

template<typename CharT, bool International>
inline bool format_money_number(basic_string<CharT>& output,
                                long double input,
                                bool show_symbol, const locale& selected,
                                bool internal_adjust = false,
                                size_t internal_fill = 0u,
                                CharT fill = static_cast<CharT>(' ')) {
    if (!__builtin_isfinite(input)) return false;
    bool negative = __builtin_signbit(input);
    long double magnitude = negative ? -input : input;
    constexpr unsigned long long maximum =
        numeric_limits<unsigned long long>::max();
    if (magnitude >= static_cast<long double>(maximum)) return false;
    unsigned long long units = static_cast<unsigned long long>(magnitude + 0.5L);
    CharT digits[32];
    unsigned count = 0;
    do {
        digits[count++] = static_cast<CharT>('0' + units % 10u);
        units /= 10u;
    } while (units != 0u);
    for (unsigned left = 0, right = count - 1u; left < right; ++left, --right) {
        CharT temporary = digits[left];
        digits[left] = digits[right];
        digits[right] = temporary;
    }
    return format_money_digits<CharT, International>(
        output, digits, count, negative, show_symbol, selected,
        internal_adjust, internal_fill, fill);
}

template<typename Iterator, typename CharT>
inline bool match_text(Iterator& input, const Iterator& end,
                       const basic_string<CharT>& text) {
    for (CharT expected : text) {
        if (input == end || *input != expected) return false;
        ++input;
    }
    return true;
}

template<typename Iterator, typename CharT, bool International>
inline bool parse_money(Iterator& input, const Iterator& end,
                        bool show_symbol, basic_string<CharT>& digits,
                        bool& negative,
                        const locale& selected) {
    const auto& punctuation = use_facet<moneypunct<CharT, International>>(
        selected);
    basic_string<CharT> symbol = punctuation.curr_symbol();
    basic_string<CharT> positive = punctuation.positive_sign();
    basic_string<CharT> minus = punctuation.negative_sign();
    money_base::pattern negative_pattern = punctuation.neg_format();
    if (!valid_money_pattern(negative_pattern)) return false;
    negative = false;
    size_t sign_size = 0u;

    string grouping = punctuation.grouping();
    const auto& classification = use_facet<ctype<CharT>>(selected);
    digits.clear();
    bool parsed_value = false;
    auto parse_value = [&]() -> bool {
        if (parsed_value) return false;
        parsed_value = true;
        string groups;
        size_t current_group = 0u;
        while (input != end) {
            if (*input >= static_cast<CharT>('0') &&
                *input <= static_cast<CharT>('9')) {
                if (!append_money_character(digits, *input++)) return false;
                ++current_group;
                continue;
            }
            if (!grouping.empty() && *input == punctuation.thousands_sep()) {
                if (current_group == 0u || current_group >= 127u ||
                    !append_money_character(
                        groups, static_cast<char>(current_group))) return false;
                current_group = 0u;
                ++input;
                continue;
            }
            break;
        }
        if (digits.empty() || current_group == 0u) return false;
        if (!groups.empty()) {
            if (current_group >= 127u || !append_money_character(
                    groups, static_cast<char>(current_group))) return false;
            size_t pattern_index = 0u;
            unsigned expected = static_cast<unsigned char>(grouping[0]);
            if (expected == 0u || expected >= 127u) return false;
            bool repeat_group = false;
            bool unrestricted_leftmost = false;
            for (size_t group = groups.size(); group != 0u; --group) {
                unsigned actual = static_cast<unsigned char>(
                    groups[group - 1u]);
                bool leftmost = group == 1u;
                if ((leftmost && !unrestricted_leftmost &&
                     (actual == 0u || actual > expected)) ||
                    (!leftmost && actual != expected)) return false;
                if (!leftmost && !repeat_group &&
                    pattern_index + 1u < grouping.size()) {
                    unsigned next = static_cast<unsigned char>(
                        grouping[++pattern_index]);
                    if (next == 0u) {
                        repeat_group = true;
                    } else if (next >= 127u) {
                        if (group != 2u) return false;
                        unrestricted_leftmost = true;
                    } else {
                        expected = next;
                    }
                } else if (!leftmost) {
                    repeat_group = true;
                }
            }
        }
        int fraction_digits = punctuation.frac_digits();
        if (fraction_digits < 0) return false;
        if (fraction_digits != 0 && input != end &&
            *input == punctuation.decimal_point()) {
            ++input;
            for (int index = 0; index < fraction_digits; ++index) {
                if (input == end || *input < static_cast<CharT>('0') ||
                    *input > static_cast<CharT>('9') ||
                    !append_money_character(digits, *input++)) return false;
            }
            if (input != end && *input >= static_cast<CharT>('0') &&
                *input <= static_cast<CharT>('9')) return false;
        }
        return true;
    };

    for (unsigned field = 0; field < 4u; ++field) {
        switch (negative_pattern.field[field]) {
            case money_base::none:
                if (field != 3u) {
                    while (input != end && classification.is(
                            ctype_base::space, *input)) ++input;
                }
                break;
            case money_base::space:
                if (input == end || !classification.is(
                        ctype_base::space, *input)) return false;
                ++input;
                while (input != end && classification.is(
                        ctype_base::space, *input)) ++input;
                break;
            case money_base::symbol:
                if ((show_symbol || (!symbol.empty() && input != end &&
                     *input == symbol[0])) &&
                    !match_text(input, end, symbol)) return false;
                break;
            case money_base::sign:
                if (!positive.empty() && input != end &&
                    *input == positive[0]) {
                    sign_size = positive.size();
                    ++input;
                } else if (!minus.empty() && input != end &&
                           *input == minus[0]) {
                    negative = true;
                    sign_size = minus.size();
                    ++input;
                } else if (!positive.empty() && minus.empty()) {
                    negative = true;
                } else if (!positive.empty() && !minus.empty()) {
                    return false;
                }
                break;
            case money_base::value:
                if (!parse_value()) return false;
                break;
            default: return false;
        }
    }
    if (sign_size > 1u) {
        const basic_string<CharT>& sign = negative ? minus : positive;
        for (size_t index = 1u; index < sign_size; ++index) {
            if (input == end || *input != sign[index]) return false;
            ++input;
        }
    }
    return parsed_value;
}

} /* namespace __locale_detail */

template<typename CharT, typename OutputIterator = ostreambuf_iterator<CharT>>
class money_put : public locale::facet {
public:
    using char_type = CharT;
    using iter_type = OutputIterator;
    using string_type = basic_string<CharT>;
    static locale::id id;

    explicit money_put(size_t refs = 0) : locale::facet(refs) {}

    bool valid(long double value, bool international, bool show_symbol,
               const locale& selected) const {
        string_type candidate;
        return international
            ? __locale_detail::format_money_number<CharT, true>(
                  candidate, value, show_symbol, selected)
            : __locale_detail::format_money_number<CharT, false>(
                  candidate, value, show_symbol, selected);
    }

    bool valid(const string_type& value, bool international,
               bool show_symbol, const locale& selected) const {
        string_type candidate;
        return international
            ? __locale_detail::format_money_string<CharT, true>(
                  candidate, value, show_symbol, selected)
            : __locale_detail::format_money_string<CharT, false>(
                  candidate, value, show_symbol, selected);
    }

    iter_type put(iter_type output, bool international, ios_base& stream,
                  CharT fill, long double value) const {
        return do_put(output, international, stream, fill, value);
    }

    iter_type put(iter_type output, bool international, ios_base& stream,
                  CharT fill, const string_type& value) const {
        return do_put(output, international, stream, fill, value);
    }

protected:
    virtual ~money_put() = default;

    virtual iter_type do_put(iter_type output, bool international,
                             ios_base& stream, CharT fill,
                             long double value) const {
        string_type candidate;
        bool show_symbol = (__locale_stream_flags(stream) & 0x0200u) != 0u;
        locale selected = __locale_stream_locale(stream);
        bool okay = international
            ? __locale_detail::format_money_number<CharT, true>(
                  candidate, value, show_symbol, selected, false, 0u, fill)
            : __locale_detail::format_money_number<CharT, false>(
                  candidate, value, show_symbol, selected, false, 0u, fill);
        if (okay && (__locale_stream_flags(stream) & 0x0010u) != 0u) {
            bool negative = __builtin_signbit(value);
            money_base::pattern pattern = international
                ? (negative
                    ? use_facet<moneypunct<CharT, true>>(selected).neg_format()
                    : use_facet<moneypunct<CharT, true>>(selected).pos_format())
                : (negative
                    ? use_facet<moneypunct<CharT, false>>(selected).neg_format()
                    : use_facet<moneypunct<CharT, false>>(selected).pos_format());
            size_t structural = candidate.size() -
                (__locale_detail::money_pattern_has_space(pattern) ? 1u : 0u);
            long requested = __locale_stream_width(stream);
            size_t internal_fill = requested > 0 &&
                    static_cast<unsigned long long>(requested) > structural
                ? static_cast<size_t>(requested) - structural : 0u;
            okay = international
                ? __locale_detail::format_money_number<CharT, true>(
                      candidate, value, show_symbol, selected, true,
                      internal_fill, fill)
                : __locale_detail::format_money_number<CharT, false>(
                      candidate, value, show_symbol, selected, true,
                      internal_fill, fill);
        }
        if (!okay) return output;
        return __locale_detail::emit_padded(output, stream, fill,
                                             candidate.data(), candidate.size());
    }

    virtual iter_type do_put(iter_type output, bool international,
                             ios_base& stream, CharT fill,
                             const string_type& value) const {
        string_type candidate;
        bool show_symbol = (__locale_stream_flags(stream) & 0x0200u) != 0u;
        locale selected = __locale_stream_locale(stream);
        bool okay = international
            ? __locale_detail::format_money_string<CharT, true>(
                  candidate, value, show_symbol, selected, false, 0u, fill)
            : __locale_detail::format_money_string<CharT, false>(
                  candidate, value, show_symbol, selected, false, 0u, fill);
        if (okay && (__locale_stream_flags(stream) & 0x0010u) != 0u) {
            bool negative = !value.empty() &&
                value[0] == static_cast<CharT>('-');
            money_base::pattern pattern = international
                ? (negative
                    ? use_facet<moneypunct<CharT, true>>(selected).neg_format()
                    : use_facet<moneypunct<CharT, true>>(selected).pos_format())
                : (negative
                    ? use_facet<moneypunct<CharT, false>>(selected).neg_format()
                    : use_facet<moneypunct<CharT, false>>(selected).pos_format());
            size_t structural = candidate.size() -
                (__locale_detail::money_pattern_has_space(pattern) ? 1u : 0u);
            long requested = __locale_stream_width(stream);
            size_t internal_fill = requested > 0 &&
                    static_cast<unsigned long long>(requested) > structural
                ? static_cast<size_t>(requested) - structural : 0u;
            okay = international
                ? __locale_detail::format_money_string<CharT, true>(
                      candidate, value, show_symbol, selected, true,
                      internal_fill, fill)
                : __locale_detail::format_money_string<CharT, false>(
                      candidate, value, show_symbol, selected, true,
                      internal_fill, fill);
        }
        if (!okay) return output;
        return __locale_detail::emit_padded(output, stream, fill,
                                             candidate.data(), candidate.size());
    }
};

template<typename CharT, typename OutputIterator>
locale::id money_put<CharT, OutputIterator>::id;

template<typename CharT, typename InputIterator = istreambuf_iterator<CharT>>
class money_get : public locale::facet {
public:
    using char_type = CharT;
    using iter_type = InputIterator;
    using string_type = basic_string<CharT>;
    static locale::id id;

    explicit money_get(size_t refs = 0) : locale::facet(refs) {}

    iter_type get(iter_type input, iter_type end, bool international,
                  ios_base& stream, unsigned int& error,
                  long double& value) const {
        return do_get(input, end, international, stream, error, value);
    }

    iter_type get(iter_type input, iter_type end, bool international,
                  ios_base& stream, unsigned int& error,
                  string_type& value) const {
        return do_get(input, end, international, stream, error, value);
    }

protected:
    virtual ~money_get() = default;

    virtual iter_type do_get(iter_type input, iter_type end,
                             bool international, ios_base& stream,
                             unsigned int& error, long double& value) const {
        string_type digits;
        bool negative = false;
        bool show_symbol = (__locale_stream_flags(stream) & 0x0200u) != 0u;
        locale selected = __locale_stream_locale(stream);
        bool okay = international
            ? __locale_detail::parse_money<iter_type, CharT, true>(
                  input, end, show_symbol, digits, negative, selected)
            : __locale_detail::parse_money<iter_type, CharT, false>(
                  input, end, show_symbol, digits, negative, selected);
        long double candidate = 0.0L;
        if (okay) {
            for (size_t index = 0; index < digits.size(); ++index) {
                candidate = candidate * 10.0L +
                    static_cast<int>(digits[index] - static_cast<CharT>('0'));
                if (!__builtin_isfinite(candidate)) { okay = false; break; }
            }
        }
        if (okay) value = negative ? -candidate : candidate;
        else error |= __locale_failbit();
        if (input == end) error |= __locale_eofbit();
        return input;
    }

    virtual iter_type do_get(iter_type input, iter_type end,
                             bool international, ios_base& stream,
                             unsigned int& error, string_type& value) const {
        string_type digits;
        bool negative = false;
        bool show_symbol = (__locale_stream_flags(stream) & 0x0200u) != 0u;
        locale selected = __locale_stream_locale(stream);
        bool okay = international
            ? __locale_detail::parse_money<iter_type, CharT, true>(
                  input, end, show_symbol, digits, negative, selected)
            : __locale_detail::parse_money<iter_type, CharT, false>(
                  input, end, show_symbol, digits, negative, selected);
        if (okay) {
            string_type candidate;
            if (negative && !__locale_detail::append_money_character(
                    candidate, static_cast<CharT>('-'))) okay = false;
            if (okay && !__locale_detail::append_money_text(
                    candidate, digits)) okay = false;
            if (okay) value = candidate;
        }
        if (!okay) {
            error |= __locale_failbit();
        }
        if (input == end) error |= __locale_eofbit();
        return input;
    }
};

template<typename CharT, typename InputIterator>
locale::id money_get<CharT, InputIterator>::id;

inline locale::category locale::_category_for_id(size_t value) noexcept {
    if (value == std::collate<char>::id._get_id() ||
        value == std::collate<wchar_t>::id._get_id())
        return locale::collate;
    if (value == std::ctype<char>::id._get_id() ||
        value == std::ctype<wchar_t>::id._get_id() ||
        value == std::codecvt<char, char, mbstate_t>::id._get_id() ||
        value == std::codecvt<wchar_t, char, mbstate_t>::id._get_id() ||
        value == std::codecvt<char16_t, char, mbstate_t>::id._get_id() ||
        value == std::codecvt<char32_t, char, mbstate_t>::id._get_id())
        return locale::ctype;
    if (value == std::moneypunct<char, false>::id._get_id() ||
        value == std::moneypunct<char, true>::id._get_id() ||
        value == std::moneypunct<wchar_t, false>::id._get_id() ||
        value == std::moneypunct<wchar_t, true>::id._get_id() ||
        value == std::money_put<char>::id._get_id() ||
        value == std::money_put<wchar_t>::id._get_id() ||
        value == std::money_get<char>::id._get_id() ||
        value == std::money_get<wchar_t>::id._get_id())
        return locale::monetary;
    if (value == std::numpunct<char>::id._get_id() ||
        value == std::numpunct<wchar_t>::id._get_id() ||
        value == std::num_get<char>::id._get_id() ||
        value == std::num_get<wchar_t>::id._get_id() ||
        value == std::num_put<char>::id._get_id() ||
        value == std::num_put<wchar_t>::id._get_id())
        return locale::numeric;
    if (value == std::time_put<char>::id._get_id() ||
        value == std::time_put<wchar_t>::id._get_id() ||
        value == std::time_get<char>::id._get_id() ||
        value == std::time_get<wchar_t>::id._get_id())
        return locale::time;
    if (value == std::messages<char>::id._get_id() ||
        value == std::messages<wchar_t>::id._get_id())
        return locale::messages;
    return locale::none;
}

/* ═══════════════════════════════════════════════════════════════
 * 便利関数
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT>
bool isspace(CharT c, const locale& loc) {
    return use_facet<ctype<CharT>>(loc).is(ctype_base::space, c);
}

template<typename CharT>
bool isprint(CharT c, const locale& loc) {
    return use_facet<ctype<CharT>>(loc).is(ctype_base::print, c);
}

template<typename CharT>
bool iscntrl(CharT c, const locale& loc) {
    return use_facet<ctype<CharT>>(loc).is(ctype_base::cntrl, c);
}

template<typename CharT>
bool isupper(CharT c, const locale& loc) {
    return use_facet<ctype<CharT>>(loc).is(ctype_base::upper, c);
}

template<typename CharT>
bool islower(CharT c, const locale& loc) {
    return use_facet<ctype<CharT>>(loc).is(ctype_base::lower, c);
}

template<typename CharT>
bool isalpha(CharT c, const locale& loc) {
    return use_facet<ctype<CharT>>(loc).is(ctype_base::alpha, c);
}

template<typename CharT>
bool isdigit(CharT c, const locale& loc) {
    return use_facet<ctype<CharT>>(loc).is(ctype_base::digit, c);
}

template<typename CharT>
bool ispunct(CharT c, const locale& loc) {
    return use_facet<ctype<CharT>>(loc).is(ctype_base::punct, c);
}

template<typename CharT>
bool isxdigit(CharT c, const locale& loc) {
    return use_facet<ctype<CharT>>(loc).is(ctype_base::xdigit, c);
}

template<typename CharT>
bool isalnum(CharT c, const locale& loc) {
    return use_facet<ctype<CharT>>(loc).is(ctype_base::alnum, c);
}

template<typename CharT>
bool isgraph(CharT c, const locale& loc) {
    return use_facet<ctype<CharT>>(loc).is(ctype_base::graph, c);
}

template<typename CharT>
bool isblank(CharT c, const locale& loc) {
    return use_facet<ctype<CharT>>(loc).is(ctype_base::blank, c);
}

template<typename CharT>
CharT toupper(CharT c, const locale& loc) {
    return use_facet<ctype<CharT>>(loc).toupper(c);
}

template<typename CharT>
CharT tolower(CharT c, const locale& loc) {
    return use_facet<ctype<CharT>>(loc).tolower(c);
}

} /* namespace std */

#undef RIN_LOCALE_SPECIALIZATION_DEFINITION
#undef RINCXX_TM_HAS_GNU_EXTENSIONS
#undef RINCXX_TM_TIMEZONE_WEAK

#endif /* RINCXX_LOCALE_H */
