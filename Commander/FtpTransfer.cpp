#include "stdafx.h"

#include <Uxtheme.h>

#include "Commander.h"
#include "FtpTransfer.h"

#include "MiscUtils.h"
#include "IconUtils.h"

namespace Commander
{
	CFtpTransfer::CFtpTransfer() : _retryTransfer( false )
	{
		_upFtpData = std::make_unique<CFtpReader::FtpData>();
	}

	CFtpTransfer::~CFtpTransfer()
	{
	}

	bool CFtpTransfer::processCommand( const EFcCommand& cmd )
	{
		bool retVal = false;

		switch( cmd )
		{
		case EFcCommand::RenameItem:
			retVal = _upFtpData->ftpClient.Rename( _entries[0].substr( _currentDirectory.length() ), _targetName ) == nsFTP::FTP_OK;
			break;

		case EFcCommand::MakeDir:
			retVal = _upFtpData->ftpClient.MakeDirectoryFtp( _targetName ) == nsFTP::FTP_OK;
			break;

		case EFcCommand::CopyItems:
		case EFcCommand::MoveItems:
			if( _toLocal )
				retVal = copyToLocal( cmd == EFcCommand::MoveItems );
			else
				retVal = copyToRemote( cmd == EFcCommand::MoveItems );
			break;

		case EFcCommand::RecycleItems:
		case EFcCommand::DeleteItems:
			retVal = deleteItems();
			break;
		}

		return retVal;
	}

	bool CFtpTransfer::_workerProc()
	{
		_ASSERTE( _upFtpData != nullptr );

		_upFtpData->ftpClient.AttachObserver( this );
		bool retVal = _upFtpData->ftpClient.Login( _upFtpData->session.logonInfo );

		if( retVal )
		{
			auto cwd = _currentDirectory.substr( wcslen( ReaderType::getModePrefix( EReadMode::Ftp ) ) );
			PathUtils::unifyDelimiters( cwd, false );

			if( _upFtpData->ftpClient.ChangeWorkingDirectory( cwd ) == nsFTP::FTP_OK )
			{
				do {
					// process requested command
					retVal = processCommand( _cmd );

					PrintDebug("Command: %d [%ls] (retry: %d)", _cmd, retVal ? L"OK" : L"FAIL", _retryTransfer);

					// check connection
					if( !_upFtpData->ftpClient.IsConnected() || _upFtpData->ftpClient.Noop() != nsFTP::FTP_OK )
						break;

				} while( _worker.isRunning() && !retVal && _retryTransfer );
			}
			else
			{
				PrintDebug("ChangeWorkingDirectory failed: %ls", cwd.c_str());
				retVal = false;
			}

			_upFtpData->ftpClient.Logout();
			_upFtpData->ftpClient.DetachObserver( this );
		}

		return retVal;
	}

	void CFtpTransfer::onInit()
	{
		SetWindowText( _hDlg, L"(0%) Transfer" );
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

		_worker.init( [this] { return _workerProc(); }, _hDlg, UM_WORKERNOTIFY );
	}

	bool CFtpTransfer::onOk()
	{
		// TODO: onPause button pressed
		return false;
	}

	bool CFtpTransfer::onClose()
	{
		if( _worker.isRunning() )
		{
			EnableWindow( GetDlgItem( _hDlg, IDCANCEL ), FALSE );
			_worker.sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)L"Aborting.." );
			_worker.stop_async( true );
		}

		if( _worker.isFinished() )
		{
			KillTimer( _hDlg, FC_TIMER_GUI_ID );
			show( SW_HIDE ); // hide dialog
		}

		return _worker.isFinished();
	}

	//
	// Draw progress-bar background rectangels
	//
	void CFtpTransfer::drawBackground( HDC hdc )
	{
		auto rct1 = IconUtils::getControlRect( GetDlgItem( _hDlg, IDC_PROGRESSFILE ) );
		auto rct2 = IconUtils::getControlRect( GetDlgItem( _hDlg, IDC_PROGRESSTOTAL ) );

		HGDIOBJ original = SelectObject( hdc, GetStockObject( DC_PEN ) );
		SetDCPenColor( hdc, GetSysColor( COLOR_ACTIVEBORDER ) );

		Rectangle( hdc, rct1.left-1, rct1.top-1, rct1.right+1, rct1.bottom+1 );
		Rectangle( hdc, rct2.left-1, rct2.top-1, rct2.right+1, rct2.bottom+1 );

		SelectObject( hdc, original );
	}


	void CFtpTransfer::transferFiles( EFcCommand cmd, const std::vector<std::wstring>& entries, ULONGLONG total, const std::wstring& targetName, std::shared_ptr<CPanelTab> spPanel, bool toLocal )
	{
		_spPanel = spPanel;
		_currentDirectory = spPanel->getDataManager()->getCurrentDirectory();
		_targetName = targetName;
		_cmd = cmd;
		_toLocal = toLocal;
		_entries = entries;
		_bytesTotal = total;
		_bytesProcessed = 0ull;

		// copy session data
		_upFtpData->session = spPanel->getDataManager()->getFtpSession();

		auto textFrom = MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_TEXTFROM ), _entries[0] );
		auto textTo = MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_TEXTTO ), _targetName );

		SetDlgItemText( _hDlg, IDC_TEXTFROM, textFrom.c_str() );
		SetDlgItemText( _hDlg, IDC_TEXTTO, textTo.c_str() );

		SetTimer( _hDlg, FC_TIMER_GUI_ID, FC_TIMER_GUI_TICK, NULL );

		_worker.start();
	}

	void CFtpTransfer::OnInternalError( const tstring& errorMsg, const tstring& fileName, DWORD lineNr )
	{
		if( _worker.isRunning() )
		{
			std::wostringstream sstr;
			sstr << errorMsg << L"\r\n" << L"Retry?";

			_retryTransfer = MessageBox( _hDlg, sstr.str().c_str(), L"Ftp Client Error", MB_ICONEXCLAMATION | MB_RETRYCANCEL ) != IDCANCEL;
		}
		else
			_retryTransfer = false;

		PrintDebug("Error: %ls [%ls, %d]", errorMsg.c_str(), fileName.c_str(), lineNr);
	}

	void CFtpTransfer::OnBeginReceivingData()
	{
		PrintDebug("Start");
	}

	void CFtpTransfer::OnEndReceivingData( long long llReceivedBytes )
	{
		PrintDebug("Finish, %zd bytes", llReceivedBytes);
	}

	void CFtpTransfer::OnBytesSent( const nsFTP::TByteVector& vBuffer, long lSentBytes )
	{
		if( _worker.isRunning() )
			_worker.sendNotify( FC_ARCHBYTESRELATIVE, (LPARAM)lSentBytes );
		else
			_upFtpData->ftpClient.Abort();

	//	PrintDebug("Sent bytes: %d", lSentBytes);
	}

	void CFtpTransfer::OnBytesReceived( const nsFTP::TByteVector& vBuffer, long lReceivedBytes )
	{
		if( _worker.isRunning() )
			_worker.sendNotify( FC_ARCHBYTESRELATIVE, (LPARAM)lReceivedBytes );
		else
			_upFtpData->ftpClient.Abort();

	//	PrintDebug("Received bytes: %d", lReceivedBytes);
	}

	void CFtpTransfer::OnPreSendFile( const tstring& strSourceFile, const tstring& strTargetFile, long long llFileSize )
	{
		if( _worker.isRunning() )
			_worker.sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)strSourceFile.c_str() );

		PrintDebug("File: %ls", strSourceFile.c_str());
	}

	void CFtpTransfer::OnPreReceiveFile( const tstring& strSourceFile, const tstring& strTargetFile, long long llFileSize )
	{
		if( _worker.isRunning() )
			_worker.sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)strSourceFile.c_str() );

		PrintDebug("File: %ls", strSourceFile.c_str());
	}

	void CFtpTransfer::OnPostReceiveFile( const tstring& strSourceFile, const tstring& strTargetFile, long long llFileSize )
	{
		PrintDebug("File: %ls, size: %zd", strSourceFile.c_str(), llFileSize);
	}

	void CFtpTransfer::OnPostSendFile( const tstring& strSourceFile, const tstring& strTargetFile, long long llFileSize )
	{
		PrintDebug("File: %ls, size: %zd", strSourceFile.c_str(), llFileSize);
	}

	void CFtpTransfer::OnSendCommand( const nsFTP::CCommand& command, const nsFTP::CArg& arguments )
	{
		_upCommand = std::make_unique<std::pair<nsFTP::CCommand, nsFTP::CArg>>( command, arguments );

		PrintDebug("Command sent: %ls, args: %zu", command.AsString().c_str(), arguments.size());
	}

	void CFtpTransfer::OnResponse( const nsFTP::CReply& reply )
	{
		std::wstring strCmd = L"[unknown]";

		if( _upCommand )
			strCmd = _upCommand->first.AsString();

		if( _upCommand && _upCommand->first != nsFTP::CCommand::SIZE() && reply.Code().IsNegativeReply() )
		{
			std::wostringstream sstr;

			for( auto it = _upCommand->second.begin(); it != _upCommand->second.end(); ++it )
				sstr << L"File: " << *it << L"\r\n";

			sstr << L"\r\n" << strCmd << L" Response: " << reply.Value();

			MessageBox( _hDlg, sstr.str().c_str(), L"Ftp Response", MB_ICONEXCLAMATION | MB_OK );
		}

		PrintDebug("%ls Response: %ls", strCmd.c_str(), reply.Value().c_str());
	}

	//
	// Copy to local machine - download data
	//
	bool CFtpTransfer::copyToLocal( bool move )
	{
		bool retVal = false;

		nsFTP::TFTPFileStatusShPtrVec files;
		_upFtpData->ftpClient.ListAll( L".", files, _upFtpData->session.passive );

		if( files.empty() )
			_upFtpData->ftpClient.List( L".", files, _upFtpData->session.passive );

		for( const auto& entry : _entries )
		{
			auto entryName = entry.substr( _currentDirectory.length() );

			if( CFtpReader::isEntryDirectory( CFtpReader::findEntry( files, entryName ) ) )
				retVal = copyToLocalDirectory( entryName );
			else
			{
				retVal = _upFtpData->ftpClient.DownloadFile( entryName, _targetName + entryName,
					nsFTP::CRepresentation( nsFTP::CType::Image() ), _upFtpData->session.passive );

				auto pred = [&entryName]( nsFTP::TFTPFileStatusShPtr entry ) { return ( entry->Name() == entryName ); };
				auto it = std::find_if( files.begin(), files.end(), pred );

				if( it != files.end() )
				{
					// restore file date and time
					FILETIME ft( FsUtils::unixTimeToFileTime( (*it)->MTime() ) );
					HANDLE hFile = CreateFile( PathUtils::getExtendedPath( _targetName + entryName ).c_str(), GENERIC_WRITE, NULL, NULL, OPEN_EXISTING, 0, NULL );
					SetFileTime( hFile, &ft, &ft, &ft );
					CloseHandle( hFile );
				}
			}

			if( retVal && move )
				_upFtpData->ftpClient.Delete( entryName );

			if( !_worker.isRunning() )
				break;
		}

		return retVal;
	}

	bool CFtpTransfer::copyToLocalDirectory( const std::wstring& path )
	{
		bool retVal = false;

		if( _upFtpData->ftpClient.ChangeWorkingDirectory( PathUtils::stripPath( path ) ) == nsFTP::FTP_OK )
		{
			FsUtils::makeDirectory( _targetName + path );

			nsFTP::TFTPFileStatusShPtrVec files;
			_upFtpData->ftpClient.ListAll( L".", files, _upFtpData->session.passive );

			if( files.empty() )
				_upFtpData->ftpClient.List( L".", files, _upFtpData->session.passive );

			for( const auto& entry : files )
			{
				if( CFtpReader::isEntryDirectory( entry ) )
				{
					if( !PathUtils::isDirectoryDotted( entry->Name().c_str() ) &&
						!PathUtils::isDirectoryDoubleDotted( entry->Name().c_str() ) )
						retVal = copyToLocalDirectory( PathUtils::addDelimiter( path ) + entry->Name() );
				}
				else
				{
					retVal = _upFtpData->ftpClient.DownloadFile( entry->Name(), _targetName + PathUtils::addDelimiter( path ) + entry->Name(),
						nsFTP::CRepresentation( nsFTP::CType::Image() ), _upFtpData->session.passive );

					// restore file date and time
					FILETIME ft( FsUtils::unixTimeToFileTime( entry->MTime() ) );
					HANDLE hFile = CreateFile( PathUtils::getExtendedPath( _targetName + PathUtils::addDelimiter( path ) + entry->Name() ).c_str(),
						GENERIC_WRITE, NULL, NULL, OPEN_EXISTING, 0, NULL );
					SetFileTime( hFile, &ft, &ft, &ft );
					CloseHandle( hFile );
				}

				if( !_worker.isRunning() )
					break;
			}

			_upFtpData->ftpClient.ChangeToParentDirectory();
		}

		return retVal;
	}

	//
	// Copy to remote server - upload data
	//
	bool CFtpTransfer::copyToRemote( bool move )
	{
		bool retVal = false;

		for( const auto& entry : _entries )
		{
			WIN32_FIND_DATA wfd;
			FsUtils::getFileInfo( entry, wfd );

			if( IsDir( wfd.dwFileAttributes ) )
			{
				retVal = copyToRemoteDirectory( entry );

				if( retVal && move )
					FsUtils::deleteDirectory( entry, FCS::inst().getApp().getFindFLags() );
			}
			else
			{
				retVal = _upFtpData->ftpClient.UploadFile( entry, PathUtils::stripPath( entry ), false,
					nsFTP::CRepresentation( nsFTP::CType::Image() ), _upFtpData->session.passive );

				if( retVal && move )
					DeleteFile( entry.c_str() );
			}

			if( !_worker.isRunning() )
				break;
		}

		return retVal;
	}

	bool CFtpTransfer::copyToRemoteDirectory( const std::wstring& path )
	{
		bool retVal = false;

		std::wstring dirName = PathUtils::stripPath( path );

		if( _upFtpData->ftpClient.MakeDirectoryFtp( dirName ) == nsFTP::FTP_OK &&
			_upFtpData->ftpClient.ChangeWorkingDirectory( dirName ) == nsFTP::FTP_OK )
		{
			auto pathFind = PathUtils::addDelimiter( path );

			WIN32_FIND_DATA wfd = { 0 };
			HANDLE hFile = FindFirstFileEx( ( PathUtils::getExtendedPath( pathFind ) + L"*" ).c_str(),
				FindExInfoStandard, &wfd, FindExSearchNameMatch, NULL, FCS::inst().getApp().getFindFLags() );

			if( hFile != INVALID_HANDLE_VALUE )
			{
				do
				{
					if( IsDir( wfd.dwFileAttributes ) )
					{
						if( !IsReparse( wfd.dwFileAttributes )
							&& !PathUtils::isDirectoryDotted( wfd.cFileName )
							&& !PathUtils::isDirectoryDoubleDotted( wfd.cFileName ) )
						{
							retVal = copyToRemoteDirectory( pathFind + wfd.cFileName );
						}
					}
					else
					{
						retVal = _upFtpData->ftpClient.UploadFile( pathFind + wfd.cFileName, wfd.cFileName, false,
							nsFTP::CRepresentation( nsFTP::CType::Image() ), _upFtpData->session.passive );
					}

				} while( FindNextFile( hFile, &wfd ) && _worker.isRunning() );

				FindClose( hFile );
			}

			_upFtpData->ftpClient.ChangeToParentDirectory();
		}

		return retVal;
	}

	//
	// Delete all items - files and/or directories
	//
	bool CFtpTransfer::deleteItems()
	{
		bool retVal = false;

		nsFTP::TFTPFileStatusShPtrVec files;
		_upFtpData->ftpClient.ListAll( L".", files, _upFtpData->session.passive );

		if( files.empty() )
			_upFtpData->ftpClient.List( L".", files, _upFtpData->session.passive );

		for( const auto& entry : _entries )
		{
			_worker.sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)entry.c_str() );

			auto entryName = entry.substr( _currentDirectory.length() );

			if( CFtpReader::isEntryDirectory( CFtpReader::findEntry( files, entryName ) ) )
				retVal = deleteDirectory( entryName );
			else
				retVal = _upFtpData->ftpClient.Delete( entryName ) == nsFTP::FTP_OK;

			if( !_worker.isRunning() )
				break;
		}

		return retVal;
	}

	bool CFtpTransfer::deleteDirectory( const std::wstring& dirName )
	{
		bool retVal = false;

		if( _upFtpData->ftpClient.ChangeWorkingDirectory( dirName ) == nsFTP::FTP_OK )
		{
			nsFTP::TFTPFileStatusShPtrVec files;
			_upFtpData->ftpClient.ListAll( L".", files, _upFtpData->session.passive );

			if( files.empty() )
				_upFtpData->ftpClient.List( L".", files, _upFtpData->session.passive );

			for( const auto& entry : files )
			{
				if( CFtpReader::isEntryDirectory( entry ) )
				{
					if( !PathUtils::isDirectoryDotted( entry->Name().c_str() ) &&
						!PathUtils::isDirectoryDoubleDotted( entry->Name().c_str() ) )
						retVal = deleteDirectory( entry->Name() );
				}
				else
					retVal = _upFtpData->ftpClient.Delete( entry->Name() ) == nsFTP::FTP_OK;

				if( !_worker.isRunning() )
					break;
			}

			_upFtpData->ftpClient.ChangeToParentDirectory();
			_upFtpData->ftpClient.RemoveDirectoryFtp( dirName );
		}

		return retVal;
	}

	void CFtpTransfer::updateProgressStatus()
	{
		auto textFrom = MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_TEXTFROM ), _processingPath );
		SetDlgItemText( _hDlg, IDC_TEXTFROM, textFrom.c_str() );

		double donePercent = ( (double)_bytesProcessed / (double)_bytesTotal ) * 100.0;
		donePercent = donePercent > 100.0 ? 100.0 : donePercent;
		SendDlgItemMessage( _hDlg, IDC_PROGRESSTOTAL, PBM_SETPOS, (int)donePercent, 0 );

		std::wostringstream status;
		status << FsUtils::bytesToString( _bytesProcessed ) << L" of " << FsUtils::bytesToString( _bytesTotal ) << L" transfered";
		SetDlgItemText( _hDlg, IDC_STATUSINFO, status.str().c_str() );

		std::wostringstream wndTitle;
		wndTitle << L"(" << (int)donePercent << L" %) Transfer";
		SetWindowText( _hDlg, wndTitle.str().c_str() );
	}

	INT_PTR CALLBACK CFtpTransfer::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_TIMER:
			if( wParam == FC_TIMER_GUI_ID )
			{
				// update gui controls in timer
				updateProgressStatus();
			}
			break;

		case UM_WORKERNOTIFY:
			switch( wParam )
			{
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
			case FC_ARCHDONEFAIL:
				// TODO: report failure ?
			case FC_ARCHDONEOK:
			default:
				if( wParam == FC_ARCHDONEOK ) updateProgressStatus();
				if( _spPanel->getDataManager()->isInFtpMode() ) {
					if( _cmd == EFcCommand::RenameItem || _cmd == EFcCommand::MakeDir )
						_spPanel->refresh( _targetName );
					else
						_spPanel->refresh();
				}
				if( _hDlgParent ) SendNotifyMessage( _hDlgParent, UM_READERNOTIFY, wParam, 0 );
				close();
				break;
			}
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
