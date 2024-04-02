#include "stdafx.h"

#include "Commander.h"
#include "TypeXzip.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CArchXzip::CArchXzip()
	{
		CrcGenerateTable();
		Crc64GenerateTable();

		_allocImp.Alloc = SzAlloc;
		_allocImp.Free = SzFree;

		_sizeInTotal = _sizeIn = _inPos = 0;
		_result = SZ_OK;

		_status = CODER_STATUS_NOT_SPECIFIED;
		_finished = false;

		XzUnpacker_Construct( &_xzUnpacker, &_allocImp );
	}

	CArchXzip::~CArchXzip()
	{
		XzUnpacker_Free( &_xzUnpacker );
	}

	//
	// Open archive
	//
	bool CArchXzip::readEntries()
	{
		if( InFile_OpenW( &_archiveStream.file, PathUtils::getExtendedPath( _fileName ).c_str() ) == 0 )
		{
			FileInStream_CreateVTable( &_archiveStream );

			_sizeInTotal = _sizeIn = _inPos = 0;
			_result = SZ_OK;

			// calculate unpacked file size
			while( _pWorker->isRunning() && !_finished )
				_sizeInTotal += decodeDataChunk();

			EntryData entry = { 0 };
			getEntryInfo( static_cast<LONGLONG>( _sizeInTotal ), &entry.wfd );

			_dataEntries.push_back( entry );

			File_Close( &_archiveStream.file );

			return true;
		}

		return false;
	}

	//
	// Extract xziped file to given directory
	//
	bool CArchXzip::extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir )
	{
		auto outName = PathUtils::stripFileExtension( PathUtils::stripPath( _fileName ) );
		auto path = PathUtils::getFullPath( targetDir + outName );

		_pWorker->sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)outName.c_str() );

		DWORD read, written = 0;
		HANDLE hFile = CreateFile( PathUtils::getExtendedPath( path ).c_str(), GENERIC_WRITE, NULL, NULL, CREATE_NEW, 0, NULL );
		if( InFile_OpenW( &_archiveStream.file, PathUtils::getExtendedPath( _fileName ).c_str() ) == 0 )
		{
			FileInStream_CreateVTable( &_archiveStream );

			_sizeInTotal = _sizeIn = _inPos = 0;
			_result = SZ_OK;

			while( !_finished )
			{
				if( !_pWorker->isRunning() || _result != SZ_OK )
				{
					CloseHandle( hFile );
					DeleteFile( PathUtils::getExtendedPath( path ).c_str() );
					File_Close( &_archiveStream.file );
					return false;
				}

				read = static_cast<DWORD>( decodeDataChunk() );

				_pWorker->sendNotify( FC_ARCHBYTESRELATIVE, read );
				WriteFile( hFile, _outputBuffer, read, &written, NULL );
			}
		}

		WIN32_FIND_DATA wfd;
		getEntryInfo( 0, &wfd );
		SetFileTime( hFile, &wfd.ftCreationTime, &wfd.ftLastAccessTime, &wfd.ftLastWriteTime );

		CloseHandle( hFile );
		File_Close( &_archiveStream.file );

		SetFileAttributes( PathUtils::getExtendedPath( path ).c_str(), wfd.dwFileAttributes );

		return true;
	}

	//
	// Get entry info as WIN32_FIND_DATA
	//
	size_t CArchXzip::decodeDataChunk()
	{
		SizeT outPos = 0;

		while( !_finished )
		{
			if( _inPos == _sizeIn )
			{
				_inPos = 0;
				_sizeIn = sizeof( _inputBuffer );
				File_Read( &_archiveStream.file, _inputBuffer, &_sizeIn );
			}

			SizeT inLen = _sizeIn - _inPos;
			SizeT outLen = sizeof( _outputBuffer ) - outPos;

			_result = XzUnpacker_Code(
				&_xzUnpacker,
				_outputBuffer + outPos,
				&outLen,
				_inputBuffer + _inPos,
				&inLen,
				0,
				( _sizeIn == 0 ? CODER_FINISH_END : CODER_FINISH_ANY ),
				&_status );

			_inPos += inLen;
			outPos += outLen;

			_finished = ( ( ( inLen == 0 ) && ( outLen == 0 ) ) || _result != SZ_OK );

			if( outPos == sizeof( _outputBuffer ) || _finished )
			{
				//if( outPos != 0 )
					//fwrite( buffOut, outPos, 1, f );
				break;
			}
		}

		return outPos;
	}

	//
	// Get entry info as WIN32_FIND_DATA
	//
	void CArchXzip::getEntryInfo( LONGLONG fileSize, WIN32_FIND_DATA *wfd )
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
