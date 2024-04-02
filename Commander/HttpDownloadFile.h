#pragma once

#include "BaseDialog.h"
#include <WinInet.h>

namespace Commander
{
	class CHttpDownloadFile : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_DOWNLOADFILE;

	public:
		virtual void onInit() override;
		virtual bool onOk() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		void updateDialogTitle( const std::wstring& status );
		void updateGuiStatus( bool enable = true );
		void updateLayout( int width, int height );
		void onChooseFileName();
		void startDownload();

	private:
		bool _workerProc();
		bool tryOpenUrl( const std::wstring& verb );
		bool openUrlSecurityFlags( const std::wstring& verb, DWORD flags );
		bool getHeader( DWORD& code );
		bool downloadFile( DWORD& code );
		bool downloadFileCore( int mode );
		void closeUrl();
		bool checkFileOnDisk();
		void readHeaders();
		void parseUrl();
		void parseHeaders();

	private:
		CBackgroundWorker _worker;

		HINTERNET _hInternet;
		HINTERNET _hSession;
		HINTERNET _hUrl;

		LONG_PTR _progressStyle;

		std::wstring _hostName;
		std::wstring _urlPath;
		std::wstring _headers;
		INTERNET_PORT _port;
		DWORD _service;

		bool _initialized;
		bool _canceled;

		WIN32_FIND_DATA _wfd;
		DWORD _errorId;

		LONGLONG _fileSize;
		ULONGLONG _offset; // offset from which to start download
		bool _fileTypeText; // file is a text file

		std::wstring _url;
		std::wstring _curDir;
		std::wstring _fileName;
		std::wstring _errorMsg;
		std::wstring _dialogTitle;
	};
}
