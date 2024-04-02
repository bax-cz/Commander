#include "stdafx.h"

#include "TarFileBzip2.h"

namespace TarLib
{
	CTarFileBzip2::CTarFileBzip2()
	{
		_bzFile = nullptr;
	}

	CTarFileBzip2::~CTarFileBzip2()
	{
	}

	bool CTarFileBzip2::isOpen()
	{
		return _bzFile != nullptr;
	}

	bool CTarFileBzip2::open( const std::wstring& fileName )
	{
		_bzFile = BZ2_bzopenW( fileName.c_str(), L"rb" ); // only read mode
		_offsetTar = 0ll;

		return isOpen();
	}

	// hack - only forward seeking is possible in bzip2 archives
	void CTarFileBzip2::seek( long long pos, int dir )
	{
		auto reqPos = pos - _offsetTar;
		if( reqPos > 0 )
		{
			char *buf = new char[(size_t)reqPos];

			_offsetTar += static_cast<long long>( BZ2_bzread( _bzFile, buf, (int)reqPos ) );

			delete []buf;
		}
	}

	long long CTarFileBzip2::read( char *buffer, size_t len )
	{
		long long read = static_cast<long long>( BZ2_bzread( _bzFile, buffer, (int)len ) );
		_offsetTar += read;

		return read;
	}

	long long CTarFileBzip2::tell()
	{
		return _offsetTar;
	}

	void CTarFileBzip2::close()
	{
		BZ2_bzclose( _bzFile );

		_bzFile = nullptr;
		_offsetTar = 0ll;
	}
}
