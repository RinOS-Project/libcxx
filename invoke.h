/*
 * RinOS C++ invoke primitives.
 * Kept below functional.h's dependency layer so thread.h can use std::invoke
 * when an algorithm/exception include cycle reaches it early.
 */
#ifndef RINCXX_INVOKE_H
#define RINCXX_INVOKE_H

#include "rincxx.h"
#include "type_traits.h"

#ifdef __cplusplus
namespace std {

namespace detail {

template<typename Class, typename T,
         enable_if_t<is_base_of<Class, remove_cvref_t<T>>::value, int> = 0>
constexpr T&& functional_invoke_target(T&& value) noexcept {
    return std::forward<T>(value);
}

template<typename Class, typename T>
constexpr T& functional_invoke_target(reference_wrapper<T> value) noexcept {
    return value.get();
}

template<typename Class, typename T,
         enable_if_t<
             !is_base_of<Class, remove_cvref_t<T>>::value &&
             !is_reference_wrapper<remove_cvref_t<T>>::value,
             int> = 0>
constexpr auto functional_invoke_target(T&& value)
    noexcept(noexcept(*std::forward<T>(value)))
    -> decltype(*std::forward<T>(value)) {
    return *std::forward<T>(value);
}

} /* namespace detail */

template<typename F, typename T, typename... Args,
         enable_if_t<
             is_member_function_pointer<remove_cvref_t<F>>::value,
             int> = 0>
constexpr auto invoke(F&& function, T&& target, Args&&... args)
    noexcept(noexcept(
        (detail::functional_invoke_target<
            typename detail::member_pointer_class<remove_cvref_t<F>>::type>(
                std::forward<T>(target)).*std::forward<F>(function))(
                    std::forward<Args>(args)...)))
    -> decltype(
        (detail::functional_invoke_target<
            typename detail::member_pointer_class<remove_cvref_t<F>>::type>(
                std::forward<T>(target)).*std::forward<F>(function))(
                    std::forward<Args>(args)...)) {
    return (detail::functional_invoke_target<
        typename detail::member_pointer_class<remove_cvref_t<F>>::type>(
            std::forward<T>(target)).*std::forward<F>(function))(
                std::forward<Args>(args)...);
}

template<typename F, typename T,
         enable_if_t<
             is_member_object_pointer<remove_cvref_t<F>>::value,
             int> = 0>
constexpr auto invoke(F&& function, T&& target)
    noexcept(noexcept(
        detail::functional_invoke_target<
            typename detail::member_pointer_class<remove_cvref_t<F>>::type>(
                std::forward<T>(target)).*std::forward<F>(function)))
    -> decltype((
        detail::functional_invoke_target<
            typename detail::member_pointer_class<remove_cvref_t<F>>::type>(
                std::forward<T>(target)).*std::forward<F>(function))) {
    return detail::functional_invoke_target<
        typename detail::member_pointer_class<remove_cvref_t<F>>::type>(
            std::forward<T>(target)).*std::forward<F>(function);
}

template<typename F, typename... Args,
         enable_if_t<!is_member_pointer<remove_cvref_t<F>>::value, int> = 0>
constexpr auto invoke(F&& function, Args&&... args)
    noexcept(noexcept(std::forward<F>(function)(
        std::forward<Args>(args)...)))
    -> decltype(std::forward<F>(function)(std::forward<Args>(args)...)) {
    return std::forward<F>(function)(std::forward<Args>(args)...);
}

#if __cplusplus > 202002L
template<typename R, typename F, typename... Args>
    requires is_invocable_r_v<R, F, Args...>
constexpr R invoke_r(F&& function, Args&&... args)
    noexcept(is_nothrow_invocable_r_v<R, F, Args...>)
{
    if constexpr (is_void_v<R>) {
        std::invoke(std::forward<F>(function), std::forward<Args>(args)...);
    } else {
        return std::invoke(
            std::forward<F>(function), std::forward<Args>(args)...);
    }
}
#endif

} /* namespace std */
#endif /* __cplusplus */

#endif /* RINCXX_INVOKE_H */
