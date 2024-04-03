#include "normal_to_height_experimental.hpp"
#include "Eigen/Dense"
#include "Eigen/Sparse"

void BuildDisplacement_WithEdges( const FloatImage2D& dxdyImg, const std::vector<FloatImage2D>& edgeImgs, float* scratchH,
    float* outputH, uint32_t numIterations, float iterationMultiplier, uint32_t mipLevel = 0 )
{
    int width = dxdyImg.width;
    int height = dxdyImg.height;
    if ( width == 1 || height == 1 )
    {
        memset( outputH, 0, width * height * sizeof( float ) );
        return;
    }
    else
    {
        int halfW = Max( width / 2, 1 );
        int halfH = Max( height / 2, 1 );
        FloatImage2D halfDxDyImg = dxdyImg.Resize( halfW, halfH );
        float scaleX = width / static_cast<float>( halfW );
        float scaleY = height / static_cast<float>( halfH );
        // update the 'invSize' from the original 'DxDyFromNormal( normal ) * invSize'
        halfDxDyImg.ForEachPixel( [&]( float* p )
            {
                p[0] *= scaleX;
                p[1] *= scaleY;
            });

        BuildDisplacement_WithEdges( halfDxDyImg, edgeImgs, scratchH, outputH, numIterations, 2 * iterationMultiplier, mipLevel + 1 );

        stbir_resize_float_generic( outputH, halfW, halfH, 0, scratchH, width, height, 0,
            1, -1, 0, STBIR_EDGE_WRAP, STBIR_FILTER_BOX, STBIR_COLORSPACE_LINEAR, NULL );
    }    
    
    float* cur = scratchH;
    float* next = outputH;
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
                
                vec4 w = edgeImg.Get( row, col ); 
                if ( mipLevel >= 0 )
                    w = vec4( 1.0f );

                float h = 0;
                h += w.x * (cur[left  + row * width]   + 0.5f * (dxdyImg.Get( row, left ).x));
                h += w.y * (cur[right + row * width]   - 0.5f * (dxdyImg.Get( row, right ).x));
                h += w.z * (cur[col   + up * width]    + 0.5f * (dxdyImg.Get( up, col ).y));
                h += w.w * (cur[col   + down * width]  - 0.5f * (dxdyImg.Get( down, col ).y));

                next[col + row * width] = h / Dot( w, vec4( 1.0f ) );
            }
        }
        std::swap( cur, next );
    }
}

GenerationResults GetHeightMapFromNormalMap_WithEdges( const FloatImage2D& normalMap, uint32_t iterations, float iterationMultiplier )
{
    GenerationResults returnData;
    returnData.heightMap = GeneratedHeightMap( normalMap.width, normalMap.height );

    auto startTime = PG::Time::GetTimePoint();

    uint32_t numMips = (uint32_t)floor( log2( Max( normalMap.width, normalMap.height ) ) ) + 1;
    std::vector<FloatImage2D> normalMips( numMips );
    normalMips[0] = normalMap;
    for ( uint32_t mipLevel = 1; mipLevel < numMips; ++mipLevel )
    {
        const FloatImage2D& src = normalMips[mipLevel - 1];
        FloatImage2D& dst = normalMips[mipLevel];
        int halfW = Max( src.width / 2, 1 );
        int halfH = Max( src.height / 2, 1 );
        dst = src.Resize( halfW, halfH );
        for ( int i = 0; i < halfW * halfH; ++i )
        {
            vec3 normal = dst.Get( i );
            normal = Normalize( normal );
            dst.Set( i, normal );
        }
        //auto copy = dst.Clone();
        //PackNormalMap( copy, options.flipY, options.flipX );
        //copy.Save( ROOT_DIR "extra_images/nm_" + std::to_string( mipLevel ) + ".png" );
    }

    std::vector<FloatImage2D> edgeImgs( numMips );
    for ( uint32_t mipLevel = 0; mipLevel < numMips; ++mipLevel )
    {
        const FloatImage2D& normalMip = normalMips[mipLevel];
        int width = normalMip.width;
        int height = normalMip.height;
        edgeImgs[mipLevel] = FloatImage2D( normalMip.width, normalMip.height, 4 );
        //auto save = FloatImage2D( normalMip.width, normalMip.height, 4 );
        for ( int row = 0; row < height; ++row )
        {
            int up = Wrap( row - 1, height );
            int down = Wrap( row + 1, height );
            
            for ( int col = 0; col < width; ++col )
            {
                int left = Wrap( col - 1, width );
                int right = Wrap( col + 1, width );
                vec3 n = normalMip.Get( row, col );
                vec3 nLeft = normalMip.Get( row, left );
                vec3 nRight = normalMip.Get( row, right );
                vec3 nUp = normalMip.Get( up, col );
                vec3 nDown = normalMip.Get( down, col );

                float dLeft = Dot( n, nLeft );
                float dRight = Dot( n, nRight );
                float dUp = Dot( n, nUp );
                float dDown = Dot( n, nDown );
                edgeImgs[mipLevel].Set( row, col, vec4( dLeft, dRight, dUp, dDown ) );
                //save.Set( row, col, vec4( 1.0f ) - vec4( dLeft, dRight, dUp, dDown ) );
            }
        }
        //save.Save( ROOT_DIR "extra_images/edges_" + std::to_string( mipLevel ) + ".png" );
    }

    FloatImage2D dxdyImg = FloatImage2D( normalMap.width, normalMap.height, 2 );
    vec2 invSize = { 1.0f / normalMap.width, 1.0f / normalMap.height };
    for ( int i = 0; i < normalMap.width * normalMap.height; ++i )
    {
        vec3 normal = normalMap.Get( i );
        dxdyImg.Set( i, DxDyFromNormal( normal ) * invSize );
    }

    FloatImage2D scratchH = FloatImage2D( normalMap.width, normalMap.height, 1 );
    BuildDisplacement_WithEdges( dxdyImg, edgeImgs, scratchH.data.get(), returnData.heightMap.map.data.get(), iterations, iterationMultiplier );

    auto stopTime = PG::Time::GetTimePoint();

    returnData.heightMap.CalcMinMax();
    returnData.iterations = iterations;
    returnData.timeToGenerate = (float)PG::Time::GetElapsedTime( startTime, stopTime ) / 1000.0f;

    return returnData;
}

GenerationResults GetHeightMapFromNormalMap_LinearSolve( const FloatImage2D& normalMap, uint32_t iterations, bool linearSolveWithGuess )
{
    GenerationResults returnData;
    returnData.heightMap = GeneratedHeightMap( normalMap.width, normalMap.height );

    using namespace Eigen;

    auto startTime = PG::Time::GetTimePoint();

    #define LIN( r, c ) (width * r + c)

    int width = normalMap.width;
    int height = normalMap.height;
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

            // right-down difference
            triplets.emplace_back( 2 * LIN(row, col) + 0, LIN(row, col), 1.0f );
            triplets.emplace_back( 2 * LIN(row, col) + 0, LIN(row, right), -1.0f );
            triplets.emplace_back( 2 * LIN(row, col) + 1, LIN(row, col), 1.0f );
            triplets.emplace_back( 2 * LIN(row, col) + 1, LIN(down, col), -1.0f );

            // left-up difference
            //triplets.emplace_back( 2 * LIN(row, col) + 0, LIN(row, left), 1.0f );
            //triplets.emplace_back( 2 * LIN(row, col) + 0, LIN(row, col), -1.0f );
            //triplets.emplace_back( 2 * LIN(row, col) + 1, LIN(up, col), 1.0f );
            //triplets.emplace_back( 2 * LIN(row, col) + 1, LIN(row, col), -1.0f );

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
    
    VectorXf b( 2 * width * height );
    vec2 invSize = { 1.0f / width, 1.0f / height };
    for ( int i = 0; i < width * height; ++i )
    {
        vec3 normal = normalMap.Get( i );
        vec2 dxdy = -DxDyFromNormal( normal ) * invSize;
        b( 2 * i + 0 ) = dxdy.x;
        b( 2 * i + 1 ) = dxdy.y;
    }

    LeastSquaresConjugateGradient<SparseMatrix<float>> solver;
    solver.compute( A );
    if ( solver.info() != Success )
    {
        // decomposition failed
        LOG_ERR( "Decomposition failed" );
        return {};
    }
    VectorXf X;
    solver.setMaxIterations( iterations );
    if ( !linearSolveWithGuess )
    {
        X = solver.solve( b );
    }
    else
    {
        GenerationResults relaxtionResults = GetHeightMapFromNormalMap( normalMap, 512, 1.0f );
        VectorXf guess( width * height );
        for ( int i = 0; i < width * height; ++i )
        {
            guess(i) = relaxtionResults.heightMap.GetH( i );
        }
        X = solver.solveWithGuess( b, guess );
    }
    if ( solver.info() != Success )
    {
        LOG_WARN( "Solver didn't converge (yet)" );
    }

    for ( int i = 0; i < width * height; ++i )
    {
        float h = X(i);
        returnData.heightMap.map.Set( i, h );
    }

    auto stopTime = PG::Time::GetTimePoint();

    returnData.heightMap.CalcMinMax();
    returnData.iterations = (uint32_t)solver.iterations();
    returnData.solverError = solver.error();
    returnData.timeToGenerate = (float)PG::Time::GetElapsedTime( startTime, stopTime ) / 1000.0f;

    return returnData;
}
