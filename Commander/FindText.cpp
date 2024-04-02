#include "stdafx.h"

#include "Commander.h"
#include "FindText.h"
#include "MiscUtils.h"
#include "TabViewUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CFindText::CFindText() : _messageBoxShown( false ), _replaceMode( false ), _fireEvent( true )
	{
		_params = { false };
	}

	CFindText::~CFindText()
	{
	}

	//
	// Text input with params initialization
	//
	void CFindText::onInit()
	{
		TabUtils::insertItem( GetDlgItem( _hDlg, IDC_FINDTEXT_TAB ), L"Find" );

		// replace mode not available by default
		if( _dlgUserDataPtr )
			TabUtils::insertItem( GetDlgItem( _hDlg, IDC_FINDTEXT_TAB ), L"Replace" );

		// subclass all the controls just to receive an accelerator key notification :(
		SetWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_TAB ),         wndProcControlsSubclass,  0, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDE_FINDTEXT_TEXTFIND ),    wndProcControlsSubclass,  1, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDE_FINDTEXT_TEXTREPLACE ), wndProcControlsSubclass,  2, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_MATCHCASE ),   wndProcControlsSubclass,  3, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_WHOLEWORD ),   wndProcControlsSubclass,  4, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_REGEXMODE ),   wndProcControlsSubclass,  5, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_HEXMODE ),     wndProcControlsSubclass,  6, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_RBUP ),        wndProcControlsSubclass,  7, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_RBDOWN ),      wndProcControlsSubclass,  8, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_REPLACE ),     wndProcControlsSubclass,  9, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_REPLACEALL ),  wndProcControlsSubclass, 10, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDOK ),                     wndProcControlsSubclass, 11, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDCANCEL ),                 wndProcControlsSubclass, 12, reinterpret_cast<DWORD_PTR>( this ) );

		// limit text to max path
		SendDlgItemMessage( _hDlg, IDE_FINDTEXT_TEXTFIND, EM_SETLIMITTEXT, (WPARAM)MAX_WPATH, 0 );
	}

	//
	// "Find" button's been pressed
	//
	bool CFindText::onOk()
	{
		MiscUtils::getWindowText( GetDlgItem( _hDlg, IDE_FINDTEXT_TEXTFIND ), _dlgResult );

		_replaceText.clear();
		readParams();

		// inform the parent that we can start searching
		SendNotifyMessage( _hDlgParent, UM_FINDLGNOTIFY, IDE_FINDTEXT_TEXTFIND, 1 );

		return false; // do not close dialog
	}

	void CFindText::onReplace( bool replaceAll )
	{
		MiscUtils::getWindowText( GetDlgItem( _hDlg, IDE_FINDTEXT_TEXTFIND ), _dlgResult );
		MiscUtils::getWindowText( GetDlgItem( _hDlg, IDE_FINDTEXT_TEXTREPLACE ), _replaceText );

		readParams();

		// inform the parent that we can start replace
		SendNotifyMessage( _hDlgParent, UM_FINDLGNOTIFY, IDE_FINDTEXT_TEXTFIND, replaceAll ? 3 : 2 );
	}

	//
	// Text input with params when Cancel button pressed
	//
	bool CFindText::onClose()
	{
		show( SW_HIDE ); // hide dialog

		return false;
	}

	//
	// Destroy messagebox if it is shown
	//
	void CFindText::onDestroy()
	{
		// inform the parent that we are being destroyed
		SendNotifyMessage( _hDlgParent, UM_FINDLGNOTIFY, IDE_FINDTEXT_TEXTFIND, 0 );

		// remove subclass from all the controls
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_TAB ),         wndProcControlsSubclass,  0 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDE_FINDTEXT_TEXTFIND ),    wndProcControlsSubclass,  1 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDE_FINDTEXT_TEXTREPLACE ), wndProcControlsSubclass,  2 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_MATCHCASE ),   wndProcControlsSubclass,  3 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_WHOLEWORD ),   wndProcControlsSubclass,  4 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_REGEXMODE ),   wndProcControlsSubclass,  5 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_HEXMODE ),     wndProcControlsSubclass,  6 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_RBUP ),        wndProcControlsSubclass,  7 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_RBDOWN ),      wndProcControlsSubclass,  8 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_REPLACE ),     wndProcControlsSubclass,  9 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDC_FINDTEXT_REPLACEALL ),  wndProcControlsSubclass, 10 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDOK ),                     wndProcControlsSubclass, 11 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDCANCEL ),                 wndProcControlsSubclass, 12 );
	}

	void CFindText::updateText( const std::wstring& text, const std::wstring& replaceText )
	{
		SetDlgItemText( _hDlg, IDE_FINDTEXT_TEXTFIND, text.c_str() );
		SetDlgItemText( _hDlg, IDE_FINDTEXT_TEXTREPLACE, replaceText.c_str() );

		SetFocus( GetDlgItem( _hDlg, IDE_FINDTEXT_TEXTFIND ) );
		SendDlgItemMessage( _hDlg, IDE_FINDTEXT_TEXTFIND, EM_SETSEL, 0, -1 );

		_dlgResult = text;
	}

	void CFindText::updateParams( const CParams& params, bool replace )
	{
		CheckDlgButton( _hDlg, IDC_FINDTEXT_MATCHCASE, params.matchCase );
		CheckDlgButton( _hDlg, IDC_FINDTEXT_WHOLEWORD, params.wholeWords );
		CheckDlgButton( _hDlg, IDC_FINDTEXT_REGEXMODE, params.regexMode );
		CheckDlgButton( _hDlg, IDC_FINDTEXT_HEXMODE, params.hexMode );
		CheckRadioButton( _hDlg, IDC_FINDTEXT_RBUP, IDC_FINDTEXT_RBDOWN, params.reverse ? IDC_FINDTEXT_RBUP : IDC_FINDTEXT_RBDOWN );

		EnableWindow( GetDlgItem( _hDlg, IDC_FINDTEXT_HEXMODE ), !params.regexMode );
		enableHexMode( params.hexMode );

		TabCtrl_SetCurSel( GetDlgItem( _hDlg, IDC_FINDTEXT_TAB ), replace ? 1 : 0 );
		tabSelectionChanged();

		_replaceMode = replace;
		_params = params;
	}

	void CFindText::readParams()
	{
		_params.reverse    = ( MiscUtils::getCheckedRadio( _hDlg, { IDC_FINDTEXT_RBUP, IDC_FINDTEXT_RBDOWN } ) == IDC_FINDTEXT_RBUP );
		_params.matchCase  = IsDlgButtonChecked( _hDlg, IDC_FINDTEXT_MATCHCASE ) == BST_CHECKED;
		_params.wholeWords = IsDlgButtonChecked( _hDlg, IDC_FINDTEXT_WHOLEWORD ) == BST_CHECKED;
		_params.regexMode  = IsDlgButtonChecked( _hDlg, IDC_FINDTEXT_REGEXMODE ) == BST_CHECKED;
		_params.hexMode    = IsDlgButtonChecked( _hDlg, IDC_FINDTEXT_HEXMODE ) == BST_CHECKED;
	}

	void CFindText::showMessageBox( const std::wstring& text )
	{
		if( !_messageBoxShown )
		{
			_messageBoxShown = true;
			_message = StringUtils::join( { L"Find", text } );

			PostMessage( FCS::inst().getFcWindow(), UM_REPORTMSGBOX, (WPARAM)( visible() ? _hDlg : _hDlgParent ), (LPARAM)&_message[0] );
		}
		else
		{
			// try to focus the MessageBox window
			HWND hMsgBox = GetWindow( _hDlg, GW_HWNDPREV );
			if( GetWindow( hMsgBox, GW_OWNER ) == _hDlg )
				SetForegroundWindow( hMsgBox );
		}
	}

	//
	// Enable/disable UI elements when hex-mode is de/selected
	//
	void CFindText::enableHexMode( bool enable )
	{
		if( enable )
		{
			CheckDlgButton( _hDlg, IDC_FINDTEXT_MATCHCASE, BST_CHECKED );
			CheckDlgButton( _hDlg, IDC_FINDTEXT_WHOLEWORD, BST_UNCHECKED );
		}

		EnableWindow( GetDlgItem( _hDlg, IDC_FINDTEXT_MATCHCASE ), !enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_FINDTEXT_WHOLEWORD ), !enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_FINDTEXT_REGEXMODE ), !enable );
	}

	//
	// Switch between find/replace tabs
	//
	void CFindText::switchFindReplaceTabs()
	{
		if( _dlgUserDataPtr ) // only when the replace mode enabled
		{
			int curSel = TabCtrl_GetCurSel( GetDlgItem( _hDlg, IDC_FINDTEXT_TAB ) );
			TabCtrl_SetCurSel( GetDlgItem( _hDlg, IDC_FINDTEXT_TAB ), !curSel ? 1 : 0 );

			tabSelectionChanged();
		}
	}

	//
	// Update UI after find/replace tab has been changed
	//
	void CFindText::tabSelectionChanged()
	{
		_replaceMode = TabCtrl_GetCurSel( GetDlgItem( _hDlg, IDC_FINDTEXT_TAB ) ) == 1;

		ShowWindow( GetDlgItem( _hDlg, IDC_FINDTEXT_REPLACECAPTION ), _replaceMode );
		ShowWindow( GetDlgItem( _hDlg, IDE_FINDTEXT_TEXTREPLACE ), _replaceMode );
		ShowWindow( GetDlgItem( _hDlg, IDC_FINDTEXT_REPLACE ), _replaceMode );
		ShowWindow( GetDlgItem( _hDlg, IDC_FINDTEXT_REPLACEALL ), _replaceMode );

		// disable even hidden windows (they would still've accepted keyboard shortcuts)
		bool enable = GetWindowTextLength( GetDlgItem( _hDlg, IDE_FINDTEXT_TEXTFIND ) ) > 0;
		EnableWindow( GetDlgItem( _hDlg, IDC_FINDTEXT_REPLACE ), enable && _replaceMode );
		EnableWindow( GetDlgItem( _hDlg, IDC_FINDTEXT_REPLACEALL ), enable && _replaceMode );
	}

	LRESULT CALLBACK CFindText::wndProcControls( HWND hWnd, UINT message, WPARAM wp, LPARAM lp )
	{
		switch( message )
		{
		case WM_COMMAND:
			switch( LOWORD( wp ) )
			{
			case IDA_PREVTAB:
			case IDA_NEXTTAB:
				switchFindReplaceTabs();
				break;
			}
			break;

		case WM_KEYDOWN:
			switch( wp )
			{
			case 0x4E: // Ctrl + N - search next
			case 0x50: // Ctrl + P - search previous
				if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
				{
					SendNotifyMessage( _hDlgParent, UM_FINDLGNOTIFY, IDE_FINDTEXT_TEXTFIND, wp == 0x50 ? 5 : 4 );
					return 0;
				}
				break;
			case VK_F3:
				{
					// invert direction when holding shift key
					bool shift = ( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0 );
					SendNotifyMessage( _hDlgParent, UM_FINDLGNOTIFY, IDE_FINDTEXT_TEXTFIND, shift ? 5 : 4 );
					break;
				}
				break;
			}
		}

		return DefSubclassProc( hWnd, message, wp, lp );
	}

	//
	// Message handler for text input box
	//
	INT_PTR CALLBACK CFindText::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case UM_REPORTMSGBOX:
			messageBoxClosed();
			break;

		case WM_SHOWWINDOW:
			if( wParam )
				MiscUtils::centerOnWindow( GetParent( _hDlg ), _hDlg );
			break;

		case WM_COMMAND:
			switch( LOWORD( wParam ) )
			{
			case IDC_FINDTEXT_REPLACE:
				onReplace();
				break;
			case IDC_FINDTEXT_REPLACEALL:
				onReplace( true );
				break;
			case IDE_FINDTEXT_TEXTFIND:
				if( HIWORD( wParam ) == EN_CHANGE )
				{
					bool enable = GetWindowTextLength( GetDlgItem( _hDlg, IDE_FINDTEXT_TEXTFIND ) ) > 0;
					EnableWindow( GetDlgItem( _hDlg, IDOK ), enable );
					EnableWindow( GetDlgItem( _hDlg, IDC_FINDTEXT_REPLACE ), _replaceMode && enable );
					EnableWindow( GetDlgItem( _hDlg, IDC_FINDTEXT_REPLACEALL ), _replaceMode && enable );

					if( _fireEvent && IsDlgButtonChecked( _hDlg, IDC_FINDTEXT_HEXMODE ) == BST_CHECKED )
						MiscUtils::sanitizeHexText( GetDlgItem( _hDlg, IDE_FINDTEXT_TEXTFIND ), _fireEvent );
				}
				break;
			case IDC_FINDTEXT_HEXMODE:
				enableHexMode( IsDlgButtonChecked( _hDlg, IDC_FINDTEXT_HEXMODE ) == BST_CHECKED );
				break;
			case IDC_FINDTEXT_REGEXMODE:
				EnableWindow( GetDlgItem( _hDlg, IDC_FINDTEXT_HEXMODE ), IsDlgButtonChecked( _hDlg, IDC_FINDTEXT_REGEXMODE ) == BST_UNCHECKED );
				break;
			default:
				break;
			}
			break;

		case WM_NOTIFY:
			if( LOWORD( wParam ) == IDC_FINDTEXT_TAB && ((LPNMHDR)lParam)->code == TCN_SELCHANGE )
			{
				tabSelectionChanged();
			}
			break;
		}

		return (INT_PTR)false;
	}
}
