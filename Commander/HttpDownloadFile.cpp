#include "stdafx.h"

#include "Commander.h"
#include "HttpDownloadFile.h"
#include "MiscUtils.h"
#include "NetworkUtils.h"
#include "TabViewUtils.h"

namespace Commander
{
	//
	// File downloader initialization
	//
	void CHttpDownloadFile::onInit()
	{
		// limit text to INTERNET_MAX_URL_LENGTH and max long path (32000 characters)
		SendDlgItemMessage( _hDlg, IDE_DOWNLOADFILE_URL, EM_SETLIMITTEXT, (WPARAM)INTERNET_MAX_URL_LENGTH, 0 );
		SendDlgItemMessage( _hDlg, IDE_DOWNLOADFILE_FILENAME, EM_SETLIMITTEXT, (WPARAM)MAX_WPATH, 0 );

		// subclass all the controls just to receive an accelerator key notification :(
		SetWindowSubclass( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_URL ),        wndProcControlsSubclass, 0, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_HEADERS ),    wndProcControlsSubclass, 1, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_HEADERS ),    wndProcControlsSubclass, 2, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_FILENAME ),   wndProcControlsSubclass, 3, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_CHOOSEFILE ), wndProcControlsSubclass, 4, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDOK ),                        wndProcControlsSubclass, 5, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDCANCEL ),                    wndProcControlsSubclass, 6, reinterpret_cast<DWORD_PTR>( this ) );

		_hInternet = nullptr;
		_hSession = nullptr;
		_hUrl = nullptr;
		_initialized = false;
		_canceled = false;
		_fileTypeText = false;
		_dataCorrupted = false;
		_zeroBytesReceived = false;
		_contentLength = -1ll;
		_offset = 0ull;
		_verifyBytes = 0ull;
		_attemptCount = FailedAttemptsMax; // 10 consecutive failed attempts stops the download process
		_errorId = NO_ERROR;
		_service = INTERNET_SERVICE_HTTP;
		_port = INTERNET_DEFAULT_HTTPS_PORT;

		auto url = MiscUtils::getClipboardText( _hDlg ); // https://a0.ww.np.dl.playstation.net/tpl/np/NPUA80662/NPUA80662-ver.xml
		_curDir = FCS::inst().getApp().getActivePanel().getActiveTab()->getDataManager()->getCurrentDirectory();

		// set progress-bar style as indifferent
		_progressStyle = GetWindowLongPtr( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_PROGRESS ), GWL_STYLE );
		SetWindowLongPtr( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_PROGRESS ), GWL_STYLE, _progressStyle | PBS_MARQUEE );

		SendDlgItemMessage( _hDlg, IDC_DOWNLOADFILE_PROGRESS, PBM_SETMARQUEE, 1, 0 );
		SendDlgItemMessage( _hDlg, IDC_DOWNLOADFILE_PROGRESS, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
		SendDlgItemMessage( _hDlg, IDC_DOWNLOADFILE_PROGRESS, PBM_SETPOS, 0, 0 );

		TabUtils::insertItem( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_HEADERS ), L"Response Headers" );
		TabUtils::insertItem( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_HEADERS ), L"Request Headers" );

		MiscUtils::getWindowText( _hDlg, _dialogTitle );
		MiscUtils::centerOnWindow( FCS::inst().getFcWindow(), _hDlg );
		ShowWindow( _hDlg, SW_SHOWNORMAL );
		updateGuiStatus();

		// initialize background worker
		_worker.init( [this] { return _workerProc(); }, _hDlg, UM_WORKERNOTIFY );

		// when a valid url is given, get HEAD
		if( !url.empty() && PathIsURL( url.c_str() ) )
		{
			_url = url;
			SetDlgItemText( _hDlg, IDE_DOWNLOADFILE_URL, url.c_str() );

			startDownload();
		}
	}

	void CHttpDownloadFile::startDownload()
	{
		updateDialogTitle( _initialized ? L"Downloading.." : L"Reading HEAD.." );
		updateGuiStatus( false );

		_canceled = false;

		_worker.start();
	}

	//
	// Ok/Get HEAD button pressed
	//
	bool CHttpDownloadFile::onOk()
	{
		if( !_worker.isRunning() )
		{
			if( !_initialized && MiscUtils::getWindowText( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_URL ), _dlgResult ) )
				_url = _dlgResult;

			if( _initialized && MiscUtils::getWindowText( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_FILENAME ), _dlgResult ) )
				_fileName = _dlgResult;

			// update the request headers content when the request tab is selected
			if( TabCtrl_GetCurSel( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_HEADERS ) ) == 1 )
			{
				MiscUtils::getWindowText( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_HEADERS ), _rqstHeaders );
			}

			startDownload();
		}
		else
		{
			// stop downloading
			EnableWindow( GetDlgItem( _hDlg, IDOK ), FALSE );
			_worker.stop_async( true );
		}

		return false;
	}

	bool CHttpDownloadFile::onClose()
	{
		if( _worker.isRunning() && MessageBox( _hDlg, L"Cancel downloading file?", _dialogTitle.c_str(), MB_ICONQUESTION | MB_YESNO ) == IDNO )
			return false;

		KillTimer( _hDlg, FC_TIMER_KEEPALIVE_ID );

		closeUrl();

		return true;
	}

	void CHttpDownloadFile::onDestroy()
	{
		if( _hInternet )
		{
			InternetCloseHandle( _hInternet );
			_hInternet = nullptr;
		}

		// remove subclass from all the controls
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_URL ),        wndProcControlsSubclass, 0 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_HEADERS ),    wndProcControlsSubclass, 1 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_HEADERS ),    wndProcControlsSubclass, 2 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_FILENAME ),   wndProcControlsSubclass, 3 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_CHOOSEFILE ), wndProcControlsSubclass, 4 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDOK ),                        wndProcControlsSubclass, 5 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDCANCEL ),                    wndProcControlsSubclass, 6 );
	}

	void CHttpDownloadFile::updateDialogTitle( const std::wstring& status )
	{
		std::wostringstream sstrTitle;

		if( !status.empty() )
		{
			sstrTitle << L"[" << status << L"] - " << _dialogTitle;

			if( _attemptCount != FailedAttemptsMax )
				sstrTitle << L" [" << FailedAttemptsMax - _attemptCount << L"/" << FailedAttemptsMax << L"]";
		}
		else
			sstrTitle << _dialogTitle;
		
		SetWindowText( _hDlg, sstrTitle.str().c_str() );
	}

	void CHttpDownloadFile::updateGuiStatus( bool enable )
	{
		// set progressbar style and status
		SetWindowLongPtr( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_PROGRESS ), GWL_STYLE, _progressStyle | ( _initialized ? 0 : PBS_MARQUEE ) );
		SendDlgItemMessage( _hDlg, IDC_DOWNLOADFILE_PROGRESS, PBM_SETMARQUEE, !_initialized, 0 );
		SendDlgItemMessage( _hDlg, IDC_DOWNLOADFILE_PROGRESS, PBM_SETPOS, 0, 0 );
		ShowWindow( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_PROGRESS ), enable ? SW_HIDE : SW_SHOWNORMAL );

		// ok button status when text has been changed
		bool enableOk = enable;
		enableOk = enableOk && GetWindowTextLength( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_URL ) ) > 0;
		enableOk = enableOk && ( GetWindowTextLength( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_FILENAME ) ) > 0 || !_initialized );

		EnableWindow( GetDlgItem( _hDlg, IDOK ), enableOk );
		EnableWindow( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_URL ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_FILENAME ), enable && _initialized );
		EnableWindow( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_CHOOSEFILE ), enable && _initialized );

		auto tabId = TabCtrl_GetCurSel( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_HEADERS ) );
		Edit_SetReadOnly( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_HEADERS ), !enable || tabId == 0 );

		SetDlgItemText( _hDlg, IDC_DOWNLOADFILE_STATUSTEXT, L"" );
		SetDlgItemText( _hDlg, IDOK, _initialized ? L"Download" : L"Get HEAD" );
	}

	void CHttpDownloadFile::updateHeaders( bool selChanged )
	{
		auto id = TabCtrl_GetCurSel( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_HEADERS ) );

		// store the request headers text when changing to response headers
		if( selChanged && !_worker.isRunning() && id == 0 )
		{
			MiscUtils::getWindowText( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_HEADERS ), _rqstHeaders );
		}

		Edit_SetReadOnly( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_HEADERS ), _worker.isRunning() || id == 0 );
		SetDlgItemText( _hDlg, IDE_DOWNLOADFILE_HEADERS, id ? _rqstHeaders.c_str() : _respHeaders.c_str() );
	}

	void CHttpDownloadFile::onChooseFileName()
	{
		std::wstring fname = _fileName;
		std::wstring outdir = _curDir;

		// update the filename and directory from edit-box
		if( MiscUtils::getWindowText( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_FILENAME ), _dlgResult ) )
		{
			StringUtils::trim( _dlgResult );

			if( !PathIsRelative( _dlgResult.c_str() ) )
			{
				outdir = PathUtils::addDelimiter( PathUtils::stripFileName( _dlgResult ) );
				fname = PathUtils::stripPath( _dlgResult );
			}
			else
				fname = _dlgResult;
		}

		// show the save file dialog
		if( MiscUtils::saveFileDialog( _hDlg, outdir, fname ) )
		{
			_fileName = PathUtils::stripPath( fname );
			_curDir = PathUtils::addDelimiter( PathUtils::stripFileName( fname ) );

			SetDlgItemText( _hDlg, IDE_DOWNLOADFILE_FILENAME, _fileName.c_str() );
		}
	}

	bool CHttpDownloadFile::openUrlSecurityFlags( const std::wstring& verb, DWORD flags )
	{
		_hSession = InternetConnect( _hInternet, &_hostName[0], _port, L"", L"", _service, 0, INTERNET_NO_CALLBACK );

		if( _hSession )
		{
			// open request
			LPCTSTR szAcceptFiles[] = { L"*/*", NULL };
			_hUrl = HttpOpenRequest( _hSession, &verb[0], (LPCWSTR)&_urlPath[0], NULL, NULL, szAcceptFiles,
				INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_SECURE, INTERNET_NO_CALLBACK );

			if( _hUrl )
			{
				DWORD dwFlags = 0;
				DWORD dwBuffLen = sizeof( dwFlags );

				// update security flags
				if( InternetQueryOption( _hUrl, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, &dwBuffLen ) )
				{
					dwFlags |= flags;
					InternetSetOption( _hUrl, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, sizeof( dwFlags ) );
				}

				if( HttpSendRequest( _hUrl, _rqstHeaders.c_str(), static_cast<DWORD>( _rqstHeaders.length() ), NULL, 0 ) )
					return true;
			}
		}

		return false;
	}

	bool CHttpDownloadFile::tryOpenUrl( const std::wstring& verb )
	{
		bool ret = true;

		// open url
		_hUrl = InternetOpenUrl( _hInternet, &_url[0], _rqstHeaders.c_str(), static_cast<DWORD>( _rqstHeaders.length() ), 0, INTERNET_NO_CALLBACK );

		if( !_hUrl ) // TODO: test and handle errors
		{
			_errorId = GetLastError();

			switch( _errorId )
			{
			case ERROR_INTERNET_INVALID_CA:
				ret = openUrlSecurityFlags( verb, SECURITY_FLAG_IGNORE_REVOCATION | SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID );
				break;
			case ERROR_INTERNET_HTTPS_TO_HTTP_ON_REDIR:
				ret = openUrlSecurityFlags( verb, SECURITY_FLAG_IGNORE_REDIRECT_TO_HTTP );
				break;
			case ERROR_INTERNET_HTTP_TO_HTTPS_ON_REDIR:
				ret = openUrlSecurityFlags( verb, SECURITY_FLAG_IGNORE_REDIRECT_TO_HTTPS );
				break;
			default:
				return false;
			}

			if( !_hUrl )
				_errorId = GetLastError();
		}

		return ret;
	}

	void CHttpDownloadFile::closeUrl()
	{
		// close the url handle
		if( _hUrl )
		{
			InternetCloseHandle( _hUrl );
			_hUrl = nullptr;
		}

		if( _hSession )
		{
			InternetCloseHandle( _hSession );
			_hSession = nullptr;
		}
	}

	std::wstring queryRawHeader( HINTERNET hUrl, WCHAR **buf, DWORD *bufLen, DWORD *idx )
	{
		if( !HttpQueryInfo( hUrl, HTTP_QUERY_RAW_HEADERS_CRLF, *buf, bufLen, idx ) )
		{
			DWORD errorId = GetLastError();

			if( errorId == ERROR_HTTP_HEADER_NOT_FOUND )
			{
				// no header available at given index
				*idx = 0;
				return L"";
			}
			else if( errorId == ERROR_INSUFFICIENT_BUFFER )
			{
				if( *buf )
					delete[] *buf;

				// allocate buffer
				*buf = new WCHAR[*bufLen];

				// try to read header again
				return queryRawHeader( hUrl, buf, bufLen, idx );
			}
		}

		return std::wstring( *buf, *bufLen );
	}

	bool queryStatusCode( HINTERNET hUrl, DWORD& code )
	{
		DWORD len = sizeof( code ), idx = 0;

		if( HttpQueryInfo( hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &code, &len, &idx ) )
			return true;

		return false;
	}

	std::wstring queryStatusText( HINTERNET hUrl )
	{
		WCHAR *buf = nullptr;
		DWORD bufLen = 0;
		DWORD idx = 0;

		if( !HttpQueryInfo( hUrl, HTTP_QUERY_STATUS_TEXT, buf, &bufLen, &idx ) )
		{
			DWORD errorId = GetLastError();

			if( errorId == ERROR_INSUFFICIENT_BUFFER )
			{
				// allocate buffer
				buf = new WCHAR[bufLen];

				// try to read the status text again
				HttpQueryInfo( hUrl, HTTP_QUERY_STATUS_TEXT, buf, &bufLen, &idx );
			}
		}

		std::wstring strOut;

		if( buf && bufLen > 0 )
		{
			strOut = std::wstring( buf, bufLen );
			delete[] buf;
		}

		return strOut.c_str();
	}

	void updateRangeRequest( std::wstring& rqstHeaders, ULONGLONG offset )
	{
		bool found = false;

		if( !rqstHeaders.empty() )
		{
			auto entries = StringUtils::split( rqstHeaders, L"\r\n" );
			size_t off = 0;

			for( auto& entry : entries )
			{
				if( StringUtils::startsWith( entry, L"Range:" ) )
				{
					found = true;
					entry = L"Range: bytes=" + std::to_wstring( offset ) + L"-";
				}
			}

			if( found )
				rqstHeaders = StringUtils::join( entries, L"\r\n" );
		}

		if( !found )
		{
			if( !rqstHeaders.empty() && !StringUtils::endsWith( rqstHeaders, L"\r\n" ) )
				rqstHeaders += L"\r\n";

			rqstHeaders += L"Range: bytes=" + std::to_wstring( offset ) + L"-";
		}
	}

	bool CHttpDownloadFile::getHeader( DWORD& code )
	{
		if( tryOpenUrl( L"HEAD" ) )
		{
			// read response
			if( queryStatusCode( _hUrl, code ) )
			{
				// read and parse response headers
				queryResponseHeaders();
				parseResponseHeaders();

				if( code >= HTTP_STATUS_OK && code <= HTTP_STATUS_PARTIAL_CONTENT )
					return true;
			}
			else
				_errorId = GetLastError();
		}

		if( !_worker.isRunning() )
			_canceled = true; // canceled by user

		return false;
	}

	bool CHttpDownloadFile::downloadFile( DWORD& code, LONGLONG& total )
	{
		int mode = std::ios::out;

		// shall we resume download or overwrite the existing file
		if( checkFileOnDisk() )
		{
			// is this the first attempt?
			if( _attemptCount == FailedAttemptsMax )
			{
				// file already exists and is in (in)complete state
				int state = ( _contentLength > _offset ) ? 0 : 1;

				// ask user what to do
				_worker.sendMessage( 2, reinterpret_cast<LPARAM>( &state ) );

				if( _contentLength > _offset )
					mode = ( state == IDYES ) ? std::ios::app : ( state == IDNO ) ? std::ios::out : -1; // yes-append, no-overwrite, cancel-cancel
				else
					mode = ( state == IDYES ) ? std::ios::out : -1; // yes-overwrite, no-cancel
			}
			else
				mode = std::ios::app;
		}

		if( mode != -1 )
		{
			// reset offset when overwriting file or file is smaller than the buffer size
			if( mode != std::ios::app || _offset < sizeof( _buff ) )
			{
				mode = std::ios::out;
				_offset = _verifyBytes = 0ull;
			}
			else if( mode == std::ios::app )
			{
				// move offset to redownload and verify previously downloaded data
				mode |= std::ios::in;
				_verifyBytes = sizeof( _buff );
				updateRangeRequest( _rqstHeaders, _offset - _verifyBytes );
			}

			if( tryOpenUrl( L"GET" ) )
			{
				// read response
				queryResponseHeaders();

				if( queryStatusCode( _hUrl, code ) )
				{
					if( code >= HTTP_STATUS_OK && code <= HTTP_STATUS_PARTIAL_CONTENT )
					{
						// finally download the file
						return downloadFileCore( mode, total );
					}
				}
				else
					_errorId = GetLastError();
			}
		}

		if( !_worker.isRunning() || mode == -1 )
			_canceled = true; // canceled by user

		return false;
	}

	bool verifyContent( std::fstream& fs, char *buff, std::streamsize buff_len )
	{
		bool ret = false;

		if( fs.is_open() )
		{
			// read data from the end of the file
			fs.seekg( buff_len * -1, std::ios::end );

			if( fs.good() )
			{
				// alloc memory for data buffer
				std::string fdata; fdata.resize( buff_len );

				// read data
				if( fs.read( &fdata[0], buff_len ).gcount() == buff_len )
				{
					// compare the data buffers
					ret = !memcmp( &fdata[0], buff, buff_len );
				}

				fs.seekg( 0, std::ios::end );
			}
		}

		return ret;
	}

	bool CHttpDownloadFile::downloadFileCore( int mode, LONGLONG& total )
	{
		if( _hUrl )
		{
			DWORD dwBytesRead = 0;
			total = _offset;

			// always download as a binary file (even when _fileTypeText)
			mode |= std::ios::binary;
			std::fstream fs( PathUtils::getExtendedPath( _curDir + _fileName ), mode );

			ULONGLONG ticks = GetTickCount64();
			BOOL verify = ( mode & std::ios::app ), ret = FALSE;

			// read the data and store it in a file
			while( ( ret = InternetReadFile( _hUrl, _buff, sizeof( _buff ), &dwBytesRead ) ) && dwBytesRead != 0 )
			{
				if( verify ) // verify the first data buffer
				{
					if( _verifyBytes != dwBytesRead || !verifyContent( fs, _buff, dwBytesRead ) )
					{
						_dataCorrupted = true;
						return false;
					}

					verify = FALSE;
					continue;
				}

				total += dwBytesRead;

				if( _contentLength > 0ll && ticks + 200 < GetTickCount64() )
				{
					// report progress every 200ms
					_worker.sendNotify( 3, static_cast<LPARAM>( total ) );
					ticks = GetTickCount64();

					// reset failed attempts counter
					_attemptCount = FailedAttemptsMax;
				}

				// write data to file
				fs.write( _buff, dwBytesRead );

				if( !_worker.isRunning() )
				{
					_canceled = true;
					return false;
				}
			}

			// reading data has been interrupted
			if( !ret )
			{
				_errorId = GetLastError();
				return false;
			}

			if( dwBytesRead == 0 )
				_zeroBytesReceived = true;

			if( _worker.isRunning() && total == _contentLength ) // report the last "100 %"
				_worker.sendNotify( 3, static_cast<LPARAM>( total ) );

			return _contentLength > 0ll ? ( total == _contentLength ) : true;
		}

		return false;
	}

	void CHttpDownloadFile::queryResponseHeaders()
	{
		WCHAR *buf = nullptr;
		DWORD bufLen = 0;
		DWORD idx = 0;

		_respHeaders = queryRawHeader( _hUrl, &buf, &bufLen, &idx );

		while( idx != 0 )
		{
			_respHeaders += L"\r\n\r\n";
			_respHeaders += queryRawHeader( _hUrl, &buf, &bufLen, &idx );
		}

		if( buf )
			delete[] buf;
	}

	std::wstring getInternetLastResponse()
	{
		DWORD errNetId = NO_ERROR, errNetDescLen = 0;
		InternetGetLastResponseInfo( &errNetId, NULL, &errNetDescLen );

		if( errNetDescLen++ > 0 )
		{
			std::wstring errNetDesc( errNetDescLen, L'\0' );
			if( InternetGetLastResponseInfo( &errNetId, &errNetDesc[0], &errNetDescLen ) )
				return errNetDesc.c_str();
		}

		return L"";
	}

	std::wstring getHttpStatusText( HINTERNET hUrl, DWORD status )
	{
		std::wstring statusCode = std::to_wstring( status );
		std::wstring statusText = queryStatusText( hUrl );

		if( !statusText.empty() )
			return statusCode + L" - " + statusText;

		return statusCode;
	}

	bool CHttpDownloadFile::_workerProc()
	{
		bool ret = false;

		if( !_hInternet )
		{
			if( InternetAttemptConnect( 0 ) == ERROR_SUCCESS )
			{
				const wchar_t USER_AGENT[] = L"Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:96.0) Gecko/20100101 Firefox/96.0";
				_hInternet = InternetOpen( USER_AGENT, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0 );
			}

			if( !_hInternet )
				_errorMsg = SysUtils::getErrorMessage( _errorId, GetModuleHandle( L"wininet.dll" ) );
		}

		if( _hInternet )
		{
			LONGLONG bytesTotal = -1ll;
			DWORD status = 0;
			_dataCorrupted = false;
			_zeroBytesReceived = false;
			_errorId = NO_ERROR;

			if( !_initialized )
			{
				parseUrl();

				// read and parse the raw http headers
				ret = getHeader( status );
			}
			else
			{
				// get the data from the file at a given url
				ret = downloadFile( status, bytesTotal );
			}

			if( ret == false )
			{
				if( _errorId != NO_ERROR )
					_errorMsg = SysUtils::getErrorMessage( _errorId, GetModuleHandle( L"wininet.dll" ) );
				else if( _dataCorrupted )
					_errorMsg = L"Unable to resume download because of data corruption.";
				else
				{
					_errorMsg.clear();

					if( status >= HTTP_STATUS_OK && status <= HTTP_STATUS_PARTIAL_CONTENT )
						_errorMsg = getInternetLastResponse();

					if( bytesTotal != -1ll && bytesTotal != _contentLength )
					{
						_errorMsg += L"\r\nExpected: " + FsUtils::bytesToString2( _contentLength ) + L" bytes";
						_errorMsg += L"\r\nReceived: " + FsUtils::bytesToString2( bytesTotal ) + L" bytes";
					}

					_errorMsg += L"\r\nHttp Status: " + getHttpStatusText( _hUrl, status );
				}
			}

			closeUrl();
		}

		return ret;
	}

	bool CHttpDownloadFile::checkFileOnDisk()
	{
		ZeroMemory( &_wfd, sizeof( _wfd ) );
		_offset = 0ull;

		if( FsUtils::getFileInfo( _curDir + _fileName, _wfd ) )
		{
			_offset = FsUtils::getFileSize( &_wfd );

			return true;
		}

		return false;
	}

	void CHttpDownloadFile::parseUrl()
	{
		WCHAR hostNameBuf[INTERNET_MAX_URL_LENGTH];
		WCHAR urlPathBuf[INTERNET_MAX_URL_LENGTH];

		// get filename from Url
		URL_COMPONENTS uc = { 0 };
		uc.dwStructSize = sizeof( uc );
		uc.lpszHostName = hostNameBuf;
		uc.dwHostNameLength = INTERNET_MAX_URL_LENGTH;
		uc.lpszUrlPath = urlPathBuf;
		uc.dwUrlPathLength = INTERNET_MAX_URL_LENGTH;

		if( InternetCrackUrl( &_url[0], static_cast<DWORD>( _url.length() ), 0, &uc ) )
		{
			std::wstring fname = PathUtils::stripPath( uc.lpszUrlPath, L'/' );
			fname = StringUtils::cutToChar( fname, L'?' );
			fname = StringUtils::cutToChar( fname, L'&' );
			fname = StringUtils::cutToChar( fname, L'#' );

			_hostName = uc.lpszHostName;
			_port = uc.nPort;
			_service = ( uc.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_SERVICE_HTTP : uc.nScheme );
			_urlPath = uc.lpszUrlPath;
			_fileName = PathUtils::sanitizeFileName( NetUtils::urlDecode( fname ) );
		}
	}

	void CHttpDownloadFile::parseResponseHeaders()
	{
		if( !_respHeaders.empty() )
		{
			auto entries = StringUtils::split( _respHeaders, L"\r\n" );
			size_t off = 0;

			for( const auto& entry : entries )
			{
				if( StringUtils::startsWith( entry, L"Content-Type:" ) )
				{
					if( entry.find( L"text/plain" ) != std::wstring::npos )
						_fileTypeText = true; // the _fileTypeText is currently ignored
				}
				else if( StringUtils::startsWith( entry, L"Content-Length:" ) )
				{
					try {
						_contentLength = std::stoll( entry.substr( 15 ) );
					}
					catch( std::invalid_argument ) {
						PrintDebug("Invalid %ls", entry.c_str());
						_contentLength = 0ll;
					}
					catch( std::out_of_range ) {
						PrintDebug("OutOfRange %ls", entry.c_str());
						_contentLength = 0ll;
					}
				}
				else if( StringUtils::startsWith( entry, L"Content-Disposition:" ) )
				{
					std::wstring fname;

					if( ( off = StringUtils::findCaseInsensitive( entry, L"FILENAME*=" ) ) != std::wstring::npos ) // utf-8
					{
						fname = entry.substr( off + 10 );
						auto encoding = StringUtils::cutToChar( fname, L'\'' );
						StringUtils::cutToChar( fname, L'\'' );

						// convert the filename to UTF-8
						if( StringUtils::findCaseInsensitive( encoding, L"UTF-8" ) != std::wstring::npos )
						{
							std::string tmpName = StringUtils::convert2A( fname/*, 28591*/ ); // TODO: codepage 8859-1
							tmpName = NetUtils::urlDecode( tmpName );
							fname = StringUtils::convert2W( tmpName );
						}
						else
							fname = NetUtils::urlDecode( fname );
					}

					if( fname.empty() && ( off = StringUtils::findCaseInsensitive( entry, L"FILENAME=" ) ) != std::wstring::npos )
					{
						// strip quotes off the filename
						if( entry[off + 9] == L'\"' )
						{
							size_t offEnd = entry.find_first_of( L'\"', off + 10 );
							fname = entry.substr( off + 10, offEnd != std::wstring::npos ? offEnd - off - 10 : offEnd );
						}
						else
						{
							size_t offEnd = entry.find_last_of( L';' );
							fname = entry.substr( off + 9, offEnd != std::wstring::npos ? offEnd - off - 9 : offEnd );
						}

						StringUtils::trim( fname );
						fname = NetUtils::urlDecode( fname );
					}

					// sanitize the filename
					_fileName = fname.empty() ? fname : PathUtils::sanitizeFileName( fname );
				}
			}
		}
	}

	std::wstring CHttpDownloadFile::getStatusText( LONGLONG total )
	{
		std::wostringstream sstr;
		sstr << FsUtils::bytesToString( total );

		if( _contentLength > 0ll )
			sstr << L"/" << FsUtils::bytesToString( _contentLength );

		return sstr.str();
	}

	LRESULT CALLBACK CHttpDownloadFile::wndProcControls( HWND hWnd, UINT message, WPARAM wp, LPARAM lp )
	{
		switch( message )
		{
		case WM_COMMAND:
			switch( LOWORD( wp ) )
			{
			case IDA_PREVTAB:
			case IDA_NEXTTAB:
			{
				int tabId = TabCtrl_GetCurSel( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_HEADERS ) );
				TabCtrl_SetCurSel( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_HEADERS ), !tabId ? 1 : 0 );

				updateHeaders();
				break;
			}
			}
			break;
		}

		return DefSubclassProc( hWnd, message, wp, lp );
	}

	//
	// Message handler for file downloader
	//
	INT_PTR CALLBACK CHttpDownloadFile::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case UM_WORKERNOTIFY:
			if( !_initialized )
			{
				if( wParam == FC_ARCHDONEFAIL )
				{
					std::wostringstream sstr;
					sstr << L"Cannot obtain HEAD from given URL.\r\n" << _errorMsg;
					MessageBox( _hDlg, sstr.str().c_str(), L"Download File Error", MB_ICONEXCLAMATION | MB_OK );
				}
				else
					_initialized = true;

				SetDlgItemText( _hDlg, IDE_DOWNLOADFILE_FILENAME, _fileName.c_str() );

				updateHeaders( false );
				updateGuiStatus();
				updateDialogTitle();
			}
			else
			{
				if( wParam == 2 )
				{
					// file already exists - ask user what to do
					int *mode = reinterpret_cast<int*>( lParam );
					if( mode && *mode )
						*mode = MessageBox( _hDlg, L"Overwrite existing file?", L"File already exists", MB_ICONQUESTION | MB_YESNO );
					else if( mode )
						*mode = MessageBox( _hDlg, L"Try to download the rest of an incomplete file?\r\nOtherwise the file will be overwritten.",
							L"File already exists", MB_ICONQUESTION | MB_YESNOCANCEL );
					break;
				}
				else if( wParam == 3 )
				{
					// update progress
					double pct = (double)lParam / (double)_contentLength * 100.0;
					updateDialogTitle( std::to_wstring( static_cast<int>( pct ) ) + L" %" );
					SetDlgItemText( _hDlg, IDC_DOWNLOADFILE_STATUSTEXT, getStatusText( static_cast<LONGLONG>( lParam ) ).c_str() );
					SendDlgItemMessage( _hDlg, IDC_DOWNLOADFILE_PROGRESS, PBM_SETPOS, static_cast<WPARAM>( pct ), 0 );

					// change the download button caption
					if( !IsWindowEnabled( GetDlgItem( _hDlg, IDOK ) ) )
					{
						EnableWindow( GetDlgItem( _hDlg, IDOK ), TRUE );
						SetDlgItemText( _hDlg, IDOK, L"Stop" );
						updateHeaders( false );
					}
					break;
				}

				if( wParam == FC_ARCHDONEFAIL && !_canceled )
				{
					// retry on error and/or when zero bytes received
					if( _attemptCount && !_dataCorrupted && ( _zeroBytesReceived || _errorId != NO_ERROR ) )
					{
						_attemptCount--;

						updateDialogTitle( L"Retrying.." );
						SetTimer( _hDlg, FC_TIMER_KEEPALIVE_ID, MiscUtils::getRand( 4500, 5500 ), NULL );
						break;
					}
					else
					{
						std::wostringstream sstr;
						sstr << L"File download error from given URL.\r\n" << _errorMsg;
						MessageBox( _hDlg, sstr.str().c_str(), L"Download File Error", MB_ICONEXCLAMATION | MB_OK );

						// reset failed attempts counter
						_attemptCount = FailedAttemptsMax;
					}
				}
				else if( wParam == FC_ARCHDONEOK )
					MessageBox( _hDlg, L"File successfully downloaded.", L"Download File", MB_ICONINFORMATION | MB_OK );

				_canceled = false;

				updateGuiStatus();
				updateDialogTitle();

				if( wParam == FC_ARCHDONEOK )
					close(); // close the dialog
			}
			break;

		case WM_COMMAND:
			switch( LOWORD( wParam ) )
			{
			case IDC_DOWNLOADFILE_CHOOSEFILE:
				onChooseFileName();
				break;
			case IDE_DOWNLOADFILE_URL:
			case IDE_DOWNLOADFILE_FILENAME:
				if( HIWORD( wParam ) == EN_CHANGE )
				{
					if( _initialized && LOWORD( wParam ) == IDE_DOWNLOADFILE_URL )
					{
						std::wstring url;
						MiscUtils::getWindowText( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_URL ), url );

						if( url != _url )
						{
							// url has been changed - reinit
							_initialized = false;

							_rqstHeaders.clear();
							updateHeaders( false );
						}
					}

					updateGuiStatus();
				}
				break;

			default:
				break;
			}
			break;

		case WM_NOTIFY:
			if( LOWORD( wParam ) == IDC_DOWNLOADFILE_HEADERS && ((LPNMHDR)lParam)->code == TCN_SELCHANGE )
			{
				updateHeaders();
			}
			break;

		case WM_TIMER:
			// restart the failed download
			startDownload();
			KillTimer( _hDlg, FC_TIMER_KEEPALIVE_ID );
			break;

		case WM_SIZE:
			updateLayout( LOWORD( lParam ), HIWORD( lParam ) );
			break;
		}

		return (INT_PTR)false;
	}

	void CHttpDownloadFile::updateLayout( int width, int height )
	{
		MoveWindow( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_URL ), 11, 28, width - 22, 23, true );
		MoveWindow( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_HEADERS ), 11, 80, width - 22, height - 185, true );
		MoveWindow( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_FILENAME ), 11, height - 90, width - 22, 12, true );
		MoveWindow( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_FILENAME ), 11, height - 72, width - 57, 23, true );
		MoveWindow( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_CHOOSEFILE ), width - 39, height - 73, 27, 23, true );
		MoveWindow( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_PROGRESS ), 11, height - 31, 92, 15, true );
		MoveWindow( GetDlgItem( _hDlg, IDC_DOWNLOADFILE_STATUSTEXT ), 107, height - 30, 190, 13, true );
		MoveWindow( GetDlgItem( _hDlg, IDOK ), width - 168, height - 34, 75, 23, true );
		MoveWindow( GetDlgItem( _hDlg, IDCANCEL ), width - 86, height - 34, 75, 23, true );
	}
}
