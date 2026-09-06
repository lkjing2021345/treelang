#include <cstdlib>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

#include <doctest/doctest.h>

#include "core/instance.hpp"
#include "core/rng.hpp"
#include "core/types.hpp"
#include "map/event.hpp"
#include "map/graph.hpp"
#include "map/room.hpp"

using treelang::Direction;
using treelang::DirectionCollection;
using treelang::Graph;
using treelang::Point;
using treelang::make_point;
using treelang::Room;
using treelang::RoomType;
using treelang::Rng;

namespace
{
    int count_type(const Graph &g, RoomType t)
    {
        int n = 0;
        for (size_t r = 0; r < treelang::k_map_size; ++r)
            for (size_t c = 0; c < treelang::k_map_size; ++c)
                if (g.at(r, c).get_type() == t) ++n;
        return n;
    }

    bool same_floor(const Graph &a, const Graph &b)
    {
        for (size_t r = 0; r < treelang::k_map_size; ++r)
            for (size_t c = 0; c < treelang::k_map_size; ++c)
            {
                if (a.at(r, c).get_type() != b.at(r, c).get_type()) return false;
                if (a.at(r, c).get_conn() != b.at(r, c).get_conn()) return false;
            }
        return true;
    }

    bool on_path(const std::vector<Point> &path, Point p)
    {
        for (const Point &q : path)
            if (q == p) return true;
        return false;
    }

    int manhattan(Point a, Point b)
    {
        Point d = b - a;
        return std::abs(d.row) + std::abs(d.col);
    }
}

TEST_CASE("room: connect / degree / leaf helpers")
{
    Room r{RoomType::Empty, make_point(1, 1)};
    CHECK_EQ(r.degree(), 0);
    CHECK_FALSE(r.is_leaf());

    r.connect(Direction::Up);
    CHECK(r.is_leaf());
    CHECK(r.connected(Direction::Up));
    CHECK_FALSE(r.connected(Direction::Down));
    CHECK_FALSE(r.connected(Direction::Left));

    r.connect(Direction::Left);
    CHECK_EQ(r.degree(), 2);
    CHECK_FALSE(r.is_leaf());
    CHECK(r.connected(static_cast<DirectionCollection>(Direction::Up) |
                     static_cast<DirectionCollection>(Direction::Left)));
}

TEST_CASE("map: build_tree spans every room with exactly 24 edges")
{
    Graph g;
    g.reset();
    Rng rng{5};
    g.build_tree(rng);

    CHECK_EQ(g.edge_count(), 24);

    std::set<Point> seen{g.get_start()};
    std::vector<Point> queue{g.get_start()};
    for (size_t head = 0; head < queue.size(); ++head)
        for (const Point &nb : g.neighbors(queue[head]))
            if (seen.insert(nb).second) queue.push_back(nb);
    CHECK_EQ(seen.size(), treelang::k_map_size * treelang::k_map_size);
}

TEST_CASE("map: structural invariants hold across seeds")
{
    for (int seed = 1; seed <= 32; ++seed)
    {
        Graph g;
        Rng rng(seed);
        g.generate(rng);

        CHECK(g.is_fully_connected());

        int loops = g.edge_count() - 24;
        CHECK(loops >= 4);
        CHECK(loops <= 12);

        CHECK_EQ(count_type(g, RoomType::Start), 1);
        CHECK_EQ(count_type(g, RoomType::Exit), 1);
        CHECK_EQ(count_type(g, RoomType::Reward), 2);
        CHECK_EQ(count_type(g, RoomType::Function), 1);
        int story = count_type(g, RoomType::Story);
        CHECK(story >= 1);
        CHECK(story <= 2);

        CHECK(g.at(treelang::k_center_row, treelang::k_center_col).get_type() ==
              RoomType::Start);
        CHECK(g.get_start() == make_point(2, 2));

        CHECK_EQ(g.at(g.get_exit()).degree(), 1);
        for (const Point &p : g.get_rewards()) CHECK_EQ(g.at(p).degree(), 1);
        for (const Point &p : g.get_functions()) CHECK_EQ(g.at(p).degree(), 1);

        for (const Point &p : g.get_rewards()) CHECK(manhattan(p, g.get_start()) > 1);
        for (const Point &p : g.get_functions()) CHECK(manhattan(p, g.get_start()) > 1);

        for (const Point &p : g.get_stories()) CHECK(on_path(g.get_path(), p));

        for (const Point &p : g.articulation_points())
        {
            RoomType t = g.at(p).get_type();
            CHECK(t != RoomType::Enemy);
            CHECK(t != RoomType::Empty);
        }
    }
}

TEST_CASE("map: connections are bidirectional")
{
    Graph g;
    Rng rng{11};
    g.generate(rng);

    for (int r = 0; r < (int)treelang::k_map_size; ++r)
        for (int c = 0; c < (int)treelang::k_map_size; ++c)
        {
            Point p = make_point(r, c);
            for (Direction d : treelang::k_all_directions)
            {
                Point nb = treelang::move_point(p, d);
                bool here = g.at(p).connected(d);
                if (Graph::in_range(nb))
                    CHECK(here == g.at(nb).connected(treelang::opposite(d)));
                else
                    CHECK_FALSE(here);
            }
        }
}

TEST_CASE("map: enemy/elite ratio stays near 4:1")
{
    Graph g;
    Rng rng{7};
    g.generate(rng);

    int enemy = count_type(g, RoomType::Enemy);
    int elite = count_type(g, RoomType::Elite);
    CHECK(elite >= 1);
    CHECK(enemy >= 1);
    CHECK(enemy <= 4 * elite + 4);
}

TEST_CASE("map: same seed reproduces the identical floor")
{
    for (int seed : {1, 42, 99, 2025})
    {
        Graph a, b;
        Rng ra(seed);
        Rng rb(seed);
        a.generate(ra);
        b.generate(rb);
        CHECK(same_floor(a, b));
    }
}

TEST_CASE("map: room events flow through the event bus")
{
    auto &bus = treelang::EventBusInstance::instance().data();
    bool entered = false;
    bus.subscribe<treelang::RoomEnteredEvent>(
        [&](treelang::RoomEnteredEvent *e)
        {
            entered = true;
            CHECK(e->type == RoomType::Elite);
        });

    auto event = std::make_shared<treelang::RoomEnteredEvent>();
    event->pos = make_point(1, 2);
    event->type = RoomType::Elite;
    bus.publish(event);

    CHECK(entered);
}
