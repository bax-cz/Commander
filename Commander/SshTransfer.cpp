#include "stdafx.h"

#include <Uxtheme.h>

#include "Commander.h"
#include "SshTransfer.h"

#include "MiscUtils.h"
#include "IconUtils.h"

namespace Commander
{
	CSshTransfer::CSshTransfer()
	{
		_upSshData = std::make_unique<CSshReader::SshData>();
		_upSshData->shell.pWorker = &_worker;
		_upSshData->shell.FOnCaptureOutput = std::bind( &CSshTransfer::onCaptureOutput, this, std::placeholders::_1, std::placeholders::_2 );
	}

	CSshTransfer::~CSshTransfer()
	{
	}

	//
	// On error oputput
	//
	void CSshTransfer::onCaptureOutput( const std::wstring& str, bcb::TCaptureOutputType outputType )
	{
		if( outputType == bcb::TCaptureOutputType::cotError )
		{
			MessageBox( _hDlg, str.c_str(), L"PuTTY Error", MB_ICONEXCLAMATION | MB_OK );
		}
	}

	bool CSshTransfer::_workerProc()
	{
		_ASSERTE( _upSshData != nullptr );

		bool retVal = false;

		try
		{
			_upSshData->shell.FSimple = true;
			_upSshData->shell.Open();

			if( _upSshData->shell.FOpened )
			{
				retVal = true;

				auto cwd = _currentDirectory.substr( wcslen( ReaderType::getModePrefix( EReadMode::Putty ) ) );
				PathUtils::unifyDelimiters( cwd, false );

				_upSshData->shell.FUtfStrings = true;

				auto res = _upSshData->shell.SendCommandFull( bcb::fsChangeDirectory, cwd );
				auto pwd = _upSshData->shell.SendCommandFull( bcb::fsCurrentDirectory );

				switch( _cmd )
				{
				case EFcCommand::RenameItem:
					res = _upSshData->shell.SendCommandFull( bcb::fsRenameFile, _entries[0].substr( _currentDirectory.length() ), _targetName );
					break;

				case EFcCommand::MakeDir:
					res = _upSshData->shell.SendCommandFull( bcb::fsCreateDirectory, _targetName );
					break;

				case EFcCommand::CopyItems:
				case EFcCommand::MoveItems:
					if( _toLocal )
						retVal = copyToLocal( _cmd == EFcCommand::MoveItems );
					else
						retVal = copyToRemote( _cmd == EFcCommand::MoveItems );
					break;

				case EFcCommand::RecycleItems:
				case EFcCommand::DeleteItems:
					for( const auto& entry : _entries )
						res = _upSshData->shell.SendCommandFull( bcb::fsDeleteFile, entry.substr( _currentDirectory.length() ) );
					break;
				}
			}
		}
		catch( bcb::ESshFatal& e )
		{
			_errorMessage = StringUtils::convert2W( e.what() );
			return false;
		}

		return retVal;
	}

	void CSshTransfer::onInit()
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

	bool CSshTransfer::onOk()
	{
		// TODO: onPause button pressed
		return false;
	}

	bool CSshTransfer::onClose()
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
	void CSshTransfer::drawBackground( HDC hdc )
	{
		auto rct1 = IconUtils::getControlRect( GetDlgItem( _hDlg, IDC_PROGRESSFILE ) );
		auto rct2 = IconUtils::getControlRect( GetDlgItem( _hDlg, IDC_PROGRESSTOTAL ) );

		HGDIOBJ original = SelectObject( hdc, GetStockObject( DC_PEN ) );
		SetDCPenColor( hdc, GetSysColor( COLOR_ACTIVEBORDER ) );

		Rectangle( hdc, rct1.left-1, rct1.top-1, rct1.right+1, rct1.bottom+1 );
		Rectangle( hdc, rct2.left-1, rct2.top-1, rct2.right+1, rct2.bottom+1 );

		SelectObject( hdc, original );
	}


	void CSshTransfer::transferFiles( EFcCommand cmd, const std::vector<std::wstring>& entries, ULONGLONG total, const std::wstring& targetName, std::shared_ptr<CPanelTab> spPanel, bool toLocal )
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
		_upSshData->session = spPanel->getDataManager()->getSshSession();

		auto textFrom = MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_TEXTFROM ), _entries[0] );
		auto textTo = MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_TEXTTO ), _targetName );

		SetDlgItemText( _hDlg, IDC_TEXTFROM, textFrom.c_str() );
		SetDlgItemText( _hDlg, IDC_TEXTTO, textTo.c_str() );

		SetTimer( _hDlg, FC_TIMER_GUI_ID, FC_TIMER_GUI_TICK, NULL );

		_worker.start();
	}

	bool CSshTransfer::copyToRemoteFile( const std::wstring& fileName, WIN32_FIND_DATA& wfd )
	{
		_worker.sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)fileName.c_str() );

		auto entrySize = FsUtils::getFileSize( &wfd );

		unsigned char resp;
		int retVal;

		// send last file access and modification time record
		_upSshData->shell.SendLine( FORMAT( L"T%lu 0 %lu 0",
			(ULONG)FsUtils::fileTimeToUnixTime( wfd.ftLastWriteTime ),
			(ULONG)FsUtils::fileTimeToUnixTime( wfd.ftLastAccessTime ) ) );
		retVal = _upSshData->shell.Receive( &resp, sizeof( resp ) );

		// send file record
		_upSshData->shell.SendLine( FORMAT( L"C0664 %zu %ls", entrySize, PathUtils::stripPath( fileName ).c_str() ) );
		retVal = _upSshData->shell.Receive( &resp, sizeof( resp ) );

		if( resp != 0 )
		{
			auto line = _upSshData->shell.ReceiveLine();
			_upSshData->shell.DisplayBanner( line );
			return false;
		}

		FILE *f = _wfopen( PathUtils::getExtendedPath( fileName ).c_str(), L"rb" );
		size_t bytesRead = 0ull;

		while( entrySize )
		{
			bytesRead = min( (size_t)entrySize, sizeof( _dataBuf ) );

			fread( _dataBuf, bytesRead, 1, f );
			_upSshData->shell.Send( _dataBuf, bytesRead );

			_worker.sendNotify( FC_ARCHBYTESRELATIVE, bytesRead );
			entrySize -= bytesRead;
		}

		fclose( f );

		// end transfer and wait for response
		_upSshData->shell.SendNull();
		retVal = _upSshData->shell.Receive( &resp, sizeof( resp ) );

		return !resp;
	}

	bool CSshTransfer::copyToRemoteDirectory( const std::wstring& dirName, WIN32_FIND_DATA& info )
	{
		//if( !_worker.isRunning() )
		//{
		//	// try to send fatal error - abort process
		//	_upSshData->shell.SendLine( L"2Aborted by user." );
		//	return false;
		//}

		_worker.sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)dirName.c_str() );

		unsigned char resp;
		int retVal;

		// send last file access and modification time record
		_upSshData->shell.SendLine( FORMAT( L"T%lu 0 %lu 0",
			(ULONG)FsUtils::fileTimeToUnixTime( info.ftLastWriteTime ),
			(ULONG)FsUtils::fileTimeToUnixTime( info.ftLastAccessTime ) ) );
		retVal = _upSshData->shell.Receive( &resp, sizeof( resp ) );

		// send directory record
		_upSshData->shell.SendLine( FORMAT( L"D0775 0 %ls", PathUtils::stripPath( dirName ).c_str() ) );
		retVal = _upSshData->shell.Receive( &resp, sizeof( resp ) );

		auto pathFind = PathUtils::addDelimiter( dirName );

		WIN32_FIND_DATA wfd = { 0 };
		HANDLE hFile = FindFirstFileEx( ( PathUtils::getExtendedPath( pathFind ) + L"*" ).c_str(), FindExInfoStandard, &wfd, FindExSearchNameMatch, NULL, FCS::inst().getApp().getFindFLags() );
		if( hFile != INVALID_HANDLE_VALUE )
		{
			do
			{
				if( ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
				{
					if( !( wfd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT )
						&& !PathUtils::isDirectoryDotted( wfd.cFileName )
						&& !PathUtils::isDirectoryDoubleDotted( wfd.cFileName ) )
					{
						copyToRemoteDirectory( pathFind + wfd.cFileName, wfd );
					}
				}
				else
				{
					copyToRemoteFile( pathFind + wfd.cFileName, wfd );
				}

			} while( FindNextFile( hFile, &wfd ) && _worker.isRunning() );

			FindClose( hFile );
		}

		// end directory record
		_upSshData->shell.SendLine( L"E" );
		retVal = _upSshData->shell.Receive( &resp, sizeof( resp ) );

		return !resp;
	}

	//
	// Copy to remote server - upload data
	//
	bool CSshTransfer::copyToRemote( bool move )
	{
		wchar_t opt[] = L"-p";

		_upSshData->shell.SendCommand( bcb::fsCopyToRemote, opt, L"." );
		_upSshData->shell.skipFirstLine();

		unsigned char resp;
		auto ret = _upSshData->shell.Receive( &resp, sizeof( resp ) );

		PrintDebug("SCP init response: %d, retval: %d", resp, ret);

		bool retVal = false;

		for( const auto& entry : _entries )
		{
			WIN32_FIND_DATA wfd;
			FsUtils::getFileInfo( entry, wfd );

			if( IsDir( wfd.dwFileAttributes ) )
			{
				retVal = copyToRemoteDirectory( entry, wfd );

				if( retVal && move )
					FsUtils::deleteDirectory( entry, FCS::inst().getApp().getFindFLags() );
			}
			else
			{
				retVal = copyToRemoteFile( entry, wfd );

				if( retVal && move )
					DeleteFile( entry.c_str() );
			}

			if( !_worker.isRunning() )
				break;
		}

		// send EOF
		_upSshData->shell.SendSpecial( SessionSpecialCode::SS_EOF );

		int returnCode = 0;
		_upSshData->shell.ReadCommandOutput( returnCode );

		PrintDebug("SCP finish, code: %d", returnCode);

		return retVal;
	}

	bool CSshTransfer::copyToLocalFile( const std::wstring& fileName, ULONGLONG fileSize, ULONG mtime )
	{
		_worker.sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)fileName.c_str() );

		auto fileNameFull = PathUtils::getExtendedPath( _targetName + fileName );

		FILE *f = _wfopen( fileNameFull.c_str(), L"wb" );
		size_t bytesRead = 0ull;

		while( fileSize )
		{
			bytesRead = min( (size_t)fileSize, sizeof( _dataBuf ) );

			auto ret = _upSshData->shell.Receive( (BYTE*)_dataBuf, (int)bytesRead );

			fwrite( _dataBuf, bytesRead, 1, f );
			_worker.sendNotify( FC_ARCHBYTESRELATIVE, bytesRead );

			fileSize -= bytesRead;
		}

		fclose( f );

		if( mtime != 0 )
		{
			FILETIME ftime = FsUtils::unixTimeToFileTime( mtime );
			HANDLE hFile = CreateFile( fileNameFull.c_str(), GENERIC_WRITE, NULL, NULL, OPEN_EXISTING, 0, NULL );
			SetFileTime( hFile, &ftime, &ftime, &ftime );
			CloseHandle( hFile );
		}

		_upSshData->shell.SendNull();

		unsigned char resp;
		auto ret = _upSshData->shell.Receive( &resp, sizeof( resp ) );

		return !resp;
	}

	bool CSshTransfer::createLocalDirectory( const std::wstring& dirName, ULONG mtime )
	{
		FsUtils::makeDirectory( _targetName + dirName );

		if( mtime != 0 )
		{
			FILETIME ftime = FsUtils::unixTimeToFileTime( mtime );
			HANDLE hFile = CreateFile( PathUtils::getExtendedPath( _targetName + dirName ).c_str(),
				FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
			SetFileTime( hFile, &ftime, &ftime, &ftime );
			CloseHandle( hFile );
		}

		return true;
	}

	bool CSshTransfer::sink( const std::wstring& path, bool& success )
	{
		ULONG mtime = 0, atime = 0;

		while( _worker.isRunning() )
		{
			_upSshData->shell.SendNull();

			// receive scp control record
			std::wstring line = _upSshData->shell.ReceiveLine();

			if( _upSshData->shell.isLastLine( line ) )
			{
				PrintDebug("Return code: %d", _upSshData->shell.getReturnCode( line ));
				return true;
			}

			//if( !_worker.isRunning() )
			//{
			//	// try to abort process
			//	_upSshData->shell.Send( "2", 1 );
			//	return false;
			//}
			//else
				_upSshData->shell.SendNull();

			auto ctrl = line[0];
			line.erase( 0, 1 );

			switch( ctrl )
			{
			default:
			case 1:
			case 2:
				PrintDebug("Error '%d': %ls", ctrl, line.c_str());
				if( ctrl == 1 )
				{
					_upSshData->shell.DisplayBanner( line );
					continue;
				}
				else if( ctrl == 2 || ctrl != 1 )
				{
					success = false;
					_upSshData->shell.PuttyFatalError( line );
				}
				return false;

			case L'T': // read attributes and receive new control record
				if( swscanf( line.c_str(), L"%lu %*d %lu %*d", &mtime, &atime ) == 2 )
				{
					_upSshData->shell.SendNull();
					continue;
				}
				PrintDebug("Record error: T%ls", line.c_str());
				continue;

			case L'E': // end record
				_upSshData->shell.SendNull();
				PrintDebug("Dir end (%ls)", path.c_str());
				return true;

			case L'C': // file
			case L'D': // directory
				{
					auto rights = StringUtils::cutToChar( line, L' ' );
					auto fsize = StringUtils::cutToChar( line, L' ' );
					auto fileSize = std::stoull( fsize );
					StringUtils::trim( line );
					auto fileName = line;

					auto localPath = path.empty() ? L"" : PathUtils::addDelimiter( path );
					localPath += fileName;

					if( ctrl == L'D' )
					{
						PrintDebug("Dir begin (%ls)", localPath.c_str());
						success = createLocalDirectory( localPath, mtime );
						mtime = atime = 0;
						sink( localPath, success );
						continue;
					}
					else
					{
						PrintDebug("File: %ls (%zu bytes)", localPath.c_str(), fileSize);
						success = copyToLocalFile( localPath, fileSize, mtime );
						mtime = atime = 0;
					}

					if( !path.empty() )
						continue;
				}
				break;
			}

			break;
		}

		return _worker.isRunning();
	}

	//
	// Copy to local machine - download data
	//
	bool CSshTransfer::copyToLocal( bool move )
	{
		bool success = false;

		wchar_t opt[] = L"-p";

		for( const auto& entry : _entries )
		{
			auto path = entry.substr( _currentDirectory.length() );

			_worker.sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)path.c_str() );

			_upSshData->shell.SendCommand( bcb::fsCopyToLocal, opt, path );
			_upSshData->shell.skipFirstLine();

			// process scp sink
			if( !sink( L"", success ) )
				break;

			if( success && move )
				auto pwd = _upSshData->shell.SendCommandFull( bcb::fsDeleteFile, path );
		}

		return success;
	}

	void CSshTransfer::updateProgressStatus()
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

	INT_PTR CALLBACK CSshTransfer::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_TIMER:
			// update gui controls in timer
			updateProgressStatus();
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
				if( !_errorMessage.empty() ) // report failure
					MessageBox( _hDlg, _errorMessage.c_str(), L"Ssh Transfer failed", MB_ICONEXCLAMATION | MB_OK );
			case FC_ARCHDONEOK:
			default:
				if( wParam == FC_ARCHDONEOK ) updateProgressStatus();
				if( _spPanel->getDataManager()->isInPuttyMode() ) {
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
