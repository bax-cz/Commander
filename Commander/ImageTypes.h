#pragma once

/*
	Supported image types - BMP, JPG, JPG2000, PCX, PNG, GIF, TGA, TIFF, ICO
*/

#include <memory>

#include "StringUtils.h"

namespace Commander
{
	struct ImageType
	{
		enum class EImageType {
			FMT_BMP,
			FMT_GIF,
			FMT_ICO,
			FMT_JPG,
			FMT_JP2K,
			FMT_PCX,
			FMT_PNG,
			FMT_TGA,
			FMT_TIFF,
			FMT_UNKNOWN
		};

		//
		// Check for known image types
		//
		static bool isKnownType( const std::wstring& fileName )
		{
			return getType( fileName ) != EImageType::FMT_UNKNOWN;
		}

		//
		// Get image type from file name
		//
		static EImageType getType( const std::wstring& fileName )
		{
			if( StringUtils::endsWith( fileName, L".bmp" ) )
				return EImageType::FMT_BMP;
			else if( StringUtils::endsWith( fileName, L".gif" ) )
				return EImageType::FMT_GIF;
			else if( StringUtils::endsWith( fileName, L".ico" ) )
				return EImageType::FMT_ICO;
			else if( StringUtils::endsWith( fileName, L".jpg" ) || StringUtils::endsWith( fileName, L".jpeg" ) )
				return EImageType::FMT_JPG;
			else if( StringUtils::endsWith( fileName, L".jp2" ) || StringUtils::endsWith( fileName, L".j2k" ) )
				return EImageType::FMT_JP2K;
			else if( StringUtils::endsWith( fileName, L".pcx" ) )
				return EImageType::FMT_PCX;
			else if( StringUtils::endsWith( fileName, L".png" ) )
				return EImageType::FMT_PNG;
			else if( StringUtils::endsWith( fileName, L".tga" ) )
				return EImageType::FMT_TGA;
			else if( StringUtils::endsWith( fileName, L".tif" ) || StringUtils::endsWith( fileName, L".tiff" ) )
				return EImageType::FMT_TIFF;

			return EImageType::FMT_UNKNOWN;
		}

		// do not instantiate this class/struct
		ImageType() = delete;
		ImageType( ImageType const& ) = delete;
		void operator=( ImageType const& ) = delete;
	};
}
