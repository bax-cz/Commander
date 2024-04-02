#include "stdafx.h"

#include "Commander.h"
#include "TypeIso.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CArchIso::CArchIso()
	{
	}

	CArchIso::~CArchIso()
	{
	}

	//
	// Open archive
	//
	bool CArchIso::readEntries()
	{
		std::wstring fileName = PathUtils::getExtendedPath( _fileName ).c_str();

		tOpenArchiveDataW arcData = { 0 };
		arcData.ArcName = fileName.c_str();

		_hArchive = IsoOpenArchiveW( &arcData );

		if( _hArchive )
		{
			EntryData entry = { 0 };
			tHeaderDataExW hdr = { 0 };

			while( _pWorker->isRunning() && IsoReadHeaderExW( _hArchive, &hdr ) == 0 )
			{
				IsoProcessFileW( _hArchive, PK_SKIP, nullptr, nullptr );

				getEntryInfo( hdr, &entry.wfd );

				// combine big file from its parts
				if( !_dataEntries.empty() && wcsncmp( entry.wfd.cFileName, _dataEntries.rbegin()->wfd.cFileName, MAX_PATH ) == 0 )
				{
					WIN32_FIND_DATA& wfd = _dataEntries.rbegin()->wfd;

					ULARGE_INTEGER size { wfd.nFileSizeLow, wfd.nFileSizeHigh };
					size.QuadPart += FsUtils::getFileSize( &entry.wfd );

					wfd.nFileSizeLow = size.LowPart;
					wfd.nFileSizeHigh = size.HighPart;
				}
				else
					_dataEntries.push_back( entry );
			}

			IsoCloseArchive( _hArchive );
			_hArchive = nullptr;

			return true;
		}

		return false;
	}

	//
	// Process data callback function
	//
	int CArchIso::processDataProc( wchar_t *fileName, int size, LPARAM userData )
	{
		CBackgroundWorker *pWorker = reinterpret_cast<CBackgroundWorker*>( userData );

		if( pWorker->isRunning() )
		{
			pWorker->sendNotify( FC_ARCHBYTESRELATIVE, static_cast<LPARAM>( size ) );
			return 1;
		}

		return 0;
	}

	//
	// Extract files to given directory
	//
	bool CArchIso::extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir )
	{
		std::wstring fileName = PathUtils::getExtendedPath( _fileName ).c_str();

		tOpenArchiveDataW arcData = { 0 };
		arcData.ArcName = fileName.c_str();

		_hArchive = IsoOpenArchiveW( &arcData );

		if( _hArchive )
		{
			WIN32_FIND_DATA wfd = { 0 };
			tHeaderDataExW hdr = { 0 };
			std::wstring outputNameFull;

			IsoSetProcessDataProcW( _hArchive, processDataProc, reinterpret_cast<LPARAM>( _pWorker ) );

			while( _pWorker->isRunning() && IsoReadHeaderExW( _hArchive, &hdr ) == 0 )
			{
				std::wstring outName = hdr.FileName;
				if( shouldExtractEntry( entries, outName ) )
				{
					outputNameFull = PathUtils::getFullPath( targetDir + outName ); // sanitize filename

					_pWorker->sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)outName.c_str() );

					if( hdr.FileAttr & FILE_ATTRIBUTE_DIRECTORY )
					{
						FsUtils::makeDirectory( outputNameFull );
						SetFileAttributes( PathUtils::getExtendedPath( outputNameFull ).c_str(), hdr.FileAttr );

						HANDLE hFile = CreateFile( PathUtils::getExtendedPath( outputNameFull ).c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );

						FILETIME ftime;
						DosDateTimeToFileTime( HIWORD( hdr.FileTime ), LOWORD( hdr.FileTime ), &ftime );
						SetFileTime( hFile, &ftime, &ftime, &ftime );

						CloseHandle( hFile );

						IsoProcessFileW( _hArchive, PK_SKIP, nullptr, nullptr );
					}
					else
					{
						FsUtils::makeDirectory( PathUtils::stripFileName( outputNameFull ) );
						IsoProcessFileW( _hArchive, PK_EXTRACT, nullptr, PathUtils::getExtendedPath( outputNameFull ).c_str() );
					}
				}
				else // skip non-listed files
					IsoProcessFileW( _hArchive, PK_SKIP, nullptr, nullptr );
			}

			IsoCloseArchive( _hArchive );
			_hArchive = nullptr;

			return true;
		}

		return false;
	}

	//
	// Get entry info as WIN32_FIND_DATA
	//
	void CArchIso::getEntryInfo( tHeaderDataExW& entry, WIN32_FIND_DATA *wfd )
	{
		FILETIME ftime;
		if( !DosDateTimeToFileTime( HIWORD( entry.FileTime ), LOWORD( entry.FileTime ), &ftime ) )
		{
			SYSTEMTIME st;
			GetSystemTime( &st );
			SystemTimeToFileTime( &st, &ftime );
		}

		// get the filename and other information from the header data
		wcsncpy( wfd->cFileName, entry.FileName, ARRAYSIZE( WIN32_FIND_DATA::cFileName ) );
		wfd->nFileSizeLow = entry.UnpSize; // get unpacked sizes
		wfd->nFileSizeHigh = entry.UnpSizeHigh;
		wfd->dwFileAttributes = entry.FileAttr;
		wfd->ftLastWriteTime = ftime;
	}
}
