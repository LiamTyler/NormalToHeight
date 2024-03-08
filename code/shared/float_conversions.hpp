#pragma once

#include "shared/math_vec.hpp"
#include <algorithm>

using float16 = uint16_t;

constexpr uint16_t FP16_ZERO = 0x0000;
constexpr uint16_t FP16_ONE  = 0x3C00;

// implementation from tinyexr library
union FP32
{
    uint32_t u;
    float f;
    struct
    {
        uint32_t Mantissa : 23;
        uint32_t Exponent : 8;
        uint32_t Sign : 1;
    };
};

union FP16
{
    uint16_t u;
    struct
    {
        uint32_t Mantissa : 10;
        uint32_t Exponent : 5;
        uint32_t Sign : 1;
    };
};

// Original ISPC reference version; this always rounds ties up.
inline uint16_t Float32ToFloat16( float f32 )
{
    FP32 f;
    f.f    = f32;
    FP16 o = { 0 };

    // Based on ISPC reference code (with minor modifications)
    if ( f.Exponent == 0 ) // Signed zero/denormal (which will underflow)
    {
        o.Exponent = 0;
    }
    else if ( f.Exponent == 255 ) // Inf or NaN (all exponent bits set)
    {
        o.Exponent = 31;
        o.Mantissa = f.Mantissa ? 0x200 : 0; // NaN->qNaN and Inf->Inf
    }
    else // Normalized number
    {
        // Exponent unbias the single, then bias the halfp
        int newexp = f.Exponent - 127 + 15;
        if ( newexp >= 31 ) // Overflow, return signed infinity
        {
            o.Exponent = 31;
        }
        else if ( newexp <= 0 ) // Underflow
        {
            if ( ( 14 - newexp ) <= 24 ) // Mantissa might be non-zero
            {
                uint32_t mant = f.Mantissa | 0x800000; // Hidden 1 bit
                o.Mantissa    = mant >> ( 14 - newexp );
                if ( ( mant >> ( 13 - newexp ) ) & 1 ) // Check for rounding
                {
                    o.u++; // Round, might overflow into exp bit, but this is OK
                }
            }
        }
        else
        {
            o.Exponent = newexp;
            o.Mantissa = f.Mantissa >> 13;
            if ( f.Mantissa & 0x1000 ) // Check for rounding
            {
                o.u++; // Round, might overflow to inf, this is OK
            }
        }
    }

    o.Sign = f.Sign;
    return o.u;
}

inline u16vec4 Float32ToFloat16( vec4 v )
{
    return { Float32ToFloat16( v.x ), Float32ToFloat16( v.y ), Float32ToFloat16( v.z ), Float32ToFloat16( v.w ) };
}

inline uint32_t Float32ToFloat16( float x, float y )
{
    uint32_t px = Float32ToFloat16( x );
    uint32_t py = Float32ToFloat16( y );
    return px | ( py << 16 );
}

inline float Float16ToFloat32( uint16_t f16 )
{
    static const FP32 magic               = { 113 << 23 };
    static const unsigned int shifted_exp = 0x7c00 << 13; // exponent mask after shift
    FP32 o;
    FP16 h;
    h.u = f16;

    o.u               = ( h.u & 0x7fffU ) << 13U; // exponent/mantissa bits
    unsigned int exp_ = shifted_exp & o.u;        // just the exponent
    o.u += ( 127 - 15 ) << 23;                    // exponent adjust

    // handle exponent special cases
    if ( exp_ == shifted_exp ) // Inf/NaN?
    {
        o.u += ( 128 - 16 ) << 23; // extra exp adjust
    }
    else if ( exp_ == 0 ) // Zero/Denormal?
    {
        o.u += 1 << 23; // extra exp adjust
        o.f -= magic.f; // renormalize
    }

    o.u |= ( h.u & 0x8000U ) << 16U; // sign bit
    return o.f;
}

inline vec4 Float16ToFloat32( u16vec4 v )
{
    return { Float16ToFloat32( v.x ), Float16ToFloat32( v.y ), Float16ToFloat32( v.z ), Float16ToFloat32( v.w ) };
}

inline constexpr uint8_t UNormFloatToByte( float x ) { return static_cast<uint8_t>( 255.0f * x + 0.5f ); }

inline u8vec4 UNormFloatToByte( vec4 v )
{
    return { UNormFloatToByte( v.x ), UNormFloatToByte( v.y ), UNormFloatToByte( v.z ), v.w };
}

inline constexpr float UNormByteToFloat( uint8_t x ) { return x / 255.0f; }

inline vec4 UNormByteToFloat( u8vec4 v )
{
    return { UNormByteToFloat( v.x ), UNormByteToFloat( v.y ), UNormByteToFloat( v.z ), v.w };
}

inline constexpr float UNorm16ToFloat( uint16_t x ) { return float( x ) / 65535.0f; }

inline constexpr uint16_t FloatToUNorm16( float x ) { return static_cast<uint16_t>( 65535.0f * x + 0.5f ); }
