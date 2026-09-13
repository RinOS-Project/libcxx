/*
 * basic_string/StringViewLike definitions for the string_view-first include
 * order.  string_view.h cannot define these members until basic_string is
 * complete; string.h includes this bridge after both headers are available.
 */
#ifndef RINCXX_STRING_VIEW_STRING_BRIDGE_H
#define RINCXX_STRING_VIEW_STRING_BRIDGE_H

#if defined(RINCXX_STRING_H) && __cplusplus >= 201703L && \
    !defined(RINCXX_STRING_VIEW_BRIDGE_DEFINED)
#define RINCXX_STRING_VIEW_BRIDGE_DEFINED 1

namespace std {

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::operator+=(
    const StringViewLike& sv) {
    return append(sv);
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::append(
    const StringViewLike& sv) {
    const basic_string_view<CharT, Traits> view(sv);
    return append(view.data(), view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::append(
    const StringViewLike& sv, size_type pos, size_type count) {
    const basic_string_view<CharT, Traits> view(sv);
    const basic_string_view<CharT, Traits> part = view.substr(pos, count);
    return append(part.data(), part.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::insert(
    size_type pos, const StringViewLike& sv) {
    const basic_string_view<CharT, Traits> view(sv);
    return insert(pos, view.data(), view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::insert(
    size_type pos, const StringViewLike& sv, size_type subpos,
    size_type count) {
    const basic_string_view<CharT, Traits> view(sv);
    const basic_string_view<CharT, Traits> part = view.substr(subpos, count);
    return insert(pos, part.data(), part.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::replace(
    size_type pos, size_type count, const StringViewLike& sv) {
    const basic_string_view<CharT, Traits> view(sv);
    return replace(pos, count, view.data(), view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::replace(
    size_type pos, size_type count, const StringViewLike& sv,
    size_type subpos, size_type count2) {
    const basic_string_view<CharT, Traits> view(sv);
    const basic_string_view<CharT, Traits> part = view.substr(subpos, count2);
    return replace(pos, count, part.data(), part.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             is_convertible<const StringViewLike&,
                            basic_string_view<CharT, Traits>>::value &&
             !is_convertible<const StringViewLike&, const CharT*>::value,
             int>::type>
typename basic_string<CharT, Traits, Allocator>::size_type
basic_string<CharT, Traits, Allocator>::find(
    const StringViewLike& str, size_type pos) const noexcept {
    const basic_string_view<CharT, Traits> view(str);
    return find(view.data(), pos, view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             is_convertible<const StringViewLike&,
                            basic_string_view<CharT, Traits>>::value &&
             !is_convertible<const StringViewLike&, const CharT*>::value,
             int>::type>
typename basic_string<CharT, Traits, Allocator>::size_type
basic_string<CharT, Traits, Allocator>::rfind(
    const StringViewLike& str, size_type pos) const noexcept {
    const basic_string_view<CharT, Traits> view(str);
    return rfind(view.data(), pos, view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             is_convertible<const StringViewLike&,
                            basic_string_view<CharT, Traits>>::value &&
             !is_convertible<const StringViewLike&, const CharT*>::value,
             int>::type>
int basic_string<CharT, Traits, Allocator>::compare(
    const StringViewLike& str) const noexcept {
    const basic_string_view<CharT, Traits> view(str);
    return compare_data(m_data, m_size, view.data(), view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             is_convertible<const StringViewLike&,
                            basic_string_view<CharT, Traits>>::value &&
             !is_convertible<const StringViewLike&, const CharT*>::value,
             int>::type>
int basic_string<CharT, Traits, Allocator>::compare(
    size_type pos, size_type count, const StringViewLike& str) const {
    const basic_string_view<CharT, Traits> view(str);
    return compare(pos, count, view.data(), view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             is_convertible<const StringViewLike&,
                            basic_string_view<CharT, Traits>>::value &&
             !is_convertible<const StringViewLike&, const CharT*>::value,
             int>::type>
int basic_string<CharT, Traits, Allocator>::compare(
    size_type pos1, size_type count1, const StringViewLike& str,
    size_type pos2, size_type count2) const {
    const basic_string_view<CharT, Traits> view(str);
    const basic_string_view<CharT, Traits> part = view.substr(pos2, count2);
    return compare(pos1, count1, part.data(), part.size());
}

#if __cplusplus >= 202002L
template<typename CharT, typename Traits, typename Allocator>
bool basic_string<CharT, Traits, Allocator>::starts_with(
    basic_string_view<CharT, Traits> sv) const noexcept {
    return sv.size() <= m_size &&
           Traits::compare(m_data, sv.data(), sv.size()) == 0;
}

template<typename CharT, typename Traits, typename Allocator>
bool basic_string<CharT, Traits, Allocator>::ends_with(
    basic_string_view<CharT, Traits> sv) const noexcept {
    return sv.size() <= m_size &&
           Traits::compare(m_data + (m_size - sv.size()), sv.data(),
                           sv.size()) == 0;
}

#if __cplusplus > 202002L
template<typename CharT, typename Traits, typename Allocator>
bool basic_string<CharT, Traits, Allocator>::contains(
    basic_string_view<CharT, Traits> sv) const noexcept {
    return find(sv.data(), 0, sv.size()) != npos;
}
#endif
#endif

} /* namespace std */
#endif

#endif /* RINCXX_STRING_VIEW_STRING_BRIDGE_H */
