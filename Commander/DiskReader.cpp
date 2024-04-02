#include "stdafx.h"

#include "Commander.h"
#include "DiskReader.h"
#include "FileSystemUtils.h"
#include "IconUtils.h"
#include "ImageTypes.h"
#include "NetworkUtils.h"
#include "MiscUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CDiskReader::CDiskReader( ESortMode mode, HWND hwndNotify )
		: CBaseReader( mode, hwndNotify )
		, _hWaitEvent( nullptr )
		, _hDirWatch( nullptr )
		, _isWatcherRunning( false )
		, _contentLoaded( false )
		, _overMaxPath( false )
		, _overlaysDone( false )
	{
		_worker.init( [this] { return _readEntries(); }, hwndNotify, UM_READERNOTIFY );
		_calcWorker.init( [this] { return _calcItemsSize(); }, hwndNotify, UM_CALCSIZEDONE );

		LvUtils::setImageList( hwndNotify, FCS::inst().getApp().getSystemImageList(), LVSIL_SMALL );
	}

	CDiskReader::~CDiskReader()
	{
		FCS::inst().getApp().getOverlaysLoader().stop( _hWndNotify );

		// stop workers
		_worker.stop();
		_calcWorker.stop();

		// stop watcher
		stopFileSystemWatcher();

		if( _hDirWatch )
			CloseHandle( _hDirWatch );
	}

	int CDiskReader::getIconIndex( const WCHAR *fileName, DWORD attr )
	{
		int idx = 0;

		auto ext = wcsrchr( fileName, L'.' );

		if( ext && ArchiveType::isKnownType( fileName ) )
			idx = IconUtils::getStockIndex( SIID_ZIPFILE );
		else if( ext && ImageType::isKnownType( fileName ) )
			idx = IconUtils::getStockIndex( SIID_IMAGEFILES );
		else
			idx = IconUtils::getFromPathWithAttribIndex( ext && ext[1] != L'\0' ? ext : L".*", attr );

		return idx;
	}

	//
	// Calculate recursively size of directory on disk
	//
	bool CDiskReader::calculateDirectorySize( const std::wstring& dirName, ULONGLONG *dirSize )
	{
		auto path = _currentDirectory + dirName;

		// exclude reparse points from calculation
		if( FsUtils::checkPathAttrib( path, FILE_ATTRIBUTE_REPARSE_POINT ) )
			return false;

		WIN32_FIND_DATA wfd = { 0 };

		// read file system structure
		HANDLE hFile = FindFirstFileEx( ( PathUtils::getExtendedPath( path + L"\\*" ) ).c_str(), FindExInfoStandard, &wfd, FindExSearchNameMatch, NULL, FCS::inst().getApp().getFindFLags() );
		if( hFile != INVALID_HANDLE_VALUE )
		{
			do
			{
				if( !_calcWorker.isRunning() )
					break;

				if( ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
				{
					if( !PathUtils::isDirectoryDotted( wfd.cFileName )
					 && !PathUtils::isDirectoryDoubleDotted( wfd.cFileName ) )
					{
						calculateDirectorySize( PathUtils::addDelimiter( dirName ) + wfd.cFileName, dirSize );
					}
				}
				else
				{
					*dirSize += FsUtils::getFileSize( &wfd );
				}
			} while( FindNextFile( hFile, &wfd ) );

			FindClose( hFile );
		}

		return true;
	}

	bool CDiskReader::_readEntries()
	{
		ShellUtils::CComInitializer _com( COINIT_APARTMENTTHREADED );

		_currentDirectory = FsUtils::canonicalizePathName( _readFromDirectory );

		_root = FsUtils::getPathRoot( _currentDirectory );
		_freeSpace = FsUtils::getDiskFreeSpace( _root );

		// report drive's free-space
		SendNotifyMessage( FCS::inst().getFcWindow(), UM_FSPACENOTIFY, _root[0], _freeSpace );

		// read file system structure
		WIN32_FIND_DATA wfd = { 0 };
		HANDLE hFile = FindFirstFileEx( ( PathUtils::getExtendedPath( _currentDirectory + L"*" ) ).c_str(), FindExInfoStandard, &wfd, FindExSearchNameMatch, NULL, FCS::inst().getApp().getFindFLags() );
		if( hFile != INVALID_HANDLE_VALUE )
		{
			do {
				auto data = std::make_shared<EntryData>( EntryData { wfd,
					FsUtils::getFormatStrFileSize( &wfd ),
					FsUtils::getFormatStrFileDate( &wfd ),
					FsUtils::getFormatStrFileTime( &wfd ),
					FsUtils::getFormatStrFileAttr( &wfd ),
					std::wstring(), 0, 0, 0ul } );

				if( ( data->wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
				{
					if( !PathUtils::isDirectoryDotted( data->wfd.cFileName ) )
					{
						data->hardLinkCnt = 0;
						data->imgSystem = IconUtils::getFromPathWithAttribIndex( L" ", data->wfd.dwFileAttributes );

						std::lock_guard<std::mutex> lock( _mutex );

						_dirEntries.push_back( data );
					}
				}
				else
				{
					if( !_overMaxPath && ( _currentDirectory.length() + wcslen( data->wfd.cFileName ) ) > MAX_PATH )
						_overMaxPath = true;

					data->hardLinkCnt = FsUtils::getHardLinksCount( _currentDirectory + data->wfd.cFileName );
					data->imgSystem = getIconIndex( data->wfd.cFileName, data->wfd.dwFileAttributes );

					std::lock_guard<std::mutex> lock( _mutex );

					_fileEntries.push_back( data );
				}

			} while( FindNextFile( hFile, &wfd ) && _worker.isRunning() );

			FindClose( hFile );

			if( !_worker.isRunning() )
				return false;

			// do sorting
			sortEntries();

			// set overlay indices
			auto itemsCount = static_cast<int>( _fileEntries.size() + _dirEntries.size() );
			for( int i = 0; i < itemsCount; ++i )
				getEntry( i )->imgOverlay = i;

			return true;
		}
		else
			_errorMsg = SysUtils::getErrorMessage( GetLastError() );

		return false;
	}

	bool CDiskReader::_calcItemsSize()
	{
		ULONGLONG fileSizeResult = 0;

		for( const auto& idx : _markedEntries )
		{
			if( !_calcWorker.isRunning() )
				return false;

			auto entryData = getEntry( idx );

			if( entryData )
			{
				if( IsDir( entryData->wfd.dwFileAttributes ) )
					calculateDirectorySize( entryData->wfd.cFileName, &fileSizeResult );
				else
					fileSizeResult += FsUtils::getFileSize( &entryData->wfd );
			}
		}

		_calcWorker.sendNotify( 2, fileSizeResult );

		return true;
	}

	void CDiskReader::startFileSystemWatcher( const std::wstring& dirName )
	{
		if( !_isWatcherRunning || _watchingDirectory != dirName )
		{
			if( _isWatcherRunning )
				stopFileSystemWatcher();

			_watchingDirectory = dirName;

			// create unnamed event to signal thread to abort waiting
			_hWaitEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

			if( _hWaitEvent != NULL )
			{
				_isWatcherRunning = true;
				_upWatcherThread = std::make_unique<std::thread>( [this] { _watcherThreadProc(); } );

				PrintDebug("Watcher started.");
			}
		}
	}

	void CDiskReader::stopFileSystemWatcher()
	{
		if( _isWatcherRunning )
		{
			_ASSERTE( _hWaitEvent != NULL );
			PrintDebug("stop called");

			// signal thread to terminate
			SetEvent( _hWaitEvent );

			// wait for thread termination
			if( _upWatcherThread && _upWatcherThread->joinable() )
				_upWatcherThread->join();

			// close unnamed event
			CloseHandle( _hWaitEvent );
			_hWaitEvent = nullptr;

			_isWatcherRunning = false;
			PrintDebug("stopped");
		}
	}

	void CDiskReader::_watcherThreadProc()
	{
		const DWORD dwInfoSize = 4096;
		FILE_NOTIFY_INFORMATION *pInfo = new FILE_NOTIFY_INFORMATION[dwInfoSize];

		HRESULT hr = S_OK;
		OVERLAPPED overlapped = { 0 };
		HANDLE waitHandles[2];
		DWORD errorId = NO_ERROR;

		const DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
		const DWORD dwFlags = FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED;
		const DWORD dwNotifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_ATTRIBUTES;

		if( !pInfo )
			hr = E_OUTOFMEMORY;

		// open directory handle
		if( hr == S_OK )
		{
			if( _hDirWatch )
				CloseHandle( _hDirWatch );

			_hDirWatch = CreateFile( PathUtils::getExtendedPath( _watchingDirectory ).c_str(), GENERIC_READ, dwShareMode, NULL, OPEN_EXISTING, dwFlags, NULL );

			if( _hDirWatch == INVALID_HANDLE_VALUE )
			{
				errorId = GetLastError();
				hr = HRESULT_FROM_WIN32( errorId );
				_hDirWatch = nullptr;
			}
		}

		// set up the OVERLAPPED structure for receiving async notifications
		if( hr == S_OK )
		{
			ZeroMemory( &overlapped, sizeof( OVERLAPPED ) );

			overlapped.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

			if( overlapped.hEvent == NULL )
			{
				errorId = GetLastError();
				hr = HRESULT_FROM_WIN32( errorId );
			}
		}

		// loop for change notifications
		while( hr == S_OK )
		{
			DWORD dwBytesReturned = 0;

			//
			// Get information that describes the most recent file change
			// This call will return immediately and will set
			// overlapped.hEvent when it has completed
			//
			if( ReadDirectoryChangesW( _hDirWatch, pInfo, dwInfoSize, FALSE, dwNotifyFilter, &dwBytesReturned, &overlapped, NULL ) == 0 )
			{
				errorId = GetLastError();
				hr = HRESULT_FROM_WIN32( errorId );
			}

			//
			// Set up a wait on the overlapped handle and on the handle used
			// to shut down this thread
			//
			if( hr == S_OK )
			{
				DWORD dwResult = 0;

				//
				// WaitForMultipleObjects will wait on both event handles in
				// waitHandles.  If either one of these events is signalled,
				// WaitForMultipleObjects will return and its return code
				// can be used to tell which event was set.
				//
				waitHandles[0] = _hWaitEvent;
				waitHandles[1] = overlapped.hEvent;

				dwResult = WaitForMultipleObjects( 2, waitHandles, FALSE, INFINITE );

				if( dwResult == WAIT_FAILED )
				{
					// Hit an error--bail out from this thread
					errorId = GetLastError();
					hr = HRESULT_FROM_WIN32( errorId );
				}
				else if( dwResult == WAIT_OBJECT_0 )
				{
					// Stop was called, and this thread should be shut down
					hr = S_FALSE;
					PrintDebug("Change notification thread shutting down.");
				}
				// Otherwise, a change notification occured, and we should
				// continue through the loop
			}

			// retrieve result from overlapped structure
			if( hr == S_OK )
			{
				DWORD dwBytesTransferred = 0;

				if( GetOverlappedResult( _hDirWatch, &overlapped, &dwBytesTransferred, TRUE ) == 0 )
				{
					errorId = GetLastError();
					hr = HRESULT_FROM_WIN32( errorId );
				}
			}

			if( hr == S_OK )
			{
				// update directory content
				onDirectoryContentChanged( pInfo );
			}
		}

		// cleanup
		if( pInfo )
		{
			delete[] pInfo;
			pInfo = nullptr;
		}

		if( overlapped.hEvent != NULL )
		{
			CloseHandle( overlapped.hEvent );
			overlapped.hEvent = NULL;
		}

		PrintDebug("Watcher thread finished.");
	}

	void CDiskReader::onDirectoryContentChanged( FILE_NOTIFY_INFORMATION *pFileNotifyInfo )
	{
		FILE_NOTIFY_INFORMATION *fni = nullptr;
		WIN32_FIND_DATA wfdTemp;
		std::wstring fileName;
		DWORD offset = 0;
		int entryOldIdx = 0;

		bool sortingNeeded = false;

		do
		{
			fni = reinterpret_cast<PFILE_NOTIFY_INFORMATION>( reinterpret_cast<unsigned char*>( pFileNotifyInfo ) + offset );
			offset += fni->NextEntryOffset;

			fileName.assign( fni->FileName, fni->FileNameLength / sizeof( WCHAR ) );

		// Glitch?? sometimes FILE_ACTION_REMOVED appears before sequence FILE_ACTION_RENAMED_OLD_NAME, FILE_ACTION_RENAMED_NEW_NAME
		// so we remove old entry and add as a new when FILE_ACTION_RENAMED_XXX_NAME fails (notice missing break;)

		//	PrintDebug( "File: %ls, %d, action: %ls", fileName.c_str(), entryOldIdx, getActionStr( fni->Action ).c_str() );

			switch( fni->Action )
			{
				case FILE_ACTION_RENAMED_OLD_NAME:
					// new name will be given in the next cycle, store entry index
					// this might fail if FILE_ACTION_REMOVED came before
					entryOldIdx = getEntryIndex( fileName );
					break;

				case FILE_ACTION_RENAMED_NEW_NAME:
				{
					auto entry = getEntry( entryOldIdx );
					if( entry ) {
						sortingNeeded = true;

						std::lock_guard<std::mutex> lock( _mutex );

						int idx = getIconIndex( fileName.c_str(), entry->wfd.dwFileAttributes );
						if( idx != entry->imgSystem )
						{
							entry->imgSystem = idx;
							entry->imgOverlay = 0;
						}
						wcsncpy( entry->wfd.cFileName, fileName.c_str(), MAX_PATH );
						break;
					} // else fall-through and add as a new item
				}

				case FILE_ACTION_ADDED:
				{
					if( FsUtils::getFileInfo( _currentDirectory + fileName, wfdTemp ) ) {
						sortingNeeded = true;

						EntryData data = { 0 };
						data.wfd = wfdTemp;

						data.size = FsUtils::getFormatStrFileSize( &data.wfd );
						data.date = FsUtils::getFormatStrFileDate( &data.wfd );
						data.time = FsUtils::getFormatStrFileTime( &data.wfd );
						data.attr = FsUtils::getFormatStrFileAttr( &data.wfd );

						data.imgSystem = getIconIndex( fileName.c_str(), data.wfd.dwFileAttributes );
						data.imgOverlay = 0;

						std::lock_guard<std::mutex> lock( _mutex );

						if( IsDir( data.wfd.dwFileAttributes ) )
							_dirEntries.push_back( std::make_shared<EntryData>( data ) );
						else
							_fileEntries.push_back( std::make_shared<EntryData>( data ) );
					}
				}
				break;

				case FILE_ACTION_REMOVED:
				{
					auto idx = getEntryIndex( fileName );
					auto entry = getEntry( idx );

					if( entry ) {
						sortingNeeded = true;

						std::lock_guard<std::mutex> lock( _mutex );

						if( IsDir( entry->wfd.dwFileAttributes ) )
							_dirEntries.erase( _dirEntries.begin() + idx );
						else
							_fileEntries.erase( _fileEntries.begin() + ( idx - _dirEntries.size() ) );
					}
				}
				break;

				case FILE_ACTION_MODIFIED:
				{
					auto idx = getEntryIndex( fileName );
					auto entry = getEntry( idx );

					if( entry ) {
						std::lock_guard<std::mutex> lock( _mutex );
						if( FsUtils::getFileInfo( _currentDirectory + fileName, wfdTemp ) )
						{
							entry->wfd = wfdTemp;
							entry->size = FsUtils::getFormatStrFileSize( &entry->wfd );
							entry->date = FsUtils::getFormatStrFileDate( &entry->wfd );
							entry->time = FsUtils::getFormatStrFileTime( &entry->wfd );
							entry->attr = FsUtils::getFormatStrFileAttr( &entry->wfd );
						}
					}
				}
				break;
			}
		} while( fni->NextEntryOffset );

		if( sortingNeeded )
		{
			sortEntries();

			// re-set overlay indices
			auto itemsCount = static_cast<int>( _fileEntries.size() + _dirEntries.size() );
			for( int i = 0; i < itemsCount; ++i )
				getEntry( i )->imgOverlay = i;
		}

		_freeSpace = FsUtils::getDiskFreeSpace( _root );

		SendNotifyMessage( FCS::inst().getFcWindow(), UM_FSPACENOTIFY, _root[0], _freeSpace );
		SendNotifyMessage( _hWndNotify, UM_DIRCONTENTCH, MAKEWPARAM( sortingNeeded, 0 ), 0 );
	}

	void CDiskReader::onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal )
	{
		PrintDebug("worker id: %zu (%d)", workerId, retVal);

		switch( msgId )
		{
		case UM_DIRCONTENTCH:
			switch( retVal )
			{
			case 1: // entries were either added, renamed or removed, stop overlay worker
				if( _overlaysDone )
				{
					_overlaysDone = false;
					LvUtils::setImageList( _hWndNotify, FCS::inst().getApp().getSystemImageList(), LVSIL_SMALL );
				}
				break;
			case 2: // reload overlays (from PanelTab)
				FCS::inst().getApp().getOverlaysLoader().start( _hWndNotify, _currentDirectory, _dirEntries, _fileEntries );
				break;
			}
			break;

		case UM_READERNOTIFY:
			_contentLoaded = !!retVal;
			_status = _contentLoaded ? EReaderStatus::DataOk : EReaderStatus::DataError;

			if( _contentLoaded )
			{
				startFileSystemWatcher( _currentDirectory );
				FCS::inst().getApp().getOverlaysLoader().start( _hWndNotify, _currentDirectory, _dirEntries, _fileEntries );
			}
			break;

		case UM_OVERLAYSDONE:
			if( retVal == 2 )
			{
				_overlaysDone = true;

				HIMAGELIST imgListOverlays = reinterpret_cast<HIMAGELIST>( workerId );
				LvUtils::setImageList( _hWndNotify, imgListOverlays, LVSIL_SMALL );
			}
			break;

		case UM_CALCSIZEDONE:
			if( retVal == 2 )
				_resultSize = workerId;
			break;
		}
	}

	//
	// Read data
	//
	bool CDiskReader::readEntries( std::wstring& path )
	{
		if( _worker.isRunning() || ( !_worker.isRunning() && !_contentLoaded ) )
		{
			_status = EReaderStatus::ReadingData;
			_contentLoaded = _overlaysDone = false;
			_overMaxPath = path.length() > MAX_PATH;

			LvUtils::setImageList( _hWndNotify, FCS::inst().getApp().getSystemImageList(), LVSIL_SMALL );

			stopFileSystemWatcher();

			_calcWorker.stop();
			_worker.stop();

			FCS::inst().getApp().getOverlaysLoader().stop( _hWndNotify );

			_dirEntries.clear();
			_fileEntries.clear();

			_readFromDirectory = PathUtils::addDelimiter( path );

			return _worker.start();
		}
		else if( _status == EReaderStatus::DataOk )
			path = _currentDirectory; // update path

		_contentLoaded = false;

		return true;
	}

	//
	// Copy
	//
	bool CDiskReader::copyEntries( const std::vector<std::wstring>& entries, const std::wstring& path )
	{
		return false;
	}

	//
	// Move
	//
	bool CDiskReader::moveEntries( const std::vector<std::wstring>& entries, const std::wstring& path )
	{
		return false;
	}

	//
	// Rename
	//
	bool CDiskReader::renameEntry( const std::wstring& path, const std::wstring& newName )
	{
		if( !FsUtils::renameFile( path.c_str(), newName.c_str() ) )
		{
			DWORD errorId = GetLastError();

			BOOL aborted = FALSE;
			bool ret = ShellUtils::renameItem( path, newName, &aborted );

			if( ret && !aborted )
			{
				WIN32_FIND_DATA wfd = { 0 };

				if( FsUtils::getFileInfo( path, wfd ) || !FsUtils::getFileInfo( newName, wfd ) )
				{
					SetLastError( errorId );
					return false;
				}
			}
		}

		return true;
	}

	//
	// Delete
	//
	bool CDiskReader::deleteEntries( const std::vector<std::wstring>& entries )
	{
		return false;
	}

	//
	// Calculate size - return false means in-thread async operation (empty entries means stop calculation)
	//
	bool CDiskReader::calculateSize( const std::vector<int>& entries )
	{
		_calcWorker.stop();

		_resultSize = 0ull;

		if( !entries.empty() )
		{
			_markedEntries = entries;
			_calcWorker.start();
		}

		return false;
	}
}
