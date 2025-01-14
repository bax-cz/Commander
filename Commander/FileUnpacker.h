#pragma once

#include "BaseDialog.h"

namespace Commander
{
	class CFileUnpacker : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_PROGRESS;

	public:
		void unpack( const std::vector<std::wstring>& items, const std::wstring& targetDir, std::shared_ptr<CPanelTab> spPanel, CArchiver::EExtractAction action = CArchiver::EExtractAction::Rename );
		void unpack( const std::wstring& fileName, const std::wstring& targetDir, std::shared_ptr<CPanelTab> spPanel, CArchiver::EExtractAction action = CArchiver::EExtractAction::Rename );
		void unpack( const std::wstring& archiveName, const std::wstring& localPath, const std::wstring& targetDir, CArchiver::EExtractAction action = CArchiver::EExtractAction::Rename );

		void repack( const std::vector<std::wstring>& items, const std::wstring& targetDir, std::shared_ptr<CPanelTab> spPanel );

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

		std::vector<std::wstring> _archiveNames;
		std::vector<std::wstring> _entries;
		std::wstring _targetDir;

		std::wstring _processingPath;
		std::wstring _processingVolume;

		ULONGLONG _bytesTotal;
		ULONGLONG _bytesProcessed;

		bool _repack;
	};
}
