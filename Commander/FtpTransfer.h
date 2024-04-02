#pragma once

#include "BaseDialog.h"

namespace Commander
{
	class CFtpTransfer : public CBaseDialog, public nsFTP::CFTPClient::CNotification
	{
	public:
		static const UINT resouceIdTemplate = IDD_PROGRESS;

	public:
		CFtpTransfer();
		~CFtpTransfer();

		void transferFiles( EFcCommand cmd, const std::vector<std::wstring>& entries, ULONGLONG total, const std::wstring& targetName, std::shared_ptr<CPanelTab> spPanel, bool toLocal = true );

		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		virtual void OnInternalError( const tstring& errorMsg, const tstring& fileName, DWORD lineNr ) override;

		virtual void OnBeginReceivingData() override;
		virtual void OnEndReceivingData( long long llReceivedBytes ) override;
		virtual void OnBytesSent( const nsFTP::TByteVector& vBuffer, long lSentBytes ) override;
		virtual void OnBytesReceived( const nsFTP::TByteVector& vBuffer, long lReceivedBytes ) override;

		virtual void OnPreSendFile( const tstring& strSourceFile, const tstring& strTargetFile, long long llFileSize ) override;
		virtual void OnPreReceiveFile( const tstring& strSourceFile, const tstring& strTargetFile, long long llFileSize ) override;
		virtual void OnPostReceiveFile( const tstring& strSourceFile, const tstring& strTargetFile, long long llFileSize ) override;
		virtual void OnPostSendFile( const tstring& strSourceFile, const tstring& strTargetFile, long long llFileSize ) override;

		virtual void OnSendCommand( const nsFTP::CCommand& command, const nsFTP::CArg& arguments ) override;
		virtual void OnResponse( const nsFTP::CReply& reply ) override;

	private:
		bool processCommand( const EFcCommand& cmd );
		bool copyToLocal( bool move = false );
		bool copyToLocalDirectory( const std::wstring& path );
		bool copyToRemote( bool move = false );
		bool copyToRemoteDirectory( const std::wstring& path );
		bool deleteDirectory( const std::wstring& dirName );
		bool deleteItems();

	private:
		void drawBackground( HDC hdc );
		void updateProgressStatus();
		bool _workerProc();

	private:
		std::unique_ptr<CFtpReader::FtpData> _upFtpData;
		CBackgroundWorker _worker;

		std::shared_ptr<CPanelTab> _spPanel;
		std::unique_ptr<std::pair<nsFTP::CCommand, nsFTP::CArg>> _upCommand;
		std::vector<std::wstring> _entries;
		std::wstring _currentDirectory;
		std::wstring _targetName;

		EFcCommand _cmd;
		bool _toLocal;
		bool _retryTransfer;

		std::wstring _processingPath;

		ULONGLONG _bytesTotal;
		ULONGLONG _bytesProcessed;

		char _dataBuf[0xFFFF];
	};
}
