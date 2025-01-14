#include "stdafx.h"

#include <Uxtheme.h>

#include "Commander.h"
#include "FileUnpacker.h"

#include "MiscUtils.h"
#include "IconUtils.h"

namespace Commander
{
	bool CFileUnpacker::_workerProc()
	{
		_ASSERTE( _upArchiver != nullptr );

		ShellUtils::CComInitializer _com( COINIT_APARTMENTTHREADED );

		// repack entries using 7-zip archiver
		std::wstring arch = L".7z";
		std::unique_ptr<CArchiver> upRePacker;
		std::wstring tmpDir = FsUtils::getNextFileName( FCS::inst().getTempPath(), L"$pack" );
		tmpDir = FCS::inst().getTempPath() + PathUtils::addDelimiter( tmpDir );

		// extract the first archive
		bool ret = _upArchiver->extractEntries( _entries, _repack ? tmpDir : _targetDir );

		if( _repack )
		{
			auto archName = _targetDir + PathUtils::stripFileExtension( PathUtils::stripPath( _archiveNames[0] ) ) + arch;

			upRePacker = ArchiveType::createArchiver( arch );
			upRePacker->init( archName, nullptr, nullptr, &_worker );
			upRePacker->packEntries( FsUtils::getEntriesList( tmpDir, FCS::inst().getApp().getFindFLags() ), archName );

			FsUtils::deleteDirectory( tmpDir, FCS::inst().getApp().getFindFLags() );
		}

		// extract others when there is more of them
		if( _archiveNames.size() > 1 )
		{
			for( auto it = _archiveNames.begin() + 1; it != _archiveNames.end(); ++it )
			{
				_worker.sendMessage( FC_ARCHPROCESSINGVOLUME, reinterpret_cast<LPARAM>( it->c_str() ) );

				auto upArchiver = ArchiveType::createArchiver( *it );
				upArchiver->init( *it, nullptr, nullptr, &_worker, _action );

				ret = ret && upArchiver->extractEntries( { *it }, _repack ? tmpDir : _targetDir );

				if( _repack )
				{
					upRePacker->packEntries( FsUtils::getEntriesList( tmpDir, FCS::inst().getApp().getFindFLags() ),
						_targetDir + PathUtils::stripFileExtension( PathUtils::stripPath( *it ) ) + arch );

					FsUtils::deleteDirectory( tmpDir, FCS::inst().getApp().getFindFLags() );
				}

				if( !_worker.isRunning() )
					break;
			}
		}

		return ret;
	}

	void CFileUnpacker::onInit()
	{
		_repack = false;

		_worker.init( [this] { return _workerProc(); }, _hDlg, UM_STATUSNOTIFY );

		SetWindowText( _hDlg, L"(0%) Extract" );
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

	bool CFileUnpacker::onOk()
	{
		// TODO: onPause button pressed
		return false;
	}

	bool CFileUnpacker::onClose()
	{
		show( SW_HIDE ); // hide dialog

		_worker.stop();

		KillTimer( _hDlg, FC_TIMER_GUI_ID );

		return true;//MessageBox( NULL, L"Really exit?", L"Question", MB_YESNO | MB_ICONQUESTION ) == IDYES;
	}

	//
	// Draw progress-bar background rectangels
	//
	void CFileUnpacker::drawBackground( HDC hdc )
	{
		auto rct1 = IconUtils::getControlRect( GetDlgItem( _hDlg, IDC_PROGRESSFILE ) );
		auto rct2 = IconUtils::getControlRect( GetDlgItem( _hDlg, IDC_PROGRESSTOTAL ) );

		HGDIOBJ original = SelectObject( hdc, GetStockObject( DC_PEN ) );
		SetDCPenColor( hdc, GetSysColor( COLOR_ACTIVEBORDER ) );

		Rectangle( hdc, rct1.left-1, rct1.top-1, rct1.right+1, rct1.bottom+1 );
		Rectangle( hdc, rct2.left-1, rct2.top-1, rct2.right+1, rct2.bottom+1 );

		SelectObject( hdc, original );
	}

	void CFileUnpacker::unpack( const std::vector<std::wstring>& items, const std::wstring& targetDir, std::shared_ptr<CPanelTab> spPanel, CArchiver::EExtractAction action )
	{
		_archiveNames = items;
		_targetDir = targetDir;
		_action = action;
		_bytesTotal = spPanel->getDataManager()->getMarkedEntriesSize();
		_bytesProcessed = 0ull;
		_entries.push_back( items[0] );

		extractCore();
	}

	void CFileUnpacker::unpack( const std::wstring& fileName, const std::wstring& targetDir, std::shared_ptr<CPanelTab> spPanel, CArchiver::EExtractAction action )
	{
		_targetDir = targetDir;
		_action = action;
		_bytesTotal = spPanel->getDataManager()->getMarkedEntriesSize();
		_bytesProcessed = 0ull;

		if( fileName.empty() )
			_entries = spPanel->getSelectedItemsPathFull();
		else
			_entries.push_back( fileName );

		_archiveNames.push_back( spPanel->getDataManager()->isInArchiveMode() ? spPanel->getDataManager()->getRootPath() : _entries[0] );

		extractCore();
	}

	void CFileUnpacker::unpack( const std::wstring& archiveName, const std::wstring& localPath, const std::wstring& targetDir, CArchiver::EExtractAction action )
	{
		_archiveNames.push_back( archiveName );
		_targetDir = targetDir;
		_action = action;
		_bytesTotal = 0ull; // TODO: calculating would slow the whole process down (.tar archives especially)
		_bytesProcessed = 0ull;

		_entries.push_back( PathUtils::addDelimiter( archiveName ) + localPath );

		extractCore();
	}

	void CFileUnpacker::repack( const std::vector<std::wstring>& items, const std::wstring& targetDir, std::shared_ptr<CPanelTab> spPanel )
	{
		_repack = true;

		_archiveNames = items;
		_targetDir = targetDir;
		_action = CArchiver::EExtractAction::Overwrite;
		_bytesTotal = spPanel->getDataManager()->getMarkedEntriesSize();
		_bytesProcessed = 0ull;
		_entries.push_back( items[0] );

		extractCore();
	}

	void CFileUnpacker::extractCore()
	{
		_ASSERTE( !_archiveNames.empty() );

		_upArchiver = ArchiveType::createArchiver( _archiveNames[0] );
		_upArchiver->init( _archiveNames[0], nullptr, nullptr, &_worker, _action );

		_processingVolume = _archiveNames[0];

		auto textFrom = MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_TEXTFROM ), _entries[0] );
		auto textTo = MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_TEXTTO ), _targetDir );

		SetDlgItemText( _hDlg, IDC_TEXTFROM, textFrom.c_str() );
		SetDlgItemText( _hDlg, IDC_TEXTTO, textTo.c_str() );

		SetTimer( _hDlg, FC_TIMER_GUI_ID, FC_TIMER_GUI_TICK, NULL );

		_worker.start();
	}

	void CFileUnpacker::updateProgressStatus()
	{
		auto textFrom = MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_TEXTFROM ), _processingPath );
		SetDlgItemText( _hDlg, IDC_TEXTFROM, textFrom.c_str() );

		double donePercent = ( (double)_bytesProcessed / (double)_bytesTotal ) * 100.0;
		donePercent = donePercent > 100.0 ? 100.0 : donePercent;
		SendDlgItemMessage( _hDlg, IDC_PROGRESSTOTAL, PBM_SETPOS, (int)donePercent, 0 );

		std::wostringstream status;
		status << FsUtils::bytesToString( _bytesProcessed ) << L" of " << FsUtils::bytesToString( _bytesTotal ) << L" extracted";
		SetDlgItemText( _hDlg, IDC_STATUSINFO, status.str().c_str() );

		std::wostringstream wndTitle;
		wndTitle << L"(" << (int)donePercent << L" %) Extracting " << PathUtils::stripPath( _processingVolume );
		SetWindowText( _hDlg, wndTitle.str().c_str() );
	}

	INT_PTR CALLBACK CFileUnpacker::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
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
				if( !res.empty() )
					wcsncpy( reinterpret_cast<wchar_t*>( lParam ), res.c_str(), MAXPASSWORD ); // TODO: MAXPASSWORD is RAR specific
				else
					wcscpy( reinterpret_cast<wchar_t*>( lParam ), L"\0" );
				break;
			}
			case FC_ARCHNEEDNEWNAME:
			{
				CTextInput::CParams params( L"File Exists", L"Enter new name or skip:", (LPCWSTR)lParam );
				auto res = MiscUtils::showTextInputBox( params, _hDlg );
				if( !res.empty() )
					wcsncpy( reinterpret_cast<wchar_t*>( lParam ), res.c_str(), MAX_PATH );
				else
					wcscpy( reinterpret_cast<wchar_t*>( lParam ), L"\0" );
				break;
			}
			case FC_ARCHPROCESSINGPATH:
			{
				_processingPath = (LPCWSTR)lParam;
				break;
			}
			case FC_ARCHBYTESRELATIVE: // add relative
			{
				_bytesProcessed += (ULONGLONG)lParam;
				break;
			}
			case FC_ARCHBYTESABSOLUTE: // set absolute
			{
				_bytesProcessed = (ULONGLONG)lParam;
				break;
			}
			case FC_ARCHBYTESTOTAL: // set total
			{
				_bytesTotal = (ULONGLONG)lParam;
				break;
			}
			case FC_ARCHNEEDNEXTVOLUME:
			{
				CTextInput::CParams params( L"Next Volume", L"Next volume required:", (LPCWSTR)lParam );
				auto res = MiscUtils::showTextInputBox( params, _hDlg );
				if( !res.empty() )
					wcsncpy( reinterpret_cast<wchar_t*>( lParam ), res.c_str(), MAX_PATH ); // TODO: in RAR it's 2048 wchars
				else
					wcscpy( reinterpret_cast<wchar_t*>( lParam ), L"\0" );
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
				MessageBox( NULL, sstr.str().c_str(), L"Error Extracting File", MB_ICONEXCLAMATION | MB_OK );
				// fall-through
			}
			case FC_ARCHDONEOK:
			default:
				if( wParam == FC_ARCHDONEOK ) updateProgressStatus();
				if( _hDlgParent ) SendNotifyMessage( _hDlgParent, UM_READERNOTIFY, wParam, 0 );
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
