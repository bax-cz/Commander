#pragma once

#include "BaseDialog.h"
#include "BackgroundWorker.h"
#include "PuttyInterface.h"

/*
	SSH Connect to server dialog
*/

namespace Commander
{
	class CSshConnect : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_SSHCONNECT;

	public:
		CSshConnect();
		~CSshConnect();

		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		void onCaptureOutput( const std::wstring& str, bcb::TCaptureOutputType outputType );
		void updateToolTip( const std::wstring& tip );
		void detectReturnVar();
		void tryChangeDirectory();
		bool _connectSshCore();

	private:
		CBackgroundWorker _worker;
		std::wstring _status;
		bool _cancelPressed;

		std::unique_ptr<CSshReader::SshData> _upSshData;

		HWND _hWndTip;
	};
}
