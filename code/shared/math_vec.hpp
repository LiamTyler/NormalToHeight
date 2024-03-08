#pragma once

#define GLM_FORCE_RADIANS
#include "glm/geometric.hpp"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "shared/math_base.hpp"

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;

using i8vec2  = glm::vec<2, int8_t>;
using i16vec2 = glm::vec<2, int16_t>;
using ivec2   = glm::vec<2, int32_t>;
using i8vec3  = glm::vec<3, int8_t>;
using i16vec3 = glm::vec<3, int16_t>;
using ivec3   = glm::vec<3, int32_t>;
using i8vec4  = glm::vec<4, int8_t>;
using i16vec4 = glm::vec<4, int16_t>;
using ivec4   = glm::vec<4, int32_t>;

using u8vec2  = glm::vec<2, uint8_t>;
using u16vec2 = glm::vec<2, uint16_t>;
using uvec2   = glm::vec<2, uint32_t>;
using u8vec3  = glm::vec<3, uint8_t>;
using u16vec3 = glm::vec<3, uint16_t>;
using uvec3   = glm::vec<3, uint32_t>;
using u8vec4  = glm::vec<4, uint8_t>;
using u16vec4 = glm::vec<4, uint16_t>;
using uvec4   = glm::vec<4, uint32_t>;

inline float Length( const vec2& v ) { return glm::length( v ); }
inline float Length( const vec3& v ) { return glm::length( v ); }
inline float Length( const vec4& v ) { return glm::length( v ); }

inline float Dot( const vec2& a, const vec2& b ) { return glm::dot( a, b ); }
inline float Dot( const vec3& a, const vec3& b ) { return glm::dot( a, b ); }
inline float Dot( const vec4& a, const vec4& b ) { return glm::dot( a, b ); }

inline float Cross( const vec2& a, const vec2& b ) { return a.x * b.y - a.y * b.x; }
inline vec3 Cross( const vec3& a, const vec3& b ) { return glm::cross( a, b ); }

inline vec2 Normalize( const vec2& v ) { return glm::normalize( v ); }
inline vec3 Normalize( const vec3& v ) { return glm::normalize( v ); }
inline vec4 Normalize( const vec4& v ) { return glm::normalize( v ); }

inline vec2 Saturate( const vec2& v ) { return glm::clamp( v, 0.0f, 1.0f ); }
inline vec3 Saturate( const vec3& v ) { return glm::clamp( v, 0.0f, 1.0f ); }
inline vec4 Saturate( const vec4& v ) { return glm::clamp( v, 0.0f, 1.0f ); }

inline vec2 DegToRad( const vec2& v ) { return v * ( PI / 180.0f ); }
inline vec3 DegToRad( const vec3& v ) { return v * ( PI / 180.0f ); }
inline vec4 DegToRad( const vec4& v ) { return v * ( PI / 180.0f ); }

inline vec2 RadToDeg( const vec2& v ) { return v * ( PI / 180.0f ); }
inline vec3 RadToDeg( const vec3& v ) { return v * ( PI / 180.0f ); }
inline vec4 RadToDeg( const vec4& v ) { return v * ( PI / 180.0f ); }

inline vec2 Min( const vec2& a, const vec2& b ) { return glm::min( a, b ); }
inline vec3 Min( const vec3& a, const vec3& b ) { return glm::min( a, b ); }
inline vec4 Min( const vec4& a, const vec4& b ) { return glm::min( a, b ); }

inline vec2 Max( const vec2& a, const vec2& b ) { return glm::max( a, b ); }
inline vec3 Max( const vec3& a, const vec3& b ) { return glm::max( a, b ); }
inline vec4 Max( const vec4& a, const vec4& b ) { return glm::max( a, b ); }

inline vec2 Lerp( const vec2& a, const vec2& b, float t ) { return glm::mix( a, b, t ); }
inline vec3 Lerp( const vec3& a, const vec3& b, float t ) { return glm::mix( a, b, t ); }
inline vec4 Lerp( const vec4& a, const vec4& b, float t ) { return glm::mix( a, b, t ); }