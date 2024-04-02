#pragma once

/*
	Disk data provider interface for accessing files' data on disk
*/

#include "BaseDataProvider.h"

namespace Commander
{
	class CDiskDataProvider : public CBaseDataProvider
	{
	public:
		CDiskDataProvider();
		~CDiskDataProvider();

		virtual bool readData( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) override;
		virtual bool readToFile( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) override;

	//	virtual void onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal ) override;

	private:
		bool _readDataCore();

	private:
		std::wstring _currentDirectory;
	};
}
