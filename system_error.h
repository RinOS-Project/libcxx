/*
 * RinOS C++ <system_error> ✿
 * システムエラー処理 (C++11)
 */

#ifndef RINCXX_SYSTEM_ERROR_H
#define RINCXX_SYSTEM_ERROR_H

#include "rincxx.h"
#include "cerrno.h"
#include "cstddef.h"
#define RINCXX_SYSTEM_ERROR_IN_PROGRESS 1
#include "string.h"
#include "stdexcept.h"
#undef RINCXX_SYSTEM_ERROR_IN_PROGRESS
#include "type_traits.h"
#include "iosfwd.h"
#include "pointer_order.h"
#if defined(__cplusplus) && __cplusplus >= 202002L
#include "compare.h"
#endif

namespace std {

class error_category;
class error_code;
class error_condition;

template<typename T>
struct is_error_code_enum : false_type {};

template<typename T>
struct is_error_condition_enum : false_type {};

/* The iostream error domain is specified by <ios>, which includes this
 * header. Keeping the enum and its category next to the generic
 * error-domain machinery gives error_code the normal ADL-free conversion
 * path while avoiding a dependency from <system_error> back to <ios>. */
enum class io_errc {
    stream = 1
};

template<>
struct is_error_code_enum<io_errc> : true_type {};

const error_category& iostream_category() noexcept;
error_code make_error_code(io_errc e) noexcept;

#if __cplusplus >= 201703L
template<typename T>
constexpr bool is_error_code_enum_v = is_error_code_enum<T>::value;

template<typename T>
constexpr bool is_error_condition_enum_v =
    is_error_condition_enum<T>::value;
#endif

template<typename T>
struct hash;

/* ═══════════════════════════════════════════════════════════════
 * errc - 標準エラー条件
 * ═══════════════════════════════════════════════════════════════*/

enum class errc {
    address_family_not_supported = EAFNOSUPPORT,
    address_in_use = EADDRINUSE,
    address_not_available = EADDRNOTAVAIL,
    already_connected = EISCONN,
    argument_list_too_long = E2BIG,
    argument_out_of_domain = EDOM,
    bad_address = EFAULT,
    bad_file_descriptor = EBADF,
    bad_message = EBADMSG,
    broken_pipe = EPIPE,
    connection_aborted = ECONNABORTED,
    connection_already_in_progress = EALREADY,
    connection_refused = ECONNREFUSED,
    connection_reset = ECONNRESET,
    cross_device_link = EXDEV,
    destination_address_required = EDESTADDRREQ,
    device_or_resource_busy = EBUSY,
    directory_not_empty = ENOTEMPTY,
    executable_format_error = ENOEXEC,
    file_exists = EEXIST,
    file_too_large = EFBIG,
    filename_too_long = ENAMETOOLONG,
    function_not_supported = ENOSYS,
    host_unreachable = EHOSTUNREACH,
    identifier_removed = EIDRM,
    illegal_byte_sequence = EILSEQ,
    inappropriate_io_control_operation = ENOTTY,
    interrupted = EINTR,
    invalid_argument = EINVAL,
    invalid_seek = ESPIPE,
    io_error = EIO,
    is_a_directory = EISDIR,
    message_size = EMSGSIZE,
    network_down = ENETDOWN,
    network_reset = ENETRESET,
    network_unreachable = ENETUNREACH,
    no_buffer_space = ENOBUFS,
    no_child_process = ECHILD,
    no_link = ENOLINK,
    no_lock_available = ENOLCK,
    no_message_available = ENODATA,
    no_message = ENOMSG,
    no_protocol_option = ENOPROTOOPT,
    no_space_on_device = ENOSPC,
    no_stream_resources = ENOSR,
    no_such_device = ENODEV,
    no_such_device_or_address = ENXIO,
    no_such_file_or_directory = ENOENT,
    no_such_process = ESRCH,
    not_a_directory = ENOTDIR,
    not_a_socket = ENOTSOCK,
    not_a_stream = ENOSTR,
    not_connected = ENOTCONN,
    not_enough_memory = ENOMEM,
    not_supported = ENOTSUP,
    operation_canceled = ECANCELED,
    operation_in_progress = EINPROGRESS,
    operation_not_permitted = EPERM,
    operation_not_supported = EOPNOTSUPP,
    operation_would_block = EWOULDBLOCK,
    owner_dead = EOWNERDEAD,
    permission_denied = EACCES,
    protocol_error = EPROTO,
    protocol_not_supported = EPROTONOSUPPORT,
    read_only_file_system = EROFS,
    resource_deadlock_would_occur = EDEADLK,
    resource_unavailable_try_again = EAGAIN,
    result_out_of_range = ERANGE,
    state_not_recoverable = ENOTRECOVERABLE,
    stream_timeout = ETIME,
    text_file_busy = ETXTBSY,
    timed_out = ETIMEDOUT,
    too_many_files_open = EMFILE,
    too_many_files_open_in_system = ENFILE,
    too_many_links = EMLINK,
    too_many_symbolic_link_levels = ELOOP,
    value_too_large = EOVERFLOW,
    wrong_protocol_type = EPROTOTYPE
};

template<>
struct is_error_condition_enum<errc> : true_type {};

error_condition make_error_condition(errc e) noexcept;

/* ═══════════════════════════════════════════════════════════════
 * error_category
 * ═══════════════════════════════════════════════════════════════*/

class error_category {
    static int compare_names(const char* left, const char* right) noexcept {
        if (!left) left = "";
        if (!right) right = "";
        while (*left != '\0' && *right != '\0' && *left == *right) {
            ++left;
            ++right;
        }
        const unsigned char left_byte =
            static_cast<unsigned char>(*left);
        const unsigned char right_byte =
            static_cast<unsigned char>(*right);
        if (left_byte < right_byte) return -1;
        if (left_byte > right_byte) return 1;
        return 0;
    }

public:
    constexpr error_category() noexcept = default;
    virtual ~error_category() = default;

    error_category(const error_category&) = delete;
    error_category& operator=(const error_category&) = delete;

    virtual const char* name() const noexcept = 0;
    virtual string message(int ev) const = 0;

    virtual error_condition default_error_condition(int code) const noexcept;
    virtual bool equivalent(int code,
                            const error_condition& condition) const noexcept;
    virtual bool equivalent(const error_code& code,
                            int condition) const noexcept;

    /* Standard categories need an identity that survives a separately
     * linked image.  User-defined categories retain the default zero and
     * continue to use object identity. */
    virtual unsigned int identity_key() const noexcept { return 0u; }

    /* A provider may reuse an identity key and diagnostic name for more than
     * one independent error domain.  Keep a second bounded discriminator so
     * such categories remain distinct without falling back to object address
     * identity across separately linked images. */
    virtual unsigned int identity_domain_key() const noexcept { return 0u; }

    bool operator==(const error_category& rhs) const noexcept {
        const unsigned int left_key = identity_key();
        const unsigned int right_key = rhs.identity_key();
        const unsigned int left_domain = identity_domain_key();
        const unsigned int right_domain = rhs.identity_domain_key();
        return this == &rhs ||
               (left_key != 0u && left_key == right_key &&
                left_domain == right_domain &&
                compare_names(name(), rhs.name()) == 0);
    }

    bool operator!=(const error_category& rhs) const noexcept {
        return !(*this == rhs);
    }

    bool operator<(const error_category& rhs) const noexcept {
        if (*this == rhs) return false;
        const unsigned int left_key = identity_key();
        const unsigned int right_key = rhs.identity_key();
        if (left_key != 0u && right_key != 0u) {
            if (left_key != right_key) return left_key < right_key;
            const unsigned int left_domain = identity_domain_key();
            const unsigned int right_domain = rhs.identity_domain_key();
            if (left_domain != right_domain) return left_domain < right_domain;
            return compare_names(name(), rhs.name()) < 0;
        }
        return detail::object_pointer_total_less(this, &rhs);
    }

#if __cplusplus >= 202002L
    strong_ordering operator<=>(const error_category& rhs) const noexcept {
        if (*this == rhs) return strong_ordering::equal;
        const unsigned int left_key = identity_key();
        const unsigned int right_key = rhs.identity_key();
        if (left_key != 0u && right_key != 0u) {
            if (left_key != right_key) {
                return left_key < right_key ? strong_ordering::less
                     : strong_ordering::greater;
            }
            const unsigned int left_domain = identity_domain_key();
            const unsigned int right_domain = rhs.identity_domain_key();
            if (left_domain != right_domain) {
                return left_domain < right_domain ? strong_ordering::less
                     : strong_ordering::greater;
            }
            const int names = compare_names(name(), rhs.name());
            return names < 0 ? strong_ordering::less
                 : names > 0 ? strong_ordering::greater
                             : strong_ordering::equal;
        }
        return detail::object_pointer_total_less(this, &rhs)
            ? strong_ordering::less
            : detail::object_pointer_total_less(&rhs, this)
                ? strong_ordering::greater
                : strong_ordering::equal;
    }
#endif
};

/* ═══════════════════════════════════════════════════════════════
 * 標準エラーカテゴリ
 * ═══════════════════════════════════════════════════════════════*/

class generic_category_impl : public error_category {
public:
    const char* name() const noexcept override { return "generic"; }

    unsigned int identity_key() const noexcept override { return 1u; }

    string message(int ev) const override;
};

class system_category_impl : public error_category {
public:
    const char* name() const noexcept override { return "system"; }

    unsigned int identity_key() const noexcept override { return 2u; }

    string message(int ev) const override;
    error_condition default_error_condition(int ev) const noexcept override;
};

class iostream_category_impl : public error_category {
public:
    const char* name() const noexcept override { return "iostream"; }

    unsigned int identity_key() const noexcept override { return 3u; }

    string message(int value) const override {
        return string(value == static_cast<int>(io_errc::stream)
                          ? "iostream error"
                          : "Unknown error");
    }
};

inline const error_category& generic_category() noexcept {
    static generic_category_impl instance;
    return instance;
}

inline const error_category& system_category() noexcept {
    static system_category_impl instance;
    return instance;
}

inline const error_category& iostream_category() noexcept {
    static iostream_category_impl instance;
    return instance;
}

/* ═══════════════════════════════════════════════════════════════
 * error_code
 * ═══════════════════════════════════════════════════════════════*/

class error_code {
    int val_;
    const error_category* cat_;

public:
    error_code() noexcept : val_(0), cat_(&system_category()) {}

    error_code(int val, const error_category& cat) noexcept
        : val_(val), cat_(&cat) {}

    template<typename ErrorCodeEnum,
             typename = enable_if_t<
                 is_error_code_enum<ErrorCodeEnum>::value>>
    error_code(ErrorCodeEnum e) noexcept
        : error_code(make_error_code(e)) {}

    template<typename ErrorCodeEnum,
             typename = enable_if_t<
                 is_error_code_enum<ErrorCodeEnum>::value>>
    error_code& operator=(ErrorCodeEnum e) noexcept {
        *this = make_error_code(e);
        return *this;
    }

    void assign(int val, const error_category& cat) noexcept {
        val_ = val;
        cat_ = &cat;
    }

    void clear() noexcept {
        val_ = 0;
        cat_ = &system_category();
    }

    int value() const noexcept { return val_; }
    const error_category& category() const noexcept { return *cat_; }
    error_condition default_error_condition() const noexcept;
    string message() const { return cat_->message(val_); }

    explicit operator bool() const noexcept { return val_ != 0; }

    void swap(error_code& other) noexcept {
        const int value = val_;
        const error_category* category = cat_;
        val_ = other.val_;
        cat_ = other.cat_;
        other.val_ = value;
        other.cat_ = category;
    }
};

inline void swap(error_code& lhs, error_code& rhs) noexcept {
    lhs.swap(rhs);
}

inline bool operator==(const error_code& lhs, const error_code& rhs) noexcept {
    return lhs.category() == rhs.category() && lhs.value() == rhs.value();
}

inline bool operator!=(const error_code& lhs, const error_code& rhs) noexcept {
    return !(lhs == rhs);
}

inline bool operator<(const error_code& lhs, const error_code& rhs) noexcept {
    return lhs.category() < rhs.category() ||
           (lhs.category() == rhs.category() && lhs.value() < rhs.value());
}

/* C++11 specifies the stream inserter in <system_error> itself.  Keep the
 * declaration independent of <ostream> so including this header does not
 * re-enter the <ios> -> <system_error> include cycle; the dependent body is
 * instantiated after the selected basic_ostream is complete. */
template<typename CharT, typename Traits>
basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& stream,
           const error_code& code) {
    stream << code.category().name();
    stream.put(static_cast<CharT>(':'));
    stream << code.value();
    return stream;
}

#if __cplusplus >= 202002L
inline strong_ordering operator<=>(const error_code& lhs,
                                   const error_code& rhs) noexcept {
    const strong_ordering categories = lhs.category() <=> rhs.category();
    if (categories != 0) return categories;
    return lhs.value() < rhs.value() ? strong_ordering::less
         : lhs.value() > rhs.value() ? strong_ordering::greater
                                     : strong_ordering::equal;
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * error_condition
 * ═══════════════════════════════════════════════════════════════*/

class error_condition {
    int val_;
    const error_category* cat_;

public:
    error_condition() noexcept : val_(0), cat_(&generic_category()) {}

    error_condition(int val, const error_category& cat) noexcept
        : val_(val), cat_(&cat) {}

    template<typename ErrorConditionEnum,
             typename = enable_if_t<
                 is_error_condition_enum<ErrorConditionEnum>::value>>
    error_condition(ErrorConditionEnum e) noexcept
        : error_condition(make_error_condition(e)) {}

    template<typename ErrorConditionEnum,
             typename = enable_if_t<
                 is_error_condition_enum<ErrorConditionEnum>::value>>
    error_condition& operator=(ErrorConditionEnum e) noexcept {
        *this = make_error_condition(e);
        return *this;
    }

    void assign(int val, const error_category& cat) noexcept {
        val_ = val;
        cat_ = &cat;
    }

    void clear() noexcept {
        val_ = 0;
        cat_ = &generic_category();
    }

    int value() const noexcept { return val_; }
    const error_category& category() const noexcept { return *cat_; }
    string message() const { return cat_->message(val_); }

    explicit operator bool() const noexcept { return val_ != 0; }

    void swap(error_condition& other) noexcept {
        const int value = val_;
        const error_category* category = cat_;
        val_ = other.val_;
        cat_ = other.cat_;
        other.val_ = value;
        other.cat_ = category;
    }
};

inline void swap(error_condition& lhs, error_condition& rhs) noexcept {
    lhs.swap(rhs);
}

inline bool operator==(const error_condition& lhs, const error_condition& rhs) noexcept {
    return lhs.category() == rhs.category() && lhs.value() == rhs.value();
}

inline bool operator!=(const error_condition& lhs, const error_condition& rhs) noexcept {
    return !(lhs == rhs);
}

inline bool operator<(const error_condition& lhs, const error_condition& rhs) noexcept {
    return lhs.category() < rhs.category() ||
           (lhs.category() == rhs.category() && lhs.value() < rhs.value());
}

#if __cplusplus >= 202002L
inline strong_ordering operator<=>(const error_condition& lhs,
                                   const error_condition& rhs) noexcept {
    const strong_ordering categories = lhs.category() <=> rhs.category();
    if (categories != 0) return categories;
    return lhs.value() < rhs.value() ? strong_ordering::less
         : lhs.value() > rhs.value() ? strong_ordering::greater
                                     : strong_ordering::equal;
}
#endif

/* error_code と error_condition の比較 */
inline bool operator==(const error_code& code, const error_condition& condition) noexcept {
    return code.category().equivalent(code.value(), condition) ||
           condition.category().equivalent(code, condition.value());
}

inline bool operator==(const error_condition& condition, const error_code& code) noexcept {
    return code == condition;
}

inline bool operator!=(const error_code& code, const error_condition& condition) noexcept {
    return !(code == condition);
}

inline bool operator!=(const error_condition& condition, const error_code& code) noexcept {
    return !(code == condition);
}

/* error_category メンバ関数の実装 */
inline error_condition
error_category::default_error_condition(int code) const noexcept {
    return error_condition(code, *this);
}

inline bool error_category::equivalent(int code, const error_condition& condition) const noexcept {
    return default_error_condition(code) == condition;
}

inline bool error_category::equivalent(const error_code& code, int condition) const noexcept {
    return *this == code.category() && code.value() == condition;
}

inline error_condition error_code::default_error_condition() const noexcept {
    return cat_->default_error_condition(val_);
}

namespace detail {

inline const char* generic_error_message(int ev) noexcept {
    if (ev == 0) return "Success";
    if (ev == static_cast<int>(errc::address_family_not_supported))
        return "Address family not supported";
    if (ev == static_cast<int>(errc::address_in_use))
        return "Address already in use";
    if (ev == static_cast<int>(errc::address_not_available))
        return "Address not available";
    if (ev == static_cast<int>(errc::already_connected))
        return "Already connected";
    if (ev == static_cast<int>(errc::argument_list_too_long))
        return "Argument list too long";
    if (ev == static_cast<int>(errc::argument_out_of_domain))
        return "Argument out of domain";
    if (ev == static_cast<int>(errc::bad_address)) return "Bad address";
    if (ev == static_cast<int>(errc::bad_file_descriptor))
        return "Bad file descriptor";
    if (ev == static_cast<int>(errc::bad_message)) return "Bad message";
    if (ev == static_cast<int>(errc::broken_pipe)) return "Broken pipe";
    if (ev == static_cast<int>(errc::connection_aborted))
        return "Connection aborted";
    if (ev == static_cast<int>(errc::connection_already_in_progress))
        return "Connection already in progress";
    if (ev == static_cast<int>(errc::connection_refused))
        return "Connection refused";
    if (ev == static_cast<int>(errc::connection_reset))
        return "Connection reset";
    if (ev == static_cast<int>(errc::cross_device_link))
        return "Cross-device link";
    if (ev == static_cast<int>(errc::destination_address_required))
        return "Destination address required";
    if (ev == static_cast<int>(errc::device_or_resource_busy))
        return "Device or resource busy";
    if (ev == static_cast<int>(errc::directory_not_empty))
        return "Directory not empty";
    if (ev == static_cast<int>(errc::executable_format_error))
        return "Executable format error";
    if (ev == static_cast<int>(errc::file_exists)) return "File exists";
    if (ev == static_cast<int>(errc::file_too_large)) return "File too large";
    if (ev == static_cast<int>(errc::filename_too_long))
        return "Filename too long";
    if (ev == static_cast<int>(errc::function_not_supported))
        return "Function not supported";
    if (ev == static_cast<int>(errc::host_unreachable))
        return "Host unreachable";
    if (ev == static_cast<int>(errc::identifier_removed))
        return "Identifier removed";
    if (ev == static_cast<int>(errc::illegal_byte_sequence))
        return "Illegal byte sequence";
    if (ev == static_cast<int>(errc::inappropriate_io_control_operation))
        return "Inappropriate I/O control operation";
    if (ev == static_cast<int>(errc::interrupted)) return "Interrupted";
    if (ev == static_cast<int>(errc::invalid_argument))
        return "Invalid argument";
    if (ev == static_cast<int>(errc::invalid_seek)) return "Invalid seek";
    if (ev == static_cast<int>(errc::io_error)) return "I/O error";
    if (ev == static_cast<int>(errc::is_a_directory)) return "Is a directory";
    if (ev == static_cast<int>(errc::message_size)) return "Message too large";
    if (ev == static_cast<int>(errc::network_down)) return "Network is down";
    if (ev == static_cast<int>(errc::network_reset)) return "Network reset";
    if (ev == static_cast<int>(errc::network_unreachable))
        return "Network unreachable";
    if (ev == static_cast<int>(errc::no_buffer_space))
        return "No buffer space";
    if (ev == static_cast<int>(errc::no_child_process))
        return "No child process";
    if (ev == static_cast<int>(errc::no_link)) return "No link";
    if (ev == static_cast<int>(errc::no_lock_available))
        return "No lock available";
    if (ev == static_cast<int>(errc::no_message_available))
        return "No message available";
    if (ev == static_cast<int>(errc::no_message)) return "No message";
    if (ev == static_cast<int>(errc::no_protocol_option))
        return "No protocol option";
    if (ev == static_cast<int>(errc::no_space_on_device))
        return "No space on device";
    if (ev == static_cast<int>(errc::no_stream_resources))
        return "No stream resources";
    if (ev == static_cast<int>(errc::no_such_device))
        return "No such device";
    if (ev == static_cast<int>(errc::no_such_device_or_address))
        return "No such device or address";
    if (ev == static_cast<int>(errc::no_such_file_or_directory))
        return "No such file or directory";
    if (ev == static_cast<int>(errc::no_such_process))
        return "No such process";
    if (ev == static_cast<int>(errc::not_a_directory))
        return "Not a directory";
    if (ev == static_cast<int>(errc::not_a_socket)) return "Not a socket";
    if (ev == static_cast<int>(errc::not_a_stream)) return "Not a stream";
    if (ev == static_cast<int>(errc::not_connected)) return "Not connected";
    if (ev == static_cast<int>(errc::not_enough_memory))
        return "Not enough memory";
    if (ev == static_cast<int>(errc::not_supported)) return "Not supported";
    if (ev == static_cast<int>(errc::operation_canceled))
        return "Operation canceled";
    if (ev == static_cast<int>(errc::operation_in_progress))
        return "Operation in progress";
    if (ev == static_cast<int>(errc::operation_not_permitted))
        return "Operation not permitted";
    if (ev == static_cast<int>(errc::operation_not_supported))
        return "Operation not supported";
    if (ev == static_cast<int>(errc::operation_would_block))
        return "Operation would block";
    if (ev == static_cast<int>(errc::owner_dead)) return "Owner dead";
    if (ev == static_cast<int>(errc::permission_denied))
        return "Permission denied";
    if (ev == static_cast<int>(errc::protocol_error)) return "Protocol error";
    if (ev == static_cast<int>(errc::protocol_not_supported))
        return "Protocol not supported";
    if (ev == static_cast<int>(errc::read_only_file_system))
        return "Read-only file system";
    if (ev == static_cast<int>(errc::resource_deadlock_would_occur))
        return "Resource deadlock would occur";
    if (ev == static_cast<int>(errc::resource_unavailable_try_again))
        return "Resource unavailable, try again";
    if (ev == static_cast<int>(errc::result_out_of_range))
        return "Result out of range";
    if (ev == static_cast<int>(errc::state_not_recoverable))
        return "State not recoverable";
    if (ev == static_cast<int>(errc::stream_timeout)) return "Stream timeout";
    if (ev == static_cast<int>(errc::text_file_busy)) return "Text file busy";
    if (ev == static_cast<int>(errc::timed_out)) return "Timed out";
    if (ev == static_cast<int>(errc::too_many_files_open))
        return "Too many files open";
    if (ev == static_cast<int>(errc::too_many_files_open_in_system))
        return "Too many files open in system";
    if (ev == static_cast<int>(errc::too_many_links)) return "Too many links";
    if (ev == static_cast<int>(errc::too_many_symbolic_link_levels))
        return "Too many symbolic link levels";
    if (ev == static_cast<int>(errc::value_too_large))
        return "Value too large";
    if (ev == static_cast<int>(errc::wrong_protocol_type))
        return "Wrong protocol type";
    return nullptr;
}

} /* namespace detail */

inline string generic_category_impl::message(int ev) const {
    const char* text = detail::generic_error_message(ev);
    return string(text ? text : "Unknown error");
}

inline string system_category_impl::message(int ev) const {
    const char* text = detail::generic_error_message(ev);
    return string(text ? text : "Unknown system error");
}

inline error_condition
system_category_impl::default_error_condition(int ev) const noexcept {
    if (detail::generic_error_message(ev) != nullptr)
        return error_condition(ev, generic_category());
    return error_condition(ev, *this);
}

/* Hashing must follow error_category::operator==.  Standard categories use a
 * stable identity key so separately linked images compare equal; hashing their
 * object address would violate the unordered-container requirement that equal
 * keys have equal hashes.  User categories retain address identity when they
 * do not opt into a shared key. */
namespace detail {

inline size_t error_category_hash_key(const error_category& category) noexcept {
    const unsigned int identity = category.identity_key();
    if (identity == 0u) {
        return static_cast<size_t>(
            detail::object_pointer_hash(&category));
    }

    /* identity_key is the cross-image anchor, while name() disambiguates a
     * reused/colliding key.  Keep the hash allocation-free and bounded by the
     * category's NUL-terminated diagnostic name. */
    size_t hash = static_cast<size_t>(2166136261u) ^
                  static_cast<size_t>(identity);
    const unsigned int domain = category.identity_domain_key();
    hash ^= static_cast<size_t>(domain) + static_cast<size_t>(0x9e3779b9u) +
            (hash << 6u) + (hash >> 2u);
    const char* text = category.name();
    if (!text) return hash;
    for (; *text != '\0'; ++text) {
        hash ^= static_cast<unsigned char>(*text);
        hash *= static_cast<size_t>(16777619u);
    }
    return hash;
}

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * system_error 例外
 * ═══════════════════════════════════════════════════════════════*/

class system_error : public runtime_error {
    error_code code_;

public:
    system_error(error_code ec, const string& what_arg)
        : runtime_error(what_arg + ": " + ec.message()), code_(ec) {}

    system_error(error_code ec, const char* what_arg)
        : runtime_error(string(what_arg) + ": " + ec.message()), code_(ec) {}

    system_error(error_code ec)
        : runtime_error(ec.message()), code_(ec) {}

    system_error(int ev, const error_category& ecat, const string& what_arg)
        : runtime_error(what_arg + ": " + ecat.message(ev)), code_(ev, ecat) {}

    system_error(int ev, const error_category& ecat, const char* what_arg)
        : runtime_error(string(what_arg) + ": " + ecat.message(ev)), code_(ev, ecat) {}

    system_error(int ev, const error_category& ecat)
        : runtime_error(ecat.message(ev)), code_(ev, ecat) {}

    const error_code& code() const noexcept { return code_; }
};

/* ═══════════════════════════════════════════════════════════════
 * make_error_code / make_error_condition
 * ═══════════════════════════════════════════════════════════════*/

inline error_code make_error_code(errc e) noexcept {
    return error_code(static_cast<int>(e), generic_category());
}

inline error_code make_error_code(io_errc e) noexcept {
    return error_code(static_cast<int>(e), iostream_category());
}

inline error_condition make_error_condition(errc e) noexcept {
    return error_condition(static_cast<int>(e), generic_category());
}

template<>
struct hash<error_code> {
    size_t operator()(const error_code& value) const noexcept {
        const size_t category = detail::error_category_hash_key(
            value.category());
        const size_t code = static_cast<size_t>(
            static_cast<unsigned int>(value.value()));
        return category ^ (code + static_cast<size_t>(0x9e3779b9u) +
                           (category << 6u) + (category >> 2u));
    }
};

template<>
struct hash<error_condition> {
    size_t operator()(const error_condition& value) const noexcept {
        const size_t category = detail::error_category_hash_key(
            value.category());
        const size_t code = static_cast<size_t>(
            static_cast<unsigned int>(value.value()));
        return category ^ (code + static_cast<size_t>(0x9e3779b9u) +
                           (category << 6u) + (category >> 2u));
    }
};

} /* namespace std */

#endif /* RINCXX_SYSTEM_ERROR_H */
