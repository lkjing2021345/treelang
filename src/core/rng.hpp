#ifndef INCLUDE_TREELANG_CORE_RNG_HPP
#define INCLUDE_TREELANG_CORE_RNG_HPP

#include <algorithm>
#include <random>

namespace treelang
{
    class Rng
    {
    public:
        using Engine = std::mt19937;
        using result_type = Engine::result_type;

        explicit Rng(result_type seed) : m_engine(seed) {}

        result_type operator()() { return m_engine(); }
        static constexpr result_type min() { return Engine::min(); }
        static constexpr result_type max() { return Engine::max(); }

        int uniform_int(int lo, int hi)
        {
            if (lo >= hi)
                return lo;
            std::uniform_int_distribution<int> dist(lo, hi);
            return dist(m_engine);
        }

        double uniform_real(double lo, double hi)
        {
            std::uniform_real_distribution<double> dist(lo, hi);
            return dist(m_engine);
        }

        bool chance(double p) { return uniform_real(0.0, 1.0) < p; }

        template <typename RandomIt>
        void shuffle(RandomIt first, RandomIt last)
        {
            std::shuffle(first, last, m_engine);
        }

    private:
        Engine m_engine;
    };
}

#endif  // INCLUDE_TREELANG_CORE_RNG_HPP