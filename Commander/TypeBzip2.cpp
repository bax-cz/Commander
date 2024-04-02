#include "stdafx.h"

#include "Commander.h"
#include "TypeBzip2.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CArchBzip2::CArchBzip2()
	{
	}

	CArchBzip2::~CArchBzip2()
	{
	}

	//
	// Open archive
	//
	bool CArchBzip2::readEntries()
	{
		EntryData entry = { 0 };
		LONGLONG fileSize = 0ll;

		_hArchive = BZ2_bzopenW( PathUtils::getExtendedPath( _fileName ).c_str(), L"rb" );

		if( _hArchive )
		{
			// calculate uncompressed file size
			int size = 0;
			while( _pWorker->isRunning() && ( size = BZ2_bzread( _hArchive, _dataBuf, sizeof( _dataBuf ) ) ) != 0 )
				fileSize += static_cast<LONGLONG>( size );

			getEntryInfo( fileSize, &entry.wfd );
			_dataEntries.push_back( entry );

			BZ2_bzclose( _hArchive );
			_hArchive = nullptr;

			return true;
		}

		return false;
	}

	//
	// Extract gziped file to given directory
	//
	bool CArchBzip2::extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir )
	{
		auto outName = PathUtils::stripFileExtension( PathUtils::stripPath( _fileName ) );
		auto path = PathUtils::getFullPath( targetDir + outName );

		_pWorker->sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)outName.c_str() );

		DWORD read, written = 0;
		HANDLE hFile = CreateFile( PathUtils::getExtendedPath( path ).c_str(), GENERIC_WRITE, NULL, NULL, CREATE_NEW, 0, NULL );
		_hArchive = BZ2_bzopenW( PathUtils::getExtendedPath( _fileName ).c_str(), L"rb" );

		// read file into buffer
		while( ( read = BZ2_bzread( _hArchive, _dataBuf, sizeof( _dataBuf ) ) ) != 0 )
		{
			if( !_pWorker->isRunning() )
			{
				CloseHandle( hFile );
				DeleteFile( PathUtils::getExtendedPath( path ).c_str() );
				BZ2_bzclose( _hArchive );
				return false;
			}

			_pWorker->sendNotify( FC_ARCHBYTESRELATIVE, read );
			WriteFile( hFile, _dataBuf, read, &written, NULL );
		}

		WIN32_FIND_DATA wfd;
		getEntryInfo( 0, &wfd );
		SetFileTime( hFile, &wfd.ftCreationTime, &wfd.ftLastAccessTime, &wfd.ftLastWriteTime );

		CloseHandle( hFile );
		BZ2_bzclose(_hArchive);
		_hArchive = nullptr;

		SetFileAttributes( PathUtils::getExtendedPath( path ).c_str(), wfd.dwFileAttributes );

		return true;
	}

	//
	// Get entry info as WIN32_FIND_DATA
	//
	void CArchBzip2::getEntryInfo( LONGLONG fileSize, WIN32_FIND_DATA *wfd )
	{
		if( FsUtils::getFileInfo( _fileName.c_str(), *wfd ) )
		{
			auto p = wcsrchr( wfd->cFileName, L'.' );
			if( p ) *p = L'\0';

			LARGE_INTEGER size;
			size.QuadPart = fileSize;
			wfd->nFileSizeLow = size.LowPart;
			wfd->nFileSizeHigh = size.HighPart;
		}
	}
}
