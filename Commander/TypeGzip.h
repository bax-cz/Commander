#pragma once

#include "Archiver.h"
#include "../Zlib/zlib.h"

namespace Commander
{
	class CArchGzip : public CArchiver
	{
	public:
		CArchGzip();
		~CArchGzip();

		virtual bool readEntries() override;
		virtual bool extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir ) override;

	private:
		void getEntryInfo( LONGLONG fileSize, WIN32_FIND_DATA *wfd );

	private:
		gzFile _hArchive;
		unsigned char _dataBuf[0x00400000];
	};
}
