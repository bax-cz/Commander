#include "stdafx.h"

#include "Commander.h"
#include "TypeTar.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CArchTar::CArchTar( TarLib::ECompressionType type ) : _compressionType( type )
	{
	}

	CArchTar::~CArchTar()
	{
	}

	//
	// Open archive
	//
	bool CArchTar::readEntries()
	{
		// create tar file with path and read mode
		TarLib::File tar( _fileName, TarLib::EFileMode::tarModeRead, _compressionType );

		// get the first entry
		TarLib::Entry tarEntry = tar.getFirstEntry();

		EntryData entry = { 0 };
		while( !tarEntry.isEmpty() )
		{
			if( !_pWorker->isRunning() )
				return false;

			getEntryInfo( tarEntry, &entry.wfd );

			entry.link = StringUtils::convert2W( tarEntry.header.linkedFileName );

			if( entry.wfd.dwFileAttributes && wcslen( entry.wfd.cFileName ) )
				_dataEntries.push_back( entry );

			// get the next entry in the TAR archive
			tarEntry = tar.getNextEntry();
		}

		_errorMessage = tar.getErrorMessage();

		return _errorMessage.empty();
	}

	//
	// Extract particular file from archive
	//
	void CArchTar::extractFile( TarLib::Entry& entry, const std::wstring& path )
	{
		FsUtils::makeDirectory( PathUtils::stripFileName( path ) );

		std::ofstream ofs( PathUtils::getExtendedPath( path ).c_str(), std::ios::binary );

		size_t readSize = 0;
		while( _pWorker->isRunning() && ( readSize = entry.read( _dataBuf, sizeof( _dataBuf ) ) ) > 0 )
		{
			ofs.write( _dataBuf, readSize );
			_pWorker->sendNotify( FC_ARCHBYTESRELATIVE, (LPARAM)readSize );
		}

		ofs.close();

		auto hFile = CreateFile( PathUtils::getExtendedPath( path ).c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
		auto ftime = FsUtils::unixTimeToFileTime( entry.header.unixTime );
		SetFileTime( hFile, &ftime, &ftime, &ftime );

		CloseHandle( hFile );
	}

	//
	// Extract selected files to given directory
	//
	bool CArchTar::extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir )
	{
		TarLib::File tar( _fileName, TarLib::EFileMode::tarModeRead, _compressionType );

		// get the first entry
		TarLib::Entry entry = tar.getFirstEntry();
		while( !entry.isEmpty() && _pWorker->isRunning() )
		{
			auto outName = StringUtils::convert2W( TarLib::Header::getFileName( entry.header )/*, CP_ACP*/ );
			PathUtils::unifyDelimiters( outName );

			if( shouldExtractEntry( entries, outName ) )
			{
				_pWorker->sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)outName.c_str() );

				auto path = PathUtils::getFullPath( targetDir + outName );

				/*if( entry.header.indicator == TarLib::EEntryType::tarEntrySymlink )
				{
					// TODO: CreateSymbolicLink requires elevated process privileges
					auto target = PathUtils::getFullPath( PathUtils::addDelimiter( PathUtils::stripFileName( path ) ) + StringUtils::convert2W( entry.header.linkedFileName ) );
					DWORD ret = CreateSymbolicLink( PathUtils::getExtendedPath( path ).c_str(), PathUtils::getExtendedPath( target ).c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY );
					if( ret == 0 )
						ret = GetLastError();
				}
				else if( entry.header.indicator == TarLib::EEntryType::tarEntryHardLink )
				{
					auto target = PathUtils::getFullPath( PathUtils::addDelimiter( PathUtils::stripFileName( path ) ) + StringUtils::convert2W( entry.header.linkedFileName ) );
					CreateHardLink( PathUtils::getExtendedPath( path ).c_str(), PathUtils::getExtendedPath( target ).c_str(), NULL );
				}
				else*/
					extractFile( entry, path );
			}

			// get the next entry in the TAR archive
			entry = tar.getNextEntry();
		}

		return true;
	}

	//
	// Pack selected files to given archive name
	//
	bool CArchTar::packEntries( const std::vector<std::wstring>& entries, const std::wstring& targetName )
	{
		TarLib::File tar( targetName, TarLib::EFileMode::tarModeWrite, _compressionType );

		// TODO
		_errorMessage = L"CArchTar: Packing files is not implemented yet.";

		return false;
	}

	//
	// Convert Tar entry header to WIN32_FIND_DATA
	//
	void CArchTar::getEntryInfo( const TarLib::Entry& entry, WIN32_FIND_DATA *wfd )
	{
		auto path = StringUtils::convert2W( TarLib::Header::getFileName( entry.header )/*, CP_ACP*/ );
		PathUtils::unifyDelimiters( path );
		wcsncpy( wfd->cFileName, path.c_str(), ARRAYSIZE( WIN32_FIND_DATA::cFileName ) );

		LARGE_INTEGER size;
		size.QuadPart = entry.header.fileSize;
		wfd->nFileSizeLow = size.LowPart;
		wfd->nFileSizeHigh = size.HighPart;
		wfd->ftLastWriteTime = FsUtils::unixTimeToFileTime( entry.header.unixTime );
		wfd->dwFileAttributes = TarLib::Header::getFileAttributes( entry.header );
	}
}
