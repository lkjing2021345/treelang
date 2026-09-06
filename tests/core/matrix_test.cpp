#include <array>
#include <memory>
#include <string>
#include <type_traits>

#include <doctest/doctest.h>

#include "core/matrix.hpp"

using treelang::Matrix;
using treelang::MatrixRow;

template <typename MatrixT>
concept HasDefaultReset = requires(MatrixT &m) { m.reset(); };

template <typename MatrixT, typename Element>
concept HasValueReset = requires(MatrixT &m, const Element &v) { m.reset(v); };

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

TEST_CASE("matrix: reset")
{
    Matrix<std::string, 2, 3> m;
    m[0][0] = "a";
    m[1][2] = "b";
    m.reset("x");
    for (size_t i = 0; i < 2; ++i)
        for (size_t j = 0; j < 3; ++j) CHECK(m[i][j] == "x");

    m.reset();
    for (size_t i = 0; i < 2; ++i)
        for (size_t j = 0; j < 3; ++j) CHECK(m[i][j].empty());

    MatrixRow<std::string, 3> row;
    row[1] = "y";
    row.reset("z");
    CHECK(row[0] == "z");
    CHECK(row[1] == "z");
    CHECK(row[2] == "z");
    row.reset();
    CHECK(row[0].empty());
    CHECK(row[1].empty());
    CHECK(row[2].empty());
}

TEST_CASE("matrix: reset move-only")
{
    Matrix<std::unique_ptr<int>, 2, 2> m;
    m[0][0] = std::make_unique<int>(1);
    m[1][1] = std::make_unique<int>(2);
    m.reset();
    for (size_t i = 0; i < 2; ++i)
        for (size_t j = 0; j < 2; ++j) CHECK(m[i][j] == nullptr);

    static_assert(HasDefaultReset<Matrix<int, 2, 2>>);
    static_assert(HasValueReset<Matrix<int, 2, 2>, int>);
    static_assert(HasDefaultReset<Matrix<std::unique_ptr<int>, 2, 2>>);
    static_assert(!HasValueReset<Matrix<std::unique_ptr<int>, 2, 2>, std::unique_ptr<int>>);
    static_assert(HasDefaultReset<MatrixRow<int, 3>>);
    static_assert(HasValueReset<MatrixRow<int, 3>, int>);
    static_assert(HasDefaultReset<MatrixRow<std::unique_ptr<int>, 3>>);
    static_assert(!HasValueReset<MatrixRow<std::unique_ptr<int>, 3>, std::unique_ptr<int>>);
}