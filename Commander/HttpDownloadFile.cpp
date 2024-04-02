#include "stdafx.h"

#include "Commander.h"
#include "HttpDownloadFile.h"
#include "MiscUtils.h"

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

		//SetWindowText( _hDlg, L"Download file from URL" );

		_hInternet = nullptr;
		_hSession = nullptr;
		_hUrl = nullptr;
		_initialized = false;
		_canceled = false;
		_fileTypeText = false;
		_fileSize = -1ll;
		_offset = 0ull;
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
		_headers.clear();

		_worker.start();
	}

	//
	// Ok/Get HEAD button pressed
	//
	bool CHttpDownloadFile::onOk()
	{
		if( !_initialized && MiscUtils::getWindowText( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_URL ), _dlgResult ) )
			_url = _dlgResult;

		if( _initialized && MiscUtils::getWindowText( GetDlgItem( _hDlg, IDE_DOWNLOADFILE_FILENAME ), _dlgResult ) )
			_fileName = _dlgResult;

		startDownload();

		return false;
	}

	void CHttpDownloadFile::onDestroy()
	{
		closeUrl();

		if( _hInternet )
		{
			InternetCloseHandle( _hInternet );
			_hInternet = nullptr;
		}
	}

	void CHttpDownloadFile::updateDialogTitle( const std::wstring& status )
	{
		std::wostringstream sstrTitle;

		if( !status.empty() )
			sstrTitle << L"[" << status << L"] - " << _dialogTitle;
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

		SetDlgItemText( _hDlg, IDOK, _initialized ? L"Download" : L"Get HEAD" );
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

				if( HttpSendRequest( _hUrl, _headers.c_str(), static_cast<DWORD>( _headers.length() ), NULL, 0 ) )
					return true;
			}
		}

		return false;
	}

	bool CHttpDownloadFile::tryOpenUrl( const std::wstring& verb )
	{
		bool ret = true;

		// open url
		_hUrl = InternetOpenUrl( _hInternet, &_url[0], _headers.c_str(), static_cast<DWORD>( _headers.length() ), 0, INTERNET_NO_CALLBACK );

		if( !_hUrl ) // TODO: test and handle errora
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

		return strOut;
	}

	bool CHttpDownloadFile::getHeader( DWORD& code )
	{
		if( tryOpenUrl( L"HEAD" ) )
		{
			// read response
			if( queryStatusCode( _hUrl, code ) )
			{
				// read and parse headers
				readHeaders();
				parseHeaders();

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

	bool CHttpDownloadFile::downloadFile( DWORD& code )
	{
		int mode = std::ios::out;

		if( checkFileOnDisk() )
		{
			// file already exists, ask user what to do
			mode = ( _fileSize != _offset ) ? 0 : 1;
			_worker.sendMessage( 2, reinterpret_cast<LPARAM>( &mode ) );

			if( _fileSize != _offset )
				mode = ( mode == IDYES ) ? std::ios::app : ( mode == IDNO ) ? std::ios::out : -1; // yes-append, no-overwrite, cancel-cancel
			else
				mode = ( mode == IDYES ) ? std::ios::out : -1; // yes-overwrite, no-cancel
		}

		if( mode != -1 )
		{
			// reset offset when overwriting file
			if( mode != std::ios::app )
				_offset = 0ull;
			else
				_headers = L"Range: bytes=" + std::to_wstring( _offset ) + L"-";

			if( tryOpenUrl( L"GET" ) )
			{
				// read response
				if( queryStatusCode( _hUrl, code ) )
				{
					if( code >= HTTP_STATUS_OK && code <= HTTP_STATUS_PARTIAL_CONTENT )
					{
						// finally download the file
						return downloadFileCore( mode );
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

	bool CHttpDownloadFile::downloadFileCore( int mode )
	{
		if( _hUrl )
		{
			char data[32768];
			DWORD dwBytesRead = 0;
			LONGLONG total = _offset;

			// is binary file
			mode |= _fileTypeText ? 0 : std::ios::binary;
			std::ofstream fs( PathUtils::getExtendedPath( _curDir + _fileName ), mode );

			ULONGLONG ticks = GetTickCount64();
			BOOL ret = FALSE;

			// read the data and store it in a file
			while( ( ret = InternetReadFile( _hUrl, data, sizeof( data ), &dwBytesRead ) ) && dwBytesRead > 0 )
			{
				total += dwBytesRead;

				if( _fileSize > 0ll && ticks + 200 < GetTickCount64() )
				{
					// report progress every 200ms
					_worker.sendNotify( 3, (LPARAM)( ( (double)total / (double)_fileSize ) * 100.0 ) );
					ticks = GetTickCount64();
				}

				// write data to file
				fs.write( data, dwBytesRead );

				if( !_worker.isRunning() )
					return false;
			}

			// reading data has been interrupted
			if( !ret )
			{
				_errorId = GetLastError();
				return false;
			}

			if( _worker.isRunning() ) // report the last "100 %"
				_worker.sendNotify( 3, (LPARAM)100.0 );

			return _fileSize > 0ll ? ( total == _fileSize ) : true;
		}

		return false;
	}

	void CHttpDownloadFile::readHeaders()
	{
		WCHAR *buf = nullptr;
		DWORD bufLen = 0;
		DWORD idx = 0;

		_headers = queryRawHeader( _hUrl, &buf, &bufLen, &idx );

		while( idx != 0 )
		{
			_headers += L"\r\n\r\n";
			_headers += queryRawHeader( _hUrl, &buf, &bufLen, &idx );
		}

		if( buf )
			delete[] buf;
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
			DWORD status = 0;
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
				ret = downloadFile( status );
			}

			if( ret == false )
			{
				if( _errorId != NO_ERROR )
					_errorMsg = SysUtils::getErrorMessage( _errorId, GetModuleHandle( L"wininet.dll" ) );
				else
					_errorMsg = std::to_wstring( status ) + L": " + queryStatusText( _hUrl );
			}

			closeUrl();
		}

		return ret;
	}

	bool CHttpDownloadFile::checkFileOnDisk()
	{
		ZeroMemory( &_wfd, sizeof( _wfd ) );

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

		if( InternetCrackUrl( &_url[0], static_cast<DWORD>( _url.length() ), ICU_ESCAPE, &uc ) )
		{
			std::wstring fname = PathUtils::stripPath( uc.lpszUrlPath, L'/' );
			fname = StringUtils::cutToChar( fname, L'?' );
			fname = StringUtils::cutToChar( fname, L'&' );
			fname = StringUtils::cutToChar( fname, L'#' );

			_hostName = uc.lpszHostName;
			_port = uc.nPort;
			_service = ( uc.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_SERVICE_HTTP : uc.nScheme );
			_urlPath = uc.lpszUrlPath;
			_fileName = PathUtils::sanitizeFileName( fname );
		}
	}

	void CHttpDownloadFile::parseHeaders()
	{
		if( !_headers.empty() )
		{
			auto entries = StringUtils::split( _headers, L"\r\n" );
			size_t off = 0;

			for( const auto& entry : entries )
			{
				if( StringUtils::startsWith( entry, L"Content-Type:" ) )
				{
					if( entry.find( L"text/plain" ) != std::wstring::npos )
						_fileTypeText = true;
				}
				else if( StringUtils::startsWith( entry, L"Content-Length:" ) )
				{
					try {
						_fileSize = std::stoll( entry.substr( 15 ) );
					}
					catch( std::invalid_argument ) {
						PrintDebug("Invalid %ls", entry.c_str());
						_fileSize = 0ll;
					}
					catch( std::out_of_range ) {
						PrintDebug("OutOfRange %ls", entry.c_str());
						_fileSize = 0ll;
					}
				}
				else if( StringUtils::startsWith( entry, L"Content-Disposition:" ) )
				{
					if( ( off = entry.find( L"filename=" ) ) != std::wstring::npos )
					{
						std::wstring fname;
						// strip quotes off the filename
						if( entry[off + 9] == L'\"' )
						{
							size_t offEnd = entry.find_first_of( L'\"', off + 10 );
							fname = entry.substr( off + 10, offEnd != std::wstring::npos ? offEnd - off - 10 : offEnd );
						}
						else
							fname = entry.substr( off + 9 );

						_fileName = PathUtils::sanitizeFileName( fname );
					}
				}
			}
		}
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

				SetDlgItemText( _hDlg, IDE_DOWNLOADFILE_HEADERS, _headers.c_str() );
				SetDlgItemText( _hDlg, IDE_DOWNLOADFILE_FILENAME, _fileName.c_str() );

				updateGuiStatus();
				updateDialogTitle( L"" );
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
					updateDialogTitle( std::to_wstring( lParam ) + L" %" );
					SendDlgItemMessage( _hDlg, IDC_DOWNLOADFILE_PROGRESS, PBM_SETPOS, static_cast<WPARAM>( lParam ), 0 );
					break;
				}

				if( wParam == FC_ARCHDONEFAIL && !_canceled )
				{
					std::wostringstream sstr;
					sstr << L"File download error from given URL.\r\n" << _errorMsg;
					MessageBox( _hDlg, sstr.str().c_str(), L"Download File Error", MB_ICONEXCLAMATION | MB_OK );
				}
				else if( wParam == FC_ARCHDONEOK )
					MessageBox( _hDlg, L"File successfully downloaded.", L"Download File", MB_ICONINFORMATION | MB_OK );

				if( _canceled ) // when canceled stay initialized
					_canceled = false;
			//	else
			//		_initialized = false;

				updateGuiStatus();
				updateDialogTitle( L"" );

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
						// url has been changed - reinit
						_initialized = false;
					}

					updateGuiStatus();
				}
				break;

			default:
				break;
			}
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
		MoveWindow( GetDlgItem( _hDlg, IDOK ), width - 168, height - 34, 75, 23, true );
		MoveWindow( GetDlgItem( _hDlg, IDCANCEL ), width - 86, height - 34, 75, 23, true );
	}
}
