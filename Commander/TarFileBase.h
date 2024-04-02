#pragma once

/*
	TAR archives base interface
*/

namespace TarLib
{
	class CTarFileBase
	{
	public:
		CTarFileBase() { _offsetTar = 0ll; }
		virtual ~CTarFileBase() {}

		virtual bool open( const std::wstring& filename ) = 0;
		virtual bool isOpen() = 0;
		virtual void seek( long long pos, int direction ) = 0;
		virtual long long tell() = 0;
		virtual long long read( char *buffer, size_t len ) = 0;
		virtual void close() = 0;

	protected:
		long long _offsetTar;
	};
}
