#include <array>
#include <memory>
#include <type_traits>

#include <doctest/doctest.h>

#include "core/matrix.hpp"

using treelang::Matrix;
using treelang::MatrixRow;

TEST_CASE("matrix: copyable")
{
    Matrix<int, 3, 4> m;
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 4; ++j) m[i][j] = static_cast<int>(i * 4 + j);

    const auto &cm = m;
    CHECK(cm[2][3] == 11);

    Matrix<int, 3, 4> c{m};
    CHECK(c[1][2] == 6);
    c = m;
    CHECK(c[0][0] == 0);

    Matrix<int, 3, 4> mv = std::move(c);
    CHECK(mv[1][2] == 6);
    m = std::move(mv);
    CHECK(m[1][2] == 6);

    std::array<std::array<int, 4>, 3> raw = {
        {{0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 10, 11}}};
    Matrix<int, 3, 4> from_copy{raw};
    CHECK(from_copy[2][3] == 11);

    auto raw2 = std::array<std::array<int, 4>, 3>{{{0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 10, 11}}};
    Matrix<int, 3, 4> from_move{std::move(raw2)};
    CHECK(from_move[2][0] == 8);

    MatrixRow<int, 4> row{raw[1]};
    CHECK(row[0] == 4);
    CHECK(row[3] == 7);
    const MatrixRow<int, 4> &crow = row;
    CHECK(crow[1] == 5);

    static_assert(std::is_default_constructible_v<Matrix<int, 3, 4>>);
    static_assert(std::is_copy_constructible_v<Matrix<int, 3, 4>>);
    static_assert(std::is_move_constructible_v<Matrix<int, 3, 4>>);
    static_assert(std::is_copy_assignable_v<Matrix<int, 3, 4>>);
    static_assert(std::is_move_assignable_v<Matrix<int, 3, 4>>);
}

TEST_CASE("matrix: double")
{
    Matrix<double, 2, 2> m;
    m[0][0] = 1.5;
    const MatrixRow<double, 2> &r = m[1];
    (void)r;
    CHECK(m[0][0] == 1.5);
}

TEST_CASE("matrix: move-only")
{
    Matrix<std::unique_ptr<int>, 2, 2> m;
    for (size_t i = 0; i < 4; ++i)
        m[i / 2][i % 2] = std::make_unique<int>(static_cast<int>(i));

    auto src = std::array<std::array<std::unique_ptr<int>, 2>, 2>{
        {std::array<std::unique_ptr<int>, 2>{std::make_unique<int>(10), std::make_unique<int>(11)},
         std::array<std::unique_ptr<int>, 2>{std::make_unique<int>(12), std::make_unique<int>(13)}}};
    Matrix<std::unique_ptr<int>, 2, 2> mv{std::move(src)};
    CHECK(*mv[0][0] == 10);
    CHECK(*mv[1][1] == 13);

    Matrix<std::unique_ptr<int>, 2, 2> dst;
    dst = std::move(mv);
    CHECK(*dst[1][1] == 13);

    static_assert(!std::is_copy_constructible_v<Matrix<std::unique_ptr<int>, 2, 2>>);
    static_assert(!std::is_copy_assignable_v<Matrix<std::unique_ptr<int>, 2, 2>>);
    static_assert(std::is_move_constructible_v<Matrix<std::unique_ptr<int>, 2, 2>>);
    static_assert(std::is_move_assignable_v<Matrix<std::unique_ptr<int>, 2, 2>>);
}