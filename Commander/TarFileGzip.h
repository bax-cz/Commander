#pragma once

#include "TarFileBase.h"
#include "../Zlib/zlib.h"

namespace TarLib
{
	class CTarFileGzip : public CTarFileBase
	{
	public:
		CTarFileGzip();
		~CTarFileGzip();

		virtual bool open( const std::wstring& fileName ) override;
		virtual bool isOpen() override;
		virtual void seek( long long pos, int direction ) override;
		virtual long long tell() override;
		virtual long long read( char *buffer, size_t len ) override;
		virtual void close() override;

	private:
		gzFile _gzFile;
	};
}
