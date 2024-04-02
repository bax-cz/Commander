#include "stdafx.h"

#include "Commander.h"
#include "MiscUtils.h"
#include "TypeZip.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CArchZip::CArchZip()
	{
		ZeroMemory( _passwd, sizeof( _passwd ) );
		_hArchive = nullptr;
	}

	CArchZip::~CArchZip()
	{
	}

	//
	// Open archive
	//
	bool CArchZip::readEntries()
	{
		int res = UNZ_ERRNO;

		if( ( _hArchive = unzOpen64( PathUtils::getExtendedPath( _fileName ).c_str() ) ) != NULL )
		{
			EntryData entry = { 0 };
			res = unzGetGlobalInfo64( _hArchive, &_globalInfo );

			char fileNameIntern[MAX_PATH] = { 0 };

			res = unzGoToFirstFile( _hArchive );
			while( res == UNZ_OK )
			{
				if( !_pWorker->isRunning() ) {
					_errorMessage = L"Canceled by user.";
					break;
				}

				unzGetCurrentFileInfo64( _hArchive, &_fileInfo, fileNameIntern, MAX_PATH, NULL, 0, NULL, 0 );
				getEntryInfo( _fileInfo, fileNameIntern, entry.wfd );

				_dataEntries.push_back( entry );
	
				res = unzGoToNextFile( _hArchive );
			}

			res = unzClose( _hArchive );
			_hArchive = nullptr;
		}
		else
			_errorMessage = getErrorMessage( res );

		return res == UNZ_OK;
	}

	std::wstring CArchZip::getErrorMessage( int errorCode )
	{
		std::wostringstream outMessage;
		outMessage << L"Error (" << errorCode << L"): ";

		switch( errorCode )
		{
		case UNZ_OK: // same value as Z_OK and UNZ_EOF
			outMessage << L"No error or end of file reached.";
			break;

		case Z_NEED_DICT:
			outMessage << L"Need dictionary.";
			break;
		case Z_STREAM_END:
			outMessage << L"End of stream.";
			break;
		case Z_ERRNO: // same value as UNZ_ERRNO
			outMessage << L"Zip file error.";
			break;
		case Z_STREAM_ERROR:
			outMessage << L"Zip stream error.";
			break;
		case Z_DATA_ERROR:
			outMessage << L"Zip data error.";
			break;
		case Z_MEM_ERROR:
			outMessage << L"Insufficient memory.";
			break;
		case Z_BUF_ERROR:
			outMessage << L"Buffer error.";
			break;
		case Z_VERSION_ERROR:
			outMessage << L"Incompatible version.";
			break;

		case UNZ_END_OF_LIST_OF_FILE:
			outMessage << L"End of list of files.";
			break;
		case UNZ_PARAMERROR:
			outMessage << L"Wrong parameters.";
			break;
		case UNZ_BADZIPFILE:
			outMessage << L"Bad zip file.";
			break;
		case UNZ_INTERNALERROR:
			outMessage << L"Internal error.";
			break;
		case UNZ_CRCERROR:
			outMessage << L"Crc error.";
			break;
		}

		return outMessage.str();
	}

	//
	// Extract particular file from archive
	//
	int CArchZip::extractFile( const std::wstring& path )
	{
		int res = UNZ_ERRNO;

		// first bit means data encryption
		if( ( _fileInfo.flag & 1 ) == 1 )
		{
			// ask user for a password
			if( wcslen( _passwd ) == 0 )
				_pWorker->sendMessage( FC_ARCHNEEDPASSWORD, reinterpret_cast<LPARAM>( _passwd ) );

			if( wcslen( _passwd ) == 0 )
			{
				_errorMessage = L"Error: Missing password.";
				return UNZ_ERRNO;
			}
			res = unzOpenCurrentFilePassword( _hArchive, StringUtils::convert2A( _passwd ).c_str() );
		}
		else
			res = unzOpenCurrentFile( _hArchive );

		if( res != UNZ_OK )
			return res;

		// create directory as a precaution
		FsUtils::makeDirectory( PathUtils::stripFileName( path ) );

		std::wstring pathTemp = path;

		// try to create new file
		DWORD read, written = 0;
		DWORD flags = ( _action == EExtractAction::Overwrite ? CREATE_ALWAYS : CREATE_NEW );

		HANDLE hFile = CreateFile( PathUtils::getExtendedPath( path ).c_str(), GENERIC_WRITE, NULL, NULL, flags, 0, NULL );
		DWORD errorId = GetLastError();

		// process conflicting file name
		if( hFile == INVALID_HANDLE_VALUE && errorId == ERROR_FILE_EXISTS && _action == EExtractAction::Rename )
		{
			WCHAR fileNameNew[MAX_PATH] = L"";
			wcsncpy( fileNameNew, PathUtils::stripPath( pathTemp ).c_str(), MAX_PATH );

			// query new name
			_pWorker->sendMessage( FC_ARCHNEEDNEWNAME, reinterpret_cast<LPARAM>( fileNameNew ) );

			if( wcslen( reinterpret_cast<wchar_t*>( fileNameNew ) ) != 0 )
			{
				// use the new name
				pathTemp = PathUtils::addDelimiter( PathUtils::stripFileName( path ) ) + fileNameNew;
				hFile = CreateFile( PathUtils::getExtendedPath( pathTemp ).c_str(), GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, 0, NULL );
				errorId = GetLastError();
			}
			else
			{
				// skip this file
				unzCloseCurrentFile( _hArchive );
				return res;
			}
		}

		if( hFile != INVALID_HANDLE_VALUE )
		{
			// read file into buffer
			while( ( res = unzReadCurrentFile( _hArchive, _dataBuf, sizeof( _dataBuf ) ) ) > 0 )
			{
				if( !_pWorker->isRunning() )
				{
					CloseHandle( hFile );
					DeleteFile( PathUtils::getExtendedPath( pathTemp ).c_str() );
					unzCloseCurrentFile( _hArchive );
					return res;
				}

				read = (DWORD)res;
				_pWorker->sendNotify( FC_ARCHBYTESRELATIVE, read );
				WriteFile( hFile, _dataBuf, read, &written, NULL );
			}

			if( res != UNZ_OK )
			{
				_errorMessage = getErrorMessage( res );
				unzCloseCurrentFile( _hArchive );

				CloseHandle( hFile );
				DeleteFile( PathUtils::getExtendedPath( pathTemp ).c_str() );

				return res;
			}

			FILETIME ftime;
			DosDateTimeToFileTime( HIWORD( _fileInfo.dosDate ), LOWORD( _fileInfo.dosDate ), &ftime );
			SetFileTime( hFile, &ftime, &ftime, &ftime );
			CloseHandle( hFile );

			SetFileAttributes( PathUtils::getExtendedPath( pathTemp ).c_str(), LOBYTE( _fileInfo.external_fa ) );
		}
		else if( _action != EExtractAction::Skip )
			_errorMessage = SysUtils::getErrorMessage( errorId );

		unzCloseCurrentFile( _hArchive );

		return res;
	}

	//
	// Extract archive entries to given directory
	//
	bool CArchZip::extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir )
	{
		int res = UNZ_ERRNO;

		if( ( _hArchive = unzOpen64( PathUtils::getExtendedPath( _fileName ).c_str() ) ) != nullptr )
		{
			res = unzGetGlobalInfo64( _hArchive, &_globalInfo );
			char fileName[MAX_PATH] = { 0 };
			wchar_t tempName[MAX_PATH] = { 0 };

			res = unzGoToFirstFile( _hArchive );

			while( _pWorker->isRunning() && res == UNZ_OK )
			{
				unzGetCurrentFileInfo64( _hArchive, &_fileInfo, fileName, MAX_PATH, NULL, 0, NULL, 0 );

				auto outName = StringUtils::convert2W( fileName, IsUTF8( fileName ) ? CP_UTF8 : CP_OEMCP );
				PathUtils::unifyDelimiters( outName );

				if( shouldExtractEntry( entries, outName ) )
				{
					// give user ability to rename current file
					wcsncpy( tempName, outName.c_str(), MAX_PATH );
					_pWorker->sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)tempName );
					outName = tempName;

					if( StringUtils::endsWith( outName, WSTRING_PATH_SEPARATOR ) )
						FsUtils::makeDirectory( PathUtils::getFullPath( targetDir + outName ) );
					else
						res = extractFile( PathUtils::getFullPath( targetDir + outName ) );
				}

				if( res != UNZ_OK || !_errorMessage.empty() )
				{
					if( _errorMessage.empty() )
						_errorMessage = getErrorMessage( res );

					unzClose( _hArchive );
					return false;
				}

				res = unzGoToNextFile( _hArchive );
			}

			_errorMessage = getErrorMessage( res );

			res = unzClose( _hArchive );
			_hArchive = nullptr;

			return res == UNZ_OK;
		}
		else
			_errorMessage = getErrorMessage( res );

		return false;
	}

	//
	// Extract archive to given directory
	//
	/*bool CArchZip::extractArchive( const std::wstring& targetDir )
	{
		if( ( _hArchive = unzOpen64( _fileName.c_str() ) ) != NULL )
		{
			bool ret = unzGetGlobalInfo64( _hArchive, &_globalInfo ) == UNZ_OK;

			char fileName[MAX_PATH] = { 0 };
			DWORD written = 0;

			unzGoToFirstFile( _hArchive );

			while( ret )
			{
				unzGetCurrentFileInfo64( _hArchive, &_fileInfo, fileName, MAX_PATH, NULL, 0, NULL, 0 );
				auto len = _fileInfo.uncompressed_size;

				// read file into buffer
				char *dataBuf = new char[len];

				unzOpenCurrentFile( _hArchive );
				unzReadCurrentFile( _hArchive, dataBuf, (unsigned int)len );
				auto path = PathUtils::getFullPath( targetDir + StringUtils::convert2W( fileName ) );
				FsUtils::makeDirectory( PathUtils::stripFileName( path ) );
				HANDLE hFile = CreateFile( path.c_str(), GENERIC_WRITE, NULL, NULL, CREATE_NEW, 0, NULL );
				WriteFile( hFile, dataBuf, (DWORD)len, &written, NULL );
				CloseHandle( hFile );
				unzCloseCurrentFile( _hArchive );

				delete[] dataBuf;
	
				ret = unzGoToNextFile( _hArchive ) == UNZ_OK;
			}

			ret = unzClose( _hArchive ) == UNZ_OK;

			_hArchive = nullptr;
			_isOpen = false;

			return ret;
		}

		return false;
	}*/

	//
	// Pack file into archive
	//
	bool CArchZip::packFile( zipFile hZip, const std::wstring& fileName, const WIN32_FIND_DATA& wfd )
	{
		HANDLE hFile = CreateFile( PathUtils::getExtendedPath( fileName ).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
		if( hFile != INVALID_HANDLE_VALUE )
		{
			_pWorker->sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)fileName.c_str() );

			DWORD read = 0;
			WORD date = 0, time = 0;
			zip_fileinfo zfi = { 0 };

			FileTimeToDosDateTime( &wfd.ftLastWriteTime, &date, &time );
			zfi.dosDate = MAKELONG( time, date );
			zfi.external_fa = wfd.dwFileAttributes;
			zfi.internal_fa = LOWORD( wfd.dwFileAttributes );

			auto interName = fileName.substr( _localPathIdx );
			PathUtils::unifyDelimiters( interName, false );
			if( zipOpenNewFileInZip4_64( hZip, StringUtils::convert2A( interName ).c_str(), &zfi, NULL, 0, NULL, 0, NULL,
				Z_DEFLATED, Z_DEFAULT_COMPRESSION, 0, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, NULL, 0, VERSIONMADEBY, 0x800/*UTF-8*/, 1/*zip64*/ ) == Z_OK )
			{
				while( ReadFile( hFile, _dataBuf, sizeof( _dataBuf ), &read, NULL ) && _pWorker->isRunning() && read != 0 )
				{
					zipWriteInFileInZip( hZip, _dataBuf, read );
					_pWorker->sendNotify( FC_ARCHBYTESRELATIVE, read );
				}

				zipCloseFileInZip( hZip );
			}

			CloseHandle( hFile );
		}

		return true;
	}

	//
	// Pack directory into archive
	//
	bool CArchZip::packDirectory( zipFile hZip, const std::wstring& dirName )
	{
		auto pathFind = PathUtils::addDelimiter( dirName );

		WIN32_FIND_DATA wfd = { 0 };
		HANDLE hFile = FindFirstFileEx( ( PathUtils::getExtendedPath( pathFind ) + L"*" ).c_str(), FindExInfoStandard, &wfd, FindExSearchNameMatch, NULL, FCS::inst().getApp().getFindFLags() );
		if( hFile != INVALID_HANDLE_VALUE )
		{
			do
			{
				if( ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
				{
					if( !( wfd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT )
						&& !PathUtils::isDirectoryDotted( wfd.cFileName )
						&& !PathUtils::isDirectoryDoubleDotted( wfd.cFileName ) )
					{
						packDirectory( hZip, pathFind + wfd.cFileName );
					}
				}
				else
				{
					packFile( hZip, pathFind + wfd.cFileName, wfd );
				}
			} while( FindNextFile( hFile, &wfd ) && _pWorker->isRunning() );

			FindClose( hFile );
		}

		return false;
	}

	//
	// Pack entries into archive to given directory
	//
	bool CArchZip::packEntries( const std::vector<std::wstring>& entries, const std::wstring& targetNane )
	{
		if( !entries.empty() )
		{
			_localPathIdx = PathUtils::stripFileName( entries[0] ).length() + 1;

			zipFile hZip = zipOpen64( PathUtils::getExtendedPath( targetNane ).c_str(), APPEND_STATUS_CREATE );

			WIN32_FIND_DATA wfd;

			if( hZip )
			{
				for( const auto& entry : entries )
				{
					if( !FsUtils::getFileInfo( entry, wfd ) )
						PrintDebug( "Entry '%ls' not found", entry.c_str() );

					if( FsUtils::checkAttrib( wfd.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY ) )
						packDirectory( hZip, entry );
					else
						packFile( hZip, entry, wfd );

					if( !_pWorker->isRunning() )
					{
						zipClose( hZip, nullptr );
						DeleteFile( PathUtils::getExtendedPath( targetNane ).c_str() );
						return false;
					}
				}

				zipClose( hZip, nullptr );

				return true;
			}
		}

		return false;
	}

	//
	// Convert entry header to WIN32_FIND_DATA
	//
	void CArchZip::getEntryInfo( const unz_file_info64& fileInfo, const char *fileName, WIN32_FIND_DATA& wfd )
	{
		/*if( fileInfo.size_file_extra )
		{
			auto ret = unzOpenCurrentFile( _hArchive );
			char *buf = new char[fileInfo.size_file_extra];
			ret = unzGetLocalExtrafield(_hArchive, buf, fileInfo.size_file_extra);
			auto str = StringUtils::convert2W( buf );
			ret = unzCloseCurrentFile( _hArchive );
			delete[] buf;
		}*/

		bool isUtf8 = ( ( fileInfo.flag & 0x800 ) == 0x800 || IsUTF8( fileName ) );
		auto path = StringUtils::convert2W( fileName, isUtf8 ? CP_UTF8 : CP_OEMCP );
		PathUtils::unifyDelimiters( path );
		wcsncpy( wfd.cFileName, path.c_str(), ARRAYSIZE( WIN32_FIND_DATA::cFileName ) );

		ULARGE_INTEGER fSize;
		fSize.QuadPart = fileInfo.uncompressed_size;
		wfd.nFileSizeLow = fSize.LowPart;
		wfd.nFileSizeHigh = fSize.HighPart;

		FILETIME ftime; // do not use SystemTimeToTzSpecificLocalTime !!
		DosDateTimeToFileTime( HIWORD( fileInfo.dosDate ), LOWORD( fileInfo.dosDate ), &ftime );
		wfd.ftLastWriteTime = ftime;
		wfd.dwFileAttributes = LOBYTE( fileInfo.external_fa ); // use only DOS attributes
	}

	void CArchZip::setPassword( const char *password )
	{
		unzOpenCurrentFilePassword( _hArchive, password );
	}

	int CArchZip::getVersion()
	{
		return 0;
	}
}
