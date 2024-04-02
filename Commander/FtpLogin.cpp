#include "stdafx.h"

#include "Commander.h"
#include "FtpLogin.h"

#include "MiscUtils.h"

namespace Commander
{
	CFtpLogin::CFtpLogin()
		: _upFtpData( nullptr )
		, _cancelPressed( false )
		, _hWndTip( nullptr )
	{
		_upFtpData = std::make_unique<CFtpReader::FtpData>();

		_upFtpData->session.logonInfo = nsFTP::CLogonInfo(
			L"ftp.ubuntu.com",
			nsFTP::T_enConstants::DEFAULT_FTP_PORT,
			L"anonymous",
			L""
			);

		_upFtpData->session.initialPath = L"/";
	}

	CFtpLogin::~CFtpLogin()
	{
	}

	bool CFtpLogin::_loginProcCore()
	{
		_upFtpData->ftpClient.AttachObserver( this );

		bool retVal = _upFtpData->ftpClient.Login( _upFtpData->session.logonInfo );

		if( retVal )
		{
			if( _upFtpData->ftpClient.ChangeWorkingDirectory( _upFtpData->session.initialPath ) != nsFTP::FTP_OK )
				_upFtpData->session.initialPath = L"/";
		}

		_upFtpData->ftpClient.DetachObserver( this );

		return retVal;
	}

	void CFtpLogin::updateToolTip( const std::wstring& tip )
	{
		// update tooltip text
		TOOLINFO toolInfo = { 0 };
		toolInfo.cbSize = sizeof( toolInfo );
		toolInfo.hwnd = _hDlg;
		toolInfo.uId = (UINT_PTR)GetDlgItem( _hDlg, IDC_FTP_STATUS );
		toolInfo.lpszText = (TCHAR*)tip.c_str();

		SendMessage( _hWndTip, TTM_UPDATETIPTEXT, 0, (LPARAM)&toolInfo );
	}

	void CFtpLogin::onInit()
	{
		// create tooltip window
		_hWndTip = CreateWindowEx(
			WS_EX_TOOLWINDOW, TOOLTIPS_CLASS, NULL,
			WS_POPUP | WS_CLIPSIBLINGS | TTS_NOPREFIX,
			CW_USEDEFAULT, CW_USEDEFAULT,
			CW_USEDEFAULT, CW_USEDEFAULT,
			_hDlg, NULL, FCS::inst().getFcInstance(), NULL );

		// associate tooltip with static control (SS_NOTIFY style needed)
		TOOLINFO toolInfo = { 0 };
		toolInfo.cbSize = sizeof( toolInfo );
		toolInfo.hwnd = _hDlg;
		toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
		toolInfo.uId = (UINT_PTR)GetDlgItem( _hDlg, IDC_FTP_STATUS );

		SendMessage( _hWndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo );

		// update dialog's texts and its controls' status
		SetDlgItemText( _hDlg, IDE_FTP_ADDRESS, _upFtpData->session.logonInfo.Hostname().c_str() );
		SetDlgItemText( _hDlg, IDE_FTP_PATH, _upFtpData->session.initialPath.c_str() );
		SetDlgItemText( _hDlg, IDE_FTP_USERNAME, _upFtpData->session.logonInfo.Username().c_str() );
		SetDlgItemText( _hDlg, IDE_FTP_USERPASSWORD, _upFtpData->session.logonInfo.Password().c_str() );

		CheckDlgButton( _hDlg, IDC_FTP_PASSIVE, BST_UNCHECKED );
		ShowWindow( GetDlgItem( _hDlg, IDC_FTP_STATUS ), SW_HIDE );
		ShowWindow( GetDlgItem( _hDlg, IDC_FTP_PROGRESS ), SW_HIDE );

		MiscUtils::centerOnWindow( _hDlgParent, _hDlg );

		_worker.init( [this] { return _loginProcCore(); }, _hDlg, UM_WORKERNOTIFY );
	}

	bool CFtpLogin::onOk()
	{
		WCHAR tempStr[MAX_PATH];
		GetDlgItemText( _hDlg, IDE_FTP_ADDRESS, tempStr, sizeof( tempStr ) );
		std::wstring hostName = tempStr;

		GetDlgItemText( _hDlg, IDE_FTP_PATH, tempStr, sizeof( tempStr ) );
		_upFtpData->session.initialPath = tempStr;

		GetDlgItemText( _hDlg, IDE_FTP_USERNAME, tempStr, sizeof( tempStr ) );
		std::wstring userName = tempStr;

		GetDlgItemText( _hDlg, IDE_FTP_USERPASSWORD, tempStr, sizeof( tempStr ) );
		std::wstring userPassword = tempStr;

		if( StringUtils::startsWith( hostName, L"ftp://" ) ) // 6 is the length of 'ftp://' string
			hostName.erase( hostName.begin(), hostName.begin() + 6 );

		hostName = PathUtils::stripDelimiter( hostName, L'/' );
		PathUtils::unifyDelimiters( _upFtpData->session.initialPath, false );

		bool utf8 = false; // TODO
		_upFtpData->session.passive = IsDlgButtonChecked( _hDlg, IDC_FTP_PASSIVE ) == BST_CHECKED;
		_upFtpData->session.logonInfo.SetHost( hostName, nsFTP::T_enConstants::DEFAULT_FTP_PORT, userName, userPassword, L"", utf8 );

		WCHAR status[] = L"Connecting";
		SetDlgItemText( _hDlg, IDC_FTP_STATUS, status );
		updateToolTip( status );

		EnableWindow( GetDlgItem( _hDlg, IDE_FTP_ADDRESS ), FALSE );
		EnableWindow( GetDlgItem( _hDlg, IDE_FTP_PATH ), FALSE );
		EnableWindow( GetDlgItem( _hDlg, IDE_FTP_USERNAME ), FALSE );
		EnableWindow( GetDlgItem( _hDlg, IDE_FTP_USERPASSWORD ), FALSE );
		EnableWindow( GetDlgItem( _hDlg, IDC_FTP_PASSIVE ), FALSE );
		EnableWindow( GetDlgItem( _hDlg, IDOK ), FALSE );

		ShowWindow( GetDlgItem( _hDlg, IDC_FTP_STATUS ), SW_SHOW );
		ShowWindow( GetDlgItem( _hDlg, IDC_FTP_PROGRESS ), SW_SHOW );
		SendMessage( GetDlgItem( _hDlg, IDC_FTP_PROGRESS ), PBM_SETMARQUEE, 1, 0 );

		_worker.start();

		return false; // do not close dialog
	}

	bool CFtpLogin::onClose()
	{
		if( _worker.isRunning() )
		{
			WCHAR status[] = L"Canceling..";
			EnableWindow( GetDlgItem( _hDlg, IDCANCEL ), FALSE );
			SetDlgItemText( _hDlg, IDC_FTP_STATUS, status );
			updateToolTip( status );

			_cancelPressed = true;
			_worker.stop_async( true );

			return false;
		}

		return true;
	}

	void CFtpLogin::onDestroy()
	{
		DestroyWindow( _hWndTip );
	}

	void CFtpLogin::OnInternalError( const tstring& errorMsg, const tstring& fileName, DWORD lineNr )
	{
		MessageBox( _hDlg, errorMsg.c_str(), L"Ftp Client Error", MB_ICONEXCLAMATION | MB_OK );
	}

	void CFtpLogin::OnSendCommand( const nsFTP::CCommand& command, const nsFTP::CArg& arguments )
	{
		_upCommand = std::make_unique<std::pair<nsFTP::CCommand, nsFTP::CArg>>( command, arguments );

		PrintDebug("Command sent: %ls, args: %zu", command.AsString().c_str(), arguments.size());
	}

	void CFtpLogin::OnResponse( const nsFTP::CReply& reply )
	{
		std::wstring strCmd = L"[unknown]";

		if( _upCommand )
			strCmd = _upCommand->first.AsString();

		if( _upCommand && _upCommand->first != nsFTP::CCommand::SIZE() && reply.Code().IsNegativeReply() )
		{
			std::wostringstream sstr;

			for( auto it = _upCommand->second.begin(); it != _upCommand->second.end(); ++it )
				sstr << L"Arg: " << ( _upCommand->first == nsFTP::CCommand::PASS() ? L"*****" : *it ) << L"\r\n";

			sstr << L"\r\n" << strCmd << L" Response: " << reply.Value();

			MessageBox( _hDlg, sstr.str().c_str(), L"Ftp Response", MB_ICONEXCLAMATION | MB_OK );
		}

		PrintDebug("%ls Response: %ls", strCmd.c_str(), reply.Value().c_str());
	}

	INT_PTR CALLBACK CFtpLogin::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case UM_WORKERNOTIFY:
			if( wParam == 0 )
			{
				if( !_cancelPressed )
				{
					WCHAR status[] = L"Login Failed";
					SetDlgItemText( _hDlg, IDC_FTP_STATUS, status );
					updateToolTip( status );
					ShowWindow( GetDlgItem( _hDlg, IDC_FTP_PROGRESS ), SW_HIDE );
					EnableWindow( GetDlgItem( _hDlg, IDE_FTP_ADDRESS ), TRUE );
					EnableWindow( GetDlgItem( _hDlg, IDE_FTP_PATH ), TRUE );
					EnableWindow( GetDlgItem( _hDlg, IDE_FTP_USERNAME ), TRUE );
					EnableWindow( GetDlgItem( _hDlg, IDE_FTP_USERPASSWORD ), TRUE );
					EnableWindow( GetDlgItem( _hDlg, IDC_FTP_PASSIVE ), TRUE );
					EnableWindow( GetDlgItem( _hDlg, IDOK ), TRUE );
				}
				else
					EndDialog( _hDlg, (INT_PTR)IDCANCEL );
			}
			else
			{
				_dlgResult = _upFtpData->session.initialPath;

				auto dataMan = FCS::inst().getApp().getActivePanel().getActiveTab()->getDataManager();
				dataMan->setFtpData( std::move( _upFtpData ) );

				EndDialog( _hDlg, (INT_PTR)IDOK );
			}
			break;

		case WM_COMMAND:
			if( HIWORD( wParam ) == EN_CHANGE )
			{
				bool res = true;
				res = res && !!GetWindowTextLength( GetDlgItem( _hDlg, IDE_FTP_ADDRESS ) );
				res = res && !!GetWindowTextLength( GetDlgItem( _hDlg, IDE_FTP_PATH ) );
				res = res && !!GetWindowTextLength( GetDlgItem( _hDlg, IDE_FTP_USERNAME ) );
				//res = res && !!GetWindowTextLength( GetDlgItem( _hDlg, IDE_FTP_USERPASSWORD ) );
				EnableWindow( GetDlgItem( _hDlg, IDOK ), res );
			}
			break;
		}

		return (INT_PTR)false;
	}
}
