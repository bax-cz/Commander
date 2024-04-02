#pragma once

#include <fstream>
#include "TarFileBase.h"

namespace TarLib
{
	class CTarFileStream : public CTarFileBase
	{
	public:
		CTarFileStream();
		~CTarFileStream();

		virtual bool open( const std::wstring& fileName ) override;
		virtual bool isOpen() override;
		virtual void seek( long long pos, int direction ) override;
		virtual long long tell() override;
		virtual long long read( char *buffer, size_t len ) override;
		virtual void close() override;

	private:
		std::fstream _tarStream;
	};
}
