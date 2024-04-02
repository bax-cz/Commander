#include "stdafx.h"

#include "TarFileStream.h"

namespace TarLib
{
	CTarFileStream::CTarFileStream()
	{
	}

	CTarFileStream::~CTarFileStream()
	{
	}

	bool CTarFileStream::isOpen()
	{
		return _tarStream.is_open();
	}

	bool CTarFileStream::open( const std::wstring& filename )
	{
		_tarStream.open( filename.c_str(), std::ios::in | std::ios::binary );

		return isOpen();
	}

	void CTarFileStream::seek( long long pos, int dir )
	{
		_tarStream.seekg( pos, dir );
	}

	long long CTarFileStream::tell()
	{
		return (long long)_tarStream.tellg();
	}

	long long CTarFileStream::read( char *buffer, size_t len )
	{
		_tarStream.read( buffer, len );

		return _tarStream.gcount();
	}

	void CTarFileStream::close()
	{
		if( _tarStream.is_open() )
		{
			_tarStream.close();
		}
	}
}
