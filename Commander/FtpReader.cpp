#include "stdafx.h"

#include "Commander.h"
#include "FtpReader.h"

#include "FileSystemUtils.h"
#include "IconUtils.h" 

namespace Commander
{
	//
	// Constructor/destructor
	//
	CFtpReader::CFtpReader( ESortMode mode, HWND hwndNotify, std::shared_ptr<FtpData> spData )
		: CBaseReader( mode, hwndNotify )
		, _contentLoaded( false )
	{
		_spFtpData = spData;
		_root = ReaderType::getModePrefix( EReadMode::Ftp );

		_worker.init( [this] { return _ftpReadCore(); }, _hWndNotify, UM_READERNOTIFY );
		_spFtpData->ftpClient.AttachObserver( this );

		SetTimer( _hWndNotify, FC_TIMER_KEEPALIVE_ID, FC_TIMER_KEEPALIVE_TICK, NULL );

		LvUtils::setImageList( hwndNotify, FCS::inst().getApp().getSystemImageList(), LVSIL_SMALL );
	}

	CFtpReader::~CFtpReader()
	{
		KillTimer( _hWndNotify, FC_TIMER_KEEPALIVE_ID );

		// stop workers
		_worker.stop();

		if( _spFtpData )
		{
			if( _spFtpData->ftpClient.IsConnected() )
				_spFtpData->ftpClient.Logout();

			_spFtpData->ftpClient.DetachObserver( this );
		}
	}

	nsFTP::TFTPFileStatusShPtr CFtpReader::findEntry( const nsFTP::TFTPFileStatusShPtrVec& entries, const tstring& name )
	{
		auto pred = [&name]( nsFTP::TFTPFileStatusShPtr entry ) { return ( entry->Name() == name ); };
		auto it = std::find_if( entries.begin(), entries.end(), pred );

		if( it != entries.end() )
			return *it;

		return nullptr;
	}

	bool CFtpReader::isEntryDirectory( const nsFTP::TFTPFileStatusShPtr entry )
	{
		if( !entry )
			return false;

		return ( entry->Attributes().empty() && ( entry->Size() == -1
			&& entry->SizeType() == nsFTP::CFTPFileStatus::T_enSizeType::stUnknown ) )
			|| ( !entry->Attributes().empty() && entry->Attributes()[0] == L'd' );
	}

	//
	// Try to connect to Ftp server using background worker
	//
	bool CFtpReader::_ftpReadCore()
	{
		nsFTP::TFTPFileStatusShPtrVec files, filesAll;

		if( _spFtpData->ftpClient.ChangeWorkingDirectory( _currentDirectory ) != nsFTP::FTP_OK ||
			_spFtpData->ftpClient.List( L".", files, _spFtpData->session.passive ) == false )
		{
			_errorMsg = _upCommand->second.Value();
			return false;
		}

		if( !_spFtpData->ftpClient.ListAll( L".", filesAll, _spFtpData->session.passive ) || filesAll.empty() )
			std::swap( files, filesAll );

		//_currentDirectory = _logonInfo.FwUsername() + L"@" + _logonInfo.Hostname();
		if( _currentDirectory.length() > 1 )
			addParentDirEntry();

		EntryData data = { 0 };

		for( auto& entry : filesAll )
		{
			// mark entries that are present in 'filesAll' but not present in 'files' as hidden
			auto pred = [&entry]( const nsFTP::TFTPFileStatusShPtr data ) { return entry->Name() == data->Name(); };
			bool isHidden = !files.empty() && std::find_if( files.begin(), files.end(), pred ) == files.end();

			data.wfd.nFileSizeLow = data.wfd.nFileSizeHigh = 0;
			data.wfd.ftLastWriteTime = FsUtils::unixTimeToFileTime( entry->MTime() );

			data.link.clear();

			data.date = FsUtils::getFormatStrFileDate( &data.wfd, false );
			data.time = FsUtils::getFormatStrFileTime( &data.wfd, false );
			wcsncpy( data.wfd.cFileName, entry->Name().c_str(), ARRAYSIZE( data.wfd.cFileName ) );

			if( !entry->Attributes().empty() )
				data.attr = entry->Attributes();

			// read directory entries' properties
			if( isEntryDirectory( entry ) )
			{
				data.wfd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
				data.size = FsUtils::getFormatStrFileSize( &data.wfd );

				if( PathUtils::isDirectoryDoubleDotted( data.wfd.cFileName ) )
				{
					if( _currentDirectory.length() > 1 )
						_dirEntries[0] = std::make_shared<EntryData>( data );
				}
				else if( !PathUtils::isDirectoryDotted( data.wfd.cFileName ) )
				{
					if( isHidden )
						data.wfd.dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;

					data.imgSystem = IconUtils::getFromPathWithAttribIndex( L" ", data.wfd.dwFileAttributes );

					_dirEntries.push_back( std::make_shared<EntryData>( data ) );
				}
			}
			else // read file entries' properties
			{
				if( !entry->Attributes().empty() && entry->Attributes()[0] == L'l' )
				{
					data.wfd.dwFileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
					wcsncpy( data.wfd.cFileName, PathUtils::getLinkTarget( entry->Name(), data.link ).c_str(), ARRAYSIZE( data.wfd.cFileName ) );
				}
				else
					data.wfd.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;

				if( isHidden )
					data.wfd.dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;

				if( entry->Size() != -1 )
				{
					LARGE_INTEGER fSize;
					fSize.QuadPart = entry->Size();
					data.wfd.nFileSizeLow = fSize.LowPart;
					data.wfd.nFileSizeHigh = fSize.HighPart;
				}

				data.imgSystem = CDiskReader::getIconIndex( data.wfd.cFileName, data.wfd.dwFileAttributes );
				data.size = FsUtils::getFormatStrFileSize( &data.wfd );

				_fileEntries.push_back( std::make_shared<EntryData>( data ) );
			}
		}

		// do sorting
		sortEntries();

		return true;
	}

	void CFtpReader::OnSendCommand( const nsFTP::CCommand& command, const nsFTP::CArg& arguments )
	{
		_upCommand = std::make_unique<std::pair<nsFTP::CCommand, nsFTP::CReply>>( command, nsFTP::CReply() );

		PrintDebug("Command sent: %ls, args: %zu", command.AsString().c_str(), arguments.size());
	}

	void CFtpReader::OnResponse( const nsFTP::CReply& reply )
	{
		std::wstring cmd = L"[unknown]";

		if( _upCommand )
		{
			cmd = _upCommand->first.AsString();
			_upCommand->second = reply;
		}

		PrintDebug("%ls Response: %ls", cmd.c_str(), reply.Value().c_str());
	}

	void CFtpReader::OnInternalError( const tstring& errorMsg, const tstring& fileName, DWORD lineNr )
	{
		_errorMsg = errorMsg; // TODO
		MessageBox(NULL, errorMsg.c_str(), L"Ftp Error", MB_ICONEXCLAMATION | MB_OK);
		PrintDebug("Error: %ls", errorMsg.c_str());
	}

	void CFtpReader::onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal )
	{
		switch( msgId )
		{
		case UM_READERNOTIFY:
			if( _worker.checkId( workerId ) )
			{
				_contentLoaded = !!retVal;
				_status = _contentLoaded ? EReaderStatus::DataOk : EReaderStatus::DataError;
			}
			break;

		case UM_CALCSIZEDONE:
			if( retVal == 2 )
				_resultSize = workerId;
			break;

		case WM_TIMER:
			if( _worker.isFinished() && retVal == FC_TIMER_KEEPALIVE_ID )
			{
				// sends the NOOP command to keep the connection alive
				_worker.init( [this] { return _ftpKeepAlive(); }, _hWndNotify, UM_STATUSNOTIFY );
				_worker.start();
			}
			break;
		}
	}

	//
	// Add parent (double dotted) directory entry
	//
	void CFtpReader::addParentDirEntry()
	{
		EntryData data = { 0 };
		data.wfd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		wcscpy( data.wfd.cFileName, PathUtils::getDoubleDottedDirectory().c_str() );

		_dirEntries.push_back( std::make_shared<EntryData>( data ) );
	}

	//
	// Check whether mode is still valid for given path
	//
	bool CFtpReader::isPathValid( const std::wstring& path )
	{
		if( _spFtpData && _spFtpData->ftpClient.IsConnected() && StringUtils::startsWith( path, ReaderType::getModePrefix( EReadMode::Ftp ) ) )
			return true;

		return false;
	}

	//
	// Read data
	//
	bool CFtpReader::readEntries( std::wstring& path )
	{
		/*std::vector<std::wstring> files = {
			L"lrwxrwxrwx    1 997      997             7 Mar 01  2012 12.04 -> precise",
			L"lrwxrwxrwx    1 997      997             7 Aug 07  2014 12.04.5 -> precise",
			L"lrwxrwxrwx    1 997      997             6 Mar 27  2014 14.04 -> trusty",
			L"lrwxrwxrwx    1 997      997             6 Mar 07  2019 14.04.6 -> trusty",
			L"lrwxrwxrwx    1 997      997             6 Feb 16  2017 16.04 -> xenial",
			L"lrwxrwxrwx    1 997      997             6 Feb 28  2019 16.04.6 -> xenial",
			L"lrwxrwxrwx    1 997      997             6 Aug 08  2019 18.04 -> bionic",
			L"lrwxrwxrwx    1 997      997             6 Feb 12  2020 18.04.4 -> bionic",
			L"lrwxrwxrwx    1 997      997             4 Oct 17  2019 19.10 -> eoan",
			L"lrwxrwxrwx    1 997      997             5 Apr 03 11:42 20.04 -> focal",
			L"lrwxrwxrwx    1 997      997             5 Aug 06 15:28 20.04.1 -> focal",
			L"-rw-rw-r--    1 997      997            22 Feb 01  2006 FOOTER.html",
			L"-rw-rw-r--    1 997      997          5425 Aug 06 16:03 HEADER.html",
			L"drwxrwxr-x    2 997      997          4096 Feb 12  2020 bionic",
			L"drwxrwxr-x    2 997      997          4096 Sep 21  2012 cdicons",
			L"drwxrwxr-x    2 997      997          4096 Oct 17  2019 eoan",
			L"-rw-rw-r--    1 997      997          1150 Jun 16  2011 favicon.ico",
			L"drwxr-xr-x    2 997      997          4096 Aug 07 09:02 focal",
			L"drwxrwxr-x    3 997      997          4096 Apr 11  2019 include",
			L"drwxrwxr-x    2 997      997          4096 Mar 12  2019 precise",
			L"lrwxrwxrwx    1 997      997             1 Jul 31  2007 releases -> .",
			L"-rw-rw-r--    1 997      997            49 Oct 29  2009 robots.txt",
			L"drwxr-xr-x    2 997      997          4096 Mar 07  2019 trusty",
			L"drwxrwxr-x    2 997      997          4096 Feb 28  2019 xenial"
		};

		nsFTP::CFTPListParser parser;
		EntryData data = { 0 };

		for( auto& file : files )
		{
			nsFTP::CFTPFileStatus status;
			parser.Parse( status, file );

			auto entry = &status;
			auto attr = entry->Attributes();
			auto tmp = entry->Path();

			data.wfd.ftLastWriteTime = FsUtils::unixTimeToFileTime( entry->MTime() );
			wcsncpy( data.cDate, FsUtils::getFormatStrFileDate( &data.wfd ).c_str(), ARRAYSIZE( EntryData::cDate ) );
			wcsncpy( data.cTime, FsUtils::getFormatStrFileTime( &data.wfd ).c_str(), ARRAYSIZE( EntryData::cTime ) );
			wcsncpy( data.wfd.cFileName, entry->Name().c_str(), MAX_PATH );

			if( !attr.empty() ) wcsncpy( data.cAttr, attr.c_str(), ARRAYSIZE( EntryData::cAttr ) );

			if( ( !attr.empty() && ( attr[0] == L'd' || attr[0] == L'D' ) ) || ( entry->Size() == -1 && entry->SizeType() == nsFTP::CFTPFileStatus::T_enSizeType::stUnknown ) )
			{
				data.wfd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
				wcsncpy( data.cSize, FsUtils::getFormatStrFileSize( &data.wfd ).c_str(), ARRAYSIZE( EntryData::cSize ) );
				_dirEntries.push_back( std::make_shared<EntryData>( data ) );
			}
			else
			{
				if( !attr.empty() && ( attr[0] == L'l' || attr[0] == L'L' ) )
					data.wfd.dwFileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
				else
					data.wfd.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;

				data.wfd.nFileSizeLow = ( entry->Size() != -1 ) ? (DWORD)entry->Size() : 0;
				wcsncpy( data.cSize, FsUtils::getFormatStrFileSize( &data.wfd ).c_str(), ARRAYSIZE( EntryData::cSize ) );
				_fileEntries.push_back( std::make_shared<EntryData>( data ) );
			}
		}

		_status = EReaderStatus::DataOk;*/

		if( isPathValid( path ) )
		{
			if( _worker.isRunning() || ( !_worker.isRunning() && !_contentLoaded ) )
			{
				_status = EReaderStatus::ReadingData;
				_contentLoaded = false;

				_worker.stop();
				_errorMsg.clear();

				_worker.init( [this] { return _ftpReadCore(); }, _hWndNotify, UM_READERNOTIFY );

				_currentDirectory = path.substr( wcslen( ReaderType::getModePrefix( EReadMode::Ftp ) ) );
				PathUtils::unifyDelimiters( _currentDirectory, false );

				_dirEntries.clear();
				_fileEntries.clear();

				return _worker.start();
			}

			_contentLoaded = false;

			return true;
		}

		_status = EReaderStatus::Invalid;

		return false;
	}

	bool CFtpReader::_ftpKeepAlive()
	{
		_spFtpData->ftpClient.KeepAlive();

		return _spFtpData->ftpClient.IsConnected();
	}

	//
	//
	//
	bool CFtpReader::copyEntries( const std::vector<std::wstring>& entries, const std::wstring& path )
	{
		return false;
	}

	//
	//
	//
	bool CFtpReader::moveEntries( const std::vector<std::wstring>& entries, const std::wstring& path )
	{
		return false;
	}

	//
	//
	//
	bool CFtpReader::renameEntry( const std::wstring& path, const std::wstring& newName )
	{
		return false;
	}

	//
	//
	//
	bool CFtpReader::deleteEntries( const std::vector<std::wstring>& entries )
	{
		return false;
	}

	void CFtpReader::calculateDirectorySize( const std::wstring& dirName, ULONGLONG& fileSizeResult )
	{
		if( _spFtpData->ftpClient.ChangeWorkingDirectory( dirName ) == nsFTP::FTP_OK )
		{
			nsFTP::TFTPFileStatusShPtrVec files;
			_spFtpData->ftpClient.ListAll( L".", files, _spFtpData->session.passive );

			if( files.empty() )
				_spFtpData->ftpClient.List( L".", files, _spFtpData->session.passive );

			for( const auto& entry : files )
			{
				if( isEntryDirectory( entry ) )
				{
					if( !PathUtils::isDirectoryDotted( entry->Name().c_str() ) &&
						!PathUtils::isDirectoryDoubleDotted( entry->Name().c_str() ) )
						calculateDirectorySize( entry->Name(), fileSizeResult );
				}
				else
					fileSizeResult += entry->Size();

				if( !_worker.isRunning() )
					break;
			}

			_spFtpData->ftpClient.ChangeToParentDirectory();
		}
	}

	bool CFtpReader::_ftpCalcSizeCore()
	{
		ULONGLONG fileSizeResult = 0;

		for( const auto& idx : _markedEntries )
		{
			auto entryData = getEntry( idx );

			if( entryData )
			{
				if( IsDir( entryData->wfd.dwFileAttributes ) )
					calculateDirectorySize( entryData->wfd.cFileName, fileSizeResult );
				else
					fileSizeResult += FsUtils::getFileSize( &entryData->wfd );
			}

			if( !_worker.isRunning() )
				break;
		}

		_worker.sendNotify( 2, fileSizeResult );

		return true;
	}

	//
	// Calculate size
	//
	bool CFtpReader::calculateSize( const std::vector<int>& entries )
	{
		_worker.stop();

		_resultSize = 0ull;

		if( !entries.empty() )
		{
			_worker.init( [this] { return _ftpCalcSizeCore(); }, _hWndNotify, UM_CALCSIZEDONE );
			_markedEntries = entries;

			_worker.start();
		}

		return false;
	}
}
