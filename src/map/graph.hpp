#ifndef INCLUDE_TREELANG_MAP_GRAPH_HPP
#define INCLUDE_TREELANG_MAP_GRAPH_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <utility>
#include <vector>

#include "core/direction.hpp"
#include "core/marco.hpp"
#include "core/matrix.hpp"
#include "core/rng.hpp"
#include "map/room.hpp"

namespace treelang
{
    /**
     * @class Graph
     * @brief 单层 5×5 无向地图，按显式多阶段 pipeline 生成。
     *
     * pipeline 顺序（generate 依次调用）：
     * reset → build_tree → place_start → place_exit →
     * place_reward_function → add_loops → place_story → place_elites →
     * fill_enemies
     *
     * 特殊叶子房（Exit/Reward/Function）在补环前选定，补环时跳过触碰它们的边，
     * 从而保证生成后仍是叶子。
     *
     * 重要节点位置（start / exit / rewards / functions / stories / elites /
     * path）在生成过程中存入成员，供后续阶段与外部复用。
     */
    class Graph
    {
        static constexpr size_t Size = k_map_size;
        using Mat = Matrix<Room, Size, Size>;

        /** 补环边数下界。 */
        static constexpr int k_loop_min = 4;
        /** 补环边数上界。 */
        static constexpr int k_loop_max = 12;
        /** 每层奖励房数量。 */
        static constexpr int k_reward_count = 2;
        /** 每层功能房数量。 */
        static constexpr int k_function_count = 1;

    private:
        DEFINE_ATTRIBUTE(Mat, data)
        DEFINE_ATTRIBUTE(Point, start)
        DEFINE_ATTRIBUTE(Point, exit)
        DEFINE_ATTRIBUTE(std::vector<Point>, rewards)
        DEFINE_ATTRIBUTE(std::vector<Point>, functions)
        DEFINE_ATTRIBUTE(std::vector<Point>, stories)
        DEFINE_ATTRIBUTE(std::vector<Point>, elites)
        DEFINE_ATTRIBUTE(std::vector<Point>, path)

    public:
        DEFAULT_CONSTRUCTOR(Graph)

    public:
        static constexpr int idx(Point p) { return p.row * (int)Size + p.col; }

        static constexpr Point point_at(int i)
        {
            return make_point(i / (int)Size, i % (int)Size);
        }

        /** @brief p 是否落在 5×5 网格内。 */
        static bool in_range(Point p)
        {
            return p.row >= 0 && p.row < (int)Size && p.col >= 0 && p.col < (int)Size;
        }

        Room &at(size_t row, size_t col) { return data[row][col]; }
        const Room &at(size_t row, size_t col) const { return data[row][col]; }
        Room &at(Point p) { return at((size_t)p.row, (size_t)p.col); }
        const Room &at(Point p) const { return at((size_t)p.row, (size_t)p.col); }

    public:
        /** @brief 生成整层地图：按 pipeline 顺序调用各阶段。 */
        void generate(Rng &rng)
        {
            reset();
            build_tree(rng);
            place_start();
            place_exit();
            place_reward_function(rng);
            add_loops(rng);
            place_story(rng);
            place_elites();
            fill_enemies(rng);
        }

        /** @brief 清空地图：所有房间复位为 Empty 并切断连接，重要节点位置清空。 */
        void reset()
        {
            data.reset();
            for (size_t r = 0; r < Size; ++r)
                for (size_t c = 0; c < Size; ++c)
                    at(r, c).get_pos() = make_point((int)r, (int)c);
            start = make_point((int)k_center_row, (int)k_center_col);
            exit = make_point(-1, -1);
            rewards.clear();
            functions.clear();
            stories.clear();
            elites.clear();
            path.clear();
        }

        /** @brief 阶段一：自起点 randomized DFS 生长生成树（前提：地图已 reset）。 */
        void build_tree(Rng &rng)
        {
            std::array<bool, Size * Size> seen{};
            std::vector<Point> stack{start};
            seen[idx(start)] = true;
            while (!stack.empty())
            {
                auto cur = stack.back();
                stack.pop_back();
                auto dirs = k_all_directions;
                rng.shuffle(dirs.begin(), dirs.end());
                for (Direction d : dirs)
                {
                    auto nb = move_point(cur, d);
                    if (!in_range(nb) || seen[idx(nb)])
                        continue;
                    seen[idx(nb)] = true;
                    link(cur, nb);
                    stack.push_back(nb);
                }
            }
        }

        /**
         * @brief 阶段五：随机补 k ∈ [k_loop_min, k_loop_max] 条额外边制造回路；
         * 候选边不触碰特殊叶子房（Exit/Reward/Function），保证其保持叶子。
         */
        void add_loops(Rng &rng)
        {
            std::vector<std::pair<Point, Point>> candidates;
            for (size_t r = 0; r < Size; ++r)
                for (size_t c = 0; c < Size; ++c)
                {
                    auto a = make_point((int)r, (int)c);
                    for (auto d : {Direction::Right, Direction::Down})
                    {
                        auto b = move_point(a, d);
                        if (!in_range(b) || at(a).connected(d))
                            continue;
                        if (is_special_leaf(a) || is_special_leaf(b))
                            continue;
                        candidates.emplace_back(a, b);
                    }
                }
            rng.shuffle(candidates.begin(), candidates.end());
            auto k =
                std::min(rng.uniform_int(k_loop_min, k_loop_max), (int)candidates.size());
            for (int i = 0; i < k; ++i) link(candidates[i].first, candidates[i].second);
        }

        /** @brief 阶段二：中央房标记为 Start。 */
        void place_start()
        {
            start = make_point((int)k_center_row, (int)k_center_col);
            at(start).get_type() = RoomType::Start;
        }

        /** @brief 阶段三：离起点最远的叶子房作为 Exit。 */
        void place_exit()
        {
            auto dist = distances_from(start);
            auto best = make_point(-1, -1);
            int best_d = -1;
            for (size_t i = 0; i < Size * Size; ++i)
            {
                if (dist[i] <= 0 || !at(point_at((int)i)).is_leaf())
                    continue;
                if (dist[i] > best_d)
                {
                    best_d = dist[i];
                    best = point_at((int)i);
                }
            }
            if (best_d < 0)
                for (size_t i = 0; i < Size * Size; ++i)
                    if (dist[i] > 0 && dist[i] > best_d)
                    {
                        best_d = dist[i];
                        best = point_at((int)i);
                    }
            at(best).get_type() = RoomType::Exit;
            exit = best;
        }

        /**
         * @brief 阶段四：自生成树叶子房挑选 k_reward_count 个 Reward 与
         * k_function_count 个 Function，优先避开起点周围 4 格。
         */
        void place_reward_function(Rng &rng)
        {
            std::vector<Point> far_leaves;
            std::vector<Point> near_leaves;
            for (size_t i = 0; i < Size * Size; ++i)
            {
                auto p = point_at((int)i);
                const Room &room = at(p);
                if (room.get_type() != RoomType::Empty || p == start || p == exit)
                    continue;
                if (!room.is_leaf())
                    continue;
                (adjacent_to(p, start) ? near_leaves : far_leaves).push_back(p);
            }
            rng.shuffle(far_leaves.begin(), far_leaves.end());
            rng.shuffle(near_leaves.begin(), near_leaves.end());
            std::vector<Point> pool;
            pool.insert(
                pool.end(), std::make_move_iterator(far_leaves.begin()),
                std::make_move_iterator(far_leaves.end()));
            pool.insert(
                pool.end(), std::make_move_iterator(near_leaves.begin()),
                std::make_move_iterator(near_leaves.end()));
            for (int i = 0; i < (int)pool.size() && i < k_reward_count + k_function_count;
                 ++i)
            {
                RoomType t = i < k_reward_count ? RoomType::Reward : RoomType::Function;
                at(pool[i]).get_type() = t;
                (i < k_reward_count ? rewards : functions).push_back(pool[i]);
            }
        }

        /** @brief 阶段六：在起点→Exit 最短路内部节点放 1~2 个 Story。 */
        void place_story(Rng &rng)
        {
            path = shortest_path(start, exit);
            if (path.size() < 3)
                return;
            std::vector interior(path.begin() + 1, path.end() - 1);
            int want = 1 + (rng.chance(0.5) ? 1 : 0);
            want = std::min(want, (int)interior.size());
            rng.shuffle(interior.begin(), interior.end());
            for (int i = 0; i < want; ++i)
            {
                at(interior[i]).get_type() = RoomType::Story;
                stories.push_back(interior[i]);
            }
        }

        /** @brief 阶段七：所有未被占用的割点置为 Elite（守卫必经之路）。 */
        void place_elites()
        {
            for (auto p : articulation_points())
                if (at(p).get_type() == RoomType::Empty)
                {
                    at(p).get_type() = RoomType::Elite;
                    elites.push_back(p);
                }
        }

        /**
         * @brief 阶段八：剩余 Empty 房保留 1/3 作过路空房，其余转战斗房，
         * Enemy:Elite ≈ 4:1（割点精英计入 1/5）。
         */
        void fill_enemies(Rng &rng)
        {
            std::vector<Point> pool;
            for (size_t i = 0; i < Size * Size; ++i)
            {
                auto p = point_at((int)i);
                if (at(p).get_type() == RoomType::Empty)
                    pool.push_back(p);
            }
            int n = (int)pool.size();
            int combat = n - n / 3;
            int cut = (int)elites.size();
            int extra = combat >= 4 * cut ? (combat - 4 * cut) / 5 : 0;
            rng.shuffle(pool.begin(), pool.end());
            for (int i = 0; i < combat; ++i)
            {
                if (elites.size() < (size_t)(cut + extra))
                {
                    at(pool[i]).get_type() = RoomType::Elite;
                    elites.push_back(pool[i]);
                }
                else
                    at(pool[i]).get_type() = RoomType::Enemy;
            }
        }

    public:
        /** @brief p 的四向连通邻居。 */
        std::vector<Point> neighbors(Point p) const
        {
            std::vector<Point> result;
            for (auto d : k_all_directions)
            {
                if (!at(p).connected(d))
                    continue;
                Point nb = move_point(p, d);
                if (!in_range(nb))
                    continue;
                result.push_back(nb);
            }
            return result;
        }

        /** @brief a、b 是否直接连通。 */
        bool is_connected(Point a, Point b) const
        {
            if (!in_range(a) || !in_range(b))
                return false;
            Point d = b - a;
            if (std::abs(d.row) + std::abs(d.col) != 1)
                return false;
            Direction dir = direction_between(a, b);
            return at(a).connected(dir) && at(b).connected(opposite(dir));
        }

        int degree(Point p) const { return at(p).degree(); }
        bool is_leaf(Point p) const { return at(p).is_leaf(); }

        /** @brief a 到 b 的最短路径（BFS），返回 [a, ..., b]；不可达时为空。 */
        std::vector<Point> shortest_path(Point a, Point b) const
        {
            constexpr size_t N = Size * Size;
            std::array<int, N> prev{};
            prev.fill(-2);
            std::vector<Point> queue{a};
            prev[idx(a)] = -1;
            size_t head = 0;
            while (head < queue.size())
            {
                Point cur = queue[head++];
                if (cur == b)
                    break;
                for (Direction d : k_all_directions)
                {
                    if (!at(cur).connected(d))
                        continue;
                    Point nb = move_point(cur, d);
                    if (!in_range(nb) || prev[idx(nb)] != -2)
                        continue;
                    prev[idx(nb)] = idx(cur);
                    queue.push_back(nb);
                }
            }
            if (prev[idx(b)] == -2)
                return {};
            std::vector<Point> result;
            for (int cur = idx(b); cur != -1; cur = prev[cur])
                result.push_back(point_at(cur));
            std::reverse(result.begin(), result.end());
            return result;
        }

        /** @brief src 到各房的 BFS 距离（不可达为 -1）。 */
        std::array<int, Size * Size> distances_from(Point src) const
        {
            std::array<int, Size * Size> dist{};
            dist.fill(-1);
            std::vector<Point> queue{src};
            dist[idx(src)] = 0;
            size_t head = 0;
            while (head < queue.size())
            {
                Point cur = queue[head++];
                for (auto d : k_all_directions)
                {
                    if (!at(cur).connected(d))
                        continue;
                    Point nb = move_point(cur, d);
                    if (!in_range(nb) || dist[idx(nb)] != -1)
                        continue;
                    dist[idx(nb)] = dist[idx(cur)] + 1;
                    queue.push_back(nb);
                }
            }
            return dist;
        }

        /** @brief 当前图的全部割点（Tarjan）。 */
        std::vector<Point> articulation_points() const
        {
            constexpr auto N = Size * Size;
            std::array<int, N> tin{};
            tin.fill(-1);
            std::array<int, N> low{};
            std::array<bool, N> is_ap{};
            int timer = 0;
            auto dfs = [&](auto &self, int u, int parent) -> void
            {
                int children = 0;
                tin[u] = low[u] = timer++;
                Point pu = point_at(u);
                for (Direction d : k_all_directions)
                {
                    if (!at(pu).connected(d))
                        continue;
                    Point nb = move_point(pu, d);
                    if (!in_range(nb))
                        continue;
                    int v = idx(nb);
                    if (v == parent)
                        continue;
                    if (tin[v] != -1)
                    {
                        low[u] = std::min(low[u], tin[v]);
                        continue;
                    }
                    ++children;
                    self(self, v, u);
                    low[u] = std::min(low[u], low[v]);
                    if (parent != -1 && low[v] >= tin[u])
                        is_ap[u] = true;
                }
                if (parent == -1 && children >= 2)
                    is_ap[u] = true;
            };
            for (int i = 0; i < (int)N; ++i)
                if (tin[i] == -1)
                    dfs(dfs, i, -1);
            std::vector<Point> result;
            for (size_t i = 0; i < N; ++i)
                if (is_ap[i])
                    result.push_back(point_at((int)i));
            return result;
        }

        /** @brief 全部 25 房是否互相可达。 */
        bool is_fully_connected() const
        {
            auto dist = distances_from(start);
            for (int d : dist)
                if (d < 0)
                    return false;
            return true;
        }

        /** @brief 图中边数。 */
        int edge_count() const
        {
            int sum = 0;
            for (size_t i = 0; i < Size * Size; ++i) sum += at(point_at((int)i)).degree();
            return sum / 2;
        }

    private:
        /** @brief 为正交相邻的 a、b 建立双向连接。 */
        void link(Point a, Point b)
        {
            auto d = direction_between(a, b);
            at(a).connect(d);
            at(b).connect(opposite(d));
        }

        /** @brief a、b 是否正交相邻（曼哈顿距离 1）。 */
        static bool adjacent_to(Point a, Point b)
        {
            auto d = b - a;
            return std::abs(d.row) + std::abs(d.col) == 1;
        }

        /** @brief 是否为受保护的特殊叶子房（Exit/Reward/Function）。 */
        bool is_special_leaf(Point p) const
        {
            auto t = at(p).get_type();
            return t == RoomType::Exit || t == RoomType::Reward ||
                   t == RoomType::Function;
        }
    };
}

#endif  // INCLUDE_TREELANG_MAP_GRAPH_HPP
