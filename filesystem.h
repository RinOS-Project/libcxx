/*
 * RinOS C++ <filesystem> ✿
 * ファイルシステムライブラリ (C++17)
 */

#ifndef RINCXX_FILESYSTEM_H
#define RINCXX_FILESYSTEM_H

#include "rincxx.h"

/* std::filesystem is a C++17 library facility. */
#if defined(__cplusplus) && __cplusplus >= 201703L

#include "string.h"
#include "vector.h"
#include "memory.h"
#include "chrono.h"
#include "functional.h"
#include "stdexcept.h"
#include "system_error.h"
#include "iomanip.h"
#if __cplusplus >= 202002L
#include "compare.h"
#endif

/* Cファイルシステム関数 */
extern "C" {
    /* ファイル操作 */
    int rin_open(const char* path, int flags);
    int rin_close(int fd);
    long rin_read(int fd, void* buf, unsigned long count);
    long rin_write(int fd, const void* buf, unsigned long count);
    long rin_seek(int fd, long offset, int whence);

    /* ファイル情報 */
    int rin_stat(const char* path, void* buf);
    int rin_lstat(const char* path, void* buf);
    int rin_fstat(int fd, void* buf);
    int rin_access(const char* path, int mode);

    /* ディレクトリ操作 */
    int rin_mkdir(const char* path, unsigned int mode);
    int rin_rmdir(const char* path);
    int rin_unlink(const char* path);
    int rin_rename(const char* oldpath, const char* newpath);
    int rin_symlink(const char* target, const char* linkpath);
    int rin_link(const char* oldpath, const char* newpath);
    int rin_readlink(const char* path, char* buffer, unsigned long capacity);

    /* ディレクトリ読み取り */
    typedef void* RinDir;
    RinDir rin_opendir(const char* path);
    int rin_closedir(RinDir dir);

    struct RinDirEntry {
        unsigned long d_ino;
        unsigned char d_type;
        char d_name[256];
    };
    /* 0 publishes an entry, a positive value is clean EOF, and a negative
     * value is an I/O failure.  The distinction is required so C++17
     * directory_iterator can report a failed increment rather than silently
     * treating it as normal end-of-directory. */
    int rin_readdir(RinDir dir, RinDirEntry* entry);

    /* 現在の作業ディレクトリ */
    char* rin_getcwd(char* buf, unsigned long size);
    int rin_chdir(const char* path);
    int rin_chmod(const char* path, unsigned int mode);
    int rin_chmod_nofollow(const char* path, unsigned int mode);
    int rin_set_times(const char* path, long long access_seconds,
                      long long write_seconds);
}

namespace std {
namespace filesystem {

/* ═══════════════════════════════════════════════════════════════
 * file_type
 * ═══════════════════════════════════════════════════════════════*/

enum class file_type {
    none = 0,
    not_found = -1,
    regular = 1,
    directory = 2,
    symlink = 3,
    block = 4,
    character = 5,
    fifo = 6,
    socket = 7,
    unknown = 8
};

using file_time_type = ::std::chrono::system_clock::time_point;

/* ═══════════════════════════════════════════════════════════════
 * perms - ファイルパーミッション
 * ═══════════════════════════════════════════════════════════════*/

enum class perms : unsigned {
    none = 0,
    owner_read = 0400,
    owner_write = 0200,
    owner_exec = 0100,
    owner_all = 0700,
    group_read = 040,
    group_write = 020,
    group_exec = 010,
    group_all = 070,
    others_read = 04,
    others_write = 02,
    others_exec = 01,
    others_all = 07,
    all = 0777,
    set_uid = 04000,
    set_gid = 02000,
    sticky_bit = 01000,
    mask = 07777,
    unknown = 0xFFFF
};

constexpr perms operator|(perms a, perms b) noexcept {
    return static_cast<perms>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

constexpr perms operator&(perms a, perms b) noexcept {
    return static_cast<perms>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
}

constexpr perms operator^(perms a, perms b) noexcept {
    return static_cast<perms>(static_cast<unsigned>(a) ^ static_cast<unsigned>(b));
}

constexpr perms operator~(perms a) noexcept {
    return static_cast<perms>(~static_cast<unsigned>(a));
}
enum class perm_options : unsigned {
    replace = 1,
    add = 2,
    remove = 4,
    nofollow = 8
};
constexpr perm_options operator|(perm_options a, perm_options b) noexcept {
    return static_cast<perm_options>(static_cast<unsigned>(a) |
                                      static_cast<unsigned>(b));
}
constexpr perm_options operator&(perm_options a, perm_options b) noexcept {
    return static_cast<perm_options>(static_cast<unsigned>(a) &
                                      static_cast<unsigned>(b));
}

/* ═══════════════════════════════════════════════════════════════
 * copy_options
 * ═══════════════════════════════════════════════════════════════*/

enum class copy_options : unsigned {
    none = 0,
    skip_existing = 1,
    overwrite_existing = 2,
    update_existing = 4,
    recursive = 8,
    copy_symlinks = 16,
    skip_symlinks = 32,
    directories_only = 64,
    create_symlinks = 128,
    create_hard_links = 256
};

constexpr copy_options operator|(copy_options a, copy_options b) noexcept {
    return static_cast<copy_options>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

constexpr copy_options operator&(copy_options a, copy_options b) noexcept {
    return static_cast<copy_options>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
}

constexpr copy_options& operator|=(copy_options& a, copy_options b) noexcept {
    a = a | b;
    return a;
}

constexpr copy_options& operator&=(copy_options& a, copy_options b) noexcept {
    a = a & b;
    return a;
}

/* ═══════════════════════════════════════════════════════════════
 * directory_options
 * ═══════════════════════════════════════════════════════════════*/

enum class directory_options : unsigned {
    none = 0,
    follow_directory_symlink = 1,
    skip_permission_denied = 2
};

constexpr directory_options operator|(directory_options a, directory_options b) noexcept {
    return static_cast<directory_options>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

constexpr directory_options operator&(directory_options a,
                                       directory_options b) noexcept {
    return static_cast<directory_options>(static_cast<unsigned>(a) &
                                          static_cast<unsigned>(b));
}

constexpr directory_options& operator|=(directory_options& a,
                                         directory_options b) noexcept {
    a = a | b;
    return a;
}

constexpr directory_options& operator&=(directory_options& a,
                                         directory_options b) noexcept {
    a = a & b;
    return a;
}

/* ═══════════════════════════════════════════════════════════════
 * path クラス
 * ═══════════════════════════════════════════════════════════════*/

/* forward declaration */
class path;
path operator/(const path& lhs, const path& rhs);

class path {
public:
    using value_type = char;
    using string_type = std::string;
    static constexpr value_type preferred_separator = '/';

private:
    string_type pathname_;

    static void split_raw_components(const string_type& value,
                                     vector<string_type>& components,
                                     bool& absolute,
                                     bool& trailing_separator) {
        absolute = !value.empty() && value[0] == '/';
        trailing_separator = !value.empty() && value.back() == '/';
        size_t cursor = absolute ? 1u : 0u;
        while (cursor < value.size()) {
            while (cursor < value.size() && value[cursor] == '/') ++cursor;
            if (cursor == value.size()) break;
            size_t next = cursor;
            while (next < value.size() && value[next] != '/') ++next;
            components.push_back(value.substr(cursor, next - cursor));
            cursor = next;
        }
    }

    static path compose_components(bool absolute,
                                   const vector<string_type>& components,
                                   bool trailing_separator) {
        string_type result;
        if (absolute) result.push_back('/');
        for (size_t index = 0; index < components.size(); ++index) {
            if (!result.empty() && result.back() != '/') result.push_back('/');
            result += components[index];
        }
        if (result.empty()) return path(absolute ? "/" : ".");
        if (trailing_separator && result.back() != '/') result.push_back('/');
        return path(result);
    }

public:
    /* コンストラクタ */
    path() noexcept = default;
    path(const path&) = default;
    path(path&&) noexcept = default;

    path(const string_type& source) : pathname_(source) {}
    path(const value_type* source) : pathname_(source) {}

    template<typename InputIt>
    path(InputIt first, InputIt last) : pathname_(first, last) {}

    /* 代入 */
    path& operator=(const path&) = default;
    path& operator=(path&&) noexcept = default;
    path& operator=(const string_type& source) { pathname_ = source; return *this; }
    path& operator=(const value_type* source) { pathname_ = source; return *this; }

    /* assign */
    path& assign(const string_type& source) { pathname_ = source; return *this; }

    path& assign(const value_type* source) {
        pathname_ = source;
        return *this;
    }

    template<typename InputIt>
    path& assign(InputIt first, InputIt last) {
        pathname_.assign(first, last);
        return *this;
    }

    /* append (/) */
    path& operator/=(const path& p) {
        /* POSIX has no root-name component.  An RHS with a root directory
         * therefore replaces the complete LHS; blindly concatenating it
         * would turn `/base` / `/child` into the non-standard `/base/child`.
         */
        if (p.has_root_directory()) {
            pathname_ = p.pathname_;
            return *this;
        }
        if (!pathname_.empty() && pathname_.back() != '/' && !p.empty())
            pathname_ += '/';
        pathname_ += p.pathname_;
        return *this;
    }

    path& operator/=(const string_type& source) {
        return operator/=(path(source));
    }

    path& operator/=(const value_type* source) {
        return operator/=(path(source));
    }

    path& operator/=(value_type component) {
        return operator/=(path(string_type(1u, component)));
    }

    path& append(const path& p) { return operator/=(p); }

    path& append(const string_type& source) {
        return operator/=(path(source));
    }

    path& append(const value_type* source) {
        return operator/=(path(source));
    }

    path& append(value_type component) {
        return operator/=(path(string_type(1u, component)));
    }

    template<typename InputIt>
    path& append(InputIt first, InputIt last) {
        /* `append` has path composition semantics even for an iterator
         * range: the source is one path, so a preferred separator is added
         * between an existing filename and the range.  `concat` below is the
         * raw byte-appending operation. */
        return operator/=(path(first, last));
    }

    /* concat */
    path& operator+=(const path& p) { pathname_ += p.pathname_; return *this; }
    path& operator+=(const string_type& s) { pathname_ += s; return *this; }
    path& operator+=(const value_type* s) { pathname_ += s; return *this; }
    path& operator+=(value_type c) { pathname_ += c; return *this; }

    path& concat(const path& p) { return operator+=(p); }

    path& concat(const string_type& source) { return operator+=(source); }
    path& concat(const value_type* source) { return operator+=(source); }
    path& concat(value_type component) { return operator+=(component); }

    template<typename InputIt>
    path& concat(InputIt first, InputIt last) {
        pathname_.append(first, last);
        return *this;
    }

    path& make_preferred() noexcept { return *this; }

    path& remove_filename() {
        if (pathname_.empty()) return *this;
        const size_t separator = pathname_.rfind('/');
        if (separator == string_type::npos) {
            pathname_.clear();
        } else {
            pathname_.erase(separator + 1u);
        }
        return *this;
    }

    /* clear */
    void clear() noexcept { pathname_.clear(); }

    /* swap */
    void swap(path& other) noexcept { pathname_.swap(other.pathname_); }

    /* native format */
    const string_type& native() const noexcept { return pathname_; }
    const value_type* c_str() const noexcept { return pathname_.c_str(); }
    operator string_type() const { return pathname_; }

    ::std::string string() const { return pathname_; }
    ::std::string generic_string() const { return pathname_; }

    /* compare */
    int compare(const path& p) const noexcept {
        return pathname_.compare(p.pathname_);
    }

    /* The source overloads compare the path's native byte sequence directly;
     * constructing a temporary path is unnecessary and, more importantly,
     * would make the pointer form ambiguous with the path conversion
     * constructor for generic callers. */
    int compare(const string_type& source) const noexcept {
        return pathname_.compare(source);
    }

    int compare(const value_type* source) const noexcept {
        return pathname_.compare(source);
    }

    /* decomposition */
    path root_name() const {
        /* Unix: 空 */
        return path();
    }

    path root_directory() const {
        if (!pathname_.empty() && pathname_[0] == '/') {
            return path("/");
        }
        return path();
    }

    path root_path() const {
        return root_name() / root_directory();
    }

    path relative_path() const {
        if (!pathname_.empty() && pathname_[0] == '/') {
            return path(pathname_.substr(1));
        }
        return *this;
    }

    path parent_path() const {
        size_t pos = pathname_.rfind('/');
        if (pos == string_type::npos) return path();
        if (pos == 0) return path("/");
        return path(pathname_.substr(0, pos));
    }

    path filename() const {
        size_t pos = pathname_.rfind('/');
        if (pos == string_type::npos) return *this;
        return path(pathname_.substr(pos + 1));
    }

    path stem() const {
        string_type fn = filename().pathname_;
        size_t pos = fn.rfind('.');
        if (pos == string_type::npos || pos == 0) return path(fn);
        return path(fn.substr(0, pos));
    }

    path extension() const {
        string_type fn = filename().pathname_;
        size_t pos = fn.rfind('.');
        if (pos == string_type::npos || pos == 0) return path();
        return path(fn.substr(pos));
    }

    path lexically_normal() const {
        vector<string_type> raw;
        bool absolute = false;
        bool trailing_separator = false;
        split_raw_components(pathname_, raw, absolute, trailing_separator);

        vector<string_type> normalized;
        for (size_t index = 0; index < raw.size(); ++index) {
            const string_type& component = raw[index];
            const bool last = index + 1u == raw.size();
            if (component == ".") {
                if (last) trailing_separator = true;
                continue;
            }
            if (component == "..") {
                if (!normalized.empty() && normalized.back() != "..") {
                    normalized.pop_back();
                    trailing_separator = !normalized.empty();
                } else if (!absolute) {
                    normalized.push_back(component);
                    trailing_separator = false;
                }
                continue;
            }
            normalized.push_back(component);
            trailing_separator = false;
        }
        return compose_components(absolute, normalized, trailing_separator);
    }

    path lexically_relative(const path& base) const {
        const path target_normal = lexically_normal();
        const path base_normal = base.lexically_normal();
        vector<string_type> target_components;
        vector<string_type> base_components;
        bool target_absolute = false;
        bool base_absolute = false;
        bool target_trailing = false;
        bool base_trailing = false;
        split_raw_components(target_normal.pathname_, target_components,
                             target_absolute, target_trailing);
        split_raw_components(base_normal.pathname_, base_components,
                             base_absolute, base_trailing);
        if (target_absolute != base_absolute) return path();

        size_t common = 0;
        while (common < target_components.size() &&
               common < base_components.size() &&
               target_components[common] == base_components[common]) {
            ++common;
        }

        vector<string_type> relative;
        for (size_t index = common; index < base_components.size(); ++index) {
            if (base_components[index] == "..") return path();
            relative.push_back("..");
        }
        for (size_t index = common; index < target_components.size(); ++index)
            relative.push_back(target_components[index]);
        if (relative.empty()) return path(".");
        return compose_components(false, relative, target_trailing);
    }

    path lexically_proximate(const path& base) const {
        const path relative = lexically_relative(base);
        return relative.empty() ? *this : relative;
    }

    /* query */
    bool empty() const noexcept { return pathname_.empty(); }

    bool has_root_name() const { return !root_name().empty(); }
    bool has_root_directory() const { return !root_directory().empty(); }
    bool has_root_path() const { return has_root_name() || has_root_directory(); }
    bool has_relative_path() const { return !relative_path().empty(); }
    bool has_parent_path() const { return !parent_path().empty(); }
    bool has_filename() const { return !filename().empty(); }
    bool has_stem() const { return !stem().empty(); }
    bool has_extension() const { return !extension().empty(); }

    bool is_absolute() const { return has_root_directory(); }
    bool is_relative() const { return !is_absolute(); }

    /* replace */
    path& replace_filename(const path& replacement) {
        *this = parent_path() / replacement;
        return *this;
    }

    path& replace_extension(const path& replacement = path()) {
        const path parent = parent_path();
        string_type filename_text = filename().pathname_;
        const size_t extension_position = filename_text.rfind('.');
        if (extension_position != string_type::npos &&
            extension_position != 0u) {
            filename_text.erase(extension_position);
        }
        pathname_ = parent.empty()
            ? filename_text
            : (parent / path(filename_text)).pathname_;
        if (!replacement.empty()) {
            if (replacement.pathname_[0] != '.') {
                pathname_ += '.';
            }
            pathname_ += replacement.pathname_;
        }
        return *this;
    }

    /* Iterators follow the POSIX decomposition rules used by the standard
     * path iterator: the first root separator is its own component, while
     * subsequent separators are delimiters (including repeated/trailing
     * separators).  Positions are canonical component starts, so copies of
     * iterators from different path objects never compare equal merely because
     * their offsets happen to match. */
    class iterator {
        friend class path;
        const path* p_;
        size_t pos_;
        size_t end_;

        static size_t skip_separators(const path* value, size_t position,
                                      size_t limit) noexcept {
            while (position < limit && value->pathname_[position] == '/') {
                ++position;
            }
            return position;
        }

        static size_t first_component(const path* value, size_t position,
                                      size_t limit) noexcept {
            if (position == 0 && limit != 0 &&
                value->pathname_[0] == '/') {
                return 0; /* root-directory component */
            }
            return skip_separators(value, position, limit);
        }

    public:
        using iterator_category = bidirectional_iterator_tag;
        using value_type = path;
        using difference_type = ptrdiff_t;
        using pointer = const path*;
        using reference = const path&;

        iterator() : p_(nullptr), pos_(0), end_(0) {}
        iterator(const path* p, size_t pos, size_t end) : p_(p), pos_(pos), end_(end) {}

        path operator*() const {
            if (!p_ || pos_ >= end_) return path();
            if (pos_ == 0 && p_->pathname_[0] == '/') return path("/");
            size_t next = p_->pathname_.find('/', pos_);
            if (next == string_type::npos) next = end_;
            return path(p_->pathname_.substr(pos_, next - pos_));
        }

        iterator& operator++() {
            if (!p_ || pos_ >= end_) return *this;
            size_t next = p_->pathname_.find('/', pos_);
            if (next == string_type::npos) {
                pos_ = end_;
            } else {
                pos_ = skip_separators(p_, next, end_);
            }
            return *this;
        }

        iterator operator++(int) {
            iterator previous(*this);
            ++(*this);
            return previous;
        }

        iterator& operator--() {
            if (!p_ || pos_ == 0) return *this;

            /* --end() starts at the last non-separator component. */
            size_t cursor = pos_ >= end_ ? end_ : pos_;
            while (cursor > 0 && p_->pathname_[cursor - 1] == '/') {
                --cursor;
            }
            if (cursor == 0) {
                /* A path made only of separators consists of root only. */
                pos_ = p_->pathname_.empty() ? end_ : 0;
                return *this;
            }

            const size_t previous_separator =
                p_->pathname_.rfind('/', cursor - 1);
            if (previous_separator == string_type::npos) {
                pos_ = 0;
            } else if (previous_separator == 0) {
                /* A root separator is itself a component only when there is
                 * no component between it and the current position. */
                pos_ = cursor == 1
                    ? 0
                    : skip_separators(p_, 1, cursor);
            } else {
                pos_ = skip_separators(p_, previous_separator + 1, cursor);
            }
            return *this;
        }

        iterator operator--(int) {
            iterator previous(*this);
            --(*this);
            return previous;
        }

        bool operator==(const iterator& other) const {
            return p_ == other.p_ && pos_ == other.pos_ && end_ == other.end_;
        }
        bool operator!=(const iterator& other) const { return !(*this == other); }
    };

    iterator begin() const {
        const size_t end = pathname_.size();
        return iterator(this, iterator::first_component(this, 0, end), end);
    }
    iterator end() const { return iterator(this, pathname_.size(), pathname_.size()); }
};

/* path演算子 */
inline path operator/(const path& lhs, const path& rhs) {
    path result = lhs;
    result /= rhs;
    return result;
}

inline bool operator==(const path& lhs, const path& rhs) noexcept {
    return lhs.compare(rhs) == 0;
}

inline bool operator!=(const path& lhs, const path& rhs) noexcept {
    return !(lhs == rhs);
}

inline bool operator<(const path& lhs, const path& rhs) noexcept {
    return lhs.compare(rhs) < 0;
}

inline bool operator<=(const path& lhs, const path& rhs) noexcept {
    return !(rhs < lhs);
}

inline bool operator>(const path& lhs, const path& rhs) noexcept {
    return rhs < lhs;
}

inline bool operator>=(const path& lhs, const path& rhs) noexcept {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
inline std::strong_ordering operator<=>(const path& lhs,
                                        const path& rhs) noexcept {
    const int result = lhs.compare(rhs);
    if (result < 0) return std::strong_ordering::less;
    if (result > 0) return std::strong_ordering::greater;
    return std::strong_ordering::equal;
}
#endif

inline void swap(path& lhs, path& rhs) noexcept {
    lhs.swap(rhs);
}

namespace detail {

static constexpr size_t path_stream_max_units = 4096u;

inline bool path_utf8_continuation(unsigned char value) noexcept {
    return (value & 0xc0u) == 0x80u;
}

template<typename CharT>
inline bool path_utf8_to_stream(const ::std::string& native,
                               basic_string<CharT>& output) {
    output.clear();
    if (sizeof(CharT) == 1u) {
        if (native.size() > path_stream_max_units) return false;
        for (size_t index = 0u; index < native.size(); ++index)
            output.push_back(static_cast<CharT>(
                static_cast<unsigned char>(native[index])));
        return output.size() == native.size();
    }

    size_t index = 0u;
    while (index < native.size()) {
        const unsigned char first = static_cast<unsigned char>(native[index++]);
        unsigned long code_point = 0u;
        size_t continuation_count = 0u;
        unsigned long minimum = 0u;
        if (first <= 0x7fu) {
            code_point = first;
        } else if (first >= 0xc2u && first <= 0xdfu) {
            code_point = first & 0x1fu;
            continuation_count = 1u;
            minimum = 0x80u;
        } else if (first >= 0xe0u && first <= 0xefu) {
            code_point = first & 0x0fu;
            continuation_count = 2u;
            minimum = 0x800u;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            code_point = first & 0x07u;
            continuation_count = 3u;
            minimum = 0x10000u;
        } else {
            return false;
        }
        if (index + continuation_count > native.size()) return false;
        for (size_t count = 0u; count < continuation_count; ++count) {
            const unsigned char next = static_cast<unsigned char>(native[index++]);
            if (!path_utf8_continuation(next)) return false;
            code_point = (code_point << 6u) | (next & 0x3fu);
        }
        if (code_point < minimum || code_point > 0x10ffffu ||
            (code_point >= 0xd800u && code_point <= 0xdfffu)) return false;

        if (sizeof(CharT) == 2u && code_point > 0xffffu) {
            if (output.size() + 2u > path_stream_max_units) return false;
            const unsigned long adjusted = code_point - 0x10000u;
            output.push_back(static_cast<CharT>(0xd800u + (adjusted >> 10u)));
            output.push_back(static_cast<CharT>(0xdc00u + (adjusted & 0x3ffu)));
        } else {
            if (output.size() == path_stream_max_units) return false;
            output.push_back(static_cast<CharT>(code_point));
        }
    }
    return true;
}

template<typename CharT>
inline bool path_stream_to_utf8(const basic_string<CharT>& input,
                               ::std::string& native) {
    native.clear();
    if (input.size() > path_stream_max_units) return false;
    size_t index = 0u;
    while (index < input.size()) {
        unsigned long code_point = static_cast<unsigned long>(
            static_cast<unsigned long long>(input[index++]));
        if (sizeof(CharT) == 2u) {
            if (code_point >= 0xd800u && code_point <= 0xdbffu) {
                if (index == input.size()) return false;
                const unsigned long trail = static_cast<unsigned long>(
                    static_cast<unsigned long long>(input[index++]));
                if (trail < 0xdc00u || trail > 0xdfffu) return false;
                code_point = 0x10000u + ((code_point - 0xd800u) << 10u) +
                             (trail - 0xdc00u);
            } else if (code_point >= 0xdc00u && code_point <= 0xdfffu) {
                return false;
            }
        }
        if (sizeof(CharT) >= 4u &&
            (code_point > 0x10ffffu ||
             (code_point >= 0xd800u && code_point <= 0xdfffu))) return false;
        if (code_point <= 0x7fu) {
            native.push_back(static_cast<char>(code_point));
        } else if (code_point <= 0x7ffu) {
            native.push_back(static_cast<char>(0xc0u | (code_point >> 6u)));
            native.push_back(static_cast<char>(0x80u | (code_point & 0x3fu)));
        } else if (code_point <= 0xffffu) {
            native.push_back(static_cast<char>(0xe0u | (code_point >> 12u)));
            native.push_back(static_cast<char>(0x80u | ((code_point >> 6u) & 0x3fu)));
            native.push_back(static_cast<char>(0x80u | (code_point & 0x3fu)));
        } else {
            native.push_back(static_cast<char>(0xf0u | (code_point >> 18u)));
            native.push_back(static_cast<char>(0x80u | ((code_point >> 12u) & 0x3fu)));
            native.push_back(static_cast<char>(0x80u | ((code_point >> 6u) & 0x3fu)));
            native.push_back(static_cast<char>(0x80u | (code_point & 0x3fu)));
        }
        if (native.size() > path_stream_max_units) return false;
    }
    return true;
}

} /* namespace detail */

/* The standard filesystem stream operators use the same quoted token
 * contract as <iomanip>.  Native paths are UTF-8, so each stream character
 * type receives a validated conversion.  Both directions stage privately;
 * malformed UTF-8/UTF-16/UTF-32 never partially mutates the path or output. */
template<typename CharT, typename Traits>
inline basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& output, const path& value) {
    basic_string<CharT> candidate;
    if (!detail::path_utf8_to_stream(value.native(), candidate)) {
        output.setstate(ios_base::failbit);
        return output;
    }
    return output << ::std::quoted(candidate.c_str());
}

template<typename CharT, typename Traits>
inline basic_istream<CharT, Traits>&
operator>>(basic_istream<CharT, Traits>& input, path& value) {
    basic_string<CharT> candidate;
    input >> ::std::quoted(candidate);
    if (!input.fail()) {
        ::std::string native;
        if (!detail::path_stream_to_utf8(candidate, native)) {
            input.setstate(ios_base::failbit);
            return input;
        }
        value = path(native);
    }
    return input;
}

/* ═══════════════════════════════════════════════════════════════
 * filesystem_error
 * ═══════════════════════════════════════════════════════════════*/

class filesystem_error : public system_error {
    path path1_;
    path path2_;

    static ::std::string diagnostic(const ::std::string& operation,
                                    const path* first,
                                    const path* second) {
        ::std::string result = operation;
        if (first) {
            result += " [";
            result += first->string();
            result += "]";
        }
        if (second) {
            result += " [";
            result += second->string();
            result += "]";
        }
        return result;
    }

public:
    filesystem_error(const ::std::string& what_arg, error_code ec)
        : system_error(ec, what_arg) {}

    filesystem_error(const ::std::string& what_arg, const path& p1,
                     error_code ec)
        : system_error(ec, diagnostic(what_arg, &p1, nullptr)), path1_(p1) {}

    filesystem_error(const ::std::string& what_arg, const path& p1,
                     const path& p2, error_code ec)
        : system_error(ec, diagnostic(what_arg, &p1, &p2)), path1_(p1),
          path2_(p2) {}

    const path& path1() const noexcept { return path1_; }
    const path& path2() const noexcept { return path2_; }
};

namespace detail {

/* The hosted and target file providers share the small open/read/write
 * carrier used by filebuf.  Keep the constants private to filesystem rather
 * than leaking POSIX headers through this freestanding public header. */
static constexpr int copy_open_read = 0x01;
static constexpr int copy_open_write = 0x02;
static constexpr int copy_open_create = 0x04;
static constexpr int copy_open_trunc = 0x10;

struct stat_record {
    unsigned long st_dev;
    unsigned long st_ino;
    unsigned int st_mode;
    unsigned int st_nlink;
    unsigned int st_uid;
    unsigned int st_gid;
    unsigned long st_rdev;
    long st_size;
    long st_blksize;
    long st_blocks;
    long st_atime;
    long st_mtime;
    long st_ctime;
};

/* Preserve provider errno details while retaining a portable fallback. */
inline errc filesystem_error_from_errno() noexcept {
    switch (errno) {
        case EPERM: return errc::operation_not_permitted;
        case ENOENT: return errc::no_such_file_or_directory;
        case EINTR: return errc::interrupted;
        case EIO: return errc::io_error;
        case EBADF: return errc::bad_file_descriptor;
        case EAGAIN: return errc::resource_unavailable_try_again;
        case ENOMEM: return errc::not_enough_memory;
        case ENOTDIR: return errc::not_a_directory;
        case EBUSY: return errc::device_or_resource_busy;
        case EEXIST: return errc::file_exists;
        case EXDEV: return errc::cross_device_link;
        case EISDIR: return errc::is_a_directory;
        case EACCES: return errc::permission_denied;
        case EMFILE: return errc::too_many_files_open;
        case ENFILE: return errc::too_many_files_open_in_system;
        case EFBIG: return errc::file_too_large;
        case EROFS: return errc::read_only_file_system;
        case ENOSPC: return errc::no_space_on_device;
        case EMLINK: return errc::too_many_links;
        case ENAMETOOLONG: return errc::filename_too_long;
        case ENOSYS: return errc::function_not_supported;
        case ENOTEMPTY: return errc::directory_not_empty;
        case ELOOP: return errc::too_many_symbolic_link_levels;
        case EOVERFLOW: return errc::value_too_large;
        case EINVAL: return errc::invalid_argument;
        default: return errc::io_error;
    }
}

[[noreturn]] inline void filesystem_failure(const char* operation,
                                            error_code ec) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw filesystem_error(::std::string(operation), ec);
#else
    (void)operation;
    (void)ec;
    __builtin_trap();
#endif
}

[[noreturn]] inline void filesystem_failure(const char* operation,
                                            const path& p, error_code ec) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw filesystem_error(::std::string(operation), p, ec);
#else
    (void)operation;
    (void)p;
    (void)ec;
    __builtin_trap();
#endif
}

[[noreturn]] inline void filesystem_failure(const char* operation,
                                            const path& p1, const path& p2,
                                            error_code ec) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw filesystem_error(::std::string(operation), p1, p2, ec);
#else
    (void)operation;
    (void)p1;
    (void)p2;
    (void)ec;
    __builtin_trap();
#endif
}

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * file_status
 * ═══════════════════════════════════════════════════════════════*/

class file_status {
    file_type type_;
    perms permissions_;

public:
    file_status() noexcept : type_(file_type::none), permissions_(perms::unknown) {}
    explicit file_status(file_type ft, perms prms = perms::unknown) noexcept
        : type_(ft), permissions_(prms) {}

    file_type type() const noexcept { return type_; }
    void type(file_type ft) noexcept { type_ = ft; }

    perms permissions() const noexcept { return permissions_; }
    void permissions(perms prms) noexcept { permissions_ = prms; }
};

/* ═══════════════════════════════════════════════════════════════
 * directory_entry
 * ═══════════════════════════════════════════════════════════════*/

class directory_entry {
    friend class directory_iterator;
public:
    using path_type = class path;

private:
    path_type path_;
    file_status status_;
    mutable file_status symlink_status_;
    mutable bool symlink_status_valid_;
    uintmax_t file_size_;
    uintmax_t hard_link_count_;
    file_time_type last_write_time_;

public:
    directory_entry() noexcept
        : path_(), status_(), symlink_status_(), symlink_status_valid_(false),
          file_size_(0), hard_link_count_(0),
          last_write_time_(file_time_type::min()) {}
    directory_entry(const directory_entry&) = default;
    directory_entry(directory_entry&&) noexcept = default;

    explicit directory_entry(const path_type& p)
        : path_(p), status_(), symlink_status_(), symlink_status_valid_(false),
          file_size_(0), hard_link_count_(0), last_write_time_() {
        refresh();
    }

    /* Internal consumers that already expose an error_code can avoid the
     * throwing constructor's discarded refresh error and publish one stat
     * result atomically.  This is intentionally an overload rather than a
     * second public status cache so all metadata still flows through
     * refresh(error_code&). */
    directory_entry(const path_type& p, error_code& ec) noexcept
        : path_(p), status_(), symlink_status_(), symlink_status_valid_(false),
          file_size_(0), hard_link_count_(0), last_write_time_() {
        refresh(ec);
    }

    directory_entry& operator=(const directory_entry&) = default;
    directory_entry& operator=(directory_entry&&) noexcept = default;

    void assign(const path_type& p) {
        path_ = p;
        refresh();
    }

    void replace_filename(const path_type& p) {
        path_.replace_filename(p);
        refresh();
    }

    void refresh(error_code& ec) noexcept {
        ec.clear();
        symlink_status_valid_ = false;
        /* stat を呼んで情報を更新する。失敗時は stale cache を
         * 公開せず、not_found/zero へ閉じる。 */
        detail::stat_record st = {};

        const int stat_result = rin_stat(path_.c_str(), &st);
        volatile detail::stat_record* stat_view = &st;
        const long measured_size = stat_view->st_size;
        if (stat_result == 0 && measured_size >= 0) {
            /* ファイルタイプを判定 */
            const unsigned int mode = stat_view->st_mode;
            if ((mode & 0170000) == 0040000) {
                status_.type(file_type::directory);
            } else if ((mode & 0170000) == 0100000) {
                status_.type(file_type::regular);
            } else if ((mode & 0170000) == 0120000) {
                status_.type(file_type::symlink);
            } else if ((mode & 0170000) == 0060000) {
                status_.type(file_type::block);
            } else if ((mode & 0170000) == 0020000) {
                status_.type(file_type::character);
            } else if ((mode & 0170000) == 0010000) {
                status_.type(file_type::fifo);
            } else if ((mode & 0170000) == 0140000) {
                status_.type(file_type::socket);
            } else {
                status_.type(file_type::unknown);
            }
            status_.permissions(static_cast<perms>(mode & 0777));
            file_size_ = static_cast<uintmax_t>(measured_size);
            hard_link_count_ = static_cast<uintmax_t>(stat_view->st_nlink);
            last_write_time_ = file_time_type(::std::chrono::duration_cast<file_time_type::duration>(::std::chrono::seconds(stat_view->st_mtime)));
        } else {
            status_.type(file_type::not_found);
            status_.permissions(perms::unknown);
            file_size_ = 0;
            hard_link_count_ = 0;
            last_write_time_ = file_time_type::min();
            ec = make_error_code(detail::filesystem_error_from_errno());
        }
    }

    void refresh() {
        error_code ec;
        refresh(ec);
        if (ec) {
            detail::filesystem_failure("directory_entry::refresh", path_, ec);
        }
    }

    const path_type& path() const noexcept { return path_; }
    operator const path_type&() const noexcept { return path_; }

    bool exists() const {
        return status_.type() != file_type::not_found &&
               status_.type() != file_type::none &&
               status_.type() != file_type::unknown;
    }
    bool is_regular_file() const { return status_.type() == file_type::regular; }
    bool is_directory() const { return status_.type() == file_type::directory; }
    file_status symlink_status(error_code& ec) const noexcept {
        ec.clear();
        if (symlink_status_valid_) return symlink_status_;
        detail::stat_record st = {};
        const int result = rin_lstat(path_.c_str(), &st);
        volatile detail::stat_record* view = &st;
        if (result != 0 || view->st_size < 0) {
            symlink_status_ = file_status(file_type::not_found);
            symlink_status_valid_ = false;
            ec = make_error_code(detail::filesystem_error_from_errno());
            return symlink_status_;
        }
        const unsigned int mode = view->st_mode & 0170000;
        file_type type = file_type::unknown;
        if (mode == 0040000) type = file_type::directory;
        else if (mode == 0100000) type = file_type::regular;
        else if (mode == 0120000) type = file_type::symlink;
        else if (mode == 0060000) type = file_type::block;
        else if (mode == 0020000) type = file_type::character;
        else if (mode == 0010000) type = file_type::fifo;
        else if (mode == 0140000) type = file_type::socket;
        symlink_status_ = file_status(type,
                                      static_cast<perms>(view->st_mode & 0777));
        symlink_status_valid_ = true;
        return symlink_status_;
    }

    file_status symlink_status() const {
        error_code ec;
        const file_status result = symlink_status(ec);
        if (ec) detail::filesystem_failure("directory_entry::symlink_status",
                                           path_, ec);
        return result;
    }

    bool is_symlink(error_code& ec) const noexcept {
        return symlink_status(ec).type() == file_type::symlink;
    }

    bool is_symlink() const noexcept {
        error_code ec;
        return is_symlink(ec);
    }
    bool is_block_file() const { return status_.type() == file_type::block; }
    bool is_character_file() const { return status_.type() == file_type::character; }
    bool is_fifo() const { return status_.type() == file_type::fifo; }
    bool is_socket() const { return status_.type() == file_type::socket; }
    bool is_other() const {
        return exists() && !is_regular_file() && !is_directory() &&
               !is_symlink();
    }

    uintmax_t file_size() const { return file_size_; }
    uintmax_t file_size(error_code& ec) const noexcept {
        ec.clear();
        if (!is_regular_file()) {
            ec = make_error_code(is_directory() ? errc::is_a_directory
                                                : errc::io_error);
            return static_cast<uintmax_t>(-1);
        }
        return file_size_;
    }
    uintmax_t hard_link_count() const { return hard_link_count_; }
    uintmax_t hard_link_count(error_code& ec) const noexcept {
        ec.clear();
        if (!exists()) {
            ec = make_error_code(errc::io_error);
            return static_cast<uintmax_t>(-1);
        }
        return hard_link_count_;
    }
    file_time_type last_write_time() const { return last_write_time_; }
    file_time_type last_write_time(error_code& ec) const noexcept {
        ec.clear();
        if (!exists()) {
            ec = make_error_code(errc::io_error);
            return file_time_type::min();
        }
        return last_write_time_;
    }
    file_status status() const { return status_; }
    file_status status(error_code& ec) const noexcept {
        ec.clear();
        return status_;
    }

    bool operator==(const directory_entry& rhs) const noexcept { return path_ == rhs.path_; }
    bool operator!=(const directory_entry& rhs) const noexcept { return path_ != rhs.path_; }
    bool operator<(const directory_entry& rhs) const noexcept { return path_ < rhs.path_; }
#if __cplusplus >= 202002L
    std::strong_ordering operator<=>(const directory_entry& rhs) const noexcept {
        return path_ <=> rhs.path_;
    }
#endif
};

/* ═══════════════════════════════════════════════════════════════
 * directory_iterator
 * ═══════════════════════════════════════════════════════════════*/

class directory_iterator {
    struct state {
        volatile int lock;
        RinDir dir;
        path directory;
        directory_entry entry;
        directory_options options;
        bool at_end;
        size_t owners;
        size_t generation;

        state(RinDir opened_dir, const path& source,
              directory_options selected_options)
            : lock(0), dir(opened_dir), directory(source),
              options(selected_options), at_end(false),
              owners(1), generation(0) {}

        ~state() {
            if (dir) rin_closedir(dir);
        }
    };

    state* state_;
    /* A postfix result owns the value observed before increment.  The
     * directory stream itself remains shared so copies still satisfy the
     * input-iterator single-pass rules, while this detached value keeps
     * operator++(int)'s returned iterator dereferenceable after the source
     * advances. */
    directory_entry snapshot_;
    bool snapshot_active_;
    size_t snapshot_generation_;
    /* Dereferencing an input iterator returns a reference, so returning the
     * shared state's mutable entry directly would let a concurrent increment
     * race with the caller's read.  Keep a per-iterator cache that is refreshed
     * under the state lock whenever the shared generation advances. */
    mutable directory_entry view_;
    mutable state* view_owner_;
    mutable size_t view_generation_;
    mutable bool view_valid_;

    static void state_lock(state* current) noexcept {
        while (__sync_lock_test_and_set(&current->lock, 1) != 0) {
        }
    }

    static void state_unlock(state* current) noexcept {
        __sync_lock_release(&current->lock);
    }

    void retain() noexcept {
        state* current = state_;
        if (!current) return;
        state_lock(current);
        ++current->owners;
        state_unlock(current);
    }

    void release() noexcept {
        state* current = state_;
        bool final_owner = false;
        state_ = nullptr;
        if (!current) return;
        state_lock(current);
        if (current->owners != 0u) {
            --current->owners;
            final_owner = current->owners == 0u;
        }
        state_unlock(current);
        if (final_owner) {
            current->~state();
            rin_free(current);
        }
    }

    static void set_io_error(error_code* ec) noexcept {
        if (ec) *ec = make_error_code(errc::io_error);
    }

    static void set_provider_error(error_code* ec) noexcept {
        if (ec) *ec = make_error_code(detail::filesystem_error_from_errno());
    }

    static void set_memory_error(error_code* ec) noexcept {
        if (ec) *ec = make_error_code(errc::not_enough_memory);
    }

    static bool permission_error(const error_code& ec) noexcept {
        return ec == make_error_code(errc::permission_denied) ||
               ec == make_error_code(errc::operation_not_permitted);
    }

    static bool entry_path_matches(const path& directory, const char* name,
                                   const path& candidate) noexcept {
        const char* directory_text = directory.c_str();
        const char* candidate_text = candidate.c_str();
        char last = '\0';
        while (*directory_text != '\0') {
            if (*candidate_text++ != *directory_text) return false;
            last = *directory_text++;
        }
        if (last != '\0' && last != '/' && *name != '\0' &&
            *name != '/') {
            if (*candidate_text++ != '/') return false;
        }
        while (*name != '\0') {
            if (*candidate_text++ != *name++) return false;
        }
        return *candidate_text == '\0';
    }

    /* RinDirEntry is a fixed ABI record, so a provider must terminate the
     * name inside that record.  Constructing a path before checking this
     * would let a malformed provider make the string reader walk past the
     * record.  Directory names cannot contain '/', and an empty name is not a
     * valid readdir result. */
    static bool entry_name_valid(const char* name) noexcept {
        if (!name) return false;
        for (size_t index = 0u;
             index < sizeof(((RinDirEntry*)0)->d_name); ++index) {
            const unsigned char value = (unsigned char)name[index];
            if (value == 0u) return index != 0u;
            if (value == (unsigned char)'/') return false;
        }
        return false;
    }

    /* A directory provider may expose only a name and d_type.  When the
     * provider can stat the joined path, publish the complete cached metadata
     * in the directory_entry; otherwise retain a trustworthy d_type-only
     * classification instead of turning a valid entry into not_found. */
    static void publish_entry_metadata(directory_entry& entry,
                                        unsigned char directory_type) noexcept {
        detail::stat_record stat = {};
        /* The hosted providers commonly fill the ABI record through a
         * compatibility struct behind the void* callback.  Read through a
         * volatile view so strict-aliasing/whole-program optimization cannot
         * retain the zero-initialized snapshot and silently discard metadata
         * that the provider just published. */
        volatile detail::stat_record* stat_view = &stat;
        const int result = rin_stat(entry.path_.c_str(), &stat);
        if (result == 0 && stat_view->st_size >= 0) {
            const unsigned int mode = stat_view->st_mode & 0170000u;
            file_type type = file_type::unknown;
            if (mode == 0040000u) type = file_type::directory;
            else if (mode == 0100000u) type = file_type::regular;
            else if (mode == 0120000u) type = file_type::symlink;
            else if (mode == 0060000u) type = file_type::block;
            else if (mode == 0020000u) type = file_type::character;
            else if (mode == 0010000u) type = file_type::fifo;
            else if (mode == 0140000u) type = file_type::socket;
            entry.status_ = file_status(type,
                                        static_cast<perms>(stat_view->st_mode & 0777u));
            entry.file_size_ = static_cast<uintmax_t>(stat_view->st_size);
            entry.hard_link_count_ = static_cast<uintmax_t>(stat_view->st_nlink);
            entry.last_write_time_ = file_time_type(
                ::std::chrono::duration_cast<file_time_type::duration>(
                    ::std::chrono::seconds(stat_view->st_mtime)));
            return;
        }

        /* POSIX d_type values are stable in the Rin directory ABI.  A zero
         * or unknown value deliberately leaves status() as none so callers
         * can request a fresh status instead of receiving guessed metadata. */
        file_type type = file_type::none;
        if (directory_type == 4u) type = file_type::directory;
        else if (directory_type == 8u) type = file_type::regular;
        else if (directory_type == 10u) type = file_type::symlink;
        else if (directory_type == 6u) type = file_type::block;
        else if (directory_type == 2u) type = file_type::character;
        else if (directory_type == 1u) type = file_type::fifo;
        else if (directory_type == 12u) type = file_type::socket;
        if (type != file_type::none) entry.status_ = file_status(type);
    }

    static void finish_locked(state* current, error_code* ec) noexcept {
        current->at_end = true;
        const int close_result = current->dir ? rin_closedir(current->dir) : 0;
        if (current->dir && close_result == 0) {
            current->dir = nullptr;
        } else if (current->dir && (!ec || !*ec)) {
            set_provider_error(ec);
        }
    }

    void finish(error_code* ec) noexcept {
        state* current = state_;
        if (!current) return;
        state_lock(current);
        finish_locked(current, ec);
        state_unlock(current);
    }

    static void advance_locked(state* current, error_code* ec) {
        /* Providers may only fill the name field; zero the ABI record so an
         * omitted d_type is deterministic and never becomes an uninitialized
         * metadata classification. */
        RinDirEntry de = {};
        int result = 0;
        if (current->at_end || !current->dir) return;

        while ((result = rin_readdir(current->dir, &de)) == 0) {
            if (!entry_name_valid(de.d_name)) {
                if (ec) *ec = make_error_code(errc::invalid_argument);
                finish_locked(current, ec);
                return;
            }
            /* "." と ".." をスキップ */
            if (de.d_name[0] == '.' &&
                (de.d_name[1] == '\0' ||
                 (de.d_name[1] == '.' && de.d_name[2] == '\0'))) {
                continue;
            }

            /* Construct the entry before publishing it.  In no-exception
             * builds basic_string reports allocation failure by retaining an
             * old/empty value, so validate the exact join rather than expose
             * an empty or truncated path as a successful directory entry. */
            const path entry_path = current->directory / de.d_name;
            if (!entry_path_matches(current->directory, de.d_name,
                                    entry_path)) {
                set_memory_error(ec);
                finish_locked(current, ec);
                return;
            }
            /* directory iteration publishes the discovered path without
             * requiring a second stat for providers that already report a
             * d_type, while opportunistically caching full metadata when a
             * stat callback is available. */
            current->entry.path_ = entry_path;
            current->entry.status_ = file_status();
            current->entry.file_size_ = 0;
            current->entry.hard_link_count_ = 0;
            current->entry.last_write_time_ = file_time_type::min();
            current->entry.symlink_status_valid_ = false;
            publish_entry_metadata(current->entry, de.d_type);
            if (current->entry.path() != entry_path) {
                set_memory_error(ec);
                finish_locked(current, ec);
                return;
            }
            ++current->generation;
            return;
        }

        if (result < 0) {
            set_provider_error(ec);
            /* POSIX providers report a directory permission failure through
             * the same readdir channel as other I/O errors.  The standard
             * skip_permission_denied option turns only that precise error
             * into a clean end iterator; close failures remain observable. */
            if (ec && permission_error(*ec) &&
                (static_cast<unsigned>(current->options) &
                 static_cast<unsigned>(directory_options::skip_permission_denied)) != 0u)
                ec->clear();
        }
        finish_locked(current, ec);
    }

    void advance(error_code* ec) {
        state* current = state_;
        if (ec) ec->clear();
        if (!current) return;

        /* Keep the entry selected by this increment paired with the
         * generation observed under the same lock.  The old implementation
         * unlocked after advance_locked() and refreshed view_ through
         * current_entry(), which allowed another shared iterator to advance
         * the stream in between and made this iterator skip the entry it had
         * just acquired.  Copying while the state is still locked preserves
         * the single-pass stream's exact generation without exposing the
         * provider's mutable entry to the caller. */
        state_lock(current);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            advance_locked(current, ec);
            if (!current->at_end && (!ec || !*ec)) {
                view_ = current->entry;
                view_owner_ = current;
                view_generation_ = current->generation;
                view_valid_ = true;
            } else {
                view_valid_ = false;
            }
        } catch (...) {
            state_unlock(current);
            set_memory_error(ec);
            finish(ec);
            throw;
        }
#else
        advance_locked(current, ec);
        if (!current->at_end && (!ec || !*ec)) {
            view_ = current->entry;
            if (view_.path() != current->entry.path()) {
                set_memory_error(ec);
                view_valid_ = false;
                finish_locked(current, ec);
            } else {
                view_owner_ = current;
                view_generation_ = current->generation;
                view_valid_ = true;
            }
        } else {
            view_valid_ = false;
        }
#endif
        state_unlock(current);
    }

    static bool state_at_end(state* current) noexcept {
        bool result;
        if (!current) return true;
        state_lock(current);
        result = current->at_end;
        state_unlock(current);
        return result;
    }

    static size_t state_generation(state* current) noexcept {
        size_t result = 0;
        if (!current) return result;
        state_lock(current);
        result = current->generation;
        state_unlock(current);
        return result;
    }

    const directory_entry& current_entry() const {
        state* current = state_;
        if (!current) {
            /* Dereferencing the end iterator is a precondition violation. */
            __builtin_trap();
        }
        state_lock(current);
        if (!view_valid_ || view_owner_ != current ||
            view_generation_ != current->generation) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            try {
                view_ = current->entry;
            } catch (...) {
                state_unlock(current);
                throw;
            }
#else
            view_ = current->entry;
            /* In an exception-disabled build basic_string reports an
             * allocation failure by retaining its previous value.  Do not
             * turn that stale cache into a successful dereference after a
             * shared iterator advanced.  There is no error_code channel on
             * operator*(), so the only safe contract is the library's
             * explicit fail-stop boundary. */
            if (view_.path() != current->entry.path()) {
                view_valid_ = false;
                state_unlock(current);
                detail::filesystem_failure(
                    "directory_iterator::dereference", current->directory,
                    make_error_code(errc::not_enough_memory));
            }
#endif
            view_owner_ = current;
            view_generation_ = current->generation;
            view_valid_ = true;
        }
        state_unlock(current);
        return view_;
    }

    void initialize(const path& p, error_code* ec,
                    bool preserve_allocation_failure,
                    directory_options options) {
        if (ec) ec->clear();
        RinDir opened_dir = rin_opendir(p.c_str());
        if (!opened_dir) {
            set_provider_error(ec);
            if (ec && permission_error(*ec) &&
                (static_cast<unsigned>(options) &
                 static_cast<unsigned>(directory_options::skip_permission_denied)) != 0u)
                ec->clear();
            return;
        }
        void* storage = rin_malloc(sizeof(state));
        if (!storage) {
            rin_closedir(opened_dir);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            if (preserve_allocation_failure) throw bad_alloc();
#endif
            if (ec) *ec = make_error_code(errc::not_enough_memory);
            return;
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            state_ = new (storage) state(opened_dir, p, options);
        } catch (...) {
            rin_free(storage);
            rin_closedir(opened_dir);
            if (preserve_allocation_failure) throw;
            set_memory_error(ec);
            return;
        }
#else
        (void)preserve_allocation_failure;
        state_ = new (storage) state(opened_dir, p, options);
#endif
        if (state_->directory != p) {
            set_memory_error(ec);
            release();
            return;
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        if (!preserve_allocation_failure) {
            try {
                advance(ec);
            } catch (...) {
                set_memory_error(ec);
                finish(ec);
            }
            return;
        }
#endif
        /* The throwing constructor must not leak the opened directory or
         * shared state when eager first-entry construction fails.  A failed
         * path append/stat can throw after state_ has been published; unwind
         * that owner before propagating the original exception. */
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            advance(ec);
        } catch (...) {
            release();
            throw;
        }
#else
        advance(ec);
#endif
    }

public:
    using iterator_category = input_iterator_tag;
    using value_type = directory_entry;
    using difference_type = ptrdiff_t;
    using pointer = const directory_entry*;
    using reference = const directory_entry&;

    directory_iterator() noexcept
        : state_(nullptr), snapshot_(), snapshot_active_(false),
          snapshot_generation_(0), view_(),
          view_owner_(nullptr), view_generation_(0), view_valid_(false) {}

    explicit directory_iterator(const path& p)
        : state_(nullptr), snapshot_(), snapshot_active_(false),
          snapshot_generation_(0), view_(),
          view_owner_(nullptr), view_generation_(0), view_valid_(false) {
        error_code ec;
        initialize(p, &ec, true, directory_options::none);
        if (ec) detail::filesystem_failure("directory_iterator", p, ec);
    }

    directory_iterator(const path& p, directory_options options)
        : state_(nullptr), snapshot_(), snapshot_active_(false),
          snapshot_generation_(0), view_(),
          view_owner_(nullptr), view_generation_(0), view_valid_(false) {
        error_code ec;
        initialize(p, &ec, true, options);
        if (ec) detail::filesystem_failure("directory_iterator", p, ec);
    }

    directory_iterator(const path& p, error_code& ec) noexcept
        : state_(nullptr), snapshot_(), snapshot_active_(false),
          snapshot_generation_(0), view_(),
          view_owner_(nullptr), view_generation_(0), view_valid_(false) {
        initialize(p, &ec, false, directory_options::none);
    }

    directory_iterator(const path& p, directory_options options,
                       error_code& ec) noexcept
        : state_(nullptr), snapshot_(), snapshot_active_(false),
          snapshot_generation_(0), view_(),
          view_owner_(nullptr), view_generation_(0), view_valid_(false) {
        initialize(p, &ec, false, options);
    }

    directory_iterator(const directory_iterator& other)
        : state_(other.state_), snapshot_(other.snapshot_),
          snapshot_active_(other.snapshot_active_),
          snapshot_generation_(other.snapshot_generation_), view_(),
          view_owner_(nullptr), view_generation_(0), view_valid_(false) {
        retain();
    }

    directory_iterator(directory_iterator&& other) noexcept
        : state_(other.state_), snapshot_(::std::move(other.snapshot_)),
          snapshot_active_(other.snapshot_active_),
          snapshot_generation_(other.snapshot_generation_), view_(),
          view_owner_(nullptr), view_generation_(0), view_valid_(false) {
        other.state_ = nullptr;
        other.snapshot_active_ = false;
        other.snapshot_generation_ = 0;
    }

    ~directory_iterator() {
        release();
    }

    directory_iterator& operator=(const directory_iterator& other) {
        if (this != &other) {
            directory_entry snapshot_copy(other.snapshot_);
            release();
            state_ = other.state_;
            snapshot_ = ::std::move(snapshot_copy);
            snapshot_active_ = other.snapshot_active_;
            snapshot_generation_ = other.snapshot_generation_;
            view_ = directory_entry();
            view_owner_ = nullptr;
            view_generation_ = 0;
            view_valid_ = false;
            retain();
        }
        return *this;
    }

    directory_iterator& operator=(directory_iterator&& other) noexcept {
        if (this != &other) {
            release();
            state_ = other.state_;
            snapshot_ = ::std::move(other.snapshot_);
            snapshot_active_ = other.snapshot_active_;
            snapshot_generation_ = other.snapshot_generation_;
            view_ = directory_entry();
            view_owner_ = nullptr;
            view_generation_ = 0;
            view_valid_ = false;
            other.state_ = nullptr;
            other.snapshot_active_ = false;
            other.snapshot_generation_ = 0;
        }
        return *this;
    }

    const directory_entry& operator*() const {
        return snapshot_active_ ? snapshot_ : current_entry();
    }
    const directory_entry* operator->() const {
        return snapshot_active_ ? &snapshot_ : &current_entry();
    }

    directory_iterator& operator++() {
        snapshot_active_ = false;
        snapshot_generation_ = 0;
        view_valid_ = false;
        error_code ec;
        advance(&ec);
        if (ec) {
            if (state_) {
                detail::filesystem_failure("directory_iterator::increment",
                                           state_->directory, ec);
            }
            detail::filesystem_failure("directory_iterator::increment", ec);
        }
        return *this;
    }

    directory_iterator operator++(int) {
        directory_iterator previous(*this);
        if (state_) {
            state_lock(state_);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            try {
                previous.snapshot_ = state_->entry;
            } catch (...) {
                state_unlock(state_);
                throw;
            }
#else
            previous.snapshot_ = state_->entry;
#endif
            previous.snapshot_generation_ = state_->generation;
            state_unlock(state_);
            previous.snapshot_active_ = true;
        }
        ++(*this);
        return previous;
    }

    directory_iterator& increment(error_code& ec) noexcept {
        snapshot_active_ = false;
        snapshot_generation_ = 0;
        view_valid_ = false;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            advance(&ec);
        } catch (...) {
            set_memory_error(&ec);
            finish(&ec);
        }
#else
        advance(&ec);
#endif
        return *this;
    }

    bool operator==(const directory_iterator& rhs) const {
        /* A postfix result is a detached value.  Its shared stream may have
         * since reached EOF, but the saved entry still denotes the position
         * observed before increment and must not compare equal to end(). */
        const bool left_end = !snapshot_active_ && state_at_end(state_);
        const bool right_end = !rhs.snapshot_active_ && state_at_end(rhs.state_);
        if (left_end || right_end) return left_end == right_end;
        if (state_ != rhs.state_) return false;
        if (!snapshot_active_ && !rhs.snapshot_active_) return true;
        const size_t left_generation = snapshot_active_
            ? snapshot_generation_ : state_generation(state_);
        const size_t right_generation = rhs.snapshot_active_
            ? rhs.snapshot_generation_ : state_generation(rhs.state_);
        return left_generation == right_generation;
    }

    bool operator!=(const directory_iterator& rhs) const {
        return !(*this == rhs);
    }
};

inline directory_iterator begin(directory_iterator iter) noexcept { return iter; }
inline directory_iterator end(directory_iterator) noexcept { return directory_iterator(); }

/* ═══════════════════════════════════════════════════════════════
 * recursive_directory_iterator
 * ═══════════════════════════════════════════════════════════════*/

class recursive_directory_iterator {
    vector<directory_iterator> stack_;
    directory_options options_;
    bool recursion_pending_;
    bool at_end_;
    directory_entry snapshot_;
    bool snapshot_active_;

    static void set_memory_error(error_code* ec) noexcept {
        if (ec) *ec = make_error_code(errc::not_enough_memory);
    }

    static bool permission_error(const error_code& ec) noexcept {
        return ec == make_error_code(errc::permission_denied) ||
               ec == make_error_code(errc::operation_not_permitted);
    }

    static bool has_option(directory_options value,
                           directory_options option) noexcept {
        return (static_cast<unsigned>(value) &
                static_cast<unsigned>(option)) != 0u;
    }

    void set_end() noexcept {
        stack_.clear();
        at_end_ = true;
        recursion_pending_ = false;
    }

    bool current_is_directory(error_code& ec) const {
        if (stack_.empty()) return false;

        /* `status()` follows a symlink, but recursive_directory_iterator is
         * required to recurse through one only when the caller explicitly
         * opts into follow_directory_symlink.  Inspect the link itself first
         * so a directory target cannot silently turn into a recursive edge.
         * This function is intentionally not noexcept: taking the iterator's
         * per-owner directory_entry snapshot may allocate, and the enclosing
         * increment transaction converts that exception to its error_code
         * failure boundary. */
        const directory_entry& entry = *stack_.back();
        if (!has_option(options_, directory_options::follow_directory_symlink)) {
            const file_status link_status = entry.symlink_status(ec);
            if (ec) return false;
            if (link_status.type() == file_type::symlink) return false;
        }
        ec.clear();
        return entry.is_directory();
    }

    void increment_impl(error_code& ec) {
        ec.clear();
        if (at_end_ || stack_.empty()) return;

        for (;;) {
            if (recursion_pending_) {
                error_code status_ec;
                const bool is_directory = current_is_directory(status_ec);
                if (status_ec) {
                    if (has_option(options_, directory_options::skip_permission_denied)) {
                        recursion_pending_ = false;
                    } else {
                        ec = status_ec;
                        set_end();
                        return;
                    }
                } else if (is_directory) {
                    error_code child_ec;
                    directory_iterator child(stack_.back()->path(), child_ec);
                    if (child_ec) {
                        if (!has_option(options_,
                                        directory_options::skip_permission_denied)) {
                            ec = child_ec;
                            set_end();
                            return;
                        }
                    } else if (child != directory_iterator()) {
                        stack_.push_back(::std::move(child));
                        recursion_pending_ = true;
                        return;
                    }
                    recursion_pending_ = false;
                } else {
                    recursion_pending_ = false;
                }
            }

            stack_.back().increment(ec);
            if (ec) {
                set_end();
                return;
            }
            if (stack_.back() != directory_iterator()) {
                recursion_pending_ = true;
                return;
            }

            stack_.pop_back();
            if (stack_.empty()) {
                set_end();
                return;
            }
            /* The parent iterator currently points at the directory just
             * exhausted; continue with the next sibling. */
            recursion_pending_ = false;
        }
    }

    void initialize(const path& root, directory_options options,
                    error_code& ec) {
        ec.clear();
        options_ = options;
        recursion_pending_ = true;
        at_end_ = false;
        directory_iterator first(root, options, ec);
        if (ec) {
            set_end();
            return;
        }
        if (first == directory_iterator()) {
            set_end();
            return;
        }
        stack_.push_back(::std::move(first));
    }

public:
    using iterator_category = input_iterator_tag;
    using value_type = directory_entry;
    using difference_type = ptrdiff_t;
    using pointer = const directory_entry*;
    using reference = const directory_entry&;

    recursive_directory_iterator() noexcept
        : stack_(), options_(directory_options::none),
          recursion_pending_(false), at_end_(true), snapshot_(),
          snapshot_active_(false) {}

    explicit recursive_directory_iterator(
        const path& root,
        directory_options options = directory_options::none)
        : stack_(), options_(options), recursion_pending_(false), at_end_(true),
          snapshot_(), snapshot_active_(false) {
        error_code ec;
        initialize(root, options, ec);
        if (ec) detail::filesystem_failure("recursive_directory_iterator", root, ec);
    }

    recursive_directory_iterator(const path& root, directory_options options,
                                 error_code& ec) noexcept
        : stack_(), options_(options), recursion_pending_(false), at_end_(true),
          snapshot_(), snapshot_active_(false) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            initialize(root, options, ec);
        } catch (...) {
            set_memory_error(&ec);
            set_end();
        }
#else
        initialize(root, options, ec);
#endif
    }

    recursive_directory_iterator(const path& root, error_code& ec) noexcept
        : recursive_directory_iterator(root, directory_options::none, ec) {}

    recursive_directory_iterator(const recursive_directory_iterator&) = default;
    recursive_directory_iterator(recursive_directory_iterator&&) noexcept = default;
    recursive_directory_iterator& operator=(const recursive_directory_iterator&) = default;
    recursive_directory_iterator& operator=(recursive_directory_iterator&&) noexcept = default;
    ~recursive_directory_iterator() = default;

    const directory_entry& operator*() const {
        return snapshot_active_ ? snapshot_ : *stack_.back();
    }
    const directory_entry* operator->() const {
        return snapshot_active_ ? &snapshot_ : stack_.back().operator->();
    }

    recursive_directory_iterator& operator++() {
        snapshot_active_ = false;
        error_code ec;
        increment(ec);
        if (ec) {
            if (!stack_.empty()) {
                detail::filesystem_failure("recursive_directory_iterator::increment",
                                           stack_.back()->path(), ec);
            }
            detail::filesystem_failure("recursive_directory_iterator::increment", ec);
        }
        return *this;
    }

    recursive_directory_iterator operator++(int) {
        recursive_directory_iterator previous(*this);
        if (!stack_.empty()) {
            previous.snapshot_ = snapshot_active_ ? snapshot_ : *stack_.back();
            previous.snapshot_active_ = true;
        }
        ++(*this);
        return previous;
    }

    recursive_directory_iterator& increment(error_code& ec) noexcept {
        snapshot_active_ = false;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            increment_impl(ec);
        } catch (...) {
            set_memory_error(&ec);
            set_end();
        }
#else
        increment_impl(ec);
#endif
        return *this;
    }

    void pop() {
        if (stack_.empty()) return;
        snapshot_active_ = false;
        recursion_pending_ = false;
        error_code ec;
        stack_.back().increment(ec);
        if (ec) {
            if (!stack_.empty()) {
                detail::filesystem_failure("recursive_directory_iterator::pop",
                                           stack_.back()->path(), ec);
            }
            detail::filesystem_failure("recursive_directory_iterator::pop", ec);
        }
        if (stack_.back() != directory_iterator()) return;
        stack_.pop_back();
        if (stack_.empty()) {
            set_end();
            return;
        }
        /* The parent iterator is already positioned at the directory that
         * pop skipped; continue with its next sibling. */
        pop();
    }

    void disable_recursion_pending() noexcept { recursion_pending_ = false; }
    bool recursion_pending() const noexcept { return recursion_pending_; }
    directory_options options() const noexcept { return options_; }
    unsigned depth() const noexcept {
        return stack_.empty() ? 0u : static_cast<unsigned>(stack_.size() - 1u);
    }

    bool operator==(const recursive_directory_iterator& rhs) const {
        const bool left_end = at_end_ || stack_.empty();
        const bool right_end = rhs.at_end_ || rhs.stack_.empty();
        if (left_end || right_end) return left_end == right_end;
        return stack_.size() == rhs.stack_.size() &&
               stack_.back() == rhs.stack_.back();
    }

    bool operator!=(const recursive_directory_iterator& rhs) const {
        return !(*this == rhs);
    }
};

inline recursive_directory_iterator begin(recursive_directory_iterator iter) noexcept {
    return iter;
}
inline recursive_directory_iterator end(recursive_directory_iterator) noexcept {
    return recursive_directory_iterator();
}

/* ═══════════════════════════════════════════════════════════════
 * フリー関数
 * ═══════════════════════════════════════════════════════════════*/

/* exists */
inline bool status_known(file_status value) noexcept {
    return value.type() != file_type::none &&
           value.type() != file_type::unknown;
}

inline bool exists(file_status value) noexcept {
    return status_known(value) && value.type() != file_type::not_found;
}

inline bool exists(const directory_entry& entry) noexcept {
    return entry.exists();
}

inline bool exists(const path& p) {
    error_code ec;
    directory_entry entry(p, ec);
    const bool result = !ec && entry.exists();
    /* A missing leaf is a normal negative query.  Preserve provider errors
     * for the error-code overload, but keep the throwing convenience form
     * equivalent to the standard status-known/not-found predicate. */
    if (ec && ec != make_error_code(errc::no_such_file_or_directory) &&
        ec != make_error_code(errc::not_a_directory)) {
        detail::filesystem_failure("exists", p, ec);
    }
    return result;
}

inline bool exists(const path& p, error_code& ec) noexcept {
    directory_entry entry(p, ec);
    return !ec && entry.exists();
}

/* is_directory */
inline bool is_directory(file_status value) noexcept {
    return value.type() == file_type::directory;
}

inline bool is_directory(const directory_entry& entry) noexcept {
    return entry.is_directory();
}

inline bool is_directory(const path& p, error_code& ec) noexcept {
    directory_entry de(p, ec);
    return !ec && de.is_directory();
}

inline bool is_directory(const path& p) {
    error_code ec;
    const bool result = is_directory(p, ec);
    if (ec) detail::filesystem_failure("is_directory", p, ec);
    return result;
}

/* is_regular_file */
inline bool is_regular_file(file_status value) noexcept {
    return value.type() == file_type::regular;
}

inline bool is_regular_file(const directory_entry& entry) noexcept {
    return entry.is_regular_file();
}

inline bool is_symlink(file_status value) noexcept {
    return value.type() == file_type::symlink;
}

inline bool is_symlink(const directory_entry& entry) noexcept {
    return entry.is_symlink();
}

inline bool is_block_file(file_status value) noexcept {
    return value.type() == file_type::block;
}

inline bool is_block_file(const directory_entry& entry) noexcept {
    return entry.is_block_file();
}

inline bool is_character_file(file_status value) noexcept {
    return value.type() == file_type::character;
}

inline bool is_character_file(const directory_entry& entry) noexcept {
    return entry.is_character_file();
}

inline bool is_fifo(file_status value) noexcept {
    return value.type() == file_type::fifo;
}

inline bool is_fifo(const directory_entry& entry) noexcept {
    return entry.is_fifo();
}

inline bool is_socket(file_status value) noexcept {
    return value.type() == file_type::socket;
}

inline bool is_socket(const directory_entry& entry) noexcept {
    return entry.is_socket();
}

inline bool is_other(file_status value) noexcept {
    return exists(value) && !is_regular_file(value) &&
           !is_directory(value) && !is_symlink(value);
}

inline bool is_other(const directory_entry& entry) noexcept {
    return entry.is_other();
}

inline file_status symlink_status(const path& p, error_code& ec) noexcept {
    ec.clear();
    detail::stat_record st = {};
    const int result = rin_lstat(p.c_str(), &st);
    volatile detail::stat_record* view = &st;
    if (result != 0 || view->st_size < 0) {
        ec = make_error_code(detail::filesystem_error_from_errno());
        return file_status(file_type::not_found);
    }
    const unsigned int mode = view->st_mode;
    file_type type = file_type::unknown;
    if ((mode & 0170000) == 0040000) {
        type = file_type::directory;
    } else if ((mode & 0170000) == 0100000) {
        type = file_type::regular;
    } else if ((mode & 0170000) == 0120000) {
        type = file_type::symlink;
    } else if ((mode & 0170000) == 0060000) {
        type = file_type::block;
    } else if ((mode & 0170000) == 0020000) {
        type = file_type::character;
    } else if ((mode & 0170000) == 0010000) {
        type = file_type::fifo;
    } else if ((mode & 0170000) == 0140000) {
        type = file_type::socket;
    }
    return file_status(type, static_cast<perms>(mode & 0777));
}

inline file_status symlink_status(const path& p) {
    error_code ec;
    const file_status result = symlink_status(p, ec);
    if (ec) detail::filesystem_failure("symlink_status", p, ec);
    return result;
}

inline bool is_symlink(const path& p, error_code& ec) noexcept {
    /* `is_symlink(path)` is specified in terms of symlink_status, not
     * status: the latter follows a link and would report the target type.
     * Keep the non-throwing overload on the lstat-backed path so a link to a
     * regular file is still recognized as a symlink. */
    const file_status value = symlink_status(p, ec);
    return !ec && value.type() == file_type::symlink;
}

inline bool is_symlink(const path& p) {
    error_code ec;
    const bool result = is_symlink(p, ec);
    if (ec) detail::filesystem_failure("is_symlink", p, ec);
    return result;
}

inline bool is_block_file(const path& p, error_code& ec) noexcept {
    directory_entry de(p, ec);
    return !ec && de.is_block_file();
}

inline bool is_block_file(const path& p) {
    error_code ec;
    const bool result = is_block_file(p, ec);
    if (ec) detail::filesystem_failure("is_block_file", p, ec);
    return result;
}

inline bool is_character_file(const path& p, error_code& ec) noexcept {
    directory_entry de(p, ec);
    return !ec && de.is_character_file();
}

inline bool is_character_file(const path& p) {
    error_code ec;
    const bool result = is_character_file(p, ec);
    if (ec) detail::filesystem_failure("is_character_file", p, ec);
    return result;
}

inline bool is_fifo(const path& p, error_code& ec) noexcept {
    directory_entry de(p, ec);
    return !ec && de.is_fifo();
}

inline bool is_fifo(const path& p) {
    error_code ec;
    const bool result = is_fifo(p, ec);
    if (ec) detail::filesystem_failure("is_fifo", p, ec);
    return result;
}

inline bool is_socket(const path& p, error_code& ec) noexcept {
    directory_entry de(p, ec);
    return !ec && de.is_socket();
}

inline bool is_socket(const path& p) {
    error_code ec;
    const bool result = is_socket(p, ec);
    if (ec) detail::filesystem_failure("is_socket", p, ec);
    return result;
}

inline bool is_other(const path& p, error_code& ec) noexcept {
    directory_entry de(p, ec);
    return !ec && de.is_other();
}

inline bool is_other(const path& p) {
    error_code ec;
    const bool result = is_other(p, ec);
    if (ec) detail::filesystem_failure("is_other", p, ec);
    return result;
}

inline bool is_regular_file(const path& p, error_code& ec) noexcept {
    directory_entry de(p, ec);
    return !ec && de.is_regular_file();
}

inline bool is_regular_file(const path& p) {
    error_code ec;
    const bool result = is_regular_file(p, ec);
    if (ec) detail::filesystem_failure("is_regular_file", p, ec);
    return result;
}

/* file_size */
/* last_write_time */
inline file_time_type last_write_time(const path& p, error_code& ec) noexcept {
    directory_entry de(p, ec);
    if (ec) return file_time_type::min();
    return de.last_write_time();
}

inline file_time_type last_write_time(const path& p) {
    error_code ec;
    const file_time_type result = last_write_time(p, ec);
    if (ec) detail::filesystem_failure("last_write_time", p, ec);
    return result;
}

inline void last_write_time(const path& p, file_time_type value,
                            error_code& ec) noexcept {
    ec.clear();
    /* The Rin path-time ABI updates access and write timestamps together.
     * Snapshot access time first so this C++ setter changes only the requested
     * write timestamp and never publishes a partially updated result. */
    detail::stat_record stat = {};
    if (rin_stat(p.c_str(), &stat) != 0) {
        ec = make_error_code(detail::filesystem_error_from_errno());
        return;
    }
    const long long write_seconds = static_cast<long long>(
        ::std::chrono::duration_cast<::std::chrono::seconds>(
            value.time_since_epoch()).count());
    if (rin_set_times(p.c_str(), static_cast<long long>(stat.st_atime),
                      write_seconds) != 0) {
        ec = make_error_code(detail::filesystem_error_from_errno());
    }
}

inline void last_write_time(const path& p, file_time_type value) {
    error_code ec;
    last_write_time(p, value, ec);
    if (ec) detail::filesystem_failure("last_write_time", p, ec);
}

/* hard_link_count */
inline uintmax_t hard_link_count(const path& p, error_code& ec) noexcept {
    directory_entry de(p, ec);
    if (ec) return static_cast<uintmax_t>(-1);
    return de.hard_link_count();
}

inline uintmax_t hard_link_count(const path& p) {
    error_code ec;
    const uintmax_t result = hard_link_count(p, ec);
    if (ec) detail::filesystem_failure("hard_link_count", p, ec);
    return result;
}

/* file_size */
inline uintmax_t file_size(const path& p, error_code& ec) noexcept {
    directory_entry de(p, ec);
    if (ec) return static_cast<uintmax_t>(-1);
    if (!de.is_regular_file()) {
        ec = make_error_code(de.is_directory() ? errc::is_a_directory
                                               : errc::io_error);
        return static_cast<uintmax_t>(-1);
    }
    return de.file_size();
}

inline uintmax_t file_size(const path& p) {
    error_code ec;
    const uintmax_t result = file_size(p, ec);
    if (ec) detail::filesystem_failure("file_size", p, ec);
    return result;
}

inline bool is_empty(const path& p, error_code& ec) noexcept {
    ec.clear();
    directory_entry entry(p, ec);
    if (ec) return false;
    if (entry.is_regular_file()) {
        return entry.file_size(ec) == 0u && !ec;
    }
    if (!entry.is_directory()) {
        ec = make_error_code(errc::io_error);
        return false;
    }
    directory_iterator iter(p, ec);
    if (ec) return false;
    return iter == directory_iterator();
}

inline bool is_empty(const path& p) {
    error_code ec;
    const bool result = is_empty(p, ec);
    if (ec) detail::filesystem_failure("is_empty", p, ec);
    return result;
}

/* create_directory */
inline bool create_directory(const path& p, error_code& ec) noexcept {
    ec.clear();
    if (rin_mkdir(p.c_str(), 0755) != 0) {
        /* POSIX mkdir reports EEXIST for both an already-created directory
         * and a conflicting non-directory.  The standard create_directory
         * contract treats the former as a clean "already present" result,
         * so preserve the provider error while rechecking the exact node.
         * The recheck is only an interpretation of EEXIST; permission,
         * parent and I/O failures remain errors and are never guessed away. */
        const errc mkdir_error = detail::filesystem_error_from_errno();
        if (mkdir_error == errc::file_exists) {
            detail::stat_record st = {};
            volatile detail::stat_record* view = &st;
            if (rin_stat(p.c_str(), &st) == 0 && view->st_size >= 0 &&
                (view->st_mode & 0170000) == 0040000) {
                ec.clear();
                return false;
            }
        }
        ec = make_error_code(mkdir_error);
        return false;
    }
    return true;
}

inline bool create_directory(const path& p) {
    error_code ec;
    const bool created = create_directory(p, ec);
    if (ec) detail::filesystem_failure("create_directory", p, ec);
    return created;
}

/* create_directories */
inline bool create_directories(const path& p, error_code& ec) noexcept {
    ec.clear();
    if (p.empty()) return false;

    /* `exists(path)` is the throwing query.  A recursive create must probe
     * through the non-throwing overload so ENOENT means "create this node",
     * while permission/provider failures still abort before any mutation. */
    error_code probe_ec;
    if (exists(p, probe_ec)) {
        /* An existing directory is the standard idempotent result.  An
         * existing non-directory is a conflict and must be reported rather
         * than silently treated as a successful no-op. */
        error_code type_ec;
        if (is_directory(p, type_ec)) return false;
        ec = type_ec ? type_ec : make_error_code(errc::file_exists);
        return false;
    }
    if (probe_ec &&
        probe_ec != make_error_code(errc::no_such_file_or_directory)) {
        ec = probe_ec;
        return false;
    }

    const path parent = p.parent_path();
    if (!parent.empty() && parent != p) {
        probe_ec.clear();
        if (!exists(parent, probe_ec)) {
            if (probe_ec &&
                probe_ec != make_error_code(errc::no_such_file_or_directory)) {
                ec = probe_ec;
                return false;
            }
            if (!create_directories(parent, ec) && ec) return false;
        }
    }
    return create_directory(p, ec);
}

inline bool create_directories(const path& p) {
    error_code ec;
    const bool created = create_directories(p, ec);
    if (ec) detail::filesystem_failure("create_directories", p, ec);
    return created;
}

/* permissions */
inline void permissions(const path& p, perms mode, perm_options options,
                        error_code& ec) noexcept {
    ec.clear();
    const unsigned int option_bits = static_cast<unsigned>(options);
    const unsigned int all_flags = static_cast<unsigned>(perm_options::replace |
                                                          perm_options::add |
                                                          perm_options::remove |
                                                          perm_options::nofollow);
    if ((option_bits & ~all_flags) != 0u ||
        ((option_bits & static_cast<unsigned>(perm_options::add)) != 0u &&
         (option_bits & static_cast<unsigned>(perm_options::remove)) != 0u) ||
        ((option_bits & static_cast<unsigned>(perm_options::replace)) != 0u &&
         (option_bits & static_cast<unsigned>(perm_options::add |
                                               perm_options::remove)) != 0u)) {
        ec = make_error_code(errc::invalid_argument);
        return;
    }
    unsigned int target = static_cast<unsigned>(mode) & 0777u;
    if ((option_bits & static_cast<unsigned>(perm_options::add |
                                             perm_options::remove)) != 0u) {
        directory_entry de(p, ec);
        if (ec) return;
        target = static_cast<unsigned>(de.status().permissions()) & 0777u;
        if (option_bits & static_cast<unsigned>(perm_options::add)) {
            target |= static_cast<unsigned>(mode) & 0777u;
        } else {
            target &= ~(static_cast<unsigned>(mode) & 0777u);
        }
    }
    const int result = option_bits & static_cast<unsigned>(perm_options::nofollow)
        ? rin_chmod_nofollow(p.c_str(), target)
        : rin_chmod(p.c_str(), target);
    if (result != 0)
        ec = make_error_code(detail::filesystem_error_from_errno());
}

inline void permissions(const path& p, perms mode,
                        perm_options options = perm_options::replace) {
    error_code ec;
    permissions(p, mode, options, ec);
    if (ec) detail::filesystem_failure("permissions", p, ec);
}

/* remove */
inline bool remove(const path& p, error_code& ec) noexcept {
    ec.clear();
    /* A failed metadata probe is not permission to guess the object type or
     * call unlink.  Preserve the probe failure and keep the operation
     * failure-atomic. */
    error_code status_ec;
    const bool directory = is_directory(p, status_ec);
    if (status_ec) {
        ec = status_ec;
        return false;
    }
    const bool removed = directory
        ? rin_rmdir(p.c_str()) == 0
        : rin_unlink(p.c_str()) == 0;
    if (!removed) {
        ec = make_error_code(detail::filesystem_error_from_errno());
        return false;
    }
    return true;
}

inline uintmax_t remove_all(const path& p, error_code& ec) noexcept {
    ec.clear();
    error_code status_ec;
    const bool directory = is_directory(p, status_ec);
    if (status_ec) {
        ec = status_ec;
        return static_cast<uintmax_t>(-1);
    }
    if (!directory) {
        if (rin_unlink(p.c_str()) != 0) {
            ec = make_error_code(detail::filesystem_error_from_errno());
            return static_cast<uintmax_t>(-1);
        }
        return 1;
    }
    uintmax_t removed = 0;
    directory_iterator iter(p, ec);
    if (ec) return static_cast<uintmax_t>(-1);
    const directory_iterator finish;
    while (iter != finish) {
        const uintmax_t child = remove_all(iter->path(), ec);
        if (ec) return static_cast<uintmax_t>(-1);
        if (child > static_cast<uintmax_t>(-1) - removed) {
            ec = make_error_code(errc::value_too_large);
            return static_cast<uintmax_t>(-1);
        }
        removed += child;
        iter.increment(ec);
        if (ec) return static_cast<uintmax_t>(-1);
    }
    if (rin_rmdir(p.c_str()) != 0 || removed == static_cast<uintmax_t>(-1)) {
        ec = make_error_code(detail::filesystem_error_from_errno());
        return static_cast<uintmax_t>(-1);
    }
    return removed + 1;
}

inline uintmax_t remove_all(const path& p) {
    error_code ec;
    const uintmax_t removed = remove_all(p, ec);
    if (ec) detail::filesystem_failure("remove_all", p, ec);
    return removed;
}

inline bool remove(const path& p) {
    error_code ec;
    const bool removed = remove(p, ec);
    if (ec) detail::filesystem_failure("remove", p, ec);
    return removed;
}

/* symlink creation */
inline void create_symlink(const path& target, const path& link,
                           error_code& ec) noexcept {
    ec.clear();
    if (rin_symlink(target.c_str(), link.c_str()) != 0) {
        ec = make_error_code(detail::filesystem_error_from_errno());
    }
}

inline void create_symlink(const path& target, const path& link) {
    error_code ec;
    create_symlink(target, link, ec);
    if (ec) detail::filesystem_failure("create_symlink", link, ec);
}

inline void create_directory_symlink(const path& target, const path& link,
                                     error_code& ec) noexcept {
    ec.clear();
    if (rin_symlink(target.c_str(), link.c_str()) != 0) {
        ec = make_error_code(detail::filesystem_error_from_errno());
    }
}

inline void create_directory_symlink(const path& target, const path& link) {
    error_code ec;
    create_directory_symlink(target, link, ec);
    if (ec) detail::filesystem_failure("create_directory_symlink", link, ec);
}

inline void create_hard_link(const path& existing, const path& link,
                             error_code& ec) noexcept {
    ec.clear();
    if (rin_link(existing.c_str(), link.c_str()) != 0) {
        ec = make_error_code(detail::filesystem_error_from_errno());
    }
}

inline void create_hard_link(const path& existing, const path& link) {
    error_code ec;
    create_hard_link(existing, link, ec);
    if (ec) detail::filesystem_failure("create_hard_link", link, ec);
}

inline path read_symlink(const path& p, error_code& ec) {
    ec.clear();
    char buffer[4096];
    const int result = rin_readlink(p.c_str(), buffer, sizeof(buffer));
    if (result < 0) {
        ec = make_error_code(detail::filesystem_error_from_errno());
        return path();
    }
    if (static_cast<unsigned long>(result) >= sizeof(buffer)) {
        ec = make_error_code(errc::value_too_large);
        return path();
    }
    buffer[result] = '\0';
    return path(buffer);
}

inline path read_symlink(const path& p) {
    error_code ec;
    const path result = read_symlink(p, ec);
    if (ec) detail::filesystem_failure("read_symlink", p, ec);
    return result;
}

inline bool equivalent(const path& left, const path& right,
                       error_code& ec) noexcept {
    ec.clear();
    detail::stat_record left_stat = {};
    detail::stat_record right_stat = {};
    if (rin_stat(left.c_str(), &left_stat) != 0 ||
        rin_stat(right.c_str(), &right_stat) != 0) {
        ec = make_error_code(detail::filesystem_error_from_errno());
        return false;
    }
    return left_stat.st_dev == right_stat.st_dev &&
           left_stat.st_ino == right_stat.st_ino;
}

inline bool equivalent(const path& left, const path& right) {
    error_code ec;
    const bool result = equivalent(left, right, ec);
    if (ec) detail::filesystem_failure("equivalent", left, right, ec);
    return result;
}

/* copy_file: bounded regular-file byte transfer.  Unsupported copy modes are
 * rejected before either descriptor is opened; this prevents a caller from
 * mistaking a directory/symlink policy fallback for a successful copy. */
inline bool copy_file(const path& from, const path& to, copy_options options,
                      error_code& ec) noexcept {
    ec.clear();
    const unsigned int bits = static_cast<unsigned>(options);
    const unsigned int supported =
        static_cast<unsigned>(copy_options::skip_existing) |
        static_cast<unsigned>(copy_options::overwrite_existing) |
        static_cast<unsigned>(copy_options::update_existing);
    const unsigned int replacement =
        bits & (static_cast<unsigned>(copy_options::skip_existing) |
                static_cast<unsigned>(copy_options::overwrite_existing) |
                static_cast<unsigned>(copy_options::update_existing));
    if ((bits & ~supported) != 0u ||
        replacement == (static_cast<unsigned>(copy_options::skip_existing) |
                        static_cast<unsigned>(copy_options::overwrite_existing)) ||
        replacement == (static_cast<unsigned>(copy_options::skip_existing) |
                        static_cast<unsigned>(copy_options::update_existing)) ||
        replacement == (static_cast<unsigned>(copy_options::overwrite_existing) |
                        static_cast<unsigned>(copy_options::update_existing))) {
        ec = make_error_code(errc::invalid_argument);
        return false;
    }

    detail::stat_record source = {};
    volatile detail::stat_record* source_view = &source;
    const int source_status = rin_stat(from.c_str(), &source);
    if (source_status != 0) {
        ec = make_error_code(detail::filesystem_error_from_errno());
        return false;
    }
    if (source_view->st_size < 0) {
        ec = make_error_code(errc::io_error);
        return false;
    }
    if ((source_view->st_mode & 0170000) != 0100000) {
        ec = make_error_code((source_view->st_mode & 0170000) == 0040000
                                 ? errc::is_a_directory
                                 : errc::io_error);
        return false;
    }

    detail::stat_record destination = {};
    volatile detail::stat_record* destination_view = &destination;
    /* A destination symlink must be treated as the destination node itself.
     * Following it here would let overwrite_existing mutate an unrelated
     * target outside the requested tree. */
    const int destination_status = rin_lstat(to.c_str(), &destination);
    if (destination_status != 0) {
        const errc destination_error = detail::filesystem_error_from_errno();
        if (destination_error != errc::no_such_file_or_directory) {
            ec = make_error_code(destination_error);
            return false;
        }
    } else if (destination_view->st_size < 0) {
        ec = make_error_code(errc::io_error);
        return false;
    }
    const bool destination_exists = destination_status == 0;
    if (destination_exists) {
        if ((destination_view->st_mode & 0170000) != 0100000) {
            ec = make_error_code((destination_view->st_mode & 0170000) == 0040000
                                     ? errc::is_a_directory
                                     : errc::io_error);
            return false;
        }
        if (source_view->st_dev == destination_view->st_dev &&
            source_view->st_ino == destination_view->st_ino) {
            ec = make_error_code(errc::invalid_argument);
            return false;
        }
        if ((bits & static_cast<unsigned>(copy_options::skip_existing)) != 0u)
            return false;
        if ((bits & static_cast<unsigned>(copy_options::update_existing)) != 0u &&
            source_view->st_mtime <= destination_view->st_mtime)
            return false;
        if (replacement == 0u) {
            ec = make_error_code(errc::file_exists);
            return false;
        }
    }

    const int source_fd = rin_open(from.c_str(), detail::copy_open_read);
    if (source_fd < 0) {
        ec = make_error_code(detail::filesystem_error_from_errno());
        return false;
    }
    const int destination_fd = rin_open(
        to.c_str(), detail::copy_open_write | detail::copy_open_create |
        detail::copy_open_trunc);
    if (destination_fd < 0) {
        (void)rin_close(source_fd);
        ec = make_error_code(detail::filesystem_error_from_errno());
        return false;
    }

    bool okay = true;
    unsigned char buffer[4096];
    while (okay) {
        const long read_count = rin_read(source_fd, buffer, sizeof(buffer));
        if (read_count < 0) {
            ec = make_error_code(detail::filesystem_error_from_errno());
            okay = false;
            break;
        }
        if (read_count > static_cast<long>(sizeof(buffer))) {
            ec = make_error_code(errc::io_error);
            okay = false;
            break;
        }
        if (read_count == 0) break;
        long transferred = 0;
        while (transferred < read_count) {
            const long written = rin_write(
                destination_fd, buffer + transferred,
                static_cast<unsigned long>(read_count - transferred));
            if (written < 0) {
                ec = make_error_code(detail::filesystem_error_from_errno());
                okay = false;
                break;
            }
            if (written == 0 || written > read_count - transferred) {
                ec = make_error_code(errc::io_error);
                okay = false;
                break;
            }
            transferred += written;
        }
    }
    const int source_close = rin_close(source_fd);
    const errc source_close_error = source_close != 0
        ? detail::filesystem_error_from_errno() : errc::io_error;
    const int destination_close = rin_close(destination_fd);
    const errc destination_close_error = destination_close != 0
        ? detail::filesystem_error_from_errno() : errc::io_error;
    if (source_close != 0 || destination_close != 0) {
        if (okay) {
            ec = make_error_code(source_close != 0 ? source_close_error
                                                   : destination_close_error);
        }
        okay = false;
    }
    if (!okay) {
        if (!ec) ec = make_error_code(errc::io_error);
        return false;
    }
    return true;
}

inline bool copy_file(const path& from, const path& to, error_code& ec) noexcept {
    return copy_file(from, to, copy_options::none, ec);
}

inline bool copy_file(const path& from, const path& to,
                      copy_options options = copy_options::none) {
    error_code ec;
    const bool result = copy_file(from, to, options, ec);
    if (ec) detail::filesystem_failure("copy_file", from, to, ec);
    return result;
}

namespace detail {

/* Bounded create_hard_links branch for copy().  It only publishes a link at a
 * missing destination (or performs skip_existing); replacing an existing
 * node would require an atomic unlink/link transaction that the provider ABI
 * does not expose yet. */
inline bool copy_hard_link(const path& from, const path& to,
                           unsigned int replacement, error_code& ec) noexcept {
    stat_record destination = {};
    volatile stat_record* destination_view = &destination;
    const int destination_status = rin_stat(to.c_str(), &destination);
    if (destination_status != 0) {
        const errc destination_error = filesystem_error_from_errno();
        if (destination_error != errc::no_such_file_or_directory) {
            ec = make_error_code(destination_error);
            return false;
        }
    } else if (destination_view->st_size < 0) {
        ec = make_error_code(errc::io_error);
        return false;
    }
    if (destination_status == 0) {
        if (replacement == static_cast<unsigned>(copy_options::skip_existing))
            return true;
        ec = replacement == 0u ? make_error_code(errc::file_exists)
                               : make_error_code(errc::not_supported);
        return false;
    }
    if (rin_link(from.c_str(), to.c_str()) != 0) {
        ec = make_error_code(filesystem_error_from_errno());
        return false;
    }
    return true;
}

/* Bounded symbolic-link publication used by copy_symlinks and
 * create_symlinks.  The target is collected before the destination is
 * mutated, and existing nodes are never unlinked implicitly. */
inline bool copy_symlink_node(const path& from, const path& to,
                              unsigned int replacement, bool preserve_target,
                              error_code& ec) noexcept {
    stat_record destination = {};
    volatile stat_record* destination_view = &destination;
    const int destination_status = rin_lstat(to.c_str(), &destination);
    if (destination_status != 0) {
        const errc destination_error = filesystem_error_from_errno();
        if (destination_error != errc::no_such_file_or_directory) {
            ec = make_error_code(destination_error);
            return false;
        }
    } else if (destination_view->st_size < 0) {
        ec = make_error_code(errc::io_error);
        return false;
    }
    if (destination_status == 0) {
        if (replacement == static_cast<unsigned>(copy_options::skip_existing))
            return true;
        ec = replacement == 0u ? make_error_code(errc::file_exists)
                               : make_error_code(errc::not_supported);
        return false;
    }

    char target[4096];
    if (preserve_target) {
        const int target_size = rin_readlink(from.c_str(), target,
                                             sizeof(target));
        if (target_size < 0) {
            ec = make_error_code(filesystem_error_from_errno());
            return false;
        }
        if (static_cast<unsigned long>(target_size) >= sizeof(target)) {
            ec = make_error_code(errc::value_too_large);
            return false;
        }
        target[target_size] = '\0';
    } else {
        const char* source = from.c_str();
        unsigned long length = 0;
        while (source[length] != '\0') {
            ++length;
            if (length >= sizeof(target)) {
                ec = make_error_code(errc::filename_too_long);
                return false;
            }
        }
        for (unsigned long i = 0; i <= length; ++i)
            target[i] = source[i];
    }
    if (rin_symlink(target, to.c_str()) != 0) {
        ec = make_error_code(filesystem_error_from_errno());
        return false;
    }
    return true;
}

/* Bounded directory-tree copier used by copy(recursive).  Directory nodes are
 * created before their descendants; regular leaves reuse copy_file so their
 * replacement policy, identity checks, and provider error mapping stay in one
 * implementation.  Links and other special nodes remain explicit failures
 * (or are omitted by directories_only). */
inline bool copy_directory_nodes(const path& from, const path& to,
                                 unsigned int bits, unsigned int depth,
                                 error_code& ec) noexcept {
    if (depth > 64u) {
        ec = make_error_code(errc::not_supported);
        return false;
    }

    stat_record destination = {};
    volatile stat_record* destination_view = &destination;
    const int destination_status = rin_stat(to.c_str(), &destination);
    if (destination_status != 0) {
        const errc destination_error = filesystem_error_from_errno();
        if (destination_error != errc::no_such_file_or_directory) {
            ec = make_error_code(destination_error);
            return false;
        }
    } else if (destination_view->st_size < 0) {
        ec = make_error_code(errc::io_error);
        return false;
    }
    bool destination_exists = destination_status == 0;
    if (destination_exists) {
        if ((destination_view->st_mode & 0170000) != 0040000) {
            /* A directory source cannot be copied into a regular (or other
             * non-directory) destination.  Preserve the POSIX boundary as
             * not_a_directory instead of collapsing the conflict into a
             * generic I/O error. */
            ec = make_error_code(errc::not_a_directory);
            return false;
        }
        if ((bits & static_cast<unsigned>(copy_options::skip_existing)) != 0u)
            return true;
    } else if (rin_mkdir(to.c_str(), 0755u) != 0) {
        /* A concurrent creator can win between the metadata probe and
         * mkdir.  Interpret only EEXIST by one exact metadata recheck; keep
         * all other provider failures as errors and never guess a directory
         * into existence. */
        const errc mkdir_error = filesystem_error_from_errno();
        if (mkdir_error != errc::file_exists) {
            ec = make_error_code(mkdir_error);
            return false;
        }
        stat_record raced = {};
        volatile stat_record* raced_view = &raced;
        const int raced_status = rin_stat(to.c_str(), &raced);
        if (raced_status != 0) {
            const errc raced_error = filesystem_error_from_errno();
            ec = make_error_code(raced_error);
            return false;
        }
        if (raced_view->st_size < 0) {
            ec = make_error_code(errc::io_error);
            return false;
        }
        if ((raced_view->st_mode & 0170000) != 0040000) {
            ec = make_error_code(errc::not_a_directory);
            return false;
        }
        destination_exists = true;
    }

    /* A raced directory has the same skip_existing meaning as one observed
     * by the initial probe: do not traverse or publish any descendant. */
    if (destination_exists &&
        (bits & static_cast<unsigned>(copy_options::skip_existing)) != 0u)
        return true;

    if ((bits & static_cast<unsigned>(copy_options::recursive)) == 0u)
        return true;

    directory_iterator iter(from, ec);
    if (ec) return false;
    const directory_iterator finish;
    while (iter != finish) {
        const path child_from = iter->path();
        const path child_to = to / child_from.filename();
        const bool skip_links =
            (bits & static_cast<unsigned>(copy_options::skip_symlinks)) != 0u;
        const bool copy_links =
            (bits & static_cast<unsigned>(copy_options::copy_symlinks)) != 0u &&
            (bits & static_cast<unsigned>(copy_options::directories_only)) == 0u;
        if (skip_links || copy_links) {
            stat_record link_status = {};
            volatile stat_record* link_view = &link_status;
            if (rin_lstat(child_from.c_str(), &link_status) != 0 ||
                link_view->st_size < 0) {
                ec = link_view->st_size < 0
                    ? make_error_code(errc::io_error)
                    : make_error_code(filesystem_error_from_errno());
                return false;
            }
            if ((link_view->st_mode & 0170000) == 0120000) {
                if (copy_links) {
                    const unsigned int replacement =
                        bits & (static_cast<unsigned>(copy_options::skip_existing) |
                                static_cast<unsigned>(copy_options::overwrite_existing) |
                                static_cast<unsigned>(copy_options::update_existing));
                    (void)copy_symlink_node(child_from, child_to, replacement,
                                             true, ec);
                    if (ec) return false;
                }
                iter.increment(ec);
                if (ec) return false;
                continue;
            }
        }
        stat_record child_status = {};
        volatile stat_record* child_view = &child_status;
        if (rin_stat(child_from.c_str(), &child_status) != 0 ||
            child_view->st_size < 0) {
            ec = child_view->st_size < 0
                ? make_error_code(errc::io_error)
                : make_error_code(filesystem_error_from_errno());
            return false;
        }
        const unsigned int child_type = child_view->st_mode & 0170000;
        if (child_type == 0100000 &&
            (bits & static_cast<unsigned>(copy_options::directories_only)) == 0u) {
            const unsigned int replacement =
                bits & (static_cast<unsigned>(copy_options::skip_existing) |
                        static_cast<unsigned>(copy_options::overwrite_existing) |
                        static_cast<unsigned>(copy_options::update_existing));
            if ((bits & static_cast<unsigned>(copy_options::create_symlinks)) != 0u) {
                (void)copy_symlink_node(child_from, child_to, replacement,
                                        false, ec);
            } else {
                (void)copy_file(child_from, child_to,
                                static_cast<copy_options>(replacement), ec);
            }
            if (ec) return false;
            iter.increment(ec);
            if (ec) return false;
            continue;
        } else if (child_type != 0040000) {
            /* `directories_only` intentionally omits regular files and other
             * leaf nodes from a recursive walk.  They are not an unsupported
             * operation: advance the source iterator and keep copying later
             * directory descendants. */
            if ((bits & static_cast<unsigned>(copy_options::directories_only)) != 0u) {
                iter.increment(ec);
                if (ec) return false;
                continue;
            }
            ec = make_error_code(errc::not_supported);
            return false;
        }
        if (!copy_directory_nodes(child_from, child_to, bits, depth + 1u,
                                  ec))
            return false;
        iter.increment(ec);
        if (ec) return false;
    }
    return true;
}

} /* namespace detail */

/* copy: regular files (the recursive bit is irrelevant for a file) and
 * recursive directory trees share the bounded copy_file transfer.  Symlink,
 * hard-link, and durable-publication modes remain explicit unsupported cases;
 * they are never reported as a successful no-op. */
inline void copy(const path& from, const path& to, copy_options options,
                 error_code& ec) noexcept {
    ec.clear();
    const unsigned int bits = static_cast<unsigned>(options);
    const unsigned int replacement =
        bits & (static_cast<unsigned>(copy_options::skip_existing) |
                static_cast<unsigned>(copy_options::overwrite_existing) |
                static_cast<unsigned>(copy_options::update_existing));
    const unsigned int unsupported =
        bits & ~(static_cast<unsigned>(copy_options::skip_existing) |
                 static_cast<unsigned>(copy_options::overwrite_existing) |
                 static_cast<unsigned>(copy_options::update_existing) |
                 static_cast<unsigned>(copy_options::directories_only) |
                 static_cast<unsigned>(copy_options::recursive) |
                 static_cast<unsigned>(copy_options::create_hard_links) |
                 static_cast<unsigned>(copy_options::skip_symlinks) |
                 static_cast<unsigned>(copy_options::copy_symlinks) |
                 static_cast<unsigned>(copy_options::create_symlinks));
    if (unsupported != 0u) {
        ec = make_error_code(errc::not_supported);
        return;
    }
    const unsigned int link_modes =
        bits & (static_cast<unsigned>(copy_options::skip_symlinks) |
                static_cast<unsigned>(copy_options::copy_symlinks) |
                static_cast<unsigned>(copy_options::create_symlinks) |
                static_cast<unsigned>(copy_options::create_hard_links));
    if ((link_modes & static_cast<unsigned>(copy_options::skip_symlinks)) != 0u &&
        (link_modes & ~(static_cast<unsigned>(copy_options::skip_symlinks))) != 0u) {
        ec = make_error_code(errc::invalid_argument);
        return;
    }
    if ((link_modes & static_cast<unsigned>(copy_options::copy_symlinks)) != 0u &&
        (link_modes & ~(static_cast<unsigned>(copy_options::copy_symlinks))) != 0u) {
        ec = make_error_code(errc::invalid_argument);
        return;
    }
    if ((link_modes & static_cast<unsigned>(copy_options::create_symlinks)) != 0u &&
        (link_modes & static_cast<unsigned>(copy_options::create_hard_links)) != 0u) {
        ec = make_error_code(errc::invalid_argument);
        return;
    }
    if ((bits & static_cast<unsigned>(copy_options::directories_only)) != 0u &&
        (link_modes & (static_cast<unsigned>(copy_options::copy_symlinks) |
                       static_cast<unsigned>(copy_options::create_symlinks))) != 0u) {
        ec = make_error_code(errc::not_supported);
        return;
    }
    if ((bits & static_cast<unsigned>(copy_options::directories_only)) == 0u &&
        (replacement == (static_cast<unsigned>(copy_options::skip_existing) |
                         static_cast<unsigned>(copy_options::overwrite_existing)) ||
         replacement == (static_cast<unsigned>(copy_options::skip_existing) |
                         static_cast<unsigned>(copy_options::update_existing)) ||
         replacement == (static_cast<unsigned>(copy_options::overwrite_existing) |
                         static_cast<unsigned>(copy_options::update_existing)))) {
        ec = make_error_code(errc::invalid_argument);
        return;
    }
    if ((bits & static_cast<unsigned>(copy_options::skip_symlinks)) != 0u) {
        detail::stat_record link_status = {};
        volatile detail::stat_record* link_view = &link_status;
        if (rin_lstat(from.c_str(), &link_status) != 0 ||
            link_view->st_size < 0) {
            ec = link_view->st_size < 0
                ? make_error_code(errc::io_error)
                : make_error_code(detail::filesystem_error_from_errno());
            return;
        }
        if ((link_view->st_mode & 0170000) == 0120000) return;
    }
    if ((bits & static_cast<unsigned>(copy_options::copy_symlinks)) != 0u &&
        (bits & static_cast<unsigned>(copy_options::directories_only)) == 0u) {
        detail::stat_record link_status = {};
        volatile detail::stat_record* link_view = &link_status;
        if (rin_lstat(from.c_str(), &link_status) != 0 ||
            link_view->st_size < 0) {
            ec = link_view->st_size < 0
                ? make_error_code(errc::io_error)
                : make_error_code(detail::filesystem_error_from_errno());
            return;
        }
        if ((link_view->st_mode & 0170000) == 0120000) {
            (void)detail::copy_symlink_node(from, to, replacement, true, ec);
            return;
        }
    }
    detail::stat_record source = {};
    volatile detail::stat_record* source_view = &source;
    if (rin_stat(from.c_str(), &source) != 0 || source_view->st_size < 0) {
        ec = source_view->st_size < 0
            ? make_error_code(errc::io_error)
            : make_error_code(detail::filesystem_error_from_errno());
        return;
    }
    if ((source_view->st_mode & 0170000) == 0100000) {
        if ((bits & static_cast<unsigned>(copy_options::directories_only)) != 0u) {
            ec = make_error_code(errc::not_supported);
            return;
        }
        if ((bits & static_cast<unsigned>(copy_options::create_hard_links)) != 0u) {
            (void)detail::copy_hard_link(from, to, replacement, ec);
            return;
        }
        if ((bits & static_cast<unsigned>(copy_options::create_symlinks)) != 0u) {
            (void)detail::copy_symlink_node(from, to, replacement, false, ec);
            return;
        }
        (void)copy_file(from, to,
                        static_cast<copy_options>(replacement), ec);
        return;
    }
    if ((source_view->st_mode & 0170000) == 0040000 &&
        (bits & static_cast<unsigned>(copy_options::directories_only)) != 0u) {
        if ((bits & static_cast<unsigned>(copy_options::recursive)) != 0u) {
            if (replacement != 0u &&
                replacement != static_cast<unsigned>(copy_options::skip_existing)) {
                ec = make_error_code(errc::not_supported);
                return;
            }
            (void)detail::copy_directory_nodes(from, to, bits, 0u, ec);
            return;
        }
        /* A directory-only copy is intentionally bounded to one directory:
         * it creates the destination node but never traverses children.  This
         * gives callers a useful non-recursive surface without pretending to
         * implement recursive tree publication or link preservation. */
        if (replacement != 0u &&
            replacement != static_cast<unsigned>(copy_options::skip_existing)) {
            ec = make_error_code(errc::not_supported);
            return;
        }
        detail::stat_record destination = {};
        volatile detail::stat_record* destination_view = &destination;
        const int destination_status = rin_stat(to.c_str(), &destination);
        if (destination_status != 0) {
            const errc destination_error =
                detail::filesystem_error_from_errno();
            if (destination_error != errc::no_such_file_or_directory) {
                ec = make_error_code(destination_error);
                return;
            }
        } else if (destination_view->st_size < 0) {
            ec = make_error_code(errc::io_error);
            return;
        }
        const bool destination_exists = destination_status == 0;
        if (destination_exists) {
            if ((destination_view->st_mode & 0170000) != 0040000) {
                /* Keep the directory/non-directory conflict visible to
                 * callers even for the bounded non-recursive form. */
                ec = make_error_code(errc::not_a_directory);
                return;
            }
            /* copy() has no boolean result; skip_existing is still a
             * successful no-op when the destination directory is present. */
            return;
        }
        if (rin_mkdir(to.c_str(), 0755u) != 0) {
            /* A concurrent creator may win the bounded one-directory form
             * too.  Accept only EEXIST followed by one validated directory
             * recheck; all other outcomes remain provider failures. */
            const errc mkdir_error = detail::filesystem_error_from_errno();
            if (mkdir_error != errc::file_exists) {
                ec = make_error_code(mkdir_error);
                return;
            }
            detail::stat_record raced = {};
            volatile detail::stat_record* raced_view = &raced;
            if (rin_stat(to.c_str(), &raced) != 0) {
                ec = make_error_code(detail::filesystem_error_from_errno());
                return;
            }
            if (raced_view->st_size < 0) {
                ec = make_error_code(errc::io_error);
                return;
            }
            if ((raced_view->st_mode & 0170000) != 0040000) {
                ec = make_error_code(errc::not_a_directory);
                return;
            }
        }
        return;
    }
    if ((source_view->st_mode & 0170000) == 0040000 &&
        (bits & static_cast<unsigned>(copy_options::recursive)) != 0u) {
        (void)detail::copy_directory_nodes(from, to, bits, 0u, ec);
        return;
    }
    ec = make_error_code((source_view->st_mode & 0170000) == 0040000
                             ? errc::not_supported
                             : errc::io_error);
}

inline void copy(const path& from, const path& to, error_code& ec) noexcept {
    copy(from, to, copy_options::none, ec);
}

inline void copy(const path& from, const path& to,
                 copy_options options = copy_options::none) {
    error_code ec;
    copy(from, to, options, ec);
    if (ec) detail::filesystem_failure("copy", from, to, ec);
}

/* rename */
inline void rename(const path& old_p, const path& new_p,
                   error_code& ec) noexcept {
    ec.clear();
    if (rin_rename(old_p.c_str(), new_p.c_str()) != 0) {
        ec = make_error_code(detail::filesystem_error_from_errno());
    }
}

inline void rename(const path& old_p, const path& new_p) {
    error_code ec;
    rename(old_p, new_p, ec);
    if (ec) detail::filesystem_failure("rename", old_p, new_p, ec);
}

/* current_path */
inline path current_path(error_code& ec) {
    ec.clear();
    char buf[4096];
    if (rin_getcwd(buf, sizeof(buf))) {
        return path(buf);
    }
    ec = make_error_code(detail::filesystem_error_from_errno());
    return path();
}

inline path current_path() {
    error_code ec;
    const path result = current_path(ec);
    if (ec) detail::filesystem_failure("current_path", ec);
    return result;
}

inline void current_path(const path& p, error_code& ec) noexcept {
    ec.clear();
    if (rin_chdir(p.c_str()) != 0) {
        ec = make_error_code(detail::filesystem_error_from_errno());
    }
}

inline void current_path(const path& p) {
    error_code ec;
    current_path(p, ec);
    if (ec) detail::filesystem_failure("current_path", p, ec);
}

/* status */
inline file_status status(const path& p, error_code& ec) noexcept {
    ec.clear();
    directory_entry de(p, ec);
    return de.status();
}

inline file_status status(const path& p) {
    error_code ec;
    const file_status result = status(p, ec);
    if (ec) detail::filesystem_failure("status", p, ec);
    return result;
}

/* absolute */
inline path absolute(const path& p) {
    if (p.is_absolute()) return p;
    return current_path() / p;
}

/* hash_value for path (Abseil compatibility) */
inline size_t hash_value(const path& p) noexcept {
    size_t hash = 0;
    const auto& s = p.native();
    for (size_t i = 0; i < s.size(); ++i) {
        hash = hash * 31 + static_cast<size_t>(
            static_cast<unsigned char>(s[i]));
    }
    return hash;
}

} /* namespace filesystem */

template<>
struct hash<filesystem::path> {
    size_t operator()(const filesystem::path& value) const noexcept {
        return filesystem::hash_value(value);
    }
};

/* 名前空間エイリアス */
namespace fs = filesystem;

} /* namespace std */

#endif /* defined(__cplusplus) && __cplusplus >= 201703L */
#endif /* RINCXX_FILESYSTEM_H */
