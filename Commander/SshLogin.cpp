#include "stdafx.h"

#include "Commander.h"
#include "SshLogin.h"

#include "ComboBoxUtils.h"
#include "MiscUtils.h"

namespace Commander
{
	CSshConnect::CSshConnect()
		: _upSshData( nullptr )
		, _cancelPressed( false )
		, _hWndTip( nullptr )
	{
		_upSshData = std::make_unique<CSshReader::SshData>();

		_upSshData->shell.pWorker = &_worker;
		_upSshData->shell.FOnCaptureOutput = std::bind( &CSshConnect::onCaptureOutput, this, std::placeholders::_1, std::placeholders::_2 );

		_upSshData->session.FHostName = L"192.168.192.161";
		_upSshData->session.FRemoteDirectory = L"/home/vmware";

		_upSshData->session.FUserName = L"vmware";
		_upSshData->session.FPassphrase = L"vmware";
	}

	CSshConnect::~CSshConnect()
	{
	}

	//
	// On error oputput
	//
	void CSshConnect::onCaptureOutput( const std::wstring& str, bcb::TCaptureOutputType outputType )
	{
		if( outputType == bcb::TCaptureOutputType::cotError )
		{
			MessageBox( _hDlg, str.c_str(), L"PuTTY Error", MB_ICONEXCLAMATION | MB_OK );
		}
	}

	void CSshConnect::detectReturnVar()
	{
		std::vector<std::wstring> returnVars = { L"status", L"?" };

		for( size_t i = 0; i < returnVars.size(); i++ )
		{
			PrintDebug("Trying \"%ls\" as ReturnVar.", returnVars[i].c_str());

			auto resp = _upSshData->shell.SendCommandFull( bcb::fsVarValue, returnVars[i] );
			if( resp.size() == 1 && !resp[0].empty() )
			{
				try
				{
					int res = std::stoi( resp[0] );

					if( res >= 0 && res <= 0xFF )
					{
						_upSshData->session.FReturnVar = returnVars[i];
						PrintDebug("ReturnVar ok.");
						break;
					}
					else
						PrintDebug("ReturnVar error, value: '%ls'.", resp[0].c_str());
				}
				catch( const std::invalid_argument& e )
				{
					e.what();
					PrintDebug("ReturnVar error, value: '%ls'.", resp[0].c_str());
				}
			}
		}
	}

	void CSshConnect::tryChangeDirectory()
	{
		_upSshData->shell.SendCommandFull( bcb::fsChangeDirectory, _upSshData->session.FRemoteDirectory.get() );
		auto resp = _upSshData->shell.SendCommandFull( bcb::fsCurrentDirectory );

		if( resp.size() == 1 && !resp[0].empty() )
		{
			if( _upSshData->session.FRemoteDirectory == resp[0] )
			{
				PrintDebug("Directory change ok: '%ls'.", resp[0].c_str());
			}
			else // fall down to root directory
				_upSshData->session.FRemoteDirectory = L"/";
		}
	}

	void CSshConnect::updateToolTip( const std::wstring& tip )
	{
		// update tooltip text
		TOOLINFO toolInfo = { 0 };
		toolInfo.cbSize = sizeof( toolInfo );
		toolInfo.hwnd = _hDlg;
		toolInfo.uId = (UINT_PTR)GetDlgItem( _hDlg, IDC_SSH_STATUS );
		toolInfo.lpszText = (TCHAR*)tip.c_str();

		SendMessage( _hWndTip, TTM_UPDATETIPTEXT, 0, (LPARAM)&toolInfo );
	}

	bool CSshConnect::_connectSshCore()
	{
		try
		{
			_upSshData->shell.FSimple = true;
			_upSshData->shell.Open();

			if( _upSshData->shell.FOpened )
			{
				detectReturnVar();
				tryChangeDirectory();
			}
		}
		catch( bcb::ESshFatal& e )
		{
			_status = StringUtils::convert2W( e.what() );
			return false;
		}

		return _upSshData->shell.FOpened;
	}

	std::vector<std::wstring> getKnownGuestsList()
	{
		std::vector<std::wstring> list;

		LSTATUS res = 0;
		HKEY hKey = nullptr;

		if( ( res = RegOpenKeyEx( HKEY_CURRENT_USER, _T(PUTTY_REG_POS) L"\\SshHostKeys", 0, KEY_READ, &hKey ) ) == ERROR_SUCCESS )
		{
			DWORD count; // number of values for this key 
			res = RegQueryInfoKey( hKey, NULL, NULL, NULL, NULL, NULL, NULL, &count, NULL, NULL, NULL, NULL );

			WCHAR valName[MAX_PATH];
			DWORD valLength, dwSize = 0, dwType = 0;

			for( DWORD i = 0; i < count; i++ )
			{
				valName[0] = L'\0';
				valLength = MAX_PATH;

				res = RegEnumValue( hKey, i, valName, &valLength, NULL, &dwType, NULL, &dwSize );
				if( res == ERROR_SUCCESS && dwType == REG_SZ )
				{
					list.push_back( valName );
				}
			}

			RegCloseKey( hKey );
		}

		return list;
	}

	void CSshConnect::onInit()
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
		toolInfo.uId = (UINT_PTR)GetDlgItem( _hDlg, IDC_SSH_STATUS );

		SendMessage( _hWndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo );

		// update dialog's texts and its controls' status
		for( const auto& guest: getKnownGuestsList() )
		{
			auto idx = guest.find_last_of( L':' );
			idx = ( idx == std::wstring::npos ? 0 : idx + 1 );

			CbUtils::addString( GetDlgItem( _hDlg, IDE_SSH_ADDRESS ), guest.substr( idx ).c_str() );
		}

		SetDlgItemText( _hDlg, IDE_SSH_ADDRESS, _upSshData->session.FHostName.c_str() );
		SetDlgItemText( _hDlg, IDE_SSH_PATH, _upSshData->session.FRemoteDirectory.c_str() );
		SetDlgItemText( _hDlg, IDE_SSH_USERNAME, _upSshData->session.FUserName.c_str() );
		SetDlgItemText( _hDlg, IDE_SSH_USERPASSWORD, _upSshData->session.FPassphrase.c_str() );

		CheckDlgButton( _hDlg, IDC_SSH_SCPONLY, BST_CHECKED );
		ShowWindow( GetDlgItem( _hDlg, IDC_SSH_STATUS ), SW_HIDE );
		ShowWindow( GetDlgItem( _hDlg, IDC_SSH_PROGRESS ), SW_HIDE );

		MiscUtils::centerOnWindow( _hDlgParent, _hDlg );

		_worker.init( [this] { return _connectSshCore(); }, _hDlg, UM_WORKERNOTIFY );
	//	_upSshData->shell.pWorker = &_worker;
	}

	bool CSshConnect::onOk()
	{
		WCHAR tempStr[MAX_PATH];
		GetDlgItemText( _hDlg, IDE_SSH_ADDRESS, tempStr, sizeof( tempStr ) );
		_upSshData->session.FHostName = tempStr;

		GetDlgItemText( _hDlg, IDE_SSH_PATH, tempStr, sizeof( tempStr ) );
		_upSshData->session.FRemoteDirectory = tempStr;

		GetDlgItemText( _hDlg, IDE_SSH_USERNAME, tempStr, sizeof( tempStr ) );
		_upSshData->session.FUserName = tempStr;

		GetDlgItemText( _hDlg, IDE_SSH_USERPASSWORD, tempStr, sizeof( tempStr ) );
		_upSshData->session.FPassphrase = tempStr;

		WCHAR status[] = L"Connecting";
		SetDlgItemText( _hDlg, IDC_SSH_STATUS, status );
		updateToolTip( status );

		EnableWindow( GetDlgItem( _hDlg, IDE_SSH_ADDRESS ), FALSE );
		EnableWindow( GetDlgItem( _hDlg, IDE_SSH_PATH ), FALSE );
		EnableWindow( GetDlgItem( _hDlg, IDE_SSH_USERNAME ), FALSE );
		EnableWindow( GetDlgItem( _hDlg, IDE_SSH_USERPASSWORD ), FALSE );
		EnableWindow( GetDlgItem( _hDlg, IDC_SSH_SCPONLY ), FALSE );
		EnableWindow( GetDlgItem( _hDlg, IDOK ), FALSE );

		ShowWindow( GetDlgItem( _hDlg, IDC_SSH_STATUS ), SW_SHOW );
		ShowWindow( GetDlgItem( _hDlg, IDC_SSH_PROGRESS ), SW_SHOW );
		SendMessage( GetDlgItem( _hDlg, IDC_SSH_PROGRESS ), PBM_SETMARQUEE, 1, 0 );

		_worker.start();

		return false; // do not close dialog
	}

	bool CSshConnect::onClose()
	{
		if( _worker.isRunning() )
		{
			WCHAR status[] = L"Canceling..";
			EnableWindow( GetDlgItem( _hDlg, IDCANCEL ), FALSE );
			SetDlgItemText( _hDlg, IDC_SSH_STATUS, status );
			updateToolTip( status );

			_cancelPressed = true;
			_worker.stop_async( true );

			return false;
		}

		return true;
	}

	void CSshConnect::onDestroy()
	{
		DestroyWindow( _hWndTip );
	}

	INT_PTR CALLBACK CSshConnect::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case UM_WORKERNOTIFY:
			if( wParam == 0 )
			{
				if( !_cancelPressed )
				{
					_status = _status.empty() ? _upSshData->shell.FAuthenticationLog.get() : _status;
					SetDlgItemText( _hDlg, IDC_SSH_STATUS, _status.empty() ? L"Failed" : _status.c_str() );
					updateToolTip( _status );
					ShowWindow( GetDlgItem( _hDlg, IDC_SSH_PROGRESS ), SW_HIDE );
					EnableWindow( GetDlgItem( _hDlg, IDE_SSH_ADDRESS ), TRUE );
					EnableWindow( GetDlgItem( _hDlg, IDE_SSH_PATH ), TRUE );
					EnableWindow( GetDlgItem( _hDlg, IDE_SSH_USERNAME ), TRUE );
					EnableWindow( GetDlgItem( _hDlg, IDE_SSH_USERPASSWORD ), TRUE );
					//EnableWindow( GetDlgItem( _hDlg, IDC_SSH_SCPONLY ), TRUE );
					EnableWindow( GetDlgItem( _hDlg, IDOK ), TRUE );
				}
				else
					EndDialog( _hDlg, (INT_PTR)IDCANCEL );
			}
			else
			{
				_dlgResult = _upSshData->session.FRemoteDirectory.get();

				auto dataMan = FCS::inst().getApp().getActivePanel().getActiveTab()->getDataManager();
				dataMan->setSshData( std::move( _upSshData ) );

				EndDialog( _hDlg, (INT_PTR)IDOK );
			}
			break;

		case WM_COMMAND:
			if( HIWORD( wParam ) == EN_CHANGE )
			{
				bool res = true;
				res = res && !!GetWindowTextLength( GetDlgItem( _hDlg, IDE_SSH_ADDRESS ) );
				res = res && !!GetWindowTextLength( GetDlgItem( _hDlg, IDE_SSH_PATH ) );
				res = res && !!GetWindowTextLength( GetDlgItem( _hDlg, IDE_SSH_USERNAME ) );
				//res = res && !!GetWindowTextLength( GetDlgItem( _hDlg, IDE_SSH_USERPASSWORD ) );
				EnableWindow( GetDlgItem( _hDlg, IDOK ), res );
			}
			break;
		}

		return (INT_PTR)false;
	}
}
