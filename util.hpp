#pragma once
#include <concepts>

namespace detail {
    template <typename T>
    using uncvref_t = std::remove_cv<std::remove_reference<T>>;

    template <typename T>
    using iter_difference_t = typename std::incrementable_traits<uncvref_t<T>>::difference_type;

    template <typename I>
    using iter_size_t = std::conditional_t<std::is_integral<std::iter_difference_t<I>>::value,
        std::make_unsigned<iter_difference_t<I>>,
        std::iter_difference_t<I>>;


    template <typename C>
    concept has_reserve_function = requires(C && c) {
        c.reserve(0);
    };

    template <has_reserve_function C, std::integral N>
    void reserve(C& c, N n) {
        c.reserve(n);
    }

    template <typename C, std::integral N>
    void reserve(C& c, N n) {}
} // namespace detail

template <typename I, typename S>
constexpr std::ranges::subrange<I, S> make_subrange2(I i, S s) {
    return { i, s };
}

template <typename I, typename S>
requires std::input_or_output_iterator<I>&& std::sentinel_for<S, I>            //
constexpr std::ranges::subrange<I, S, std::ranges::subrange_kind::sized> //
make_subrange2(I i, S s, detail::iter_size_t<I> n) {
    return { i, s, n };
}

template <typename R>
constexpr auto make_subrange2(R&& r)
-> std::ranges::subrange<std::ranges::iterator_t<R>,
    std::ranges::sentinel_t<R>,
    (std::ranges::sized_range<R> || std::sized_sentinel_for<std::ranges::sentinel_t<R>, std::ranges::iterator_t<R>>)
    ? std::ranges::subrange_kind::sized
    : std::ranges::subrange_kind::unsized> //
{
    return { static_cast<R&&>(r) };
}


