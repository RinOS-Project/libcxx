/*
 * RinOS C++ <net> ✿
 * ネットワーキングライブラリ (Experimental TS風)
 */

#ifndef RINCXX_NET_H
#define RINCXX_NET_H

#include "rincxx.h"

/* This experimental networking surface requires the C++17 dependencies it
 * exposes; old modes must not parse a partial declaration set. */
#if defined(__cplusplus) && __cplusplus >= 201703L

#if __cplusplus >= 202002L
#include "compare.h"
#endif
#include "array.h"
#include "string.h"
#include "system_error.h"
#include "memory.h"
#include "chrono.h"
#include "cerrno.h"
#include "../../../src/shared/ipv6_text.h"
#include "../../../src/shared/dns_abi.h"
#include "../../../src/shared/netif_addrconfig_policy.h"

/* C socket API */
extern "C" {
    /* socket functions */
    int socket(int domain, int type, int protocol);
    int bind(int sockfd, const struct sockaddr* addr, unsigned int addrlen);
    int listen(int sockfd, int backlog);
    int accept(int sockfd, struct sockaddr* addr, unsigned int* addrlen);
    int connect(int sockfd, const struct sockaddr* addr, unsigned int addrlen);
    long send(int sockfd, const void* buf, unsigned long len, int flags);
    long recv(int sockfd, void* buf, unsigned long len, int flags);
    long sendto(int sockfd, const void* buf, unsigned long len, int flags,
                const struct sockaddr* dest_addr, unsigned int addrlen);
    long recvfrom(int sockfd, void* buf, unsigned long len, int flags,
                  struct sockaddr* src_addr, unsigned int* addrlen);
    int shutdown(int sockfd, int how);
    int setsockopt(int sockfd, int level, int optname, const void* optval, unsigned int optlen);
    int getsockopt(int sockfd, int level, int optname, void* optval, unsigned int* optlen);
    int getsockname(int sockfd, struct sockaddr* addr, unsigned int* addrlen);
    int getpeername(int sockfd, struct sockaddr* addr, unsigned int* addrlen);
    int rin_close(int fd);

    /* address conversion */
    unsigned int inet_addr(const char* cp);
    char* inet_ntoa(struct in_addr in);
    int inet_pton(int af, const char* src, void* dst);
    const char* inet_ntop(int af, const void* src, char* dst, unsigned int size);

    /* byte order */
    unsigned short htons(unsigned short hostshort);
    unsigned short ntohs(unsigned short netshort);
    unsigned int htonl(unsigned int hostlong);
    unsigned int ntohl(unsigned int netlong);

    /* name resolution */
    struct hostent* gethostbyname(const char* name);
    int getaddrinfo(const char* node, const char* service,
                    const struct addrinfo* hints, struct addrinfo** res);
    void freeaddrinfo(struct addrinfo* res);
    const char* gai_strerror(int errcode);

    /* nonblocking resolved owner */
    int rin_dns_resolve_begin(const char* hostname);
    int rin_dns_resolve_poll(unsigned int handle, RinDnsResolveResult* out);
    int rin_dns_resolve_cancel(unsigned int handle);
    int rin_dns_getaddrinfo_begin(const char* hostname, const char* service,
                                  int family, int socktype, int protocol,
                                  unsigned int flags);
    int rin_dns_getaddrinfo_poll(unsigned int handle,
                                 RinDnsGetAddrInfoResult* out);
    int rin_dns_getaddrinfo_batch_begin(const char* hostname,
                                        const char* service, int family,
                                        int socktype, int protocol,
                                        unsigned int flags);
    int rin_dns_getaddrinfo_batch_poll(
        unsigned int handle, RinDnsGetAddrInfoBatchResult* out);
    int rin_net_get_primary_info(RinNetPrimaryInfo* out);

    /* interface name/index conversion */
    unsigned int if_nametoindex(const char* ifname);
    char* if_indextoname(unsigned int ifindex, char* ifname);
}

/* 定数 */
#ifndef AF_UNSPEC
#define AF_UNSPEC    0
#endif
#ifndef IF_NAMESIZE
#define IF_NAMESIZE 16
#endif
#ifndef AF_INET
#define AF_INET      2
#define AF_INET6     10
#define SOCK_STREAM  1
#define SOCK_DGRAM   2
#define IPPROTO_TCP  6
#define IPPROTO_UDP  17
#define SOL_SOCKET   1
#define SO_REUSEADDR 2
#define SO_KEEPALIVE 9
#define TCP_NODELAY  1
#define SHUT_RD      0
#define SHUT_WR      1
#define SHUT_RDWR    2
#define INADDR_ANY   0
#define INADDR_LOOPBACK 0x7F000001
#endif
#ifndef AI_PASSIVE
#define AI_PASSIVE     0x0001
#define AI_CANONNAME   0x0002
#define AI_NUMERICHOST 0x0004
#define AI_NUMERICSERV 0x0008
#define AI_V4MAPPED    0x0010
#define AI_ALL         0x0020
#define AI_ADDRCONFIG  0x0040
#endif

/* アドレス構造体 (extern Cブロック外) */
struct sockaddr {
    unsigned short sa_family;
    char sa_data[14];
};

struct in_addr {
    unsigned int s_addr;
};

struct sockaddr_in {
    unsigned short sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    unsigned char sin_zero[8];
};

struct in6_addr {
    unsigned char s6_addr[16];
};

struct sockaddr_in6 {
    unsigned short sin6_family;
    unsigned short sin6_port;
    unsigned int sin6_flowinfo;
    struct in6_addr sin6_addr;
    unsigned int sin6_scope_id;
};

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    unsigned int ai_addrlen;
    struct sockaddr* ai_addr;
    char* ai_canonname;
    struct addrinfo* ai_next;
};

struct hostent {
    char* h_name;
    char** h_aliases;
    int h_addrtype;
    int h_length;
    char** h_addr_list;
};

namespace std {
namespace net {

/* ═══════════════════════════════════════════════════════════════
 * ip::address_v4 - IPv4アドレス
 * ═══════════════════════════════════════════════════════════════*/

namespace ip {

class address_v4 {
public:
    using bytes_type = array<unsigned char, 4>;
    using uint_type = unsigned int;

    /* コンストラクタ */
    constexpr address_v4() noexcept : addr_(0), valid_(true) {}

    constexpr explicit address_v4(uint_type val) noexcept
        : addr_(val), valid_(true) {}

    constexpr explicit address_v4(const bytes_type& bytes) noexcept
        : addr_(0), valid_(true) {
        addr_ = (static_cast<uint_type>(bytes[0]) << 24) |
                (static_cast<uint_type>(bytes[1]) << 16) |
                (static_cast<uint_type>(bytes[2]) << 8) |
                static_cast<uint_type>(bytes[3]);
    }

    /* アクセサ */
    constexpr bool is_valid() const noexcept { return valid_; }

    constexpr bool is_loopback() const noexcept {
        return valid_ && (addr_ & 0xFF000000u) == 0x7F000000u;
    }

    constexpr bool is_unspecified() const noexcept {
        return valid_ && addr_ == 0;
    }

    constexpr bool is_multicast() const noexcept {
        return valid_ && (addr_ & 0xF0000000u) == 0xE0000000u;
    }

    constexpr uint_type to_uint() const noexcept {
        return addr_;
    }

    constexpr bytes_type to_bytes() const noexcept {
        return bytes_type{{
            static_cast<unsigned char>((addr_ >> 24) & 0xFFu),
            static_cast<unsigned char>((addr_ >> 16) & 0xFFu),
            static_cast<unsigned char>((addr_ >> 8) & 0xFFu),
            static_cast<unsigned char>(addr_ & 0xFFu)
        }};
    }

    string to_string(error_code& ec) const {
        char buf[16];
        struct in_addr in;
        if (!valid_) {
            ec = make_error_code(errc::invalid_argument);
            return string();
        }
        in.s_addr = htonl(addr_);
        if (!inet_ntop(AF_INET, &in, buf, sizeof(buf))) {
            ec = make_error_code(errc::bad_address);
            return string();
        }
        ec.clear();
        return string(buf);
    }

    string to_string() const {
        error_code ec;
        return to_string(ec);
    }

    /* 比較 */
    friend constexpr bool operator==(const address_v4& a, const address_v4& b) noexcept {
        return a.valid_ == b.valid_ && (!a.valid_ || a.addr_ == b.addr_);
    }

    friend constexpr bool operator!=(const address_v4& a, const address_v4& b) noexcept {
        return !(a == b);
    }

    friend constexpr bool operator<(const address_v4& a, const address_v4& b) noexcept {
        return a.valid_ != b.valid_ ? !a.valid_ :
               (a.valid_ && a.addr_ < b.addr_);
    }

#if __cplusplus >= 202002L
    friend constexpr strong_ordering operator<=>(const address_v4& a,
                                                 const address_v4& b) noexcept {
        if (a.valid_ != b.valid_)
            return a.valid_ ? strong_ordering::greater : strong_ordering::less;
        if (!a.valid_) return strong_ordering::equal;
        if (a.addr_ < b.addr_) return strong_ordering::less;
        if (b.addr_ < a.addr_) return strong_ordering::greater;
        return strong_ordering::equal;
    }
#endif

    /* 特殊アドレス */
    static constexpr address_v4 any() noexcept { return address_v4(); }
    static constexpr address_v4 loopback() noexcept { return address_v4(0x7F000001); }
    static constexpr address_v4 broadcast() noexcept { return address_v4(0xFFFFFFFF); }
    static constexpr address_v4 invalid() noexcept {
        return address_v4(invalid_tag{});
    }

    /* 内部用 */
    unsigned int native() const noexcept { return valid_ ? htonl(addr_) : 0u; }

private:
    struct invalid_tag {};
    constexpr explicit address_v4(invalid_tag) noexcept
        : addr_(0), valid_(false) {}

    unsigned int addr_;
    bool valid_;
};

/* make_address_v4 */
inline address_v4 make_address_v4(const char* str, error_code& ec) noexcept {
    struct in_addr in;
    if (!str || inet_pton(AF_INET, str, &in) != 1) {
        ec = make_error_code(errc::invalid_argument);
        return address_v4::invalid();
    }
    ec.clear();
    return address_v4(ntohl(in.s_addr));
}

inline address_v4 make_address_v4(const char* str) noexcept {
    error_code ec;
    return make_address_v4(str, ec);
}

inline address_v4 make_address_v4(const string& str, error_code& ec) noexcept {
    return make_address_v4(str.c_str(), ec);
}

inline address_v4 make_address_v4(const string& str) noexcept {
    return make_address_v4(str.c_str());
}

/* ═══════════════════════════════════════════════════════════════
 * ip::address_v6 - IPv6アドレス
 * ═══════════════════════════════════════════════════════════════*/

class address_v6 {
public:
    using bytes_type = array<unsigned char, 16>;
    using scope_id_type = unsigned int;

    constexpr address_v6() noexcept
        : bytes_{{}}, scope_id_(0), valid_(true), named_scope_(false) {}

    constexpr explicit address_v6(const bytes_type& bytes,
                                  scope_id_type scope_id = 0) noexcept
        : bytes_(bytes), scope_id_(scope_id), valid_(true),
          named_scope_(false) {}

    constexpr bool is_valid() const noexcept { return valid_; }

    constexpr bool is_loopback() const noexcept {
        if (!valid_ || bytes_[15] != 1u) return false;
        for (size_t index = 0; index < 15; ++index) {
            if (bytes_[index] != 0u) return false;
        }
        return true;
    }

    constexpr bool is_unspecified() const noexcept {
        if (!valid_) return false;
        for (size_t index = 0; index < bytes_.size(); ++index) {
            if (bytes_[index] != 0u) return false;
        }
        return true;
    }

    constexpr bool is_multicast() const noexcept {
        return valid_ && bytes_[0] == 0xFFu;
    }

    constexpr bytes_type to_bytes() const noexcept { return bytes_; }
    constexpr scope_id_type scope_id() const noexcept { return scope_id_; }
    constexpr void scope_id(scope_id_type value) noexcept {
        scope_id_ = value;
        named_scope_ = false;
    }

    string to_string(error_code& ec) const {
        char buf[62];
        if (!valid_) {
            ec = make_error_code(errc::invalid_argument);
            return string();
        }
        if (rin_ipv6_text_format(bytes_.data(), buf, 46u) != 0) {
            ec = make_error_code(errc::bad_address);
            return string();
        }
        if (scope_id_ != 0u) {
            unsigned length = 0;
            while (buf[length] != '\0') ++length;
            buf[length++] = '%';
            if (named_scope_) {
                char scope_name[IF_NAMESIZE];
                if (!if_indextoname(scope_id_, scope_name)) {
                    ec.assign(errno != 0 ? errno : static_cast<int>(errc::no_such_device),
                              generic_category());
                    return string();
                }
                unsigned name_length = 0;
                while (name_length < IF_NAMESIZE &&
                       scope_name[name_length] != '\0')
                    ++name_length;
                if (name_length == 0u || name_length == IF_NAMESIZE ||
                    length + name_length >= sizeof(buf)) {
                    ec = make_error_code(errc::protocol_error);
                    return string();
                }
                for (unsigned index = 0; index < name_length; ++index)
                    buf[length++] = scope_name[index];
            } else {
                char reversed[10];
                unsigned count = 0;
                scope_id_type remaining = scope_id_;
                do {
                    reversed[count++] = static_cast<char>('0' + remaining % 10u);
                    remaining /= 10u;
                } while (remaining != 0u);
                while (count != 0u) buf[length++] = reversed[--count];
            }
            buf[length] = '\0';
        }
        ec.clear();
        return string(buf);
    }

    string to_string() const {
        error_code ec;
        return to_string(ec);
    }

    friend constexpr bool operator==(const address_v6& a,
                                     const address_v6& b) noexcept {
        if (a.valid_ != b.valid_) return false;
        if (!a.valid_) return true;
        for (size_t index = 0; index < a.bytes_.size(); ++index) {
            if (a.bytes_[index] != b.bytes_[index]) return false;
        }
        return a.scope_id_ == b.scope_id_;
    }

    friend constexpr bool operator!=(const address_v6& a,
                                     const address_v6& b) noexcept {
        return !(a == b);
    }

    friend constexpr bool operator<(const address_v6& a,
                                    const address_v6& b) noexcept {
        if (a.valid_ != b.valid_) return !a.valid_;
        if (!a.valid_) return false;
        for (size_t index = 0; index < a.bytes_.size(); ++index) {
            if (a.bytes_[index] < b.bytes_[index]) return true;
            if (b.bytes_[index] < a.bytes_[index]) return false;
        }
        return a.scope_id_ < b.scope_id_;
    }

#if __cplusplus >= 202002L
    friend constexpr strong_ordering operator<=>(const address_v6& a,
                                                 const address_v6& b) noexcept {
        if (a.valid_ != b.valid_)
            return a.valid_ ? strong_ordering::greater : strong_ordering::less;
        if (!a.valid_) return strong_ordering::equal;
        for (size_t index = 0; index < a.bytes_.size(); ++index) {
            if (a.bytes_[index] < b.bytes_[index]) return strong_ordering::less;
            if (b.bytes_[index] < a.bytes_[index]) return strong_ordering::greater;
        }
        if (a.scope_id_ < b.scope_id_) return strong_ordering::less;
        if (b.scope_id_ < a.scope_id_) return strong_ordering::greater;
        return strong_ordering::equal;
    }
#endif

    static constexpr address_v6 any() noexcept { return address_v6(); }

    static constexpr address_v6 loopback() noexcept {
        bytes_type bytes{{}};
        bytes[15] = 1u;
        return address_v6(bytes);
    }

    static constexpr address_v6 invalid() noexcept {
        return address_v6(invalid_tag{});
    }

private:
    struct invalid_tag {};
    constexpr explicit address_v6(invalid_tag) noexcept
        : bytes_{{}}, scope_id_(0), valid_(false), named_scope_(false) {}

    constexpr address_v6(const bytes_type& bytes, scope_id_type scope_id,
                         bool named_scope) noexcept
        : bytes_(bytes), scope_id_(scope_id), valid_(true),
          named_scope_(named_scope) {}

    friend address_v6 make_address_v6(const char*, error_code&) noexcept;

    bytes_type bytes_;
    scope_id_type scope_id_;
    bool valid_;
    bool named_scope_;
};

inline address_v6 make_address_v6(const char* str, error_code& ec) noexcept {
    address_v6::bytes_type bytes{{}};
    char address_text[46];
    unsigned length = 0;
    if (!str) {
        ec = make_error_code(errc::invalid_argument);
        return address_v6::invalid();
    }
    while (str[length] != '\0' && str[length] != '%') {
        if (length == sizeof(address_text) - 1u) {
            ec = make_error_code(errc::invalid_argument);
            return address_v6::invalid();
        }
        address_text[length] = str[length];
        ++length;
    }
    address_text[length] = '\0';
    address_v6::scope_id_type scope_id = 0;
    bool named_scope = false;
    if (str[length] == '%') {
        const char* scope = str + length + 1u;
        if (*scope == '\0') {
            ec = make_error_code(errc::invalid_argument);
            return address_v6::invalid();
        }
        const char* scan = scope;
        bool numeric = true;
        unsigned scope_length = 0u;
        while (*scan != '\0') {
            if (*scan < '0' || *scan > '9') numeric = false;
            if (scope_length == 1024u) {
                ec = make_error_code(errc::invalid_argument);
                return address_v6::invalid();
            }
            ++scope_length;
            ++scan;
        }
        if (!numeric && scope_length >= IF_NAMESIZE) {
            ec = make_error_code(errc::invalid_argument);
            return address_v6::invalid();
        }
        if (numeric) {
            while (*scope != '\0') {
                unsigned digit = static_cast<unsigned>(*scope++ - '0');
                if (scope_id > (0xffffffffu - digit) / 10u) {
                    ec = make_error_code(errc::result_out_of_range);
                    return address_v6::invalid();
                }
                scope_id = scope_id * 10u + digit;
            }
        } else {
            scope_id = if_nametoindex(scope);
            if (scope_id == 0u) {
                ec.assign(errno != 0 ? errno : static_cast<int>(errc::no_such_device),
                          generic_category());
                return address_v6::invalid();
            }
            named_scope = true;
        }
    }
    if (rin_ipv6_text_parse(address_text, bytes.data()) != 0) {
        ec = make_error_code(errc::invalid_argument);
        return address_v6::invalid();
    }
    ec.clear();
    return address_v6(bytes, scope_id, named_scope);
}

inline address_v6 make_address_v6(const char* str) noexcept {
    error_code ec;
    return make_address_v6(str, ec);
}

inline address_v6 make_address_v6(const string& str, error_code& ec) noexcept {
    return make_address_v6(str.c_str(), ec);
}

inline address_v6 make_address_v6(const string& str) noexcept {
    return make_address_v6(str.c_str());
}

/* ═══════════════════════════════════════════════════════════════
 * ip::address - 汎用IPアドレス
 * ═══════════════════════════════════════════════════════════════*/

class address {
public:
    constexpr address() noexcept : v4_(), v6_(), kind_(kind_v4) {}

    constexpr address(const address_v4& a) noexcept
        : v4_(a), v6_(), kind_(a.is_valid() ? kind_v4 : kind_invalid) {}

    constexpr address(const address_v6& a) noexcept
        : v4_(), v6_(a), kind_(a.is_valid() ? kind_v6 : kind_invalid) {}

    constexpr bool is_valid() const noexcept { return kind_ != kind_invalid; }
    constexpr bool is_v4() const noexcept { return kind_ == kind_v4; }
    constexpr bool is_v6() const noexcept { return kind_ == kind_v6; }

    constexpr bool is_loopback() const noexcept {
        return is_v4() ? v4_.is_loopback() :
               (is_v6() ? v6_.is_loopback() : false);
    }

    constexpr bool is_unspecified() const noexcept {
        return is_v4() ? v4_.is_unspecified() :
               (is_v6() ? v6_.is_unspecified() : false);
    }

    constexpr bool is_multicast() const noexcept {
        return is_v4() ? v4_.is_multicast() :
               (is_v6() ? v6_.is_multicast() : false);
    }

    constexpr address_v4 to_v4() const noexcept {
        return is_v4() ? v4_ : address_v4::invalid();
    }

    constexpr address_v6 to_v6() const noexcept {
        return is_v6() ? v6_ : address_v6::invalid();
    }

    string to_string(error_code& ec) const {
        if (is_v4()) return v4_.to_string(ec);
        if (is_v6()) return v6_.to_string(ec);
        ec = make_error_code(errc::invalid_argument);
        return string();
    }

    string to_string() const {
        error_code ec;
        return to_string(ec);
    }

    friend bool operator==(const address& a, const address& b) noexcept {
        if (a.kind_ != b.kind_) return false;
        if (a.is_v4()) return a.v4_ == b.v4_;
        if (a.is_v6()) return a.v6_ == b.v6_;
        return true;
    }

    friend bool operator!=(const address& a, const address& b) noexcept {
        return !(a == b);
    }

    friend bool operator<(const address& a, const address& b) noexcept {
        if (a.kind_ != b.kind_) return a.kind_ < b.kind_;
        if (a.is_v4()) return a.v4_ < b.v4_;
        if (a.is_v6()) return a.v6_ < b.v6_;
        return false;
    }

#if __cplusplus >= 202002L
    friend constexpr strong_ordering operator<=>(const address& a,
                                                 const address& b) noexcept {
        if (a.kind_ < b.kind_) return strong_ordering::less;
        if (b.kind_ < a.kind_) return strong_ordering::greater;
        if (a.is_v4()) return a.v4_ <=> b.v4_;
        if (a.is_v6()) return a.v6_ <=> b.v6_;
        return strong_ordering::equal;
    }
#endif

    static constexpr address invalid() noexcept {
        return address(invalid_tag{});
    }

private:
    enum kind : unsigned char {
        kind_invalid = 0,
        kind_v4 = 1,
        kind_v6 = 2
    };
    struct invalid_tag {};
    constexpr explicit address(invalid_tag) noexcept
        : v4_(), v6_(), kind_(kind_invalid) {}

    address_v4 v4_;
    address_v6 v6_;
    kind kind_;
};

inline address make_address(const char* str, error_code& ec) noexcept {
    struct in_addr in4;
    if (str && inet_pton(AF_INET, str, &in4) == 1) {
        ec.clear();
        return address(address_v4(ntohl(in4.s_addr)));
    }
    error_code v6_error;
    address_v6 v6 = make_address_v6(str, v6_error);
    if (v6.is_valid()) {
        ec.clear();
        return address(v6);
    }
    ec = v6_error;
    return address::invalid();
}

inline address make_address(const char* str) noexcept {
    error_code ec;
    return make_address(str, ec);
}

inline address make_address(const string& str, error_code& ec) noexcept {
    return make_address(str.c_str(), ec);
}

inline address make_address(const string& str) noexcept {
    return make_address(str.c_str());
}

/* ═══════════════════════════════════════════════════════════════
 * ip::tcp / ip::udp プロトコル
 * ═══════════════════════════════════════════════════════════════*/

template<typename Protocol>
class basic_endpoint;

class tcp {
public:
    using endpoint = basic_endpoint<tcp>;

    static tcp v4() noexcept { return tcp(AF_INET); }
    static tcp v6() noexcept { return tcp(AF_INET6); }

    int family() const noexcept { return family_; }
    int type() const noexcept { return SOCK_STREAM; }
    int protocol() const noexcept { return IPPROTO_TCP; }

    friend bool operator==(const tcp& a, const tcp& b) { return a.family_ == b.family_; }
    friend bool operator!=(const tcp& a, const tcp& b) { return a.family_ != b.family_; }

private:
    explicit tcp(int family) : family_(family) {}
    int family_;
};

class udp {
public:
    using endpoint = basic_endpoint<udp>;

    static udp v4() noexcept { return udp(AF_INET); }
    static udp v6() noexcept { return udp(AF_INET6); }

    int family() const noexcept { return family_; }
    int type() const noexcept { return SOCK_DGRAM; }
    int protocol() const noexcept { return IPPROTO_UDP; }

    friend bool operator==(const udp& a, const udp& b) { return a.family_ == b.family_; }
    friend bool operator!=(const udp& a, const udp& b) { return a.family_ != b.family_; }

private:
    explicit udp(int family) : family_(family) {}
    int family_;
};

/* ═══════════════════════════════════════════════════════════════
 * basic_endpoint - エンドポイント (アドレス+ポート)
 * ═══════════════════════════════════════════════════════════════*/

template<typename Protocol>
class basic_endpoint {
public:
    using protocol_type = Protocol;

    basic_endpoint() noexcept : addr_(), port_(0) {}

    basic_endpoint(const protocol_type& proto, unsigned short port) noexcept
        : addr_(proto.family() == AF_INET6
                    ? ip::address(address_v6::any())
                    : ip::address(address_v4::any())),
          port_(port) {}

    basic_endpoint(const address& addr, unsigned short port) noexcept
        : addr_(addr), port_(port) {}

    protocol_type protocol() const noexcept {
        return addr_.is_v4() ? Protocol::v4() : Protocol::v6();
    }

    address address() const noexcept { return addr_; }
    void address(const ip::address& addr) noexcept { addr_ = addr; }

    unsigned short port() const noexcept { return port_; }
    void port(unsigned short p) noexcept { port_ = p; }

    /* ソケットアドレスへのtransactional変換 */
    bool to_sockaddr(struct sockaddr_in& sa, error_code& ec) const noexcept {
        if (!addr_.is_valid() || !addr_.is_v4()) {
            ec = make_error_code(errc::address_family_not_supported);
            return false;
        }
        struct sockaddr_in candidate{};
        candidate.sin_family = AF_INET;
        candidate.sin_port = htons(port_);
        candidate.sin_addr.s_addr = addr_.to_v4().native();
        sa = candidate;
        ec.clear();
        return true;
    }

    bool to_sockaddr(struct sockaddr_in& sa) const noexcept {
        error_code ec;
        return to_sockaddr(sa, ec);
    }

    bool to_sockaddr(struct sockaddr_in6& sa, error_code& ec) const noexcept {
        if (!addr_.is_valid() || !addr_.is_v6()) {
            ec = make_error_code(errc::address_family_not_supported);
            return false;
        }
        struct sockaddr_in6 candidate{};
        candidate.sin6_family = AF_INET6;
        candidate.sin6_port = htons(port_);
        address_v6 source = addr_.to_v6();
        address_v6::bytes_type bytes = source.to_bytes();
        for (unsigned index = 0; index < 16u; ++index) {
            candidate.sin6_addr.s6_addr[index] = bytes[index];
        }
        candidate.sin6_scope_id = source.scope_id();
        sa = candidate;
        ec.clear();
        return true;
    }

    bool to_sockaddr(struct sockaddr_in6& sa) const noexcept {
        error_code ec;
        return to_sockaddr(sa, ec);
    }

    bool from_sockaddr(const struct sockaddr_in& sa,
                       error_code& ec) noexcept {
        if (sa.sin_family != AF_INET) {
            ec = make_error_code(errc::address_family_not_supported);
            return false;
        }
        ip::address candidate(address_v4(ntohl(sa.sin_addr.s_addr)));
        addr_ = candidate;
        port_ = ntohs(sa.sin_port);
        ec.clear();
        return true;
    }

    bool from_sockaddr(const struct sockaddr_in& sa) noexcept {
        error_code ec;
        return from_sockaddr(sa, ec);
    }

    bool from_sockaddr(const struct sockaddr_in6& sa,
                       error_code& ec) noexcept {
        if (sa.sin6_family != AF_INET6 || sa.sin6_flowinfo != 0u) {
            ec = make_error_code(sa.sin6_family != AF_INET6
                ? errc::address_family_not_supported
                : errc::invalid_argument);
            return false;
        }
        address_v6::bytes_type bytes{{}};
        for (unsigned index = 0; index < 16u; ++index) {
            bytes[index] = sa.sin6_addr.s6_addr[index];
        }
        ip::address candidate(address_v6(bytes, sa.sin6_scope_id));
        addr_ = candidate;
        port_ = ntohs(sa.sin6_port);
        ec.clear();
        return true;
    }

    bool from_sockaddr(const struct sockaddr_in6& sa) noexcept {
        error_code ec;
        return from_sockaddr(sa, ec);
    }

    friend bool operator==(const basic_endpoint& a, const basic_endpoint& b) {
        return a.addr_ == b.addr_ && a.port_ == b.port_;
    }

    friend bool operator!=(const basic_endpoint& a, const basic_endpoint& b) {
        return !(a == b);
    }

private:
    ip::address addr_;
    unsigned short port_;
};

namespace detail {

union endpoint_storage {
    struct sockaddr base;
    struct sockaddr_in v4;
    struct sockaddr_in6 v6;
};

inline bool validate_buffer(const void* data, size_t size,
                              error_code& ec) noexcept {
    const size_t max_api_length =
        static_cast<size_t>(~static_cast<unsigned long>(0));
    if (size > max_api_length) {
        ec = make_error_code(errc::invalid_argument);
        return false;
    }
    if (size != 0u && data == nullptr) {
        ec = make_error_code(errc::invalid_argument);
        return false;
    }
    return true;
}

inline void assign_socket_error(error_code& ec) noexcept {
    int value = errno;
    ec.assign(value > 0 ? value : static_cast<int>(errc::io_error),
              generic_category());
}

template<typename Endpoint>
inline bool endpoint_to_native(const Endpoint& endpoint,
                               endpoint_storage& storage,
                               unsigned int& length,
                               error_code& ec) noexcept {
    if (!endpoint.address().is_valid()) {
        ec = make_error_code(errc::invalid_argument);
        return false;
    }
    endpoint_storage candidate{};
    if (endpoint.address().is_v4()) {
        if (!endpoint.to_sockaddr(candidate.v4, ec)) return false;
        length = sizeof(candidate.v4);
    } else if (endpoint.address().is_v6()) {
        if (!endpoint.to_sockaddr(candidate.v6, ec)) return false;
        length = sizeof(candidate.v6);
    } else {
        ec = make_error_code(errc::address_family_not_supported);
        return false;
    }
    storage = candidate;
    ec.clear();
    return true;
}

template<typename Endpoint>
inline bool endpoint_from_native(const endpoint_storage& storage,
                                 unsigned int length, Endpoint& endpoint,
                                 error_code& ec) noexcept {
    Endpoint candidate;
    if (storage.base.sa_family == AF_INET) {
        if (length != sizeof(storage.v4)) {
            ec = make_error_code(errc::invalid_argument);
            return false;
        }
        if (!candidate.from_sockaddr(storage.v4, ec)) return false;
    } else if (storage.base.sa_family == AF_INET6) {
        if (length != sizeof(storage.v6)) {
            ec = make_error_code(errc::invalid_argument);
            return false;
        }
        if (!candidate.from_sockaddr(storage.v6, ec)) return false;
    } else {
        ec = make_error_code(errc::address_family_not_supported);
        return false;
    }
    endpoint = candidate;
    ec.clear();
    return true;
}

} /* namespace detail */

} /* namespace ip */

/* ═══════════════════════════════════════════════════════════════
 * socket_base - ソケットオプション基底
 * ═══════════════════════════════════════════════════════════════*/

class socket_base {
public:
    /* shutdown タイプ */
    enum shutdown_type {
        shutdown_receive = SHUT_RD,
        shutdown_send = SHUT_WR,
        shutdown_both = SHUT_RDWR
    };

    /* ソケットオプション */
    class reuse_address {
    public:
        explicit reuse_address(bool v = false) : value_(v) {}
        bool value() const { return value_; }
        void value(bool v) { value_ = v; }
        explicit operator bool() const { return value_; }
        static constexpr int level() noexcept { return SOL_SOCKET; }
        static constexpr int name() noexcept { return SO_REUSEADDR; }
    private:
        bool value_;
    };

    class keep_alive {
    public:
        explicit keep_alive(bool v = false) : value_(v) {}
        bool value() const { return value_; }
        void value(bool v) { value_ = v; }
        explicit operator bool() const { return value_; }
        static constexpr int level() noexcept { return SOL_SOCKET; }
        static constexpr int name() noexcept { return SO_KEEPALIVE; }
    private:
        bool value_;
    };

    static constexpr int max_listen_connections = 128;
};

/* ═══════════════════════════════════════════════════════════════
 * basic_socket - 基本ソケット
 * ═══════════════════════════════════════════════════════════════*/

template<typename Protocol>
class basic_socket_acceptor;

template<typename Protocol>
class basic_socket : public socket_base {
public:
    using protocol_type = Protocol;
    using endpoint_type = typename Protocol::endpoint;
    using native_handle_type = int;

    basic_socket() : fd_(-1), protocol_(Protocol::v4()) {}

    explicit basic_socket(const protocol_type& proto)
        : fd_(-1), protocol_(proto) {
        error_code ec;
        if (!open(proto, ec)) rin_panic("net: socket open failed");
    }

    basic_socket(const protocol_type& proto, error_code& ec)
        : fd_(-1), protocol_(proto) {
        open(proto, ec);
    }

    basic_socket(basic_socket&& other) noexcept
        : fd_(other.fd_), protocol_(other.protocol_) {
        other.fd_ = -1;
    }

    ~basic_socket() {
        if (fd_ >= 0) (void)rin_close(fd_);
    }

    basic_socket& operator=(basic_socket&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0 && rin_close(fd_) != 0)
                rin_panic("net: socket move close failed");
            fd_ = other.fd_;
            protocol_ = other.protocol_;
            other.fd_ = -1;
        }
        return *this;
    }

    /* コピー禁止 */
    basic_socket(const basic_socket&) = delete;
    basic_socket& operator=(const basic_socket&) = delete;

    /* オープン */
    bool open(const protocol_type& proto, error_code& ec) {
        if (fd_ >= 0 && !close(ec)) return false;
        int candidate = ::socket(proto.family(), proto.type(), proto.protocol());
        if (candidate < 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        fd_ = candidate;
        protocol_ = proto;
        ec.clear();
        return true;
    }

    bool open(const protocol_type& proto = Protocol::v4()) {
        error_code ec;
        return open(proto, ec);
    }

    bool is_open() const noexcept { return fd_ >= 0; }

    bool close(error_code& ec) {
        if (fd_ < 0) {
            ec.clear();
            return true;
        }
        if (rin_close(fd_) != 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        fd_ = -1;
        ec.clear();
        return true;
    }

    bool close() {
        error_code ec;
        return close(ec);
    }

    native_handle_type native_handle() const noexcept { return fd_; }
    protocol_type protocol() const noexcept { return protocol_; }

    /* バインド */
    bool bind(const endpoint_type& endpoint, error_code& ec) {
        if (!require_open(ec)) return false;
        ip::detail::endpoint_storage storage{};
        unsigned int length = 0;
        if (!ip::detail::endpoint_to_native(endpoint, storage, length, ec))
            return false;
        if (storage.base.sa_family != protocol_.family()) {
            ec = make_error_code(errc::address_family_not_supported);
            return false;
        }
        if (::bind(fd_, &storage.base, length) != 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        ec.clear();
        return true;
    }

    bool bind(const endpoint_type& endpoint) {
        error_code ec;
        return bind(endpoint, ec);
    }

    /* 接続 */
    bool connect(const endpoint_type& endpoint, error_code& ec) {
        if (!require_open(ec)) return false;
        ip::detail::endpoint_storage storage{};
        unsigned int length = 0;
        if (!ip::detail::endpoint_to_native(endpoint, storage, length, ec))
            return false;
        if (storage.base.sa_family != protocol_.family()) {
            ec = make_error_code(errc::address_family_not_supported);
            return false;
        }
        if (::connect(fd_, &storage.base, length) != 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        ec.clear();
        return true;
    }

    bool connect(const endpoint_type& endpoint) {
        error_code ec;
        return connect(endpoint, ec);
    }

    /* シャットダウン */
    bool shutdown(shutdown_type how, error_code& ec) {
        if (!require_open(ec)) return false;
        if (::shutdown(fd_, static_cast<int>(how)) != 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        ec.clear();
        return true;
    }

    bool shutdown(shutdown_type how) {
        error_code ec;
        return shutdown(how, ec);
    }

    /* オプション設定 */
    template<typename Option>
    bool set_option(const Option& option, error_code& ec) {
        if (!require_open(ec)) return false;
        int val = option.value() ? 1 : 0;
        if (::setsockopt(fd_, Option::level(), Option::name(), &val,
                         sizeof(val)) != 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        ec.clear();
        return true;
    }

    template<typename Option>
    bool set_option(const Option& option) {
        error_code ec;
        return set_option(option, ec);
    }

    /* オプション取得 */
    template<typename Option>
    bool get_option(Option& option, error_code& ec) const {
        if (!require_open(ec)) return false;
        int value = 0;
        unsigned int length = sizeof(value);
        if (::getsockopt(fd_, Option::level(), Option::name(), &value,
                         &length) != 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        if (length != sizeof(value) || (value != 0 && value != 1)) {
            ec = make_error_code(errc::protocol_error);
            return false;
        }
        option.value(value != 0);
        ec.clear();
        return true;
    }

    template<typename Option>
    bool get_option(Option& option) const {
        error_code ec;
        return get_option(option, ec);
    }

    /* ローカルエンドポイント取得 */
    bool local_endpoint(endpoint_type& endpoint, error_code& ec) const {
        if (!require_open(ec)) return false;
        ip::detail::endpoint_storage storage{};
        unsigned int length = sizeof(storage);
        if (::getsockname(fd_, &storage.base, &length) != 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        endpoint_type candidate;
        if (!ip::detail::endpoint_from_native(storage, length, candidate, ec))
            return false;
        if (candidate.protocol().family() != protocol_.family()) {
            ec = make_error_code(errc::address_family_not_supported);
            return false;
        }
        endpoint = candidate;
        ec.clear();
        return true;
    }

    endpoint_type local_endpoint(error_code& ec) const {
        endpoint_type endpoint;
        (void)local_endpoint(endpoint, ec);
        return endpoint;
    }

    endpoint_type local_endpoint() const {
        error_code ec;
        endpoint_type endpoint = local_endpoint(ec);
        if (ec) rin_panic("net: local endpoint query failed");
        return endpoint;
    }

    /* リモートエンドポイント取得 */
    bool remote_endpoint(endpoint_type& endpoint, error_code& ec) const {
        if (!require_open(ec)) return false;
        ip::detail::endpoint_storage storage{};
        unsigned int length = sizeof(storage);
        if (::getpeername(fd_, &storage.base, &length) != 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        endpoint_type candidate;
        if (!ip::detail::endpoint_from_native(storage, length, candidate, ec))
            return false;
        if (candidate.protocol().family() != protocol_.family()) {
            ec = make_error_code(errc::address_family_not_supported);
            return false;
        }
        endpoint = candidate;
        ec.clear();
        return true;
    }

    endpoint_type remote_endpoint(error_code& ec) const {
        endpoint_type endpoint;
        (void)remote_endpoint(endpoint, ec);
        return endpoint;
    }

    endpoint_type remote_endpoint() const {
        error_code ec;
        endpoint_type endpoint = remote_endpoint(ec);
        if (ec) rin_panic("net: remote endpoint query failed");
        return endpoint;
    }

protected:
    bool require_open(error_code& ec) const noexcept {
        if (fd_ >= 0) return true;
        ec = make_error_code(errc::bad_file_descriptor);
        return false;
    }

    int fd_;
    protocol_type protocol_;

    template<typename>
    friend class basic_socket_acceptor;
};

/* ═══════════════════════════════════════════════════════════════
 * basic_stream_socket - ストリームソケット (TCP)
 * ═══════════════════════════════════════════════════════════════*/

template<typename Protocol>
class basic_stream_socket : public basic_socket<Protocol> {
    using base = basic_socket<Protocol>;
public:
    using protocol_type = Protocol;
    using endpoint_type = typename Protocol::endpoint;

    basic_stream_socket() = default;
    explicit basic_stream_socket(const protocol_type& proto) : base(proto) {}
    basic_stream_socket(const protocol_type& proto, error_code& ec)
        : base(proto, ec) {}
    basic_stream_socket(basic_stream_socket&& other) = default;
    basic_stream_socket& operator=(basic_stream_socket&& other) = default;

    /* 送信 */
    size_t send(const void* data, size_t size, error_code& ec) {
        if (!this->require_open(ec)) return 0;
        if (!ip::detail::validate_buffer(data, size, ec)) return 0;
        long n = ::send(this->fd_, data, size, 0);
        if (n < 0) {
            ip::detail::assign_socket_error(ec);
            return 0;
        }
        if (static_cast<size_t>(n) > size) {
            ec = make_error_code(errc::protocol_error);
            return 0;
        }
        ec.clear();
        return static_cast<size_t>(n);
    }

    size_t send(const void* data, size_t size) {
        error_code ec;
        size_t result = send(data, size, ec);
        if (ec) rin_panic("net: stream send failed");
        return result;
    }

    size_t send(const string& data) {
        return send(data.data(), data.size());
    }

    size_t send(const string& data, error_code& ec) {
        return send(data.data(), data.size(), ec);
    }

    /* 受信 */
    size_t receive(void* data, size_t size, error_code& ec) {
        if (!this->require_open(ec)) return 0;
        if (!ip::detail::validate_buffer(data, size, ec)) return 0;
        long n = ::recv(this->fd_, data, size, 0);
        if (n < 0) {
            ip::detail::assign_socket_error(ec);
            return 0;
        }
        if (static_cast<size_t>(n) > size) {
            ec = make_error_code(errc::protocol_error);
            return 0;
        }
        ec.clear();
        return static_cast<size_t>(n);
    }

    size_t receive(void* data, size_t size) {
        error_code ec;
        size_t result = receive(data, size, ec);
        if (ec) rin_panic("net: stream receive failed");
        return result;
    }

    string receive(size_t max_size = 4096) {
        error_code ec;
        string result = receive(max_size, ec);
        if (ec) rin_panic("net: stream receive failed");
        return result;
    }

    string receive(size_t max_size, error_code& ec) {
        if (max_size == 0u) {
            ec.clear();
            return string();
        }
        unique_ptr<char[]> buf(new char[max_size]);
        size_t n = receive(buf.get(), max_size, ec);
        return string(buf.get(), n);
    }

    /* read_some / write_some */
    size_t read_some(void* data, size_t size) { return receive(data, size); }
    size_t read_some(void* data, size_t size, error_code& ec) {
        return receive(data, size, ec);
    }
    size_t write_some(const void* data, size_t size) { return send(data, size); }
    size_t write_some(const void* data, size_t size, error_code& ec) {
        return send(data, size, ec);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * basic_datagram_socket - データグラムソケット (UDP)
 * ═══════════════════════════════════════════════════════════════*/

template<typename Protocol>
class basic_datagram_socket : public basic_socket<Protocol> {
    using base = basic_socket<Protocol>;
public:
    using protocol_type = Protocol;
    using endpoint_type = typename Protocol::endpoint;

    basic_datagram_socket() = default;
    explicit basic_datagram_socket(const protocol_type& proto) : base(proto) {}
    basic_datagram_socket(const protocol_type& proto, error_code& ec)
        : base(proto, ec) {}
    basic_datagram_socket(basic_datagram_socket&& other) = default;
    basic_datagram_socket& operator=(basic_datagram_socket&& other) = default;

    /* 送信 */
    size_t send_to(const void* data, size_t size, const endpoint_type& dest,
                   error_code& ec) {
        if (!this->require_open(ec)) return 0;
        if (!ip::detail::validate_buffer(data, size, ec)) return 0;
        ip::detail::endpoint_storage storage{};
        unsigned int length = 0;
        if (!ip::detail::endpoint_to_native(dest, storage, length, ec)) return 0;
        if (storage.base.sa_family != this->protocol_.family()) {
            ec = make_error_code(errc::address_family_not_supported);
            return 0;
        }
        long n = ::sendto(this->fd_, data, size, 0,
                          &storage.base, length);
        if (n < 0) {
            ip::detail::assign_socket_error(ec);
            return 0;
        }
        if (static_cast<size_t>(n) > size) {
            ec = make_error_code(errc::protocol_error);
            return 0;
        }
        ec.clear();
        return static_cast<size_t>(n);
    }

    size_t send_to(const void* data, size_t size, const endpoint_type& dest) {
        error_code ec;
        size_t result = send_to(data, size, dest, ec);
        if (ec) rin_panic("net: datagram send failed");
        return result;
    }

    /* 受信 */
    size_t receive_from(void* data, size_t size, endpoint_type& sender,
                        error_code& ec) {
        if (!this->require_open(ec)) return 0;
        if (!ip::detail::validate_buffer(data, size, ec)) return 0;
        ip::detail::endpoint_storage storage{};
        unsigned int length = sizeof(storage);
        long n = ::recvfrom(this->fd_, data, size, 0,
                            &storage.base, &length);
        if (n < 0) {
            ip::detail::assign_socket_error(ec);
            return 0;
        }
        if (static_cast<size_t>(n) > size) {
            ec = make_error_code(errc::protocol_error);
            return 0;
        }
        endpoint_type candidate;
        if (!ip::detail::endpoint_from_native(storage, length, candidate, ec))
            return 0;
        if (candidate.protocol().family() != this->protocol_.family()) {
            ec = make_error_code(errc::address_family_not_supported);
            return 0;
        }
        sender = candidate;
        ec.clear();
        return static_cast<size_t>(n);
    }

    size_t receive_from(void* data, size_t size, endpoint_type& sender) {
        error_code ec;
        size_t result = receive_from(data, size, sender, ec);
        if (ec) rin_panic("net: datagram receive failed");
        return result;
    }

    /* send/receive (connected socket用) */
    size_t send(const void* data, size_t size, error_code& ec) {
        if (!this->require_open(ec)) return 0;
        if (!ip::detail::validate_buffer(data, size, ec)) return 0;
        long n = ::send(this->fd_, data, size, 0);
        if (n < 0) {
            ip::detail::assign_socket_error(ec);
            return 0;
        }
        if (static_cast<size_t>(n) > size) {
            ec = make_error_code(errc::protocol_error);
            return 0;
        }
        ec.clear();
        return static_cast<size_t>(n);
    }

    size_t send(const void* data, size_t size) {
        error_code ec;
        size_t result = send(data, size, ec);
        if (ec) rin_panic("net: datagram send failed");
        return result;
    }

    size_t receive(void* data, size_t size, error_code& ec) {
        if (!this->require_open(ec)) return 0;
        if (!ip::detail::validate_buffer(data, size, ec)) return 0;
        long n = ::recv(this->fd_, data, size, 0);
        if (n < 0) {
            ip::detail::assign_socket_error(ec);
            return 0;
        }
        if (static_cast<size_t>(n) > size) {
            ec = make_error_code(errc::protocol_error);
            return 0;
        }
        ec.clear();
        return static_cast<size_t>(n);
    }

    size_t receive(void* data, size_t size) {
        error_code ec;
        size_t result = receive(data, size, ec);
        if (ec) rin_panic("net: datagram receive failed");
        return result;
    }
};

/* ═══════════════════════════════════════════════════════════════
 * basic_socket_acceptor - 接続受け入れ (サーバー用)
 * ═══════════════════════════════════════════════════════════════*/

template<typename Protocol>
class basic_socket_acceptor : public socket_base {
public:
    using protocol_type = Protocol;
    using endpoint_type = typename Protocol::endpoint;
    using socket_type = basic_stream_socket<Protocol>;

    basic_socket_acceptor() : fd_(-1), protocol_(Protocol::v4()) {}

    explicit basic_socket_acceptor(const endpoint_type& endpoint)
        : fd_(-1), protocol_(endpoint.protocol()) {
        error_code ec;
        if (open(protocol_, ec) && bind(endpoint, ec) && listen(ec)) return;
        if (fd_ >= 0) {
            error_code cleanup;
            (void)close(cleanup);
        }
        rin_panic("net: acceptor setup failed");
    }

    basic_socket_acceptor(const endpoint_type& endpoint, error_code& ec)
        : fd_(-1), protocol_(endpoint.protocol()) {
        if (open(protocol_, ec) && bind(endpoint, ec) && listen(ec)) return;
        if (fd_ >= 0) {
            error_code cleanup;
            (void)close(cleanup);
        }
    }

    basic_socket_acceptor(basic_socket_acceptor&& other) noexcept
        : fd_(other.fd_), protocol_(other.protocol_) {
        other.fd_ = -1;
    }

    ~basic_socket_acceptor() {
        if (fd_ >= 0) (void)rin_close(fd_);
    }

    basic_socket_acceptor& operator=(basic_socket_acceptor&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0 && rin_close(fd_) != 0)
                rin_panic("net: acceptor move close failed");
            fd_ = other.fd_;
            protocol_ = other.protocol_;
            other.fd_ = -1;
        }
        return *this;
    }

    /* コピー禁止 */
    basic_socket_acceptor(const basic_socket_acceptor&) = delete;
    basic_socket_acceptor& operator=(const basic_socket_acceptor&) = delete;

    bool open(const protocol_type& proto, error_code& ec) {
        if (fd_ >= 0 && !close(ec)) return false;
        int candidate = ::socket(proto.family(), proto.type(), proto.protocol());
        if (candidate < 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        fd_ = candidate;
        protocol_ = proto;
        ec.clear();
        return true;
    }

    bool open(const protocol_type& proto = Protocol::v4()) {
        error_code ec;
        return open(proto, ec);
    }

    bool is_open() const noexcept { return fd_ >= 0; }
    protocol_type protocol() const noexcept { return protocol_; }

    bool close(error_code& ec) {
        if (fd_ < 0) {
            ec.clear();
            return true;
        }
        if (rin_close(fd_) != 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        fd_ = -1;
        ec.clear();
        return true;
    }

    bool close() {
        error_code ec;
        return close(ec);
    }

    bool bind(const endpoint_type& endpoint, error_code& ec) {
        if (!require_open(ec)) return false;
        ip::detail::endpoint_storage storage{};
        unsigned int length = 0;
        if (!ip::detail::endpoint_to_native(endpoint, storage, length, ec))
            return false;
        if (storage.base.sa_family != protocol_.family()) {
            ec = make_error_code(errc::address_family_not_supported);
            return false;
        }
        if (::bind(fd_, &storage.base, length) != 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        ec.clear();
        return true;
    }

    bool bind(const endpoint_type& endpoint) {
        error_code ec;
        return bind(endpoint, ec);
    }

    bool listen(int backlog, error_code& ec) {
        if (!require_open(ec)) return false;
        if (backlog < 0 || backlog > max_listen_connections) {
            ec = make_error_code(errc::invalid_argument);
            return false;
        }
        if (::listen(fd_, backlog) != 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        ec.clear();
        return true;
    }

    bool listen(error_code& ec) {
        return listen(max_listen_connections, ec);
    }

    bool listen(int backlog = max_listen_connections) {
        error_code ec;
        return listen(backlog, ec);
    }

    bool accept(socket_type& socket, endpoint_type& peer_endpoint,
                error_code& ec) {
        if (!require_open(ec)) return false;
        ip::detail::endpoint_storage storage{};
        unsigned int length = sizeof(storage);
        int candidate_fd = ::accept(fd_, &storage.base, &length);
        if (candidate_fd < 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        endpoint_type candidate_endpoint;
        if (!ip::detail::endpoint_from_native(storage, length,
                                              candidate_endpoint, ec) ||
            candidate_endpoint.protocol().family() != protocol_.family()) {
            if (!ec) ec = make_error_code(errc::address_family_not_supported);
            (void)rin_close(candidate_fd);
            return false;
        }
        if (socket.fd_ >= 0 && !socket.close(ec)) {
            (void)rin_close(candidate_fd);
            return false;
        }
        socket.fd_ = candidate_fd;
        socket.protocol_ = protocol_;
        peer_endpoint = candidate_endpoint;
        ec.clear();
        return true;
    }

    bool accept(socket_type& socket, error_code& ec) {
        endpoint_type peer_endpoint;
        return accept(socket, peer_endpoint, ec);
    }

    socket_type accept(error_code& ec) {
        socket_type socket;
        (void)accept(socket, ec);
        return socket;
    }

    socket_type accept() {
        error_code ec;
        socket_type socket = accept(ec);
        if (ec) rin_panic("net: accept failed");
        return socket;
    }

    socket_type accept(endpoint_type& peer_endpoint, error_code& ec) {
        socket_type socket;
        (void)accept(socket, peer_endpoint, ec);
        return socket;
    }

    socket_type accept(endpoint_type& peer_endpoint) {
        error_code ec;
        socket_type socket = accept(peer_endpoint, ec);
        if (ec) rin_panic("net: accept failed");
        return socket;
    }

    template<typename Option>
    bool set_option(const Option& option, error_code& ec) {
        if (!require_open(ec)) return false;
        int val = option.value() ? 1 : 0;
        if (::setsockopt(fd_, Option::level(), Option::name(), &val,
                         sizeof(val)) != 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        ec.clear();
        return true;
    }

    template<typename Option>
    bool set_option(const Option& option) {
        error_code ec;
        return set_option(option, ec);
    }

    template<typename Option>
    bool get_option(Option& option, error_code& ec) const {
        if (!require_open(ec)) return false;
        int value = 0;
        unsigned int length = sizeof(value);
        if (::getsockopt(fd_, Option::level(), Option::name(), &value,
                         &length) != 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        if (length != sizeof(value) || (value != 0 && value != 1)) {
            ec = make_error_code(errc::protocol_error);
            return false;
        }
        option.value(value != 0);
        ec.clear();
        return true;
    }

    template<typename Option>
    bool get_option(Option& option) const {
        error_code ec;
        return get_option(option, ec);
    }

    bool local_endpoint(endpoint_type& endpoint, error_code& ec) const {
        if (!require_open(ec)) return false;
        ip::detail::endpoint_storage storage{};
        unsigned int length = sizeof(storage);
        if (::getsockname(fd_, &storage.base, &length) != 0) {
            ip::detail::assign_socket_error(ec);
            return false;
        }
        endpoint_type candidate;
        if (!ip::detail::endpoint_from_native(storage, length, candidate, ec))
            return false;
        if (candidate.protocol().family() != protocol_.family()) {
            ec = make_error_code(errc::address_family_not_supported);
            return false;
        }
        endpoint = candidate;
        ec.clear();
        return true;
    }

    endpoint_type local_endpoint(error_code& ec) const {
        endpoint_type endpoint;
        (void)local_endpoint(endpoint, ec);
        return endpoint;
    }

    endpoint_type local_endpoint() const {
        error_code ec;
        endpoint_type endpoint = local_endpoint(ec);
        if (ec) rin_panic("net: acceptor endpoint query failed");
        return endpoint;
    }

private:
    bool require_open(error_code& ec) const noexcept {
        if (fd_ >= 0) return true;
        ec = make_error_code(errc::bad_file_descriptor);
        return false;
    }

    int fd_;
    protocol_type protocol_;

    friend class basic_stream_socket<Protocol>;
};

/* ═══════════════════════════════════════════════════════════════
 * 型エイリアス
 * ═══════════════════════════════════════════════════════════════*/

namespace ip {

using tcp_socket = basic_stream_socket<tcp>;
using tcp_acceptor = basic_socket_acceptor<tcp>;
using udp_socket = basic_datagram_socket<udp>;

} /* namespace ip */

/* ═══════════════════════════════════════════════════════════════
 * resolver - 名前解決
 * ═══════════════════════════════════════════════════════════════*/

namespace ip {

enum class resolver_flags : unsigned int {
    none = 0,
    passive = AI_PASSIVE,
    canonical_name = AI_CANONNAME,
    numeric_host = AI_NUMERICHOST,
    numeric_service = AI_NUMERICSERV,
    v4_mapped = AI_V4MAPPED,
    all_matching = AI_ALL,
    address_configured = AI_ADDRCONFIG
};

enum class resolver_async_state : unsigned int {
    invalid,
    pending,
    completed,
    cancelled,
    failed
};

inline constexpr resolver_flags operator|(resolver_flags left,
                                          resolver_flags right) noexcept {
    return static_cast<resolver_flags>(static_cast<unsigned int>(left) |
                                       static_cast<unsigned int>(right));
}

inline constexpr resolver_flags operator&(resolver_flags left,
                                          resolver_flags right) noexcept {
    return static_cast<resolver_flags>(static_cast<unsigned int>(left) &
                                       static_cast<unsigned int>(right));
}

template<typename Protocol, typename Allocator = allocator<unsigned char>>
class basic_resolver {
public:
    using protocol_type = Protocol;
    using endpoint_type = typename Protocol::endpoint;
    using allocator_type = Allocator;

    class resolver_entry {
    public:
        resolver_entry() noexcept
            : endpoint_(), host_name_(nullptr), service_name_(nullptr) {}

        const endpoint_type& endpoint() const noexcept { return endpoint_; }
        const char* host_name() const noexcept {
            return host_name_ ? host_name_ : "";
        }
        const char* service_name() const noexcept {
            return service_name_ ? service_name_ : "";
        }

    private:
        endpoint_type endpoint_;
        const char* host_name_;
        const char* service_name_;

        friend class basic_resolver;
    };

    class results_type {
    public:
        using value_type = resolver_entry;
        using const_iterator = const resolver_entry*;

        results_type() noexcept : storage_(nullptr) {}

        results_type(const results_type& other) noexcept
            : storage_(other.storage_) {
            retain();
        }

        results_type(results_type&& other) noexcept
            : storage_(other.storage_) {
            other.storage_ = nullptr;
        }

        results_type& operator=(const results_type& other) noexcept {
            if (this != &other) {
                shared_results* candidate = other.storage_;
                if (candidate) retain(candidate);
                release();
                storage_ = candidate;
            }
            return *this;
        }

        results_type& operator=(results_type&& other) noexcept {
            if (this != &other) {
                release();
                storage_ = other.storage_;
                other.storage_ = nullptr;
            }
            return *this;
        }

        ~results_type() { release(); }

        const_iterator begin() const noexcept {
            return storage_ ? storage_->entries : nullptr;
        }
        const_iterator end() const noexcept {
            return storage_ ? storage_->entries + storage_->size : nullptr;
        }
        bool empty() const noexcept { return !storage_ || storage_->size == 0; }
        size_t size() const noexcept { return storage_ ? storage_->size : 0; }
        const resolver_entry& operator[](size_t index) const {
            if (!storage_ || index >= storage_->size)
                rin_panic("net: resolver result out of range");
            return storage_->entries[index];
        }

    private:
        struct shared_results {
            allocator_type allocator;
            size_t allocation_units;
            size_t references;
            size_t size;
            resolver_entry entries[1];

            shared_results(const allocator_type& source_allocator,
                           size_t units, size_t count)
                : allocator(source_allocator), allocation_units(units),
                  references(1), size(count), entries{} {}
        };

        using storage_allocator =
            typename allocator_traits<allocator_type>::template
                rebind_alloc<shared_results>;
        using storage_traits = allocator_traits<storage_allocator>;
        static_assert(is_same<typename storage_traits::pointer,
                              shared_results*>::value,
                      "net: resolver allocator requires raw pointers");

        bool allocate(size_t size, size_t string_bytes,
                      const allocator_type& source_allocator,
                      error_code& ec) {
            if (size == 0) {
                ec.clear();
                return true;
            }
            constexpr size_t prefix =
                __builtin_offsetof(shared_results, entries);
            if (size > (static_cast<size_t>(-1) - prefix) /
                           sizeof(resolver_entry)) {
                ec = make_error_code(errc::value_too_large);
                return false;
            }
            size_t entry_bytes = size * sizeof(resolver_entry);
            if (string_bytes > static_cast<size_t>(-1) - prefix - entry_bytes) {
                ec = make_error_code(errc::value_too_large);
                return false;
            }
            size_t bytes = prefix + entry_bytes + string_bytes;
            if (bytes > static_cast<size_t>(-1) -
                            (sizeof(shared_results) - 1u)) {
                ec = make_error_code(errc::value_too_large);
                return false;
            }
            size_t units = (bytes + sizeof(shared_results) - 1u) /
                           sizeof(shared_results);
            storage_allocator owner(source_allocator);
            shared_results* allocation = storage_traits::allocate(owner, units);
            if (!allocation) {
                ec = make_error_code(errc::not_enough_memory);
                return false;
            }
            shared_results* candidate = new (allocation) shared_results(
                source_allocator, units, size);
            for (size_t index = 1; index < size; ++index)
                new (&candidate->entries[index]) resolver_entry();
            storage_ = candidate;
            ec.clear();
            return true;
        }

        static void retain(shared_results* storage) noexcept {
            size_t observed = __atomic_load_n(&storage->references,
                                              __ATOMIC_ACQUIRE);
            for (;;) {
                if (observed == static_cast<size_t>(-1))
                    rin_panic("net: resolver reference overflow");
                size_t desired = observed + 1;
                if (__atomic_compare_exchange_n(&storage->references,
                                                &observed, desired, true,
                                                __ATOMIC_ACQ_REL,
                                                __ATOMIC_ACQUIRE))
                    return;
            }
        }

        void retain() noexcept {
            if (storage_) retain(storage_);
        }

        void release() noexcept {
            if (!storage_) return;
            shared_results* old = storage_;
            storage_ = nullptr;
            if (__atomic_sub_fetch(&old->references, static_cast<size_t>(1),
                                   __ATOMIC_ACQ_REL) != 0)
                return;
            for (size_t index = old->size; index > 1; --index)
                old->entries[index - 1].~resolver_entry();
            storage_allocator owner(old->allocator);
            size_t units = old->allocation_units;
            old->~shared_results();
            storage_traits::deallocate(owner, old, units);
        }

        shared_results* storage_;

        friend class basic_resolver;
    };

    class async_operation {
    public:
        async_operation(const async_operation&) = delete;
        async_operation& operator=(const async_operation&) = delete;
        async_operation& operator=(async_operation&&) = delete;

        async_operation(async_operation&& other) noexcept
            : allocator_(other.allocator_), handle_(other.handle_),
              secondary_handle_(other.secondary_handle_), port_(other.port_),
              family_(other.family_), wire_family_(other.wire_family_),
              secondary_family_(other.secondary_family_),
              socket_type_(other.socket_type_), protocol_(other.protocol_),
              flags_(other.flags_), wire_flags_(other.wire_flags_),
              v4_fallback_(other.v4_fallback_),
              next_family_(other.next_family_),
              first_result_(other.first_result_),
              first_result_family_(other.first_result_family_),
              first_result_valid_(other.first_result_valid_),
              second_result_(other.second_result_),
              second_result_family_(other.second_result_family_),
              second_result_valid_(other.second_result_valid_),
              parallel_(other.parallel_),
              poll_primary_next_(other.poll_primary_next_),
              completion_dispatched_(other.completion_dispatched_),
              watch_addrconfig_(other.watch_addrconfig_),
              addrconfig_snapshot_(other.addrconfig_snapshot_),
              host_length_(other.host_length_),
              service_length_(other.service_length_), state_(other.state_) {
            copy_text(host_, other.host_, host_length_);
            copy_text(service_, other.service_, service_length_);
            other.handle_ = 0u;
            other.secondary_handle_ = 0u;
            other.host_length_ = 0u;
            other.service_length_ = 0u;
            other.host_[0] = '\0';
            other.service_[0] = '\0';
            other.v4_fallback_ = false;
            other.next_family_ = AF_UNSPEC;
            other.first_result_ = {};
            other.first_result_family_ = AF_UNSPEC;
            other.first_result_valid_ = false;
            other.second_result_ = {};
            other.second_result_family_ = AF_UNSPEC;
            other.second_result_valid_ = false;
            other.secondary_family_ = AF_UNSPEC;
            other.parallel_ = false;
            other.poll_primary_next_ = true;
            other.completion_dispatched_ = true;
            other.watch_addrconfig_ = false;
            other.addrconfig_snapshot_ = {};
            other.state_ = resolver_async_state::invalid;
        }

        ~async_operation() {
            if (state_ != resolver_async_state::pending) return;
            if (handle_ != 0u) (void)rin_dns_resolve_cancel(handle_);
            if (secondary_handle_ != 0u)
                (void)rin_dns_resolve_cancel(secondary_handle_);
        }

        resolver_async_state state() const noexcept { return state_; }

        resolver_async_state poll(results_type& output, error_code& ec) {
            if (state_ != resolver_async_state::pending ||
                !has_active_handle()) {
                ec = make_error_code(errc::invalid_argument);
                return state_;
            }
            if (!refresh_addrconfig(ec)) return state_;
            if (parallel_) return poll_parallel(output, ec);
            RinDnsGetAddrInfoBatchResult native{};
            int status = rin_dns_getaddrinfo_batch_poll(handle_, &native);
            if (status == 1) {
                ec.clear();
                return state_;
            }
            if (status != 0) {
                handle_ = 0u;
                state_ = resolver_async_state::failed;
                ec = make_error_code(errc::io_error);
                return state_;
            }

            handle_ = 0u;
            /* The snapshot may have changed while the nonblocking service
             * poll was in progress.  Recheck before a terminal reply can be
             * retained or published. */
            if (!refresh_addrconfig(ec)) return state_;
            if (native.reserved0 != 0u || native.reserved1 != 0u) {
                state_ = resolver_async_state::failed;
                ec = make_error_code(errc::protocol_error);
                return state_;
            }
            if (native.status != RIN_DNS_STATUS_DONE) {
                if (native.status == RIN_DNS_STATUS_NOT_FOUND &&
                    v4_fallback_ && wire_family_ == AF_INET6) {
                    v4_fallback_ = false;
                    if (!begin_family(AF_INET, ec)) return state_;
                    return state_;
                }
                if (native.status == RIN_DNS_STATUS_NOT_FOUND &&
                    next_family_ != AF_UNSPEC) {
                    int followup_family = next_family_;
                    next_family_ = AF_UNSPEC;
                    if (!begin_family(followup_family, ec)) return state_;
                    return state_;
                }
                if (native.status == RIN_DNS_STATUS_NOT_FOUND &&
                    first_result_valid_)
                    return complete(nullptr, AF_UNSPEC, output, ec);
                state_ = native.status == RIN_DNS_STATUS_CANCELLED
                    ? resolver_async_state::cancelled
                    : resolver_async_state::failed;
                if (native.status == RIN_DNS_STATUS_TIMED_OUT)
                    ec = make_error_code(errc::timed_out);
                else if (native.status == RIN_DNS_STATUS_CANCELLED)
                    ec = make_error_code(errc::operation_canceled);
                else if (native.status == RIN_DNS_STATUS_CONFIG_MISSING)
                    ec = make_error_code(errc::network_unreachable);
                else if (native.status == RIN_DNS_STATUS_SEND_BLOCKED)
                    ec = make_error_code(errc::resource_unavailable_try_again);
                else if (native.status == RIN_DNS_STATUS_NOT_FOUND)
                    ec = make_error_code(errc::address_not_available);
                else if (native.status == RIN_DNS_STATUS_FAILED)
                    ec = make_error_code(errc::io_error);
                else
                    ec = make_error_code(errc::protocol_error);
                return state_;
            }

            if (next_family_ != AF_UNSPEC) {
                if (!basic_resolver::validate_async_batch_shape(native, ec)) {
                    state_ = resolver_async_state::failed;
                    return state_;
                }
                first_result_ = native;
                first_result_family_ = wire_family_;
                first_result_valid_ = true;
                int followup_family = next_family_;
                next_family_ = AF_UNSPEC;
                if (!begin_family(followup_family, ec)) return state_;
                return state_;
            }
            return complete(&native, wire_family_, output, ec);
        }

        template<typename CompletionHandler>
        bool dispatch(CompletionHandler&& handler, error_code& ec) {
            if (completion_dispatched_ ||
                state_ != resolver_async_state::pending ||
                !has_active_handle()) {
                ec = make_error_code(errc::invalid_argument);
                return false;
            }
            results_type result;
            resolver_async_state observed = poll(result, ec);
            if (observed == resolver_async_state::pending) return false;
            completion_dispatched_ = true;
            error_code delivered_error = ec;
            static_cast<CompletionHandler&&>(handler)(
                observed, static_cast<results_type&&>(result), delivered_error);
            return true;
        }

        bool cancel(error_code& ec) {
            if (state_ != resolver_async_state::pending ||
                !has_active_handle()) {
                ec = make_error_code(errc::invalid_argument);
                return false;
            }
            unsigned int old_handle = handle_;
            unsigned int old_secondary_handle = secondary_handle_;
            handle_ = 0u;
            secondary_handle_ = 0u;
            bool primary_cancelled = old_handle == 0u ||
                rin_dns_resolve_cancel(old_handle) == 0;
            bool secondary_cancelled = old_secondary_handle == 0u ||
                rin_dns_resolve_cancel(old_secondary_handle) == 0;
            if (!primary_cancelled || !secondary_cancelled) {
                state_ = resolver_async_state::failed;
                ec = make_error_code(errc::io_error);
                return false;
            }
            state_ = resolver_async_state::cancelled;
            ec.clear();
            return true;
        }

    private:
        async_operation(const allocator_type& allocator, unsigned int handle,
                        unsigned short port, int family, int wire_family,
                        int socket_type, int protocol, unsigned int flags,
                        unsigned int wire_flags, bool v4_fallback,
                        const char* host,
                        size_t host_length, const char* service,
                        size_t service_length,
                        int next_family = AF_UNSPEC,
                        unsigned int secondary_handle = 0u,
                        int secondary_family = AF_UNSPEC,
                        bool watch_addrconfig = false,
                        const RinNetPrimaryInfo* addrconfig_snapshot = nullptr) noexcept
            : allocator_(allocator), handle_(handle),
              secondary_handle_(secondary_handle), port_(port),
              family_(family), wire_family_(wire_family),
              secondary_family_(secondary_family),
              socket_type_(socket_type), protocol_(protocol),
              flags_(flags), wire_flags_(wire_flags),
              v4_fallback_(v4_fallback),
              next_family_(next_family), first_result_(),
              first_result_family_(AF_UNSPEC), first_result_valid_(false),
              second_result_(), second_result_family_(AF_UNSPEC),
              second_result_valid_(false),
              parallel_(secondary_handle != 0u), poll_primary_next_(true),
              completion_dispatched_(false),
              watch_addrconfig_(watch_addrconfig && addrconfig_snapshot != nullptr),
              addrconfig_snapshot_(),
              host_length_(host_length), service_length_(service_length),
              state_(handle != 0u ? resolver_async_state::pending
                                  : resolver_async_state::invalid) {
            copy_text(host_, host, host_length_);
            copy_text(service_, service, service_length_);
            if (watch_addrconfig_) addrconfig_snapshot_ = *addrconfig_snapshot;
        }

        bool begin_family(int family, error_code& ec) {
            int next_handle = rin_dns_getaddrinfo_batch_begin(
                host_, service_, family, socket_type_, protocol_, wire_flags_);
            if (next_handle <= 0) {
                state_ = resolver_async_state::failed;
                ec = make_error_code(errc::io_error);
                return false;
            }
            handle_ = static_cast<unsigned int>(next_handle);
            wire_family_ = family;
            ec.clear();
            return true;
        }

        void clear_stored_results() noexcept {
            first_result_ = {};
            first_result_family_ = AF_UNSPEC;
            first_result_valid_ = false;
            second_result_ = {};
            second_result_family_ = AF_UNSPEC;
            second_result_valid_ = false;
        }

        bool restart_for_addrconfig(const RinNetPrimaryInfo& snapshot,
                                    int ipv4_configured,
                                    int ipv6_configured,
                                    error_code& ec) {
            int planned_family = AF_UNSPEC;
            int planned_next = AF_UNSPEC;
            bool planned_fallback = false;
            int primary;
            if (!basic_resolver::async_addrconfig_plan(
                    family_, flags_, ipv4_configured, ipv6_configured,
                    planned_family, planned_next, planned_fallback, ec)) {
                state_ = resolver_async_state::failed;
                return false;
            }
            primary = rin_dns_getaddrinfo_batch_begin(
                host_, service_, planned_family, socket_type_, protocol_,
                wire_flags_);
            if (primary <= 0) {
                state_ = resolver_async_state::failed;
                ec = make_error_code(errc::io_error);
                return false;
            }
            handle_ = static_cast<unsigned int>(primary);
            secondary_handle_ = 0u;
            wire_family_ = planned_family;
            secondary_family_ = AF_UNSPEC;
            next_family_ = planned_next;
            v4_fallback_ = planned_fallback;
            parallel_ = false;
            poll_primary_next_ = true;
            clear_stored_results();
            if (next_family_ != AF_UNSPEC && !v4_fallback_) {
                int secondary = rin_dns_getaddrinfo_batch_begin(
                    host_, service_, next_family_, socket_type_, protocol_,
                    wire_flags_);
                if (secondary > 0) {
                    secondary_handle_ = static_cast<unsigned int>(secondary);
                    secondary_family_ = next_family_;
                    next_family_ = AF_UNSPEC;
                    parallel_ = true;
                }
            }
            addrconfig_snapshot_ = snapshot;
            ec.clear();
            return true;
        }

        bool refresh_addrconfig(error_code& ec) {
            RinNetPrimaryInfo observed{};
            int ipv4_configured = 0;
            int ipv6_configured = 0;
            if (!watch_addrconfig_) return true;
            if (!basic_resolver::async_addrconfig_snapshot(
                    ipv4_configured, ipv6_configured, &observed, ec)) {
                if (!cancel_parallel_remaining())
                    ec = make_error_code(errc::io_error);
                state_ = resolver_async_state::failed;
                return false;
            }
            if (basic_resolver::async_addrconfig_equal(
                    addrconfig_snapshot_, observed)) {
                return true;
            }
            if (!cancel_parallel_remaining()) {
                state_ = resolver_async_state::failed;
                ec = make_error_code(errc::io_error);
                return false;
            }
            /* A changed interface snapshot invalidates every private family
             * result.  Start a new composition; no stale endpoint can be
             * mixed with the next configuration generation. */
            (void)restart_for_addrconfig(observed, ipv4_configured,
                                         ipv6_configured, ec);
            return false;
        }

        bool has_active_handle() const noexcept {
            return handle_ != 0u || secondary_handle_ != 0u;
        }

        bool cancel_parallel_remaining() noexcept {
            unsigned int primary = handle_;
            unsigned int secondary = secondary_handle_;
            bool okay = true;
            handle_ = 0u;
            secondary_handle_ = 0u;
            if (primary != 0u && rin_dns_resolve_cancel(primary) != 0)
                okay = false;
            if (secondary != 0u && rin_dns_resolve_cancel(secondary) != 0)
                okay = false;
            return okay;
        }

        static resolver_async_state terminal_status(unsigned int status,
                                                     error_code& ec) {
            if (status == RIN_DNS_STATUS_TIMED_OUT) {
                ec = make_error_code(errc::timed_out);
                return resolver_async_state::failed;
            }
            if (status == RIN_DNS_STATUS_CANCELLED) {
                ec = make_error_code(errc::operation_canceled);
                return resolver_async_state::cancelled;
            }
            if (status == RIN_DNS_STATUS_CONFIG_MISSING) {
                ec = make_error_code(errc::network_unreachable);
                return resolver_async_state::failed;
            }
            if (status == RIN_DNS_STATUS_SEND_BLOCKED) {
                ec = make_error_code(errc::resource_unavailable_try_again);
                return resolver_async_state::failed;
            }
            if (status == RIN_DNS_STATUS_FAILED) {
                ec = make_error_code(errc::io_error);
                return resolver_async_state::failed;
            }
            ec = make_error_code(errc::protocol_error);
            return resolver_async_state::failed;
        }

        resolver_async_state abort_parallel(resolver_async_state terminal,
                                            const error_code& failure,
                                            error_code& ec) {
            state_ = cancel_parallel_remaining() ? terminal
                                                 : resolver_async_state::failed;
            ec = state_ == terminal ? failure : make_error_code(errc::io_error);
            return state_;
        }

        resolver_async_state poll_parallel(results_type& output,
                                           error_code& ec) {
            bool primary = poll_primary_next_;
            poll_primary_next_ = !poll_primary_next_;
            if (primary && handle_ == 0u) primary = false;
            if (!primary && secondary_handle_ == 0u) primary = true;

            unsigned int active_handle =
                primary ? handle_ : secondary_handle_;
            int active_family =
                primary ? wire_family_ : secondary_family_;
            RinDnsGetAddrInfoBatchResult native{};
            int status = rin_dns_getaddrinfo_batch_poll(active_handle, &native);
            if (status == 1) {
                ec.clear();
                return state_;
            }
            if (status != 0) {
                return abort_parallel(resolver_async_state::failed,
                                      make_error_code(errc::io_error), ec);
            }

            if (primary)
                handle_ = 0u;
            else
                secondary_handle_ = 0u;
            /* Do not combine a just-arrived family result with a peer from
             * a newer interface generation. */
            if (!refresh_addrconfig(ec)) return state_;
            if (native.reserved0 != 0u || native.reserved1 != 0u) {
                return abort_parallel(resolver_async_state::failed,
                                      make_error_code(errc::protocol_error), ec);
            }
            if (native.status != RIN_DNS_STATUS_DONE) {
                if (native.status == RIN_DNS_STATUS_NOT_FOUND) {
                    if (!has_active_handle())
                        return complete_stored_results(output, ec);
                    ec.clear();
                    return state_;
                }
                error_code terminal_error;
                resolver_async_state terminal =
                    terminal_status(native.status, terminal_error);
                return abort_parallel(terminal, terminal_error, ec);
            }

            if (!basic_resolver::validate_async_batch_shape(native, ec)) {
                error_code validation_error = ec;
                return abort_parallel(resolver_async_state::failed,
                                      validation_error, ec);
            }
            if (primary) {
                first_result_ = native;
                first_result_family_ = active_family;
                first_result_valid_ = true;
            } else {
                second_result_ = native;
                second_result_family_ = active_family;
                second_result_valid_ = true;
            }
            if (!has_active_handle()) return complete_stored_results(output, ec);
            ec.clear();
            return state_;
        }

        resolver_async_state complete_stored_results(results_type& output,
                                                      error_code& ec) {
            const RinDnsGetAddrInfoBatchResult* natives[2] = {nullptr, nullptr};
            int native_families[2] = {AF_UNSPEC, AF_UNSPEC};
            size_t count = 0u;
            if (first_result_valid_) {
                natives[count] = &first_result_;
                native_families[count++] = first_result_family_;
            }
            if (second_result_valid_) {
                natives[count] = &second_result_;
                native_families[count++] = second_result_family_;
            }
            return complete_results(natives, native_families, count, output, ec);
        }

        resolver_async_state complete_results(
            const RinDnsGetAddrInfoBatchResult* const* natives,
            const int* native_families, size_t count, results_type& output,
            error_code& ec) {
            constexpr size_t maximum_entries =
                RIN_DNS_GETADDRINFO_MAX_RESULTS * 2u;
            const RinDnsGetAddrInfoResult* entries[maximum_entries];
            int entry_families[maximum_entries];
            size_t entry_count = 0u;
            if (count == 0u) {
                state_ = resolver_async_state::failed;
                ec = make_error_code(errc::address_not_available);
                return state_;
            }
            if (count > 2u) {
                state_ = resolver_async_state::failed;
                ec = make_error_code(errc::protocol_error);
                return state_;
            }
            for (size_t batch_index = 0u; batch_index < count; ++batch_index) {
                if (!natives[batch_index] ||
                    !basic_resolver::validate_async_batch_shape(
                        *natives[batch_index], ec)) {
                    state_ = resolver_async_state::failed;
                    return state_;
                }
                for (size_t entry_index = 0u;
                     entry_index < natives[batch_index]->count;
                     ++entry_index) {
                    entries[entry_count] =
                        &natives[batch_index]->entries[entry_index];
                    entry_families[entry_count++] =
                        native_families[batch_index];
                }
            }
            if (entry_count == 0u) {
                state_ = resolver_async_state::failed;
                ec = make_error_code(errc::address_not_available);
                return state_;
            }
            results_type candidate;
            if (!basic_resolver::publish_async_results(
                    allocator_, entries, entry_families, entry_count, family_,
                    socket_type_, protocol_, flags_, wire_flags_, port_, host_,
                    host_length_, service_, service_length_, candidate, ec)) {
                state_ = resolver_async_state::failed;
                return state_;
            }
            output = static_cast<results_type&&>(candidate);
            state_ = resolver_async_state::completed;
            ec.clear();
            return state_;
        }

        resolver_async_state complete(
            const RinDnsGetAddrInfoBatchResult* current, int current_family,
            results_type& output, error_code& ec) {
            const RinDnsGetAddrInfoBatchResult* natives[2] = {nullptr, nullptr};
            int native_families[2] = {AF_UNSPEC, AF_UNSPEC};
            size_t count = 0u;
            if (first_result_valid_) {
                natives[count] = &first_result_;
                native_families[count++] = first_result_family_;
            }
            if (current) {
                natives[count] = current;
                native_families[count++] = current_family;
            }
            return complete_results(natives, native_families, count, output, ec);
        }

        allocator_type allocator_;
        unsigned int handle_;
        unsigned int secondary_handle_;
        unsigned short port_;
        int family_;
        int wire_family_;
        int secondary_family_;
        int socket_type_;
        int protocol_;
        unsigned int flags_;
        unsigned int wire_flags_;
        bool v4_fallback_;
        int next_family_;
        RinDnsGetAddrInfoBatchResult first_result_;
        int first_result_family_;
        bool first_result_valid_;
        RinDnsGetAddrInfoBatchResult second_result_;
        int second_result_family_;
        bool second_result_valid_;
        bool parallel_;
        bool poll_primary_next_;
        bool completion_dispatched_;
        bool watch_addrconfig_;
        RinNetPrimaryInfo addrconfig_snapshot_;
        size_t host_length_;
        size_t service_length_;
        char host_[RIN_DNS_NAME_MAX + 1u];
        char service_[6];
        resolver_async_state state_;

        friend class basic_resolver;
    };

    basic_resolver() : allocator_() {}
    explicit basic_resolver(const allocator_type& allocator)
        : allocator_(allocator) {}

    allocator_type get_allocator() const { return allocator_; }

    results_type resolve(const string& host, const string& service,
                         error_code& ec) {
        return resolve_impl(host, service, AF_UNSPEC, resolver_flags::none, ec);
    }

    results_type resolve(const string& host, const string& service) {
        error_code ec;
        return resolve(host, service, ec);
    }

    results_type resolve(const string& host, const string& service,
                         resolver_flags flags, error_code& ec) {
        return resolve_impl(host, service, AF_UNSPEC, flags, ec);
    }

    results_type resolve(const string& host, const string& service,
                         resolver_flags flags) {
        error_code ec;
        return resolve(host, service, flags, ec);
    }

    results_type resolve(const protocol_type& protocol, const string& host,
                         const string& service, error_code& ec) {
        return resolve_impl(host, service, protocol.family(),
                            resolver_flags::none, ec);
    }

    results_type resolve(const protocol_type& protocol, const string& host,
                         const string& service) {
        error_code ec;
        return resolve(protocol, host, service, ec);
    }

    results_type resolve(const protocol_type& protocol, const string& host,
                         const string& service, resolver_flags flags,
                         error_code& ec) {
        return resolve_impl(host, service, protocol.family(), flags, ec);
    }

    results_type resolve(const protocol_type& protocol, const string& host,
                         const string& service, resolver_flags flags) {
        error_code ec;
        return resolve(protocol, host, service, flags, ec);
    }

    results_type resolve(const string& host, unsigned short port,
                         error_code& ec) {
        char service[6];
        format_service(port, service);
        return resolve(host, string(service), ec);
    }

    results_type resolve(const string& host, unsigned short port,
                         resolver_flags flags, error_code& ec) {
        char service[6];
        format_service(port, service);
        return resolve(host, string(service), flags, ec);
    }

    results_type resolve(const string& host, unsigned short port,
                         resolver_flags flags) {
        error_code ec;
        return resolve(host, port, flags, ec);
    }

    results_type resolve(const string& host, unsigned short port) {
        error_code ec;
        return resolve(host, port, ec);
    }

    results_type resolve(const protocol_type& protocol, const string& host,
                         unsigned short port, error_code& ec) {
        char service[6];
        format_service(port, service);
        return resolve(protocol, host, string(service), ec);
    }

    results_type resolve(const protocol_type& protocol, const string& host,
                         unsigned short port, resolver_flags flags,
                         error_code& ec) {
        char service[6];
        format_service(port, service);
        return resolve(protocol, host, string(service), flags, ec);
    }

    results_type resolve(const protocol_type& protocol, const string& host,
                         unsigned short port, resolver_flags flags) {
        error_code ec;
        return resolve(protocol, host, port, flags, ec);
    }

    results_type resolve(const protocol_type& protocol, const string& host,
                         unsigned short port) {
        error_code ec;
        return resolve(protocol, host, port, ec);
    }

    async_operation async_resolve(const protocol_type& protocol,
                                  const string& host, unsigned short port,
                                  error_code& ec) {
        return async_resolve(protocol, host, port, resolver_flags::none, ec);
    }

    async_operation async_resolve(const string& host, unsigned short port,
                                  error_code& ec) {
        return async_resolve(host, port, resolver_flags::none, ec);
    }

    async_operation async_resolve(const string& host, unsigned short port,
                                  resolver_flags flags, error_code& ec) {
        return async_resolve_impl(AF_UNSPEC, Protocol::v4().type(),
                                  Protocol::v4().protocol(), host, port,
                                  flags, ec);
    }

    async_operation async_resolve(const protocol_type& protocol,
                                  const string& host, unsigned short port,
                                  resolver_flags flags, error_code& ec) {
        return async_resolve_impl(protocol.family(), protocol.type(),
                                  protocol.protocol(), host, port, flags, ec);
    }

private:
    async_operation async_resolve_impl(
        int family, int socket_type, int protocol, const string& host,
        unsigned short port, resolver_flags flags, error_code& ec) {
        char service[6];
        format_service(port, service);
        unsigned int raw_flags = static_cast<unsigned int>(flags);
        unsigned int wire_flags = raw_flags & ~static_cast<unsigned int>(
            AI_ADDRCONFIG | AI_V4MAPPED | AI_ALL);
        int wire_family = family == AF_UNSPEC ? AF_INET6 : family;
        int next_family = family == AF_UNSPEC ? AF_INET : AF_UNSPEC;
        bool all_matching = (raw_flags & AI_ALL) != 0u;
        bool v4_fallback = (raw_flags & AI_V4MAPPED) != 0u && !all_matching;
        bool watch_addrconfig = false;
        RinNetPrimaryInfo addrconfig_snapshot{};
        if ((family != AF_UNSPEC && family != AF_INET && family != AF_INET6) ||
            !async_resolver_flags_valid(raw_flags) ||
            (all_matching && (raw_flags & AI_V4MAPPED) == 0u) ||
            (((raw_flags & (AI_ALL | AI_V4MAPPED)) != 0u) &&
             family != AF_INET6) ||
            host.size() > RIN_DNS_NAME_MAX ||
            (host.empty() && (raw_flags & AI_CANONNAME) != 0u) ||
            contains_nul(host)) {
            ec = make_error_code(errc::invalid_argument);
            return async_operation(allocator_, 0u, port, family,
                                   wire_family, socket_type,
                                   protocol, raw_flags, wire_flags,
                                   false, "", 0u,
                                   service, bounded_text_length(service));
        }
        if ((raw_flags & AI_ADDRCONFIG) != 0u) {
            int ipv4_configured = 0;
            int ipv6_configured = 0;
            if (!async_addrconfig_snapshot(
                    ipv4_configured, ipv6_configured, &addrconfig_snapshot, ec)) {
                return async_operation(
                    allocator_, 0u, port, family, wire_family,
                    socket_type, protocol, raw_flags,
                    wire_flags, false, host.c_str(), host.size(), service,
                    bounded_text_length(service));
            }
            watch_addrconfig = true;
            if (!async_addrconfig_plan(
                    family, raw_flags, ipv4_configured, ipv6_configured,
                    wire_family, next_family, v4_fallback, ec)) {
                return async_operation(
                    allocator_, 0u, port, family, wire_family,
                    socket_type, protocol, raw_flags,
                    wire_flags, false, host.c_str(), host.size(), service,
                    bounded_text_length(service));
            }
        }
        if (all_matching && (raw_flags & AI_ADDRCONFIG) == 0u)
            next_family = AF_INET;
        int handle = rin_dns_getaddrinfo_batch_begin(
            host.c_str(), service, wire_family, socket_type,
            protocol, wire_flags);
        if (handle <= 0) {
            ec = make_error_code(errc::io_error);
            return async_operation(allocator_, 0u, port, family,
                                   wire_family, socket_type,
                                   protocol, raw_flags, wire_flags,
                                   false, "", 0u,
                                   service, bounded_text_length(service));
        }
        unsigned int secondary_handle = 0u;
        int secondary_family = AF_UNSPEC;
        if (next_family != AF_UNSPEC && !v4_fallback) {
            int started = rin_dns_getaddrinfo_batch_begin(
                host.c_str(), service, next_family, socket_type,
                protocol, wire_flags);
            if (started > 0) {
                secondary_handle = static_cast<unsigned int>(started);
                secondary_family = next_family;
                next_family = AF_UNSPEC;
            }
        }
        ec.clear();
        return async_operation(allocator_, static_cast<unsigned int>(handle),
                               port, family, wire_family,
                               socket_type, protocol, raw_flags,
                               wire_flags, v4_fallback,
                               host.c_str(), host.size(), service,
                               bounded_text_length(service), next_family,
                               secondary_handle, secondary_family,
                               watch_addrconfig,
                               watch_addrconfig ? &addrconfig_snapshot : nullptr);
    }
    static bool async_resolver_flags_valid(unsigned int flags) noexcept {
        return (flags & ~static_cast<unsigned int>(
            AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST |
            AI_NUMERICSERV | AI_V4MAPPED | AI_ALL | AI_ADDRCONFIG)) == 0u;
    }

    static bool async_addrconfig_snapshot(int& ipv4_configured,
                                          int& ipv6_configured,
                                          RinNetPrimaryInfo* snapshot,
                                          error_code& ec) noexcept {
        RinNetPrimaryInfo info{};
        if (snapshot) *snapshot = {};
        if (rin_net_get_primary_info(&info) != 0) {
            ec = make_error_code(errc::io_error);
            return false;
        }
        if (rin_netif_addrconfig_decode(
                &info, &ipv4_configured, &ipv6_configured) != 0) {
            ec = make_error_code(errc::protocol_error);
            return false;
        }
        if (snapshot) *snapshot = info;
        ec.clear();
        return true;
    }

    static size_t bounded_text_length(const char* text) noexcept {
        size_t length = 0u;
        while (text[length] != '\0') ++length;
        return length;
    }

    static void format_service(unsigned short port, char (&service)[6]) {
        int n = 0;
        unsigned int p = port;
        if (p >= 10000) { service[n++] = '0' + p / 10000; p %= 10000; }
        if (p >= 1000 || n > 0) { service[n++] = '0' + p / 1000; p %= 1000; }
        if (p >= 100 || n > 0) { service[n++] = '0' + p / 100; p %= 100; }
        if (p >= 10 || n > 0) { service[n++] = '0' + p / 10; p %= 10; }
        service[n++] = '0' + p;
        service[n] = '\0';
    }

    results_type resolve_impl(const string& host, const string& service,
                              int family, resolver_flags flags,
                              error_code& ec) {
        results_type results;
        constexpr unsigned int allowed_flags =
            AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST | AI_NUMERICSERV |
            AI_V4MAPPED | AI_ALL | AI_ADDRCONFIG;
        unsigned int raw_flags = static_cast<unsigned int>(flags);
        if ((raw_flags & ~allowed_flags) != 0u ||
            ((raw_flags & AI_ALL) != 0u &&
             (raw_flags & AI_V4MAPPED) == 0u) ||
            (((raw_flags & (AI_ALL | AI_V4MAPPED)) != 0u) &&
             family != AF_INET6) ||
            host.size() > 1024u || service.size() > 31u ||
            contains_nul(host) || contains_nul(service)) {
            ec = make_error_code(errc::invalid_argument);
            return results;
        }
        struct addrinfo hints{};
        hints.ai_flags = static_cast<int>(raw_flags);
        hints.ai_family = family;
        hints.ai_socktype = Protocol::v4().type();
        hints.ai_protocol = Protocol::v4().protocol();

        struct addrinfo* head = nullptr;
        const char* node = ((raw_flags & AI_PASSIVE) != 0u && host.empty())
            ? nullptr : host.c_str();
        int status = getaddrinfo(node, service.c_str(), &hints, &head);
        if (status != 0) {
            if (head) freeaddrinfo(head);
            ec.assign(status, system_category());
            return results;
        }

        /* A cyclic ownership list cannot be passed back to a conventional
         * freeaddrinfo implementation safely. Reject it before allocation. */
        const struct addrinfo* slow = head;
        const struct addrinfo* fast = head;
        while (fast && fast->ai_next) {
            slow = slow->ai_next;
            fast = fast->ai_next->ai_next;
            if (slow == fast) {
                ec = make_error_code(errc::protocol_error);
                return results;
            }
        }

        size_t visited = 0;
        size_t accepted = 0;
        const char* canonical_name = nullptr;
        size_t canonical_length = 0;
        error_code candidate_error;
        for (struct addrinfo* current = head; current; current = current->ai_next) {
            if (visited == static_cast<size_t>(-1)) {
                if (head) freeaddrinfo(head);
                ec = make_error_code(errc::value_too_large);
                return results;
            }
            ++visited;
            endpoint_type candidate;
            candidate_error.clear();
            if (parse_result(current, family, hints, candidate,
                             candidate_error)) {
                if (accepted == static_cast<size_t>(-1)) {
                    if (head) freeaddrinfo(head);
                    ec = make_error_code(errc::value_too_large);
                    return results;
                }
                ++accepted;
                if ((raw_flags & AI_CANONNAME) != 0u &&
                    current->ai_canonname) {
                    size_t length = 0;
                    if (!bounded_length(current->ai_canonname, 1024u,
                                        length) || length == 0u ||
                        (canonical_name &&
                         !text_equal(canonical_name, canonical_length,
                                     current->ai_canonname, length))) {
                        if (head) freeaddrinfo(head);
                        ec = make_error_code(errc::protocol_error);
                        return results;
                    }
                    canonical_name = current->ai_canonname;
                    canonical_length = length;
                }
            } else if (candidate_error) {
                if (head) freeaddrinfo(head);
                ec = make_error_code(errc::protocol_error);
                return results;
            }
        }
        if (accepted == 0) {
            if (head) freeaddrinfo(head);
            ec = make_error_code(errc::address_not_available);
            return results;
        }
        if ((raw_flags & AI_CANONNAME) != 0u && !canonical_name) {
            if (head) freeaddrinfo(head);
            ec = make_error_code(errc::protocol_error);
            return results;
        }
        const char* stored_host = canonical_name ? canonical_name : host.c_str();
        size_t stored_host_length = canonical_name ? canonical_length : host.size();
        size_t name_bytes = stored_host_length + 1u;
        if (service.size() == static_cast<size_t>(-1) ||
            name_bytes > static_cast<size_t>(-1) - service.size() - 1u) {
            if (head) freeaddrinfo(head);
            ec = make_error_code(errc::value_too_large);
            return results;
        }
        name_bytes += service.size() + 1u;
        if (accepted > static_cast<size_t>(-1) / name_bytes) {
            if (head) freeaddrinfo(head);
            ec = make_error_code(errc::value_too_large);
            return results;
        }
        size_t string_bytes = accepted * name_bytes;
        if (!results.allocate(accepted, string_bytes, allocator_, ec)) {
            if (head) freeaddrinfo(head);
            return results;
        }

        size_t output_index = 0;
        char* text_output = reinterpret_cast<char*>(
            results.storage_->entries + accepted);
        for (struct addrinfo* current = head; current; current = current->ai_next) {
            endpoint_type candidate;
            candidate_error.clear();
            if (parse_result(current, family, hints, candidate,
                             candidate_error)) {
                resolver_entry& entry =
                    results.storage_->entries[output_index++];
                entry.endpoint_ = candidate;
                entry.host_name_ = text_output;
                copy_text(text_output, stored_host, stored_host_length);
                text_output += stored_host_length + 1u;
                entry.service_name_ = text_output;
                copy_text(text_output, service.c_str(), service.size());
                text_output += service.size() + 1u;
            }
        }
        if (head) freeaddrinfo(head);
        if (output_index != accepted) {
            results = results_type();
            ec = make_error_code(errc::protocol_error);
            return results;
        }
        ec.clear();
        return results;
    }

    static bool contains_nul(const string& text) noexcept {
        for (size_t index = 0; index < text.size(); ++index)
            if (text[index] == '\0') return true;
        return false;
    }

    static bool bounded_length(const char* text, size_t maximum,
                               size_t& length) noexcept {
        if (!text) return false;
        for (size_t index = 0; index <= maximum; ++index) {
            if (text[index] == '\0') {
                length = index;
                return true;
            }
        }
        return false;
    }

    static bool text_equal(const char* left, size_t left_length,
                           const char* right, size_t right_length) noexcept {
        if (left_length != right_length) return false;
        for (size_t index = 0; index < left_length; ++index)
            if (left[index] != right[index]) return false;
        return true;
    }

    static void copy_text(char* output, const char* input,
                          size_t length) noexcept {
        for (size_t index = 0; index < length; ++index)
            output[index] = input[index];
        output[length] = '\0';
    }

    static bool validate_async_batch_shape(
        const RinDnsGetAddrInfoBatchResult& native,
        error_code& ec) noexcept {
        if (native.status != RIN_DNS_STATUS_DONE ||
            native.reserved0 != 0u || native.reserved1 != 0u ||
            native.count == 0u ||
            native.count > RIN_DNS_GETADDRINFO_MAX_RESULTS) {
            ec = make_error_code(errc::protocol_error);
            return false;
        }
        ec.clear();
        return true;
    }

    static bool async_addrconfig_equal(const RinNetPrimaryInfo& left,
                                       const RinNetPrimaryInfo& right) noexcept {
        const unsigned char* left_bytes =
            reinterpret_cast<const unsigned char*>(&left);
        const unsigned char* right_bytes =
            reinterpret_cast<const unsigned char*>(&right);
        for (size_t index = 0u; index < sizeof(RinNetPrimaryInfo); ++index) {
            if (left_bytes[index] != right_bytes[index]) return false;
        }
        return true;
    }

    static bool async_addrconfig_plan(int family, unsigned int raw_flags,
                                      int ipv4_configured, int ipv6_configured,
                                      int& wire_family, int& next_family,
                                      bool& v4_fallback,
                                      error_code& ec) noexcept {
        bool all_matching = (raw_flags & AI_ALL) != 0u;
        wire_family = family == AF_UNSPEC ? AF_INET6 : family;
        next_family = family == AF_UNSPEC ? AF_INET : AF_UNSPEC;
        v4_fallback = (raw_flags & AI_V4MAPPED) != 0u && !all_matching;
        if (family == AF_UNSPEC) {
            if (!ipv4_configured && !ipv6_configured) {
                ec = make_error_code(errc::address_not_available);
                return false;
            }
            wire_family = ipv6_configured ? AF_INET6 : AF_INET;
            next_family = ipv6_configured && ipv4_configured
                ? AF_INET : AF_UNSPEC;
        } else if (all_matching) {
            if (ipv6_configured) {
                wire_family = AF_INET6;
                next_family = ipv4_configured ? AF_INET : AF_UNSPEC;
            } else if (ipv4_configured) {
                wire_family = AF_INET;
                next_family = AF_UNSPEC;
            } else {
                ec = make_error_code(errc::address_not_available);
                return false;
            }
        } else if (v4_fallback) {
            if (!ipv6_configured && ipv4_configured) {
                wire_family = AF_INET;
                v4_fallback = false;
            } else if (ipv6_configured) {
                v4_fallback = ipv4_configured != 0;
            } else {
                ec = make_error_code(errc::address_not_available);
                return false;
            }
        } else if ((family == AF_INET && !ipv4_configured) ||
                   (family == AF_INET6 && !ipv6_configured)) {
            ec = make_error_code(errc::address_not_available);
            return false;
        }
        ec.clear();
        return true;
    }

    static bool validate_async_result(
        const RinDnsGetAddrInfoResult& native, int family, int wire_family,
        int socket_type, int protocol, unsigned int flags,
        unsigned int wire_flags, unsigned short port,
        const char* host, size_t host_length,
        const char* service, size_t service_length,
        endpoint_type& endpoint, const char*& stored_host,
        size_t& stored_host_length, error_code& ec) {
        if (!host || !service || host_length > RIN_DNS_NAME_MAX ||
            service_length > 5u || host[host_length] != '\0' ||
            service[service_length] != '\0' ||
            native.status != RIN_DNS_STATUS_DONE ||
            native.reserved0 != 0u || native.reserved1 != 0u ||
            native.ai_family != wire_family ||
            native.ai_socktype != socket_type ||
            native.ai_protocol != protocol ||
            static_cast<unsigned int>(native.ai_flags) != wire_flags ||
            native.port != htons(port) ||
            (wire_family == AF_INET &&
             native.ai_addrlen != sizeof(sockaddr_in)) ||
            (wire_family == AF_INET6 &&
             native.ai_addrlen != sizeof(sockaddr_in6)) ||
            (family != AF_UNSPEC && family != wire_family &&
             !(family == AF_INET6 && wire_family == AF_INET))) {
            ec = make_error_code(errc::protocol_error);
            return false;
        }
        if (wire_family == AF_INET) {
            for (unsigned int index = 4u; index < 16u; ++index) {
                if (native.addr[index] != 0u) {
                    ec = make_error_code(errc::protocol_error);
                    return false;
                }
            }
        }
        stored_host = host;
        stored_host_length = host_length;
        if ((flags & AI_CANONNAME) != 0u) {
            if (native.canon_len == 0u || native.canon_len > RIN_DNS_NAME_MAX ||
                native.canonical_name[native.canon_len] != '\0') {
                ec = make_error_code(errc::protocol_error);
                return false;
            }
            for (size_t index = 0; index < native.canon_len; ++index) {
                if (native.canonical_name[index] == '\0') {
                    ec = make_error_code(errc::protocol_error);
                    return false;
                }
            }
            stored_host = native.canonical_name;
            stored_host_length = native.canon_len;
        } else if (native.canon_len != 0u || native.canonical_name[0] != '\0') {
            ec = make_error_code(errc::protocol_error);
            return false;
        }
        if (family == AF_INET ||
            (family == AF_UNSPEC && wire_family == AF_INET)) {
            address_v4::bytes_type address_bytes{{
                native.addr[0], native.addr[1], native.addr[2], native.addr[3]
            }};
            endpoint = endpoint_type(address(address_v4(address_bytes)), port);
        } else {
            address_v6::bytes_type address_bytes{};
            if (wire_family == AF_INET) {
                address_bytes[10] = 0xffu;
                address_bytes[11] = 0xffu;
                for (unsigned int index = 0; index < 4u; ++index)
                    address_bytes[index + 12u] = native.addr[index];
            } else {
                for (unsigned int index = 0; index < 16u; ++index)
                    address_bytes[index] = native.addr[index];
            }
            endpoint = endpoint_type(
                address(address_v6(address_bytes, 0u)), port);
        }
        ec.clear();
        return true;
    }

    static bool publish_async_results(
        const allocator_type& allocator,
        const RinDnsGetAddrInfoResult* const* natives,
        const int* wire_families, size_t count, int family,
        int socket_type, int protocol, unsigned int flags,
        unsigned int wire_flags, unsigned short port,
        const char* host, size_t host_length,
        const char* service, size_t service_length, results_type& output,
        error_code& ec) {
        constexpr size_t maximum_entries =
            RIN_DNS_GETADDRINFO_MAX_RESULTS * 2u;
        endpoint_type endpoints[maximum_entries];
        const char* stored_hosts[maximum_entries] = {};
        size_t stored_host_lengths[maximum_entries] = {};
        size_t string_bytes = 0u;
        if (!natives || !wire_families || count == 0u ||
            count > maximum_entries) {
            ec = make_error_code(errc::protocol_error);
            return false;
        }
        for (size_t index = 0u; index < count; ++index) {
            if (!natives[index] ||
                !validate_async_result(
                    *natives[index], family, wire_families[index],
                    socket_type, protocol, flags, wire_flags, port, host,
                    host_length, service, service_length, endpoints[index],
                    stored_hosts[index], stored_host_lengths[index], ec))
                return false;
            if (index != 0u && (flags & AI_CANONNAME) != 0u &&
                !text_equal(stored_hosts[0], stored_host_lengths[0],
                            stored_hosts[index], stored_host_lengths[index])) {
                ec = make_error_code(errc::protocol_error);
                return false;
            }
            if (stored_host_lengths[index] > static_cast<size_t>(-1) -
                    service_length - 2u) {
                ec = make_error_code(errc::value_too_large);
                return false;
            }
            size_t entry_bytes = stored_host_lengths[index] +
                                 service_length + 2u;
            if (string_bytes > static_cast<size_t>(-1) - entry_bytes) {
                ec = make_error_code(errc::value_too_large);
                return false;
            }
            string_bytes += entry_bytes;
        }
        results_type candidate;
        if (!candidate.allocate(count, string_bytes, allocator, ec))
            return false;
        char* text_output = reinterpret_cast<char*>(
            candidate.storage_->entries + count);
        for (size_t index = 0u; index < count; ++index) {
            resolver_entry& entry = candidate.storage_->entries[index];
            entry.endpoint_ = endpoints[index];
            entry.host_name_ = text_output;
            copy_text(text_output, stored_hosts[index],
                      stored_host_lengths[index]);
            text_output += stored_host_lengths[index] + 1u;
            entry.service_name_ = text_output;
            copy_text(text_output, service, service_length);
            text_output += service_length + 1u;
        }
        output = static_cast<results_type&&>(candidate);
        ec.clear();
        return true;
    }

    static bool parse_result(const struct addrinfo* current, int family,
                             const struct addrinfo& hints,
                             endpoint_type& endpoint,
                             error_code& ec) noexcept {
        if (current->ai_family != AF_INET && current->ai_family != AF_INET6) {
            ec.clear();
            return false;
        }
        if (family != AF_UNSPEC && current->ai_family != family) {
            ec.clear();
            return false;
        }
        if ((current->ai_socktype != 0 &&
             current->ai_socktype != hints.ai_socktype) ||
            (current->ai_protocol != 0 &&
             current->ai_protocol != hints.ai_protocol)) {
            ec.clear();
            return false;
        }
        unsigned int expected = current->ai_family == AF_INET
            ? static_cast<unsigned int>(sizeof(struct sockaddr_in))
            : static_cast<unsigned int>(sizeof(struct sockaddr_in6));
        if (!current->ai_addr || current->ai_addrlen != expected) {
            ec = make_error_code(errc::protocol_error);
            return false;
        }

        ip::detail::endpoint_storage storage{};
        const unsigned char* source =
            reinterpret_cast<const unsigned char*>(current->ai_addr);
        unsigned char* target = reinterpret_cast<unsigned char*>(&storage);
        for (unsigned int index = 0; index < expected; ++index)
            target[index] = source[index];
        if (!ip::detail::endpoint_from_native(storage, expected, endpoint, ec)) {
            ec = make_error_code(errc::protocol_error);
            return false;
        }
        ec.clear();
        return true;
    }

    allocator_type allocator_;
};

using tcp_resolver = basic_resolver<tcp>;
using udp_resolver = basic_resolver<udp>;

/* An unconstrained getaddrinfo request can legitimately publish both TCP and
 * UDP entries.  Those entries cannot be represented by basic_resolver<T>,
 * whose endpoint has one fixed protocol. */
#include "net_multi_resolver.h"

} /* namespace ip */

} /* namespace net */
} /* namespace std */

#endif /* defined(__cplusplus) && __cplusplus >= 201703L */
#endif /* RINCXX_NET_H */
