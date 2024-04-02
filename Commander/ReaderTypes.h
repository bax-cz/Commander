#pragma once

/*
	Supported reader types - Disk, archive, network, etc.
*/

#include <map>

#include "StringUtils.h"
#include "FileSystemUtils.h"
#include "ArchiveReader.h"
#include "ArchiveDataProvider.h"
#include "DiskReader.h"
#include "DiskDataProvider.h"
#include "FtpReader.h"
#include "FtpDataProvider.h"
#include "NetworkReader.h"
#include "SshReader.h"
#include "SshDataProvider.h"
#include "RegistryReader.h"
#include "RegistryDataProvider.h"

namespace Commander
{
	struct ReaderType
	{
		//
		// Default modes prefixes
		//
		static constexpr WCHAR *_modePrefixes[4] = { L"net:", L"ftp:", L"ssh:", L"reg:" };

		//
		// Check for known archive types
		//
		static WCHAR *getModePrefix( EReadMode readMode )
		{
			switch( readMode )
			{
			case EReadMode::Network:
				return _modePrefixes[0];
			case EReadMode::Ftp:
				return _modePrefixes[1];
			case EReadMode::Putty:
				return _modePrefixes[2];
			case EReadMode::Reged:
				return _modePrefixes[3];
			}

			_ASSERTE( "Invalid reading mode" );

			return L"";
		}

		//
		// Get mode type from path name
		//
		static EReadMode getRequestedMode( const std::wstring& path, bool isFile )
		{
			if( StringUtils::startsWith( path, getModePrefix( EReadMode::Network ) ) )
				return EReadMode::Network;
			if( StringUtils::startsWith( path, getModePrefix( EReadMode::Ftp ) ) )
				return EReadMode::Ftp;
			if( StringUtils::startsWith( path, getModePrefix( EReadMode::Putty ) ) )
				return EReadMode::Putty;
			if( StringUtils::startsWith( path, getModePrefix( EReadMode::Reged ) ) )
				return EReadMode::Reged;
			if( isFile && ArchiveType::isKnownType( path ) )
				return EReadMode::Archive;

			return EReadMode::Disk;
		}


		// stop the compiler generating methods of copy the object
		ReaderType( ReaderType const& ) = delete;
		void operator=( ReaderType const& ) = delete;
	};
}
