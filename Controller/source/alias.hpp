#pragma once
#include <stdint.h>
#include <limits>
#include <filesystem>
#include <string>
#define GLM_FORCE_XYZW_ONLY
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

#define sconst static constexpr
#define Breakpoint __debugbreak

#define StaticArraySize(arr) (sizeof(arr) / sizeof((arr)[0]))

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using f32 = float;
using f64 = double;

constexpr u8  U8_MAX  = UINT8_MAX;
constexpr u16 U16_MAX = UINT16_MAX;
constexpr u32 U32_MAX = UINT32_MAX;
constexpr u64 U64_MAX = UINT64_MAX;

constexpr s8  S8_MIN  = INT8_MIN;
constexpr s16 S16_MIN = INT16_MIN;
constexpr s32 S32_MIN = INT32_MIN;
constexpr s64 S64_MIN = INT64_MIN;

constexpr s8  S8_MAX  = INT8_MAX;
constexpr s16 S16_MAX = INT16_MAX;
constexpr s32 S32_MAX = INT32_MAX;
constexpr s64 S64_MAX = INT64_MAX;

constexpr f32 F32_INFINITY = std::numeric_limits<f32>::infinity();
constexpr f64 F64_INFINITY = std::numeric_limits<f64>::infinity();

template<typename T>
using Vec2  = glm::tvec2<T>;
using Vec2u = glm::tvec2<u32>;
using Vec2s = glm::tvec2<s32>;
using Vec2f = glm::tvec2<f32>;
template<typename T>
using Vec3  = glm::tvec3<T>;
using Vec3u = glm::tvec3<u32>;
using Vec3s = glm::tvec3<s32>;
using Vec3f = glm::tvec3<f32>;
template<typename T>
using Vec4  = glm::tvec4<T>;
using Vec4u = glm::tvec4<u32>;
using Vec4s = glm::tvec4<s32>;
using Vec4f = glm::tvec4<f32>;

using Mat3 = glm::mat3x3;
using Mat4 = glm::mat4x4;

using Quaternion = glm::quat;

using Str    = std::string;
using WStr   = std::basic_string<wchar_t>;
using StrPtr = std::string_view;

using Path = std::filesystem::path;
