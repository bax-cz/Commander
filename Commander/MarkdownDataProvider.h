#pragma once

/*
	MarkDown data provider interface for decoding .md files
*/

#include "BaseDataProvider.h"

namespace Commander
{
	class CMarkdownDataProvider : public CBaseDataProvider
	{
	public:
		CMarkdownDataProvider();
		~CMarkdownDataProvider();

		virtual bool readData( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) override;
		virtual bool readToFile( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) override;

	//	virtual void onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal ) override;

	private:
		bool _readDataCore();

	private:
		std::wstring _currentDirectory;
	};
}
