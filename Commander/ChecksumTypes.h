#pragma once

/*
	Supported checksum calculation types - CRC32(SFV), MD5, SHA1, SHA256, SHA512
*/

#include <memory>

#include "StringUtils.h"

namespace Commander
{
	struct ChecksumType
	{
		enum class EChecksumType {
			FMT_CRC,
			FMT_MD5,
			FMT_SHA1,
			FMT_SHA256,
			FMT_SHA512,
			FMT_UNKNOWN
		};

		//
		// Checksum entry data
		//
		struct ChecksumData {
			EntryData    entry;
			std::wstring fileName;
			std::wstring checkSum;
			std::wstring status;
			std::wstring crc32;
			std::wstring md5;
			std::wstring sha1;
			std::wstring sha256;
			std::wstring sha512;
		};

		//
		// Get known checksum type length
		//
		static const int getLength( const EChecksumType type )
		{
			switch( type )
			{
			case EChecksumType::FMT_CRC:
				return 8;
			case EChecksumType::FMT_MD5:
				return 32;
			case EChecksumType::FMT_SHA1:
				return 40;
			case EChecksumType::FMT_SHA256:
				return 64;
			case EChecksumType::FMT_SHA512:
				return 128;
			case EChecksumType::FMT_UNKNOWN:
			default:
				_ASSERTE("Invalid checksum type");
				break;
			}

			return 0;
		}

		//
		// Check for known checksum types
		//
		static bool isKnownType( const std::wstring& fileName )
		{
			return getType( fileName ) != EChecksumType::FMT_UNKNOWN;
		}

		//
		// Get checksum type from file name
		//
		static EChecksumType getType( const std::wstring& fileName )
		{
			if( StringUtils::endsWith( fileName, L".sfv" ) )
				return EChecksumType::FMT_CRC;
			else if( StringUtils::endsWith( fileName, L".md5" ) )
				return EChecksumType::FMT_MD5;
			else if( StringUtils::endsWith( fileName, L".sha1" ) )
				return EChecksumType::FMT_SHA1;
			else if( StringUtils::endsWith( fileName, L".sha256" ) )
				return EChecksumType::FMT_SHA256;
			else if( StringUtils::endsWith( fileName, L".sha512" ) )
				return EChecksumType::FMT_SHA512;

			return EChecksumType::FMT_UNKNOWN;
		}

		//
		// Check for valid checksum data
		//
		static bool isValid( EChecksumType type, const std::wstring& sum )
		{
			if( getLength( type ) != static_cast<int>( sum.length() ) )
				return false;

			for( const auto& ch : sum )
			{
				if( !( ( ch >= L'0' && ch <= L'9' ) || ( ch >= L'A' && ch <= L'F' ) ) )
					return false;
			}

			return true;
		}

		// do not instantiate this class/struct
		ChecksumType() = delete;
		ChecksumType( ChecksumType const& ) = delete;
		void operator=( ChecksumType const& ) = delete;
	};
}
