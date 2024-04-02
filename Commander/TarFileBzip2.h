#pragma once

#include "TarFileBase.h"
#include "../Zlib/bzip2/bzlib.h"

namespace TarLib
{
	class CTarFileBzip2 : public CTarFileBase
	{
	public:
		CTarFileBzip2();
		~CTarFileBzip2();

		virtual bool open( const std::wstring& fileName ) override;
		virtual bool isOpen() override;
		virtual void seek( long long pos, int direction ) override;
		virtual long long tell() override;
		virtual long long read( char *buffer, size_t len ) override;
		virtual void close() override;

	private:
		BZFILE *_bzFile;
	};
}
