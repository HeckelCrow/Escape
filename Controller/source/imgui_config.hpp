#pragma once

#include "alias.hpp"
#define IM_VEC2_CLASS_EXTRA                                                    \
    constexpr ImVec2(const Vec2f& f) : x(f.x), y(f.y)                          \
    {}                                                                         \
    operator Vec2f() const                                                     \
    {                                                                          \
        return Vec2f(x, y);                                                    \
    }

#define IM_VEC4_CLASS_EXTRA                                                    \
    constexpr ImVec4(const Vec4f& f) : x(f.x), y(f.y), z(f.z), w(f.w)          \
    {}                                                                         \
    operator Vec4f() const                                                     \
    {                                                                          \
        return Vec4f(x, y, z, w);                                              \
    }