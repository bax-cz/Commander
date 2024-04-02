#pragma once

#include "BaseDialog.h"

namespace Commander
{
	class CSshTransfer : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_PROGRESS;

	public:
		CSshTransfer();
		~CSshTransfer();

		void transferFiles( EFcCommand cmd, const std::vector<std::wstring>& entries, ULONGLONG total, const std::wstring& targetName, std::shared_ptr<CPanelTab> spPanel, bool toLocal = true );

		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		void onCaptureOutput( const std::wstring& str, bcb::TCaptureOutputType outputType );

		bool sink( const std::wstring& path, bool& success );

		bool copyToLocal( bool move = false );
		bool copyToLocalFile( const std::wstring& fileName, ULONGLONG fileSize, ULONG mtime );
		bool createLocalDirectory( const std::wstring& dirName, ULONG mtime );

		bool copyToRemote( bool move = false );
		bool copyToRemoteFile( const std::wstring& fileName, WIN32_FIND_DATA& wfd );
		bool copyToRemoteDirectory( const std::wstring& dirName, WIN32_FIND_DATA& wfd );

		void drawBackground( HDC hdc );
		void updateProgressStatus();
		bool _workerProc();

	private:
		std::unique_ptr<CSshReader::SshData> _upSshData;
		CBackgroundWorker _worker;

		std::shared_ptr<CPanelTab> _spPanel;
		std::vector<std::wstring> _entries;
		std::wstring _currentDirectory;
		std::wstring _targetName;

		EFcCommand _cmd;
		bool _toLocal;

		std::wstring _processingPath;
		std::wstring _errorMessage;

		ULONGLONG _bytesTotal;
		ULONGLONG _bytesProcessed;

		char _dataBuf[0xFFFF];
	};
}
