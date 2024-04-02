#include "stdafx.h"

#include "Commander.h"
#include "ArchiveReader.h"
#include "FileSystemUtils.h"
#include "MiscUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CArchiveReader::CArchiveReader( ESortMode mode, HWND hwndNotify )
		: CBaseReader( mode, hwndNotify )
	{
		ZeroMemory( &_wfdArchFile, sizeof( WIN32_FIND_DATA ) );
		_worker.init( [this] { return _openArchiveCore(); }, _hWndNotify, UM_ARCHOPENDONE );

		LvUtils::setImageList( hwndNotify, FCS::inst().getApp().getSystemImageList(), LVSIL_SMALL );
	}

	CArchiveReader::~CArchiveReader()
	{
		// stop workers
		_worker.stop();
	}

	//
	// Instantiate particular archiver according to extension
	//
	bool CArchiveReader::createArchiver( const std::wstring& fileName )
	{
		_upArchiver = ArchiveType::createArchiver( fileName );

		if( _upArchiver == nullptr )
		{
			_errorMsg = FORMAT( L"Unknown archive type: '%ls'", PathUtils::stripPath( fileName ).c_str() );
			return false;
		}

		return true;
	}

	//
	// Read archive content in background worker
	//
	bool CArchiveReader::_openArchiveCore()
	{
		if( FsUtils::getFileInfo( _upArchiver->getName(), _wfdArchFile ) )
			return _upArchiver->readEntries();

		_status = EReaderStatus::Invalid;

		return false;
	}

	void CArchiveReader::onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal )
	{
		if( msgId == UM_ARCHOPENDONE )
		{
			switch( retVal )
			{
			case FC_ARCHDONEOK:
				if( _status == EReaderStatus::ReadingData )
					_status = EReaderStatus::DataOk;
				break;
			case FC_ARCHDONEFAIL:
				_status = EReaderStatus::DataError;
				_errorMsg = _upArchiver->getErrorMessage();
				break;
			case FC_ARCHNEEDPASSWORD:
				{
					CTextInput::CParams params( L"Password", L"Enter password:", L"", true, true );
					auto res = MiscUtils::showTextInputBox( params, _hWndNotify );
					if( !res.empty() )
						wcsncpy( reinterpret_cast<wchar_t*>( workerId ), res.c_str(), MAXPASSWORD );
					else
						wcscpy( reinterpret_cast<wchar_t*>( workerId ), L"\0" );
				}
				break;
			case FC_ARCHNEEDNEXTVOLUME:
				{
					CTextInput::CParams params( L"Next Volume", L"Next volume required:", (LPCWSTR)workerId );
					auto res = MiscUtils::showTextInputBox( params, _hWndNotify );
					if( !res.empty() )
						wcsncpy( reinterpret_cast<wchar_t*>( workerId ), res.c_str(), MAX_PATH );
					else
						wcscpy( reinterpret_cast<wchar_t*>( workerId ), L"\0" );
				}
				break;
			default:
				break;
			}
		}
	}

	//
	// Check whether mode is still valid for given path
	//
	bool CArchiveReader::isPathValid( const std::wstring& path )
	{
		if( StringUtils::startsWith( path, _upArchiver->getName() ) )
			return true;

		return false;
	}

	//
	// Read data
	//
	bool CArchiveReader::readEntries( std::wstring& path )
	{
		if( !_upArchiver )
		{
			if( !createArchiver( path ) )
				return false;

			_status = EReaderStatus::ReadingData;
			_root = path;

			// set pointers to data items
			_upArchiver->init( path, &_fileEntries, &_dirEntries, &_worker );
			
			// add temporary double-dotted directory while reading archive
			_upArchiver->addParentDirEntry( _wfdArchFile );

			return _worker.start();
		}
		else if( isPathValid( path ) )
		{
			if( !_worker.isRunning() )
			{
				_dirEntries.clear();
				_fileEntries.clear();

				_upArchiver->addParentDirEntry( _wfdArchFile ); // add double-dotted directory first
				_upArchiver->readLocalPath( PathUtils::addDelimiter( path ) );

				// do sorting
				sortEntries();
			}

			return true;
		}
		
		_worker.stop();
		_upArchiver = nullptr;

		_status = EReaderStatus::Invalid;

		return false;
	}

	//
	//
	//
	bool CArchiveReader::copyEntries( const std::vector<std::wstring>& entries, const std::wstring& path )
	{
		return false;
	}

	//
	//
	//
	bool CArchiveReader::moveEntries( const std::vector<std::wstring>& entries, const std::wstring& path )
	{
		return false;
	}

	//
	//
	//
	bool CArchiveReader::renameEntry( const std::wstring& path, const std::wstring& newName )
	{
		return false;
	}

	//
	//
	//
	bool CArchiveReader::deleteEntries( const std::vector<std::wstring>& entries )
	{
		return false;
	}

	//
	// Calculate size - return true means operation successfully done on ui thread
	//
	bool CArchiveReader::calculateSize( const std::vector<int>& entries )
	{
		_resultSize = 0ull;

		for( const auto& idx : entries )
		{
			auto entryData = getEntry( idx );

			if( entryData )
			{
				if( IsDir( entryData->wfd.dwFileAttributes ) )
					_upArchiver->calculateDirectorySize( entryData->wfd.cFileName, &_resultSize );
				else
					_resultSize += FsUtils::getFileSize( &entryData->wfd );
			}
		}

		return true;
	}
}
