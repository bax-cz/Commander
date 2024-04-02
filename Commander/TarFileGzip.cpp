#include "stdafx.h"

#include "TarFileGzip.h"

namespace TarLib
{
	CTarFileGzip::CTarFileGzip()
	{
		_gzFile = nullptr;
	}

	CTarFileGzip::~CTarFileGzip()
	{
	}

	bool CTarFileGzip::isOpen()
	{
		return _gzFile != nullptr;
	}

	bool CTarFileGzip::open( const std::wstring& fileName )
	{
		_gzFile = gzopen_w( fileName.c_str(), "rb" ); // only read mode

		return isOpen();
	}

	void CTarFileGzip::seek( long long pos, int dir )
	{
		gzseek64( _gzFile, pos, dir );
	}

	long long CTarFileGzip::tell()
	{
		return gztell64( _gzFile );
	}

	long long CTarFileGzip::read( char *buffer, size_t len )
	{
		return (long long)gzread( _gzFile, buffer, (unsigned int)len );
	}

	void CTarFileGzip::close()
	{
		gzclose( _gzFile );
		_gzFile = nullptr;
	}
}
