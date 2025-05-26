#pragma once

/*
	Supported archive types - RAR, ZIP, 7ZIP, TAR archives
*/

#include <memory>

#include "StringUtils.h"

#include "TypeRar.h"
#include "TypeZip.h"
#include "Type7zip.h"
#include "TypeBzip2.h"
#include "TypeGzip.h"
#include "TypeDmg.h"
#include "TypeIso.h"
#include "TypeTar.h"
#include "TypeXzip.h"

namespace Commander
{
	struct ArchiveType
	{
		enum class EArchType {
			FMT_RAR,
			FMT_ZIP,
			FMT_ARJ,
			FMT_7ZIP,
			FMT_NSIS,
			FMT_LZMA,
			FMT_PPMD,
			FMT_GZIP,
			FMT_XZIP,
			FMT_BZIP2,
			FMT_Z,
			FMT_LZH,
			FMT_CAB,
			FMT_WIM,
			FMT_DMG,
			FMT_ISO,
			FMT_NRG,
			FMT_RPM,
			FMT_DEB,
			FMT_XAR,
			FMT_TAR,
			FMT_TAR_GZIP,
			FMT_TAR_BZIP2,
			FMT_TAR_XZIP,
			FMT_UNKNOWN
		};

		//
		// Check for known archive types
		//
		static bool isKnownType( const std::wstring& fileName )
		{
			return getType( fileName ) != EArchType::FMT_UNKNOWN;
		}

		//
		// Get archive type from file name
		//
		static EArchType getType( const std::wstring& fileName )
		{
			if( StringUtils::endsWith( fileName, L".rar" ) )
				return EArchType::FMT_RAR;
			else if( StringUtils::endsWith( fileName, L".zip" ) )
				return EArchType::FMT_ZIP;
			else if( StringUtils::endsWith( fileName, L".7z" ) )
				return EArchType::FMT_7ZIP;
			else if( StringUtils::endsWith( fileName, L".arj" ) )
				return EArchType::FMT_ARJ;
			else if( StringUtils::endsWith( fileName, L".z" ) )
				return EArchType::FMT_Z;
			else if( StringUtils::endsWith( fileName, L".lzh" ) )
				return EArchType::FMT_LZH;
			else if( StringUtils::endsWith( fileName, L".cab" ) )
				return EArchType::FMT_CAB;
			else if( StringUtils::endsWith( fileName, L".wim" ) )
				return EArchType::FMT_WIM;
			else if( StringUtils::endsWith( fileName, L".dmg" ) )
				return EArchType::FMT_DMG;
			else if( StringUtils::endsWith( fileName, L".xar" ) )
				return EArchType::FMT_XAR;
			else if( StringUtils::endsWith( fileName, L".rpm" ) )
				return EArchType::FMT_RPM;
			else if( StringUtils::endsWith( fileName, L".deb" ) )
				return EArchType::FMT_DEB;
			else if( StringUtils::endsWith( fileName, L".nsis" ) )
				return EArchType::FMT_NSIS;
			else if( StringUtils::endsWith( fileName, L".lzma" ) )
				return EArchType::FMT_LZMA;
			else if( StringUtils::endsWith( fileName, L".ppmd" ) )
				return EArchType::FMT_PPMD;
			else if( StringUtils::endsWith( fileName, L".iso" ) )
				return EArchType::FMT_ISO;
			else if( StringUtils::endsWith( fileName, L".nrg" ) )
				return EArchType::FMT_NRG;
			else if( StringUtils::endsWith( fileName, L".tar" ) )
				return EArchType::FMT_TAR;
			else if( StringUtils::endsWith( fileName, L".tar.gz" ) || StringUtils::endsWith( fileName, L".tgz" ) )
				return EArchType::FMT_TAR_GZIP;
			else if( StringUtils::endsWith( fileName, L".tar.bz2" ) )
				return EArchType::FMT_TAR_BZIP2;
			else if( StringUtils::endsWith( fileName, L".tar.xz" ) || StringUtils::endsWith( fileName, L".txz" ) )
				return EArchType::FMT_TAR_XZIP;
			else if( StringUtils::endsWith( fileName, L".gz" ) || StringUtils::endsWith( fileName, L".gzip" ) ) // plain .gz, .xz after tar
				return EArchType::FMT_GZIP;
			else if( StringUtils::endsWith( fileName, L".bz2" ) )
				return EArchType::FMT_BZIP2;
			else if( StringUtils::endsWith( fileName, L".xz" ) )
				return EArchType::FMT_XZIP;

			return EArchType::FMT_UNKNOWN;
		}

		//
		// Instantiate particular archiver according to extension
		//
		static std::unique_ptr<CArchiver> createArchiver( const std::wstring& fileName )
		{
			std::unique_ptr<CArchiver> upArchiver;

			switch( getType( fileName ) )
			{
			case EArchType::FMT_RAR:
				upArchiver = std::make_unique<CArchRar>();
				break;
			case EArchType::FMT_ZIP:
			//	upArchiver = std::make_unique<CArchZip>();
			//	break;
			case EArchType::FMT_7ZIP:
			case EArchType::FMT_ARJ:
			case EArchType::FMT_Z:
			case EArchType::FMT_LZH:
			case EArchType::FMT_CAB:
			case EArchType::FMT_WIM:
		//	case EArchType::FMT_DMG:
			case EArchType::FMT_ISO:
			case EArchType::FMT_XAR:
			case EArchType::FMT_RPM:
			case EArchType::FMT_DEB:
			case EArchType::FMT_NSIS:
			case EArchType::FMT_LZMA:
			case EArchType::FMT_PPMD:
				upArchiver = std::make_unique<CArch7zip>();
				break;
			case EArchType::FMT_GZIP:
				upArchiver = std::make_unique<CArchGzip>();
				break;
			case EArchType::FMT_BZIP2:
				upArchiver = std::make_unique<CArchBzip2>();
				break;
			case EArchType::FMT_XZIP:
				upArchiver = std::make_unique<CArchXzip>();
				break;
			case EArchType::FMT_DMG:
				upArchiver = std::make_unique<CArchDmg>();
				break;
			case EArchType::FMT_NRG:
				upArchiver = std::make_unique<CArchIso>();
				break;
			case EArchType::FMT_TAR:
				upArchiver = std::make_unique<CArchTar>( TarLib::ECompressionType::tarNone );
				break;
			case EArchType::FMT_TAR_GZIP:
				upArchiver = std::make_unique<CArchTar>( TarLib::ECompressionType::tarGzip );
				break;
			case EArchType::FMT_TAR_BZIP2:
				upArchiver = std::make_unique<CArchTar>( TarLib::ECompressionType::tarBzip2 );
				break;
			case EArchType::FMT_TAR_XZIP:
				upArchiver = std::make_unique<CArchTar>( TarLib::ECompressionType::tarXzip );
				break;
			case EArchType::FMT_UNKNOWN:
			default:
				break;
			}

			return upArchiver;
		}

		// do not instantiate this class/struct
		ArchiveType() = delete;
		ArchiveType( ArchiveType const& ) = delete;
		void operator=( ArchiveType const& ) = delete;
	};
}
