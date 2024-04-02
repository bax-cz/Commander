#pragma once

#include "Archiver.h"
#include "../Undmg/FcUndmg.h"

namespace Commander
{
	class CArchDmg : public CArchiver
	{
	public:
		CArchDmg();
		~CArchDmg();

		virtual bool readEntries() override;
		virtual bool extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir ) override;

	private:
		void readEntriesTree( HFSCatalogNodeID folderID, wchar_t *root );
		void extractEntries( HFSCatalogNodeID folderID, const std::wstring& root, const std::vector<std::wstring>& entries, const std::wstring& targetDir );
		void extractFile( HFSPlusCatalogFile *file, const std::wstring& path );

		std::wstring getTempFileName();

	private:
		Volume *_volume;
		std::wstring _tempFileName;
		unsigned char _dataBuf[0x00400000];
	};
}
