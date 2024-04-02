#include "stdafx.h"

#include "Commander.h"
#include "TypeRar.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CArchRar::CArchRar()
	{
		_hArchive = nullptr;
	}

	CArchRar::~CArchRar()
	{
	}

	std::wstring CArchRar::getErrorMessage( int errorCode )
	{
		std::wostringstream outMessage;
		outMessage << L"Error: (" << errorCode << L") ";

		switch( errorCode )
		{
		case ERAR_END_ARCHIVE:
			outMessage << L"End of archive reached.";
			break;
		case ERAR_NO_MEMORY:
			outMessage << L"Not sufficient memory.";
			break;
		case ERAR_BAD_DATA:
			outMessage << L"Corrupted data.";
			break;
		case ERAR_BAD_ARCHIVE:
			outMessage << L"Corrupted archive.";
			break;
		case ERAR_UNKNOWN_FORMAT:
			outMessage << L"Unknown format.";
			break;
		case ERAR_EOPEN:
			outMessage << L"Cannot open file.";
			break;
		case ERAR_ECREATE:
			outMessage << L"Cannot create file.";
			break;
		case ERAR_ECLOSE:
			outMessage << L"Cannot close file.";
			break;
		case ERAR_EREAD:
			outMessage << L"Cannot read file.";
			break;
		case ERAR_EWRITE:
			outMessage << L"Cannot write file.";
			break;
		case ERAR_SMALL_BUF:
			outMessage << L"Too small buffer.";
			break;
		case ERAR_UNKNOWN:
			outMessage << L"Unknown error.";
			break;
		case ERAR_MISSING_PASSWORD:
			outMessage << L"Missing password.";
			break;
		case ERAR_EREFERENCE:
			outMessage << L"Missing reference.";
			break;
		case ERAR_BAD_PASSWORD:
			outMessage << L"Password mismatch.";
			break;
		case ERAR_SUCCESS:
		default:
			outMessage << L"No error.";
			break;
		}

		return outMessage.str();
	}

	int CALLBACK CArchRar::unrarProc( UINT msg, LPARAM userData, LPARAM par1, LPARAM par2 )
	{
		CBackgroundWorker *pWorker = reinterpret_cast<CBackgroundWorker*>( userData );

		if( pWorker->isRunning() )
		{
			switch( msg )
			{
			case UCM_CHANGEVOLUME:
				// interested only in wide version
				break;

			case UCM_CHANGEVOLUMEW:
				if( par2 == RAR_VOL_ASK )
				{
					// ask for another part of the archive
					pWorker->sendMessage( FC_ARCHNEEDNEXTVOLUME, par1 );
					if( wcslen( reinterpret_cast<wchar_t*>( par1 ) ) == 0 )
					{
						return -1;
					}
				}
				else if( par2 == RAR_VOL_NOTIFY )
				{
					pWorker->sendMessage( FC_ARCHPROCESSINGVOLUME, par1 );
				}
				break;

			case UCM_PROCESSDATA:
				pWorker->sendNotify( FC_ARCHBYTESRELATIVE, par2 );
				break;

			case UCM_NEEDPASSWORD:
				// interested only in wide version
				break;

			case UCM_NEEDPASSWORDW:
				// ask for password
				pWorker->sendMessage( FC_ARCHNEEDPASSWORD, par1 );
				if( wcslen( reinterpret_cast<wchar_t*>( par1 ) ) == 0 )
				{
					return -1;
				}
				break;

			default:
				return -1;
			}

			return 0;
		}

		return -1;
	}

	//
	// Open archive
	//
	bool CArchRar::readEntries()
	{
		bool retVal = false;

		ZeroMemory( &_archiveDataEx, sizeof( _archiveDataEx ) );
		ZeroMemory( &_headerDataEx, sizeof( _headerDataEx ) );

		_archiveDataEx.ArcNameW = &_fileName[0];
		_archiveDataEx.CmtBuf = nullptr;
		_archiveDataEx.OpenMode = RAR_OM_LIST;

		// try to open the archive
		if( ( _hArchive = RAROpenArchiveEx( &_archiveDataEx ) ) && _archiveDataEx.OpenResult == ERAR_SUCCESS )
		{
			RARSetCallback( _hArchive, unrarProc, reinterpret_cast<LPARAM>( _pWorker ) );
			retVal = true;

			char emptyBuf[16384] = { 0 };
			_headerDataEx.CmtBuf = emptyBuf;
			_headerDataEx.CmtBufSize = sizeof( emptyBuf );

			int readHeaderCode, processFileCode;
			EntryData entry = { 0 };

			// do while we get a valid response to reading the header
			while( ( readHeaderCode = RARReadHeaderEx( _hArchive, &_headerDataEx ) ) == ERAR_SUCCESS )
			{
				if( !_pWorker->isRunning() ) {
					retVal = false;
					break;
				}

				// store file/directory entries
				getEntryInfo( _headerDataEx, &entry.wfd );
				_dataEntries.push_back( entry );

				// break on error
				if( ( processFileCode = RARProcessFileW( _hArchive, RAR_SKIP, nullptr, nullptr ) ) != ERAR_SUCCESS )
				{
					_errorMessage = getErrorMessage( processFileCode );
					retVal = false;
					break;
				}
			}

			if( !( readHeaderCode == ERAR_SUCCESS || readHeaderCode == ERAR_END_ARCHIVE ) )
			{
				_errorMessage = getErrorMessage( readHeaderCode );
				retVal = false;
			}

			RARCloseArchive( _hArchive );
			_hArchive = nullptr;
		}
		else
			_errorMessage = getErrorMessage( _archiveDataEx.OpenResult );

		return retVal;
	}

	//
	// Extract selected files to given directory
	//
	bool CArchRar::extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir )
	{
		ZeroMemory( &_archiveDataEx, sizeof( _archiveDataEx ) );
		ZeroMemory( &_headerDataEx, sizeof( _headerDataEx ) );

		_archiveDataEx.ArcNameW = &_fileName[0];
		_archiveDataEx.CmtBuf = nullptr;
		_archiveDataEx.OpenMode = RAR_OM_EXTRACT;

		wchar_t outputNameFull[1024];

		// try to open the archive
		if( ( _hArchive = RAROpenArchiveEx( &_archiveDataEx ) ) && _archiveDataEx.OpenResult == ERAR_SUCCESS )
		{
			RARSetCallback( _hArchive, unrarProc, reinterpret_cast<LPARAM>( _pWorker ) );

			_headerDataEx.CmtBuf = nullptr;
			_headerDataEx.CmtBufSize = 0;

			int readHeaderCode, processFileCode;

			// do while we get a valid response to reading the header
			while( _pWorker->isRunning() && ( readHeaderCode = RARReadHeaderEx( _hArchive, &_headerDataEx ) ) == ERAR_SUCCESS )
			{
				std::wstring outName = _headerDataEx.FileNameW;
				if( shouldExtractEntry( entries, outName ) )
				{
					wsprintf( outputNameFull, L"%ls%ls", targetDir.c_str(), outName.c_str() );

					_pWorker->sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)outName.c_str() );
					processFileCode = RARProcessFileW( _hArchive, RAR_EXTRACT, nullptr, outputNameFull );
				}
				else // skip non-listed files
					processFileCode = RARProcessFileW( _hArchive, RAR_SKIP, nullptr, nullptr );

				// break on error
				if( processFileCode != ERAR_SUCCESS )
				{
					_errorMessage = getErrorMessage( processFileCode );
					break;
				}
			}

			if( !( readHeaderCode == ERAR_SUCCESS || readHeaderCode == ERAR_END_ARCHIVE ) )
			{
				_errorMessage = getErrorMessage( readHeaderCode );
			}

			int ret = RARCloseArchive( _hArchive );
			_hArchive = nullptr;

			return processFileCode == ERAR_SUCCESS;
		}
		else
			_errorMessage = getErrorMessage( _archiveDataEx.OpenResult );

		return false;
	}

	//
	// Convert entry header to WIN32_FIND_DATA
	//
	void CArchRar::getEntryInfo( const RARHeaderDataEx& entry, WIN32_FIND_DATA *wfd )
	{
		FILETIME ftime; // nepouzivat SystemTimeToTzSpecificLocalTime !!
		DosDateTimeToFileTime( HIWORD( entry.FileTime ), LOWORD( entry.FileTime ), &ftime );

		// get the filename and other information from the header data
		wcsncpy( wfd->cFileName, entry.FileNameW, ARRAYSIZE( WIN32_FIND_DATA::cFileName ) );
		wfd->nFileSizeLow = entry.UnpSize; // get unpacked sizes
		wfd->nFileSizeHigh = entry.UnpSizeHigh;
		wfd->dwFileAttributes = entry.FileAttr;
		wfd->ftLastWriteTime = ftime;

		// check attributes validity
		/*if( ( wfd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) == FILE_ATTRIBUTE_DIRECTORY
			&& FsUtils::getFileSize( wfd->nFileSizeLow, wfd->nFileSizeHigh ) != 0ULL )
		{
			wfd->dwFileAttributes &= ~FILE_ATTRIBUTE_DIRECTORY;
		}*/
	}

	void CArchRar::setPassword( const char *password )
	{
		RARSetPassword( _hArchive, const_cast<char*>( password ) );
	}

	int CArchRar::getVersion()
	{
		return RARGetDllVersion();
	}
}
