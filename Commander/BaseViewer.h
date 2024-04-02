#pragma once

#include "BaseDialog.h"
#include "BaseDataProvider.h"
#include "BackgroundWorker.h"

/*
	CBaseViewer - view file content
*/

namespace Commander
{
	class CBaseViewer : public CBaseDialog
	{
	public:
		CBaseViewer() {}
		virtual ~CBaseViewer() = default;

		virtual bool viewFile( const std::wstring& dirName, const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel = nullptr ) = 0;

	private:
		//void addEntryFile( const WIN32_FIND_DATA *wfd, const std::wstring& fileName );

	protected:
		//bool shouldExtractEntry( const std::vector<std::wstring>& entries, std::wstring& entryName );

	protected:
		std::shared_ptr<CPanelTab> _spPanel;

		std::wstring _fileName;
		std::wstring _localDirectory;
		std::wstring _errorMessage;

		CBackgroundWorker _worker;
	};
}
