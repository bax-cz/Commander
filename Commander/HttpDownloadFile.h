#pragma once

#include "BaseDialog.h"
#include <WinInet.h>

namespace Commander
{
	class CHttpDownloadFile : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_DOWNLOADFILE;

	private:
		static constexpr int FailedAttemptsMax = 10; // max consecutive download attempts

	public:
		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;
		LRESULT CALLBACK wndProcControls( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

	private:
		void updateDialogTitle( const std::wstring& status = L"" );
		void updateGuiStatus( bool enable = true );
		void updateLayout( int width, int height );
		void updateHeaders( bool tabChanged = false );
		void onChooseFileName();
		void startDownload();

	private:
		bool _workerProc();
		bool tryOpenUrl( const std::wstring& verb );
		bool openUrlSecurityFlags( const std::wstring& verb, DWORD flags );
		bool getHeader( DWORD& code );
		bool downloadFile( DWORD& code, LONGLONG& total );
		bool downloadFileCore( int mode, LONGLONG& total );
		void closeUrl();
		bool checkFileOnDisk();
		void parseUrl();
		void queryResponseHeaders();
		void parseResponseHeaders();
		std::wstring getStatusText( LONGLONG total );

	private:
		CBackgroundWorker _worker;

		HINTERNET _hInternet;
		HINTERNET _hSession;
		HINTERNET _hUrl;

		LONG_PTR _progressStyle;

		std::wstring _hostName;
		std::wstring _urlPath;
		std::wstring _respHeaders;
		std::wstring _rqstHeaders;
		INTERNET_PORT _port;
		DWORD _service;

		bool _initialized;
		bool _canceled;
		bool _dataCorrupted;
		bool _zeroBytesReceived;

		int _attemptCount; // consecutive download attempts counter

		WIN32_FIND_DATA _wfd;
		DWORD _errorId;

		LONGLONG _contentLength; // original content length
		ULONGLONG _offset; // offset from which to start download
		ULONGLONG _verifyBytes; // bytes to verify when resume
		bool _fileTypeText; // file is a text file

		std::wstring _url;
		std::wstring _curDir;
		std::wstring _fileName;
		std::wstring _errorMsg;
		std::wstring _dialogTitle;

		char _buff[32768];

	private:
		static LRESULT CALLBACK wndProcControlsSubclass( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR data )
		{
			return reinterpret_cast<CHttpDownloadFile*>( data )->wndProcControls( hWnd, message, wParam, lParam );
		}
	};
}
