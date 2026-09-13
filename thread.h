/*
 * RinOS C++ <thread> ✿
 * スレッドサポート
 */

#ifndef RINCXX_THREAD_H
#define RINCXX_THREAD_H

#include "rincxx.h"
#include "version.h"
#if __cplusplus >= 202002L
#include "compare.h"
#endif
#include "functional.h"
#include "memory.h"
#include "chrono.h"
#include "limits.h"
#include "tuple.h"
#include "stop_token.h"
#include "system_error.h"

/* Target builds retain the Rin ABI; hosted C++ consumers use the compiler
 * pthread ABI through the common ownership boundary. */
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <errno.h>
#include_next <unistd.h>
#else
#include "../libc/errno.h"
#include "../libc/sys/syscall.h"
#endif
#include "__pthread.h"

namespace std {

namespace detail {

[[noreturn]] inline void thread_operation_failed(int error,
                                                 const char* operation) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    /* POSIX pthread APIs return errno values directly.  The standard thread
     * operations expose those failures through system_category(), rather
     * than recategorizing a platform result as a generic condition. */
    throw system_error(error, system_category(), operation);
#else
    (void)error;
    (void)operation;
    terminate();
#endif
}

[[noreturn]] inline void thread_allocation_failed() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_alloc();
#else
    terminate();
#endif
}

template<typename Callable, typename... Args>
class thread_invoker {
    Callable callable_;
    tuple<Args...> arguments_;

    template<size_t... Index>
    static void invoke(Callable&& callable, tuple<Args...>&& arguments,
                       index_sequence<Index...>) {
        std::invoke(std::move(callable),
                    std::get<Index>(std::move(arguments))...);
    }

public:
    template<typename Function, typename... Arguments>
    thread_invoker(Function&& function, Arguments&&... arguments)
        : callable_(std::forward<Function>(function)),
          arguments_(std::forward<Arguments>(arguments)...) {}

    void operator()() {
        invoke(std::move(callable_), std::move(arguments_),
               index_sequence_for<Args...>{});
    }
};

/* `pthread_t` is deliberately opaque in POSIX.  Some hosted libcs expose it
 * as an integer, while others use an object pointer.  The C++ thread::id
 * relational operators still need a strict total order, so never apply the
 * built-in pointer `<` directly. */
template<typename T>
inline bool thread_id_less_dispatch(T left, T right,
                                    integral_constant<int, 1>) noexcept {
    return object_pointer_total_less(left, right);
}

template<typename T>
inline bool thread_id_less_dispatch(T left, T right,
                                    integral_constant<int, 2>) noexcept {
    return function_pointer_total_less(left, right);
}

template<typename T>
inline bool thread_id_less_dispatch(T left, T right,
                                    integral_constant<int, 0>) noexcept {
    return left < right;
}

template<typename T>
inline bool thread_id_less(T left, T right) noexcept {
    using plain_type = typename remove_cv<T>::type;
    constexpr int pointer_kind =
        is_object_pointer<plain_type>::value
            ? 1
            : (is_function_pointer<plain_type>::value ? 2 : 0);
    return thread_id_less_dispatch(left, right,
                                   integral_constant<int, pointer_kind>{});
}

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * thread クラス
 * ═══════════════════════════════════════════════════════════════*/

class thread {
public:
    class id {
        pthread_t id_;

        friend class thread;
        friend struct hash<id>;
        friend bool operator==(id x, id y) noexcept {
            return ::pthread_equal(x.id_, y.id_) != 0;
        }
        friend bool operator!=(id x, id y) noexcept { return !(x == y); }
        friend bool operator<(id x, id y) noexcept {
            return detail::thread_id_less(x.id_, y.id_);
        }
        friend bool operator<=(id x, id y) noexcept {
            return !detail::thread_id_less(y.id_, x.id_);
        }
        friend bool operator>(id x, id y) noexcept {
            return detail::thread_id_less(y.id_, x.id_);
        }
        friend bool operator>=(id x, id y) noexcept {
            return !detail::thread_id_less(x.id_, y.id_);
        }
#if __cplusplus >= 202002L
        friend strong_ordering operator<=>(id x, id y) noexcept {
            if (x == y) return strong_ordering::equal;
            return detail::thread_id_less(x.id_, y.id_)
                ? strong_ordering::less : strong_ordering::greater;
        }
#endif

    public:
        id() noexcept : id_(0) {}
        explicit id(pthread_t tid) noexcept : id_(tid) {}
    };

    using native_handle_type = pthread_t;

private:
    pthread_t thread_id_;

    /* スレッド関数呼び出し用ラッパー */
    template<typename Callable>
    struct thread_data {
        Callable func;

        thread_data(Callable&& f) : func(std::move(f)) {}

        static void destroy(thread_data* data) noexcept {
            if (!data) return;
            data->~thread_data();
            ::rin_free(data);
        }

        static void* thread_func(void* arg) {
            thread_data* data = static_cast<thread_data*>(arg);
            struct cleanup_guard {
                thread_data* value;
                ~cleanup_guard() { thread_data::destroy(value); }
            } guard{data};
            data->func();
            return nullptr;
        }
    };

public:
    /* コンストラクタ */
    thread() noexcept : thread_id_(0) {}

    template<typename Callable, typename... Args,
             enable_if_t<
                 !is_same<remove_cvref_t<Callable>, thread>::value &&
                 is_invocable<decay_t<Callable>, decay_t<Args>...>::value,
                 int> = 0>
    explicit thread(Callable&& f, Args&&... args) : thread_id_(0) {
        /* C++11でもdecay-copyした関数と引数を新しいthread上でrvalueとして呼ぶ */
        using bound_type = detail::thread_invoker<
            decay_t<Callable>, decay_t<Args>...>;
        bound_type bound(std::forward<Callable>(f),
                         std::forward<Args>(args)...);
        using Data = thread_data<bound_type>;
        void* storage = ::rin_malloc(sizeof(Data));
        if (!storage) detail::thread_allocation_failed();
        Data* data = nullptr;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            data = ::new (storage) Data(std::move(bound));
        } catch (...) {
            ::rin_free(storage);
            throw;
        }
#else
        data = ::new (storage) Data(std::move(bound));
#endif

        int result = pthread_create(&thread_id_, nullptr, Data::thread_func, data);
        if (result != 0) {
            Data::destroy(data);
            thread_id_ = 0;
            detail::thread_operation_failed(result, "thread construction");
        }
    }

    /* コピー禁止 */
    thread(const thread&) = delete;
    thread& operator=(const thread&) = delete;

    /* ムーブ */
    thread(thread&& other) noexcept : thread_id_(other.thread_id_) {
        other.thread_id_ = 0;
    }

    thread& operator=(thread&& other) noexcept {
        if (this != &other) {
            if (joinable()) {
                terminate();
            }
            thread_id_ = other.thread_id_;
            other.thread_id_ = 0;
        }
        return *this;
    }

    /* デストラクタ */
    ~thread() {
        if (joinable()) {
            terminate();
        }
    }

    /* joinable */
    bool joinable() const noexcept {
        return thread_id_ != 0;
    }

    /* join */
    void join() {
        if (!joinable())
            detail::thread_operation_failed(EINVAL, "thread join");
        const int result = pthread_join(thread_id_, nullptr);
        if (result != 0)
            detail::thread_operation_failed(result, "thread join");
        thread_id_ = 0;
    }

    /* detach */
    void detach() {
        if (!joinable())
            detail::thread_operation_failed(EINVAL, "thread detach");
        const int result = pthread_detach(thread_id_);
        if (result != 0)
            detail::thread_operation_failed(result, "thread detach");
        thread_id_ = 0;
    }

    /* ID取得 */
    id get_id() const noexcept {
        return id(thread_id_);
    }

    /* native handle */
    native_handle_type native_handle() noexcept {
        return thread_id_;
    }

    /* swap */
    void swap(thread& other) noexcept {
        std::swap(thread_id_, other.thread_id_);
    }

    /* ハードウェア並行性 */
    static unsigned int hardware_concurrency() noexcept {
#if defined(RIN_FREESTANDING) || defined(RIN_USERSPACE)
        const intptr_t count = __rin_syscall_posixize(
            _syscall0((uintptr_t)SYS_GETCPU_COUNT));
        if (count <= 0 ||
            static_cast<uintptr_t>(count) >
                static_cast<uintptr_t>(UINT_MAX)) {
            return 0;
        }
        return static_cast<unsigned int>(count);
#else
#if defined(_SC_NPROCESSORS_ONLN)
        const long count = sysconf(_SC_NPROCESSORS_ONLN);
        if (count <= 0 ||
            static_cast<unsigned long>(count) >
                static_cast<unsigned long>(UINT_MAX)) {
            return 0;
        }
        return static_cast<unsigned int>(count);
#else
        return 0;
#endif
#endif
    }
};

inline void swap(thread& x, thread& y) noexcept {
    x.swap(y);
}

template<>
struct hash<thread::id> {
    size_t operator()(const thread::id& value) const noexcept {
        return hash<pthread_t>{}(value.id_);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * jthread クラス (C++20)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 202002L

namespace detail {

template<typename F, typename Tuple, size_t... Index>
void invoke_with_stop_token(F&& function, stop_token token, Tuple&& arguments,
                            index_sequence<Index...>) {
    std::invoke(std::forward<F>(function), std::move(token),
                std::get<Index>(std::forward<Tuple>(arguments))...);
}

template<typename F, typename... Args>
auto make_jthread_invoker(stop_token token, F&& function, Args&&... args) {
    using function_type = decay_t<F>;
    using argument_tuple = tuple<decay_t<Args>...>;

    if constexpr (is_invocable<function_type, stop_token,
                               decay_t<Args>...>::value) {
        return [token = std::move(token),
                function = function_type(std::forward<F>(function)),
                arguments = argument_tuple(std::forward<Args>(args)...)]()
                   mutable {
            invoke_with_stop_token(
                std::move(function), std::move(token), std::move(arguments),
                index_sequence_for<Args...>{});
        };
    } else {
        static_assert(is_invocable<function_type, decay_t<Args>...>::value,
                      "jthread function must be invocable");
        return [function = function_type(std::forward<F>(function)),
                arguments = argument_tuple(std::forward<Args>(args)...)]()
                   mutable {
            std::apply(std::move(function), std::move(arguments));
        };
    }
}

} /* namespace detail */

class jthread {
public:
    using id = thread::id;
    using native_handle_type = thread::native_handle_type;

private:
    stop_source source_;
    thread thread_;

public:
    jthread() noexcept : source_(nostopstate), thread_() {}

    template<typename F, typename... Args,
             enable_if_t<
                 !is_same<remove_cvref_t<F>, jthread>::value &&
                 (is_invocable<decay_t<F>, stop_token,
                               decay_t<Args>...>::value ||
                  is_invocable<decay_t<F>, decay_t<Args>...>::value),
                 int> = 0>
    explicit jthread(F&& function, Args&&... args)
        : source_(),
          thread_(detail::make_jthread_invoker(
              source_.get_token(), std::forward<F>(function),
              std::forward<Args>(args)...)) {}

    ~jthread() {
        if (joinable()) {
            request_stop();
            join();
        }
    }

    jthread(const jthread&) = delete;
    jthread& operator=(const jthread&) = delete;

    jthread(jthread&& other) noexcept
        : source_(std::move(other.source_)),
          thread_(std::move(other.thread_)) {}

    jthread& operator=(jthread&& other) noexcept {
        if (this != &other) {
            if (joinable()) {
                request_stop();
                join();
            }
            source_ = std::move(other.source_);
            thread_ = std::move(other.thread_);
        }
        return *this;
    }

    void swap(jthread& other) noexcept {
        source_.swap(other.source_);
        thread_.swap(other.thread_);
    }

    [[nodiscard]] bool joinable() const noexcept {
        return thread_.joinable();
    }

    void join() {
        thread_.join();
    }

    void detach() {
        thread_.detach();
    }

    [[nodiscard]] id get_id() const noexcept {
        return thread_.get_id();
    }

    native_handle_type native_handle() noexcept {
        return thread_.native_handle();
    }

    static unsigned int hardware_concurrency() noexcept {
        return thread::hardware_concurrency();
    }

    [[nodiscard]] stop_source get_stop_source() noexcept {
        return source_;
    }

    [[nodiscard]] stop_token get_stop_token() const noexcept {
        return source_.get_token();
    }

    bool request_stop() noexcept {
        return source_.request_stop();
    }
};

inline void swap(jthread& left, jthread& right) noexcept {
    left.swap(right);
}

#endif /* C++20 */

/* ═══════════════════════════════════════════════════════════════
 * this_thread 名前空間
 * ═══════════════════════════════════════════════════════════════*/

namespace this_thread {

namespace detail {

inline bool sleep_once(timespec requested) noexcept {
    for (;;) {
        timespec remaining{};
        if (::nanosleep(&requested, &remaining) == 0)
            return true;
        if (errno != EINTR)
            return false;
        requested = remaining;
    }
}

} /* namespace detail */

inline thread::id get_id() noexcept {
    return thread::id(pthread_self());
}

inline void yield() noexcept {
    ::std::detail::rin_cxx_scheduler_yield();
}

template<typename Rep, typename Period>
void sleep_for(const chrono::duration<Rep, Period>& sleep_duration) {
    const long double seconds =
        static_cast<long double>(sleep_duration.count()) *
        static_cast<long double>(Period::num) /
        static_cast<long double>(Period::den);
    if (!(seconds > 0.0L)) {
        return;
    }

    constexpr long double nanoseconds_per_second = 1000000000.0L;
    const long double maximum_seconds =
        static_cast<long double>(numeric_limits<time_t>::max());
    long double remaining_seconds = seconds;

    while (remaining_seconds > 0.0L) {
        timespec request{};
        long double represented_seconds;
        if (remaining_seconds >= maximum_seconds) {
            request.tv_sec = numeric_limits<time_t>::max();
            request.tv_nsec = 0;
            represented_seconds = maximum_seconds;
        } else {
            request.tv_sec = static_cast<time_t>(remaining_seconds);
            const long double fractional_nanoseconds =
                (remaining_seconds -
                 static_cast<long double>(request.tv_sec)) *
                nanoseconds_per_second;
            request.tv_nsec = static_cast<long>(fractional_nanoseconds);
            if (static_cast<long double>(request.tv_nsec) <
                fractional_nanoseconds) {
                ++request.tv_nsec;
            }
            if (request.tv_nsec >= 1000000000L) {
                ++request.tv_sec;
                request.tv_nsec = 0;
            }
            if (request.tv_sec == 0 && request.tv_nsec == 0)
                request.tv_nsec = 1;
            represented_seconds = remaining_seconds;
        }

        if (!detail::sleep_once(request))
            return;

        /* A floating-point duration may carry positive infinity.  Subtracting
         * a finite time_t-sized chunk from it leaves infinity unchanged, so
         * the old loop would repeatedly issue the same request forever.  A
         * finite duration can also lose progress once its magnitude exceeds
         * the precision of long double.  The request above is the largest
         * representable POSIX chunk in both cases; stop when the accumulator
         * cannot make progress instead of spinning in the caller forever. */
        const long double next_seconds =
            remaining_seconds - represented_seconds;
        if (!(next_seconds < remaining_seconds))
            return;
        remaining_seconds = next_seconds;
    }
}

template<typename Clock, typename Duration>
void sleep_until(const chrono::time_point<Clock, Duration>& sleep_time) {
    auto now = Clock::now();
    if (sleep_time > now) {
        sleep_for(sleep_time - now);
    }
}

} /* namespace this_thread */

} /* namespace std */

#endif /* RINCXX_THREAD_H */
