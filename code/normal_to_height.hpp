#pragma once

#include "image.hpp"
#include "shared/logger.hpp"
#include "shared/time.hpp"
#include "stb/stb_image_resize.h"

enum class HeightGenMethod
{
    RELAXATION = 0,
    RELAXTION_EDGE_AWARE = 1,
    LINEAR_SYSTEM = 2,

    COUNT = 3,
    DEFAULT = RELAXATION
};

struct GeneratedHeightMap
{
    GeneratedHeightMap() = default;
    GeneratedHeightMap( int width, int height )
    {
        map = FloatImage2D( width, height, 1 );
    }

    FloatImage2D map;
    float minH = FLT_MAX;
    float maxH = -FLT_MAX;
    float scale = 1;
    float bias = 0;

    void CalcMinMax();
    void Pack0To1();
    void Unpack0To1();
    float GetH( int pixelIndex ) const;
    float GetH( int row, int col ) const;
};

struct GenerationResults
{
    GeneratedHeightMap heightMap;
    uint32_t iterations;
    float timeToGenerate;

    // the parmeters below are only available when using heightGenMethod == LINEAR_SYSTEM
    float solverError;
};

static inline int Wrap( int v, int maxVal )
{
    if ( v < 0 ) return v + maxVal;
    else if ( v >= maxVal ) return 0;
    else return v;
}

vec2 DxDyFromNormal( vec3 normal );

GenerationResults GetHeightMapFromNormalMap( const FloatImage2D& normalMap, uint32_t iterations, float iterationMultiplier = 1.0f );