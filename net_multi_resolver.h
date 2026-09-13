/* RinOS C++ <net> multi-socket resolver extension. */
#ifndef RINCXX_NET_MULTI_RESOLVER_H
#define RINCXX_NET_MULTI_RESOLVER_H

/* This header is included by net.h after the IP endpoint and resolver types
 * have been declared, while std::net::ip is open. */

class resolver_socket_endpoint {
public:
    resolver_socket_endpoint() noexcept
        : address_(), port_(0u), socket_type_(0), protocol_(0) {}

    resolver_socket_endpoint(const ip::address& value, unsigned short port,
                             int socket_type, int protocol) noexcept
        : address_(value), port_(port), socket_type_(socket_type),
          protocol_(protocol) {}

    ip::address address() const noexcept { return address_; }
    unsigned short port() const noexcept { return port_; }
    int socket_type() const noexcept { return socket_type_; }
    int protocol() const noexcept { return protocol_; }

    friend bool operator==(const resolver_socket_endpoint& left,
                           const resolver_socket_endpoint& right) noexcept {
        return left.address_ == right.address_ && left.port_ == right.port_ &&
               left.socket_type_ == right.socket_type_ &&
               left.protocol_ == right.protocol_;
    }
    friend bool operator!=(const resolver_socket_endpoint& left,
                           const resolver_socket_endpoint& right) noexcept {
        return !(left == right);
    }

private:
    ip::address address_;
    unsigned short port_;
    int socket_type_;
    int protocol_;
};

/* multi_resolver deliberately has a distinct result type.  Calling the
 * protocol-typed basic_resolver remains the smaller, one-transport API. */
class multi_resolver {
public:
    class resolver_entry {
    public:
        resolver_entry() noexcept
            : endpoint_(), host_name_(nullptr), service_name_(nullptr) {}

        const resolver_socket_endpoint& endpoint() const noexcept {
            return endpoint_;
        }
        const char* host_name() const noexcept {
            return host_name_ ? host_name_ : "";
        }
        const char* service_name() const noexcept {
            return service_name_ ? service_name_ : "";
        }

    private:
        resolver_socket_endpoint endpoint_;
        const char* host_name_;
        const char* service_name_;
        friend class multi_resolver;
    };

    class results_type {
    public:
        using value_type = resolver_entry;
        using const_iterator = const resolver_entry*;

        results_type() noexcept : storage_(nullptr) {}
        results_type(const results_type& other) noexcept : storage_(other.storage_) {
            retain(storage_);
        }
        results_type(results_type&& other) noexcept : storage_(other.storage_) {
            other.storage_ = nullptr;
        }
        ~results_type() { release(); }

        results_type& operator=(const results_type& other) noexcept {
            if (this == &other) return *this;
            retain(other.storage_);
            release();
            storage_ = other.storage_;
            return *this;
        }
        results_type& operator=(results_type&& other) noexcept {
            if (this == &other) return *this;
            release();
            storage_ = other.storage_;
            other.storage_ = nullptr;
            return *this;
        }

        const_iterator begin() const noexcept {
            return storage_ ? storage_->entries : nullptr;
        }
        const_iterator end() const noexcept {
            return storage_ ? storage_->entries + storage_->size : nullptr;
        }
        bool empty() const noexcept { return !storage_ || storage_->size == 0u; }
        size_t size() const noexcept { return storage_ ? storage_->size : 0u; }
        const resolver_entry& operator[](size_t index) const {
            if (!storage_ || index >= storage_->size)
                rin_panic("net: multi resolver result out of range");
            return storage_->entries[index];
        }

    private:
        struct shared_results {
            size_t allocation_units;
            size_t references;
            size_t size;
            resolver_entry entries[1];

            shared_results(size_t units, size_t count)
                : allocation_units(units), references(1u), size(count),
                  entries() {}
        };

        using storage_allocator =
            typename allocator_traits<allocator<unsigned char>>::template rebind_alloc<shared_results>;
        using storage_traits = allocator_traits<storage_allocator>;
        static_assert(is_same<typename storage_traits::pointer,
                              shared_results*>::value,
                      "net: multi resolver allocator requires raw pointers");

        bool allocate(size_t count, size_t string_bytes, error_code& ec) {
            constexpr size_t prefix = __builtin_offsetof(shared_results, entries);
            if (count == 0u ||
                count > (static_cast<size_t>(-1) - prefix) /
                            sizeof(resolver_entry)) {
                ec = make_error_code(errc::value_too_large);
                return false;
            }
            size_t entry_bytes = count * sizeof(resolver_entry);
            if (string_bytes > static_cast<size_t>(-1) - prefix - entry_bytes) {
                ec = make_error_code(errc::value_too_large);
                return false;
            }
            size_t bytes = prefix + entry_bytes + string_bytes;
            size_t units = (bytes + sizeof(shared_results) - 1u) /
                sizeof(shared_results);
            if (units == 0u) {
                ec = make_error_code(errc::value_too_large);
                return false;
            }
            storage_allocator owner;
            shared_results* memory = storage_traits::allocate(owner, units);
            if (!memory) {
                ec = make_error_code(errc::not_enough_memory);
                return false;
            }
            shared_results* candidate = new (memory) shared_results(units, count);
            for (size_t index = 1u; index < count; ++index)
                new (&candidate->entries[index]) resolver_entry();
            storage_ = candidate;
            ec.clear();
            return true;
        }

        static void retain(shared_results* storage) noexcept {
            if (!storage) return;
            size_t observed = __atomic_load_n(&storage->references,
                                              __ATOMIC_ACQUIRE);
            for (;;) {
                if (observed == static_cast<size_t>(-1))
                    rin_panic("net: multi resolver reference overflow");
                size_t desired = observed + 1u;
                if (__atomic_compare_exchange_n(&storage->references, &observed,
                                                desired, true,
                                                __ATOMIC_ACQ_REL,
                                                __ATOMIC_ACQUIRE))
                    return;
            }
        }

        void release() noexcept {
            shared_results* old = storage_;
            storage_ = nullptr;
            if (!old || __atomic_sub_fetch(&old->references,
                                           static_cast<size_t>(1u),
                                           __ATOMIC_ACQ_REL) != 0u)
                return;
            for (size_t index = old->size; index > 1u; --index)
                old->entries[index - 1u].~resolver_entry();
            storage_allocator owner;
            size_t units = old->allocation_units;
            old->~shared_results();
            storage_traits::deallocate(owner, old, units);
        }

        shared_results* storage_;
        friend class multi_resolver;
    };

    class async_operation {
    public:
        async_operation(const async_operation&) = delete;
        async_operation& operator=(const async_operation&) = delete;

        async_operation(async_operation&& other) noexcept
            : port_(other.port_), flags_(other.flags_), wire_flags_(other.wire_flags_),
              host_length_(other.host_length_), service_length_(other.service_length_),
              query_count_(other.query_count_), next_query_(other.next_query_),
              completion_dispatched_(other.completion_dispatched_),
              watch_addrconfig_(other.watch_addrconfig_),
              addrconfig_snapshot_(other.addrconfig_snapshot_), state_(other.state_) {
            copy_text(host_, other.host_, host_length_);
            copy_text(service_, other.service_, service_length_);
            for (size_t index = 0u; index < max_queries; ++index)
                queries_[index] = other.queries_[index];
            other.clear_after_move();
        }

        ~async_operation() {
            if (state_ == resolver_async_state::pending) (void)cancel_remaining();
        }

        resolver_async_state state() const noexcept { return state_; }

        bool cancel(error_code& ec) {
            if (state_ != resolver_async_state::pending) {
                ec = make_error_code(errc::invalid_argument);
                return false;
            }
            if (!cancel_remaining()) {
                state_ = resolver_async_state::failed;
                ec = make_error_code(errc::io_error);
                return false;
            }
            state_ = resolver_async_state::cancelled;
            ec.clear();
            return true;
        }

        resolver_async_state poll(results_type& output, error_code& ec) {
            if (state_ != resolver_async_state::pending) {
                ec = make_error_code(errc::invalid_argument);
                return state_;
            }
            if (!refresh_addrconfig(ec)) return state_;
            size_t index = next_active_query();
            if (index == max_queries) return complete(output, ec);
            RinDnsGetAddrInfoBatchResult native{};
            int status = rin_dns_getaddrinfo_batch_poll(queries_[index].handle,
                                                         &native);
            if (status == 1) {
                ec.clear();
                return state_;
            }
            if (status != 0) return fail_with_cancel(
                make_error_code(errc::io_error), ec);
            queries_[index].handle = 0u;
            queries_[index].complete = true;
            /* A service poll may span an interface transition.  Do not keep
             * that terminal batch if the generation changed during the call. */
            if (!refresh_addrconfig(ec)) return state_;
            if (native.reserved0 != 0u || native.reserved1 != 0u)
                return fail_with_cancel(make_error_code(errc::protocol_error), ec);
            if (native.status == RIN_DNS_STATUS_DONE) {
                if (native.count == 0u ||
                    native.count > RIN_DNS_GETADDRINFO_MAX_RESULTS)
                    return fail_with_cancel(
                        make_error_code(errc::protocol_error), ec);
                queries_[index].result = native;
                queries_[index].success = true;
            } else if (native.status == RIN_DNS_STATUS_NOT_FOUND) {
                queries_[index].success = false;
            } else {
                return fail_with_cancel(status_error(native.status), ec);
            }
            if (next_active_query() != max_queries) {
                ec.clear();
                return state_;
            }
            return complete(output, ec);
        }

        template<typename CompletionHandler>
        bool dispatch(CompletionHandler&& handler, error_code& ec) {
            if (completion_dispatched_ ||
                state_ != resolver_async_state::pending) {
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

    private:
        static constexpr size_t max_queries = 4u;
        struct query {
            unsigned int handle;
            int family;
            int socket_type;
            int protocol;
            bool complete;
            bool success;
            RinDnsGetAddrInfoBatchResult result;
        };

        async_operation(const char* host, size_t host_length,
                        const char* service, size_t service_length,
                        unsigned short port, unsigned int flags,
                        unsigned int wire_flags) noexcept
            : port_(port), flags_(flags), wire_flags_(wire_flags),
              host_length_(host_length), service_length_(service_length),
              query_count_(0u), next_query_(0u), completion_dispatched_(false),
              watch_addrconfig_(false),
              addrconfig_snapshot_(), state_(resolver_async_state::invalid) {
            copy_text(host_, host, host_length_);
            copy_text(service_, service, service_length_);
            for (size_t index = 0u; index < max_queries; ++index)
                queries_[index] = {};
        }

        static void copy_text(char* output, const char* input,
                              size_t length) noexcept {
            for (size_t index = 0u; index < length; ++index)
                output[index] = input[index];
            output[length] = '\0';
        }

        static error_code status_error(unsigned int status) {
            if (status == RIN_DNS_STATUS_TIMED_OUT)
                return make_error_code(errc::timed_out);
            if (status == RIN_DNS_STATUS_CANCELLED)
                return make_error_code(errc::operation_canceled);
            if (status == RIN_DNS_STATUS_CONFIG_MISSING)
                return make_error_code(errc::network_unreachable);
            if (status == RIN_DNS_STATUS_SEND_BLOCKED)
                return make_error_code(errc::resource_unavailable_try_again);
            if (status == RIN_DNS_STATUS_FAILED)
                return make_error_code(errc::io_error);
            return make_error_code(errc::protocol_error);
        }

        bool cancel_remaining() noexcept {
            bool okay = true;
            for (size_t index = 0u; index < query_count_; ++index) {
                if (queries_[index].handle != 0u &&
                    rin_dns_resolve_cancel(queries_[index].handle) != 0)
                    okay = false;
                queries_[index].handle = 0u;
            }
            return okay;
        }

        resolver_async_state fail_with_cancel(const error_code& failure,
                                              error_code& ec) {
            const bool cancelled = cancel_remaining();
            state_ = resolver_async_state::failed;
            ec = cancelled ? failure : make_error_code(errc::io_error);
            return state_;
        }

        size_t next_active_query() noexcept {
            for (size_t attempt = 0u; attempt < query_count_; ++attempt) {
                size_t index = (next_query_ + attempt) % query_count_;
                if (queries_[index].handle != 0u) {
                    next_query_ = (index + 1u) % query_count_;
                    return index;
                }
            }
            return max_queries;
        }

        bool start_queries(int ipv4_configured, int ipv6_configured,
                           error_code& ec) {
            static const int families[2] = { AF_INET6, AF_INET };
            static const int socket_types[2] = { SOCK_STREAM, SOCK_DGRAM };
            static const int protocols[2] = { IPPROTO_TCP, IPPROTO_UDP };
            query_count_ = 0u;
            next_query_ = 0u;
            for (size_t family_index = 0u; family_index < 2u; ++family_index) {
                int family = families[family_index];
                if ((family == AF_INET && !ipv4_configured) ||
                    (family == AF_INET6 && !ipv6_configured))
                    continue;
                for (size_t transport = 0u; transport < 2u; ++transport) {
                    int handle = rin_dns_getaddrinfo_batch_begin(
                        host_, service_, family, socket_types[transport],
                        protocols[transport], wire_flags_);
                    if (handle <= 0) {
                        (void)cancel_remaining();
                        query_count_ = 0u;
                        ec = make_error_code(errc::io_error);
                        return false;
                    }
                    query& added = queries_[query_count_++];
                    added = {};
                    added.handle = static_cast<unsigned int>(handle);
                    added.family = family;
                    added.socket_type = socket_types[transport];
                    added.protocol = protocols[transport];
                }
            }
            if (query_count_ == 0u) {
                ec = make_error_code(errc::address_not_available);
                return false;
            }
            state_ = resolver_async_state::pending;
            ec.clear();
            return true;
        }

        bool refresh_addrconfig(error_code& ec) {
            if (!watch_addrconfig_) return true;
            RinNetPrimaryInfo observed{};
            int ipv4_configured = 0;
            int ipv6_configured = 0;
            if (!multi_resolver::addrconfig_snapshot(
                    observed, ipv4_configured, ipv6_configured, ec)) {
                (void)cancel_remaining();
                state_ = resolver_async_state::failed;
                return false;
            }
            if (multi_resolver::addrconfig_equal(addrconfig_snapshot_, observed))
                return true;
            if (!cancel_remaining() ||
                !start_queries(ipv4_configured, ipv6_configured, ec)) {
                state_ = resolver_async_state::failed;
                if (!ec) ec = make_error_code(errc::io_error);
                return false;
            }
            addrconfig_snapshot_ = observed;
            /* A configuration transition discards every prior batch and
             * restarts all transports before any result is published. */
            ec.clear();
            return false;
        }

        bool has_v6_result_for_transport(int socket_type,
                                         int protocol) const noexcept {
            for (size_t index = 0u; index < query_count_; ++index) {
                const query& current = queries_[index];
                if (current.success && current.family == AF_INET6 &&
                    current.socket_type == socket_type &&
                    current.protocol == protocol) {
                    return true;
                }
            }
            return false;
        }

        resolver_async_state complete(results_type& output, error_code& ec) {
            constexpr size_t maximum_entries =
                max_queries * RIN_DNS_GETADDRINFO_MAX_RESULTS;
            const RinDnsGetAddrInfoResult* entries[maximum_entries];
            const query* owners[maximum_entries];
            const char* canonical = nullptr;
            size_t canonical_length = 0u;
            size_t count = 0u;
            size_t string_bytes = 0u;
            const bool v4_mapped = (flags_ & AI_V4MAPPED) != 0u;
            const bool all_matching = (flags_ & AI_ALL) != 0u;
            for (size_t query_index = 0u; query_index < query_count_; ++query_index) {
                const query& current = queries_[query_index];
                if (!current.success) continue;
                if (current.family == AF_INET && v4_mapped && !all_matching &&
                    has_v6_result_for_transport(current.socket_type,
                                                current.protocol)) {
                    continue;
                }
                for (size_t item = 0u; item < current.result.count; ++item) {
                    const RinDnsGetAddrInfoResult& native =
                        current.result.entries[item];
                    const char* entry_canonical = nullptr;
                    size_t entry_canonical_length = 0u;
                    if (!multi_resolver::validate_entry(
                            native, current.family, current.socket_type,
                            current.protocol, flags_, wire_flags_, port_, host_,
                            host_length_, entry_canonical,
                            entry_canonical_length, ec)) {
                        state_ = resolver_async_state::failed;
                        return state_;
                    }
                    if ((flags_ & AI_CANONNAME) != 0u) {
                        if (!canonical) {
                            canonical = entry_canonical;
                            canonical_length = entry_canonical_length;
                        } else if (!multi_resolver::text_equal(
                                       canonical, canonical_length,
                                       entry_canonical,
                                       entry_canonical_length)) {
                            state_ = resolver_async_state::failed;
                            ec = make_error_code(errc::protocol_error);
                            return state_;
                        }
                    }
                    if (count == maximum_entries ||
                        entry_canonical_length > static_cast<size_t>(-1) -
                            service_length_ - 2u) {
                        state_ = resolver_async_state::failed;
                        ec = make_error_code(errc::value_too_large);
                        return state_;
                    }
                    size_t bytes = entry_canonical_length + service_length_ + 2u;
                    if (string_bytes > static_cast<size_t>(-1) - bytes) {
                        state_ = resolver_async_state::failed;
                        ec = make_error_code(errc::value_too_large);
                        return state_;
                    }
                    entries[count] = &native;
                    owners[count] = &current;
                    ++count;
                    string_bytes += bytes;
                }
            }
            if (count == 0u) {
                state_ = resolver_async_state::failed;
                ec = make_error_code(errc::address_not_available);
                return state_;
            }
            results_type candidate;
            if (!candidate.allocate(count, string_bytes, ec)) {
                state_ = resolver_async_state::failed;
                return state_;
            }
            char* text = reinterpret_cast<char*>(candidate.storage_->entries + count);
            for (size_t index = 0u; index < count; ++index) {
                const RinDnsGetAddrInfoResult& native = *entries[index];
                const query& owner = *owners[index];
                resolver_entry& entry = candidate.storage_->entries[index];
                entry.endpoint_ = multi_resolver::endpoint_from_entry(
                    native, owner.family, owner.socket_type, owner.protocol,
                    port_, v4_mapped && owner.family == AF_INET);
                const char* stored_host = (flags_ & AI_CANONNAME) != 0u
                    ? native.canonical_name : host_;
                size_t stored_length = (flags_ & AI_CANONNAME) != 0u
                    ? native.canon_len : host_length_;
                entry.host_name_ = text;
                copy_text(text, stored_host, stored_length);
                text += stored_length + 1u;
                entry.service_name_ = text;
                copy_text(text, service_, service_length_);
                text += service_length_ + 1u;
            }
            output = static_cast<results_type&&>(candidate);
            state_ = resolver_async_state::completed;
            ec.clear();
            return state_;
        }

        void clear_after_move() noexcept {
            for (size_t index = 0u; index < max_queries; ++index)
                queries_[index] = {};
            host_[0] = '\0';
            service_[0] = '\0';
            host_length_ = service_length_ = query_count_ = next_query_ = 0u;
            completion_dispatched_ = true;
            watch_addrconfig_ = false;
            addrconfig_snapshot_ = {};
            state_ = resolver_async_state::invalid;
        }

        query queries_[max_queries];
        unsigned short port_;
        unsigned int flags_;
        unsigned int wire_flags_;
        size_t host_length_;
        size_t service_length_;
        size_t query_count_;
        size_t next_query_;
        bool completion_dispatched_;
        bool watch_addrconfig_;
        RinNetPrimaryInfo addrconfig_snapshot_;
        char host_[RIN_DNS_NAME_MAX + 1u];
        char service_[6];
        resolver_async_state state_;
        friend class multi_resolver;
    };

    async_operation async_resolve(const string& host, unsigned short port,
                                  error_code& ec) {
        return async_resolve(host, port, resolver_flags::none, ec);
    }

    async_operation async_resolve(const string& host, unsigned short port,
                                  resolver_flags flags, error_code& ec) {
        char service[6];
        format_service(port, service);
        unsigned int raw_flags = static_cast<unsigned int>(flags);
        unsigned int wire_flags = raw_flags & ~static_cast<unsigned int>(
            AI_ADDRCONFIG | AI_V4MAPPED | AI_ALL);
        if (!flags_valid(raw_flags) || host.size() > RIN_DNS_NAME_MAX ||
            contains_nul(host) ||
            (host.empty() && (raw_flags & AI_CANONNAME) != 0u)) {
            ec = make_error_code(errc::invalid_argument);
            /* Do not copy an over-limit caller string into the operation's
             * fixed request snapshot merely to return an invalid operation. */
            return async_operation("", 0u, service, text_length(service),
                                   port, raw_flags, wire_flags);
        }
        async_operation operation(host.c_str(), host.size(), service,
                                  text_length(service), port, raw_flags,
                                  wire_flags);
        int ipv4_configured = 1;
        int ipv6_configured = 1;
        if ((raw_flags & AI_ADDRCONFIG) != 0u) {
            RinNetPrimaryInfo snapshot{};
            if (!addrconfig_snapshot(snapshot, ipv4_configured,
                                     ipv6_configured, ec))
                return operation;
            operation.watch_addrconfig_ = true;
            operation.addrconfig_snapshot_ = snapshot;
        }
        (void)operation.start_queries(ipv4_configured, ipv6_configured, ec);
        return operation;
    }

private:
    static bool flags_valid(unsigned int flags) noexcept {
        constexpr unsigned int supported = AI_PASSIVE | AI_CANONNAME |
            AI_NUMERICHOST | AI_NUMERICSERV | AI_ADDRCONFIG |
            AI_V4MAPPED | AI_ALL;
        return (flags & ~supported) == 0u &&
            ((flags & AI_ALL) == 0u || (flags & AI_V4MAPPED) != 0u);
    }

    static bool contains_nul(const string& value) noexcept {
        for (size_t index = 0u; index < value.size(); ++index)
            if (value[index] == '\0') return true;
        return false;
    }

    static size_t text_length(const char* text) noexcept {
        size_t length = 0u;
        while (text[length] != '\0') ++length;
        return length;
    }

    static void format_service(unsigned short port, char (&service)[6]) noexcept {
        unsigned int value = port;
        size_t length = 0u;
        if (value >= 10000u) { service[length++] = static_cast<char>('0' + value / 10000u); value %= 10000u; }
        if (value >= 1000u || length) { service[length++] = static_cast<char>('0' + value / 1000u); value %= 1000u; }
        if (value >= 100u || length) { service[length++] = static_cast<char>('0' + value / 100u); value %= 100u; }
        if (value >= 10u || length) { service[length++] = static_cast<char>('0' + value / 10u); value %= 10u; }
        service[length++] = static_cast<char>('0' + value);
        service[length] = '\0';
    }

    static bool addrconfig_snapshot(RinNetPrimaryInfo& snapshot,
                                    int& ipv4_configured,
                                    int& ipv6_configured,
                                    error_code& ec) noexcept {
        snapshot = {};
        if (rin_net_get_primary_info(&snapshot) != 0) {
            ec = make_error_code(errc::io_error);
            return false;
        }
        if (rin_netif_addrconfig_decode(&snapshot, &ipv4_configured,
                                        &ipv6_configured) != 0) {
            ec = make_error_code(errc::protocol_error);
            return false;
        }
        if (!ipv4_configured && !ipv6_configured) {
            ec = make_error_code(errc::address_not_available);
            return false;
        }
        ec.clear();
        return true;
    }

    static bool addrconfig_equal(const RinNetPrimaryInfo& left,
                                 const RinNetPrimaryInfo& right) noexcept {
        const unsigned char* a = reinterpret_cast<const unsigned char*>(&left);
        const unsigned char* b = reinterpret_cast<const unsigned char*>(&right);
        for (size_t index = 0u; index < sizeof(RinNetPrimaryInfo); ++index)
            if (a[index] != b[index]) return false;
        return true;
    }

    static bool text_equal(const char* left, size_t left_length,
                           const char* right, size_t right_length) noexcept {
        if (left_length != right_length) return false;
        for (size_t index = 0u; index < left_length; ++index)
            if (left[index] != right[index]) return false;
        return true;
    }

    static bool validate_entry(const RinDnsGetAddrInfoResult& native,
                               int family, int socket_type, int protocol,
                               unsigned int flags, unsigned int wire_flags,
                               unsigned short port, const char* host,
                               size_t host_length, const char*& canonical,
                               size_t& canonical_length,
                               error_code& ec) noexcept {
        if (!host || host_length > RIN_DNS_NAME_MAX ||
            host[host_length] != '\0' ||
            native.status != RIN_DNS_STATUS_DONE || native.reserved0 != 0u ||
            native.reserved1 != 0u || native.ai_family != family ||
            native.ai_socktype != socket_type || native.ai_protocol != protocol ||
            static_cast<unsigned int>(native.ai_flags) != wire_flags ||
            native.port != htons(port) ||
            (family == AF_INET && native.ai_addrlen != sizeof(sockaddr_in)) ||
            (family == AF_INET6 && native.ai_addrlen != sizeof(sockaddr_in6))) {
            ec = make_error_code(errc::protocol_error);
            return false;
        }
        if (family == AF_INET) {
            for (size_t index = 4u; index < 16u; ++index) {
                if (native.addr[index] != 0u) {
                    ec = make_error_code(errc::protocol_error);
                    return false;
                }
            }
        }
        canonical = host;
        canonical_length = host_length;
        if ((flags & AI_CANONNAME) != 0u) {
            if (native.canon_len == 0u || native.canon_len > RIN_DNS_NAME_MAX ||
                native.canonical_name[native.canon_len] != '\0') {
                ec = make_error_code(errc::protocol_error);
                return false;
            }
            for (size_t index = 0u; index < native.canon_len; ++index) {
                if (native.canonical_name[index] == '\0') {
                    ec = make_error_code(errc::protocol_error);
                    return false;
                }
            }
            canonical = native.canonical_name;
            canonical_length = native.canon_len;
        } else if (native.canon_len != 0u || native.canonical_name[0] != '\0') {
            ec = make_error_code(errc::protocol_error);
            return false;
        }
        ec.clear();
        return true;
    }

    static resolver_socket_endpoint endpoint_from_entry(
        const RinDnsGetAddrInfoResult& native, int family, int socket_type,
        int protocol, unsigned short port, bool map_ipv4) noexcept {
        if (family == AF_INET) {
            if (map_ipv4) {
                address_v6::bytes_type mapped{};
                mapped[10u] = 0xffu;
                mapped[11u] = 0xffu;
                for (size_t index = 0u; index < 4u; ++index)
                    mapped[12u + index] = native.addr[index];
                return resolver_socket_endpoint(
                    ip::address(address_v6(mapped, 0u)), port,
                    socket_type, protocol);
            }
            address_v4::bytes_type bytes{{ native.addr[0], native.addr[1],
                                           native.addr[2], native.addr[3] }};
            return resolver_socket_endpoint(ip::address(address_v4(bytes)), port,
                                            socket_type, protocol);
        }
        address_v6::bytes_type bytes{};
        for (size_t index = 0u; index < 16u; ++index) bytes[index] = native.addr[index];
        return resolver_socket_endpoint(ip::address(address_v6(bytes, 0u)), port,
                                        socket_type, protocol);
    }
};

#endif /* RINCXX_NET_MULTI_RESOLVER_H */
