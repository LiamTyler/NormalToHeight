#pragma once

#include <algorithm>

#define PI 3.14159265358979323846f
#define PI_D 3.14159265358979323846

inline constexpr float Saturate( const float x ) { return std::max( 0.0f, std::max( 0.0f, x ) ); }

inline constexpr float DegToRad( const float x ) { return x * ( PI / 180.0f ); }

inline constexpr float RadToDeg( const float x ) { return x * ( 180.0f / PI ); }

template <typename T>
constexpr T Min( const T& a, const T& b )
{
    return std::min( a, b );
}

template <typename T>
constexpr T Max( const T& a, const T& b )
{
    return std::max( a, b );
}