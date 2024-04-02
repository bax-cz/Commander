#pragma once

#include "Archiver.h"
#include "../Uniso/uniso.h"

namespace Commander
{
	class CArchIso : public CArchiver
	{
	public:
		CArchIso();
		~CArchIso();

		virtual bool readEntries() override;
		virtual bool extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir ) override;

	public:
		static int processDataProc( wchar_t *fileName, int size, LPARAM userData );

	private:
		void getEntryInfo( tHeaderDataExW& entry, WIN32_FIND_DATA *wfd );

	private:
		HANDLE _hArchive;
	};
}
