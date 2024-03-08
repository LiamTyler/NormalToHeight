#pragma once

#include "shared/platform_defines.hpp"
#include <cstdio>
#include <string>

enum class LogSeverity
{
    DEBUG,
    WARN,
    ERR
};

void Logger_Init();

void Logger_Shutdown();

void Logger_AddLogLocation( const std::string& name, FILE* file, bool useColors = true );

void Logger_AddLogLocation( const std::string& name, const std::string& filename );

void Logger_RemoveLogLocation( const std::string& name );

void Logger_ChangeLocationColored( const std::string& name, bool colored );

void Logger_Log( LogSeverity sev, const char* fmt, ... );

#if USING( SHIP_BUILD )

#define LOG( ... ) \
    do             \
    {              \
    } while ( 0 )
#define LOG_WARN( ... ) \
    do                  \
    {                   \
    } while ( 0 )
#define LOG_ERR( ... ) \
    do                 \
    {                  \
    } while ( 0 )

#else // #if USING( SHIP_BUILD )

// printf specification: https://www.cplusplus.com/reference/cstdio/printf/
enum class FormatCheckError
{
    NONE,
    TODO_NOT_IMPLEMENTED_YET,
    TOO_MANY_ARGS,
    TOO_FEW_ARGS,
    INVALID_FORMAT_STRING,
    EXPECTED_UNSIGNED_INTEGRAL,
    EXPECTED_SIGNED_INTEGRAL,
    INVALID_INTEGRAL_LENGTH,
    EXPECTED_FLOAT_OR_DOUBLE,
    INVALID_FLOAT_LENGTH,
    EXPECTED_LONG_DOUBLE,
    EXPECTED_CHAR_TYPE,
    EXPECTED_WIDE_CHAR_TYPE,
    INVALID_CHAR_LENGTH,
    EXPECTED_CHAR_POINTER,
    EXPECTED_WIDE_CHAR_POINTER,
    EXPECTED_POINTER,
    INVALID_STRING_LENGTH,

    INVALID_POINTER_LENGTH,

    EXPECTED_CHAR_LENGTH,
    EXPECTED_SHORT_LENGTH,
    EXPECTED_LONG_LENGTH,
    EXPECTED_LONG_LONG_LENGTH,
    EXPECTED_INTMAX_LENGTH,
    EXPECTED_SIZE_T_LENGTH,
    EXPECTED_PTRDIFF_LENGTH,
    EXPECTED_LONG_DOUBLE_LENGTH,
};

consteval size_t GetNextFormatSpecifier( std::string_view fmt )
{
    size_t pos = 0;
    while ( pos < fmt.size() )
    {
        if ( fmt[pos] == '%' )
        {
            ++pos;
            if ( pos == fmt.size() || fmt[pos] != '%' )
            {
                return pos;
            }
        }
        ++pos;
    }

    return pos;
}

consteval size_t ParseFlags( std::string_view fmt, size_t pos )
{
    switch ( fmt[pos] )
    {
    case '-':
    case '+':
    case ' ':
    case '#':
    case '0': return pos + 1;
    }

    return pos;
}

consteval size_t SkipPastNumbers( std::string_view fmt, size_t pos )
{
    while ( pos < fmt.size() && '0' <= fmt[pos] && fmt[pos] <= '9' )
    {
        ++pos;
    }
    return pos;
}

enum class LengthSpecifier
{
    NONE,
    CHAR,
    SHORT,
    LONG,
    LONG_LONG,
    INTMAX,
    SIZE_T,
    PTRDIFF,
    LONG_DOUBLE,
};

consteval LengthSpecifier GetLength( std::string_view fmt, size_t pos )
{
    switch ( fmt[pos] )
    {
    case 'h':
        if ( pos < fmt.size() && fmt[pos + 1] == 'h' )
            return LengthSpecifier::CHAR;
        return LengthSpecifier::SHORT;
    case 'l':
        if ( pos < fmt.size() && fmt[pos + 1] == 'l' )
            return LengthSpecifier::LONG_LONG;
        return LengthSpecifier::LONG;
    case 'j': return LengthSpecifier::INTMAX;
    case 'z': return LengthSpecifier::SIZE_T;
    case 't': return LengthSpecifier::PTRDIFF;
    case 'L': return LengthSpecifier::LONG_DOUBLE;
    }

    return LengthSpecifier::NONE;
}

consteval FormatCheckError CheckIntegralLength( LengthSpecifier fmtLen, size_t typeLen )
{
    switch ( fmtLen )
    {
    case LengthSpecifier::CHAR:
        if ( typeLen != sizeof( char ) )
            return FormatCheckError::EXPECTED_CHAR_LENGTH;
        break;
    case LengthSpecifier::SHORT:
        if ( typeLen != sizeof( short ) )
            return FormatCheckError::EXPECTED_SHORT_LENGTH;
        break;
    case LengthSpecifier::LONG:
        if ( typeLen != sizeof( long ) )
            return FormatCheckError::EXPECTED_LONG_LENGTH;
        break;
    case LengthSpecifier::LONG_LONG:
        if ( typeLen != sizeof( long long ) )
            return FormatCheckError::EXPECTED_LONG_LONG_LENGTH;
        break;
    case LengthSpecifier::INTMAX:
        if ( typeLen != sizeof( intmax_t ) )
            return FormatCheckError::EXPECTED_INTMAX_LENGTH;
        break;
    case LengthSpecifier::SIZE_T:
        if ( typeLen != sizeof( size_t ) )
            return FormatCheckError::EXPECTED_SIZE_T_LENGTH;
        break;
    case LengthSpecifier::PTRDIFF:
        if ( typeLen != sizeof( std::ptrdiff_t ) )
            return FormatCheckError::EXPECTED_PTRDIFF_LENGTH;
        break;
    case LengthSpecifier::LONG_DOUBLE: return FormatCheckError::INVALID_INTEGRAL_LENGTH;
    }

    return FormatCheckError::NONE;
}

template <typename... Args>
    requires( sizeof...( Args ) == 0 )
consteval FormatCheckError CheckFromat( std::string_view fmt )
{
    const size_t pos = GetNextFormatSpecifier( fmt );
    if ( fmt.size() > 0 && pos < fmt.size() )
    {
        return FormatCheckError::TOO_FEW_ARGS;
    }
    return FormatCheckError::NONE;
}

template <typename T, typename... Args>
consteval FormatCheckError CheckFromat( std::string_view fmt )
{
    using UT = relaxed_underlying_type<std::remove_cvref_t<std::remove_pointer_t<std::decay_t<T>>>>::type;

    size_t pos = GetNextFormatSpecifier( fmt );
    if ( pos >= fmt.size() )
    {
        return FormatCheckError::TOO_MANY_ARGS;
    }

    // handle flags
    pos = ParseFlags( fmt, pos );
    if ( pos >= fmt.size() )
        return FormatCheckError::INVALID_FORMAT_STRING;

    // handle width
    if ( fmt[pos] == '*' )
        return FormatCheckError::TODO_NOT_IMPLEMENTED_YET;
    pos = SkipPastNumbers( fmt, pos );
    if ( pos >= fmt.size() )
        return FormatCheckError::INVALID_FORMAT_STRING;

    // handle precision
    if ( fmt[pos] == '.' )
    {
        ++pos;
        if ( pos >= fmt.size() )
            return FormatCheckError::INVALID_FORMAT_STRING;

        if ( fmt[pos] == '*' )
            return FormatCheckError::TODO_NOT_IMPLEMENTED_YET;
        pos = SkipPastNumbers( fmt, pos );
        if ( pos >= fmt.size() )
            return FormatCheckError::INVALID_FORMAT_STRING;
    }

    LengthSpecifier length = GetLength( fmt, pos );
    pos += length != LengthSpecifier::NONE;
    pos += ( length == LengthSpecifier::CHAR || length == LengthSpecifier::LONG_LONG );

    if ( pos >= fmt.size() )
        return FormatCheckError::INVALID_FORMAT_STRING;

    FormatCheckError error = FormatCheckError::NONE;
    switch ( fmt[pos] )
    {
    case 'i':
    case 'd':
        if ( !std::is_integral_v<UT> || !std::is_signed_v<UT> )
            return FormatCheckError::EXPECTED_SIGNED_INTEGRAL;
        error = CheckIntegralLength( length, sizeof( UT ) );
        break;
    case 'o':
    case 'x':
    case 'X':
    case 'u':
        if ( !std::is_integral_v<UT> || !std::is_unsigned_v<UT> )
            return FormatCheckError::EXPECTED_UNSIGNED_INTEGRAL;
        error = CheckIntegralLength( length, sizeof( UT ) );
        break;
    case 'f':
    case 'F':
    case 'e':
    case 'E':
    case 'g':
    case 'G':
    case 'a':
    case 'A':
        if ( !std::is_floating_point_v<UT> )
            return FormatCheckError::EXPECTED_FLOAT_OR_DOUBLE;
        switch ( length )
        {
        case LengthSpecifier::NONE: break;
        case LengthSpecifier::LONG_DOUBLE:
            if ( !std::is_same_v<UT, long double> )
                return FormatCheckError::EXPECTED_LONG_DOUBLE;
            break;
        default: return FormatCheckError::INVALID_FLOAT_LENGTH;
        }
        break;
    case 'c':
        switch ( length )
        {
        case LengthSpecifier::NONE:
            if ( !std::is_same_v<UT, char> )
                return FormatCheckError::EXPECTED_CHAR_TYPE;
            break;
        case LengthSpecifier::LONG:
            if ( !std::is_same_v<UT, wchar_t> )
                return FormatCheckError::EXPECTED_WIDE_CHAR_TYPE;
            break;
        default: return FormatCheckError::INVALID_CHAR_LENGTH;
        }
        break;
    case 's':
        switch ( length )
        {
        case LengthSpecifier::NONE:
            if ( !(std::is_pointer_v<std::decay_t<T>> && std::is_same_v<UT, char>))
                return FormatCheckError::EXPECTED_CHAR_POINTER;
            break;
        case LengthSpecifier::LONG:
            if ( !std::is_pointer_v<std::decay_t<T>> && std::is_same_v<UT, wchar_t> )
                return FormatCheckError::EXPECTED_WIDE_CHAR_POINTER;
            break;
        default: return FormatCheckError::INVALID_STRING_LENGTH;
        }
        break;
    case 'p':
        switch ( length )
        {
        case LengthSpecifier::NONE:
            if ( !std::is_pointer_v<T> )
                return FormatCheckError::EXPECTED_POINTER;
            break;
        default: return FormatCheckError::INVALID_POINTER_LENGTH;
        }
        break;
    case 'n': return FormatCheckError::TODO_NOT_IMPLEMENTED_YET;
    default: return FormatCheckError::INVALID_FORMAT_STRING;
    }

    if ( error != FormatCheckError::NONE )
        return error;

    return CheckFromat<Args...>( fmt.substr( pos ) );
}

#define CHECK_FORMAT( fmt, ... )                                                                                                          \
    constexpr FormatCheckError error = CheckFromat<PG_WRAP_VARGS( __VA_ARGS__ )>( std::string_view( fmt ) );                              \
    static_assert( error != FormatCheckError::TODO_NOT_IMPLEMENTED_YET,                                                                   \
        "TODO: haven't implemented the format check for this specifier yet (* or n)" );                                                   \
    static_assert( error != FormatCheckError::TOO_FEW_ARGS, "Expected more arguments" );                                                  \
    static_assert( error != FormatCheckError::TOO_MANY_ARGS, "Expected fewer arguments" );                                                \
    static_assert( error != FormatCheckError::INVALID_FORMAT_STRING, "Invalid format string" );                                           \
    static_assert( error != FormatCheckError::EXPECTED_UNSIGNED_INTEGRAL, "Expected unsigned integral argument" );                        \
    static_assert( error != FormatCheckError::EXPECTED_SIGNED_INTEGRAL, "Expected signed integral argument" );                            \
    static_assert( error != FormatCheckError::INVALID_INTEGRAL_LENGTH, "'L' specifier not allowed on integral arguments" );               \
    static_assert( error != FormatCheckError::EXPECTED_FLOAT_OR_DOUBLE, "Expected a float, double, or long double arg" );                 \
    static_assert( error != FormatCheckError::INVALID_FLOAT_LENGTH, "Only the long double 'L' specifier is allowed for floating types" ); \
    static_assert( error != FormatCheckError::EXPECTED_LONG_DOUBLE, "Expected long double arg" );                                         \
    static_assert( error != FormatCheckError::EXPECTED_CHAR_TYPE, "Expected char arg" );                                                  \
    static_assert( error != FormatCheckError::EXPECTED_WIDE_CHAR_TYPE, "Expected wchar_t arg" );                                          \
    static_assert( error != FormatCheckError::INVALID_CHAR_LENGTH, "Only wide char length specifier 'l' allowed for chars" );             \
    static_assert( error != FormatCheckError::EXPECTED_CHAR_POINTER, "Expected char pointer" );                                           \
    static_assert( error != FormatCheckError::EXPECTED_WIDE_CHAR_POINTER, "Expected wchar_t pointer" );                                   \
    static_assert( error != FormatCheckError::INVALID_STRING_LENGTH, "Only wide char length specifier 'l' allowed for strings" );         \
    static_assert( error != FormatCheckError::EXPECTED_POINTER, "Expected pointer arg" );                                                 \
    static_assert( error != FormatCheckError::INVALID_POINTER_LENGTH, "No length specifiers are allowed for pointers" );                  \
    static_assert( error != FormatCheckError::EXPECTED_CHAR_LENGTH, "Expected char sized arg" );                                          \
    static_assert( error != FormatCheckError::EXPECTED_SHORT_LENGTH, "Expected short sized arg" );                                        \
    static_assert( error != FormatCheckError::EXPECTED_LONG_LENGTH, "Expected long sized arg" );                                          \
    static_assert( error != FormatCheckError::EXPECTED_LONG_LONG_LENGTH, "Expected long long sized arg" );                                \
    static_assert( error != FormatCheckError::EXPECTED_INTMAX_LENGTH, "Expected intmax_t sized arg" );                                    \
    static_assert( error != FormatCheckError::EXPECTED_SIZE_T_LENGTH, "Expected size_t sized arg" );                                      \
    static_assert( error != FormatCheckError::EXPECTED_PTRDIFF_LENGTH, "Expected ptrdiff_t sized arg" );                                  \
    static_assert( error != FormatCheckError::EXPECTED_LONG_DOUBLE_LENGTH, "Expected long double sized arg" )

#define LOG( fmt, ... )                                                   \
    do                                                                    \
    {                                                                     \
        CHECK_FORMAT( fmt, __VA_ARGS__ );                                 \
        Logger_Log( LogSeverity::DEBUG, fmt __VA_OPT__(, ) __VA_ARGS__ ); \
    } while ( false )

#define LOG_WARN( fmt, ... )                                             \
    do                                                                   \
    {                                                                    \
        CHECK_FORMAT( fmt, __VA_ARGS__ );                                \
        Logger_Log( LogSeverity::WARN, fmt __VA_OPT__(, ) __VA_ARGS__ ); \
    } while ( false )

#define LOG_ERR( fmt, ... )                                             \
    do                                                                  \
    {                                                                   \
        CHECK_FORMAT( fmt, __VA_ARGS__ );                               \
        Logger_Log( LogSeverity::ERR, fmt __VA_OPT__(, ) __VA_ARGS__ ); \
    } while ( false )

#endif // #else // #if USING( SHIP_BUILD )
