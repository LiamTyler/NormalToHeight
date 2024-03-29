#include "height_to_normal.hpp"

const char* NormalCalcMethodToStr( NormalCalcMethod method )
{
    static const char* names[] =
    {
        "cross",    // CROSS
        "forward"   // FORWARD
        "sobel",    // SOBEL
        "scharr",   // SCHARR
        "improved", // IMPROVED
        "accurate", // ACCURATE
    };
    static_assert( Underlying( NormalCalcMethod::COUNT ) == 6, "dont forget to update this array when adding/deleting methods" );

    return names[Underlying( method )];
}

double CompareNormalMaps( const FloatImage2D& img1, const FloatImage2D& img2 )
{
    double mse = 0;
    for ( int row = 0; row < img1.height; ++row )
    {
        for ( int col = 0; col < img1.width; ++col )
        {
            vec3 n1 = img1.Get( row, col );
            vec3 n2 = img2.Get( row, col );
            float d = -Dot( n1, n2 ) + 1;
            mse += d * d;
        }
    }

    mse /= ( img1.width * img1.height );
    return 10.0 * log10( 2.0 * 2.0 / mse );
}

FloatImage2D DiffNormalMaps( const FloatImage2D& img1, const FloatImage2D& img2 )
{
    FloatImage2D res = FloatImage2D( img1.width, img2.height, 3 );
    for ( int row = 0; row < res.height; ++row )
    {
        for ( int col = 0; col < res.width; ++col )
        {
            vec3 n1 = img1.Get( row, col );
            vec3 n2 = img2.Get( row, col );
            float d = ( -Dot( n1, n2 ) + 1.0f ) / 2.0f;
            res.Set( row, col, vec3( d ) );
        }
    }

    return res;
}

FloatImage2D GetNormalMapFromHeightMap( const GeneratedHeightMap& heightMap, NormalCalcMethod method )
{
    int width  = heightMap.map.width;
    int height = heightMap.map.height;
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

            float h_ML = heightMap.GetH( row, left );
            float h_MM = heightMap.GetH( row, col );
            float h_MR = heightMap.GetH( row, right );
            float h_UL = heightMap.GetH( up, left );
            float h_UM = heightMap.GetH( up, col );
            float h_UR = heightMap.GetH( up, right );
            float h_DL = heightMap.GetH( down, left );
            float h_DM = heightMap.GetH( down, col );
            float h_DR = heightMap.GetH( down, right );
            
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
                float h_ML2 = heightMap.GetH( row, Wrap( col - 2, width ) );
                float h_MR2 = heightMap.GetH( row, Wrap( col + 2, width ) );

                float dLeft  = abs( 2.0f * h_ML - h_ML2 - h_MM );
                float dRight = abs( 2.0f * h_MR - h_MR2 - h_MM );
                vec3 dpdx = dLeft < dRight ? vec3( 1.0f / scale_H, 0, h_MM - h_ML ) :
                                             vec3( 1.0f / scale_H, 0, h_MR - h_MM );

                float h_UM2 = heightMap.GetH( row, Wrap( row - 2, height ) );
                float h_DM2 = heightMap.GetH( row, Wrap( row + 2, height ) );
                float dUp   = abs( 2.0f * h_UM - h_UM2 - h_MM );
                float dDown = abs( 2.0f * h_DM - h_DM2 - h_MM );
                vec3 dpdy = dUp < dDown ? vec3( 0, 1.0f / scale_V, h_MM - h_UM ) :
                                          vec3( 0, 1.0f / scale_V, h_DM - h_MM );

                normal = Cross( dpdx, dpdy );
            }

            normal = Normalize( normal );
            normalMap.Set( row, col, normal );
        }
    }

    return normalMap;
}

void PackNormalMap( FloatImage2D& normalMap, bool flipY, bool flipX )
{
    for ( int i = 0; i < normalMap.width * normalMap.height; ++i )
    {
        vec3 normal = normalMap.Get( i );
        if ( flipY )
            normal.y *= -1;
        if ( flipX )
            normal.x *= -1;

        vec3 packedNormal = 0.5f * (normal + vec3( 1.0f ));
        normalMap.Set( i, packedNormal );
    }
}