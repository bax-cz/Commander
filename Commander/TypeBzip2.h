#pragma once

#include "Archiver.h"
#include "../Zlib/bzip2/bzlib.h"

namespace Commander
{
	class CArchBzip2 : public CArchiver
	{
	public:
		CArchBzip2();
		~CArchBzip2();

		virtual bool readEntries() override;
		virtual bool extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir ) override;

	private:
		void getEntryInfo( LONGLONG fileSize, WIN32_FIND_DATA *wfd );

	private:
		BZFILE *_hArchive;
		unsigned char _dataBuf[0x00400000];
	};
}
