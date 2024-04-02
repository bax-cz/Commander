#include "stdafx.h"

#include "Commander.h"
#include "FileSystemUtils.h"
#include "IconUtils.h"
#include "NetworkReader.h"
#include "MiscUtils.h"
#include "NetworkUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CNetworkReader::CNetworkReader( ESortMode mode, HWND hwndNotify )
		: CBaseReader( mode, hwndNotify )
	{
		_status = EReaderStatus::DataOk;
		_root = ReaderType::getModePrefix( EReadMode::Network );

		_worker.init( [this] { return _scanNetCore(); }, _hWndNotify, UM_READERNOTIFY );
		_worker.start();

		LvUtils::setImageList( hwndNotify, FCS::inst().getApp().getSystemImageList(), LVSIL_SMALL );
	}

	CNetworkReader::~CNetworkReader()
	{
		_worker.stop();

		PrintDebug("dtor");
	}

	bool connectToSmbServer( const std::wstring& serverName )
	{
	//	auto result = WNetConnectionDialog( FCS::inst().getFcWindow(), RESOURCETYPE_DISK ); // pripojit sitovou jednotku
		//WNetGetConnection(NULL, L"\\\\JIRIS-MAC-PRO\\Downloads", 0);

		std::wstring remoteName = L"\\\\";
		remoteName += serverName;

		NETRESOURCE resource = { 0 };
		resource.dwType = RESOURCETYPE_DISK;
		resource.lpRemoteName = &remoteName[0];

		auto result = WNetAddConnection3( FCS::inst().getFcWindow(), &resource, NULL, NULL, CONNECT_INTERACTIVE | CONNECT_PROMPT );
		if( result != NO_ERROR )
		{
			std::wostringstream sstr;
			sstr << L"Path: \"" << serverName << L"\"\r\n";
			sstr << SysUtils::getErrorMessage( result );

			MessageBox( FCS::inst().getFcWindow(), sstr.str().c_str(), L"Error Connecting to Server", MB_ICONEXCLAMATION | MB_OK );

			return false;
		}

		return true;
	}

	bool CNetworkReader::_scanNetCore()
	{
		// wait until neighbor thread finishes loading
		while( _worker.isRunning() )
		{
			if( FCS::inst().getApp().neighborsFinished() )
				break;

			Sleep( 10 );
		}

		// TODO: we are interested only in samba type (WNNC_NET_SMB)
		if( FCS::inst().getApp().getNeighbors().empty() )
		{
			WCHAR providerName[MAX_PATH];
			DWORD ProviderLen = MAX_PATH;

			DWORD ret = WNetGetProviderName( WNNC_NET_SMB, providerName, &ProviderLen );

			if( ret == NO_ERROR )
			{
				NETINFOSTRUCT nis = { 0 };
				nis.cbStructure = sizeof( nis );

				ret = WNetGetNetworkInformation( providerName, &nis );
				PrintDebug("%ls: NetType: %d, ProviderVersion: %d", providerName, nis.wNetType, nis.dwProviderVersion);
			}

			if( ret != NO_ERROR )
				_errorMsg = NetUtils::getErrorDescription( ret );
			else
				_errorMsg = L"No SMB neighbors were found.";

			return false;
		}
		//else
		//{
		//	auto& neighbors = FCS::inst().getApp().getNeighbors();
		//	for( const auto& neighbor : neighbors )
		//	{
		//		auto entry = StringUtils::split( neighbor );
		//		DWORD ret = NetShareCheck( serverName, device, type );
		//	}
		//}

		return true;
	}

	void CNetworkReader::onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal )
	{
		if( msgId == UM_READERNOTIFY && _worker.checkId( workerId ) )
		{
			_status = retVal ? EReaderStatus::DataOk : EReaderStatus::Invalid;
		}
	}

	//
	// Add parent (double dotted) directory entry
	//
	void CNetworkReader::addParentDirEntry()
	{
		EntryData data = { 0 };
		data.wfd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		wcscpy( data.wfd.cFileName, PathUtils::getDoubleDottedDirectory().c_str() );

		_dirEntries.push_back( std::make_shared<EntryData>( data ) );
	}

	//
	// Check whether mode is still valid for given path
	//
	bool CNetworkReader::isPathValid( const std::wstring& path )
	{
		if( StringUtils::startsWith( path, ReaderType::getModePrefix( EReadMode::Network ) ) )
			return true;

		return false;
	}

	//
	// Read data
	//
	bool CNetworkReader::readEntries( std::wstring& path )
	{
		if( isPathValid( path ) )
		{
			if( _worker.isRunning() )
			{
				_status = EReaderStatus::ReadingData;
				return true;
			}

			auto localPath = path.substr( wcslen( ReaderType::getModePrefix( EReadMode::Network ) ) + 1 );

			// change directory to UNC path and continue by reading from disk
			if( !localPath.empty() && PathUtils::getParentDirectory( localPath ) != localPath )
			{
				path = std::wstring( L"\\\\" ) + localPath;
				_status = EReaderStatus::Invalid;

				return true;
			}

			EntryData data = { 0 };
			data.wfd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
			_dirEntries.clear();

			auto& neighbors = FCS::inst().getApp().getNeighbors();
			for( const auto& neighbor : neighbors )
			{
				auto entry = StringUtils::split( neighbor );

				// add neighborhood server names - RESOURCEDISPLAYTYPE_SERVER
				if( entry[0] == L"2" && localPath.empty() )
				{
					data.imgSystem = IconUtils::getStockIndex( SIID_SERVER );
					wcsncpy( data.wfd.cFileName, entry[1].c_str(), ARRAYSIZE( WIN32_FIND_DATA::cFileName ) );

				//	connectToSmbServer( entry[1] );

					_dirEntries.push_back( std::make_shared<EntryData>( data ) );
				}
				// add server's shared folders - RESOURCEDISPLAYTYPE_SHARE
				else if( entry[0] == L"3" && !localPath.empty() && StringUtils::startsWith( entry[1], localPath ) )
				{
					if( _dirEntries.empty() ) addParentDirEntry();

					data.imgSystem = IconUtils::getStockIndex( SIID_SERVERSHARE );
					wcsncpy( data.wfd.cFileName, PathUtils::stripPath( entry[1] ).c_str(), ARRAYSIZE( WIN32_FIND_DATA::cFileName ) );

					_dirEntries.push_back( std::make_shared<EntryData>( data ) );
				}
			}

			// do sorting
			sortEntries();

			return true;
		}

		_status = EReaderStatus::Invalid;

		return false;
	}
}
