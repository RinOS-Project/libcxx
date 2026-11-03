/* SPDX-License-Identifier: MIT */
#ifndef RINCXX_CHRONO_TZDB_H
#define RINCXX_CHRONO_TZDB_H

#include "algorithm.h"
#include "cstdlib.h"
#include "forward_list.h"
#include "mutex.h"
#include "stdexcept.h"
#include "string.h"
#include "string_view.h"
#include "vector.h"

#if __cplusplus >= 202002L

#if defined(__GNUC__) || defined(__clang__)
#define RIN_CHRONO_TZDB_WEAK __attribute__((weak))
#else
#define RIN_CHRONO_TZDB_WEAK
#endif

extern "C" {
int rin_system_timezone_current(char*, size_t, size_t*)
    RIN_CHRONO_TZDB_WEAK;
int rin_system_timezone_canonicalize(const char*, char*, size_t, size_t*)
    RIN_CHRONO_TZDB_WEAK;
int rin_system_timezone_tzdb_catalog(char*, size_t, size_t*)
    RIN_CHRONO_TZDB_WEAK;
int rin_system_timezone_reload_tzdb(void) RIN_CHRONO_TZDB_WEAK;
int rin_system_timezone_offset_seconds(const char*, int64_t, int32_t*, int*)
    RIN_CHRONO_TZDB_WEAK;
int rin_system_timezone_name_at(const char*, int64_t, char*, size_t)
    RIN_CHRONO_TZDB_WEAK;
int rin_system_timezone_local_offsets_seconds(
    const char*, int64_t, int32_t*, int*, size_t, size_t*)
    RIN_CHRONO_TZDB_WEAK;
int rin_system_timezone_transition(const char*, int64_t, int, int, int64_t*)
    RIN_CHRONO_TZDB_WEAK;
}

namespace std {
namespace chrono {

struct local_t {};

template<class Duration>
using sys_time = time_point<system_clock, Duration>;
template<class Duration>
using local_time = time_point<local_t, Duration>;
using sys_seconds = sys_time<seconds>;
using sys_days = sys_time<days>;
using local_seconds = local_time<seconds>;

enum class choose { earliest, latest };

struct sys_info {
    sys_seconds begin;
    sys_seconds end;
    seconds offset;
    minutes save;
    std::string abbrev;
};

struct local_info {
    static constexpr int unique = 0;
    static constexpr int nonexistent = 1;
    static constexpr int ambiguous = 2;
    int result = unique;
    sys_info first;
    sys_info second;
};

class ambiguous_local_time : public std::runtime_error {
public:
    explicit ambiguous_local_time(const char* message)
        : std::runtime_error(message) {}
};
class nonexistent_local_time : public std::runtime_error {
public:
    explicit nonexistent_local_time(const char* message)
        : std::runtime_error(message) {}
};

namespace chrono_tzdb_detail {

[[noreturn]] inline void failure(const char* message) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    throw std::runtime_error(message);
#else
    rin_panic(message);
#endif
}

inline std::mutex& catalog_mutex() {
    static std::mutex mutex;
    return mutex;
}

template<class Duration>
inline seconds floor_seconds(Duration value) {
    seconds whole = duration_cast<seconds>(value);
    if (duration_cast<Duration>(whole) > value) whole -= seconds(1);
    return whole;
}

inline int query_offset(const char* zone, int64_t epoch_seconds,
                        int32_t* offset, int* in_dst) {
    if (!rin_system_timezone_offset_seconds || !offset || !in_dst ||
        !rin_system_timezone_offset_seconds(zone, epoch_seconds, offset,
                                            in_dst))
        return 0;
    if (*offset < -86400 || *offset > 86400 || (*in_dst != 0 && *in_dst != 1))
        return 0;
    return 1;
}

inline int query_transition(const char* zone, int64_t epoch_seconds,
                            int direction, int include_given,
                            int64_t* transition) {
    if (!rin_system_timezone_transition || !transition) return -1;
    return rin_system_timezone_transition(zone, epoch_seconds, direction,
                                          include_given, transition);
}

inline bool standard_offset_near(const char* zone, int64_t epoch_seconds,
                                 int32_t current_offset, int* is_dst,
                                 int32_t* standard_offset) {
    if (!is_dst || !standard_offset) return false;
    int32_t ignored_offset = 0;
    if (!query_offset(zone, epoch_seconds, &ignored_offset, is_dst)) return false;
    if (!*is_dst) {
        *standard_offset = current_offset;
        return true;
    }
    for (int direction = 0; direction <= 1; ++direction) {
        int64_t cursor = epoch_seconds;
        for (unsigned int step = 0u; step < 64u; ++step) {
            int64_t transition = 0;
            const int found = query_transition(zone, cursor, direction, 0,
                                               &transition);
            if (found < 0) return false;
            if (found == 0) break;
            if (transition == INT64_MIN || transition == INT64_MAX)
                break;
            int32_t before_offset = 0;
            int32_t after_offset = 0;
            int before_dst = 0;
            int after_dst = 0;
            if (!query_offset(zone, transition - 1, &before_offset,
                              &before_dst) ||
                !query_offset(zone, transition, &after_offset, &after_dst))
                return false;
            if (!before_dst) {
                *standard_offset = before_offset;
                return true;
            }
            if (!after_dst) {
                *standard_offset = after_offset;
                return true;
            }
            cursor = direction == 0 ? transition - 1 : transition;
        }
    }
    return false;
}

inline std::string abbreviation_at(const char* zone, int64_t epoch_seconds) {
    char abbreviation[32] = {};
    if (!rin_system_timezone_name_at ||
        !rin_system_timezone_name_at(zone, epoch_seconds, abbreviation,
                                     sizeof(abbreviation)) ||
        abbreviation[0] == '\0')
        failure("chrono time-zone abbreviation is unavailable");
    return std::string(abbreviation);
}

} // namespace chrono_tzdb_detail

class time_zone {
    std::string name_;

    explicit time_zone(std::string name) : name_(static_cast<std::string&&>(name)) {}
    friend struct tzdb;

public:
    time_zone(time_zone&&) = default;
    time_zone& operator=(time_zone&&) = default;

    std::string_view name() const noexcept {
        return std::string_view(name_.data(), name_.size());
    }

    template<class Duration>
    sys_info get_info(const sys_time<Duration>& value) const {
        using chrono_tzdb_detail::failure;
        using chrono_tzdb_detail::query_offset;
        using chrono_tzdb_detail::query_transition;
        const seconds epoch = chrono_tzdb_detail::floor_seconds(
            value.time_since_epoch());
        const int64_t epoch_seconds = static_cast<int64_t>(epoch.count());
        int32_t offset_seconds = 0;
        int in_dst = 0;
        if (!query_offset(name_.c_str(), epoch_seconds, &offset_seconds,
                          &in_dst))
            failure("chrono time-zone instant is outside the available database");

        int32_t standard_seconds = offset_seconds;
        int dst_state = in_dst;
        if (!chrono_tzdb_detail::standard_offset_near(
                name_.c_str(), epoch_seconds, offset_seconds, &dst_state,
                &standard_seconds))
            failure("chrono time-zone standard offset is unavailable");

        sys_info result;
        int64_t boundary = 0;
        int transition = query_transition(name_.c_str(), epoch_seconds, 0, 1,
                                          &boundary);
        if (transition < 0)
            failure("chrono time-zone previous transition query failed");
        result.begin = transition == 1
            ? sys_seconds(seconds(boundary)) : sys_seconds::min();
        transition = query_transition(name_.c_str(), epoch_seconds, 1, 0,
                                      &boundary);
        if (transition < 0)
            failure("chrono time-zone next transition query failed");
        result.end = transition == 1
            ? sys_seconds(seconds(boundary)) : sys_seconds::max();
        result.offset = seconds(offset_seconds);
        result.save = minutes((offset_seconds - standard_seconds) / 60);
        result.abbrev = chrono_tzdb_detail::abbreviation_at(
            name_.c_str(), epoch_seconds);
        return result;
    }

    template<class Duration>
    local_info get_info(const local_time<Duration>& value) const {
        using chrono_tzdb_detail::failure;
        using chrono_tzdb_detail::query_offset;
        using chrono_tzdb_detail::query_transition;
        const seconds local_seconds_value =
            chrono_tzdb_detail::floor_seconds(value.time_since_epoch());
        const int64_t local_epoch_seconds =
            static_cast<int64_t>(local_seconds_value.count());
        int32_t offsets[2] = {};
        int dst_values[2] = {};
        size_t count = 0u;
        if (!rin_system_timezone_local_offsets_seconds ||
            !rin_system_timezone_local_offsets_seconds(
                name_.c_str(), local_epoch_seconds, offsets, dst_values, 2u,
                &count) || count > 2u)
            failure("chrono local-time query failed");
        for (size_t index = 0u; index < count; ++index) {
            if (offsets[index] < -86400 || offsets[index] > 86400 ||
                (dst_values[index] != 0 && dst_values[index] != 1))
                failure("chrono local-time provider returned invalid data");
        }

        local_info result;
        using common_duration = common_type_t<Duration, seconds>;
        if (count == 1u) {
            const common_duration local_epoch = common_duration(
                value.time_since_epoch());
            result.first = get_info(sys_time<common_duration>(
                local_epoch - seconds(offsets[0])));
            result.result = local_info::unique;
            return result;
        }
        if (count == 2u) {
            const common_duration local_epoch = common_duration(
                value.time_since_epoch());
            sys_time<common_duration> first_sys(
                local_epoch - seconds(offsets[0]));
            sys_time<common_duration> second_sys(
                local_epoch - seconds(offsets[1]));
            if (second_sys < first_sys) {
                const sys_time<common_duration> temporary = first_sys;
                first_sys = second_sys;
                second_sys = temporary;
            }
            result.first = get_info(first_sys);
            result.second = get_info(second_sys);
            result.result = local_info::ambiguous;
            return result;
        }

        /* A gap has no matching offset. Find the exact forward transition
         * whose skipped local interval contains the requested wall time. */
        for (int direction = 0; direction <= 1; ++direction) {
            int64_t cursor = local_epoch_seconds;
            if (direction == 1) {
                if (cursor > INT64_MAX - 86400) continue;
                cursor += 86400;
            } else {
                if (cursor < INT64_MIN + 86400) continue;
                cursor -= 86400;
            }
            for (unsigned int step = 0u; step < 16u; ++step) {
                int64_t transition = 0;
                const int found = query_transition(name_.c_str(), cursor,
                                                   direction, 0, &transition);
                if (found < 0)
                    failure("chrono local-time transition query failed");
                if (found == 0) break;
                if (transition == INT64_MIN || transition == INT64_MAX) break;
                int32_t before_offset = 0;
                int32_t after_offset = 0;
                int before_dst = 0;
                int after_dst = 0;
                if (!query_offset(name_.c_str(), transition - 1,
                                  &before_offset, &before_dst) ||
                    !query_offset(name_.c_str(), transition,
                                  &after_offset, &after_dst))
                    failure("chrono local-time transition offsets are unavailable");
                const int64_t before_local = transition + before_offset;
                const int64_t after_local = transition + after_offset;
                if (after_local > before_local &&
                    local_epoch_seconds >= before_local &&
                    local_epoch_seconds < after_local) {
                    result.first = get_info(sys_seconds(seconds(transition - 1)));
                    result.second = get_info(sys_seconds(seconds(transition)));
                    result.result = local_info::nonexistent;
                    return result;
                }
                cursor = direction == 0 ? transition - 1 : transition;
            }
        }
        failure("chrono local time has no database mapping");
    }

    template<class Duration>
    sys_time<common_type_t<Duration, seconds>>
    to_sys(const local_time<Duration>& value) const {
        const local_info info = get_info(value);
        if (info.result == local_info::ambiguous) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
            throw ambiguous_local_time("ambiguous local time");
#else
            chrono_tzdb_detail::failure("ambiguous local time");
#endif
        }
        if (info.result == local_info::nonexistent) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
            throw nonexistent_local_time("nonexistent local time");
#else
            chrono_tzdb_detail::failure("nonexistent local time");
#endif
        }
        using result_duration = common_type_t<Duration, seconds>;
        return sys_time<result_duration>(
            result_duration(value.time_since_epoch()) - info.first.offset);
    }

    template<class Duration>
    sys_time<common_type_t<Duration, seconds>>
    to_sys(const local_time<Duration>& value, choose choice) const {
        const local_info info = get_info(value);
        using result_duration = common_type_t<Duration, seconds>;
        if (info.result == local_info::nonexistent)
            return sys_time<result_duration>(info.first.end.time_since_epoch());
        const sys_info& selected =
            info.result == local_info::ambiguous && choice == choose::latest
                ? info.second : info.first;
        return sys_time<result_duration>(
            result_duration(value.time_since_epoch()) - selected.offset);
    }

    template<class Duration>
    local_time<common_type_t<Duration, seconds>>
    to_local(const sys_time<Duration>& value) const {
        using result_duration = common_type_t<Duration, seconds>;
        const sys_info info = get_info(value);
        return local_time<result_duration>(
            result_duration(value.time_since_epoch()) + info.offset);
    }
};

inline bool operator==(const time_zone& left, const time_zone& right) noexcept {
    return left.name() == right.name();
}

inline strong_ordering operator<=>(const time_zone& left,
                                  const time_zone& right) noexcept {
    return left.name() <=> right.name();
}

class time_zone_link {
    std::string name_;
    std::string target_;
    time_zone_link(std::string name, std::string target)
        : name_(static_cast<std::string&&>(name)),
          target_(static_cast<std::string&&>(target)) {}
    friend struct tzdb;
public:
    std::string_view name() const noexcept {
        return std::string_view(name_.data(), name_.size());
    }
    std::string_view target() const noexcept {
        return std::string_view(target_.data(), target_.size());
    }
};

class leap_second {
    sys_seconds date_;
    seconds value_;
    leap_second(sys_seconds date, seconds value) : date_(date), value_(value) {}
    friend struct tzdb;
public:
    leap_second(const leap_second&) = default;
    leap_second& operator=(const leap_second&) = default;
    constexpr sys_seconds date() const noexcept { return date_; }
    constexpr seconds value() const noexcept { return value_; }
};

inline bool operator==(const leap_second& left,
                      const leap_second& right) noexcept {
    return left.date() == right.date();
}
inline strong_ordering operator<=>(const leap_second& left,
                                  const leap_second& right) noexcept {
    return left.date() <=> right.date();
}
template<class Duration>
inline bool operator==(const leap_second& left,
                       const sys_time<Duration>& right) noexcept {
    return left.date() == right;
}
template<class Duration>
inline bool operator==(const sys_time<Duration>& left,
                       const leap_second& right) noexcept {
    return left == right.date();
}
template<class Duration>
inline bool operator<(const leap_second& left,
                      const sys_time<Duration>& right) noexcept {
    return left.date() < right;
}
template<class Duration>
inline bool operator<(const sys_time<Duration>& left,
                      const leap_second& right) noexcept {
    return left < right.date();
}

struct tzdb {
    std::string version;
    std::vector<time_zone> zones;
    std::vector<time_zone_link> links;
    std::vector<leap_second> leap_seconds;

    tzdb() = default;
    tzdb(const tzdb&) = delete;
    tzdb& operator=(const tzdb&) = delete;
    tzdb(tzdb&&) = default;
    tzdb& operator=(tzdb&&) = default;

    static tzdb load_from_system() {
        using chrono_tzdb_detail::failure;
        if (!rin_system_timezone_tzdb_catalog)
            failure("chrono time-zone database provider is unavailable");
        std::string wire(1024u * 1024u, '\0');
        size_t length = 0u;
        if (!rin_system_timezone_tzdb_catalog(wire.data(), wire.size(),
                                              &length) ||
            length >= wire.size())
            failure("chrono time-zone database snapshot is unavailable");
        wire.resize(length);

        tzdb result;
        bool saw_version = false;
        int64_t previous_leap = INT64_MIN;
        size_t cursor = 0u;
        while (cursor < wire.size()) {
            size_t line_end = wire.find('\n', cursor);
            if (line_end == std::string::npos) line_end = wire.size();
            const size_t line_size = line_end - cursor;
            const char* line = wire.data() + cursor;
            if (line_size < 3u || line[1] != '\t')
                failure("chrono time-zone snapshot has an invalid record");
            if (line[0] == 'v') {
                if (saw_version || line_size < 7u)
                    failure("chrono time-zone snapshot has an invalid version");
                result.version.assign(line + 2u, line_size - 2u);
                saw_version = true;
            } else if (line[0] == 'z') {
                const char* name_start = line + 2u;
                const char* name_end = static_cast<const char*>(
                    memchr(name_start, '\t', line_size - 2u));
                if (!name_end || name_end == name_start)
                    failure("chrono time-zone snapshot has an invalid zone");
                const char* target_start = name_end + 1u;
                const size_t target_length =
                    line_size - static_cast<size_t>(target_start - line);
                if (target_length == 0u ||
                    memchr(target_start, '\t', target_length) != nullptr)
                    failure("chrono time-zone snapshot has an invalid target");
                std::string name(name_start,
                                 static_cast<size_t>(name_end - name_start));
                std::string target(target_start, target_length);
                if (name == target) {
                    for (const auto& existing : result.zones) {
                        if (existing.name() == name)
                            failure("chrono time-zone snapshot has duplicate zones");
                    }
                    result.zones.push_back(time_zone(static_cast<std::string&&>(name)));
                } else {
                    for (const auto& existing : result.links) {
                        if (existing.name() == name)
                            failure("chrono time-zone snapshot has duplicate links");
                    }
                    result.links.push_back(time_zone_link(
                        static_cast<std::string&&>(name),
                        static_cast<std::string&&>(target)));
                }
            } else if (line[0] == 'l') {
                const char* date_start = line + 2u;
                const char* date_end = static_cast<const char*>(
                    memchr(date_start, '\t', line_size - 2u));
                if (!date_end || date_end == date_start)
                    failure("chrono time-zone snapshot has an invalid leap date");
                std::string date_text(date_start,
                                      static_cast<size_t>(date_end - date_start));
                std::string value_text(
                    date_end + 1u,
                    line_size - static_cast<size_t>(date_end + 1u - line));
                char* parse_end = nullptr;
                const long long date_value =
                    std::strtoll(date_text.c_str(), &parse_end, 10);
                if (!parse_end || *parse_end != '\0' ||
                    (date_value <= previous_leap) ||
                    (value_text != "1" && value_text != "-1"))
                    failure("chrono time-zone snapshot has invalid leap data");
                const seconds leap_value(
                    value_text == "1" ? 1 : -1);
                result.leap_seconds.push_back(leap_second(
                    sys_seconds(seconds(static_cast<int64_t>(date_value))),
                    leap_value));
                previous_leap = static_cast<int64_t>(date_value);
            } else {
                failure("chrono time-zone snapshot has an unknown record");
            }
            cursor = line_end + (line_end < wire.size() ? 1u : 0u);
        }
        if (!saw_version || result.zones.empty() || result.leap_seconds.empty())
            failure("chrono time-zone snapshot is incomplete");
        std::sort(result.zones.begin(), result.zones.end(),
                  [](const time_zone& left, const time_zone& right) {
                      return left.name() < right.name();
                  });
        std::sort(result.links.begin(), result.links.end(),
                  [](const time_zone_link& left, const time_zone_link& right) {
                      return left.name() < right.name();
                  });
        for (const auto& link : result.links) {
            bool target_found = false;
            for (const auto& zone : result.zones) {
                if (zone.name() == link.target()) {
                    target_found = true;
                    break;
                }
            }
            if (!target_found)
                failure("chrono time-zone alias target is missing");
        }
        return result;
    }

    const time_zone* locate_zone(std::string_view zone_name) const {
        for (const auto& zone : zones) {
            if (zone.name() == zone_name) return &zone;
        }
        for (const auto& link : links) {
            if (link.name() != zone_name) continue;
            for (const auto& zone : zones) {
                if (zone.name() == link.target()) return &zone;
            }
        }
        chrono_tzdb_detail::failure("requested time zone is not in the database");
    }

    const time_zone* current_zone() const {
        if (!rin_system_timezone_current)
            chrono_tzdb_detail::failure("system time-zone provider is unavailable");
        char name[128] = {};
        size_t length = 0u;
        if (!rin_system_timezone_current(name, sizeof(name), &length) ||
            length == 0u || length >= sizeof(name))
            chrono_tzdb_detail::failure("system time zone is unavailable");
        return locate_zone(std::string_view(name, length));
    }
};

class tzdb_list {
public:
    using const_iterator = std::forward_list<tzdb>::const_iterator;

private:
    std::forward_list<tzdb> values_;
    friend const tzdb& reload_tzdb();
    friend tzdb_list& get_tzdb_list();

    void initialize() {
        std::lock_guard<std::mutex> guard(chrono_tzdb_detail::catalog_mutex());
        if (values_.empty()) values_.push_front(tzdb::load_from_system());
    }

public:
    tzdb_list() = default;
    tzdb_list(const tzdb_list&) = delete;
    tzdb_list& operator=(const tzdb_list&) = delete;

    const tzdb& front() const noexcept {
        std::lock_guard<std::mutex> guard(chrono_tzdb_detail::catalog_mutex());
        return values_.front();
    }
    const_iterator erase_after(const_iterator position) {
        std::lock_guard<std::mutex> guard(chrono_tzdb_detail::catalog_mutex());
        return const_iterator(values_.erase_after(position));
    }
    const_iterator begin() const noexcept {
        std::lock_guard<std::mutex> guard(chrono_tzdb_detail::catalog_mutex());
        return values_.cbegin();
    }
    const_iterator end() const noexcept {
        std::lock_guard<std::mutex> guard(chrono_tzdb_detail::catalog_mutex());
        return values_.cend();
    }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }
};

inline tzdb_list& get_tzdb_list() {
    static tzdb_list list;
    list.initialize();
    return list;
}

inline const tzdb& get_tzdb() { return get_tzdb_list().front(); }
inline const time_zone* locate_zone(std::string_view name) {
    return get_tzdb().locate_zone(name);
}
inline const time_zone* current_zone() { return get_tzdb().current_zone(); }

inline const tzdb& reload_tzdb() {
    tzdb_list& list = get_tzdb_list();
    std::lock_guard<std::mutex> guard(chrono_tzdb_detail::catalog_mutex());
    if (!rin_system_timezone_reload_tzdb ||
        !rin_system_timezone_reload_tzdb())
        chrono_tzdb_detail::failure("system time-zone database reload failed");
    tzdb candidate = tzdb::load_from_system();
    if (candidate.version != list.values_.front().version)
        list.values_.push_front(static_cast<tzdb&&>(candidate));
    return list.values_.front();
}

inline std::string remote_version() {
    return tzdb::load_from_system().version;
}

struct leap_second_info {
    bool is_leap_second;
    seconds elapsed;
};

struct utc_clock;
struct tai_clock;
struct gps_clock;
template<class Duration> using utc_time = time_point<utc_clock, Duration>;
template<class Duration> using tai_time = time_point<tai_clock, Duration>;
template<class Duration> using gps_time = time_point<gps_clock, Duration>;

template<class Duration>
leap_second_info get_leap_second_info(const utc_time<Duration>& value);

struct utc_clock {
    using rep = long long;
    using period = nano;
    using duration = nanoseconds;
    using time_point = utc_time<duration>;
    static constexpr bool is_steady = false;

    static time_point now() {
        return from_sys(system_clock::now());
    }

    template<class Duration>
    static sys_time<common_type_t<Duration, seconds>>
    to_sys(const utc_time<Duration>& value) {
        using result_duration = common_type_t<Duration, seconds>;
        const result_duration utc_epoch(value.time_since_epoch());
        seconds elapsed(0);
        for (const auto& leap : get_tzdb().leap_seconds) {
            const result_duration transition(
                leap.date().time_since_epoch());
            if (leap.value() > seconds::zero()) {
                const result_duration inserted_start = transition + elapsed;
                const result_duration inserted_end =
                    inserted_start + leap.value();
                if (utc_epoch >= inserted_start && utc_epoch < inserted_end)
                    return sys_time<result_duration>(
                        transition - result_duration(1));
                if (utc_epoch < inserted_start)
                    return sys_time<result_duration>(utc_epoch - elapsed);
            } else if (utc_epoch < transition + elapsed) {
                /* A negative leap overlaps the final pre-transition UTC
                 * second; select its earlier system-time representation. */
                return sys_time<result_duration>(utc_epoch - elapsed);
            }
            elapsed += leap.value();
        }
        return sys_time<result_duration>(utc_epoch - elapsed);
    }

    template<class Duration>
    static utc_time<common_type_t<Duration, seconds>>
    from_sys(const sys_time<Duration>& value) {
        using result_duration = common_type_t<Duration, seconds>;
        const result_duration sys_epoch(value.time_since_epoch());
        seconds elapsed(0);
        for (const auto& leap : get_tzdb().leap_seconds) {
            if (sys_epoch < result_duration(leap.date().time_since_epoch()))
                break;
            elapsed += leap.value();
        }
        return utc_time<result_duration>(sys_epoch + elapsed);
    }
};

template<class Duration>
leap_second_info get_leap_second_info(const utc_time<Duration>& value) {
    using result_duration = common_type_t<Duration, seconds>;
    const result_duration utc_epoch(value.time_since_epoch());
    seconds elapsed(0);
    for (const auto& leap : get_tzdb().leap_seconds) {
        const result_duration transition(leap.date().time_since_epoch());
        if (leap.value() > seconds::zero()) {
            const result_duration start = transition + elapsed;
            const result_duration end = start + leap.value();
            if (utc_epoch >= start && utc_epoch < end)
                return { true, elapsed + leap.value() };
            if (utc_epoch < start) break;
        } else if (utc_epoch < transition + elapsed) {
            break;
        }
        elapsed += leap.value();
    }
    return { false, elapsed };
}

struct tai_clock {
    using rep = long long;
    using period = nano;
    using duration = nanoseconds;
    using time_point = tai_time<duration>;
    static constexpr bool is_steady = false;

    static time_point now() { return from_utc(utc_clock::now()); }
    template<class Duration>
    static utc_time<common_type_t<Duration, seconds>>
    to_utc(const tai_time<Duration>& value) noexcept {
        using result_duration = common_type_t<Duration, seconds>;
        return utc_time<result_duration>(
            result_duration(value.time_since_epoch()) - seconds(378691210));
    }
    template<class Duration>
    static tai_time<common_type_t<Duration, seconds>>
    from_utc(const utc_time<Duration>& value) noexcept {
        using result_duration = common_type_t<Duration, seconds>;
        return tai_time<result_duration>(
            result_duration(value.time_since_epoch()) + seconds(378691210));
    }
};

struct gps_clock {
    using rep = long long;
    using period = nano;
    using duration = nanoseconds;
    using time_point = gps_time<duration>;
    static constexpr bool is_steady = false;

    static time_point now() { return from_utc(utc_clock::now()); }
    template<class Duration>
    static utc_time<common_type_t<Duration, seconds>>
    to_utc(const gps_time<Duration>& value) noexcept {
        using result_duration = common_type_t<Duration, seconds>;
        return utc_time<result_duration>(
            result_duration(value.time_since_epoch()) + seconds(315964809));
    }
    template<class Duration>
    static gps_time<common_type_t<Duration, seconds>>
    from_utc(const utc_time<Duration>& value) noexcept {
        using result_duration = common_type_t<Duration, seconds>;
        return gps_time<result_duration>(
            result_duration(value.time_since_epoch()) - seconds(315964809));
    }
};

template<class DestClock, class SourceClock>
struct clock_time_conversion {};

template<class Clock>
struct clock_time_conversion<Clock, Clock> {
    template<class Duration>
    constexpr time_point<Clock, Duration> operator()(
        const time_point<Clock, Duration>& value) const noexcept {
        return value;
    }
};

template<>
struct clock_time_conversion<utc_clock, system_clock> {
    template<class Duration>
    auto operator()(const sys_time<Duration>& value) const
        -> decltype(utc_clock::from_sys(value)) {
        return utc_clock::from_sys(value);
    }
};
template<>
struct clock_time_conversion<system_clock, utc_clock> {
    template<class Duration>
    auto operator()(const utc_time<Duration>& value) const
        -> decltype(utc_clock::to_sys(value)) {
        return utc_clock::to_sys(value);
    }
};
template<>
struct clock_time_conversion<tai_clock, utc_clock> {
    template<class Duration>
    auto operator()(const utc_time<Duration>& value) const noexcept
        -> decltype(tai_clock::from_utc(value)) {
        return tai_clock::from_utc(value);
    }
};
template<>
struct clock_time_conversion<utc_clock, tai_clock> {
    template<class Duration>
    auto operator()(const tai_time<Duration>& value) const noexcept
        -> decltype(tai_clock::to_utc(value)) {
        return tai_clock::to_utc(value);
    }
};
template<>
struct clock_time_conversion<gps_clock, utc_clock> {
    template<class Duration>
    auto operator()(const utc_time<Duration>& value) const noexcept
        -> decltype(gps_clock::from_utc(value)) {
        return gps_clock::from_utc(value);
    }
};
template<>
struct clock_time_conversion<utc_clock, gps_clock> {
    template<class Duration>
    auto operator()(const gps_time<Duration>& value) const noexcept
        -> decltype(gps_clock::to_utc(value)) {
        return gps_clock::to_utc(value);
    }
};

template<class DestClock, class SourceClock, class Duration>
auto clock_cast(const time_point<SourceClock, Duration>& value) {
    if constexpr (is_same<DestClock, SourceClock>::value) {
        return time_point<DestClock, Duration>(value.time_since_epoch());
    } else if constexpr (is_same<DestClock, utc_clock>::value &&
                         is_same<SourceClock, system_clock>::value) {
        return clock_time_conversion<utc_clock, system_clock>{}(value);
    } else if constexpr (is_same<DestClock, system_clock>::value &&
                         is_same<SourceClock, utc_clock>::value) {
        return clock_time_conversion<system_clock, utc_clock>{}(value);
    } else if constexpr (is_same<DestClock, tai_clock>::value &&
                         is_same<SourceClock, utc_clock>::value) {
        return clock_time_conversion<tai_clock, utc_clock>{}(value);
    } else if constexpr (is_same<DestClock, utc_clock>::value &&
                         is_same<SourceClock, tai_clock>::value) {
        return clock_time_conversion<utc_clock, tai_clock>{}(value);
    } else if constexpr (is_same<DestClock, gps_clock>::value &&
                         is_same<SourceClock, utc_clock>::value) {
        return clock_time_conversion<gps_clock, utc_clock>{}(value);
    } else if constexpr (is_same<DestClock, utc_clock>::value &&
                         is_same<SourceClock, gps_clock>::value) {
        return clock_time_conversion<utc_clock, gps_clock>{}(value);
    } else if constexpr (is_same<DestClock, tai_clock>::value &&
                         is_same<SourceClock, system_clock>::value) {
        return tai_clock::from_utc(utc_clock::from_sys(value));
    } else if constexpr (is_same<DestClock, system_clock>::value &&
                         is_same<SourceClock, tai_clock>::value) {
        return utc_clock::to_sys(tai_clock::to_utc(value));
    } else if constexpr (is_same<DestClock, gps_clock>::value &&
                         is_same<SourceClock, system_clock>::value) {
        return gps_clock::from_utc(utc_clock::from_sys(value));
    } else if constexpr (is_same<DestClock, system_clock>::value &&
                         is_same<SourceClock, gps_clock>::value) {
        return utc_clock::to_sys(gps_clock::to_utc(value));
    } else if constexpr (is_same<DestClock, gps_clock>::value &&
                         is_same<SourceClock, tai_clock>::value) {
        return gps_clock::from_utc(tai_clock::to_utc(value));
    } else if constexpr (is_same<DestClock, tai_clock>::value &&
                         is_same<SourceClock, gps_clock>::value) {
        return tai_clock::from_utc(gps_clock::to_utc(value));
    } else {
        static_assert(is_same<DestClock, SourceClock>::value,
                      "clock_cast has no conversion for these clocks");
    }
}

template<class TimeZonePtr>
struct zoned_traits {
    static TimeZonePtr default_zone() { return TimeZonePtr{}; }
    static TimeZonePtr locate_zone(std::string_view) { return TimeZonePtr{}; }
};

template<>
struct zoned_traits<const time_zone*> {
    static const time_zone* default_zone() { return current_zone(); }
    static const time_zone* locate_zone(std::string_view name) {
        return chrono::locate_zone(name);
    }
};

template<class Duration, class TimeZonePtr = const time_zone*>
class zoned_time {
public:
    using duration = Duration;
    using time_point = sys_time<Duration>;
    using sys_time_type = time_point;
    using local_time_type = local_time<Duration>;
    using time_zone_ptr = TimeZonePtr;

private:
    TimeZonePtr zone_;
    time_point time_;

    static TimeZonePtr checked_zone(TimeZonePtr zone) {
        if (!zone) chrono_tzdb_detail::failure("zoned_time received a null zone");
        return zone;
    }

public:
    zoned_time() : zone_(checked_zone(zoned_traits<TimeZonePtr>::default_zone())),
                   time_() {}
    explicit zoned_time(TimeZonePtr zone)
        : zone_(checked_zone(zone)), time_() {}

    template<class Duration2>
    zoned_time(TimeZonePtr zone, const sys_time<Duration2>& value)
        : zone_(checked_zone(zone)), time_(value.time_since_epoch()) {}

    template<class Duration2>
    zoned_time(TimeZonePtr zone, const local_time<Duration2>& value)
        : zone_(checked_zone(zone)), time_(zone_->to_sys(value).time_since_epoch()) {}

    template<class Duration2>
    zoned_time(TimeZonePtr zone, const local_time<Duration2>& value,
               choose choice)
        : zone_(checked_zone(zone)),
          time_(zone_->to_sys(value, choice).time_since_epoch()) {}

    template<class Duration2>
    zoned_time(std::string_view zone, const sys_time<Duration2>& value)
        : zoned_time(zoned_traits<TimeZonePtr>::locate_zone(zone), value) {}

    template<class Duration2>
    zoned_time(std::string_view zone, const local_time<Duration2>& value)
        : zoned_time(zoned_traits<TimeZonePtr>::locate_zone(zone), value) {}

    template<class Duration2>
    zoned_time(std::string_view zone, const local_time<Duration2>& value,
               choose choice)
        : zoned_time(zoned_traits<TimeZonePtr>::locate_zone(zone), value,
                     choice) {}

    TimeZonePtr get_time_zone() const noexcept { return zone_; }
    const time_point& get_sys_time() const noexcept { return time_; }
    local_time_type get_local_time() const { return zone_->to_local(time_); }
    sys_info get_info() const { return zone_->get_info(time_); }
    operator time_point() const { return get_sys_time(); }
    operator local_time_type() const { return get_local_time(); }
};

template<class Duration, class TimeZonePtr>
inline zoned_time<Duration, TimeZonePtr>
make_zoned(TimeZonePtr zone, const sys_time<Duration>& value) {
    return zoned_time<Duration, TimeZonePtr>(zone, value);
}

template<class Duration, class TimeZonePtr>
inline zoned_time<Duration, TimeZonePtr>
make_zoned(TimeZonePtr zone, const local_time<Duration>& value,
           choose choice = choose::earliest) {
    return zoned_time<Duration, TimeZonePtr>(zone, value, choice);
}

} // namespace chrono
} // namespace std

#undef RIN_CHRONO_TZDB_WEAK
#endif /* C++20 */
#endif /* RINCXX_CHRONO_TZDB_H */
