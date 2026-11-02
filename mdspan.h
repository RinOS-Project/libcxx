/*
 * RinOS C++ <mdspan>
 * C++23 non-owning multidimensional views.
 */

#ifndef RINCXX_MDSPAN_H
#define RINCXX_MDSPAN_H

#include "rincxx.h"
#include "version.h"

#if defined(RIN_LIBCXX_CXX23_DIALECT)

#include "array.h"
#include "limits.h"
#include "span.h"
#include "type_traits.h"
#include "utility.h"

namespace std {

template<class IndexType, size_t... Extents>
class extents;

struct layout_right { template<class Extents> class mapping; };
struct layout_left { template<class Extents> class mapping; };
struct layout_stride { template<class Extents> class mapping; };

namespace detail {

[[noreturn]] inline void mdspan_contract_fail() noexcept {
    __builtin_trap();
}

template<size_t I>
constexpr size_t mdspan_extent_at(size_t) noexcept {
    return dynamic_extent;
}

template<size_t I, size_t First, size_t... Rest>
constexpr size_t mdspan_extent_at(size_t index) noexcept {
    return index == I ? First : mdspan_extent_at<I + 1, Rest...>(index);
}

template<size_t... Extents>
struct mdspan_extent_pack {
    static constexpr size_t rank = sizeof...(Extents);
    static constexpr size_t get(size_t index) noexcept {
        return mdspan_extent_at<0, Extents...>(index);
    }
};

template<size_t... Values>
struct mdspan_dynamic_count;

template<>
struct mdspan_dynamic_count<> : integral_constant<size_t, 0> {};

template<size_t First, size_t... Rest>
struct mdspan_dynamic_count<First, Rest...>
    : integral_constant<size_t,
          (First == dynamic_extent ? 1u : 0u) +
          mdspan_dynamic_count<Rest...>::value> {};

template<class LeftSequence, class RightSequence>
struct mdspan_extent_compatible : false_type {};

template<>
struct mdspan_extent_compatible<index_sequence<>, index_sequence<>>
    : true_type {};

template<size_t Left, size_t... LeftRest,
         size_t Right, size_t... RightRest>
struct mdspan_extent_compatible<index_sequence<Left, LeftRest...>,
                                index_sequence<Right, RightRest...>>
    : integral_constant<bool,
          (Left == dynamic_extent || Right == dynamic_extent || Left == Right) &&
          mdspan_extent_compatible<index_sequence<LeftRest...>,
                                   index_sequence<RightRest...>>::value> {};

template<class TargetSequence, class SourceSequence>
struct mdspan_static_from_dynamic : false_type {};

template<>
struct mdspan_static_from_dynamic<index_sequence<>, index_sequence<>>
    : false_type {};

template<size_t Target, size_t... TargetRest,
         size_t Source, size_t... SourceRest>
struct mdspan_static_from_dynamic<index_sequence<Target, TargetRest...>,
                                  index_sequence<Source, SourceRest...>>
    : integral_constant<bool,
          (Target != dynamic_extent && Source == dynamic_extent) ||
          mdspan_static_from_dynamic<index_sequence<TargetRest...>,
                                     index_sequence<SourceRest...>>::value> {};

template<class IndexType, size_t Rank, class Sequence>
struct mdspan_make_dextents;

template<class IndexType, size_t Rank, size_t... Indices>
struct mdspan_make_dextents<IndexType, Rank, index_sequence<Indices...>> {
    using type = extents<IndexType,
        (static_cast<void>(Indices), dynamic_extent)...>;
};

template<class Left, class Right>
struct mdspan_extents_compatible : false_type {};

template<class LeftIndex, size_t... Left, class RightIndex, size_t... Right>
struct mdspan_extents_compatible<extents<LeftIndex, Left...>,
                                 extents<RightIndex, Right...>>
    : mdspan_extent_compatible<index_sequence<Left...>,
                               index_sequence<Right...>> {};

template<class Target, class Source>
struct mdspan_extents_conversion_explicit : false_type {};

template<class TargetIndex, size_t... Target,
         class SourceIndex, size_t... Source>
struct mdspan_extents_conversion_explicit<extents<TargetIndex, Target...>,
                                          extents<SourceIndex, Source...>>
    : integral_constant<bool,
          mdspan_static_from_dynamic<index_sequence<Target...>,
                                     index_sequence<Source...>>::value ||
          (numeric_limits<TargetIndex>::digits <
           numeric_limits<SourceIndex>::digits)> {};

template<class T>
struct mdspan_is_extents : false_type {};

template<class IndexType, size_t... ExtentValues>
struct mdspan_is_extents<extents<IndexType, ExtentValues...>> : true_type {};

template<class T>
struct mdspan_maybe_static_extent
    : integral_constant<size_t, dynamic_extent> {};

template<class Integer, Integer Value>
struct mdspan_maybe_static_extent<integral_constant<Integer, Value>>
    : integral_constant<size_t, static_cast<size_t>(Value)> {};

template<class Extents>
constexpr typename Extents::index_type mdspan_product(
    const Extents& values, size_t first, size_t last) noexcept {
    using index_type = typename Extents::index_type;
    index_type product = 1;
    for (size_t dimension = first; dimension < last; ++dimension) {
        const index_type extent = values.extent(dimension);
        if (extent == 0) return 0;
        product = static_cast<index_type>(product * extent);
    }
    return product;
}

template<class Mapping, size_t... Indices>
constexpr typename Mapping::index_type mdspan_mapping_offset(
    const Mapping& mapping, const array<typename Mapping::index_type,
                                        sizeof...(Indices)>& indices,
    index_sequence<Indices...>) noexcept {
    typename Mapping::index_type offset = 0;
    ((offset = static_cast<typename Mapping::index_type>(
          offset + indices[Indices] * mapping.stride(Indices))), ...);
    return offset;
}

} /* namespace detail */

template<class IndexType, size_t Rank>
using dextents = typename detail::mdspan_make_dextents<
    IndexType, Rank, make_index_sequence<Rank>>::type;

template<class IndexType, size_t... ExtentValues>
class extents {
    static_assert(is_integral<IndexType>::value &&
                  !is_same<IndexType, bool>::value,
                  "std::extents requires an integer index type");
    static_assert(((ExtentValues == dynamic_extent ||
                    ExtentValues <= static_cast<size_t>(
                        numeric_limits<IndexType>::max())) && ...),
                  "a static extent must fit in the index type");
    static_assert(((ExtentValues == dynamic_extent ||
                    ExtentValues <= static_cast<size_t>(
                        numeric_limits<IndexType>::max())) && ...),
                  "a static extent must fit in the index type");

public:
    using index_type = IndexType;
    using size_type = typename make_unsigned<index_type>::type;
    using rank_type = size_t;

private:
    static constexpr size_t rank_value = sizeof...(ExtentValues);
    static constexpr size_t dynamic_rank_value =
        detail::mdspan_dynamic_count<ExtentValues...>::value;
    array<index_type, dynamic_rank_value> dynamic_extents_{};

    static constexpr size_t static_extent_value(size_t rank) noexcept {
        return detail::mdspan_extent_pack<ExtentValues...>::get(rank);
    }

    static constexpr size_t dynamic_index(size_t rank) noexcept {
        size_t result = 0;
        for (size_t i = 0; i < rank; ++i) {
            if (static_extent_value(i) == dynamic_extent) ++result;
        }
        return result;
    }

    constexpr void assign_all(const index_type* values) noexcept {
        for (size_t rank = 0; rank < rank_value; ++rank) {
            const size_t fixed = static_extent_value(rank);
            if (fixed == dynamic_extent) {
                dynamic_extents_[dynamic_index(rank)] = values[rank];
            } else if (values[rank] != static_cast<index_type>(fixed)) {
                detail::mdspan_contract_fail();
            }
        }
    }

public:
    static constexpr rank_type rank() noexcept { return rank_value; }
    static constexpr rank_type rank_dynamic() noexcept {
        return dynamic_rank_value;
    }

    constexpr extents() noexcept = default;

    template<class OtherIndexType, size_t... OtherExtents,
             typename enable_if<
                 detail::mdspan_extents_compatible<
                     extents, extents<OtherIndexType, OtherExtents...>>::value,
                 int>::type = 0>
    constexpr explicit(detail::mdspan_extents_conversion_explicit<
        extents, extents<OtherIndexType, OtherExtents...>>::value)
    extents(const extents<OtherIndexType, OtherExtents...>& other) noexcept {
        for (size_t rank = 0; rank < rank_value; ++rank) {
            const size_t fixed = static_extent_value(rank);
            const index_type value = static_cast<index_type>(other.extent(rank));
            if (fixed == dynamic_extent) {
                dynamic_extents_[dynamic_index(rank)] = value;
            } else if (value != static_cast<index_type>(fixed)) {
                detail::mdspan_contract_fail();
            }
        }
    }

    template<class... OtherIndexTypes,
             typename enable_if<
                 (sizeof...(OtherIndexTypes) == dynamic_rank_value ||
                  sizeof...(OtherIndexTypes) == rank_value) &&
                 conjunction<is_convertible<OtherIndexTypes, index_type>...>::value &&
                 conjunction<is_nothrow_constructible<index_type,
                                                       OtherIndexTypes>...>::value,
                 int>::type = 0>
    constexpr explicit extents(OtherIndexTypes... values) noexcept {
        const array<index_type, sizeof...(OtherIndexTypes)> input{{
            static_cast<index_type>(std::move(values))...}};
        if constexpr (sizeof...(OtherIndexTypes) == rank_value) {
            assign_all(input.data());
        } else {
            size_t dynamic = 0;
            for (size_t rank = 0; rank < rank_value; ++rank) {
                if (static_extent_value(rank) == dynamic_extent) {
                    dynamic_extents_[dynamic] = input[dynamic];
                    ++dynamic;
                }
            }
        }
    }

    template<class OtherIndexType, size_t N,
             typename enable_if<
                 (N == dynamic_rank_value || N == rank_value) &&
                 is_convertible<const OtherIndexType&, index_type>::value &&
                 is_nothrow_constructible<index_type,
                                          const OtherIndexType&>::value,
                 int>::type = 0>
    constexpr explicit(N != dynamic_rank_value)
    extents(const span<OtherIndexType, N>& values) noexcept {
        if constexpr (N == rank_value) {
            for (size_t rank = 0; rank < rank_value; ++rank) {
                const index_type value = static_cast<index_type>(values[rank]);
                const size_t fixed = static_extent_value(rank);
                if (fixed == dynamic_extent) {
                    dynamic_extents_[dynamic_index(rank)] = value;
                } else if (value != static_cast<index_type>(fixed)) {
                    detail::mdspan_contract_fail();
                }
            }
        } else {
            for (size_t rank = 0, dynamic = 0; rank < rank_value; ++rank) {
                if (static_extent_value(rank) == dynamic_extent) {
                    dynamic_extents_[dynamic] =
                        static_cast<index_type>(values[dynamic]);
                    ++dynamic;
                }
            }
        }
    }

    template<class OtherIndexType, size_t N,
             typename enable_if<
                 (N == dynamic_rank_value || N == rank_value) &&
                 is_convertible<const OtherIndexType&, index_type>::value &&
                 is_nothrow_constructible<index_type,
                                          const OtherIndexType&>::value,
                 int>::type = 0>
    constexpr explicit(N != dynamic_rank_value)
    extents(const array<OtherIndexType, N>& values) noexcept {
        if constexpr (N == rank_value) {
            for (size_t rank = 0; rank < rank_value; ++rank) {
                const index_type value = static_cast<index_type>(values[rank]);
                const size_t fixed = static_extent_value(rank);
                if (fixed == dynamic_extent) {
                    dynamic_extents_[dynamic_index(rank)] = value;
                } else if (value != static_cast<index_type>(fixed)) {
                    detail::mdspan_contract_fail();
                }
            }
        } else {
            for (size_t rank = 0, dynamic = 0; rank < rank_value; ++rank) {
                if (static_extent_value(rank) == dynamic_extent) {
                    dynamic_extents_[dynamic] =
                        static_cast<index_type>(values[dynamic]);
                    ++dynamic;
                }
            }
        }
    }

    static constexpr size_t static_extent(rank_type rank) noexcept {
        if (rank >= rank_value) detail::mdspan_contract_fail();
        return static_extent_value(rank);
    }

    constexpr index_type extent(rank_type rank) const noexcept {
        if (rank >= rank_value) detail::mdspan_contract_fail();
        const size_t fixed = static_extent_value(rank);
        return fixed == dynamic_extent
            ? dynamic_extents_[dynamic_index(rank)]
            : static_cast<index_type>(fixed);
    }
};

template<class LeftIndex, size_t... Left,
         class RightIndex, size_t... Right>
constexpr bool operator==(const extents<LeftIndex, Left...>& left,
                          const extents<RightIndex, Right...>& right) noexcept {
    if constexpr (sizeof...(Left) != sizeof...(Right)) return false;
    else {
        for (size_t rank = 0; rank < sizeof...(Left); ++rank) {
            if (left.extent(rank) != right.extent(rank)) return false;
        }
        return true;
    }
}

template<class Extents>
class layout_right::mapping {
        static_assert(detail::mdspan_is_extents<Extents>::value,
                      "layout_right requires std::extents");
    public:
        using extents_type = Extents;
        using index_type = typename extents_type::index_type;
        using size_type = typename extents_type::size_type;
        using rank_type = typename extents_type::rank_type;
        using layout_type = layout_right;

    private:
        extents_type extents_{};

    public:
        constexpr mapping() noexcept = default;
        constexpr mapping(const extents_type& values) noexcept
            : extents_(values) {}

        template<class OtherExtents,
                 typename enable_if<
                     detail::mdspan_extents_compatible<
                         extents_type, OtherExtents>::value,
                     int>::type = 0>
        constexpr explicit(!is_convertible<const OtherExtents&,
                                           extents_type>::value)
        mapping(const typename layout_right::template mapping<
                    OtherExtents>& other) noexcept
            : extents_(other.extents()) {}

        constexpr const extents_type& extents() const noexcept {
            return extents_;
        }
        constexpr index_type required_span_size() const noexcept {
            return detail::mdspan_product(extents_, 0, extents_type::rank());
        }
        static constexpr bool is_always_unique() noexcept { return true; }
        static constexpr bool is_always_exhaustive() noexcept { return true; }
        static constexpr bool is_always_strided() noexcept { return true; }
        constexpr bool is_unique() const noexcept { return true; }
        constexpr bool is_exhaustive() const noexcept { return true; }
        constexpr bool is_strided() const noexcept { return true; }
        constexpr index_type stride(rank_type rank) const noexcept {
            if (rank >= extents_type::rank()) detail::mdspan_contract_fail();
            return detail::mdspan_product(extents_, rank + 1,
                                           extents_type::rank());
        }

        template<class... Indices,
                 typename enable_if<
                     sizeof...(Indices) == extents_type::rank() &&
                     conjunction<is_convertible<Indices, index_type>...>::value &&
                     conjunction<is_nothrow_constructible<index_type,
                                                          Indices>...>::value,
                     int>::type = 0>
        constexpr index_type operator()(Indices... indices) const noexcept {
            const array<index_type, extents_type::rank()> values{{
                static_cast<index_type>(indices)...}};
            return detail::mdspan_mapping_offset(
                *this, values, make_index_sequence<extents_type::rank()>{});
        }

};

template<class Extents>
class layout_left::mapping {
        static_assert(detail::mdspan_is_extents<Extents>::value,
                      "layout_left requires std::extents");
    public:
        using extents_type = Extents;
        using index_type = typename extents_type::index_type;
        using size_type = typename extents_type::size_type;
        using rank_type = typename extents_type::rank_type;
        using layout_type = layout_left;

    private:
        extents_type extents_{};

    public:
        constexpr mapping() noexcept = default;
        constexpr mapping(const extents_type& values) noexcept
            : extents_(values) {}

        template<class OtherExtents,
                 typename enable_if<
                     detail::mdspan_extents_compatible<
                         extents_type, OtherExtents>::value,
                     int>::type = 0>
        constexpr explicit(!is_convertible<const OtherExtents&,
                                           extents_type>::value)
        mapping(const typename layout_left::template mapping<
                    OtherExtents>& other) noexcept
            : extents_(other.extents()) {}

        constexpr const extents_type& extents() const noexcept {
            return extents_;
        }
        constexpr index_type required_span_size() const noexcept {
            return detail::mdspan_product(extents_, 0, extents_type::rank());
        }
        static constexpr bool is_always_unique() noexcept { return true; }
        static constexpr bool is_always_exhaustive() noexcept { return true; }
        static constexpr bool is_always_strided() noexcept { return true; }
        constexpr bool is_unique() const noexcept { return true; }
        constexpr bool is_exhaustive() const noexcept { return true; }
        constexpr bool is_strided() const noexcept { return true; }
        constexpr index_type stride(rank_type rank) const noexcept {
            if (rank >= extents_type::rank()) detail::mdspan_contract_fail();
            return detail::mdspan_product(extents_, 0, rank);
        }

        template<class... Indices,
                 typename enable_if<
                     sizeof...(Indices) == extents_type::rank() &&
                     conjunction<is_convertible<Indices, index_type>...>::value &&
                     conjunction<is_nothrow_constructible<index_type,
                                                          Indices>...>::value,
                     int>::type = 0>
        constexpr index_type operator()(Indices... indices) const noexcept {
            const array<index_type, extents_type::rank()> values{{
                static_cast<index_type>(indices)...}};
            return detail::mdspan_mapping_offset(
                *this, values, make_index_sequence<extents_type::rank()>{});
        }

};

template<class Extents>
class layout_stride::mapping {
        static_assert(detail::mdspan_is_extents<Extents>::value,
                      "layout_stride requires std::extents");
    public:
        using extents_type = Extents;
        using index_type = typename extents_type::index_type;
        using size_type = typename extents_type::size_type;
        using rank_type = typename extents_type::rank_type;
        using layout_type = layout_stride;

    private:
        extents_type extents_{};
        array<index_type, extents_type::rank()> strides_{};

    public:
        constexpr mapping() noexcept = default;

        template<class OtherIndexType,
                 typename enable_if<
                     is_convertible<OtherIndexType, index_type>::value &&
                     is_nothrow_constructible<index_type,
                                              OtherIndexType>::value,
                     int>::type = 0>
        constexpr mapping(const extents_type& values,
                          const array<OtherIndexType,
                                      extents_type::rank()>& strides) noexcept
            : extents_(values) {
            for (rank_type rank = 0; rank < extents_type::rank(); ++rank) {
                strides_[rank] = static_cast<index_type>(strides[rank]);
            }
        }

        template<class OtherIndexType,
                 typename enable_if<
                     is_convertible<OtherIndexType, index_type>::value &&
                     is_nothrow_constructible<index_type,
                                              OtherIndexType>::value,
                     int>::type = 0>
        constexpr mapping(const extents_type& values,
                          span<OtherIndexType,
                               extents_type::rank()> strides) noexcept
            : extents_(values) {
            for (rank_type rank = 0; rank < extents_type::rank(); ++rank) {
                strides_[rank] = static_cast<index_type>(strides[rank]);
            }
        }

        template<class OtherMapping,
                 typename enable_if<
                     OtherMapping::is_always_strided() &&
                     OtherMapping::is_always_unique() &&
                     is_constructible<extents_type,
                         typename OtherMapping::extents_type>::value,
                     int>::type = 0>
        constexpr explicit(
            !is_convertible<const typename OtherMapping::extents_type&,
                            extents_type>::value)
        mapping(const OtherMapping& other) noexcept
            : extents_(other.extents()) {
            for (rank_type rank = 0; rank < extents_type::rank(); ++rank) {
                strides_[rank] = static_cast<index_type>(other.stride(rank));
            }
        }

        constexpr const extents_type& extents() const noexcept {
            return extents_;
        }
        constexpr array<index_type, extents_type::rank()>
        strides() const noexcept { return strides_; }
        constexpr index_type required_span_size() const noexcept {
            if (extents_type::rank() == 0) return 1;
            for (rank_type rank = 0; rank < extents_type::rank(); ++rank) {
                if (extents_.extent(rank) == 0) return 0;
            }
            index_type result = 1;
            for (rank_type rank = 0; rank < extents_type::rank(); ++rank) {
                result = static_cast<index_type>(
                    result + (extents_.extent(rank) - 1) * strides_[rank]);
            }
            return result;
        }
        static constexpr bool is_always_unique() noexcept { return true; }
        static constexpr bool is_always_exhaustive() noexcept {
            for (rank_type rank = 0; rank < extents_type::rank(); ++rank) {
                const size_t extent = extents_type::static_extent(rank);
                if (extent == dynamic_extent || extent > 1) return false;
            }
            return true;
        }
        static constexpr bool is_always_strided() noexcept { return true; }
        constexpr bool is_unique() const noexcept { return true; }
        constexpr bool is_exhaustive() const noexcept {
            return required_span_size() ==
                   detail::mdspan_product(extents_, 0, extents_type::rank());
        }
        constexpr bool is_strided() const noexcept { return true; }
        constexpr index_type stride(rank_type rank) const noexcept {
            if (rank >= extents_type::rank()) detail::mdspan_contract_fail();
            return strides_[rank];
        }

        template<class... Indices,
                 typename enable_if<
                     sizeof...(Indices) == extents_type::rank() &&
                     conjunction<is_convertible<Indices, index_type>...>::value &&
                     conjunction<is_nothrow_constructible<index_type,
                                                          Indices>...>::value,
                     int>::type = 0>
        constexpr index_type operator()(Indices... indices) const noexcept {
            const array<index_type, extents_type::rank()> values{{
                static_cast<index_type>(indices)...}};
            return detail::mdspan_mapping_offset(
                *this, values, make_index_sequence<extents_type::rank()>{});
        }

};

namespace detail {
template<class T>
struct mdspan_is_layout_mapping : false_type {};
template<class E>
struct mdspan_is_layout_mapping<layout_right::mapping<E>> : true_type {};
template<class E>
struct mdspan_is_layout_mapping<layout_left::mapping<E>> : true_type {};
template<class E>
struct mdspan_is_layout_mapping<layout_stride::mapping<E>> : true_type {};
} /* namespace detail */

template<class LeftMapping, class RightMapping,
         typename enable_if<
             detail::mdspan_is_layout_mapping<LeftMapping>::value &&
             detail::mdspan_is_layout_mapping<RightMapping>::value,
             int>::type = 0>
constexpr bool operator==(const LeftMapping& left,
                          const RightMapping& right) noexcept {
    if (!(left.extents() == right.extents())) return false;
    using left_extents_type = typename LeftMapping::extents_type;
    for (size_t rank = 0; rank < left_extents_type::rank(); ++rank) {
        if (left.stride(rank) != right.stride(rank)) return false;
    }
    return true;
}

template<class ElementType>
class default_accessor {
public:
    using offset_policy = default_accessor;
    using element_type = ElementType;
    using reference = element_type&;
    using data_handle_type = element_type*;

    constexpr default_accessor() noexcept = default;
    template<class OtherElementType,
             typename enable_if<is_convertible<OtherElementType(*)[],
                                               element_type(*)[]>::value,
                                int>::type = 0>
    constexpr default_accessor(
        const default_accessor<OtherElementType>&) noexcept {}
    constexpr reference access(data_handle_type pointer,
                               size_t index) const noexcept {
        return pointer[index];
    }
    constexpr data_handle_type offset(data_handle_type pointer,
                                      size_t index) const noexcept {
        return pointer + index;
    }
};

template<class ElementType, class Extents,
         class LayoutPolicy = layout_right,
         class AccessorPolicy = default_accessor<ElementType>>
class mdspan {
    static_assert(detail::mdspan_is_extents<Extents>::value,
                  "std::mdspan requires std::extents");
public:
    using extents_type = Extents;
    using layout_type = LayoutPolicy;
    using accessor_type = AccessorPolicy;
    using mapping_type = typename layout_type::template mapping<extents_type>;
    using element_type = ElementType;
    using value_type = typename remove_cv<element_type>::type;
    using index_type = typename extents_type::index_type;
    using size_type = typename extents_type::size_type;
    using rank_type = typename extents_type::rank_type;
    using data_handle_type = typename accessor_type::data_handle_type;
    using reference = typename accessor_type::reference;
    static_assert(is_same<element_type,
                          typename accessor_type::element_type>::value,
                  "mdspan element and accessor element types must match");

private:
    accessor_type accessor_{};
    mapping_type mapping_{};
    data_handle_type pointer_{};

public:
    constexpr mdspan()
        requires (extents_type::rank_dynamic() > 0 &&
                  is_default_constructible<data_handle_type>::value &&
                  is_default_constructible<mapping_type>::value &&
                  is_default_constructible<accessor_type>::value) = default;

    template<class... OtherIndexTypes,
             typename enable_if<
                 (sizeof...(OtherIndexTypes) == extents_type::rank() ||
                  sizeof...(OtherIndexTypes) == extents_type::rank_dynamic()) &&
                 conjunction<is_convertible<OtherIndexTypes, index_type>...>::value &&
                 is_constructible<mapping_type, extents_type>::value &&
                 is_default_constructible<accessor_type>::value,
                 int>::type = 0>
    constexpr explicit mdspan(data_handle_type pointer,
                              OtherIndexTypes... values)
        : accessor_(), mapping_(extents_type(std::move(values)...)),
          pointer_(std::move(pointer)) {}

    template<class OtherIndexType, size_t N,
             typename enable_if<
                 (N == extents_type::rank() ||
                  N == extents_type::rank_dynamic()) &&
                 is_default_constructible<accessor_type>::value,
                 int>::type = 0>
    constexpr explicit(N != extents_type::rank_dynamic())
    mdspan(data_handle_type pointer,
           const span<OtherIndexType, N>& values)
        : accessor_(), mapping_(extents_type(values)),
          pointer_(std::move(pointer)) {}

    template<class OtherIndexType, size_t N,
             typename enable_if<
                 (N == extents_type::rank() ||
                  N == extents_type::rank_dynamic()) &&
                 is_default_constructible<accessor_type>::value,
                 int>::type = 0>
    constexpr explicit(N != extents_type::rank_dynamic())
    mdspan(data_handle_type pointer,
           const array<OtherIndexType, N>& values)
        : accessor_(), mapping_(extents_type(values)),
          pointer_(std::move(pointer)) {}

    constexpr mdspan(data_handle_type pointer,
                     const extents_type& values)
        requires (is_default_constructible<accessor_type>::value)
        : accessor_(), mapping_(values), pointer_(std::move(pointer)) {}

    constexpr mdspan(data_handle_type pointer,
                     const mapping_type& mapping)
        requires (is_default_constructible<accessor_type>::value)
        : accessor_(), mapping_(mapping), pointer_(std::move(pointer)) {}

    constexpr mdspan(data_handle_type pointer, const mapping_type& mapping,
                     const accessor_type& accessor)
        : accessor_(accessor), mapping_(mapping), pointer_(std::move(pointer)) {}

    template<class OtherElementType, class OtherExtents,
             class OtherLayout, class OtherAccessor,
             typename enable_if<
                 is_constructible<mapping_type,
                     const typename OtherLayout::template mapping<
                         OtherExtents>&>::value &&
                 is_constructible<accessor_type,
                                  const OtherAccessor&>::value &&
                 is_constructible<data_handle_type,
                     const typename OtherAccessor::data_handle_type&>::value &&
                 is_constructible<extents_type, OtherExtents>::value,
                 int>::type = 0>
    constexpr explicit(
        !is_convertible<const typename OtherLayout::template mapping<
                            OtherExtents>&, mapping_type>::value ||
        !is_convertible<const OtherAccessor&, accessor_type>::value)
    mdspan(const mdspan<OtherElementType, OtherExtents,
                        OtherLayout, OtherAccessor>& other)
        : accessor_(other.accessor()), mapping_(other.mapping()),
          pointer_(other.data_handle()) {}

    template<class... Indices,
             typename enable_if<sizeof...(Indices) ==
                                    extents_type::rank() &&
                                    conjunction<is_convertible<Indices,
                                                               index_type>...>::value &&
                                    conjunction<is_nothrow_constructible<
                                        index_type, Indices>...>::value,
                                int>::type = 0>
    constexpr reference operator()(Indices... indices) const noexcept {
        return accessor_.access(pointer_, mapping_(indices...));
    }

    template<class... Indices,
             typename enable_if<sizeof...(Indices) ==
                                    extents_type::rank() &&
                                    conjunction<is_convertible<Indices,
                                                               index_type>...>::value &&
                                    conjunction<is_nothrow_constructible<
                                        index_type, Indices>...>::value,
                                int>::type = 0>
    constexpr reference operator[](Indices... indices) const noexcept {
        return (*this)(std::move(indices)...);
    }

    static constexpr rank_type rank() noexcept { return extents_type::rank(); }
    static constexpr rank_type rank_dynamic() noexcept {
        return extents_type::rank_dynamic();
    }
    static constexpr index_type static_extent(rank_type rank) noexcept {
        return static_cast<index_type>(extents_type::static_extent(rank));
    }
    constexpr size_type size() const noexcept {
        return static_cast<size_type>(
            detail::mdspan_product(extents(), 0, extents_type::rank()));
    }
    constexpr bool empty() const noexcept { return size() == 0; }
    constexpr index_type extent(rank_type rank) const noexcept {
        return extents().extent(rank);
    }
    constexpr const extents_type& extents() const noexcept {
        return mapping_.extents();
    }
    constexpr const data_handle_type& data_handle() const noexcept {
        return pointer_;
    }
    constexpr const mapping_type& mapping() const noexcept {
        return mapping_;
    }
    constexpr const accessor_type& accessor() const noexcept {
        return accessor_;
    }
    static constexpr bool is_always_unique() noexcept {
        return mapping_type::is_always_unique();
    }
    static constexpr bool is_always_exhaustive() noexcept {
        return mapping_type::is_always_exhaustive();
    }
    static constexpr bool is_always_strided() noexcept {
        return mapping_type::is_always_strided();
    }
    constexpr bool is_unique() const noexcept { return mapping_.is_unique(); }
    constexpr bool is_exhaustive() const noexcept {
        return mapping_.is_exhaustive();
    }
    constexpr bool is_strided() const noexcept { return mapping_.is_strided(); }
    constexpr index_type stride(rank_type rank) const noexcept {
        return mapping_.stride(rank);
    }

    friend constexpr void swap(mdspan& left, mdspan& right) noexcept {
        using std::swap;
        swap(left.pointer_, right.pointer_);
        swap(left.mapping_, right.mapping_);
        swap(left.accessor_, right.accessor_);
    }
};

template<class ElementType, class... Integrals>
    requires (sizeof...(Integrals) > 0 &&
              conjunction<is_convertible<Integrals, size_t>...>::value)
explicit mdspan(ElementType*, Integrals...)
    -> mdspan<ElementType, extents<size_t,
        detail::mdspan_maybe_static_extent<
            typename remove_cvref<Integrals>::type>::value...>>;

template<class Pointer>
    requires is_pointer<remove_reference_t<Pointer>>::value
mdspan(Pointer&&)
    -> mdspan<remove_pointer_t<remove_reference_t<Pointer>>,
              extents<size_t>>;

template<class ElementType, class OtherIndexType, size_t N>
mdspan(ElementType*, const span<OtherIndexType, N>&)
    -> mdspan<ElementType, dextents<size_t, N>>;

template<class ElementType, class OtherIndexType, size_t N>
mdspan(ElementType*, const array<OtherIndexType, N>&)
    -> mdspan<ElementType, dextents<size_t, N>>;

template<class ElementType, class IndexType, size_t... ExtentValues>
mdspan(ElementType*, const extents<IndexType, ExtentValues...>&)
    -> mdspan<ElementType, extents<IndexType, ExtentValues...>>;

template<class ElementType, class MappingType>
mdspan(ElementType*, const MappingType&)
    -> mdspan<ElementType, typename MappingType::extents_type,
              typename MappingType::layout_type>;

template<class MappingType, class AccessorType>
mdspan(typename AccessorType::data_handle_type, const MappingType&,
       const AccessorType&)
    -> mdspan<typename AccessorType::element_type,
              typename MappingType::extents_type,
              typename MappingType::layout_type, AccessorType>;

template<class ElementType, size_t N>
mdspan(ElementType (&)[N]) -> mdspan<ElementType, extents<size_t, N>>;

} /* namespace std */

#endif /* defined(RIN_LIBCXX_CXX23_DIALECT) */

#endif /* RINCXX_MDSPAN_H */
