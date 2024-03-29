#include "image.hpp"
#include "shared/filesystem.hpp"
#include "shared/logger.hpp"
#define STBI_NO_PIC
#define STBI_NO_PSD
#define STBI_NO_GIF
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "tiffio.h"
#include "tinyexr/tinyexr.h"

bool RawImage2D::Load( const std::string& filename, ImageLoadFlags loadFlags )
{
    std::string ext = GetFileExtension( filename );
    if ( ext == ".jpg" || ext == ".png" || ext == ".tga" || ext == ".bmp" || ext == ".ppm" || ext == ".pbm" || ext == ".hdr" )
    {
        FILE* file = fopen( filename.c_str(), "rb" );
        if ( file == NULL )
        {
            LOG_ERR( "RawImage2D::Load: Could not open file '%s'", filename.c_str() );
            return false;
        }

        int w, h, numChannels;
        ImageFormat startFormat;
        uint8_t* pixels;
        if ( ext == ".hdr" )
        {
            startFormat = ImageFormat::R32_FLOAT;
            pixels      = (uint8_t*)stbi_loadf( filename.c_str(), &w, &h, &numChannels, 0 );
        }
        else if ( stbi_is_16_bit_from_file( file ) )
        {
            startFormat = ImageFormat::R16_UNORM;
            pixels      = (uint8_t*)stbi_load_from_file_16( file, &w, &h, &numChannels, 0 );
        }
        else
        {
            startFormat = ImageFormat::R8_UNORM;
            pixels      = stbi_load_from_file( file, &w, &h, &numChannels, 0 );
        }
        if ( !pixels )
        {
            LOG_ERR( "RawImage2D::Load: error while loading image '%s'", filename.c_str() );
            return false;
        }
        data   = std::shared_ptr<uint8_t[]>( pixels, []( void* p ) { stbi_image_free( p ); } );
        width  = static_cast<uint32_t>( w );
        height = static_cast<uint32_t>( h );
        format = static_cast<ImageFormat>( (int)startFormat + numChannels - 1 );
    }
    else if ( ext == ".tif" || ext == ".tiff" )
    {
        // TIFFSetWarningHandler( NULL );
        // TIFFSetWarningHandlerExt( NULL );
        // TIFFSetErrorHandler( NULL );
        // TIFFSetErrorHandlerExt( NULL );

        TIFF* tif = TIFFOpen( filename.c_str(), "rb" );
        if ( tif )
        {
            if ( TIFFIsTiled( tif ) )
            {
                LOG_ERR( "Tiled TIF images not currently supported (image '%s')", filename.c_str() );
                TIFFClose( tif );
                return false;
            }

            uint16_t config;
            TIFFGetField( tif, TIFFTAG_PLANARCONFIG, &config );
            if ( config != PLANARCONFIG_CONTIG )
            {
                LOG_ERR( "Separate planar TIF images not currently supported (image '%s')", filename.c_str() );
                TIFFClose( tif );
                return false;
            }

            uint16_t numChannels, numBitsPerChannel;
            TIFFGetField( tif, TIFFTAG_SAMPLESPERPIXEL, &numChannels );
            TIFFGetField( tif, TIFFTAG_BITSPERSAMPLE, &numBitsPerChannel );
            if ( numBitsPerChannel != 8 && numBitsPerChannel != 16 && numBitsPerChannel != 32 )
            {
                LOG_ERR( "%u bit TIF images not currently supported (image '%s')", numBitsPerChannel, filename.c_str() );
                TIFFClose( tif );
                return false;
            }

            TIFFGetField( tif, TIFFTAG_IMAGEWIDTH, &width );
            TIFFGetField( tif, TIFFTAG_IMAGELENGTH, &height );

            ImageFormat format = ImageFormat::R8_UNORM;
            if ( numBitsPerChannel == 16 )
                format = ImageFormat::R16_UNORM;
            else if ( numBitsPerChannel == 32 )
                format = ImageFormat::R32_FLOAT;

            format = static_cast<ImageFormat>( Underlying( format ) + numChannels - 1 );
            *this  = RawImage2D( width, height, format );

            size_t stripSize   = TIFFStripSize( tif );
            uint32_t numStrips = TIFFNumberOfStrips( tif );
            uint32_t rowsPerStrip;
            TIFFGetFieldDefaulted( tif, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip );
            uint8_t* buf = static_cast<uint8_t*>( _TIFFmalloc( stripSize ) );

            uint32_t bytesPerPixel = numChannels * numBitsPerChannel / 8;
            uint32_t bytesPerRow   = width * bytesPerPixel;

            int totalRowsRead = 0;
            for ( uint32_t strip = 0; strip < numStrips; strip++ )
            {
                if ( TIFFReadEncodedStrip( tif, strip, buf, (tsize_t)-1 ) == -1 )
                {
                    LOG_ERR( "TIFFReadEncodedStrip error while processing TIF '%s'", filename.c_str() );
                    _TIFFfree( buf );
                    TIFFClose( tif );
                    return false;
                }

                for ( uint32_t r = 0; r < rowsPerStrip; ++r )
                {
                    if ( totalRowsRead < height )
                    {
                        memcpy( Raw() + totalRowsRead * bytesPerRow, buf + r * bytesPerRow, bytesPerRow );
                        ++totalRowsRead;
                    }
                }
            }
            _TIFFfree( buf );
            TIFFClose( tif );
        }
    }
    else if ( ext == ".exr" )
    {
        const char* err = nullptr;
        float* pixels;
        int w, h;
        bool success = LoadEXR( &pixels, &w, &h, filename.c_str(), &err ) == TINYEXR_SUCCESS;
        if ( !success )
        {
            LOG_ERR( "RawImage2D::Load: error while loading image '%s'", filename.c_str() );
            if ( err )
            {
                LOG_ERR( "\tTinyexr error '%s'", err );
            }
            return false;
        }
        data   = std::shared_ptr<uint8_t[]>( (uint8_t*)pixels, []( void* p ) { free( p ); } );
        width  = static_cast<uint32_t>( w );
        height = static_cast<uint32_t>( h );
        format = ImageFormat::R32_G32_B32_A32_FLOAT;
    }
    else
    {
        LOG_ERR( "Image filetype '%s' for image '%s' is not supported", ext.c_str(), filename.c_str() );
        return false;
    }

    if ( IsSet( loadFlags, ImageLoadFlags::FLIP_VERTICALLY ) )
    {
        int bytesPerRow = width * BitsPerPixel() / 8;
        uint8_t* tmpRow = new uint8_t[bytesPerRow];
        for ( int row = 0; row < height / 2; ++row )
        {
            uint8_t* upperRow = data.get() + row * bytesPerRow;
            uint8_t* lowerRow = data.get() + ( height - row - 1 ) * bytesPerRow;
            memcpy( tmpRow, upperRow, bytesPerRow );
            memcpy( upperRow, lowerRow, bytesPerRow );
            memcpy( lowerRow, tmpRow, bytesPerRow );
        }
        delete[] tmpRow;
    }

    return true;
}
