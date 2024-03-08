#include "image.hpp"
#include "shared/filesystem.hpp"
#include "shared/logger.hpp"
#include "shared/time.hpp"
#include "stb/stb_image_resize.h"
#include <unordered_set>

void BuildDisplacement( const FloatImage2D& dxdyImg, float* h0, float* h1, uint32_t numIterations, float iterationMultiplier )
{
    uint32_t width = dxdyImg.width;
    uint32_t height = dxdyImg.height;
    if ( width == 1 || height == 1 )
    {
        memset( h0, 0, width * height * sizeof( float ) );
    }
    else
    {
        FloatImage2D halfDxDyImg = dxdyImg.Resize( width / 2, height / 2 );

        BuildDisplacement( halfDxDyImg, h0, h1, numIterations, 2 * iterationMultiplier );

        stbir_resize_float_generic( h1, width / 2, height / 2, 0, h0, width, height, 0,
            1, -1, 0, STBIR_EDGE_WRAP, STBIR_FILTER_BOX, STBIR_COLORSPACE_LINEAR, NULL );
    }    
    
    float* cur = h0;
    float* next = h1;
    numIterations = static_cast<uint32_t>( Min( 1.0f, iterationMultiplier ) * numIterations );
    numIterations += numIterations % 2; // ensure odd number
    
    for ( uint32_t iter = 0; iter < numIterations; ++iter )
    {
        #pragma omp parallel for
        for ( int row = 0; row < (int)height; ++row )
        {
            // wrap all edges
            uint32_t up = row == 0 ? (height - 1) : (row - 1);
            uint32_t down = row == (height - 1) ? 0 : (row + 1);
            
            for ( uint32_t col = 0; col < width; ++col )
            {
                uint32_t left = col == 0 ? (width - 1) : (col - 1);
                uint32_t right = col == (width - 1) ? 0 : (col + 1);
                //vec2 currDxDy = vec2( dxdyImg.GetFloat4( row, col ) );

                float h = 0;
                h += cur[left  + row * width]   + (dxdyImg.GetFloat4( row, left ).x);
                h += cur[right + row * width]   - (dxdyImg.GetFloat4( row, right ).x);
                h += cur[col   + up * width]    + (dxdyImg.GetFloat4( up, col ).y);
                h += cur[col   + down * width]  - (dxdyImg.GetFloat4( down, col ).y);

                next[col + row * width] = h / 4;
            }
        }
        std::swap( cur, next );
    }
    //LOG( "Iterated on %u x %u", width, height );
}

vec2 DxDyFromNormal( vec3 normal )
{
    vec2 dxdy = fabs( normal.z ) >= 0.001f ? -vec2( normal ) / normal.z : vec2( 0.0f );
    const float slope = Length( dxdy );
    constexpr float MAX_SLOPE = 16.0f;
    if ( slope > MAX_SLOPE )
    {
        dxdy *= MAX_SLOPE / slope;
    }

    return dxdy;
}

struct HeightMapData
{
    FloatImage2D image;
    float minH;
    float maxH;
    uint32_t iterations;
};

HeightMapData GetHeightMapFromNormalMap( const FloatImage2D& normalMap, uint32_t iterations = 512, float multiplier = 1.0f )
{
    FloatImage2D dxdyImg = FloatImage2D( normalMap.width, normalMap.height, 2 );
    vec2 rcpSize = { 1.0f / normalMap.width, 1.0f / normalMap.height };
    for ( uint32_t i = 0; i < normalMap.width * normalMap.height; ++i )
    {
        vec3 normal = vec3( normalMap.GetFloat4( i ) );
        dxdyImg.SetFromFloat4( i, vec4( DxDyFromNormal( normal ) * rcpSize, 0, 0 ) );
    }

#if 0
    FloatImage2D dxdyImg2 = FloatImage2D( normalMap.width, normalMap.height, 3 );
    for ( uint32_t i = 0; i < normalMap.width * normalMap.height; ++i )
    {
        dxdyImg2.SetFromFloat4( i, vec4( DxDyFromNormal( normal ), 0, 0 ) );
    }
    dxdyImg2.Save( ROOT_DIR "dxdy.exr", ImageSaveFlags::KEEP_FLOATS_AS_32_BIT );
#endif

    FloatImage2D h0 = FloatImage2D( normalMap.width, normalMap.height, 1 );
    FloatImage2D h1 = FloatImage2D( normalMap.width, normalMap.height, 1 );
    BuildDisplacement( dxdyImg, h0.data.get(), h1.data.get(), iterations, multiplier );

    // normalize h1 to [0, 1] range
    float minH = FLT_MAX;
    float maxH = -FLT_MAX;
    for ( uint32_t i = 0; i < normalMap.width * normalMap.height; ++i )
    {
        float h = h1.data[i];
        minH = Min( minH, h );
        maxH = Max( maxH, h );
    }

    float scale = maxH - minH;
    float invScale = scale > 0 ? 1.0f / scale : 1.0f;
    LOG( "MinH: %f, MaxH = %f, Scale: %f", minH, maxH, scale );
    for ( uint32_t i = 0; i < normalMap.width * normalMap.height; ++i )
    {
        float h = h1.data[i];
        h1.data[i] = (h - minH) * invScale;
    }

    HeightMapData returnData;
    returnData.image = h1;
    returnData.minH = minH;
    returnData.maxH = maxH;
    returnData.iterations = iterations;

    return returnData;
}

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
        // unpack normals from [0-1] colors to [-1,-1] vectors
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

int main( int argc, char** argv )
{
    Logger_Init();
    Logger_AddLogLocation( "stdout", stdout );

    bool isYDown = false;
    std::string filename;
    filename = ROOT_DIR "images/rock_wall_10_nor_dx_1k.png"; isYDown = true;
    //filename = ROOT_DIR "images/test_normals_1_512.png"; isYDown = false;
    //filename = ROOT_DIR "images/test.png"; isYDown = false;

    FloatImage2D normalMap = LoadNormalMap( filename, 1.0f, isYDown );

#if 0
    FloatImage2D flippedNormalMap( normalMap.width, normalMap.height, 3 );
    for ( uint32_t i = 0; i < normalMap.width * normalMap.height; ++i )
    {
        vec3 N = normalMap.GetFloat4( i );
        flippedNormalMap.SetFromFloat4( i, vec4( 0.5f * (N + vec3( 1.0f )), 0 ) );
    }
    flippedNormalMap.Save( GetFilenameMinusExtension( filename ) + "_YDown.png" );
    return 0;
#endif

    uint32_t iterations[] = { 32, 64, 128, 256, 512, 1024, 2048, 4096, 32768, 131072 };
    HeightMapData heightMaps[ARRAY_COUNT( iterations )];
    for ( int i = 0; i < ARRAY_COUNT( iterations ); ++i )
    {
        auto startTime = PG::Time::GetTimePoint();
        heightMaps[i] = GetHeightMapFromNormalMap( normalMap, iterations[i] );
        LOG( "Finished %ux%u image with %u iterations in %.2f seconds\n", normalMap.width, normalMap.height, iterations[i], PG::Time::GetTimeSince( startTime ) / 1000.0f );

        //std::string heightMapFilename = GetFilenameMinusExtension( filename ) + "_h_" + std::to_string( iterations[i] ) + GetFileExtension( filename );
        //heightMap.Save( heightMapFilename );
    }

    // combine images into 1 big one for comparison, with a 4 pixel border between
    std::unordered_set<uint32_t> imgsToSave = { 32, 128, 512, 2048, 32768, 131072 };
    uint32_t imagesSaved = 0;
    uint32_t totalImagesToSave = imgsToSave.size();
    RawImage2D combinedImage( normalMap.width * totalImagesToSave + 4 * (totalImagesToSave - 1), normalMap.height, ImageFormat::R8_G8_B8_UNORM );
    for ( int i = 0; i < ARRAY_COUNT( iterations ); ++i )
    {
        if ( !imgsToSave.contains( iterations[i] ) )
            continue;

        const HeightMapData& data = heightMaps[i];
        
        for ( uint32_t row = 0; row < data.image.height; ++row )
        {
            for ( uint32_t col = 0; col < data.image.width; ++col )
            {
                uint32_t combinedCol = col + normalMap.width * imagesSaved + 4 * imagesSaved;
                float h = data.image.GetFloat4( row, col ).x;
                combinedImage.SetPixelFromFloat4( row, combinedCol, vec4( h, h, h, 1 ) );
            }
        }

        ++imagesSaved;

        // draw boundary line between images
        if ( imagesSaved != totalImagesToSave )
        {
            for ( uint32_t row = 0; row < data.image.height; ++row )
            {
                for ( uint32_t col = 0; col < 4; ++col )
                {
                    uint32_t combinedCol = col + normalMap.width * imagesSaved + 4 * (imagesSaved - 1);
                    combinedImage.SetPixelFromFloat4( row, combinedCol, vec4( 0, 0, 1, 1 ) );
                }
            }
        }
    }
    std::string combinedName = GetFilenameMinusExtension( filename ) + "_heights.png";
    combinedImage.Save( combinedName );

    Logger_Shutdown();

    return 0;
}
