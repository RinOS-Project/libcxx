/*
 * RinOS C++23 smart pointer output adaptors.
 */

#ifndef RINCXX_OUT_PTR_H
#define RINCXX_OUT_PTR_H

#include "memory.h"
#include "tuple.h"

#if __cplusplus > 202002L

namespace std {

namespace __out_ptr_detail {

template<typename Smart, typename = void>
struct pointer_of_element {
    using type = typename pointer_traits<Smart>::element_type*;
};

template<typename Smart>
struct pointer_of_element<Smart, void_t<typename Smart::element_type>> {
    using type = typename Smart::element_type*;
};

template<typename Smart, typename = void>
struct pointer_of : pointer_of_element<Smart> {};

template<typename Smart>
struct pointer_of<Smart, void_t<typename Smart::pointer>> {
    using type = typename Smart::pointer;
};

template<typename Smart, typename Pointer, bool = is_void<Pointer>::value>
struct select_pointer {
    using type = Pointer;
};

template<typename Smart, typename Pointer>
struct select_pointer<Smart, Pointer, true> : pointer_of<Smart> {};

template<typename Smart, typename Fallback, typename = void>
struct pointer_of_or {
    using type = Fallback;
};

template<typename Smart, typename Fallback>
struct pointer_of_or<Smart, Fallback,
                     void_t<typename pointer_of<Smart>::type>> {
    using type = typename pointer_of<Smart>::type;
};

template<typename Smart>
struct has_reset_no_args {
private:
    template<typename T>
    static auto test(int) -> decltype(
        (void)declval<T&>().reset(), true_type{});
    template<typename>
    static false_type test(...);

public:
    static constexpr bool value = decltype(test<Smart>(0))::value;
};

template<typename Smart, typename Pointer, typename... Args>
struct has_reset_with_pointer {
private:
    template<typename T>
    static auto test(int) -> decltype(
        (void)declval<T&>().reset(declval<Pointer>(), declval<Args>()...),
         true_type{});
    template<typename>
    static false_type test(...);

public:
    static constexpr bool value = decltype(test<Smart>(0))::value;
};

template<typename Smart, typename = void>
struct has_release : false_type {};

template<typename Smart>
struct has_release<Smart, void_t<decltype(declval<Smart&>().release())>>
    : true_type {};

template<typename Smart>
struct is_shared_ptr : false_type {};

template<typename T>
struct is_shared_ptr<shared_ptr<T>> : true_type {};

template<typename T>
struct is_object_pointer : false_type {};

template<typename T>
struct is_object_pointer<T*>
    : integral_constant<bool, !is_function<T>::value> {};

template<typename Smart>
void prepare_out_ptr(Smart& smart, true_type) {
    smart.reset();
}

template<typename Smart>
void prepare_out_ptr(Smart& smart, false_type) {
    static_assert(is_default_constructible<Smart>::value,
                  "out_ptr requires reset() or a default-constructible owner");
    smart = Smart();
}

template<typename Smart, typename Pointer>
Pointer initial_inout_pointer(Smart& smart, true_type) {
    return static_cast<Pointer>(smart);
}

template<typename Smart, typename Pointer>
auto initial_inout_pointer(Smart& smart, false_type)
    -> decltype(static_cast<Pointer>(smart.get())) {
    return static_cast<Pointer>(smart.get());
}

template<typename Pointer>
Pointer output_pointer_value(const Pointer& pointer, void*, bool, false_type) {
    return pointer;
}

template<typename Pointer>
Pointer output_pointer_value(const Pointer& pointer, void* void_pointer,
                             bool used_void_pointer, true_type) {
    return used_void_pointer ? static_cast<Pointer>(void_pointer) : pointer;
}

template<typename Smart, typename Pointer, typename SmartPointer,
         typename Tuple, size_t... Indices>
void commit_out_pointer(Smart& smart, const Pointer& pointer,
                        void* void_pointer, bool used_void_pointer,
                        Tuple& arguments, index_sequence<Indices...>) {
    const Pointer output = output_pointer_value(
        pointer, void_pointer, used_void_pointer,
        integral_constant<bool, is_object_pointer<Pointer>::value>{});
    if (!output)
        return;

    const SmartPointer owner_pointer = static_cast<SmartPointer>(output);
    if constexpr (has_reset_with_pointer<
                      Smart, SmartPointer,
                      tuple_element_t<Indices, Tuple>...>::value) {
        smart.reset(owner_pointer,
                    forward<tuple_element_t<Indices, Tuple>>(
                        get<Indices>(arguments))...);
    } else {
        static_assert(is_constructible<
                          Smart, SmartPointer,
                          tuple_element_t<Indices, Tuple>...>::value,
                      "out_ptr owner must reset or construct from its pointer and arguments");
        smart = Smart(owner_pointer,
                      forward<tuple_element_t<Indices, Tuple>>(
                          get<Indices>(arguments))...);
    }
}

template<typename Smart, typename Pointer, typename Tuple, size_t... Indices>
void commit_inout_pointer(Smart& smart, const Pointer& pointer,
                          void* void_pointer, bool used_void_pointer,
                          bool released, Tuple& arguments,
                          index_sequence<Indices...>) {
    if constexpr (is_pointer<Smart>::value) {
        static_assert(sizeof...(Indices) == 0,
                      "raw pointer inout_ptr does not accept reset arguments");
        const Pointer output = output_pointer_value(
            pointer, void_pointer, used_void_pointer,
            integral_constant<bool, is_object_pointer<Pointer>::value>{});
        smart = static_cast<Smart>(output);
    } else {
        using smart_pointer = typename pointer_of_or<Smart, Pointer>::type;
        const Pointer output = output_pointer_value(
            pointer, void_pointer, used_void_pointer,
            integral_constant<bool, is_object_pointer<Pointer>::value>{});
        if (!released) {
            if constexpr (has_release<Smart>::value)
                (void)smart.release();
        }
        if (!output)
            return;

        const smart_pointer owner_pointer = static_cast<smart_pointer>(output);
        if constexpr (has_reset_with_pointer<
                          Smart, smart_pointer,
                          tuple_element_t<Indices, Tuple>...>::value) {
            smart.reset(owner_pointer,
                        forward<tuple_element_t<Indices, Tuple>>(
                            get<Indices>(arguments))...);
        } else {
            static_assert(is_constructible<Smart, smart_pointer,
                                           tuple_element_t<Indices, Tuple>...>::value,
                          "inout_ptr owner must reset or construct from its pointer and arguments");
            smart = Smart(owner_pointer,
                          forward<tuple_element_t<Indices, Tuple>>(
                              get<Indices>(arguments))...);
        }
    }
}

} /* namespace __out_ptr_detail */

template<typename Smart, typename Pointer, typename... Args>
class out_ptr_t {
    Smart& smart_;
    tuple<Args...> arguments_;
    mutable Pointer pointer_;
    mutable void* void_pointer_;
    mutable bool used_void_pointer_;

public:
    explicit out_ptr_t(Smart& smart, Args... args)
        : smart_(smart), arguments_(forward<Args>(args)...), pointer_(),
          void_pointer_(nullptr), used_void_pointer_(false) {
        static_assert(!__out_ptr_detail::is_shared_ptr<Smart>::value ||
                          sizeof...(Args) != 0,
                      "out_ptr with shared_ptr requires a deleter argument");
        __out_ptr_detail::prepare_out_ptr(
            smart_, integral_constant<bool,
                __out_ptr_detail::has_reset_no_args<Smart>::value>{});
    }

    out_ptr_t(const out_ptr_t&) = delete;
    out_ptr_t(out_ptr_t&&) = delete;
    out_ptr_t& operator=(const out_ptr_t&) = delete;
    out_ptr_t& operator=(out_ptr_t&&) = delete;

    ~out_ptr_t() {
        using smart_pointer = typename __out_ptr_detail::pointer_of_or<
            Smart, Pointer>::type;
        __out_ptr_detail::commit_out_pointer<
            Smart, Pointer, smart_pointer>(
                smart_, pointer_, void_pointer_, used_void_pointer_,
                arguments_, make_index_sequence<sizeof...(Args)>{});
    }

    operator Pointer*() const noexcept {
        return addressof(pointer_);
    }

    template<typename P = Pointer,
             typename enable_if<
                 __out_ptr_detail::is_object_pointer<P>::value &&
                     !is_same<P, void*>::value,
                 int>::type = 0>
    operator void**() const noexcept {
        used_void_pointer_ = true;
        void_pointer_ = const_cast<void*>(
            static_cast<const volatile void*>(pointer_));
        return addressof(void_pointer_);
    }
};

template<typename Smart, typename Pointer, typename... Args>
class inout_ptr_t {
    Smart& smart_;
    tuple<Args...> arguments_;
    mutable Pointer pointer_;
    mutable void* void_pointer_;
    mutable bool used_void_pointer_;
    bool released_;

public:
    explicit inout_ptr_t(Smart& smart, Args... args)
        : smart_(smart), arguments_(forward<Args>(args)...),
          pointer_(__out_ptr_detail::initial_inout_pointer<Pointer>(
              smart, integral_constant<bool, is_pointer<Smart>::value>{})),
          void_pointer_(nullptr), used_void_pointer_(false), released_(false) {
        if constexpr (!is_pointer<Smart>::value &&
                      __out_ptr_detail::has_release<Smart>::value) {
            (void)smart_.release();
            released_ = true;
        }
    }

    inout_ptr_t(const inout_ptr_t&) = delete;
    inout_ptr_t(inout_ptr_t&&) = delete;
    inout_ptr_t& operator=(const inout_ptr_t&) = delete;
    inout_ptr_t& operator=(inout_ptr_t&&) = delete;

    ~inout_ptr_t() {
        __out_ptr_detail::commit_inout_pointer<Smart, Pointer>(
                smart_, pointer_, void_pointer_, used_void_pointer_,
                released_, arguments_, make_index_sequence<sizeof...(Args)>{});
    }

    operator Pointer*() const noexcept {
        return addressof(pointer_);
    }

    template<typename P = Pointer,
             typename enable_if<
                 __out_ptr_detail::is_object_pointer<P>::value &&
                     !is_same<P, void*>::value,
                 int>::type = 0>
    operator void**() const noexcept {
        used_void_pointer_ = true;
        void_pointer_ = const_cast<void*>(
            static_cast<const volatile void*>(pointer_));
        return addressof(void_pointer_);
    }
};

template<typename Pointer = void, typename Smart, typename... Args>
auto out_ptr(Smart& smart, Args&&... args)
    -> out_ptr_t<Smart,
        typename __out_ptr_detail::select_pointer<Smart, Pointer>::type,
        Args&&...> {
    using result_type = out_ptr_t<
        Smart, typename __out_ptr_detail::select_pointer<Smart, Pointer>::type,
        Args&&...>;
    return result_type(smart, forward<Args>(args)...);
}

template<typename Pointer = void, typename Smart, typename... Args>
auto inout_ptr(Smart& smart, Args&&... args)
    -> inout_ptr_t<Smart,
        typename __out_ptr_detail::select_pointer<Smart, Pointer>::type,
        Args&&...> {
    using result_type = inout_ptr_t<
        Smart, typename __out_ptr_detail::select_pointer<Smart, Pointer>::type,
        Args&&...>;
    return result_type(smart, forward<Args>(args)...);
}

} /* namespace std */

#endif /* C++23 */
#endif /* RINCXX_OUT_PTR_H */
