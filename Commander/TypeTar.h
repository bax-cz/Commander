#pragma once

#include "Archiver.h"
#include "TarLib.h"

namespace Commander
{
	class CArchTar : public CArchiver
	{
	public:
		CArchTar( TarLib::ECompressionType type );
		~CArchTar();

		virtual bool readEntries() override;
		virtual bool extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir ) override;
		virtual bool packEntries( const std::vector<std::wstring>& entries, const std::wstring& targetName ) override;

	private:
		void extractFile( TarLib::Entry& entry, const std::wstring& path );
		void getEntryInfo( const TarLib::Entry& entry, WIN32_FIND_DATA *wfd );

	private:
		TarLib::ECompressionType _compressionType;
		char _dataBuf[TarLib::tarChunkSize];
	};
}
