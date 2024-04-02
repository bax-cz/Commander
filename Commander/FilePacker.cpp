#include "stdafx.h"

#include <Uxtheme.h>

#include "Commander.h"
#include "FilePacker.h"

#include "MiscUtils.h"
#include "IconUtils.h"

namespace Commander
{
	bool CFilePacker::_workerProc()
	{
		_ASSERTE( _upArchiver != nullptr );
		return _upArchiver->packEntries( _entries, _targetName );
	}

	void CFilePacker::onInit()
	{
		_worker.init( [this] { return _workerProc(); }, _hDlg, UM_STATUSNOTIFY );

		SetWindowText( _hDlg, L"(0%) Pack" );
		SendDlgItemMessage( _hDlg, IDC_PROGRESSFILE, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
		SendDlgItemMessage( _hDlg, IDC_PROGRESSTOTAL, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
		SendDlgItemMessage( _hDlg, IDC_PROGRESSFILE, PBM_SETPOS, 0, 0 );
		SendDlgItemMessage( _hDlg, IDC_PROGRESSTOTAL, PBM_SETPOS, 0, 0 );

		// disable themes to be able to change progress-bar color
		SetWindowTheme( GetDlgItem( _hDlg, IDC_PROGRESSFILE ), L" ", L" " );
		SetWindowTheme( GetDlgItem( _hDlg, IDC_PROGRESSTOTAL ), L" ", L" " );
		SendDlgItemMessage( _hDlg, IDC_PROGRESSFILE, PBM_SETBARCOLOR, 0, FC_COLOR_PROGBAR );
		SendDlgItemMessage( _hDlg, IDC_PROGRESSTOTAL, PBM_SETBARCOLOR, 0, FC_COLOR_PROGBAR );

		MiscUtils::centerOnWindow( FCS::inst().getFcWindow(), _hDlg );

		show(); // show dialog
	}

	bool CFilePacker::onClose()
	{
		show( SW_HIDE ); // hide dialog

		_worker.stop();

		KillTimer( _hDlg, FC_TIMER_GUI_ID );

		return true;//MessageBox( NULL, L"Really exit?", L"Question", MB_YESNO | MB_ICONQUESTION ) == IDYES;
	}

	//
	// Draw progress-bar background rectangels
	//
	void CFilePacker::drawBackground( HDC hdc )
	{
		auto rct1 = IconUtils::getControlRect( GetDlgItem( _hDlg, IDC_PROGRESSFILE ) );
		auto rct2 = IconUtils::getControlRect( GetDlgItem( _hDlg, IDC_PROGRESSTOTAL ) );

		HGDIOBJ original = SelectObject( hdc, GetStockObject( DC_PEN ) );
		SetDCPenColor( hdc, GetSysColor( COLOR_ACTIVEBORDER ) );

		Rectangle( hdc, rct1.left-1, rct1.top-1, rct1.right+1, rct1.bottom+1 );
		Rectangle( hdc, rct2.left-1, rct2.top-1, rct2.right+1, rct2.bottom+1 );

		SelectObject( hdc, original );
	}


	void CFilePacker::packFiles( const std::wstring& targetName, CPanelTab *pPanel )
	{
		_targetName = PathUtils::getFullPath( targetName );
		_processingVolume = _targetName;
		_entries = pPanel->getSelectedItemsPathFull();
		_bytesTotal = pPanel->getDataManager()->getMarkedEntriesSize();
		_bytesProcessed = 0ull;

		_upArchiver = ArchiveType::createArchiver( _targetName );

		if( _upArchiver )
		{
			_upArchiver->init( _targetName, nullptr, nullptr, &_worker );

			auto textFrom = MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_TEXTFROM ), _entries[0] );
			auto textTo = MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_TEXTTO ), _targetName );

			SetDlgItemText( _hDlg, IDC_TEXTFROM, textFrom.c_str() );
			SetDlgItemText( _hDlg, IDC_TEXTTO, textTo.c_str() );

			SetTimer( _hDlg, FC_TIMER_GUI_ID, FC_TIMER_GUI_TICK, NULL );

			_worker.start();
		}
		else
		{
			MessageBox( NULL, L"Invalid archiver name.", L"Error Packing Files", MB_ICONEXCLAMATION | MB_OK );

			close();
		}
	}

	void CFilePacker::updateProgressStatus()
	{
		auto textFrom = MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_TEXTFROM ), _processingPath );
		SetDlgItemText( _hDlg, IDC_TEXTFROM, textFrom.c_str() );

		double donePercent = ( (double)_bytesProcessed / (double)_bytesTotal ) * 100.0;
		donePercent = donePercent > 100.0 ? 100.0 : donePercent;
		SendDlgItemMessage( _hDlg, IDC_PROGRESSTOTAL, PBM_SETPOS, (int)donePercent, 0 );

		std::wostringstream status;
		status << FsUtils::bytesToString( _bytesProcessed ) << L" of " << FsUtils::bytesToString( _bytesTotal ) << L" packed";
		SetDlgItemText( _hDlg, IDC_STATUSINFO, status.str().c_str() );

		std::wostringstream wndTitle;
		wndTitle << L"(" << (int)donePercent << L" %) Packing " << PathUtils::stripPath( _processingVolume );
		SetWindowText( _hDlg, wndTitle.str().c_str() );
	}

	INT_PTR CALLBACK CFilePacker::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_TIMER:
			// update gui controls in timer
			updateProgressStatus();
			break;

		case UM_STATUSNOTIFY:
			switch( wParam )
			{
			case FC_ARCHNEEDPASSWORD:
			{
				CTextInput::CParams params( L"Password", L"Enter password:", L"", true, true );
				auto res = MiscUtils::showTextInputBox( params, _hDlg );
				wcsncpy( reinterpret_cast<wchar_t*>( lParam ), res.c_str(), MAXPASSWORD ); // TODO: MAXPASSWORD is RAR specific
				break;
			}
			case FC_ARCHPROCESSINGPATH:
			{
				_processingPath = (LPCWSTR)lParam;
				break;
			}
			case FC_ARCHBYTESRELATIVE:
			{
				_bytesProcessed += (ULONGLONG)lParam;
				break;
			}
			case FC_ARCHBYTESABSOLUTE:
			{
				_bytesProcessed = (ULONGLONG)lParam;
				break;
			}
			case FC_ARCHBYTESTOTAL:
			{
				_bytesTotal = (ULONGLONG)lParam;
				break;
			}
			case FC_ARCHNEEDNEXTVOLUME:
			{
				CTextInput::CParams params( L"Next Volume", L"Next volume required:", (TCHAR*)lParam );
				auto res = MiscUtils::showTextInputBox( params, _hDlg );
				break;
			}
			case FC_ARCHPROCESSINGVOLUME:
			{
				_processingVolume = reinterpret_cast<LPCWSTR>( lParam );
				break;
			}
			case FC_ARCHDONEFAIL:
			{
				std::wostringstream sstr;
				sstr << _upArchiver->getErrorMessage();
				MessageBox( NULL, sstr.str().c_str(), L"Error Packing Files", MB_ICONEXCLAMATION | MB_OK );
				// fall-through
			}
			case FC_ARCHDONEOK:
			default:
				updateProgressStatus();
				close();
				break;
			}

		case WM_COMMAND:
		//	if( HIWORD( wParam ) == EN_SETFOCUS )
		//	{
		//		HideCaret( reinterpret_cast<HWND>( lParam ) );
		//	}
			break;

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint( _hDlg, &ps );
			drawBackground( hdc );
			EndPaint( _hDlg, &ps );
			break;
		}
		}

		return (INT_PTR)false;
	}
}
