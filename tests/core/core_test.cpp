#include <doctest/doctest.h>

#include "core/element.hpp"
#include "core/rng.hpp"
#include "core/types.hpp"

using treelang::Element;
using treelang::ElementAttr;
using treelang::Rng;
using treelang::RoomType;

TEST_CASE("core: element id/name round-trip")
{
    CHECK(treelang::element_id(Element::Fire) == "fire");
    CHECK(treelang::element_id(Element::Water) == "water");
    CHECK(treelang::element_id(Element::Metal) == "metal");

    CHECK(treelang::element_name(Element::Fire) == "火");
    CHECK(treelang::element_name(Element::Water) == "水");
    CHECK(treelang::element_name(Element::Metal) == "金");

    CHECK(treelang::element_from_id("fire") == Element::Fire);
    CHECK(treelang::element_from_id("water") == Element::Water);
    CHECK(treelang::element_from_id("metal") == Element::Metal);
    CHECK(!treelang::element_from_id("earth").has_value());
}

TEST_CASE("core: room type / constants / element attr")
{
    CHECK(treelang::kMapSize == 5);
    CHECK(treelang::kSanMin == 0);
    CHECK(treelang::kSanMax == 100);
    CHECK(treelang::kAttributeCap == 20);

    ElementAttr attr{Element::Fire, 3};
    CHECK(attr.element == Element::Fire);
    CHECK(attr.level == 3);

    CHECK(static_cast<int>(RoomType::Start) == 0);
    CHECK(static_cast<int>(RoomType::Story) == 4);
}

TEST_CASE("core: rng is deterministic and in-range")
{
    Rng a{42};
    Rng b{42};
    for (int i = 0; i < 100; ++i) CHECK(a() == b());

    for (int i = 0; i < 1000; ++i)
    {
        int v = a.uniform_int(1, 6);
        CHECK(v >= 1);
        CHECK(v <= 6);

        double r = a.uniform_real(0.0, 1.0);
        CHECK(r >= 0.0);
        CHECK(r < 1.0);
    }

    CHECK(a.uniform_int(5, 5) == 5);
    CHECK(a.chance(1.0));
    CHECK(!a.chance(0.0));
}