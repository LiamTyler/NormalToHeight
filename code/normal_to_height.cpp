#include "normal_to_height.hpp"

void GeneratedHeightMap::CalcMinMax()
{
    minH = FLT_MAX;
    maxH = -FLT_MAX;
    for ( int i = 0; i < map.width * map.height; ++i )
    {
        float h = map.data[i];
        minH = Min( minH, h );
        maxH = Max( maxH, h );
    }
}

void GeneratedHeightMap::Pack0To1()
{
    CalcMinMax();
    scale = maxH - minH;
    bias = minH;
    float invScale = scale > 0 ? 1.0f / scale : 1.0f;
    for ( int i = 0; i < map.width * map.height; ++i )
    {
        float h = map.data[i];
        map.data[i] = (h - bias) * invScale;
    }
}

void GeneratedHeightMap::Unpack0To1()
{
    for ( int i = 0; i < map.width * map.height; ++i )
    {
        float h = map.data[i];
        map.data[i] = h * scale + minH;
    }
    scale = 1;
    bias = 0;
}

float GeneratedHeightMap::GetH( int pixelIndex ) const
{
    float packed = map.Get( pixelIndex ).x;
    return packed * scale + bias;
}

float GeneratedHeightMap::GetH( int row, int col ) const
{
    return GetH( row * map.width + col );
}

vec2 DxDyFromNormal( vec3 normal )
{
    vec2 dxdy = vec2( 0.0f );
    if ( normal.z >= 0.001f )
    {
        dxdy = vec2( -normal.x / normal.z, -normal.y / normal.z );
    }

    const float slope = Length( dxdy );
    constexpr float MAX_SLOPE = 16.0f;
    if ( slope > MAX_SLOPE )
    {
        dxdy *= MAX_SLOPE / slope;
    }

    return dxdy;
}

void BuildDisplacement( const FloatImage2D& dxdyImg, float* tmpH1, float* tmpH2, uint32_t numIterations, float iterationMultiplier )
{
    int width = dxdyImg.width;
    int height = dxdyImg.height;
    if ( width == 1 || height == 1 )
    {
        memset( tmpH2, 0, width * height * sizeof( float ) );
        return;
    }
    else
    {
        int halfW = Max( width / 2, 1 );
        int halfH = Max( height / 2, 1 );
        FloatImage2D halfDxDyImg = dxdyImg.Resize( halfW, halfH );
        float scaleX = width / static_cast<float>( halfW );
        float scaleY = height / static_cast<float>( halfH );
        vec2 scales = vec2( scaleX, scaleY );
        // update the slopes, to account for each texel having a bigger footprint now.
        // Aka, re-correcting 'invSize' from the original 'DxDyFromNormal( normal ) * invSize' in mip0
        for ( int i = 0; i < halfW * halfH; ++i )
            halfDxDyImg.Set( i, scales * vec2( halfDxDyImg.Get( i ) ) );


        BuildDisplacement( halfDxDyImg, tmpH1, tmpH2, numIterations, 2 * iterationMultiplier );

        stbir_resize_float_generic( tmpH2, halfW, halfH, 0, tmpH1, width, height, 0,
            1, -1, 0, STBIR_EDGE_WRAP, STBIR_FILTER_BOX, STBIR_COLORSPACE_LINEAR, NULL );
    }
    
    float* cur = tmpH1;
    float* next = tmpH2;
    numIterations = static_cast<uint32_t>( Min( 1.0f, iterationMultiplier ) * numIterations );
    numIterations += numIterations % 2; // ensure odd number
    
    for ( uint32_t iter = 0; iter < numIterations; ++iter )
    {
        #pragma omp parallel for
        for ( int row = 0; row < height; ++row )
        {
            float localDiff = 0;
            int up = Wrap( row - 1, height );
            int down = Wrap( row + 1, height );
            
            for ( int col = 0; col < width; ++col )
            {
                int left = Wrap( col - 1, width );
                int right = Wrap( col + 1, width );
                
                float h = 0;
                h += cur[left  + row  * width]  + 0.5f * dxdyImg.Get( row, left ).x;
                h += cur[right + row  * width]  - 0.5f * dxdyImg.Get( row, right ).x;
                h += cur[col   + up   * width]  + 0.5f * dxdyImg.Get( up, col ).y;
                h += cur[col   + down * width]  - 0.5f * dxdyImg.Get( down, col ).y;
                
                next[col + row * width] = h / 4;
            }
        }

        std::swap( cur, next );
    }
}

GenerationResults GetHeightMapFromNormalMap( const FloatImage2D& normalMap, uint32_t iterations, float iterationMultiplier )
{
    GenerationResults returnData;
    returnData.heightMap = GeneratedHeightMap( normalMap.width, normalMap.height );

    auto startTime = PG::Time::GetTimePoint();

    FloatImage2D dxdyImg = FloatImage2D( normalMap.width, normalMap.height, 2 );
    vec2 invSize = { 1.0f / normalMap.width, 1.0f / normalMap.height };
    for ( int i = 0; i < normalMap.width * normalMap.height; ++i )
    {
        vec3 normal = normalMap.Get( i );
        dxdyImg.Set( i, DxDyFromNormal( normal ) * invSize );
    }

    FloatImage2D tmpH1 = FloatImage2D( normalMap.width, normalMap.height, 1 );
    BuildDisplacement( dxdyImg, tmpH1.data.get(), returnData.heightMap.map.data.get(), iterations, iterationMultiplier );

    auto stopTime = PG::Time::GetTimePoint();

    returnData.heightMap.CalcMinMax();
    returnData.iterations = iterations;
    returnData.timeToGenerate = (float)PG::Time::GetElapsedTime( startTime, stopTime ) / 1000.0f;

    return returnData;
}