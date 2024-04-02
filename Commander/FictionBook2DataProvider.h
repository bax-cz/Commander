#pragma once

/*
	FictionBook2 data provider interface for decoding .fb2 files
*/

#include "BaseDataProvider.h"

namespace Commander
{
	class CFictionBook2DataProvider : public CBaseDataProvider
	{
	public:
		CFictionBook2DataProvider();
		~CFictionBook2DataProvider();

		virtual bool readData( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) override;
		virtual bool readToFile( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) override;

	//	virtual void onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal ) override;

	private:
		bool fb2html( const std::wstring& fileNameIn, const std::wstring& fileNameOut );
		bool _readDataCore();

	private:
		std::wstring _currentDirectory;
	};
}
