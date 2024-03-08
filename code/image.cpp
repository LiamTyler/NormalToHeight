#include "image.hpp"
#include "shared/assert.hpp"
#include "shared/float_conversions.hpp"
#include "shared/logger.hpp"
#include "shared/math_vec.hpp"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize.h"
#include <array>

static vec4 DEFAULT_PIXEL_FLOAT32    = vec4( 0, 0, 0, 1 );

float RawImage2D::GetPixelAsFloat( int row, int col, int chan ) const
{
    uint32_t numChannels = NumChannels();
    if ( chan >= (int)numChannels )
    {
        return DEFAULT_PIXEL_FLOAT32[chan];
    }
    int index = numChannels * ( row * width + col ) + chan;

    if ( IsFormat8BitUnorm( format ) )
    {
        return UNormByteToFloat( Raw<uint8_t>()[index] );
    }
    else if ( IsFormat16BitUnorm( format ) )
    {
        return UNorm16ToFloat( Raw<uint16_t>()[index] );
    }
    else if ( IsFormat16BitFloat( format ) )
    {
        return Float16ToFloat32( Raw<float16>()[index] );
    }
    else if ( IsFormat32BitFloat( format ) )
    {
        return Raw<float>()[index];
    }

    return 0;
}

vec4 RawImage2D::GetPixelAsFloat4( int row, int col ) const
{
    vec4 pixel           = DEFAULT_PIXEL_FLOAT32;
    uint32_t numChannels = NumChannels();
    uint32_t index       = numChannels * ( row * width + col );
    if ( IsFormat8BitUnorm( format ) )
    {
        for ( uint32_t chan = 0; chan < numChannels; ++chan )
            pixel[chan] = UNormByteToFloat( Raw<uint8_t>()[index + chan] );
    }
    else if ( IsFormat16BitUnorm( format ) )
    {
        for ( uint32_t chan = 0; chan < numChannels; ++chan )
            pixel[chan] = UNorm16ToFloat( Raw<uint16_t>()[index + chan] );
    }
    else if ( IsFormat16BitFloat( format ) )
    {
        for ( uint32_t chan = 0; chan < numChannels; ++chan )
            pixel[chan] = Float16ToFloat32( Raw<float16>()[index + chan] );
    }
    else if ( IsFormat32BitFloat( format ) )
    {
        for ( uint32_t chan = 0; chan < numChannels; ++chan )
            pixel[chan] = Raw<float>()[index + chan];
    }

    return pixel;
}

void RawImage2D::SetPixelFromFloat( int row, int col, int chan, float x )
{
    uint32_t numChannels = NumChannels();
    int index            = numChannels * ( row * width + col ) + chan;
    if ( IsFormat8BitUnorm( format ) )
    {
        Raw<uint8_t>()[index] = UNormFloatToByte( x );
    }
    else if ( IsFormat16BitUnorm( format ) )
    {
        Raw<uint16_t>()[index] = FloatToUNorm16( x );
    }
    else if ( IsFormat16BitFloat( format ) )
    {
        Raw<float16>()[index] = Float32ToFloat16( x );
    }
    else if ( IsFormat32BitFloat( format ) )
    {
        Raw<float>()[index] = x;
    }
}

void RawImage2D::SetPixelFromFloat4( int row, int col, vec4 pixel )
{
    uint32_t numChannels = NumChannels();
    uint32_t index       = numChannels * ( row * width + col );
    if ( IsFormat8BitUnorm( format ) )
    {
        for ( uint32_t chan = 0; chan < numChannels; ++chan )
            Raw<uint8_t>()[index + chan] = UNormFloatToByte( pixel[chan] );
    }
    else if ( IsFormat16BitUnorm( format ) )
    {
        for ( uint32_t chan = 0; chan < numChannels; ++chan )
            Raw<uint16_t>()[index + chan] = FloatToUNorm16( pixel[chan] );
    }
    else if ( IsFormat16BitFloat( format ) )
    {
        for ( uint32_t chan = 0; chan < numChannels; ++chan )
            Raw<float16>()[index + chan] = Float32ToFloat16( pixel[chan] );
    }
    else if ( IsFormat32BitFloat( format ) )
    {
        for ( uint32_t chan = 0; chan < numChannels; ++chan )
            Raw<float>()[index + chan] = pixel[chan];
    }
}

// TODO: optimize
RawImage2D RawImage2D::Convert( ImageFormat dstFormat ) const
{
    RawImage2D outputImg( width, height, dstFormat );
    uint32_t inputChannels  = NumChannels();
    uint32_t outputChannels = outputImg.NumChannels();

    for ( uint32_t row = 0; row < height; ++row )
    {
        for ( uint32_t col = 0; col < width; ++col )
        {
            vec4 pixelAsFloat = GetPixelAsFloat4( row, col );
            outputImg.SetPixelFromFloat4( row, col, pixelAsFloat );
        }
    }

    return outputImg;
}

RawImage2D RawImage2D::Clone() const
{
    RawImage2D ret( width, height, format );
    memcpy( ret.data.get(), data.get(), TotalBytes() );
    return ret;
}

// TODO: load directly
bool FloatImage2D::Load( const std::string& filename, ImageLoadFlags loadFlags )
{
    RawImage2D rawImg;
    if ( !rawImg.Load( filename, loadFlags ) )
        return false;

    *this = FloatImageFromRawImage2D( rawImg );
    return true;
}

bool FloatImage2D::Save( const std::string& filename, ImageSaveFlags saveFlags ) const
{
    ImageFormat format = static_cast<ImageFormat>( Underlying( ImageFormat::R32_FLOAT ) + numChannels - 1 );
    RawImage2D img     = RawImage2DFromFloatImage( *this, format );
    return img.Save( filename, saveFlags );
}

FloatImage2D FloatImage2D::Resize( uint32_t newWidth, uint32_t newHeight ) const
{
    if ( width == newWidth && height == newHeight )
    {
        return *this;
    }

    FloatImage2D outputImage( newWidth, newHeight, numChannels );
    if ( width == 1 && height == 1 )
    {
        float p[4];
        for ( uint32_t i = 0; i < numChannels; ++i )
            p[i] = data[i];

        for ( uint32_t i = 0; i < newWidth * newHeight * numChannels; i += numChannels )
        {
            memcpy( &outputImage.data[i], p, numChannels * sizeof( float ) );
        }
        return outputImage;
    }

    if ( !stbir_resize_float( data.get(), width, height, 0, outputImage.data.get(), newWidth, newHeight, 0, numChannels ) )
    {
        return {};
    }

    return outputImage;
}

FloatImage2D FloatImage2D::Clone() const
{
    FloatImage2D ret( width, height, numChannels );
    memcpy( ret.data.get(), data.get(), width * height * numChannels * sizeof( float ) );
    return ret;
}

vec4 FloatImage2D::Sample( vec2 uv, bool clampHorizontal, bool clampVertical ) const
{
    uv.x -= std::floor( uv.x );
    uv.y -= std::floor( uv.y );

    // subtract 0.5 to account for pixel centers
    uv         = uv * vec2( width, height ) - vec2( 0.5f );
    vec2 start = floor( uv );
    int col    = static_cast<int>( start.x );
    int row    = static_cast<int>( start.y );

    vec2 d          = uv - start;
    const float w00 = ( 1.0f - d.x ) * ( 1.0f - d.y );
    const float w01 = d.x * ( 1.0f - d.y );
    const float w10 = ( 1.0f - d.x ) * d.y;
    const float w11 = d.x * d.y;

    vec4 p00 = GetFloat4( row, col, clampHorizontal, clampVertical );
    vec4 p01 = GetFloat4( row, col + 1, clampHorizontal, clampVertical );
    vec4 p10 = GetFloat4( row + 1, col, clampHorizontal, clampVertical );
    vec4 p11 = GetFloat4( row + 1, col + 1, clampHorizontal, clampVertical );

    vec4 ret( 0, 0, 0, 1 );
    for ( uint32_t i = 0; i < numChannels; ++i )
    {
        ret[i] = w00 * p00[i] + w01 * p01[i] + w10 * p10[i] + w11 * p11[i];
    }

    return ret;
}

vec4 FloatImage2D::GetFloat4( uint32_t pixelIndex ) const
{
    vec4 pixel( 0, 0, 0, 1 );
    pixelIndex *= numChannels;
    for ( uint32_t chan = 0; chan < numChannels; ++chan )
    {
        pixel[chan] = data[pixelIndex + chan];
    }

    return pixel;
}

vec4 FloatImage2D::GetFloat4( uint32_t row, uint32_t col ) const { return GetFloat4( row * width + col ); }

vec4 FloatImage2D::GetFloat4( uint32_t row, uint32_t col, bool clampHorizontal, bool clampVertical ) const
{
    if ( clampHorizontal )
    {
        col = std::clamp( col, 0u, width - 1 );
    }
    else
    {
        col = ( col % width );
        if ( col < 0 )
            col += width;
    }
    if ( clampVertical )
    {
        row = std::clamp( row, 0u, height - 1 );
    }
    else
    {
        row = ( row % height );
        if ( row < 0 )
            row += height;
    }

    return GetFloat4( row, col );
}

void FloatImage2D::SetFromFloat4( uint32_t pixelIndex, const vec4& pixel )
{
    pixelIndex *= numChannels;
    for ( uint32_t chan = 0; chan < numChannels; ++chan )
    {
        data[pixelIndex + chan] = pixel[chan];
    }
}

void FloatImage2D::SetFromFloat4( uint32_t row, uint32_t col, const vec4& pixel ) { SetFromFloat4( row * width + col, pixel ); }

FloatImage2D FloatImageFromRawImage2D( const RawImage2D& rawImage )
{
    FloatImage2D floatImage;
    floatImage.width       = rawImage.width;
    floatImage.height      = rawImage.height;
    floatImage.numChannels = rawImage.NumChannels();
    if ( IsFormat32BitFloat( rawImage.format ) )
    {
        floatImage.data = std::reinterpret_pointer_cast<float[]>( rawImage.data );
    }
    else
    {
        auto convertedImage =
            rawImage.Convert( static_cast<ImageFormat>( Underlying( ImageFormat::R32_FLOAT ) + floatImage.numChannels - 1 ) );
        floatImage.data = std::reinterpret_pointer_cast<float[]>( convertedImage.data );
    }

    return floatImage;
}

RawImage2D RawImage2DFromFloatImage( const FloatImage2D& floatImage, ImageFormat format )
{
    RawImage2D rawImage;
    rawImage.width  = floatImage.width;
    rawImage.height = floatImage.height;
    rawImage.format = static_cast<ImageFormat>( Underlying( ImageFormat::R32_FLOAT ) + floatImage.numChannels - 1 );
    rawImage.data   = std::reinterpret_pointer_cast<uint8_t[]>( floatImage.data );
    if ( format == ImageFormat::INVALID )
    {
        format = rawImage.format;
    }
    if ( rawImage.format != format )
    {
        rawImage = rawImage.Convert( format );
    }

    return rawImage;
}

std::vector<RawImage2D> RawImage2DFromFloatImages( const std::vector<FloatImage2D>& floatImages, ImageFormat format )
{
    std::vector<RawImage2D> rawImages( floatImages.size() );
    for ( size_t i = 0; i < floatImages.size(); ++i )
    {
        rawImages[i] = RawImage2DFromFloatImage( floatImages[i], format );
    }

    return rawImages;
}

std::vector<FloatImage2D> GenerateMipmaps( const FloatImage2D& image, const MipmapGenerationSettings& settings )
{
    std::vector<FloatImage2D> mips;

    uint32_t numMips = CalculateNumMips( image.width, image.height );

    uint32_t w           = image.width;
    uint32_t h           = image.height;
    uint32_t numChannels = image.numChannels;
    stbir_edge edgeModeU = settings.clampHorizontal ? STBIR_EDGE_CLAMP : STBIR_EDGE_WRAP;
    stbir_edge edgeModeV = settings.clampVertical ? STBIR_EDGE_CLAMP : STBIR_EDGE_WRAP;
    for ( uint32_t mipLevel = 0; mipLevel < numMips; ++mipLevel )
    {
        FloatImage2D mip( w, h, image.numChannels );
        if ( mipLevel == 0 )
        {
            memcpy( mip.data.get(), image.data.get(), w * h * numChannels * sizeof( float ) );
        }
        else
        {
            // NOTE!!! With STBIR_EDGE_WRAP and STBIR_FILTER_MITCHELL, im seeing artifacts, at least for non-even dimensioned images.
            // Both the top and right edges have dark lines near them, and some seemingly garbage pixels
            int alphaChannel            = STBIR_ALPHA_CHANNEL_NONE;
            const FloatImage2D& prevMip = mips[mipLevel - 1];
            stbir_resize( prevMip.data.get(), prevMip.width, prevMip.height, prevMip.width * numChannels * sizeof( float ), mip.data.get(),
                w, h, w * numChannels * sizeof( float ), STBIR_TYPE_FLOAT, numChannels, alphaChannel, 0, edgeModeU, edgeModeV,
                STBIR_FILTER_BOX, STBIR_FILTER_BOX, STBIR_COLORSPACE_LINEAR, NULL );
        }

        mips.push_back( mip );
        w = Max( 1u, w >> 1 );
        h = Max( 1u, h >> 1 );
    }

    return mips;
}

uint32_t CalculateNumMips( uint32_t width, uint32_t height )
{
    uint32_t largestDim = Max( width, height );
    if ( largestDim == 0 )
    {
        return 0;
    }

    return 1 + static_cast<uint32_t>( std::log2f( static_cast<float>( largestDim ) ) );
}

double FloatImageMSE( const FloatImage2D& img1, const FloatImage2D& img2, uint32_t channelsToCalc )
{
    if ( img1.width != img2.width || img1.height != img2.height || img1.numChannels != img2.numChannels )
    {
        LOG_ERR( "Images must be same size and channel count to calculate MSE" );
        return -1;
    }

    uint32_t width       = img1.width;
    uint32_t height      = img1.height;
    uint32_t numChannels = img1.numChannels;

    double mse = 0;
    for ( uint32_t pixelIdx = 0; pixelIdx < width * height; ++pixelIdx )
    {
        if ( pixelIdx == 350 * img1.width + 334 )
        {
            printf( "" );
        }
        for ( uint32_t chan = 0; chan < numChannels; ++chan )
        {
            if ( channelsToCalc & ( 1 << ( 3 - chan ) ) )
            {
                float x = img1.data[numChannels * pixelIdx + chan];
                float y = img2.data[numChannels * pixelIdx + chan];
                mse += ( x - y ) * ( x - y );
            }
        }
    }

    uint32_t numActualChannels = 0;
    for ( uint32_t chan = 0; chan < numChannels; ++chan )
    {
        if ( channelsToCalc & ( 1 << ( 3 - chan ) ) )
        {
            ++numActualChannels;
        }
    }

    mse /= ( width * height * numActualChannels );
    return mse;
}

double MSEToPSNR( double mse, double maxValue ) { return 10.0 * log10( maxValue * maxValue / mse ); }


static vec3 UnpackNormal_8Bit( const uint8_t* v )
{
    float x = (v[0] - 128) / 127.0f;
    float y = (v[1] - 128) / 127.0f;
    float z = (v[2] - 128) / 127.0f;
    return Normalize( vec3( x, y, z ) );
}

static vec3 UnpackNormal_16Bit( const uint16_t* v )
{
    float x = (v[0] - 32768) / 32767.0f;
    float y = (v[1] - 32768) / 32767.0f;
    float z = (v[2] - 32768) / 32767.0f;
    return Normalize( vec3( x, y, z ) );
}

static vec3 UnpackNormal_32Bit( const vec3& v )
{
    return Normalize( 2.0f * v - vec3( 1.0f ) );
}

static vec3 ScaleNormal( vec3 n, float scale )
{
    n.x *= scale;
    n.y *= scale;

    return Normalize( n );
}

// Unpack the normals such that the error on neutral normals is 0, at the cost of higher error elsewhere
// http://www.aclockworkberry.com/normal-unpacking-quantization-errors/
FloatImage2D LoadNormalMap( const std::string& filename, float slopeScale, bool isYDown )
{
    RawImage2D rawImg;
    if ( !rawImg.Load( filename ) )
        return {};

    FloatImage2D normalMap;
    if ( IsFormat16BitFloat( rawImg.format ) || IsFormat32BitFloat( rawImg.format ) )
    {
        normalMap = FloatImageFromRawImage2D( rawImg );
        for ( uint32_t i = 0; i < normalMap.width * normalMap.height; ++i )
        {
            vec3 normal = UnpackNormal_32Bit( vec3( normalMap.GetFloat4( i ) ) );
            if ( !isYDown )
                normal.y *= -1;
            normal = ScaleNormal( normal, slopeScale );
            normalMap.SetFromFloat4( i, vec4( normal, 0 ) );
        }
    }
    else
    {
        normalMap = FloatImage2D( rawImg.width, rawImg.height, 3 );
        for ( uint32_t i = 0; i < normalMap.width * normalMap.height; ++i )
        {
            vec3 normal;
            if ( IsFormat8BitUnorm( rawImg.format ) )
                normal = UnpackNormal_8Bit( rawImg.Raw<uint8_t>() + i * rawImg.NumChannels() );
            else
                normal = UnpackNormal_16Bit( rawImg.Raw<uint16_t>() + i * rawImg.NumChannels() );

            if ( !isYDown )
                normal.y *= -1;
            normal = ScaleNormal( normal, slopeScale );
            normalMap.SetFromFloat4( i, vec4( normal, 0 ) );
        }
    }

    return normalMap;
}