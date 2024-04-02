#pragma once

#include "Archiver.h"

#include "../Unrar/dll.hpp"
#include "../Unrar/rardefs.hpp"

namespace Commander
{
	class CArchRar : public CArchiver
	{
	public:
		CArchRar();
		~CArchRar();

		virtual bool readEntries() override;
		virtual bool extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir ) override;

		virtual void setPassword( const char *password ) override;
		virtual int  getVersion() override;

	public:
		static int CALLBACK unrarProc( UINT msg, LPARAM userData, LPARAM par1, LPARAM par2 );

	private:
		void getEntryInfo( const RARHeaderDataEx& entry, WIN32_FIND_DATA *wfd );
		std::wstring getErrorMessage( int errorCode );

	private:
		HANDLE _hArchive;

		RAROpenArchiveDataEx _archiveDataEx;
		RARHeaderDataEx      _headerDataEx;
	};
}
