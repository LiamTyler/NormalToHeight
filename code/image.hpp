#pragma once

#include "shared/core_defines.hpp"
#include "shared/float_conversions.hpp"
#include "shared/math_vec.hpp"
#include <memory>
#include <string>
#include <vector>

enum class ImageLoadFlags : uint32_t
{
    DEFAULT         = 0,
    FLIP_VERTICALLY = ( 1u << 0 ),
};
PG_DEFINE_ENUM_OPS( ImageLoadFlags );

enum class ImageSaveFlags : uint32_t
{
    DEFAULT               = 0,
    KEEP_FLOATS_AS_32_BIT = ( 1u << 0 ), // will convert f32 to fp16 by default if applicable, like when saving EXRs
};
PG_DEFINE_ENUM_OPS( ImageSaveFlags );

enum class ImageFormat : uint8_t
{
    INVALID,

    R8_UNORM,
    R8_G8_UNORM,
    R8_G8_B8_UNORM,
    R8_G8_B8_A8_UNORM,

    R16_UNORM,
    R16_G16_UNORM,
    R16_G16_B16_UNORM,
    R16_G16_B16_A16_UNORM,

    R16_FLOAT,
    R16_G16_FLOAT,
    R16_G16_B16_FLOAT,
    R16_G16_B16_A16_FLOAT,

    R32_FLOAT,
    R32_G32_FLOAT,
    R32_G32_B32_FLOAT,
    R32_G32_B32_A32_FLOAT,

    COUNT
};

constexpr bool IsFormat8BitUnorm( ImageFormat format )
{
    return Underlying( ImageFormat::R8_UNORM ) <= Underlying( format ) &&
           Underlying( format ) <= Underlying( ImageFormat::R8_G8_B8_A8_UNORM );
}

constexpr bool IsFormat16BitUnorm( ImageFormat format )
{
    return Underlying( ImageFormat::R16_UNORM ) <= Underlying( format ) &&
           Underlying( format ) <= Underlying( ImageFormat::R16_G16_B16_A16_UNORM );
}

constexpr bool IsFormat16BitFloat( ImageFormat format )
{
    return Underlying( ImageFormat::R16_FLOAT ) <= Underlying( format ) &&
           Underlying( format ) <= Underlying( ImageFormat::R16_G16_B16_A16_FLOAT );
}

constexpr bool IsFormat32BitFloat( ImageFormat format )
{
    return Underlying( ImageFormat::R32_FLOAT ) <= Underlying( format ) &&
           Underlying( format ) <= Underlying( ImageFormat::R32_G32_B32_A32_FLOAT );
}

inline uint32_t BitsPerPixel( ImageFormat format )
{
    static uint8_t mapping[] =
    {
        0,   // INVALID,
        8,   // R8_UNORM,
        16,  // R8_G8_UNORM,
        24,  // R8_G8_B8_UNORM,
        32,  // R8_G8_B8_A8_UNORM,
        16,  // R16_UNORM,
        32,  // R16_G16_UNORM,
        48,  // R16_G16_B16_UNORM,
        64,  // R16_G16_B16_A16_UNORM,
        16,  // R16_FLOAT,
        32,  // R16_G16_FLOAT,
        48,  // R16_G16_B16_FLOAT,
        64,  // R16_G16_B16_A16_FLOAT,
        32,  // R32_FLOAT,
        64,  // R32_G32_FLOAT,
        96,  // R32_G32_B32_FLOAT,
        128, // R32_G32_B32_A32_FLOAT,
    };

    static_assert( ARRAY_COUNT( mapping ) == static_cast<int>( ImageFormat::COUNT ) );
    return mapping[Underlying( format )];
}

inline uint32_t NumChannels( ImageFormat format )
{
    static constexpr uint8_t mapping[] =
    {
        0, // INVALID,
        1, // R8_UNORM,
        2, // R8_G8_UNORM,
        3, // R8_G8_B8_UNORM,
        4, // R8_G8_B8_A8_UNORM,
        1, // R16_UNORM,
        2, // R16_G16_UNORM,
        3, // R16_G16_B16_UNORM,
        4, // R16_G16_B16_A16_UNORM,
        1, // R16_FLOAT,
        2, // R16_G16_FLOAT,
        3, // R16_G16_B16_FLOAT,
        4, // R16_G16_B16_A16_FLOAT,
        1, // R32_FLOAT,
        2, // R32_G32_FLOAT,
        3, // R32_G32_B32_FLOAT,
        4, // R32_G32_B32_A32_FLOAT,
    };

    static_assert( ARRAY_COUNT( mapping ) == static_cast<int>( ImageFormat::COUNT ) );
    return mapping[Underlying( format )];
}

enum class Channel : uint8_t
{
    R = 0,
    G = 1,
    B = 2,
    A = 3,

    COUNT = 4
};

struct RawImage2D
{
    uint32_t width     = 0;
    uint32_t height    = 0;
    ImageFormat format = ImageFormat::INVALID;
    std::shared_ptr<uint8_t[]> data;

    RawImage2D() = default;
    RawImage2D( uint32_t inWidth, uint32_t inHeight, ImageFormat inFormat ) : width( inWidth ), height( inHeight ), format( inFormat )
    {
        size_t totalBytes = TotalBytes();
        data              = std::make_shared<uint8_t[]>( totalBytes );
    }
    RawImage2D( uint32_t inWidth, uint32_t inHeight, ImageFormat inFormat, uint8_t* srcData )
        : width( inWidth ), height( inHeight ), format( inFormat )
    {
        size_t totalBytes = TotalBytes();
        data              = std::shared_ptr<uint8_t[]>( srcData, []( uint8_t* x ) {} );
    }

    bool Load( const std::string& filename, ImageLoadFlags loadFlags = ImageLoadFlags::DEFAULT );

    // If the file format doesn't support saving the current Format, then the pixels will be converted to an appropriate format for that file
    bool Save( const std::string& filename, ImageSaveFlags saveFlags = ImageSaveFlags::DEFAULT ) const;

    // Returns a new image with same size + pixel data as the current image, but in the desired Format
    RawImage2D Convert( ImageFormat dstFormat ) const;

    RawImage2D Clone() const;

    float GetPixelAsFloat( int row, int col, int chan ) const;
    vec4 GetPixelAsFloat4( int row, int col ) const;

    void SetPixelFromFloat( int row, int col, int chan, float x );
    void SetPixelFromFloat4( int row, int col, vec4 pixel );

    uint32_t BitsPerPixel() const { return ::BitsPerPixel( format ); }
    uint32_t NumChannels() const { return ::NumChannels( format ); }

    size_t TotalBytes() const { return width * height * BitsPerPixel() / 8; }

    template <typename T = uint8_t>
    T* Raw()
    {
        return reinterpret_cast<T*>( data.get() );
    }

    template <typename T = uint8_t>
    const T* Raw() const
    {
        return reinterpret_cast<const T*>( data.get() );
    }
};

struct FloatImage2D
{
    uint32_t width       = 0;
    uint32_t height      = 0;
    uint32_t numChannels = 0;
    std::shared_ptr<float[]> data;

    FloatImage2D() = default;
    FloatImage2D( uint32_t inWidth, uint32_t inHeight, uint32_t inNumChannels )
        : width( inWidth ), height( inHeight ), numChannels( inNumChannels )
    {
        data = std::make_shared<float[]>( width * height * numChannels );
    }

    // Currently just calls RawImage2D::Load, and then FloatImageFromRawImage2D
    bool Load( const std::string& filename, ImageLoadFlags loadFlags = ImageLoadFlags::DEFAULT );

    // Currently just calls RawImage2DFromFloatImage, and then RawImage2D::Save
    bool Save( const std::string& filename, ImageSaveFlags saveFlags = ImageSaveFlags::DEFAULT ) const;

    FloatImage2D Resize( uint32_t newWidth, uint32_t newHeight ) const;
    FloatImage2D Clone() const;

    template <typename Func>
    void ForEachPixelIndex( Func F )
    {
        for ( uint32_t i = 0; i < width * height; ++i )
            F( i );
    }

    template <typename Func>
    void ForEachPixel( Func F )
    {
        for ( uint32_t i = 0; i < width * height; ++i )
        {
            F( &data[i * numChannels] );
        }
    }

    vec4 Sample( vec2 uv, bool clampHorizontal, bool clampVertical ) const; // bilinear
    vec4 GetFloat4( uint32_t pixelIndex ) const;
    vec4 GetFloat4( uint32_t row, uint32_t col ) const;
    vec4 GetFloat4( uint32_t row, uint32_t col, bool clampHorizontal, bool clampVertical ) const;
    void SetFromFloat4( uint32_t pixelIndex, const vec4& pixel );
    void SetFromFloat4( uint32_t row, uint32_t col, const vec4& pixel );

    float operator()( uint32_t row, uint32_t col ) const
    {
        return data[(row * width + col) * numChannels];
    }

    operator bool() const { return width && height && numChannels && data != nullptr; }
};


// Creates a new image in the float32 version of rawImage. One caveat: if the raw format is already float32,
// then data will just point to the same RawImage2D memory to avoid an allocation + copy
FloatImage2D FloatImageFromRawImage2D( const RawImage2D& rawImage );

// Creates a new raw image with the specified format. Like FloatImageFromRawImage2D, if "format" is already the same as the float image,
// then the returned raw image isn't "new", it just points to the same memory as the float image to avoid an allocation + copy
// If format == ImageFormat::INVALID, then it just uses the float image's original format
RawImage2D RawImage2DFromFloatImage( const FloatImage2D& floatImage, ImageFormat format = ImageFormat::INVALID );
std::vector<RawImage2D> RawImage2DFromFloatImages(
    const std::vector<FloatImage2D>& floatImages, ImageFormat format = ImageFormat::INVALID );

struct MipmapGenerationSettings
{
    bool clampHorizontal = false;
    bool clampVertical   = false;
};

std::vector<FloatImage2D> GenerateMipmaps( const FloatImage2D& floatImage, const MipmapGenerationSettings& settings );

uint32_t CalculateNumMips( uint32_t width, uint32_t height );
double FloatImageMSE( const FloatImage2D& img1, const FloatImage2D& img2, uint32_t channelsToCalc = 0b1111 );
double MSEToPSNR( double mse, double maxValue = 1.0 );

FloatImage2D LoadNormalMap( const std::string& filename, float slopeScale, bool flipY, bool flipX );