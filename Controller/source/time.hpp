#pragma once
#include <chrono>
#include <fmt/format.h>
using Clock     = std::chrono::high_resolution_clock;
using Timepoint = std::chrono::time_point<Clock>;
using Duration  = std::chrono::microseconds;

constexpr Duration
Microseconds(s64 t)
{
    return std::chrono::microseconds(t);
}

constexpr Duration
Milliseconds(s64 t)
{
    return std::chrono::milliseconds(t);
}

constexpr Duration
Seconds(s64 t)
{
    return std::chrono::seconds(t);
}

constexpr Duration
Minutes(s64 t)
{
    return std::chrono::minutes(t);
}

template<typename T, typename R>
Str
DurationToString(std::chrono::duration<T, R> d)
{
    u64 ms  = std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
    Str str = fmt::format("{:02}:{:02}:{:02}.{:03}", ms / 3'600'000,
                          ms / 60000 % 60, ms / 1000 % 60, ms % 1000);
    return str;
}
