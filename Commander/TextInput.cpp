#include "stdafx.h"

#include "Commander.h"
#include "TextInput.h"
#include "MiscUtils.h"

namespace Commander
{
	//
	// Text input with params initialization
	//
	void CTextInput::onInit()
	{
		auto params = reinterpret_cast<CParams*>( _dlgUserDataPtr );

		if( !params->_password )
		{
			// unset edit control's password style
			SendDlgItemMessage( _hDlg, IDE_INPUTBOX_TEXT, EM_SETPASSWORDCHAR, 0, 0 );
		}

		// limit text to max long path (32000 characters)
		SendDlgItemMessage( _hDlg, IDE_INPUTBOX_TEXT, EM_SETLIMITTEXT, (WPARAM)MAX_WPATH, 0 );

		SetWindowText( _hDlg, params->_dialogTitle.c_str() );
		ShowWindow( GetDlgItem( _hDlg, IDE_INPUTBOX_TEXT ), params->_showInputBox );
		SetWindowPos( _hDlg, NULL, 0, 0, params->_dialogWidth, 140, SWP_NOZORDER | SWP_NOMOVE );
			
		const int padding = params->_dialogWidth - 40;
		SetWindowPos( GetDlgItem( _hDlg, IDC_INPUTBOX_CAPTION ), NULL, 0, 0, padding, 20, SWP_NOZORDER | SWP_NOMOVE );
		SetWindowPos( GetDlgItem( _hDlg, IDE_INPUTBOX_TEXT ), NULL, 0, 0, padding, 20, SWP_NOZORDER | SWP_NOMOVE );
		SetWindowPos( GetDlgItem( _hDlg, IDOK ), NULL, padding - 143, 67, 0, 0, SWP_NOZORDER | SWP_NOSIZE );
		SetWindowPos( GetDlgItem( _hDlg, IDCANCEL ), NULL, padding - 63, 67, 0, 0, SWP_NOZORDER | SWP_NOSIZE );

		auto caption = MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_INPUTBOX_CAPTION ), params->_dialogCaption );
		SetDlgItemText( _hDlg, IDC_INPUTBOX_CAPTION, caption.c_str() );
		SetDlgItemText( _hDlg, IDE_INPUTBOX_TEXT, params->_userText.c_str() );

		MiscUtils::centerOnWindow( params->_hParentWnd, _hDlg );
	}

	//
	// Text input with params when Ok button pressed
	//
	bool CTextInput::onOk()
	{
		MiscUtils::getWindowText( GetDlgItem( _hDlg, IDE_INPUTBOX_TEXT ), _dlgResult );

		return true;
	}

	//
	// Message handler for text input box
	//
	INT_PTR CALLBACK CTextInput::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_COMMAND:
			switch( LOWORD( wParam ) )
			{
			case IDE_INPUTBOX_TEXT:
				if( HIWORD( wParam ) == EN_CHANGE )
				{
					bool enable = GetWindowTextLength( GetDlgItem( _hDlg, IDE_INPUTBOX_TEXT ) ) > 0;
					EnableWindow( GetDlgItem( _hDlg, IDOK ), enable );
				}
				break;

			default:
				break;
			}
			break;
		}

		return (INT_PTR)false;
	}
}
