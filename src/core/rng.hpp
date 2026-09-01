/**
 * @file rng.hpp
 * @brief 可注入种子的随机数发生器。
 */

#ifndef INCLUDE_TREELANG_CORE_RNG_HPP
#define INCLUDE_TREELANG_CORE_RNG_HPP

#include <algorithm>
#include <cstdint>
#include <random>

namespace treelang
{
    /**
     * @class Rng
     * @brief 可注入种子的随机数发生器。
     *
     * 地图生成、奖励品质、随机事件倾向等统一经由本类取随机数；
     * 测试中注入固定种子即可获得确定性输出。
     */
    class Rng
    {
    public:
        /** @brief 底层引擎类型 */
        using Engine = std::mt19937;
        /** @brief 结果值类型 */
        using result_type = Engine::result_type;

        /**
         * @brief 构造随机数发生器并注入种子。
         * @param seed 随机种子；相同种子产生相同随机序列。
         */
        explicit Rng(result_type seed)
            : m_engine(seed)
        {
        }

        /** @brief 返回下一个原始随机值。 */
        result_type operator()() { return m_engine(); }

        /** @brief 引擎可能产生的最小值。 */
        static constexpr result_type min() { return Engine::min(); }

        /** @brief 引擎可能产生的最大值。 */
        static constexpr result_type max() { return Engine::max(); }

        /**
         * @brief 在闭区间 [lo, hi] 内产生均匀随机整数。
         * @param lo 区间下界。
         * @param hi 区间上界。
         * @return [lo, hi] 内的整数；当 lo == hi 时返回该值。
         */
        int uniform_int(int lo, int hi)
        {
            if (lo >= hi) return lo;
            std::uniform_int_distribution<int> dist(lo, hi);
            return dist(m_engine);
        }

        /**
         * @brief 在半开区间 [lo, hi) 内产生均匀随机浮点。
         * @param lo 区间下界（含）。
         * @param hi 区间上界（不含）。
         * @return [lo, hi) 内的浮点数。
         */
        double uniform_real(double lo, double hi)
        {
            std::uniform_real_distribution<double> dist(lo, hi);
            return dist(m_engine);
        }

        /**
         * @brief 以给定概率命中。
         * @param p 命中概率，范围 [0, 1]。
         * @return 命中返回 true，否则返回 false。
         */
        bool chance(double p) { return uniform_real(0.0, 1.0) < p; }

        /**
         * @brief 打乱序列（洗牌）。
         * @tparam RandomIt 随机访问迭代器类型。
         * @param first 序列起始迭代器。
         * @param last 序列终止迭代器（不含）。
         */
        template <typename RandomIt>
        void shuffle(RandomIt first, RandomIt last)
        {
            std::shuffle(first, last, m_engine);
        }

    private:
        Engine m_engine; /**< 底层 Mersenne Twister 引擎 */
    };
}

#endif  // INCLUDE_TREELANG_CORE_RNG_HPP