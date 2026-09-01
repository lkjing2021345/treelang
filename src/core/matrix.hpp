/**
 * @file matrix.hpp
 * @brief 通用固定尺寸矩阵类模板。
 *
 * 提供 Matrix / MatrixRow / MatrixElement 三层结构，适用于地图网格等
 * 固定尺寸的二维容器。复制/移动语义由元素类型 T 的特性在编译期启用
 * 或禁用（requires 子句）。
 */

#ifndef INCLUDE_TREELANG_CORE_MATRIX_HPP
#define INCLUDE_TREELANG_CORE_MATRIX_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

#include "helper.hpp"

namespace treelang
{
    /**
     * @brief 矩阵元素包装。
     *
     * 包装单个元素值，使得 Matrix 的复制/移动语义可随 T 的特性
     * 在编译期启用或禁用。
     *
     * @tparam T 元素类型；不能是引用或 const 限定。
     */
    template <typename T>
        requires(!std::is_reference_v<T> && !std::is_const_v<T>)
    class MatrixElement
    {
    private:
        T m_data;

    public:
        /** @brief 默认构造（要求 T 可默认构造）。 */
        MatrixElement()
            requires(std::is_default_constructible_v<T>)
        = default;
        /** @brief 拷贝构造（要求 T 可拷贝构造）。 */
        MatrixElement(const MatrixElement<T> &)
            requires(std::is_copy_constructible_v<T>)
        = default;
        /** @brief 移动构造（要求 T 可移动构造）。 */
        MatrixElement(MatrixElement<T> &&) noexcept
            requires(std::is_move_constructible_v<T>)
        = default;

        /**
         * @brief 从常量引用构造。
         * @param val 元素值。
         */
        MatrixElement(const T &val)
            requires(std::is_copy_constructible_v<T>)
            : m_data(val)
        {
        }
        /**
         * @brief 从右值移动构造。
         * @param val 元素值。
         */
        MatrixElement(T &&val)
            requires(std::is_move_constructible_v<T>)
            : m_data(std::move(val))
        {
        }

    public:
        /** @brief 拷贝赋值（要求 T 可拷贝赋值）。 */
        MatrixElement &operator=(const MatrixElement<T> &)
            requires(std::is_copy_assignable_v<T>)
        = default;
        /** @brief 移动赋值（要求 T 可移动赋值）。 */
        MatrixElement &operator=(MatrixElement<T> &&) noexcept
            requires(std::is_move_assignable_v<T>)
        = default;

        /** @brief 返回元素的可变引用。 */
        T &data() { return m_data; }
        /** @brief 返回元素的常量引用。 */
        const T &data() const { return m_data; }
    };

    /**
     * @brief 矩阵行。
     *
     * 固定长度 M 的一行元素，可通过 operator[] 按列访问。
     *
     * @tparam T 元素类型。
     * @tparam M 行长度（列数）。
     */
    template <typename T, size_t M>
    class MatrixRow
    {
    private:
        std::array<MatrixElement<T>, M> m_data;

    public:
        /** @brief 默认构造（要求 T 可默认构造）。 */
        MatrixRow()
            requires(std::is_default_constructible_v<T>)
        = default;
        /** @brief 拷贝构造（要求 T 可拷贝构造）。 */
        MatrixRow(const MatrixRow<T, M> &)
            requires(std::is_copy_constructible_v<T>)
        = default;
        /** @brief 移动构造（要求 T 可移动构造）。 */
        MatrixRow(MatrixRow<T, M> &&) noexcept
            requires(std::is_move_constructible_v<T>)
        = default;

        /**
         * @brief 从 std::array 构造（左值版本）。
         * @param arr 长度 M 的元素数组。
         */
        MatrixRow(const std::array<T, M> &arr)
            requires(std::is_copy_constructible_v<T>)
            : m_data(transform_array(arr, [](T val) { return MatrixElement<T>{val}; }))
        {
        }
        /**
         * @brief 从 std::array 构造（右值版本）。
         * @param arr 长度 M 的元素数组。
         */
        MatrixRow(std::array<T, M> &&arr)
            requires(std::is_move_constructible_v<T>)
            : m_data(transform_array(std::move(arr), [](T &val) {
                  return MatrixElement<T>{std::move(val)};
              }))
        {
        }

    public:
        /** @brief 拷贝赋值（要求 T 可拷贝赋值）。 */
        MatrixRow &operator=(const MatrixRow<T, M> &)
            requires(std::is_copy_assignable_v<T>)
        = default;
        /** @brief 移动赋值（要求 T 可移动赋值）。 */
        MatrixRow &operator=(MatrixRow<T, M> &&) noexcept
            requires(std::is_move_assignable_v<T>)
        = default;

        /**
         * @brief 按列下标访问元素。
         * @param idx 列下标，范围 [0, M)。
         * @return 元素的可变引用。
         */
        T &operator[](size_t idx) { return m_data[idx].data(); }
        /**
         * @brief 按列下标访问元素（常量版本）。
         * @param idx 列下标，范围 [0, M)。
         * @return 元素的常量引用。
         */
        const T &operator[](size_t idx) const { return m_data[idx].data(); }
    };

    /**
     * @brief 通用固定尺寸矩阵。
     *
     * N 行 × M 列的二维容器，通过 matrix[row][col] 访问元素。
     *
     * @tparam T 元素类型。
     * @tparam N 行数。
     * @tparam M 列数。
     */
    template <typename T, size_t N, size_t M>
    class Matrix
    {
    private:
        std::array<MatrixRow<T, M>, N> m_rows;

    public:
        /** @brief 默认构造（要求 T 可默认构造）。 */
        Matrix()
            requires(std::is_default_constructible_v<T>)
        = default;
        /** @brief 拷贝构造（要求 T 可拷贝构造）。 */
        Matrix(const Matrix<T, N, M> &)
            requires(std::is_copy_constructible_v<T>)
        = default;
        /** @brief 移动构造（要求 T 可移动构造）。 */
        Matrix(Matrix<T, N, M> &&) noexcept
            requires(std::is_move_constructible_v<T>)
        = default;

        /**
         * @brief 从嵌套 std::array 构造（左值版本）。
         * @param arr N×M 的元素数组。
         */
        Matrix(const std::array<std::array<T, M>, N> &arr)
            requires(std::is_copy_constructible_v<T>)
            : m_rows(transform_array(arr, [](const std::array<T, M> &row) {
                  return MatrixRow<T, M>{row};
              }))
        {
        }
        /**
         * @brief 从嵌套 std::array 构造（右值版本）。
         * @param arr N×M 的元素数组。
         */
        Matrix(std::array<std::array<T, M>, N> &&arr)
            requires(std::is_move_constructible_v<T>)
            : m_rows(transform_array(std::move(arr), [](std::array<T, M> &row) {
                  return MatrixRow<T, M>{std::move(row)};
              }))
        {
        }

    public:
        /** @brief 拷贝赋值（要求 T 可拷贝赋值）。 */
        Matrix &operator=(const Matrix<T, N, M> &)
            requires(std::is_copy_assignable_v<T>)
        = default;
        /** @brief 移动赋值（要求 T 可移动赋值）。 */
        Matrix &operator=(Matrix<T, N, M> &&) noexcept
            requires(std::is_move_assignable_v<T>)
        = default;

        /**
         * @brief 按行下标访问矩阵行。
         * @param idx 行下标，范围 [0, N)。
         * @return 矩阵行的可变引用。
         */
        MatrixRow<T, M> &operator[](size_t idx) { return m_rows[idx]; }
        /**
         * @brief 按行下标访问矩阵行（常量版本）。
         * @param idx 行下标，范围 [0, N)。
         * @return 矩阵行的常量引用。
         */
        const MatrixRow<T, M> &operator[](size_t idx) const { return m_rows[idx]; }
    };
}

#endif  // INCLUDE_TREELANG_CORE_MATRIX_HPP