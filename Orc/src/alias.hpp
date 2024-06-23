#pragma once

#define STRINGIFY(s) STRINGIFY1(s)
#define STRINGIFY1(s) #s

#include <stdint.h>
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;

using f32 = float;

#include <limits>
constexpr s16 S16_MAX = std::numeric_limits<s16>::max();
constexpr s16 S16_MIN = std::numeric_limits<s16>::min();

constexpr s32 S32_MAX = std::numeric_limits<s32>::max();
constexpr s32 S32_MIN = std::numeric_limits<s32>::min();

constexpr u32 U32_MAX = std::numeric_limits<u32>::max();
constexpr u16 U16_MAX = std::numeric_limits<u16>::max();