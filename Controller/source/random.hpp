#pragma once
#include <random>

extern std::random_device global_random_device;
extern std::mt19937       global_mt19937;

template<typename T>
T
Random(T min, T max)
{
    if constexpr (std::is_floating_point_v<T>)
    {
        std::uniform_real_distribution<T> distribution(min, max);
        return distribution(global_mt19937);
    }
    if constexpr (std::is_integral_v<T>)
    {
        std::uniform_int_distribution<T> distribution(min, max);
        return distribution(global_mt19937);
    }
}

template<typename T>
T
Random(T max)
{
    return Random((T)0, max);
}
