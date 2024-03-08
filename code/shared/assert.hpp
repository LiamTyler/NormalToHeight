#pragma once

#include "platform_defines.hpp"
#include <cstdio>
#include <cstdlib>

#if !USING( SHIP_BUILD )

#define PG_ASSERT( ... ) _PG_VA_SELECT( _PG_ASSERT, __VA_ARGS__ )

#define _PG_ASSERT_START( X ) \
    if ( !( X ) )             \
    {                         \
        printf( "Failed assertion: (%s) at line %d in file %s\n", #X, __LINE__, __FILE__ );

#define _PG_ASSERT_END() \
    fflush( stdout );    \
    abort();             \
    }

#define _PG_ASSERT1( X )  \
    _PG_ASSERT_START( X ) \
    _PG_ASSERT_END()

#define _PG_ASSERT2( X, MSG )              \
    _PG_ASSERT_START( X )                  \
    printf( "Assert message: " MSG "\n" ); \
    _PG_ASSERT_END()

#define _PG_ASSERT3( X, FMT, A1 )              \
    _PG_ASSERT_START( X )                      \
    printf( "Assert message: " FMT "\n", A1 ); \
    _PG_ASSERT_END()

#define _PG_ASSERT4( X, FMT, A1, A2 )              \
    _PG_ASSERT_START( X )                          \
    printf( "Assert message: " FMT "\n", A1, A2 ); \
    _PG_ASSERT_END()

#define _PG_ASSERT5( X, FMT, A1, A2, A3 )              \
    _PG_ASSERT_START( X )                              \
    printf( "Assert message: " FMT "\n", A1, A2, A3 ); \
    _PG_ASSERT_END()

#define _PG_ASSERT6( X, FMT, A1, A2, A3, A4 )              \
    _PG_ASSERT_START( X )                                  \
    printf( "Assert message: " FMT "\n", A1, A2, A3, A4 ); \
    _PG_ASSERT_END()

#define _PG_ASSERT7( X, FMT, A1, A2, A3, A4, A5 )              \
    _PG_ASSERT_START( X )                                      \
    printf( "Assert message: " FMT "\n", A1, A2, A3, A4, A5 ); \
    _PG_ASSERT_END()

#define _PG_ASSERT8( X, FMT, A1, A2, A3, A4, A5, A6 )              \
    _PG_ASSERT_START( X )                                          \
    printf( "Assert message: " FMT "\n", A1, A2, A3, A4, A5, A6 ); \
    _PG_ASSERT_END()

#define _PG_ASSERT9( X, FMT, A1, A2, A3, A4, A5, A6, A7 )              \
    _PG_ASSERT_START( X )                                              \
    printf( "Assert message: " FMT "\n", A1, A2, A3, A4, A5, A6, A7 ); \
    _PG_ASSERT_END()

#define _PG_ASSERT10( X, FMT, A1, A2, A3, A4, A5, A6, A7, A8 )             \
    _PG_ASSERT_START( X )                                                  \
    printf( "Assert message: " FMT "\n", A1, A2, A3, A4, A5, A6, A7, A8 ); \
    _PG_ASSERT_END()

#define _PG_ASSERT11( X, FMT, A1, A2, A3, A4, A5, A6, A7, A8, A9 )             \
    _PG_ASSERT_START( X )                                                      \
    printf( "Assert message: " FMT "\n", A1, A2, A3, A4, A5, A6, A7, A8, A9 ); \
    _PG_ASSERT_END()

#define _PG_ASSERT12( X, FMT, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10 )             \
    _PG_ASSERT_START( X )                                                           \
    printf( "Assert message: " FMT "\n", A1, A2, A3, A4, A5, A6, A7, A8, A9, A10 ); \
    _PG_ASSERT_END()

#else // #if !USING( SHIP_BUILD )

#define PG_ASSERT( ... ) \
    do                   \
    {                    \
    } while ( 0 )

#endif // #else // #if !USING( SHIP_BUILD )
