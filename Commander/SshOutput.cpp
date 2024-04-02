#include "stdafx.h"

#include "Commander.h"
#include "SshOutput.h"

#include "MiscUtils.h"
#include "MenuUtils.h"

namespace Commander
{
	CSshOutput::CSshOutput()
	{
		_upSshData = std::make_unique<CSshReader::SshData>();
		_upSshData->shell.pWorker = &_worker;
		_upSshData->shell.FOnCaptureOutput = std::bind( &CSshOutput::onCaptureOutput, this, std::placeholders::_1, std::placeholders::_2 );

		_commandListPos = -1;
	}

	CSshOutput::~CSshOutput()
	{
		PrintDebug("dtor");
	}

	void CSshOutput::onInit()
	{
		_worker.init( [this] { return _workerProc(); }, _hDlg, UM_STATUSNOTIFY );

		SetWindowSubclass( GetDlgItem( _hDlg, IDE_SECURESHELL_INPUT ), wndProcEditSubclass, 0, reinterpret_cast<DWORD_PTR>( this ) );

		// init system menu
		HMENU hSysMenu = GetSystemMenu( _hDlg, FALSE );
		int idx = GetMenuItemCount( hSysMenu ) - 1;

		InsertMenu( hSysMenu, idx, MF_BYPOSITION | MF_SEPARATOR, 0, NULL );
		InsertMenu( hSysMenu, idx, MF_BYPOSITION | MF_STRING, IDM_SECURESHELL_SHUTDOWN, L"Shutdown" );
		InsertMenu( hSysMenu, idx, MF_BYPOSITION | MF_STRING, IDM_SECURESHELL_CLEAR, L"Clear" );
		InsertMenu( hSysMenu, idx, MF_BYPOSITION | MF_SEPARATOR, 0, NULL );
		InsertMenu( hSysMenu, idx, MF_BYPOSITION | MF_STRING, IDM_OPTIONS_ALWAYSONTOP, L"Always on Top" );

		// limit maximum command length
		SendDlgItemMessage( _hDlg, IDE_SECURESHELL_INPUT, EM_SETLIMITTEXT, SSH_COMMAND_LENGTH_MAX, 0 );

		// init console fonts
		SendDlgItemMessage( _hDlg, IDE_SECURESHELL_INPUT, WM_SETFONT, reinterpret_cast<WPARAM>( FCS::inst().getApp().getViewFont() ), TRUE );
		SendDlgItemMessage( _hDlg, IDE_SECURESHELL_OUTPUT, WM_SETFONT, reinterpret_cast<WPARAM>( FCS::inst().getApp().getViewFont() ), TRUE );

		// store window title
		GetWindowText( _hDlg, _buf, SSH_COMMAND_LENGTH_MAX );
		_windowTitle = _buf;

		MiscUtils::centerOnWindow( FCS::inst().getFcWindow(), _hDlg );

		updateSendButtonStatus();
	}

	bool CSshOutput::onOk()
	{
		GetDlgItemText( _hDlg, IDE_SECURESHELL_INPUT, _buf, SSH_COMMAND_LENGTH_MAX );
		SetDlgItemText( _hDlg, IDE_SECURESHELL_INPUT, L"" );

		_currentCommand = _buf;

		std::wostringstream sstr;
		sstr << _upSshData->session.FUserName.get() << L"@" << _upSshData->session.FHostName.get();
		sstr << L":" << _currentDirectory << L"$ " << _buf;

		_secureShellText += sstr.str();
		_secureShellText += L"\r\n";

		updateCommandsList();
		updateWindowTitle( _currentCommand );

		_worker.start();

		return false;
	}

	bool CSshOutput::onClose()
	{
		show( SW_HIDE ); // hide dialog

		return false;
	}

	void CSshOutput::onDestroy()
	{
		updateWindowTitle( L"Disconnecting.." );
		updateSecureShellWindow();

		_worker.stop();

		RemoveWindowSubclass( GetDlgItem( _hDlg, IDE_SECURESHELL_INPUT ), wndProcEditSubclass, 0 );
	}

	//
	// On error oputput
	//
	void CSshOutput::onCaptureOutput( const std::wstring& str, bcb::TCaptureOutputType outputType )
	{
		if( outputType == bcb::TCaptureOutputType::cotError )
		{
			//MessageBox( _hDlg, str.c_str(), L"PuTTY Error", MB_ICONEXCLAMATION | MB_OK );

			_secureShellText += str;
			_secureShellText += L"\r\n";

			_worker.sendNotify( 3, 0 );
		}

		//PrintDebug("thread id: %d", GetCurrentThreadId());
	}

	void CSshOutput::readStartupMessage()
	{
		_upSshData->shell.SendCommand( bcb::fsFirstLine, SSH_FIRST_LINE );

		std::wstring line;

		// read until "fsFirstLine"
		while( _worker.isRunning() )
		{
			line = _upSshData->shell.ReceiveLine();

			if( _upSshData->shell.isLastLine( line ) )
				break;

			if( line.find( SSH_FIRST_LINE ) == std::wstring::npos )
			{
				_secureShellText += line;
				_secureShellText += L"\r\n";
			}
		}
	}

	bool CSshOutput::_workerProc()
	{
		_ASSERTE( _upSshData != nullptr );

		try
		{
			if( !_upSshData->shell.FOpened )
			{
				// try to connect
				_worker.sendNotify( 2, 2 );

				_upSshData->shell.FSimple = true;
				_upSshData->shell.Open();

				// report connection attempt result
				_worker.sendNotify( 2, _upSshData->shell.FOpened );

				if( _upSshData->shell.FOpened )
				{
					readStartupMessage();

					_upSshData->shell.SendCommandFull( bcb::fsChangeDirectory, _currentDirectory );
				}
			}
			else
			{
				_upSshData->shell.FUtfStrings = true;

				auto ret = _upSshData->shell.SendCommandFull( bcb::fsAnyCommand, _currentCommand );
				auto cwd = _upSshData->shell.SendCommandFull( bcb::fsCurrentDirectory );

				if( !cwd.empty() && !cwd[0].empty() )
					_currentDirectory = cwd[0];

				for( auto const& line : ret )
				{
					_secureShellText += line;
					_secureShellText += L"\r\n";
				}
			}
		}
		catch( bcb::ESshFatal& e )
		{
			_errorMessage = StringUtils::convert2W( e.what() );
			return false;
		}

		return true;
	}

	void CSshOutput::setParent( std::shared_ptr<CPanelTab> spPanel )
	{
		if( spPanel )
		{
			_spPanel = spPanel;

			_currentDirectory = PathUtils::stripDelimiter( spPanel->getDataManager()->getCurrentDirectory() );
			_currentDirectory = _currentDirectory.substr( wcslen( ReaderType::getModePrefix( EReadMode::Putty ) ) );
			PathUtils::unifyDelimiters( _currentDirectory, false );

			// copy session data
			_upSshData->session = spPanel->getDataManager()->getSshSession();

			// update window title
			updateWindowTitle();

			_worker.start();
		}
	}

	void CSshOutput::updateWindowTitle( const std::wstring& command )
	{
		std::wostringstream sstr;

		if( !command.empty() )
		{
			sstr << L"[" << command << L"] ";
		}

		sstr << _upSshData->session.FUserName.get() << L"@" << _upSshData->session.FHostName.get() << L":";
		sstr << _currentDirectory << L" - " << _windowTitle;

		SetWindowText( _hDlg, sstr.str().c_str() );
	}

	void CSshOutput::updateSecureShellWindow()
	{
		SetWindowText( GetDlgItem( _hDlg, IDE_SECURESHELL_OUTPUT ), &_secureShellText[0] );

		auto cnt = SendDlgItemMessage( _hDlg, IDE_SECURESHELL_OUTPUT, EM_GETLINECOUNT, 0, 0 );
		SendDlgItemMessage( _hDlg, IDE_SECURESHELL_OUTPUT, EM_LINESCROLL, 0, (LPARAM)cnt );
	}

	void CSshOutput::updateSendButtonStatus()
	{
		BOOL res = GetWindowTextLength( GetDlgItem( _hDlg, IDE_SECURESHELL_INPUT ) );
		EnableWindow( GetDlgItem( _hDlg, IDOK ), res && _upSshData->shell.FOpened && !_worker.isRunning() );
	}

	void CSshOutput::updateCommandsList()
	{
		auto it = std::find( _commandList.begin(), _commandList.end(), _currentCommand );

		if( it != _commandList.end() )
		{
			_commandList.erase( it );
		}

		_commandList.push_back( _currentCommand );
		_commandListPos = -1;
	}

	void CSshOutput::summonCommandFromList( bool prev )
	{
		if( _commandListPos == -1 )
		{
			GetDlgItemText( _hDlg, IDE_SECURESHELL_INPUT, _buf, SSH_COMMAND_LENGTH_MAX );

			if( prev && _commandList.size() != 0 )
				_commandListPos = static_cast<int>( _commandList.size() - 1 );
			else
				return;
		}
		else
		{
			if( prev && _commandListPos - 1 >= 0 )
				_commandListPos--;
			else if( !prev && _commandListPos + 1 < _commandList.size() )
				_commandListPos++;
			else
			{
				if( !prev )
				{
					SetDlgItemText( _hDlg, IDE_SECURESHELL_INPUT, _buf );
					SendDlgItemMessage( _hDlg, IDE_SECURESHELL_INPUT, EM_SETSEL, 0, -1 );

					_commandListPos = -1;
				}
				return;
			}
		}

		SetDlgItemText( _hDlg, IDE_SECURESHELL_INPUT, _commandList[_commandListPos].c_str() );
		SendDlgItemMessage( _hDlg, IDE_SECURESHELL_INPUT, EM_SETSEL, 0, -1 );
	}

	LRESULT CALLBACK CSshOutput::wndProcEdit( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_KEYDOWN:
			if( wParam == VK_UP || wParam == VK_DOWN )
			{
				summonCommandFromList( wParam == VK_UP );
				return 0;
			}
			break;
		}

		return DefSubclassProc( hWnd, message, wParam, lParam );
	}

	INT_PTR CALLBACK CSshOutput::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_ACTIVATE:
		//	if( LOWORD( wParam ) == WA_INACTIVE )
				SetFocus( GetDlgItem( _hDlg, IDE_SECURESHELL_INPUT ) );
			break;

		case WM_COMMAND:
			switch( LOWORD( wParam ) )
			{
			case IDE_SECURESHELL_INPUT:
				if( HIWORD( wParam ) == EN_CHANGE )
					updateSendButtonStatus();
				break;

			default:
				break;
			}
			break;

		case WM_SYSCOMMAND:
			switch( wParam )
			{
			case IDM_OPTIONS_ALWAYSONTOP:
			{
				HMENU hSysMenu = GetSystemMenu( _hDlg, FALSE );
				if( MenuUtils::checkItem( hSysMenu, IDM_OPTIONS_ALWAYSONTOP ) )
				{
					SetWindowPos( _hDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
					CheckMenuItem( hSysMenu, IDM_OPTIONS_ALWAYSONTOP, MF_CHECKED );
				}
				else
				{
					SetWindowPos( _hDlg, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
					CheckMenuItem( hSysMenu, IDM_OPTIONS_ALWAYSONTOP, MF_UNCHECKED );
				}
				break;
			}
			case IDM_SECURESHELL_CLEAR:
				_secureShellText.clear();
				updateSecureShellWindow();
				break;
			case IDM_SECURESHELL_SHUTDOWN:
				_currentCommand = L"echo ";
				_currentCommand += _upSshData->session.FPassphrase.get();
				_currentCommand += L" | sudo -S shutdown -h now";
				_worker.start();
				break;
			}
			break;

		case UM_STATUSNOTIFY:
			switch( wParam )
			{
			case 2: // connecting
				if( lParam == 2 )
					updateWindowTitle( L"Connecting.." );
				else
					updateWindowTitle( lParam ? L"" : L"NOT CONNECTED" );
				break;
			case 3: // capture output
				updateSecureShellWindow();
				break;
			case 0:
				if( !_errorMessage.empty() ) // report failure
					MessageBox( _hDlg, _errorMessage.c_str(), L"Ssh Error", MB_ICONEXCLAMATION | MB_OK );
			case 1:
				updateSendButtonStatus();
				updateSecureShellWindow();
				updateWindowTitle();
			default:
			//	if( _spPanel->getDataManager()->isInPuttyMode() ) {
			//		_spPanel->refresh();
				break;
			}
			break;

		case WM_CTLCOLORDLG:
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORSTATIC:
	//	case WM_CTLCOLORSCROLLBAR: // MSDN: Scrollbars attached to a window do not generate this message.
			SetTextColor( reinterpret_cast<HDC>( wParam ), RGB( 192, 192, 192 ) );
			SetBkColor( reinterpret_cast<HDC>( wParam ), FC_COLOR_TEXT );
			return reinterpret_cast<LRESULT>( GetStockBrush( BLACK_BRUSH ) );

		case WM_SIZE:
			MoveWindow( GetDlgItem( _hDlg, IDE_SECURESHELL_OUTPUT ), 5, 5, LOWORD( lParam ) - 10, HIWORD( lParam ) - 40, TRUE );
			MoveWindow( GetDlgItem( _hDlg, IDE_SECURESHELL_INPUT ), 5, HIWORD( lParam ) - 30, LOWORD( lParam ) - 95, 25, TRUE );
			MoveWindow( GetDlgItem( _hDlg, IDOK ), LOWORD( lParam ) - 85, HIWORD( lParam ) - 30, 80, 25, TRUE );
			break;
		}

		return (INT_PTR)false;
	}
}
