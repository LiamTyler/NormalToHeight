#include "image.hpp"
#include "shared/assert.hpp"
#include "shared/filesystem.hpp"
#include "shared/float_conversions.hpp"
#include "shared/logger.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include "tiffio.h"
#include "tinyexr/tinyexr.h"
#include <memory>

static bool SaveExr( const std::string& filename, int width, int height, int numChannels, float* pixels, bool saveAsFP16 )
{
    if ( numChannels == 2 )
    {
        LOG_ERR( "Can't Currently the EXR save code doesn't support 2-channel EXRs. Only 1, 3, or 4." );
        return false;
    }

    const char* err;
    int ret = SaveEXR( pixels, width, height, numChannels, saveAsFP16, filename.c_str(), &err );

    if ( ret != TINYEXR_SUCCESS )
    {
        LOG_ERR( "SaveExr error: '%s'", err );
        return false;
    }

    return true;
}

bool RawImage2D::Save( const std::string& filename, ImageSaveFlags saveFlags ) const
{
    uint32_t numChannels = NumChannels();
    std::string ext      = GetFileExtension( filename );
    bool saveSuccessful  = false;
    if ( ext == ".jpg" || ext == ".png" || ext == ".tga" || ext == ".bmp" )
    {
        RawImage2D imgToSave = *this;
        if ( !IsFormat8BitUnorm( format ) )
        {
            imgToSave = Convert( static_cast<ImageFormat>( Underlying( ImageFormat::R8_UNORM ) + numChannels - 1 ) );
        }

        int ret = 1;
        switch ( ext[1] )
        {
        case 'p': ret = stbi_write_png( filename.c_str(), width, height, numChannels, imgToSave.Raw(), width * numChannels ); break;
        case 'j': ret = stbi_write_jpg( filename.c_str(), width, height, numChannels, imgToSave.Raw(), 95 ); break;
        case 'b': ret = stbi_write_bmp( filename.c_str(), width, height, numChannels, imgToSave.Raw() ); break;
        case 't': ret = stbi_write_tga( filename.c_str(), width, height, numChannels, imgToSave.Raw() ); break;
        default: ret = 0;
        }
        saveSuccessful = ret != 0;
    }
    else if ( ext == ".hdr" )
    {
        RawImage2D imgToSave = *this;
        if ( !IsFormat32BitFloat( format ) )
        {
            imgToSave = Convert( static_cast<ImageFormat>( Underlying( ImageFormat::R32_FLOAT ) + numChannels - 1 ) );
        }
        saveSuccessful = 0 != stbi_write_hdr( filename.c_str(), width, height, numChannels, imgToSave.Raw<float>() );
    }
    else if ( ext == ".exr" )
    {
        RawImage2D imgToSave = *this;
        if ( !IsFormat32BitFloat( format ) )
        {
            imgToSave = Convert( static_cast<ImageFormat>( Underlying( ImageFormat::R32_FLOAT ) + numChannels - 1 ) );
        }
        bool saveAsFP16 = !IsSet( saveFlags, ImageSaveFlags::KEEP_FLOATS_AS_32_BIT );
        saveSuccessful  = SaveExr( filename, width, height, numChannels, imgToSave.Raw<float>(), saveAsFP16 );
    }
    else
    {
        LOG_ERR( "RawImage2D::Save: Unrecognized image extension when saving file '%s'", filename.c_str() );
    }

    if ( !saveSuccessful )
    {
        LOG_ERR( "Failed to save image '%s'", filename.c_str() );
    }

    return saveSuccessful;
}
