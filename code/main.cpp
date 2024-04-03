#include "normal_to_height.hpp"
#include "normal_to_height_experimental.hpp"
#include "height_to_normal.hpp"
#include "getopt/getopt.h"
#include "shared/filesystem.hpp"
#include "shared/logger.hpp"
#include "shared/time.hpp"
#include <iostream>
#include <unordered_set>


struct Options
{
    std::string normalMapPath;
    bool flipY = false;
    bool flipX = false;
    float slopeScale = 1.0f;
    HeightGenMethod heightGenMethod = HeightGenMethod::DEFAULT;
    uint32_t numIterations = 1024;
    float iterationMultiplier = 0.25f;
    bool outputGenNormals = false;
    bool rangeOfIterations = false;

    // the options below are only available when using heightGenMethod == LINEAR_SYSTEM
    bool linearSolveWithGuess = true;
};

static void DisplayHelp()
{
    auto msg =
        "Usage: NormalToHeight [options] PATH_TO_NORMAL_MAP\n"
        "Will generate height map(s) and will create and output them in a directory called '[PATH_TO_NORMAL_MAP]__autogen/'\n"
        "Note: this tool expects the normal map to have +X to the right, and +Y down. See the --flipY option if the +Y direction is up\n\n"
        "Options\n"
        "  -g, --genNormalMap    Generate the normal map from the generated height map to compare to the original\n"
        "  -h, --help            Print this message and exit\n"
        "  -i, --iterations=N    How many iterations to use while generating the height map. Default is 512\n"
        "      --iterMultipier=X Only applicable with HeightGenMethod::RELAXATION*. The lower this is, the fewer iterations happen on the largest mips. (0, 1]\n"
        "  -m  --method          Which method to use to generate the height map (0 == RELAXATION, 1 == RELAXTION_EDGE_AWARE, 2 == LINEAR_SYSTEM). The outputted\n"
        "                            height maps will have '_gh_', '_ghe_', or '_ghl_' in their postfixes, respectively.\n"
        "  -r, --range           If specified, will output several images, with a range of iterations (ignoring the -i command).\n"
        "                        This can take a long time, especially for large images. Suggested on 1024 or smaller images\n"
        "  -s, --slopeScale=X    How much to scale the normals by, before generating the height map. Default is 1.0\n"
        "  -w, --withoutGuess    Only applicable with HeightGenMethod::LINEAR_SYSTEM. By default, it generates a height map using RELAXATION, and uses that\n"
        "                            as the initial guess for the solver\n"
        "  -x, --flipX           Flip the X direction on the normal map when loading it\n"
        "  -y, --flipY           Flip the Y direction on the normal map when loading it\n"
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
        { "genNormalMap",   no_argument,       0, 'g' },
        { "help",           no_argument,       0, 'h' },
        { "iterations",     required_argument, 0, 'i' },
        { "iterMultiplier", required_argument, 0, 1000 },
        { "method",         required_argument, 0, 'm' },
        { "range",          no_argument,       0, 'r' },
        { "slopeScale",     required_argument, 0, 's' },
        { "withoutGuess",   no_argument,       0, 'w' },
        { "flipX",          no_argument,       0, 'x' },
        { "flipY",          no_argument,       0, 'y' },
        { 0,                0,                 0,  0  }
    };

    int option_index  = 0;
    int c             = -1;
    while ( ( c = getopt_long( argc, argv, "ghi:m:rs:wxy", long_options, &option_index ) ) != -1 )
    {
        switch ( c )
        {
        case 'g':
            options.outputGenNormals = true;
            break;
        case 'h':
            DisplayHelp();
            return false;
        case 'i':
            options.numIterations = std::stoul( optarg );
            break;
        case 1000:
            options.iterationMultiplier = std::stof( optarg );
            break;
        case 'm':
            options.heightGenMethod = (HeightGenMethod)std::clamp( std::stoi( optarg ), 0, (int)HeightGenMethod::COUNT - 1 );
            break;
        case 'r':
            options.rangeOfIterations = true;
            break;
        case 's':
            options.slopeScale = std::stof( optarg );
            break;
        case 'w':
            options.linearSolveWithGuess = false;
            break;
        case 'x':
            options.flipX = true;
            break;
        case 'y':
            options.flipY = true;
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
    LOG( "Processing %s...", options.normalMapPath.c_str() );

    FloatImage2D normalMap = LoadNormalMap( options.normalMapPath, 1.0f, options.flipY, options.flipX );
    if ( !normalMap )
        return false;
    std::string normalMapExt = GetFileExtension( options.normalMapPath );

    std::string outputDir = GetFilenameMinusExtension( options.normalMapPath ) + "_autogen/";
    CreateDirectory( outputDir );

    std::vector<uint32_t> iterationsList;
    if ( options.rangeOfIterations )
        iterationsList = { 32, 64, 128, 256, 512, 1024, 2048, 4096, 32768 };
    else
        iterationsList = { options.numIterations };

    for ( size_t i = 0; i < iterationsList.size(); ++i )
    {
        std::string postfixH = ""; // for generated height maps
        std::string postfixN = ""; // for normal maps generated from the generated height maps
        GenerationResults result;
        if ( options.heightGenMethod == HeightGenMethod::RELAXATION )
        {
            postfixH = "_gh_";
            postfixN = "_gn_";
            result = GetHeightMapFromNormalMap( normalMap, iterationsList[i], options.iterationMultiplier );
        }
        else if ( options.heightGenMethod == HeightGenMethod::RELAXATION )
        {
            postfixH = "_ghe_";
            postfixN = "_gne_";
            result = GetHeightMapFromNormalMap_WithEdges( normalMap, iterationsList[i], options.iterationMultiplier );
        }
        else
        {
            postfixH = "_ghl_";
            postfixN = "_gnl_";
            result = GetHeightMapFromNormalMap_LinearSolve( normalMap, iterationsList[i], options.linearSolveWithGuess );
        }

        LOG( "Finished %dx%d image with %u iterations in %.3f seconds", normalMap.width, normalMap.height, result.iterations, result.timeToGenerate );
        LOG( "\tGenerated Height Map: Scale = %f, Bias = %f", result.heightMap.maxH - result.heightMap.minH, result.heightMap.minH );

        std::string outputPathBase = outputDir + GetFilenameStem( options.normalMapPath );
        if ( options.outputGenNormals )
        {
#if 0
            for ( uint32_t nIdx = 0; nIdx < Underlying( NormalCalcMethod::COUNT ); ++nIdx )
            {
                NormalCalcMethod method = (NormalCalcMethod)nIdx;
                FloatImage2D generatedNormalMap = GetNormalMapFromHeightMap( result.heightMap, method );
                double PSNR = CompareNormalMaps( normalMap, generatedNormalMap );
                LOG( "\tGenerated Normal Method %s PSNR = %f", NormalCalcMethodToStr( method ), PSNR );
                PackNormalMap( generatedNormalMap, options.flipY, options.flipX );
                generatedNormalMap.Save( outputPathBase + postfixN + NormalCalcMethodToStr( method ) + normalMapExt );
            }
#else
            FloatImage2D generatedNormalMap = GetNormalMapFromHeightMap( result.heightMap, NormalCalcMethod::CROSS );
            double PSNR = CompareNormalMaps( normalMap, generatedNormalMap );
            LOG( "\tGenerated Normals PSNR = %f", PSNR );

            //auto diffImg = DiffNormalMaps( normalMap, generatedNormalMap );
            //diffImg.Save( outputPathBase + postfixN + "diff_" + std::to_string( iterationsList[i] ) + ".exr" );

            PackNormalMap( generatedNormalMap, options.flipY, options.flipX );
            generatedNormalMap.Save( outputPathBase + postfixN + std::to_string( iterationsList[i] ) + normalMapExt );
#endif
        }

        if ( normalMapExt != ".exr" )
            result.heightMap.Pack0To1();

        result.heightMap.map.Save( outputPathBase + postfixH + std::to_string( iterationsList[i] ) + GetFileExtension( options.normalMapPath ) );
    }

    LOG( "" );

    return true;
}

int main( int argc, char** argv )
{
    Logger_Init();
    Logger_AddLogLocation( "stdout", stdout );
    Logger_AddLogLocation( "file", "log.txt" );

    Options options = {};
    if ( !ParseCommandLineArgs( argc, argv, options ) )
    {
        return 0;
    }
    Process( options );

    /*
    float slopeScale = 1.0f;
    HeightGenMethod heightGenMethod = HeightGenMethod::RELAXATION;
    uint32_t numIterations = 1024;
    float iterationMultiplier = 1.0f;
    bool outputGenNormals = true;
    bool rangeOfIterations = false;
    std::vector<Options> options =
    {
        //{ ROOT_DIR "normal_maps/gray_rocks_nor_dx_1k.jpg",   false, false },
        //{ ROOT_DIR "normal_maps/pine_bark_nor_dx_1k.png",    false, false },
        //{ ROOT_DIR "normal_maps/rock_wall_08_nor_dx_1k.jpg", false, false },
        { ROOT_DIR "normal_maps/rock_wall_10_1k.png",        false, false },
        //{ ROOT_DIR "normal_maps/synthetic_rings_512.png",    false, false },
        //{ ROOT_DIR "normal_maps/synthetic_shapes_1_512.png", true,  false },
        //{ ROOT_DIR "normal_maps/synthetic_ramp_h_256.png",   false, false },
    };
    for ( Options option : options )
    {
        option.slopeScale = slopeScale;
        option.heightGenMethod = heightGenMethod;
        option.numIterations = numIterations;
        option.iterationMultiplier = iterationMultiplier;
        option.outputGenNormals = outputGenNormals;
        option.rangeOfIterations = rangeOfIterations;

        Process( option );
    }
    */

    Logger_Shutdown();

    return 0;
}
