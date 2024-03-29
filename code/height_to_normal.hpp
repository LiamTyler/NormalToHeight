#pragma once

#include "image.hpp"
#include "normal_to_height.hpp"

enum class NormalCalcMethod : uint32_t
{
    CROSS,
    FORWARD,
    SOBEL,
    SCHARR,
    IMPROVED, // https://wickedengine.net/2019/09/22/improved-normal-reconstruction-from-depth/
    ACCURATE, // https://atyuwen.github.io/posts/normal-reconstruction/

    COUNT
};

const char* NormalCalcMethodToStr( NormalCalcMethod method );

// returns the PSNR of the dot product between img1 and img2
double CompareNormalMaps( const FloatImage2D& img1, const FloatImage2D& img2 );

// returns the image of the dot product between img1 and img2
FloatImage2D DiffNormalMaps( const FloatImage2D& img1, const FloatImage2D& img2 );

FloatImage2D GetNormalMapFromHeightMap( const GeneratedHeightMap& heightMap, NormalCalcMethod method );

void PackNormalMap( FloatImage2D& normalMap, bool flipY, bool flipX );