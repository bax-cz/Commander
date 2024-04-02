#include "stdafx.h"

#include "Commander.h"
#include "DataManager.h"

#include "IconUtils.h"
#include "MiscUtils.h"
#include "NetworkUtils.h"
#include "ShellUtils.h"
#include "StringUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CDataManager::CDataManager()
		: _currentMode( EReadMode::Disk )
		, _sortMode( ESortMode::ByName )
		, _hWndNotify( nullptr )
		, _markedDirsCount( 0 )
		, _markedFilesCount( 0 )
	{
	}

	CDataManager::~CDataManager()
	{
		PrintDebug("dtor");
	}


	//
	// Initialize directory watcher and background worker threads
	//
	void CDataManager::init( HWND hWndNotify )
	{
		// the disk reader is not destroyed when switching readers
		_spDiskReader = std::make_shared<CDiskReader>( _sortMode, hWndNotify );

		_hWndNotify = hWndNotify;
		_spReader = createReader( EReadMode::Disk );
	}

	//
	// Create particular reader instance (CDiskReader is created only once)
	//
	std::shared_ptr<CBaseReader> CDataManager::createReader( EReadMode mode )
	{
		_currentMode = mode;

		switch( mode )
		{
		case EReadMode::Disk:
			_spDataProvider = std::make_shared<CDiskDataProvider>();
			return _spDiskReader;
		case EReadMode::Archive:
			_spDataProvider = std::make_shared<CArchiveDataProvider>();
			return std::make_shared<CArchiveReader>( _sortMode, _hWndNotify );
		case EReadMode::Network:
			return std::make_shared<CNetworkReader>( _sortMode, _hWndNotify );
		case EReadMode::Reged:
			_spDataProvider = std::make_shared<CRegistryDataProvider>();
			return std::make_shared<CRegistryReader>( _sortMode, _hWndNotify );
		case EReadMode::Ftp:
			_spDataProvider = std::make_shared<CFtpDataProvider>();
			if( _spFtpData != nullptr ) return std::make_shared<CFtpReader>( _sortMode, _hWndNotify, _spFtpData );
			else break;
		case EReadMode::Putty:
			_spDataProvider = std::make_shared<CSshDataProvider>();
			if( _spSshData != nullptr ) return std::make_shared<CSshReader>( _sortMode, _hWndNotify, _spSshData );
			else break;
		}

		PrintDebug( "Invalid reading mode (%d)", mode );

		return nullptr;
	}

	//
	// Read files/directories from a given source (disk, archive, network, etc.)
	//
	bool CDataManager::readFromPath( std::wstring& path )
	{
		int idx = getEntryIndex( PathUtils::stripPath( path ) );
		bool isFile = isEntryFile( idx );
		
		// clean up
		_markedEntriesSize = 0ull;

		auto reqMode = ReaderType::getRequestedMode( path, isFile );

		if( ( !isInArchiveMode() && _currentMode != reqMode ) ||
			( isInArchiveMode() && !_spReader->isPathValid( path ) ) )
		{
			// create reader according to a requested mode, use a disk reader when failed
			auto spReader = createReader( reqMode );

			_spReader = spReader ? spReader : _spDiskReader;

			// session data for ftp/ssh does not exist
			if( !spReader )
				return false;
		}

		// try to read entries
		bool ret = _spReader->readEntries( path );

		// a hack for the network mode - read from the disk after selecting a directory
		if( ret && _spReader->getStatus() == EReaderStatus::Invalid )
		{
			_spReader = _spDiskReader;
			_spReader->readEntries( path );
		}

		// update current directory
		_currentDirectory = PathUtils::addDelimiter( path );

		return true;
	}

	//
	// On worker thread notification message
	//
	void CDataManager::onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal )
	{
		if( _spReader )
			_spReader->onWorkerNotify( msgId, workerId, retVal );

		if( msgId == UM_CALCSIZEDONE && retVal == 2 )
		{
			_markedEntriesSize = _spReader->getResultSize();
			_propertiesString = FsUtils::getFormatStrItemsCount( _markedFilesCount, _markedDirsCount, _markedEntriesSize, true );
		//	PrintDebug("files:%d, dirs:%d, size:%zu", _markedFilesCount, _markedDirsCount, _markedEntriesSize);
		}
	}

	//
	// Check whether path has an attribute set
	//
	bool CDataManager::isEntryAttrSet( int entryIndex, DWORD attrib )
	{
		auto entryData = _spReader->getEntry( entryIndex );

		if( entryData )
		{
			return ( entryData->wfd.dwFileAttributes & attrib ) ? true : false;
		}

		return false;
	}

	//
	// Check whether path has a hard-link
	//
	bool CDataManager::isEntryHardLink( int entryIndex )
	{
		auto entryData = _spReader->getEntry( entryIndex );

		if( entryData )
		{
			if( !isInDiskMode() )
			{
				// show soft/hard links in archive as hardlink
				return isEntryAttrSet( entryIndex, FILE_ATTRIBUTE_REPARSE_POINT );
			}
			else
				return ( entryData->hardLinkCnt > 1 );
		}

		return false;
	}

	//
	// Check whether path is a fixed entry - cannot be renamed
	//
	bool CDataManager::isEntryFixed( int entryIndex )
	{
		return PathUtils::isDirectoryDoubleDotted( getEntryName( entryIndex ) );
	}

	//
	// Check whether we are in archive mode - read-only
	//
	bool CDataManager::isInReadOnlyMode() const
	{
		if( isInArchiveMode() || isInNetworkMode() )
		{
			return true;
		}

		return false;
	}

	//
	// Check whether path is a file
	//
	bool CDataManager::isEntryFile( int entryIndex )
	{
		return getEntryType( entryIndex ) == EEntryType::File;
	}

	//
	// Get entry attributes - file, directory
	//
	EEntryType CDataManager::getEntryType( int entryIndex ) const
	{
		auto entryData = _spReader->getEntry( entryIndex );

		if( entryData )
		{
			return ( IsDir( entryData->wfd.dwFileAttributes ) ) ?
				( ( IsReparse( entryData->wfd.dwFileAttributes ) ) ?
					EEntryType::Reparse : EEntryType::Directory ) : EEntryType::File;
		}

		return EEntryType::NotFound;
	}

	//
	// Get entry internal name
	//
	const TCHAR *CDataManager::getEntryName( int entryIndex ) const
	{
		auto entryData = _spReader->getEntry( entryIndex );

		if( entryData )
		{
			return entryData->wfd.cFileName;
		}

		return L"";
	}

	//
	// Get entry internal size as string
	//
	const TCHAR *CDataManager::getEntrySize( int entryIndex ) const
	{
		auto entryData = _spReader->getEntry( entryIndex );

		if( entryData )
		{
			return entryData->size.c_str();
		}

		return L"";
	}

	//
	// Get entry internal date as string
	//
	const TCHAR *CDataManager::getEntryDate( int entryIndex ) const
	{
		auto entryData = _spReader->getEntry( entryIndex );

		if( entryData )
		{
			return entryData->date.c_str();
		}

		return L"";
	}

	//
	// Get entry internal time as string
	//
	const TCHAR *CDataManager::getEntryTime( int entryIndex ) const
	{
		auto entryData = _spReader->getEntry( entryIndex );

		if( entryData )
		{
			return entryData->time.c_str();
		}

		return L"";
	}

	//
	// Get entry internal attributes as string
	//
	const TCHAR *CDataManager::getEntryAttr( int entryIndex ) const
	{
		auto entryData = _spReader->getEntry( entryIndex );

		if( entryData )
		{
			return entryData->attr.c_str();
		}

		return L"";
	}

	//
	// Get entry internal image index
	//
	int CDataManager::getEntryImageIndex( int entryIndex ) const
	{
		auto entryData = _spReader->getEntry( entryIndex );

		if( entryData )
		{
			return _spReader->isOverlaysDone() ? entryData->imgOverlay : entryData->imgSystem;
		}

		return 0;
	}

	//
	// Get filename with full path
	//
	std::wstring CDataManager::getEntryPathFull( const std::wstring& entryName ) const
	{
		return _currentDirectory + entryName;
	}

	//
	// Get filename with full path
	//
	std::wstring CDataManager::getEntryPathFull( int entryIndex ) const
	{
		return getEntryPathFull( getEntryName( entryIndex ) );
	}

	//
	// Rename entry internaly and on disk
	//
	bool CDataManager::renameEntry( int entryIndex, const std::wstring& fileName )
	{
		bool retVal = false;
		auto entryData = _spReader->getEntry( entryIndex );

		if( entryData )
		{
			retVal = _spReader->renameEntry(
				( _currentDirectory + entryData->wfd.cFileName ).c_str(),
				( _currentDirectory + fileName ).c_str() );

		//	if( retVal )
		//	{
		//		wcsncpy( entryData->wfd.cFileName, fileName.c_str(), ARRAYSIZE( WIN32_FIND_DATA::cFileName ) );
		//	}
		//	else
		//		SET_ERROR_MESSAGE( "MoveFile" );
		}

		return retVal;
	}

	//
	// Get icon entries indices
	//
	//void CDataManager::loadAssociatedlIcons( int cacheFrom, int cacheTo ) const
	//{
	//	if( !isInRegedMode() )
	//	{
	//		for( int i = cacheFrom; i <= cacheTo; ++i )
	//		{
	//			auto entryData = _spReader->getEntry( i );

	//			if( entryData && entryData->imgSystem == 0 )
	//			{
	//				if( getEntryType( i ) == EEntryType::File )
	//				{
	//					auto ext = wcsrchr( entryData->wfd.cFileName, L'.' );
	//					auto idx = IconUtils::getFromPathWithAttribIndex( ext && ext[1] != L'\0' ? ext : L".*", entryData->wfd.dwFileAttributes );

	//					entryData->imgSystem = ( idx < 0 ? 0 : idx );
	//				}
	//				else
	//					entryData->imgSystem = IconUtils::getFromPathWithAttribIndex( L" ", FILE_ATTRIBUTE_DIRECTORY );
	//			}
	//		}
	//	}
	//}

	//
	// Count selected files and directories
	//
	void CDataManager::countEntries( const std::vector<int>& markedEntries, int& filesCnt, int& dirsCnt )
	{
		ULONGLONG fileSizeResult = 0ull;
		filesCnt = dirsCnt = 0;

		for( const auto& idx : markedEntries )
		{
			auto entryData = _spReader->getEntry( idx );

			if( entryData )
			{
				switch( getEntryType( idx ) )
				{
				case EEntryType::File:
					filesCnt++;
					break;

				case EEntryType::Directory:
					dirsCnt++;
					break;

				default:
					break;
				}
			}
		}
	}

	//
	// Get selected file/directory properties
	//
	std::wstring CDataManager::getPropertiesSingle( int entryIndex )
	{
		std::wostringstream sstr;

		if( _spReader->getStatus() == EReaderStatus::ReadingData )
		{
			switch( _currentMode )
			{
			case EReadMode::Archive:
				sstr << L"Opening archive..";
				break;
			case EReadMode::Network:
				sstr << L"Scanning network..";
				break;
			case EReadMode::Ftp:
			case EReadMode::Putty:
				sstr << L"Waiting for response..";
				break;
			case EReadMode::Disk:
			default:
				sstr << L"Reading tree..";
				break;
			}
		}
		else
		{
			auto entryData = _spReader->getEntry( entryIndex );
			if( entryData )
			{
				sstr << entryData->wfd.cFileName;

				if( !entryData->link.empty() )
					sstr << L" -> " << entryData->link;

				if( !isInNetworkMode() )
				{
					sstr << L": " << FsUtils::getFormatStrFileSize( &entryData->wfd );
					sstr << L", " << FsUtils::getFormatStrFileDate( &entryData->wfd/*, !isInArchiveMode()*/ ); // TODO: convert to local time ??
					sstr << L", " << FsUtils::getFormatStrFileTime( &entryData->wfd/*, !isInArchiveMode()*/ );
					sstr << L", " << FsUtils::getFormatStrFileAttr( &entryData->wfd );

					_markedEntriesSize = FsUtils::getFileSize( &entryData->wfd );
				}
			}
		}

		return sstr.str();
	}

	//
	// Update properties text according to current entry's selection
	//
	void CDataManager::updateProperties( int entryIndex )
	{
		_markedEntriesSize = 0ull;

		if( !_markedEntries.empty() )
		{
			countEntries( _markedEntries, _markedFilesCount, _markedDirsCount );
			_propertiesString = L"(Calculating size) " + FsUtils::getFormatStrItemsCount( _markedFilesCount, _markedDirsCount );

			// when calc async, properties string is set in onWorkerNotify
			if( _spReader->calculateSize( _markedEntries ) )
			{
				_markedEntriesSize = _spReader->getResultSize();
				_propertiesString = FsUtils::getFormatStrItemsCount( _markedFilesCount, _markedDirsCount, _markedEntriesSize, true );
			}
		}
		else
		{
			_spDiskReader->calculateSize( {} ); // stop any ongoing calculations
			_propertiesString = getPropertiesSingle( entryIndex );
		}
	}
}
