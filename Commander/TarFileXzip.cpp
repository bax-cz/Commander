#include "stdafx.h"

#include "TarFileXzip.h"

namespace TarLib
{
	CTarFileXzip::CTarFileXzip()
	{
		CrcGenerateTable();
		Crc64GenerateTable();

		_allocImp.Alloc = SzAlloc;
		_allocImp.Free = SzFree;

		_sizeInTotal = _sizeIn = _inPos = 0;
		_result = SZ_OK;

		_status = CODER_STATUS_NOT_SPECIFIED;
		_finished = false;
		_open = false;

		XzUnpacker_Construct( &_xzUnpacker, &_allocImp );
	}

	CTarFileXzip::~CTarFileXzip()
	{
	}

	bool CTarFileXzip::isOpen()
	{
		return _open;
	}

	bool CTarFileXzip::open( const std::wstring& fileName )
	{
		if( InFile_OpenW( &_archiveStream.file, fileName.c_str() ) == 0 )
		{
			FileInStream_CreateVTable( &_archiveStream );

			_sizeInTotal = _sizeIn = _inPos = 0;
			_offsetTar = 0ll;
			_open = true;
			_result = SZ_OK;

			return true;
		}

		return false;
	}

	void CTarFileXzip::seek( long long pos, int direction )
	{
		while( (size_t)pos > _sizeInTotal ) // hack - seek forward only
		{
			_sizeInTotal += decodeDataChunk();

			if( _finished )
				break;
		}

		_offsetTar = pos;
	}

	long long CTarFileXzip::tell()
	{
		return _offsetTar;
	}

	long long CTarFileXzip::read( char *buffer, size_t bytesToRead )
	{
		size_t readBytes = 0;

		while( (size_t)_offsetTar + bytesToRead > _sizeInTotal )
		{
			if( (size_t)_offsetTar < _sizeInTotal )
			{
				readBytes = _sizeInTotal - (size_t)_offsetTar;
				size_t pos = sizeof( _outputBuffer ) - readBytes;

				memcpy( buffer, _outputBuffer + pos, readBytes );

				_offsetTar += (long long)readBytes;
			}

			_sizeInTotal += decodeDataChunk();

			if( _finished )
				break;
		}

		if( readBytes != bytesToRead )
		{
			size_t pos = (size_t)_offsetTar % sizeof( _outputBuffer );
			memcpy( buffer + readBytes, _outputBuffer + pos, bytesToRead - readBytes );
			readBytes += ( bytesToRead - readBytes );
			_offsetTar += (long long)readBytes;
		}

		return (long long)readBytes;
	}

	void CTarFileXzip::close()
	{
		XzUnpacker_Free( &_xzUnpacker );
		File_Close( &_archiveStream.file );

		_open = false;
	}

	size_t CTarFileXzip::decodeDataChunk()
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

			//PrintDebug( L"inPos = %6d  inLen = %5d, outLen = %5d", inPos, inLen, outLen );
			
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
}
