#pragma once

#include "BaseDialog.h"

namespace Commander
{
	class CFilePacker : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_PROGRESS;

	public:
		void packFiles( const std::wstring& targetName, CPanelTab *pPanel );

		virtual void onInit() override;
		virtual bool onClose() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		void drawBackground( HDC hdc );
		void updateProgressStatus();
		bool _workerProc();

	private:
		std::unique_ptr<CArchiver> _upArchiver;
		CBackgroundWorker _worker;

		std::vector<std::wstring> _entries;
		std::wstring _targetName;

		std::wstring _processingPath;
		std::wstring _processingVolume;

		ULONGLONG _bytesTotal;
		ULONGLONG _bytesProcessed;
	};
}
