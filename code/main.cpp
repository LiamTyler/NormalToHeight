#include "image.hpp"
#include "getopt/getopt.h"
#include "shared/filesystem.hpp"
#include "shared/logger.hpp"
#include "shared/time.hpp"
#include "stb/stb_image_resize.h"
#include <unordered_set>

struct Options
{
    std::string normalMapPath;
    uint32_t numIterations = 1024;
    float slopeScale = 1.0f;
    bool flipY = false;
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
        for ( int row = 0; row < height; ++row )
        {
            // wrap all edges
            uint32_t up = Wrap( row - 1, height );
            uint32_t down = Wrap( row + 1, height );
            
            for ( int col = 0; col < width; ++col )
            {
                uint32_t left = Wrap( col - 1, width );
                uint32_t right = Wrap( col + 1, width );
                //vec2 currDxDy = vec2( dxdyImg.GetFloat4( row, col ) );

                float h = 0;
                h += cur[left  + row * width]   + (dxdyImg.GetFloat4( row, left ).x);
                h += cur[right + row * width]   - (dxdyImg.GetFloat4( row, right ).x);
                h += cur[col   + up * width]    + (dxdyImg.GetFloat4( up, col ).y);
                h += cur[col   + down * width]  - (dxdyImg.GetFloat4( down, col ).y);

                //h += cur[left  + row * width]   + (dxdyImg.GetFloat4( row, left ).x);
                //h += cur[right + row * width]   - (dxdyImg.GetFloat4( row, col ).x);
                //h += cur[col   + up * width]    + (dxdyImg.GetFloat4( up, col ).y);
                //h += cur[col   + down * width]  - (dxdyImg.GetFloat4( row, col ).y);

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

#define SOBEL 0

FloatImage2D GetNormalMapFromHeightMap( const GenerationResults& generationData )
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
            float h_MR = generationData.GetH( row, right );
            float h_UM = generationData.GetH( up, col );
            float h_DM = generationData.GetH( down, col );
        #if SOBEL
            float h_UL = generationData.GetH( up, left );
            float h_UR = generationData.GetH( up, right );
            float h_DL = generationData.GetH( down, left );
            float h_DR = generationData.GetH( down, right );

            float X = ((h_UL - h_UR) + 2.0f * (h_ML - h_MR) + (h_DL - h_DR)) / 8.0f;
            float Y = ((h_UL - h_DL) + 2.0f * (h_UM - h_DM) + (h_UR - h_DR)) / 8.0f;
            vec3 normal = vec3( scale_H * X, scale_V * Y, 1 );
        #else
            vec3 normal = vec3( scale_H * (h_ML - h_MR) / 2.0f, scale_V * (h_UM - h_DM) / 2.0f, 1 );
        #endif // SOBEL

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
    returnData.timeToGenerate = (float)PG::Time::GetElapsedTime( startTime, stopTime );

    if ( options.outputGenNormals )
    {
        returnData.generatedNormalMap = GetNormalMapFromHeightMap( returnData );
        double MSE = FloatImageMSE( normalMap, returnData.generatedNormalMap );
        returnData.PSNR = (float)MSEToPSNR( MSE, 2.0 );
    }

    return returnData;
}

void PackNormalMap( FloatImage2D& normalMap, bool sourceWasYDown )
{
    for ( uint32_t i = 0; i < normalMap.width * normalMap.height; ++i )
    {
        vec3 normal = normalMap.GetFloat4( i );
        if ( !sourceWasYDown )
            normal.y *= -1;

        vec3 packedNormal = 0.5f * (normal + vec3( 1.0f ));
        normalMap.SetFromFloat4( i, vec4( packedNormal, 0 ) );
    }
}

static void DisplayHelp()
{
    auto msg =
        "Usage: NormalToHeight [options] PATH_TO_NORMAL_MAP\n"
        "Will generate height map(s) and will create and output them in a directory called '[PATH_TO_NORMAL_MAP]__autogen/'\n"
        "Note: this tool expects the normal map to have +Y pointed down. If it's not, use the --flipY option\n\n"
        "Options\n"
        "  -f, --flipY           Flip the Y direction on the normal map when loading it\n"
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
        { "genNormalMap", no_argument,       0, 'g' },
        { "help",         no_argument,       0, 'h' },
        { "iterations",   required_argument, 0, 'i' },
        { "range",        no_argument,       0, 'r' },
        { "slopeScale",   required_argument, 0, 's' },
        { 0,              0,                 0,  0  }
    };

    int option_index  = 0;
    int c             = -1;
    while ( ( c = getopt_long( argc, argv, "fghi:rs:", long_options, &option_index ) ) != -1 )
    {
        switch ( c )
        {
        case 'f':
            options.flipY = true;
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

int main( int argc, char** argv )
{
    Logger_Init();
    Logger_AddLogLocation( "stdout", stdout );

    Options options = {};
    if ( !ParseCommandLineArgs( argc, argv, options ) )
    {
        return 0;
    }

    FloatImage2D normalMap = LoadNormalMap( options.normalMapPath, 1.0f, !options.flipY );
    if ( !normalMap )
        return 0;

    std::string outputDir = GetFilenameMinusExtension( options.normalMapPath ) + "_autogen/";
    CreateDirectory( outputDir );

    std::vector<uint32_t> iterationsList;
    if ( options.rangeOfIterations )
        iterationsList = { 32, 64, 128, 256, 512, 1024, 2048, 4096, 32768 };// 131072 };
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
            LOG( "\tGenerated Normals PSNR = %.3f", result.PSNR );
            PackNormalMap( result.generatedNormalMap, !options.flipY );
            result.generatedNormalMap.Save( outputPathBase + "_gn_" + std::to_string( iterationsList[i] ) + GetFileExtension( options.normalMapPath ) );
        }
        result.heightMap.Save( outputPathBase + "_gh_" + std::to_string( iterationsList[i] ) + GetFileExtension( options.normalMapPath ) );

        LOG( "" );
    }

    if ( options.rangeOfIterations )
    {
        // combine images into 1 big one for comparison, with a 4 pixel border between
        // if normal maps were generated from the height maps, add a 2nd row to this combined image with those
        const std::unordered_set<uint32_t> imgsToSave = { 32, 128, 512, 2048, 32768 }; //, 131072 };
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

    Logger_Shutdown();

    return 0;
}
