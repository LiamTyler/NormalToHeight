#include "Eigen/Dense"
#include "Eigen/Sparse"
#include "image.hpp"
#include "getopt/getopt.h"
#include "shared/filesystem.hpp"
#include "shared/logger.hpp"
#include "shared/time.hpp"
#include "stb/stb_image_resize.h"
#include <iostream>
#include <unordered_set>

enum class HeightGenMethod
{
    RELAXATION = 0,
    RELAXTION_EDGE_AWARE = 1,
    LINEAR_SYSTEM = 2,

    COUNT = 3,
    DEFAULT = RELAXATION
};

struct Options
{
    std::string normalMapPath;
    HeightGenMethod heightGenMethod = HeightGenMethod::DEFAULT;
    uint32_t numIterations = 512;
    float slopeScale = 1.0f;
    bool flipY = false;
    bool flipX = false;
    bool outputGenNormals = false;
    bool rangeOfIterations = false;
};

struct GenerationResults
{
    FloatImage2D heightMap;
    float minH;
    float maxH;
    uint32_t iterations;
    float timeToGenerate;

    uint32_t maxIterations; // when using LINEAR_SYSTEM
    float solverError; // when using LINEAR_SYSTEM

    FloatImage2D generatedNormalMap;
    float PSNR; // against the original input normal map

    float GetH( uint32_t row, uint32_t col ) const
    {
        float scale = maxH - minH;
        float packed = heightMap.GetFloat4( row, col ).x;
        return packed * scale + minH;
    }
};

static uint32_t Wrap( int v, int maxVal )
{
    if ( v < 0 ) return v + maxVal;
    else if ( v >= maxVal ) return 0;
    else return v;
}

#define DIAGONALS 0

void BuildDisplacement( const FloatImage2D& dxdyImg, float* h0, float* h1, uint32_t numIterations, float iterationMultiplier )
{
    int width = (int)dxdyImg.width;
    int height = (int)dxdyImg.height;
    if ( width == 1 || height == 1 )
    {
        memset( h0, 0, width * height * sizeof( float ) );
    }
    else
    {
        int halfW = Max( width / 2, 1 );
        int halfH = Max( height / 2, 1 );
        FloatImage2D halfDxDyImg = dxdyImg.Resize( halfW, halfH );
        float scaleX = width / static_cast<float>( halfW );
        float scaleY = height / static_cast<float>( halfH );
        // update the 'rcpSize' from the original 'DxDyFromNormal( normal ) * rcpSize'
        halfDxDyImg.ForEachPixel( [&]( float* p )
            {
                p[0] *= scaleX;
                p[1] *= scaleY;
            });

        BuildDisplacement( halfDxDyImg, h0, h1, numIterations, 2 * iterationMultiplier );

        stbir_resize_float_generic( h1, halfW, halfH, 0, h0, width, height, 0,
            1, -1, 0, STBIR_EDGE_WRAP, STBIR_FILTER_BOX, STBIR_COLORSPACE_LINEAR, NULL );
    }    
    
    float* cur = h0;
    float* next = h1;
    numIterations = static_cast<uint32_t>( Min( 1.0f, iterationMultiplier ) * numIterations );
    numIterations += numIterations % 2; // ensure odd number
    
    for ( uint32_t iter = 0; iter < numIterations; ++iter )
    {
        #pragma omp parallel for
        for ( int row = 0; row < height; ++row )
        {
            int up = Wrap( row - 1, height );
            int down = Wrap( row + 1, height );
            
            for ( int col = 0; col < width; ++col )
            {
                int left = Wrap( col - 1, width );
                int right = Wrap( col + 1, width );
                
                float h = 0;
                h += cur[left  + row * width]   + 0.5f * (dxdyImg.GetFloat4( row, left ).x);
                h += cur[right + row * width]   - 0.5f * (dxdyImg.GetFloat4( row, right ).x);
                h += cur[col   + up * width]    + 0.5f * (dxdyImg.GetFloat4( up, col ).y);
                h += cur[col   + down * width]  - 0.5f * (dxdyImg.GetFloat4( down, col ).y);
                
                next[col + row * width] = h / 4;

                //h += cur[left  + row * width]   + 0.5f * (dxdyImg.GetFloat4( row, left ).x);
                //h += cur[right + row * width]   - 0.5f * (dxdyImg.GetFloat4( row, col ).x);
                //h += cur[col   + up * width]    + 0.5f * (dxdyImg.GetFloat4( up, col ).y);
                //h += cur[col   + down * width]  - 0.5f * (dxdyImg.GetFloat4( row, col ).y);
                //next[col + row * width] = h / 4;

                //h += cur[right + row * width]   - 0.5f * (dxdyImg.GetFloat4( row, right ).x);
                //h += cur[col   + down * width]  - 0.5f * (dxdyImg.GetFloat4( down, col ).y);
                //next[col + row * width] = h / 2;
            }
        }
        std::swap( cur, next );
    }
}

void BuildDisplacement_WithEdges( const FloatImage2D& dxdyImg, const std::vector<FloatImage2D>& edgeImgs, float* h0, float* h1, uint32_t numIterations, float iterationMultiplier, uint32_t mipLevel )
{
    int width = (int)dxdyImg.width;
    int height = (int)dxdyImg.height;
    if ( width == 1 || height == 1 )
    {
        memset( h0, 0, width * height * sizeof( float ) );
    }
    else
    {
        int halfW = Max( width / 2, 1 );
        int halfH = Max( height / 2, 1 );
        FloatImage2D halfDxDyImg = dxdyImg.Resize( halfW, halfH );
        float scaleX = width / static_cast<float>( halfW );
        float scaleY = height / static_cast<float>( halfH );
        // update the 'rcpSize' from the original 'DxDyFromNormal( normal ) * rcpSize'
        halfDxDyImg.ForEachPixel( [&]( float* p )
            {
                p[0] *= scaleX;
                p[1] *= scaleY;
            });

        BuildDisplacement_WithEdges( halfDxDyImg, edgeImgs, h0, h1, numIterations, 2 * iterationMultiplier, mipLevel + 1 );

        stbir_resize_float_generic( h1, halfW, halfH, 0, h0, width, height, 0,
            1, -1, 0, STBIR_EDGE_WRAP, STBIR_FILTER_BOX, STBIR_COLORSPACE_LINEAR, NULL );
    }    
    
    float* cur = h0;
    float* next = h1;
    numIterations = static_cast<uint32_t>( Min( 1.0f, iterationMultiplier ) * numIterations );
    numIterations += numIterations % 2; // ensure odd number
    const FloatImage2D& edgeImg = edgeImgs[mipLevel];
    
    for ( uint32_t iter = 0; iter < numIterations; ++iter )
    {
        #pragma omp parallel for
        for ( int row = 0; row < height; ++row )
        {
            // wrap all edges
            uint32_t up = Wrap( row - 1, height );
            uint32_t down = Wrap( row + 1, height );
            
            for ( int col = 0; col < width; ++col )
            {
                uint32_t left = Wrap( col - 1, width );
                uint32_t right = Wrap( col + 1, width );
                
                vec4 w = edgeImg.GetFloat4( row, col ); 
                if ( mipLevel >= 0 )
                    w = vec4( 1.0f );

                float h = 0;
                h += w.x * (cur[left  + row * width]   + 0.5f * (dxdyImg.GetFloat4( row, left ).x));
                h += w.y * (cur[right + row * width]   - 0.5f * (dxdyImg.GetFloat4( row, right ).x));
                h += w.z * (cur[col   + up * width]    + 0.5f * (dxdyImg.GetFloat4( up, col ).y));
                h += w.w * (cur[col   + down * width]  - 0.5f * (dxdyImg.GetFloat4( down, col ).y));

                next[col + row * width] = h / Dot( w, vec4( 1.0f ) );
            }
        }
        std::swap( cur, next );
    }
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

enum class NormalCalcMethod : uint32_t
{
    CROSS,
    FORWARD,
    SOBEL,
    SCHARR,
    IMPROVED,
    ACCURATE,

    COUNT
};

FloatImage2D GetNormalMapFromHeightMap( const GenerationResults& generationData, NormalCalcMethod method )
{
    int width  = (int)generationData.heightMap.width;
    int height = (int)generationData.heightMap.height;
    float scale_H = (float)width;
    float scale_V = (float)height;

    FloatImage2D normalMap( width, height, 3 );
    for ( int row = 0; row < height; ++row )
    {
        int up = Wrap( row - 1, height );
        int down = Wrap( row + 1, height );
            
        for ( int col = 0; col < width; ++col )
        {
            int left = Wrap( col - 1, width );
            int right = Wrap( col + 1, width );

            float h_ML = generationData.GetH( row, left );
            float h_MM = generationData.GetH( row, col );
            float h_MR = generationData.GetH( row, right );
            float h_UL = generationData.GetH( up, left );
            float h_UM = generationData.GetH( up, col );
            float h_UR = generationData.GetH( up, right );
            float h_DL = generationData.GetH( down, left );
            float h_DM = generationData.GetH( down, col );
            float h_DR = generationData.GetH( down, right );
            
            vec3 normal = vec3( 0 );
            if ( method == NormalCalcMethod::CROSS )
            {
                float X = (h_ML - h_MR ) / 2.0f;
                float Y = (h_UM - h_DM ) / 2.0f;
                normal = vec3( scale_H * X, scale_V * Y, 1 );
            }
            else if ( method == NormalCalcMethod::SOBEL )
            {
                float X = ((h_UL - h_UR) + 2.0f * (h_ML - h_MR) + (h_DL - h_DR)) / 8.0f;
                float Y = ((h_UL - h_DL) + 2.0f * (h_UM - h_DM) + (h_UR - h_DR)) / 8.0f;
                normal = vec3( scale_H * X, scale_V * Y, 1 );
            }
            else if ( method == NormalCalcMethod::SCHARR )
            {
                float X = (3.0f * (h_UL - h_UR) + 10.0f * (h_ML - h_MR) + 3.0f * (h_DL - h_DR)) / 32.0f;
                float Y = (3.0f * (h_UL - h_DL) + 10.0f * (h_UM - h_DM) + 3.0f * (h_UR - h_DR)) / 32.0f;
                normal = vec3( scale_H * X, scale_V * Y, 1 );
            }
            else if ( method == NormalCalcMethod::FORWARD )
            {
                float X = (h_MM - h_MR );
                float Y = (h_MM - h_DM );
                normal = vec3( scale_H * X, scale_V * Y, 1 );
            }
            // https://wickedengine.net/2019/09/22/improved-normal-reconstruction-from-depth/
            else if ( method == NormalCalcMethod::IMPROVED )
            {
                const uint32_t best_Z_horizontal = abs(h_MR - h_MM) < abs(h_ML - h_MM) ? 1 : 2; // right, left
                const uint32_t best_Z_vertical = abs(h_DM - h_MM) < abs(h_UM - h_MM) ? 3 : 4; // down, up

                vec3 P0 = vec3( 0, 0, h_MM ); // center
                vec3 P1 = vec3( 0 );
                vec3 P2 = vec3( 0 );
                if ( best_Z_horizontal == 1 && best_Z_vertical == 4 ) // center, right, up
                {
                    P1 = vec3( 1, 0, h_MR ); // right
                    P2 = vec3( 0, -1, h_UM ); // up
                }
                else if ( best_Z_horizontal == 1 && best_Z_vertical == 3 ) // center, down, right
                {
                    P1 = vec3( 0, 1, h_DM ); // down
                    P2 = vec3( 1, 0, h_MR ); // right
                }
                else if ( best_Z_horizontal == 2 && best_Z_vertical == 4 ) // center, up, left
                {
                    P1 = vec3( 0, -1, h_UM ); // up
                    P2 = vec3( -1, 0, h_ML ); // left
                }
                else // center, left, down
                {
                    P1 = vec3( -1, 0, h_ML ); // left
                    P2 = vec3( 0, 1, h_DM ); // down
                }
                P1.x /= scale_H;
                P2.x /= scale_H;
                P1.y /= scale_V;
                P2.y /= scale_V;

                normal = Cross( P2 - P0, P1 - P0 );
            }
            // https://atyuwen.github.io/posts/normal-reconstruction/
            else if ( method == NormalCalcMethod::ACCURATE )
            {
                float h_ML2 = generationData.GetH( row, Wrap( col - 2, width ) );
                float h_MR2 = generationData.GetH( row, Wrap( col + 2, width ) );

                float dLeft  = abs( 2.0f * h_ML - h_ML2 - h_MM );
                float dRight = abs( 2.0f * h_MR - h_MR2 - h_MM );
                vec3 dpdx = dLeft < dRight ? vec3( 1.0f / scale_H, 0, h_MM - h_ML ) :
                                             vec3( 1.0f / scale_H, 0, h_MR - h_MM );

                float h_UM2 = generationData.GetH( row, Wrap( row - 2, height ) );
                float h_DM2 = generationData.GetH( row, Wrap( row + 2, height ) );
                float dUp   = abs( 2.0f * h_UM - h_UM2 - h_MM );
                float dDown = abs( 2.0f * h_DM - h_DM2 - h_MM );
                vec3 dpdy = dUp < dDown ? vec3( 0, 1.0f / scale_V, h_MM - h_UM ) :
                                          vec3( 0, 1.0f / scale_V, h_DM - h_MM );

                normal = Cross( dpdx, dpdy );
            }

            normal = Normalize( normal );
            normalMap.SetFromFloat4( row, col, vec4( normal, 0 ) );
        }
    }

    return normalMap;
}

GenerationResults GetHeightMapFromNormalMap( const FloatImage2D& normalMap, uint32_t iterations, const Options& options )
{
    auto startTime = PG::Time::GetTimePoint();

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
        if ( i == (183 * normalMap.width + 183) )
        {
            printf( "" );
        }
        vec3 normal = vec3( normalMap.GetFloat4( i ) );
        dxdyImg2.SetFromFloat4( i, vec4( DxDyFromNormal( normal ), 0, 0 ) );
    }
    dxdyImg2.Save( ROOT_DIR "dxdy.exr", ImageSaveFlags::KEEP_FLOATS_AS_32_BIT );
#endif

    FloatImage2D h0 = FloatImage2D( normalMap.width, normalMap.height, 1 );
    FloatImage2D h1 = FloatImage2D( normalMap.width, normalMap.height, 1 );
    BuildDisplacement( dxdyImg, h0.data.get(), h1.data.get(), iterations, 1.0f );

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
    for ( uint32_t i = 0; i < normalMap.width * normalMap.height; ++i )
    {
        float h = h1.data[i];
        h1.data[i] = (h - minH) * invScale;
    }

    auto stopTime = PG::Time::GetTimePoint();

    GenerationResults returnData;
    returnData.heightMap = h1;
    returnData.minH = minH;
    returnData.maxH = maxH;
    returnData.iterations = iterations;
    returnData.timeToGenerate = (float)PG::Time::GetElapsedTime( startTime, stopTime ) / 1000.0f;

    //if ( options.outputGenNormals )
    //{
    //    returnData.generatedNormalMap = GetNormalMapFromHeightMap( returnData );
    //    double MSE = FloatImageMSE( normalMap, returnData.generatedNormalMap );
    //    returnData.PSNR = (float)MSEToPSNR( MSE, 2.0 );
    //}

    return returnData;
}

GenerationResults GetHeightMapFromNormalMap_WithEdges( const FloatImage2D& normalMap, const std::vector<FloatImage2D>& edgeImgs, uint32_t iterations, const Options& options )
{
    auto startTime = PG::Time::GetTimePoint();

    FloatImage2D dxdyImg = FloatImage2D( normalMap.width, normalMap.height, 2 );
    vec2 rcpSize = { 1.0f / normalMap.width, 1.0f / normalMap.height };
    for ( uint32_t i = 0; i < normalMap.width * normalMap.height; ++i )
    {
        vec3 normal = vec3( normalMap.GetFloat4( i ) );
        dxdyImg.SetFromFloat4( i, vec4( DxDyFromNormal( normal ) * rcpSize, 0, 0 ) );
    }

    FloatImage2D h0 = FloatImage2D( normalMap.width, normalMap.height, 1 );
    FloatImage2D h1 = FloatImage2D( normalMap.width, normalMap.height, 1 );
    BuildDisplacement_WithEdges( dxdyImg, edgeImgs, h0.data.get(), h1.data.get(), iterations, 1.0f, 0 );

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
    for ( uint32_t i = 0; i < normalMap.width * normalMap.height; ++i )
    {
        float h = h1.data[i];
        h1.data[i] = (h - minH) * invScale;
    }

    auto stopTime = PG::Time::GetTimePoint();

    GenerationResults returnData;
    returnData.heightMap = h1;
    returnData.minH = minH;
    returnData.maxH = maxH;
    returnData.iterations = iterations;
    returnData.timeToGenerate = (float)PG::Time::GetElapsedTime( startTime, stopTime ) / 1000.0f;

    return returnData;
}

GenerationResults GetHeightMapFromNormalMap_LinearSolve( const FloatImage2D& normalMap, const Options& options )
{
    using namespace Eigen;

    auto startTime = PG::Time::GetTimePoint();

#define LIN( r, c ) (width * r + c)

    int width = (int)normalMap.width;
    int height = (int)normalMap.height;
    SparseMatrix<float> A( 2 * width * height, width * height );
    std::vector<Triplet<float>> triplets;
    triplets.reserve( 2 * width * height );
    for ( int row = 0; row < height; ++row )
    {
        int up = Wrap( row - 1, height );
        int down = Wrap( row + 1, height );
            
        for ( int col = 0; col < width; ++col )
        {
            int left = Wrap( col - 1, width );
            int right = Wrap( col + 1, width );

            triplets.emplace_back( 2 * LIN(row, col) + 0, LIN(row, col), 1.0f );
            triplets.emplace_back( 2 * LIN(row, col) + 0, LIN(row, right), -1.0f );
            triplets.emplace_back( 2 * LIN(row, col) + 1, LIN(row, col), 1.0f );
            triplets.emplace_back( 2 * LIN(row, col) + 1, LIN(down, col), -1.0f );

            //triplets.emplace_back( 2 * LIN(row, col) + 0, LIN(row, left), 0.5f );
            //triplets.emplace_back( 2 * LIN(row, col) + 0, LIN(row, right), -0.5f );
            //triplets.emplace_back( 2 * LIN(row, col) + 1, LIN(up, col), 0.5f );
            //triplets.emplace_back( 2 * LIN(row, col) + 1, LIN(down, col), -0.5f );
            
            //triplets.emplace_back( 2 * LIN(row, col) + 0, LIN(row, left), 0.25f );
            //triplets.emplace_back( 2 * LIN(row, col) + 0, LIN(row, col),  0.5f );
            //triplets.emplace_back( 2 * LIN(row, col) + 0, LIN(row, right), -0.75f );
            //triplets.emplace_back( 2 * LIN(row, col) + 1, LIN(up, col), 0.25f );
            //triplets.emplace_back( 2 * LIN(row, col) + 1, LIN(row, col), 0.5f );
            //triplets.emplace_back( 2 * LIN(row, col) + 1, LIN(down, col), -0.75f );
        }
    }
    A.setFromTriplets( triplets.begin(), triplets.end() );
    //A = A.transpose() * A;
    //std::cout << A << std::endl << std::endl;
    //std::cout << A.completeOrthogonalDecomposition().pseudoInverse() << std::endl;
    
    VectorXf b( 2 * width * height );
    vec2 rcpSize = { 1.0f / width, 1.0f / height };
    for ( int i = 0; i < width * height; ++i )
    {
        vec3 normal = vec3( normalMap.GetFloat4( i ) );
        vec2 dxdy = -DxDyFromNormal( normal ) * rcpSize;
        b( 2 * i + 0 ) = dxdy.x;
        b( 2 * i + 1 ) = dxdy.y;
    }

    //auto At = A.transpose();
    //auto AtAInv = (At * A).inverse();
    //auto X = AtAInv * (At * b);
    //VectorXf X = A.inverse() * b;
    //VectorXf X = A.completeOrthogonalDecomposition().pseudoInverse() * b;
    //VectorXf X = A.completeOrthogonalDecomposition().solve( b );
    //VectorXf X = (A.transpose() * A).ldlt().solve(A.transpose() * b);

    LeastSquaresConjugateGradient<SparseMatrix<float>> solver;
    solver.compute( A );
    if ( solver.info() != Success )
    {
        // decomposition failed
        LOG_ERR( "Decomposition failed" );
        return {};
    }
    VectorXf X;
    solver.setMaxIterations( 2000 );
    if ( true )
    {
        X = solver.solve( b );
    }
    else
    {
        GenerationResults relaxtionResults = GetHeightMapFromNormalMap( normalMap, 512, options );
        float scale = relaxtionResults.maxH - relaxtionResults.minH;
        VectorXf guess( width * height );
        for ( int i = 0; i < width * height; ++i )
        {
            guess(i) = relaxtionResults.heightMap.data[i] * scale + relaxtionResults.minH;
        }
        X = solver.solveWithGuess( b, guess );
    }
    //if ( solver.info() != Success )
    //{
    //    LOG_ERR( "Solver didn't converge (yet)" );
    //    return {};
    //}

    // normalize h1 to [0, 1] range
    float minH = FLT_MAX;
    float maxH = -FLT_MAX;
    for ( int i = 0; i < width * height; ++i )
    {
        float h = X(i);
        minH = Min( minH, h );
        maxH = Max( maxH, h );
    }
    
    float scale = maxH - minH;
    float invScale = scale > 0 ? 1.0f / scale : 1.0f;
    
    FloatImage2D heightMap( width, height, 1 );
    for ( int i = 0; i < width * height; ++i )
    {
        float h = X(i);
        h = (h - minH) * invScale;
        heightMap.SetFromFloat4( i, vec4( h, h, h, 1 ) );
    }

    auto stopTime = PG::Time::GetTimePoint();

    GenerationResults returnData;
    returnData.heightMap = heightMap;
    returnData.minH = minH;
    returnData.maxH = maxH;
    returnData.iterations = (uint32_t)solver.iterations();
    returnData.maxIterations = (uint32_t)solver.maxIterations();
    returnData.solverError = solver.error();
    returnData.timeToGenerate = (float)PG::Time::GetElapsedTime( startTime, stopTime ) / 1000.0f;

    return returnData;
}

void PackNormalMap( FloatImage2D& normalMap, bool flipY, bool flipX )
{
    for ( uint32_t i = 0; i < normalMap.width * normalMap.height; ++i )
    {
        vec3 normal = normalMap.GetFloat4( i );
        if ( flipY )
            normal.y *= -1;
        if ( flipX )
            normal.x *= -1;

        vec3 packedNormal = 0.5f * (normal + vec3( 1.0f ));
        normalMap.SetFromFloat4( i, vec4( packedNormal, 0 ) );
    }
}

static void DisplayHelp()
{
    auto msg =
        "Usage: NormalToHeight [options] PATH_TO_NORMAL_MAP\n"
        "Will generate height map(s) and will create and output them in a directory called '[PATH_TO_NORMAL_MAP]__autogen/'\n"
        "Note: this tool expects the normal map to have +X to the right, and +Y down. See the --flipY option if the +Y direction is up\n\n"
        "Options\n"
        "  -f, --flipY           Flip the Y direction on the normal map when loading it\n"
        "  -x, --flipX           Flip the X direction on the normal map when loading it\n"
        "  -g, --genNormalMap    Generate the normal map from the generated height map to compare to the original\n"
        "  -h, --help            Print this message and exit\n"
        "  -i, --iterations=N    How many iterations to use while generating the height map. Default is 1024\n"
        "  -r, --range           If specified, will output several images, with a range of iterations (ignoring the -i command).\n"
        "                        This can take a long time, especially for large images. Suggested on 1024 or smaller images\n"
        "  -s, --slopeScale=X    How much to scale the normals by, before generating the height map. Default is 1.0\n"
        "\n";

    LOG( "%s", msg );
}

static bool ParseCommandLineArgs( int argc, char** argv, Options& options )
{
    if ( argc == 1 )
    {
        DisplayHelp();
        return false;
    }

    static struct option long_options[] =
    {
        { "flipY",        no_argument,       0, 'f' },
        { "flipX",        no_argument,       0, 'x' },
        { "genNormalMap", no_argument,       0, 'g' },
        { "help",         no_argument,       0, 'h' },
        { "iterations",   required_argument, 0, 'i' },
        { "range",        no_argument,       0, 'r' },
        { "slopeScale",   required_argument, 0, 's' },
        { 0,              0,                 0,  0  }
    };

    int option_index  = 0;
    int c             = -1;
    while ( ( c = getopt_long( argc, argv, "fxghi:rs:", long_options, &option_index ) ) != -1 )
    {
        switch ( c )
        {
        case 'f':
            options.flipY = true;
            break;
        case 'x':
            options.flipX = true;
            break;
        case 'g':
            options.outputGenNormals = true;
            break;
        case 'h':
            DisplayHelp();
            return false;
        case 'i':
            options.numIterations = std::stoul( optarg );
            break;
        case 'r':
            options.rangeOfIterations = true;
            break;
        case 's':
            options.slopeScale = std::stof( optarg );
            break;
        default:
            LOG_ERR( "Invalid option, try 'NormalToHeight --help' for more information" );
            return false;
        }
    }

    if ( optind >= argc )
    {
        DisplayHelp();
        return false;
    }
    options.normalMapPath = argv[optind];

    return true;
}

bool Process( const Options& options )
{
    FloatImage2D normalMap = LoadNormalMap( options.normalMapPath, 1.0f, options.flipY, options.flipX );
    if ( !normalMap )
        return false;

    std::string outputDir = GetFilenameMinusExtension( options.normalMapPath ) + "_autogen/";
    CreateDirectory( outputDir );

    std::vector<uint32_t> iterationsList;
    if ( options.rangeOfIterations )
        iterationsList = { 32, 64, 128, 256, 512, 1024, 2048, 4096, 32768 };
    else
        iterationsList = { options.numIterations };

    std::vector<GenerationResults> generationResults( iterationsList.size() );
    for ( size_t i = 0; i < iterationsList.size(); ++i )
    {
        generationResults[i] = GetHeightMapFromNormalMap( normalMap, iterationsList[i], options );
        GenerationResults& result = generationResults[i];
        LOG( "Finished %ux%u image with %u iterations in %.2f seconds", normalMap.width, normalMap.height, iterationsList[i], result.timeToGenerate );
        LOG( "\tGenerated Height Map: MinH = %.3f, MaxH = %.3f, Range = %.3f", result.minH, result.maxH, result.maxH - result.minH );

        std::string outputPathBase = outputDir + GetFilenameStem( options.normalMapPath );
        if ( options.outputGenNormals )
        {
            for ( uint32_t nIdx = 0; nIdx < Underlying( NormalCalcMethod::COUNT ); ++nIdx )
            {
                FloatImage2D generatedNormalMap = GetNormalMapFromHeightMap( result, (NormalCalcMethod)nIdx );
                double MSE = FloatImageMSE( normalMap, generatedNormalMap );
                double PSNR = (float)MSEToPSNR( MSE, 2.0 );
                LOG( "\tGenerated Normal Method[%u] PSNR = %.3f", nIdx, PSNR );
                PackNormalMap( generatedNormalMap, options.flipY, options.flipX );
                generatedNormalMap.Save( outputPathBase + "_gn_" + std::to_string( nIdx ) + GetFileExtension( options.normalMapPath ) );
            }
            //LOG( "\tGenerated Normals PSNR = %.3f", result.PSNR );
            //PackNormalMap( result.generatedNormalMap, options.flipY, options.flipX );
            //result.generatedNormalMap.Save( outputPathBase + "_gn_" + std::to_string( iterationsList[i] ) + GetFileExtension( options.normalMapPath ) );
        }
        result.heightMap.Save( outputPathBase + "_gh_" + std::to_string( iterationsList[i] ) + GetFileExtension( options.normalMapPath ) );

        LOG( "" );
    }

    if ( options.rangeOfIterations )
    {
        // combine images into 1 big one for comparison, with a 4 pixel border between
        // if normal maps were generated from the height maps, add a 2nd row to this combined image with those
        const std::unordered_set<uint32_t> imgsToSave = { 32, 128, 512, 2048, 32768 };
        uint32_t imagesSaved = 0;
        const uint32_t totalImagesToSave = (uint32_t)imgsToSave.size();
        const vec4 borderColor = vec4( 0, 0, 1, 1 );
        const uint32_t borderWidth = 4;
        const uint32_t combinedWidth = normalMap.width * totalImagesToSave + borderWidth * (totalImagesToSave - 1);
        const uint32_t combinedHeight = options.outputGenNormals ? 2 * normalMap.height + borderWidth: normalMap.height;
        RawImage2D combinedImage( combinedWidth, combinedHeight, ImageFormat::R8_G8_B8_UNORM );

        if ( options.outputGenNormals )
        {
            // draw line between the 2 rows
            for ( uint32_t row = 0; row < borderWidth; ++row )
            {
                for ( uint32_t col = 0; col < combinedWidth; ++col )
                {
                    combinedImage.SetPixelFromFloat4( row, col, borderColor );
                }
            }
        }

        for ( size_t i = 0; i < iterationsList.size(); ++i )
        {
            if ( !imgsToSave.contains( iterationsList[i] ) )
                continue;

            const GenerationResults& data = generationResults[i];

            // copy height image into combined, on the 1st row
            for ( uint32_t row = 0; row < normalMap.height; ++row )
            {
                for ( uint32_t col = 0; col < normalMap.width; ++col )
                {
                    uint32_t combinedCol = col + normalMap.width * imagesSaved + borderWidth * imagesSaved;
                    float h = data.heightMap.GetFloat4( row, col ).x;
                    combinedImage.SetPixelFromFloat4( row, combinedCol, vec4( h, h, h, 1 ) );
                }
            }

            if ( options.outputGenNormals )
            {
                // copy normal image into combined, on the 2nd row
                for ( uint32_t row = 0; row < normalMap.height; ++row )
                {
                    for ( uint32_t col = 0; col < normalMap.width; ++col )
                    {
                        uint32_t combinedCol = col + normalMap.width * imagesSaved + borderWidth * imagesSaved;
                        vec3 packedNormal = data.generatedNormalMap.GetFloat4( row, col );
                        combinedImage.SetPixelFromFloat4(  normalMap.height + borderWidth + row, combinedCol, vec4( packedNormal, 1 ) );
                    }
                }
            }

            ++imagesSaved;

            // draw boundary line between images
            if ( imagesSaved != totalImagesToSave )
            {
                for ( uint32_t row = 0; row < combinedHeight; ++row )
                {
                    for ( uint32_t col = 0; col < borderWidth; ++col )
                    {
                        uint32_t combinedCol = col + normalMap.width * imagesSaved + borderWidth * (imagesSaved - 1);
                        combinedImage.SetPixelFromFloat4( row, combinedCol, borderColor );
                    }
                }
            }
        }
        std::string combinedName = outputDir + GetFilenameStem( options.normalMapPath ) + "_combinedView.png";
        combinedImage.Save( combinedName );
    }

    return true;
}

FloatImage2D DiffNormalMaps( const FloatImage2D& img1, const FloatImage2D& img2 )
{
    FloatImage2D res = FloatImage2D( img1.width, img2.height, 3 );
    for ( uint32_t row = 0; row < res.height; ++row )
    {
        for ( uint32_t col = 0; col < res.width; ++col )
        {
            vec3 n1 = img1.GetFloat4( row, col );
            vec3 n2 = img2.GetFloat4( row, col );
            float d = 1.0f - Max( 0.0f, Dot( n1, n2 ) );
            res.SetFromFloat4( row, col, vec4( d, d, d, 1.0f ) );
        }
    }

    return res;
}

bool Process_WithEdges( const Options& options )
{
    FloatImage2D normalMap = LoadNormalMap( options.normalMapPath, 1.0f, options.flipY, options.flipX );
    if ( !normalMap )
        return false;

    uint32_t numMips = (uint32_t)floor( log2( Max( normalMap.width, normalMap.height ) ) ) + 1;
    std::vector<FloatImage2D> normalMips( numMips );
    normalMips[0] = normalMap;
    for ( uint32_t mipLevel = 1; mipLevel < numMips; ++mipLevel )
    {
        const FloatImage2D& src = normalMips[mipLevel - 1];
        FloatImage2D& dst = normalMips[mipLevel];
        uint32_t halfW = Max( src.width / 2, 1u );
        uint32_t halfH = Max( src.height / 2, 1u );
        dst = src.Resize( halfW, halfH );
        for ( uint32_t i = 0; i < halfW * halfH; ++i )
        {
            vec3 normal = dst.GetFloat4( i );
            normal = Normalize( normal );
            dst.SetFromFloat4( i, vec4( normal, 0 ) );
        }
        //auto copy = dst.Clone();
        //PackNormalMap( copy, options.flipY, options.flipX );
        //copy.Save( ROOT_DIR "extra_images/nm_" + std::to_string( mipLevel ) + ".png" );
    }

    std::vector<FloatImage2D> edgeImgs( numMips );
    for ( uint32_t mipLevel = 0; mipLevel < numMips; ++mipLevel )
    {
        const FloatImage2D& normalMip = normalMips[mipLevel];
        int width = (int)normalMip.width;
        int height = (int)normalMip.height;
        edgeImgs[mipLevel] = FloatImage2D( normalMip.width, normalMip.height, 4 );
        auto save = FloatImage2D( normalMip.width, normalMip.height, 4 );
        for ( int row = 0; row < height; ++row )
        {
            int up = Wrap( row - 1, height );
            int down = Wrap( row + 1, height );
            
            for ( int col = 0; col < width; ++col )
            {
                int left = Wrap( col - 1, width );
                int right = Wrap( col + 1, width );
                vec3 n = normalMip.GetFloat4( row, col );
                vec3 nLeft = normalMip.GetFloat4( row, left );
                vec3 nRight = normalMip.GetFloat4( row, right );
                vec3 nUp = normalMip.GetFloat4( up, col );
                vec3 nDown = normalMip.GetFloat4( down, col );

                float dLeft = Dot( n, nLeft );
                float dRight = Dot( n, nRight );
                float dUp = Dot( n, nUp );
                float dDown = Dot( n, nDown );
                edgeImgs[mipLevel].SetFromFloat4( row, col, vec4( dLeft, dRight, dUp, dDown ) );
                save.SetFromFloat4( row, col, vec4( 1.0f ) - vec4( dLeft, dRight, dUp, dDown ) );
            }
        }
        //save.Save( ROOT_DIR "extra_images/edges_" + std::to_string( mipLevel ) + ".png" );
    }

    std::string outputDir = GetFilenameMinusExtension( options.normalMapPath ) + "_autogen/";
    CreateDirectory( outputDir );

    std::vector<uint32_t> iterationsList;
    if ( options.rangeOfIterations )
        iterationsList = { 32, 64, 128, 256, 512, 1024, 2048, 4096, 32768 };
    else
        iterationsList = { options.numIterations };

    std::vector<GenerationResults> generationResults( iterationsList.size() );
    for ( size_t i = 0; i < iterationsList.size(); ++i )
    {
        generationResults[i] = GetHeightMapFromNormalMap_WithEdges( normalMap, edgeImgs, iterationsList[i], options );
        GenerationResults regularResults = GetHeightMapFromNormalMap( normalMap, iterationsList[i], options );
        GenerationResults& result = generationResults[i];
        LOG( "Finished %ux%u image with %u iterations in %.2f seconds", normalMap.width, normalMap.height, iterationsList[i], result.timeToGenerate );
        LOG( "\tGenerated Height Map: MinH = %.3f, MaxH = %.3f, Range = %.3f", result.minH, result.maxH, result.maxH - result.minH );

        std::string outputPathBase = outputDir + GetFilenameStem( options.normalMapPath );
        if ( options.outputGenNormals )
        {
            for ( uint32_t nIdx = 0; nIdx < Underlying( NormalCalcMethod::COUNT ); ++nIdx )
            {
                FloatImage2D generatedNormalMap = GetNormalMapFromHeightMap( result, (NormalCalcMethod)nIdx );
                FloatImage2D generatedNormalMapORIG = GetNormalMapFromHeightMap( regularResults, (NormalCalcMethod)nIdx );
                FloatImage2D diffImg = DiffNormalMaps( generatedNormalMap, generatedNormalMapORIG );
                FloatImage2D diffImg2 = DiffNormalMaps( generatedNormalMapORIG, normalMap );
                //diffImg.Save( ROOT_DIR "extra_images/diff_" + std::to_string( nIdx ) + ".exr" );
                diffImg2.Save( ROOT_DIR "extra_images/gold_diff_" + std::to_string( nIdx ) + ".exr" );

                double MSE = FloatImageMSE( normalMap, generatedNormalMap );
                double PSNR = (float)MSEToPSNR( MSE, 2.0 );
                LOG( "\tGenerated Normal Method[%u] PSNR = %.3f", nIdx, PSNR );
                PackNormalMap( generatedNormalMap, options.flipY, options.flipX );
                generatedNormalMap.Save( outputPathBase + "_gn_" + std::to_string( nIdx ) + GetFileExtension( options.normalMapPath ) );
            }
            //LOG( "\tGenerated Normals PSNR = %.3f", result.PSNR );
            //PackNormalMap( result.generatedNormalMap, options.flipY, options.flipX );
            //result.generatedNormalMap.Save( outputPathBase + "_gn_" + std::to_string( iterationsList[i] ) + GetFileExtension( options.normalMapPath ) );
        }
        result.heightMap.Save( outputPathBase + "_gh_" + std::to_string( iterationsList[i] ) + GetFileExtension( options.normalMapPath ) );

        LOG( "" );
    }

    return true;
}

bool Process_LinearSolve( const Options& options )
{
    using namespace Eigen;

    FloatImage2D normalMap = LoadNormalMap( options.normalMapPath, 1.0f, options.flipY, options.flipX );
    if ( !normalMap )
        return false;

    std::string outputDir = GetFilenameMinusExtension( options.normalMapPath ) + "_autogen/";
    CreateDirectory( outputDir );

    GenerationResults result = GetHeightMapFromNormalMap_LinearSolve( normalMap, options );
    LOG( "Finished %ux%u image in %.2f seconds", normalMap.width, normalMap.height, result.timeToGenerate );
    LOG( "\tSolver took %u / %u iterations, error: %f", result.iterations, result.maxIterations, result.solverError );
    LOG( "\tGenerated Height Map: MinH = %.3f, MaxH = %.3f, Range = %.3f", result.minH, result.maxH, result.maxH - result.minH );

    std::string outputPathBase = outputDir + GetFilenameStem( options.normalMapPath );
    if ( options.outputGenNormals )
    {
        for ( uint32_t nIdx = 0; nIdx < Underlying( NormalCalcMethod::COUNT ); ++nIdx )
        {
            FloatImage2D generatedNormalMap = GetNormalMapFromHeightMap( result, (NormalCalcMethod)nIdx );
            double MSE = FloatImageMSE( normalMap, generatedNormalMap );
            double PSNR = (float)MSEToPSNR( MSE, 2.0 );
            LOG( "\tGenerated Normal Method[%u] PSNR = %.3f", nIdx, PSNR );
            PackNormalMap( generatedNormalMap, options.flipY, options.flipX );
            generatedNormalMap.Save( outputPathBase + "_gln_" + std::to_string( nIdx ) + GetFileExtension( options.normalMapPath ) );
        }
        //LOG( "\tGenerated Normals PSNR = %.3f", result.PSNR );
        //PackNormalMap( result.generatedNormalMap, options.flipY, options.flipX );
        //result.generatedNormalMap.Save( outputPathBase + "_gn_" + std::to_string( iterationsList[i] ) + GetFileExtension( options.normalMapPath ) );
    }
    result.heightMap.Save( outputPathBase + "_glh_" + std::to_string( result.iterations ) + GetFileExtension( options.normalMapPath ) );

    return true;
}

int main( int argc, char** argv )
{
    Logger_Init();
    Logger_AddLogLocation( "stdout", stdout );
    Logger_AddLogLocation( "file", "log.txt" );

    //FloatImage2D dispMap;
    //dispMap.Load( ROOT_DIR "extra_images/rock_wall_10_disp_1k.png" );
    //FloatImage2D normalMap = LoadNormalMap( ROOT_DIR "normal_maps/rock_wall_10_1k.png", 1.0f, false, false );
    //
    //GenerationResults res;
    //res.heightMap = dispMap;
    ////for ( int i = 1; i <= 100; ++i )
    //{
    //    res.minH = 0;
    //    //res.maxH = i / 100.0f;
    //    res.maxH = 0.2f;
    //    res.generatedNormalMap = GetNormalMapFromHeightMap( res, NormalCalcMethod::CROSS );
    //    double MSE = FloatImageMSE( normalMap, res.generatedNormalMap );
    //    double PSNR = (float)MSEToPSNR( MSE, 2.0 );
    //    LOG( "Scale: %.2f, PSNR: %f", res.maxH, PSNR );
    //}
    //
    //PackNormalMap( res.generatedNormalMap, false, false );
    //res.generatedNormalMap.Save( ROOT_DIR "extra_images/rock_wall_10_disp_1k_gn.png" );
    //return 0;

    //int SIZE = 256;
    //FloatImage2D img( SIZE, SIZE, 3 );
    //for ( uint32_t row = 0; row < img.height; ++row )
    //{
    //    for ( uint32_t col = 0; col < img.width; ++col )
    //    {
    //        vec3 normal = vec3( 0, 0, 1 );
    //        //if ( img.width / 4 <= col && col < img.width / 2 )
    //        //    normal = Normalize( vec3( -1, 0, 1 ) );
    //        //else if ( img.width / 2 <= col && col < 3 * img.width / 4 )
    //        //    normal = Normalize( vec3( 1, 0, 1 ) );
    //        if ( img.height / 4 <= row && row < img.height / 2 )
    //            normal = Normalize( vec3( 0, -1, 1 ) );
    //        else if ( img.height / 2 <= row && row < 3 * img.height / 4 )
    //            normal = Normalize( vec3( 0, 1, 1 ) );
    //
    //        img.SetFromFloat4( row, col, vec4( normal, 0 ) );
    //    }
    //}
    //
    //PackNormalMap( img, false, false );
    //img.Save( ROOT_DIR "normal_maps/synthetic_ramp_v_256.png" );
    //return 0;
    

    //Options options = {};
    //if ( !ParseCommandLineArgs( argc, argv, options ) )
    //{
    //    return 0;
    //}
    //Process( options );

    HeightGenMethod m = HeightGenMethod::DEFAULT;
    std::vector<Options> options =
    {
        //{ ROOT_DIR "normal_maps/gray_rocks_nor_dx_1k.jpg", m, 512, 1.0f, false, false, true, false },
        //{ ROOT_DIR "normal_maps/pine_bark_nor_dx_1k.png", m, 512, 1.0f, false, false, true, false },
        { ROOT_DIR "normal_maps/rock_wall_08_nor_dx_1k.jpg", m, 512, 1.0f, false, false, true, false },
        //{ ROOT_DIR "normal_maps/rock_wall_10_1k.png", m, 512, 1.0f, false, false, true, false },
        //{ ROOT_DIR "normal_maps/synthetic_rings_512.png", m, 512, 1.0f, false, false, true, false },
        //{ ROOT_DIR "normal_maps/synthetic_shapes_1_512.png", m, 512, 1.0f, true, false, true, false },
        //{ ROOT_DIR "normal_maps/synthetic_ramp_h_256.png", m, 512, 1.0f, false, false, true, false },
        //{ ROOT_DIR "normal_maps/synthetic_ramp_v_256.png", m, 512, 1.0f, false, false, true, false },
    };
    for ( const Options& option : options )
    {
        Process_LinearSolve( option );
        Process( option );
    }

    Logger_Shutdown();

    return 0;
}
