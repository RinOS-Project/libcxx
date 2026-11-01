/*
 * RinOS C++ <future>
 * Shared asynchronous results, promises, packaged tasks, and async launch.
 */

#ifndef RINCXX_FUTURE_H
#define RINCXX_FUTURE_H

#include "rincxx.h"
#include "chrono.h"
#include "condition_variable.h"
#include "exception.h"
#include "functional.h"
#include "memory.h"
#include "new.h"
#include "system_error.h"
#include "thread.h"
#include "tuple.h"
#include "type_traits.h"
#include "utility.h"

namespace std {

enum class future_errc {
    broken_promise = 1,
    future_already_retrieved = 2,
    promise_already_satisfied = 3,
    no_state = 4
};

class future_error_category final : public error_category {
public:
    const char* name() const noexcept override { return "future"; }

    unsigned int identity_key() const noexcept override { return 4u; }

    unsigned int identity_domain_key() const noexcept override {
        return 0x46555452u;
    }

    string message(int value) const override {
        switch (static_cast<future_errc>(value)) {
        case future_errc::broken_promise:
            return string("broken promise");
        case future_errc::future_already_retrieved:
            return string("future already retrieved");
        case future_errc::promise_already_satisfied:
            return string("promise already satisfied");
        case future_errc::no_state:
            return string("future has no shared state");
        }
        return string("unknown future error");
    }
};

inline const error_category& future_category() noexcept {
    static future_error_category category;
    return category;
}

inline error_code make_error_code(future_errc error) noexcept {
    return error_code(static_cast<int>(error), future_category());
}

inline error_condition make_error_condition(future_errc error) noexcept {
    return error_condition(static_cast<int>(error), future_category());
}

template<>
struct is_error_code_enum<future_errc> : true_type {};

template<>
struct is_error_condition_enum<future_errc> : true_type {};

class future_error : public logic_error {
    error_code code_;

public:
    explicit future_error(error_code code)
        : logic_error(code.message()), code_(code) {}

    explicit future_error(future_errc error)
        : future_error(make_error_code(error)) {}

    const error_code& code() const noexcept { return code_; }
};

enum class future_status { ready, timeout, deferred };

enum class launch : unsigned int {
    async = 1u,
    deferred = 2u
};

inline launch operator|(launch left, launch right) noexcept {
    return static_cast<launch>(static_cast<unsigned int>(left) |
                               static_cast<unsigned int>(right));
}

inline launch operator&(launch left, launch right) noexcept {
    return static_cast<launch>(static_cast<unsigned int>(left) &
                               static_cast<unsigned int>(right));
}

inline launch operator^(launch left, launch right) noexcept {
    return static_cast<launch>(static_cast<unsigned int>(left) ^
                               static_cast<unsigned int>(right));
}

inline launch operator~(launch value) noexcept {
    return static_cast<launch>(~static_cast<unsigned int>(value));
}

inline launch& operator|=(launch& left, launch right) noexcept {
    left = left | right;
    return left;
}

inline launch& operator&=(launch& left, launch right) noexcept {
    left = left & right;
    return left;
}

inline launch& operator^=(launch& left, launch right) noexcept {
    left = left ^ right;
    return left;
}

namespace __future_detail {

[[noreturn]] inline void raise(future_errc error) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw future_error(error);
#else
    (void)error;
    terminate();
#endif
}

template<typename T>
class state;

template<typename T>
class work {
public:
    virtual ~work() {}
    virtual void run(state<T>& shared) = 0;
};

template<typename T, bool IsVoid = is_void<T>::value,
         bool IsReference = is_reference<T>::value>
struct shared_result {
    using type = const T&;
};

template<typename T>
struct shared_result<T, false, true> { using type = T; };

template<typename T>
struct shared_result<T, true, false> { using type = void; };

template<typename T, bool IsReference = is_reference<T>::value,
         bool IsVoid = is_void<T>::value>
class value_storage {
    aligned_storage_t<sizeof(T), alignof(T)> bytes_;
    bool engaged_;

    T* pointer() noexcept {
        return reinterpret_cast<T*>(&bytes_);
    }

    const T* pointer() const noexcept {
        return reinterpret_cast<const T*>(&bytes_);
    }

public:
    value_storage() noexcept : engaged_(false) {}
    value_storage(const value_storage&) = delete;
    value_storage& operator=(const value_storage&) = delete;

    ~value_storage() { reset(); }

    template<typename U>
    void construct(U&& value) {
        new (&bytes_) T(std::forward<U>(value));
        engaged_ = true;
    }

    void reset() noexcept {
        if (engaged_) {
            pointer()->~T();
            engaged_ = false;
        }
    }

    T take() {
        return std::move(*pointer());
    }

    const T& share() const noexcept { return *pointer(); }
};

template<typename T>
class value_storage<T, true, false> {
    using referred_type = typename remove_reference<T>::type;
    referred_type* pointer_;

public:
    value_storage() noexcept : pointer_(nullptr) {}
    template<typename U> void construct(U&& value) noexcept {
        pointer_ = __builtin_addressof(value);
    }
    void reset() noexcept { pointer_ = nullptr; }
    T take() const noexcept { return *pointer_; }
    T share() const noexcept { return *pointer_; }
};

template<>
class value_storage<void, false, true> {
public:
    void construct() noexcept {}
    void reset() noexcept {}
    void take() const noexcept {}
    void share() const noexcept {}
};

template<typename T>
class state {
    mutable mutex mutex_;
    condition_variable condition_;
    value_storage<T> value_;
    exception_ptr exception_;
    future_errc error_;
    bool ready_;
    bool deferred_;
    bool deferred_started_;
    bool async_release_wait_;
    unsigned int observers_;
    thread::id async_thread_;
    thread async_thread_handle_;
    bool async_join_in_progress_;
    bool async_joined_;
    shared_ptr<work<T>> deferred_work_;

    void notify_ready(unique_lock<mutex>& lock) {
        lock.unlock();
        condition_.notify_all();
    }

    void throw_if_unavailable() const {
        if (error_ != static_cast<future_errc>(0))
            raise(error_);
        if (exception_) {
            exception_ptr exception = exception_;
            rethrow_exception(std::move(exception));
        }
    }

    void check_unsatisfied() const {
        if (ready_) raise(future_errc::promise_already_satisfied);
    }

    void run_deferred() {
        shared_ptr<work<T>> invocation;
        {
            unique_lock<mutex> lock(mutex_);
            if (deferred_ && !deferred_started_ && !ready_) {
                deferred_started_ = true;
                invocation = std::move(deferred_work_);
            }
        }
        if (invocation) invocation->run(*this);
    }

    void join_async_thread() {
        thread joiner;
        {
            unique_lock<mutex> lock(mutex_);
            if (!async_release_wait_ || async_thread_ == this_thread::get_id() || async_joined_)
                return;
            while (async_join_in_progress_) {
                condition_.wait(lock, [this] { return async_joined_ || !async_join_in_progress_; });
                if (async_joined_)
                    return;
            }
            if (!async_thread_handle_.joinable())
                return;
            async_join_in_progress_ = true;
            joiner = std::move(async_thread_handle_);
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            joiner.join();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            unique_lock<mutex> lock(mutex_);
            async_thread_handle_ = std::move(joiner);
            async_join_in_progress_ = false;
            lock.unlock();
            condition_.notify_all();
            throw;
        }
#endif
        {
            unique_lock<mutex> lock(mutex_);
            async_joined_ = true;
            async_join_in_progress_ = false;
        }
        condition_.notify_all();
    }

public:
    state()
        : error_(static_cast<future_errc>(0)), ready_(false),
          deferred_(false), deferred_started_(false),
          async_release_wait_(false), observers_(0), async_thread_(),
          async_join_in_progress_(false), async_joined_(false) {}

    state(const state&) = delete;
    state& operator=(const state&) = delete;

    ~state() {
        if (!async_thread_handle_.joinable())
            return;
        if (async_thread_ == this_thread::get_id())
            async_thread_handle_.detach();
        else
            async_thread_handle_.join();
    }

    template<typename U>
    void set_value(U&& input) {
        unique_lock<mutex> lock(mutex_);
        check_unsatisfied();
        value_.construct(std::forward<U>(input));
        error_ = static_cast<future_errc>(0);
        exception_ = exception_ptr();
        ready_ = true;
        notify_ready(lock);
    }

    void set_value() {
        unique_lock<mutex> lock(mutex_);
        check_unsatisfied();
        value_.construct();
        error_ = static_cast<future_errc>(0);
        exception_ = exception_ptr();
        ready_ = true;
        notify_ready(lock);
    }

    template<typename U>
    void set_value_at_thread_exit(U&& input,
                                  shared_ptr<state<T>> lifetime) {
        unique_lock<mutex> lock(mutex_);
        check_unsatisfied();
        value_.construct(std::forward<U>(input));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            std::notify_all_at_thread_exit(
                condition_, std::move(lock), std::move(lifetime));
            /* notify_all_at_thread_exit transfers the still-locked native
             * mutex to its TLS exit node.  Publish readiness before this
             * thread can run that node. */
            error_ = static_cast<future_errc>(0);
            exception_ = exception_ptr();
            ready_ = true;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            unique_lock<mutex> rollback(mutex_);
            value_.reset();
            throw;
        }
#endif
    }

    void set_value_at_thread_exit(shared_ptr<state<T>> lifetime) {
        unique_lock<mutex> lock(mutex_);
        check_unsatisfied();
        value_.construct();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            std::notify_all_at_thread_exit(
                condition_, std::move(lock), std::move(lifetime));
            error_ = static_cast<future_errc>(0);
            exception_ = exception_ptr();
            ready_ = true;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            unique_lock<mutex> rollback(mutex_);
            value_.reset();
            throw;
        }
#endif
    }

    void set_exception(exception_ptr exception) {
        if (!exception) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            throw invalid_argument("promise exception must not be empty");
#else
            terminate();
#endif
        }
        unique_lock<mutex> lock(mutex_);
        check_unsatisfied();
        exception_ = std::move(exception);
        error_ = static_cast<future_errc>(0);
        ready_ = true;
        notify_ready(lock);
    }

    void set_exception_at_thread_exit(exception_ptr exception,
                                      shared_ptr<state<T>> lifetime) {
        if (!exception) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            throw invalid_argument("promise exception must not be empty");
#else
            terminate();
#endif
        }
        unique_lock<mutex> lock(mutex_);
        check_unsatisfied();
        exception_ = std::move(exception);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            std::notify_all_at_thread_exit(
                condition_, std::move(lock), std::move(lifetime));
            error_ = static_cast<future_errc>(0);
            ready_ = true;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            unique_lock<mutex> rollback(mutex_);
            exception_ = exception_ptr();
            throw;
        }
#endif
    }

    void abandon_producer() noexcept {
        unique_lock<mutex> lock(mutex_);
        if (!ready_) {
            error_ = future_errc::broken_promise;
            ready_ = true;
            notify_ready(lock);
        }
    }

    void set_deferred(shared_ptr<work<T>> invocation) {
        unique_lock<mutex> lock(mutex_);
        deferred_work_ = std::move(invocation);
        deferred_ = true;
        async_release_wait_ = false;
    }

    void mark_async_launch() noexcept {
        unique_lock<mutex> lock(mutex_);
        async_release_wait_ = true;
    }

    void mark_async_thread(thread::id id) noexcept {
        unique_lock<mutex> lock(mutex_);
        async_thread_ = id;
    }

    void attach_async_thread(thread&& worker) noexcept {
        async_thread_handle_ = std::move(worker);
    }

    void acquire_observer() noexcept {
        if (__atomic_add_fetch(&observers_, 1u, __ATOMIC_RELAXED) == 0u)
            terminate();
    }

    void release_observer() noexcept {
        const unsigned int previous =
            __atomic_fetch_sub(&observers_, 1u, __ATOMIC_ACQ_REL);
        if (previous == 0u) terminate();
        if (previous != 1u) return;

        bool should_wait = false;
        {
            unique_lock<mutex> lock(mutex_);
            should_wait = async_release_wait_ && async_thread_ != this_thread::get_id();
        }
        if (should_wait) wait();
    }

    void wait() {
        run_deferred();
        {
            unique_lock<mutex> lock(mutex_);
            condition_.wait(lock, [this] { return ready_; });
        }
        join_async_thread();
    }

    template<typename Rep, typename Period>
    future_status wait_for(const chrono::duration<Rep, Period>& duration) {
        future_status result;
        {
            unique_lock<mutex> lock(mutex_);
            if (!ready_ && deferred_ && !deferred_started_)
                return future_status::deferred;
            result = condition_.wait_for(lock, duration, [this] { return ready_; })
                         ? future_status::ready
                         : future_status::timeout;
        }
        if (result == future_status::ready)
            join_async_thread();
        return result;
    }

    template<typename Clock, typename Duration>
    future_status wait_until(
        const chrono::time_point<Clock, Duration>& deadline) {
        future_status result;
        {
            unique_lock<mutex> lock(mutex_);
            if (!ready_ && deferred_ && !deferred_started_)
                return future_status::deferred;
            result = condition_.wait_until(lock, deadline, [this] { return ready_; })
                         ? future_status::ready
                         : future_status::timeout;
        }
        if (result == future_status::ready)
            join_async_thread();
        return result;
    }

    typename conditional<is_void<T>::value, void, T>::type take() {
        wait();
        unique_lock<mutex> lock(mutex_);
        throw_if_unavailable();
        return value_.take();
    }

    typename shared_result<T>::type share() {
        wait();
        unique_lock<mutex> lock(mutex_);
        throw_if_unavailable();
        return value_.share();
    }
};

template<typename T, typename F, typename Tuple>
class invocation_work;

template<typename T, typename F, typename... Args>
struct invoke_store {
    static void run(state<T>& shared, shared_ptr<state<T>> lifetime,
                    bool at_exit, F&& callable,
                    Args&&... args) {
        T result = std::invoke(std::forward<F>(callable),
                               std::forward<Args>(args)...);
        if (at_exit)
            shared.set_value_at_thread_exit(
                std::forward<T>(result), std::move(lifetime));
        else
            shared.set_value(std::forward<T>(result));
    }
};

template<typename F, typename... Args>
struct invoke_store<void, F, Args...> {
    static void run(state<void>& shared, shared_ptr<state<void>> lifetime,
                    bool at_exit, F&& callable,
                    Args&&... args) {
        std::invoke(std::forward<F>(callable),
                    std::forward<Args>(args)...);
        if (at_exit) shared.set_value_at_thread_exit(std::move(lifetime));
        else shared.set_value();
    }
};

template<typename T, typename F, typename... Args>
class invocation_work<T, F, tuple<Args...>> : public work<T> {
    F callable_;
    tuple<Args...> arguments_;

    template<size_t... Index>
    void invoke(state<T>& shared, index_sequence<Index...>) {
        invoke_store<T, F&&,
                     decltype(std::get<Index>(std::move(arguments_)))...>::run(
            shared, shared_ptr<state<T>>(), false, std::move(callable_),
            std::get<Index>(std::move(arguments_))...);
    }

public:
    template<typename Function, typename... Values>
    invocation_work(Function&& callable, Values&&... values)
        : callable_(std::forward<Function>(callable)),
          arguments_(std::forward<Values>(values)...) {}

    void run(state<T>& shared) override {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            invoke(shared, index_sequence_for<Args...>{});
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            try {
                shared.set_exception(current_exception());
            } catch (...) {
                terminate();
            }
        }
#endif
    }
};

template<typename R, typename... Args>
struct packaged_callable {
    virtual ~packaged_callable() {}
    virtual R invoke(Args... args) = 0;

    R operator()(Args... args) {
        return invoke(std::forward<Args>(args)...);
    }
};

template<typename R, typename F, typename... Args>
struct packaged_callable_impl : packaged_callable<R, Args...> {
    F callable_;

    template<typename Function>
    explicit packaged_callable_impl(Function&& callable)
        : callable_(std::forward<Function>(callable)) {}

    R invoke(Args... args) override {
        return std::invoke(callable_,
                           std::forward<Args>(args)...);
    }
};

#if __cplusplus >= 201703L
template<typename Signature>
struct task_signature;

template<typename R, typename... Args>
struct task_signature<R (*)(Args...)> {
    using type = R(Args...);
};

template<typename R, typename... Args>
struct task_signature<R (*)(Args...) noexcept> {
    using type = R(Args...);
};

#define RIN_FUTURE_TASK_SIGNATURE(CV, REF, NOEXCEPT_SPEC) \
    template<typename R, typename C, typename... Args> \
    struct task_signature<R (C::*)(Args...) CV REF NOEXCEPT_SPEC> { \
        using type = R(Args...); \
    }

RIN_FUTURE_TASK_SIGNATURE(, , );
RIN_FUTURE_TASK_SIGNATURE(const, , );
RIN_FUTURE_TASK_SIGNATURE(volatile, , );
RIN_FUTURE_TASK_SIGNATURE(const volatile, , );
RIN_FUTURE_TASK_SIGNATURE(, &, );
RIN_FUTURE_TASK_SIGNATURE(const, &, );
RIN_FUTURE_TASK_SIGNATURE(volatile, &, );
RIN_FUTURE_TASK_SIGNATURE(const volatile, &, );
RIN_FUTURE_TASK_SIGNATURE(, &&, );
RIN_FUTURE_TASK_SIGNATURE(const, &&, );
RIN_FUTURE_TASK_SIGNATURE(volatile, &&, );
RIN_FUTURE_TASK_SIGNATURE(const volatile, &&, );
RIN_FUTURE_TASK_SIGNATURE(, , noexcept);
RIN_FUTURE_TASK_SIGNATURE(const, , noexcept);
RIN_FUTURE_TASK_SIGNATURE(volatile, , noexcept);
RIN_FUTURE_TASK_SIGNATURE(const volatile, , noexcept);
RIN_FUTURE_TASK_SIGNATURE(, &, noexcept);
RIN_FUTURE_TASK_SIGNATURE(const, &, noexcept);
RIN_FUTURE_TASK_SIGNATURE(volatile, &, noexcept);
RIN_FUTURE_TASK_SIGNATURE(const volatile, &, noexcept);
RIN_FUTURE_TASK_SIGNATURE(, &&, noexcept);
RIN_FUTURE_TASK_SIGNATURE(const, &&, noexcept);
RIN_FUTURE_TASK_SIGNATURE(volatile, &&, noexcept);
RIN_FUTURE_TASK_SIGNATURE(const volatile, &&, noexcept);

#undef RIN_FUTURE_TASK_SIGNATURE
#endif

} /* namespace __future_detail */

template<typename T> class future;
template<typename T> class shared_future;
template<typename T> class promise;
template<typename Signature> class packaged_task;

template<typename T>
class future {
    shared_ptr<__future_detail::state<T>> state_;

    void release() noexcept {
        if (state_) {
            state_->release_observer();
            state_.reset();
        }
    }

    template<typename> friend class promise;
    template<typename> friend class shared_future;
    template<typename> friend class packaged_task;
public:
    using result_type = T;

    future() noexcept = default;
    /* Internal shared-state constructor used by the header-only dispatcher. */
    explicit future(shared_ptr<__future_detail::state<T>> state)
        : state_(std::move(state)) {
        if (state_) state_->acquire_observer();
    }
    future(const future&) = delete;
    future& operator=(const future&) = delete;

    future(future&& other) noexcept : state_(std::move(other.state_)) {}

    future& operator=(future&& other) noexcept {
        if (this != &other) {
            release();
            state_ = std::move(other.state_);
        }
        return *this;
    }

    ~future() { release(); }

    bool valid() const noexcept { return static_cast<bool>(state_); }

    void wait() const {
        if (!state_) __future_detail::raise(future_errc::no_state);
        state_->wait();
    }

    template<typename Rep, typename Period>
    future_status wait_for(
        const chrono::duration<Rep, Period>& duration) const {
        if (!state_) __future_detail::raise(future_errc::no_state);
        return state_->wait_for(duration);
    }

    template<typename Clock, typename Duration>
    future_status wait_until(
        const chrono::time_point<Clock, Duration>& deadline) const {
        if (!state_) __future_detail::raise(future_errc::no_state);
        return state_->wait_until(deadline);
    }

    typename conditional<is_void<T>::value, void, T>::type get() {
        if (!state_) __future_detail::raise(future_errc::no_state);
        shared_ptr<__future_detail::state<T>> state = std::move(state_);
        state->release_observer();
        return state->take();
    }

    shared_future<T> share() noexcept;

    void swap(future& other) noexcept { state_.swap(other.state_); }
};

template<typename T>
class shared_future {
    shared_ptr<__future_detail::state<T>> state_;

    explicit shared_future(shared_ptr<__future_detail::state<T>> state,
                           bool acquire)
        : state_(std::move(state)) {
        if (acquire && state_) state_->acquire_observer();
    }

    template<typename> friend class future;

public:
    using result_type = T;

    shared_future() noexcept = default;
    shared_future(const shared_future& other) : state_(other.state_) {
        if (state_) state_->acquire_observer();
    }

    shared_future(shared_future&& other) noexcept
        : state_(std::move(other.state_)) {}

    shared_future(future<T>&& other) noexcept
        : state_(std::move(other.state_)) {}

    shared_future& operator=(const shared_future& other) {
        if (this != &other) {
            shared_future copy(other);
            swap(copy);
        }
        return *this;
    }

    shared_future& operator=(shared_future&& other) noexcept {
        if (this != &other) {
            if (state_) state_->release_observer();
            state_ = std::move(other.state_);
        }
        return *this;
    }

    shared_future& operator=(future<T>&& other) noexcept {
        if (state_) state_->release_observer();
        state_ = std::move(other.state_);
        return *this;
    }

    ~shared_future() {
        if (state_) state_->release_observer();
    }

    bool valid() const noexcept { return static_cast<bool>(state_); }

    void wait() const {
        if (!state_) __future_detail::raise(future_errc::no_state);
        state_->wait();
    }

    template<typename Rep, typename Period>
    future_status wait_for(
        const chrono::duration<Rep, Period>& duration) const {
        if (!state_) __future_detail::raise(future_errc::no_state);
        return state_->wait_for(duration);
    }

    template<typename Clock, typename Duration>
    future_status wait_until(
        const chrono::time_point<Clock, Duration>& deadline) const {
        if (!state_) __future_detail::raise(future_errc::no_state);
        return state_->wait_until(deadline);
    }

    typename __future_detail::shared_result<T>::type get() const {
        if (!state_) __future_detail::raise(future_errc::no_state);
        return state_->share();
    }

    void swap(shared_future& other) noexcept { state_.swap(other.state_); }
};

template<typename T>
shared_future<T> future<T>::share() noexcept {
    if (!state_) return shared_future<T>();
    return shared_future<T>(std::move(state_), false);
}

template<typename T>
class promise {
    shared_ptr<__future_detail::state<T>> state_;
    bool future_retrieved_;

    template<typename> friend class future;

    void abandon() noexcept {
        if (state_) state_->abandon_producer();
    }

public:
    using allocator_type = allocator<unsigned char>;

    promise()
        : state_(make_shared<__future_detail::state<T>>()),
          future_retrieved_(false) {}

    template<typename Alloc>
    promise(allocator_arg_t, const Alloc& alloc)
        : state_(allocate_shared<__future_detail::state<T>>(alloc)),
          future_retrieved_(false) {}

    promise(const promise&) = delete;
    promise& operator=(const promise&) = delete;

    promise(promise&& other) noexcept
        : state_(std::move(other.state_)),
          future_retrieved_(other.future_retrieved_) {
        other.future_retrieved_ = false;
    }

    template<typename Alloc>
    promise(allocator_arg_t, const Alloc&, promise&& other) noexcept
        : state_(std::move(other.state_)),
          future_retrieved_(other.future_retrieved_) {
        other.future_retrieved_ = false;
    }

    promise& operator=(promise&& other) noexcept {
        if (this != &other) {
            abandon();
            state_ = std::move(other.state_);
            future_retrieved_ = other.future_retrieved_;
            other.future_retrieved_ = false;
        }
        return *this;
    }

    ~promise() { abandon(); }

    future<T> get_future();

    void set_value(const T& value) { require_state(); state_->set_value(value); }
    void set_value(T&& value) { require_state(); state_->set_value(std::move(value)); }
    void set_exception(exception_ptr value) {
        require_state(); state_->set_exception(std::move(value));
    }
    void set_value_at_thread_exit(const T& value) {
        require_state(); state_->set_value_at_thread_exit(value, state_);
    }
    void set_value_at_thread_exit(T&& value) {
        require_state(); state_->set_value_at_thread_exit(std::move(value), state_);
    }
    void set_exception_at_thread_exit(exception_ptr value) {
        require_state();
        state_->set_exception_at_thread_exit(std::move(value), state_);
    }

    void swap(promise& other) noexcept {
        state_.swap(other.state_);
        std::swap(future_retrieved_, other.future_retrieved_);
    }

private:
    void require_state() const {
        if (!state_) __future_detail::raise(future_errc::no_state);
    }
};

template<typename T>
class promise<T&> {
    shared_ptr<__future_detail::state<T&>> state_;
    bool future_retrieved_;

public:
    using allocator_type = allocator<unsigned char>;
    promise() : state_(make_shared<__future_detail::state<T&>>()),
                future_retrieved_(false) {}
    template<typename Alloc>
    promise(allocator_arg_t, const Alloc& alloc)
        : state_(allocate_shared<__future_detail::state<T&>>(alloc)),
          future_retrieved_(false) {}
    promise(const promise&) = delete;
    promise& operator=(const promise&) = delete;
    promise(promise&& other) noexcept
        : state_(std::move(other.state_)),
          future_retrieved_(other.future_retrieved_) {
        other.future_retrieved_ = false;
    }
    template<typename Alloc>
    promise(allocator_arg_t, const Alloc&, promise&& other) noexcept
        : state_(std::move(other.state_)),
          future_retrieved_(other.future_retrieved_) {
        other.future_retrieved_ = false;
    }
    promise& operator=(promise&& other) noexcept {
        if (this != &other) {
            abandon(); state_ = std::move(other.state_);
            future_retrieved_ = other.future_retrieved_;
            other.future_retrieved_ = false;
        }
        return *this;
    }
    ~promise() { abandon(); }
    future<T&> get_future();
    void set_value(T& value) { require_state(); state_->set_value(value); }
    void set_exception(exception_ptr value) {
        require_state(); state_->set_exception(std::move(value));
    }
    void set_value_at_thread_exit(T& value) {
        require_state(); state_->set_value_at_thread_exit(value, state_);
    }
    void set_exception_at_thread_exit(exception_ptr value) {
        require_state();
        state_->set_exception_at_thread_exit(std::move(value), state_);
    }
    void swap(promise& other) noexcept {
        state_.swap(other.state_);
        std::swap(future_retrieved_, other.future_retrieved_);
    }

private:
    void require_state() const {
        if (!state_) __future_detail::raise(future_errc::no_state);
    }
    void abandon() noexcept {
        if (state_) state_->abandon_producer();
    }
};

template<>
class promise<void> {
    shared_ptr<__future_detail::state<void>> state_;
    bool future_retrieved_;

public:
    using allocator_type = allocator<unsigned char>;
    promise() : state_(make_shared<__future_detail::state<void>>()),
                future_retrieved_(false) {}
    template<typename Alloc>
    promise(allocator_arg_t, const Alloc& alloc)
        : state_(allocate_shared<__future_detail::state<void>>(alloc)),
          future_retrieved_(false) {}
    promise(const promise&) = delete;
    promise& operator=(const promise&) = delete;
    promise(promise&& other) noexcept
        : state_(std::move(other.state_)),
          future_retrieved_(other.future_retrieved_) {
        other.future_retrieved_ = false;
    }
    template<typename Alloc>
    promise(allocator_arg_t, const Alloc&, promise&& other) noexcept
        : state_(std::move(other.state_)),
          future_retrieved_(other.future_retrieved_) {
        other.future_retrieved_ = false;
    }
    promise& operator=(promise&& other) noexcept {
        if (this != &other) {
            abandon(); state_ = std::move(other.state_);
            future_retrieved_ = other.future_retrieved_;
            other.future_retrieved_ = false;
        }
        return *this;
    }
    ~promise() { abandon(); }
    future<void> get_future();
    void set_value() { require_state(); state_->set_value(); }
    void set_exception(exception_ptr value) {
        require_state(); state_->set_exception(std::move(value));
    }
    void set_value_at_thread_exit() {
        require_state(); state_->set_value_at_thread_exit(state_);
    }
    void set_exception_at_thread_exit(exception_ptr value) {
        require_state();
        state_->set_exception_at_thread_exit(std::move(value), state_);
    }
    void swap(promise& other) noexcept {
        state_.swap(other.state_);
        std::swap(future_retrieved_, other.future_retrieved_);
    }

private:
    void require_state() const {
        if (!state_) __future_detail::raise(future_errc::no_state);
    }
    void abandon() noexcept {
        if (state_) state_->abandon_producer();
    }
};

template<typename T>
future<T> promise<T>::get_future() {
    if (!state_) __future_detail::raise(future_errc::no_state);
    if (future_retrieved_)
        __future_detail::raise(future_errc::future_already_retrieved);
    future_retrieved_ = true;
    return future<T>(state_);
}

template<typename T>
future<T&> promise<T&>::get_future() {
    if (!state_) __future_detail::raise(future_errc::no_state);
    if (future_retrieved_)
        __future_detail::raise(future_errc::future_already_retrieved);
    future_retrieved_ = true;
    return future<T&>(state_);
}

inline future<void> promise<void>::get_future() {
    if (!state_) __future_detail::raise(future_errc::no_state);
    if (future_retrieved_)
        __future_detail::raise(future_errc::future_already_retrieved);
    future_retrieved_ = true;
    return future<void>(state_);
}

template<typename T>
void swap(promise<T>& left, promise<T>& right) noexcept { left.swap(right); }

template<typename T>
void swap(future<T>& left, future<T>& right) noexcept { left.swap(right); }

template<typename T>
void swap(shared_future<T>& left, shared_future<T>& right) noexcept {
    left.swap(right);
}

template<typename R, typename... Args>
class packaged_task<R(Args...)> {
    using state_type = __future_detail::state<R>;
    using callable_type = __future_detail::packaged_callable<R, Args...>;
    unique_ptr<callable_type> callable_;
    shared_ptr<state_type> state_;
    bool future_retrieved_;
    bool invoked_;

    template<typename F>
    void install(F&& function) {
        using target_type = decay_t<F>;
        static_assert(is_invocable_r<R, target_type&, Args...>::value,
                      "packaged_task target must match its signature");
        callable_.reset(new __future_detail::packaged_callable_impl<
            R, target_type, Args...>(std::forward<F>(function)));
    }

    void abandon() noexcept {
        if (state_) state_->abandon_producer();
    }

public:
    using result_type = R;
    using allocator_type = allocator<unsigned char>;

    packaged_task() noexcept : future_retrieved_(false), invoked_(false) {}

    template<typename F>
    explicit packaged_task(F&& function)
        : state_(make_shared<state_type>()), future_retrieved_(false),
          invoked_(false) {
        install(std::forward<F>(function));
    }

    template<typename Alloc, typename F>
    packaged_task(allocator_arg_t, const Alloc& alloc, F&& function)
        : state_(allocate_shared<state_type>(alloc)),
          future_retrieved_(false), invoked_(false) {
        install(std::forward<F>(function));
    }

    packaged_task(const packaged_task&) = delete;
    packaged_task& operator=(const packaged_task&) = delete;

    packaged_task(packaged_task&& other) noexcept
        : callable_(std::move(other.callable_)),
          state_(std::move(other.state_)),
          future_retrieved_(other.future_retrieved_), invoked_(other.invoked_) {
        other.future_retrieved_ = false;
        other.invoked_ = false;
    }

    template<typename Alloc>
    packaged_task(allocator_arg_t, const Alloc&,
                  packaged_task&& other) noexcept
        : callable_(std::move(other.callable_)),
          state_(std::move(other.state_)),
          future_retrieved_(other.future_retrieved_), invoked_(other.invoked_) {
        other.future_retrieved_ = false;
        other.invoked_ = false;
    }

    packaged_task& operator=(packaged_task&& other) noexcept {
        if (this != &other) {
            abandon();
            callable_ = std::move(other.callable_);
            state_ = std::move(other.state_);
            future_retrieved_ = other.future_retrieved_;
            invoked_ = other.invoked_;
            other.future_retrieved_ = false;
            other.invoked_ = false;
        }
        return *this;
    }

    ~packaged_task() { abandon(); }

    bool valid() const noexcept { return static_cast<bool>(state_); }

    future<R> get_future() {
        if (!state_) __future_detail::raise(future_errc::no_state);
        if (future_retrieved_)
            __future_detail::raise(future_errc::future_already_retrieved);
        future_retrieved_ = true;
        return future<R>(state_);
    }

    void operator()(Args... args) {
        invoke(false, std::forward<Args>(args)...);
    }

    void make_ready_at_thread_exit(Args... args) {
        invoke(true, std::forward<Args>(args)...);
    }

    void reset() {
        if (!state_) __future_detail::raise(future_errc::no_state);
        abandon();
        state_ = make_shared<state_type>();
        future_retrieved_ = false;
        invoked_ = false;
    }

    void swap(packaged_task& other) noexcept {
        callable_.swap(other.callable_);
        state_.swap(other.state_);
        std::swap(future_retrieved_, other.future_retrieved_);
        std::swap(invoked_, other.invoked_);
    }

private:
    template<typename... CallArgs>
    void invoke(bool at_exit, CallArgs&&... args) {
        if (!state_ || !callable_)
            __future_detail::raise(future_errc::no_state);
        if (invoked_)
            __future_detail::raise(future_errc::promise_already_satisfied);
        invoked_ = true;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            __future_detail::invoke_store<R, callable_type&,
                                           CallArgs&&...>::run(
                *state_, state_, at_exit, *callable_,
                std::forward<CallArgs>(args)...);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            try {
                if (at_exit)
                    state_->set_exception_at_thread_exit(
                        current_exception(), state_);
                else
                    state_->set_exception(current_exception());
            } catch (...) {
                terminate();
            }
        }
#endif
    }
};

template<typename R, typename... Args>
void swap(packaged_task<R(Args...)>& left,
          packaged_task<R(Args...)>& right) noexcept {
    left.swap(right);
}

#if __cplusplus >= 201703L
template<typename R, typename... Args>
packaged_task(R (*)(Args...)) -> packaged_task<R(Args...)>;

template<typename R, typename... Args>
packaged_task(R (*)(Args...) noexcept) -> packaged_task<R(Args...)>;

template<typename F>
packaged_task(F) -> packaged_task<
    typename __future_detail::task_signature<decltype(&F::operator())>::type>;
#endif

namespace __future_detail {

template<typename R, typename F, typename... Args>
future<R> async_impl(launch policy, F&& function, Args&&... args) {
    const unsigned int flags = static_cast<unsigned int>(policy);
    const bool use_async = (flags & static_cast<unsigned int>(launch::async)) != 0u;
    const bool use_deferred =
        (flags & static_cast<unsigned int>(launch::deferred)) != 0u;
    if (!use_async && !use_deferred) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        throw system_error(EINVAL, generic_category(), "std::async launch policy");
#else
        terminate();
#endif
    }

    using callable_type = decay_t<F>;
    using tuple_type = tuple<decay_t<Args>...>;
    using work_type = invocation_work<R, callable_type, tuple_type>;
    shared_ptr<state<R>> shared = make_shared<state<R>>();
    shared_ptr<work<R>> invocation = make_shared<work_type>(
        std::forward<F>(function), std::forward<Args>(args)...);

    if (use_async) {
        shared->mark_async_launch();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            thread worker([shared, invocation] {
                shared->mark_async_thread(this_thread::get_id());
                invocation->run(*shared);
            });
            shared->attach_async_thread(std::move(worker));
            return future<R>(shared);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            if (!use_deferred) throw;
        }
#endif
    }

    shared->set_deferred(std::move(invocation));
    return future<R>(std::move(shared));
}

} /* namespace __future_detail */

template<typename F, typename... Args>
future<invoke_result_t<decay_t<F>, decay_t<Args>...>>
async(launch policy, F&& function, Args&&... args) {
    using result_type = invoke_result_t<decay_t<F>, decay_t<Args>...>;
    return __future_detail::async_impl<result_type>(
        policy, std::forward<F>(function), std::forward<Args>(args)...);
}

template<typename F, typename... Args>
future<invoke_result_t<decay_t<F>, decay_t<Args>...>>
async(F&& function, Args&&... args) {
    return std::async(launch::async | launch::deferred,
                      std::forward<F>(function), std::forward<Args>(args)...);
}

} /* namespace std */

#endif /* RINCXX_FUTURE_H */
