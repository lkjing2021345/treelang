/**
 * @file helper.hpp
 * @brief 通用编译期工具函数。
 */

#ifndef INCLUDE_TREELANG_CORE_HELPER_HPP
#define INCLUDE_TREELANG_CORE_HELPER_HPP

#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace treelang
{
    namespace detail
    {
        /**
         * @brief transform_array 的实现细节。
         *
         * 将 std::array 中的每个元素经 func 变换后写入新数组。
         *
         * @tparam Arr 数组类型。
         * @tparam Func 变换函数类型。
         * @param arr 待变换的数组（左值或右值引用）。
         * @param func 逐元素变换函数。
         * @return 变换结果数组。
         */
        template <typename Arr, typename Func>
        constexpr auto transform_array_impl(Arr &&arr, Func &&func)
        {
            using ArrType = std::remove_cvref_t<Arr>;
            constexpr size_t N = std::tuple_size_v<ArrType>;

            using ElemType = decltype(std::forward<Arr>(arr)[0]);
            using ResultType = std::remove_cvref_t<std::invoke_result_t<Func, ElemType>>;

            std::array<ResultType, N> dst{};
            for (size_t i = 0; i < N; ++i) dst[i] = func(std::forward<Arr>(arr)[i]);

            return dst;
        }
    }

    /**
     * @brief 将 std::array 逐元素变换为新数组（左值版本）。
     * @tparam T 元素类型。
     * @tparam N 数组长度。
     * @tparam Func 变换函数类型。
     * @param arr 待变换的数组（左值引用）。
     * @param func 逐元素变换函数。
     * @return 变换结果数组。
     */
    template <typename T, size_t N, typename Func>
    constexpr auto transform_array(const std::array<T, N> &arr, Func &&func)
    {
        return detail::transform_array_impl(arr, std::forward<Func>(func));
    }

    /**
     * @brief 将 std::array 逐元素变换为新数组（右值版本）。
     * @tparam T 元素类型。
     * @tparam N 数组长度。
     * @tparam Func 变换函数类型。
     * @param arr 待变换的数组（右值引用）。
     * @param func 逐元素变换函数。
     * @return 变换结果数组。
     */
    template <typename T, size_t N, typename Func>
    constexpr auto transform_array(std::array<T, N> &&arr, Func &&func)
    {
        return detail::transform_array_impl(std::move(arr), std::forward<Func>(func));
    }
}

#endif  // INCLUDE_TREELANG_CORE_HELPER_HPP