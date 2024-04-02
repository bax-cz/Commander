#pragma once

#include "BaseDialog.h"

namespace Commander
{
	class CFileExtractor : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_PROGRESS;

	public:
		void extract( const std::wstring& fileName, const std::wstring& targetDir, std::shared_ptr<CPanelTab> spPanel, CArchiver::EExtractAction action = CArchiver::EExtractAction::Rename );
		void extract( const std::wstring& archiveName, const std::wstring& localPath, const std::wstring& targetDir, CArchiver::EExtractAction action = CArchiver::EExtractAction::Rename );

		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		void extractCore();
		void drawBackground( HDC hdc );
		void updateProgressStatus();
		bool _workerProc();

	private:
		std::unique_ptr<CArchiver> _upArchiver;
		CBackgroundWorker _worker;

		CArchiver::EExtractAction _action;

		std::vector<std::wstring> _entries;
		std::wstring _archiveName;
		std::wstring _targetDir;

		std::wstring _processingPath;
		std::wstring _processingVolume;

		ULONGLONG _bytesTotal;
		ULONGLONG _bytesProcessed;
	};
}
