#pragma once

/*
	Archive data provider interface for reading data from archive
*/

#include "BaseDataProvider.h"

namespace Commander
{
	class CArchiveDataProvider : public CBaseDataProvider
	{
	public:
		CArchiveDataProvider();
		~CArchiveDataProvider();

		virtual bool readData( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) override;
		virtual bool readToFile( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) override;

		//virtual void onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal ) override;

	private:
		//bool createArchiver( const std::wstring& fileName );
		//bool _openArchiveCore();

	private:
		//std::unique_ptr<CArchiver> _upArchiver; // archiver (unzip, unrar, gzip..)
	};
}
