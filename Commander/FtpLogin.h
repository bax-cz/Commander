#pragma once

#include "BaseDialog.h"

/*
	FTP Connect to server dialog
*/

namespace Commander
{
	class CFtpLogin : public CBaseDialog, public nsFTP::CFTPClient::CNotification
	{
	public:
		static const UINT resouceIdTemplate = IDD_FTPCONNECT;

	public:
		CFtpLogin();
		~CFtpLogin();

		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		virtual void OnInternalError( const tstring& errorMsg, const tstring& fileName, DWORD lineNr ) override;
		virtual void OnSendCommand( const nsFTP::CCommand& command, const nsFTP::CArg& arguments ) override;
		virtual void OnResponse( const nsFTP::CReply& reply ) override;

	private:
		void updateToolTip( const std::wstring& tip );
		bool _loginProcCore();

	private:
		CBackgroundWorker _worker;
		bool _cancelPressed;

		std::unique_ptr<CFtpReader::FtpData> _upFtpData;
		std::unique_ptr<std::pair<nsFTP::CCommand, nsFTP::CArg>> _upCommand;

		HWND _hWndTip;
	};
}
