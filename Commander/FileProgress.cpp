#include "stdafx.h"

#include <Uxtheme.h>

#include "Commander.h"
#include "FileProgress.h"

#include "MiscUtils.h"
#include "IconUtils.h"

namespace Commander
{
	/*bool CFileProgress::workerProc()
	{
		//_ASSERTE( _upArchiver != nullptr );
		return false;
	}*/

	void CFileProgress::onInit()
	{
		//_worker.init( [this] { return workerProc(); }, _hDlg, UM_STATUSNOTIFY );

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

	bool CFileProgress::onOk()
	{
		pauseFastCopy();

		return false;
	}

	bool CFileProgress::onClose()
	{
		//_worker.stop();

		_bSuspended = !!_fastCopy.Suspend();
		KillTimer( _hDlg, FC_TIMER_GUI_ID );

		if( MessageBox( _hDlg, L"Really cancel?", L"Cancel operation", MB_YESNO | MB_ICONQUESTION ) == IDNO )
		{
			SetTimer( _hDlg, FC_TIMER_GUI_ID, FC_TIMER_GUI_TICK, NULL );
			_bSuspended = !_fastCopy.Resume();
		}
		else
			stopFastCopy();

		return false;
	}

	void CFileProgress::onDestroy()
	{
		FCS::inst().getFastCopyManager().onDestroyed( this );
	}

	//
	// Initiate fastcopy process
	//
	void CFileProgress::runJob( EFcCommand cmd, std::vector<std::wstring> items, std::wstring dirTo )
	{
		std::wstring wndTitle, captionFrom;
		_fastCopyMode = FCS::inst().getFastCopyManager().getModeName( cmd, wndTitle, captionFrom );

		auto captionTo = _fastCopyMode != FastCopy::DELETE_MODE ? L"To" : L"";
		auto textFrom = MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_TEXTFROM ), items[0] );
		auto textTo = MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_TEXTTO ), dirTo );

		wndTitle = L"(0 %) " + wndTitle;
		SetWindowText( _hDlg, wndTitle.c_str() );

		SetDlgItemText( _hDlg, IDC_CAPTIONFROM, captionFrom.c_str() );
		SetDlgItemText( _hDlg, IDC_CAPTIONTO, captionTo );
		SetDlgItemText( _hDlg, IDC_TEXTFROM, textFrom.c_str() );
		SetDlgItemText( _hDlg, IDC_TEXTTO, textTo.c_str() );

		_entries = items;
		_command = cmd;
		_targetDir = dirTo;

		_bSuspended = false;
		_bytesProcessed = 0ull;
		_calcTimes = 0;
		_lastTotalSec = 0;
		_doneRatePercent = 0;

		initFastCopy();

		auto ret = _fastCopy.Start( &_ti );
		SetTimer( _hDlg, FC_TIMER_GUI_ID, FC_TIMER_GUI_TICK, NULL );
	}

	//
	// FastCopy internal initialization
	//
	void CFileProgress::initFastCopy()
	{
		FastCopy::Info *fcpi = &_fastCopyInfo;
		ZeroMemory( fcpi, sizeof( FastCopy::Info ) );

		// "fcpi->isRenameMode" is output parameter -> do not set
		fcpi->ignoreEvent    = 0; // FASTCOPY_ERROR_EVENT | FASTCOPY_STOP_EVENT
		fcpi->mode           = _fastCopyMode;
		fcpi->overWrite      = FastCopy::OverWrite::BY_NAME;
		fcpi->dlsvtMode      = 0;
		fcpi->timeDiffGrace  = 0; // 1ms -> 100ns
		fcpi->fileLogFlags   = 0;
		fcpi->debugFlags     = 0;
		fcpi->bufSize        = 128 * 1024 * 1024;
		fcpi->maxOpenFiles   = 256;
		fcpi->maxOvlSize     = 1 * 1024 * 1024;
		fcpi->maxOvlNum      = 4;
		fcpi->maxAttrSize    = 128 * 1024 * 1024;
		fcpi->maxDirSize     = 128 * 1024 * 1024;
		fcpi->maxMoveSize    = 16 * 1024 * 1024;
		fcpi->maxDigestSize  = 16 * 1024 * 1024;
		fcpi->minSectorSize  = 0;
		fcpi->nbMinSizeNtfs  = 64 * 1024;
		fcpi->nbMinSizeFat   = 128 * 1024;
		fcpi->maxLinkHash    = 300000;
		fcpi->allowContFsize = 1024 * 1024 * 1024;
		fcpi->hNotifyWnd     = _hDlg;
		fcpi->uNotifyMsg     = UM_FASTCOPYINFO;
		fcpi->lcid           = GetThreadLocale();
		fcpi->fromDateFilter = 0;
		fcpi->toDateFilter   = 0;
		fcpi->minSizeFilter  = -1;
		fcpi->maxSizeFilter  = -1;
		//fcpi->driveMap;         // physical drive map
		fcpi->maxRunNum      = 0;
		fcpi->netDrvMode     = 0;
		fcpi->aclReset       = 0;

		fcpi->verifyFlags    = 0;
		fcpi->flags          = 0//FastCopy::WITH_ACL | FastCopy::WITH_ALTSTREAM
							// | FastCopy::REPORT_ACL_ERROR | FastCopy::REPORT_STREAM_ERROR
							 | FastCopy::PRE_SEARCH;//FastCopy::VERIFY_FILE

		if( fcpi->mode != FastCopy::MOVE_MODE && fcpi->mode != FastCopy::DELETE_MODE )
			fcpi->flags |= FastCopy::REPARSE_AS_NORMAL;

		if (fcpi->mode == FastCopy::DELETE_MODE)
			fcpi->flags &= ~FastCopy::PRE_SEARCH;

		PathArray srcArray, dstArray, incArray, excArray;
		srcArray.RegisterMultiPath( StringUtils::join( _entries ).c_str(), L"|" );
		dstArray.RegisterPath( _targetDir.c_str() );

		_fastCopy.RegisterInfo( &srcArray, &dstArray, fcpi, &incArray, &excArray );
		//_fastCopy.TakeExclusivePriv();
	}

	//
	// Pause button pressed
	//
	void CFileProgress::pauseFastCopy()
	{
		if( _bSuspended )
			_bSuspended = !_fastCopy.Resume();
		else
			_bSuspended = !!_fastCopy.Suspend();

		if( _bSuspended )
		{
			TCHAR wndTitleTemp[256];
			GetWindowText( _hDlg, wndTitleTemp, ARRAYSIZE( wndTitleTemp ) );

			std::wstring wndTitle = std::wstring( wndTitleTemp ) + L" - Paused";
			SetWindowText( _hDlg, wndTitle.c_str() );

			SetDlgItemText( _hDlg, IDOK, L"Resume" );
		}
		else
		{
			SetDlgItemText( _hDlg, IDOK, L"Pause" );
		}
	}

	//
	// Stop FastCopy process copy/move/delete
	//
	void CFileProgress::stopFastCopy()
	{
		show( SW_HIDE ); // hide dialog

		_fastCopy.Suspend();

		KillTimer( _hDlg, FC_TIMER_GUI_ID );

		_fastCopy.Resume();
		_fastCopy.Aborting();

		if( _fastCopy.IsStarting() )
			_fastCopy.End();

		destroy();
	}

	//
	// Draw progress-bar background rectangels
	//
	void CFileProgress::drawBackground( HDC hdc )
	{
		auto rct1 = IconUtils::getControlRect( GetDlgItem( _hDlg, IDC_PROGRESSFILE ) );
		auto rct2 = IconUtils::getControlRect( GetDlgItem( _hDlg, IDC_PROGRESSTOTAL ) );

		HGDIOBJ original = SelectObject( hdc, GetStockObject( DC_PEN ) );
		SetDCPenColor( hdc, GetSysColor( COLOR_ACTIVEBORDER ) );

		Rectangle( hdc, rct1.left-1, rct1.top-1, rct1.right+1, rct1.bottom+1 );
		Rectangle( hdc, rct2.left-1, rct2.top-1, rct2.right+1, rct2.bottom+1 );

		SelectObject( hdc, original );
	}

	//
	// Update dialog progress status
	//
	void CFileProgress::updateProgressStatus()
	{
		if( _fastCopy.IsStarting() && !_fastCopy.IsAborting() && !_bSuspended )
		{
			TransInfo *ti = &_ti;
			_fastCopy.GetTransInfo( ti );

			if( ti->fullTickCount == 0 )
			{
				ti->fullTickCount++;
			}

			int remain_sec, total_sec, remain_h, remain_m, remain_s;
			double doneRate;
			//if( ( _fastCopyInfo.flags & FastCopy::PRE_SEARCH) && !ti->total.isPreSearch )
			//{
				calcInfoTotal( &doneRate, &remain_sec, &total_sec );
				remain_h = remain_sec / 3600;
				remain_m = (remain_sec % 3600) / 60;
				remain_s = remain_sec % 60;
				_doneRatePercent = (int)(MiscUtils::round( doneRate * 100.0 ) );
				_doneRatePercent = ( _doneRatePercent > 100 ) ? 100 : _doneRatePercent;
			//}

			std::wstring title, caption;
			FastCopy::Mode mode = CFastCopyManager::getModeName( _command, title, caption );
			std::wostringstream wndTitle;
			wndTitle << L"(" << _doneRatePercent << L" %) " << title;

			std::wostringstream status;
			status << FsUtils::bytesToString( ti->total.writeTrans ) << L" of " << FsUtils::bytesToString( ti->preTotal.readTrans );
			status << ( mode == FastCopy::DELETE_MODE ? L" deleted" : ( mode == FastCopy::MOVE_MODE ? L" moved" : L" copied" ) );
			status << L", speed: " << FsUtils::bytesToString( ti->total.writeTrans / ti->fullTickCount / 1024 * 1000 * 1000 ) << L"/s";
			status << L", time left: ";
			if( remain_h ) status << remain_h << L" hour ";
			if( remain_m ) status << remain_m << L" min ";
			status << remain_s << L" sec" ;

			SetWindowText( _hDlg, wndTitle.str().c_str() );
			SetDlgItemText( _hDlg, IDC_TEXTTO, MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_TEXTTO ), ti->curPath ).c_str() );
			SendDlgItemMessage( _hDlg, IDC_PROGRESSFILE, PBM_SETPOS, calcInfoPerFile(), 0 );
			SendDlgItemMessage( _hDlg, IDC_PROGRESSTOTAL, PBM_SETPOS, _doneRatePercent, 0 );
			SetDlgItemText( _hDlg, IDC_STATUSINFO, status.str().c_str() );

			FCS::inst().getFastCopyManager().reportProgress( _doneRatePercent );
		}
	}

	//
	// Check if listing only
	//
	bool CFileProgress::isListingOnly()
	{
		return ( _fastCopyInfo.flags & FastCopy::LISTING_ONLY ) ? true : false;
	}

	//
	// ETA calculation helper
	//
	double CFileProgress::sizeToCoef( double ave_size, int64 files )
	{
		if( files <= 2 )
			return	0.0;

		if( ave_size < AVE_WATERMARK_MIN )
			return 2.0;

		double ret = AVE_WATERMARK_MID / ave_size;
		if( ret >= 2.0 )
			return 2.0;

		if( ret <= 0.001 )
			return 0.001;

		return ret;
	}

	//
	// Calculate total ETA time
	//
	void CFileProgress::calcInfoTotal( double *doneRate, int *remain_sec, int *total_sec )
	{
		int64	preFiles;
		int64	doneFiles;
		double	realDoneRate;
		auto	&pre = _ti.preTotal;
		auto	&cur = _ti.total;

		_calcTimes++;

		preFiles = pre.readFiles + pre.readDirs +
			pre.writeFiles + pre.writeDirs +
			pre.deleteFiles + pre.deleteDirs +
			pre.verifyFiles +
			(pre.skipFiles + pre.skipDirs) / 2000 +
			pre.allErrFiles + pre.allErrDirs;

		doneFiles = cur.readFiles + cur.readDirs +
			cur.writeFiles + cur.writeDirs +
			cur.deleteFiles + cur.deleteDirs +
			cur.verifyFiles +
			(cur.skipFiles + cur.skipDirs) / 2000 +
			cur.allErrFiles + cur.allErrDirs;

		if (_fastCopyInfo.mode == FastCopy::DELETE_MODE) {
			realDoneRate = *doneRate = doneFiles / (preFiles + 0.01);
			_lastTotalSec = (int)(_ti.execTickCount / realDoneRate / 1000);
		}
		else {
			double verifyCoef = 0.9;
			int64 preTrans =
				(_ti.isSameDrv ? pre.readTrans : 0) +
				pre.writeTrans +
				//					pre.deleteTrans + 
				int64(pre.verifyTrans * verifyCoef) +
				//					pre.skipTrans + 
				pre.allErrTrans;

			int64 doneTrans =
				(_ti.isSameDrv ? cur.readTrans : 0) +
				cur.writeTrans +
				//					cur.deleteTrans + 
				int64(cur.verifyTrans * verifyCoef) +
				//					cur.skipTrans + 
				cur.allErrTrans;

			if ((_fastCopyInfo.verifyFlags & FastCopy::VERIFY_FILE)) {
				//			preFiles  += pre.writeFiles;
				//			doneFiles += cur.verifyFiles;

				//			preTrans  += pre.writeTrans;
				//			doneTrans += cur.verifyTrans;
			}

			//Debug("F: %5d(%4d/%4d) / %5d  T: %7d / %7d   \n", int(doneFiles), int(cur.writeFiles),
			//	int(cur.readFiles), int(preFiles), int(doneTrans / 1024), int(preTrans / 1024));

			if (doneFiles > preFiles) preFiles = doneFiles;
			if (doneTrans > preTrans) preTrans = doneTrans;

			double doneTransRate = doneTrans / (preTrans + 0.01);
			double doneFileRate = doneFiles / (preFiles + 0.01);
			double aveSize = preTrans / (preFiles + 0.01);
			double coef = sizeToCoef(aveSize, preFiles);

			*doneRate = (doneFileRate * coef + doneTransRate) / (coef + 1);
			realDoneRate = *doneRate;

			if (realDoneRate < 0.0001) {
				realDoneRate = 0.0001;
			}

			_lastTotalSec = (int)(_ti.execTickCount / realDoneRate / 1000);

			static int prevTotalSec;
			if (_calcTimes >= 5) {
				int diff = _lastTotalSec - prevTotalSec;
				int remain = _lastTotalSec - (_ti.execTickCount / 1000);
				int diff_ratio = (remain >= 30) ? 8 : (remain >= 10) ? 4 : (remain >= 5) ? 2 : 1;
				_lastTotalSec = prevTotalSec + (diff / diff_ratio);
			}
			prevTotalSec = _lastTotalSec;

			//Debug("recalc(%d) %.2f sec=%d coef=%.2f pre=%lld\n",
			//	(realDoneRate < 0.70 ? 10 : (11 - int(realDoneRate * 10))),
			//	doneTransRate, _lastTotalSec, coef, preFiles);
		}

		*total_sec = _lastTotalSec;
		if ((*remain_sec = *total_sec - (_ti.execTickCount / 1000)) < 0) {
			*remain_sec = 0;
		}
	}

	//
	// Calculate progress per file
	//
	int CFileProgress::calcInfoPerFile()
	{
		TransInfo *ti = &_ti;

		int doneRate = 0;
		static int fileCnt = ti->total.writeFiles;
		if( fileCnt == ti->total.writeFiles )
		{
			/*WIN32_FIND_DATA wfd;
			FsUtils::getFileInfo( ti->curPath, wfd );
			auto fileSize = FsUtils::getFileSize( &wfd );
			auto fileTrans = (ULONGLONG)( ti->total.readTrans - ti->total.writeTrans );
			if( fileTrans < fileSize )
			{
				double diff = (double)( fileSize - fileTrans );
				double size = (double)fileSize;
				doneRate = (int)( size / 100.0 * diff );

				PrintDebug( "size:%lf, diff:%lf (%d)", size, diff, doneRate );
			}
			else*/
				doneRate = 100;
		}
		else
			fileCnt = ti->total.writeFiles;

		return doneRate;
	}

	//
	// Handle info message from fast copy
	//
	INT_PTR CFileProgress::onFastCopyInfo( WPARAM message, FastCopy::Confirm *confirm )
	{
		switch( message )
		{
		case FastCopy::END_NOTIFY:
			updateProgressStatus(); // TODO - test
			stopFastCopy();
			break;

		case FastCopy::CONFIRM_NOTIFY:
			{
				std::wostringstream sstr;
				if( confirm->path )
				{
					sstr << L"Path: \"" << confirm->path << L"\"\r\n";
					sstr << SysUtils::getErrorMessage( confirm->err_code );
				}
				else sstr << confirm->message;
	
				int res = MessageBox( _hDlg, sstr.str().c_str(), L"I/O Error",
					( confirm->allow_continue ? MB_ABORTRETRYIGNORE : MB_RETRYCANCEL ) | MB_ICONEXCLAMATION );

				switch( res ) {
				case IDIGNORE:
					confirm->result = FastCopy::Confirm::IGNORE_RESULT;
					break;
				case IDABORT:
				case IDCANCEL:
					confirm->result = FastCopy::Confirm::CANCEL_RESULT;
					break;
				default:
					confirm->result = FastCopy::Confirm::CONTINUE_RESULT;
					break;
				}
			}
			break;

		case FastCopy::RENAME_NOTIFY:
			PrintDebug( "RENAME_NOTIFY" );
			break;

		case FastCopy::LISTING_NOTIFY:
			PrintDebug( "LISTING_NOTIFY" );
			//if( isListingOnly() ) setListInfo();
			//else setFileLogInfo();
			break;
		}

		return (INT_PTR)TRUE;
	}


	INT_PTR CALLBACK CFileProgress::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_COMMAND:
			switch( LOWORD( wParam ) )
			{
			case IDC_MINIMIZE:
				show( SW_MINIMIZE );
				break;
			}
			break;

		case UM_FASTCOPYINFO:
			return onFastCopyInfo( wParam, (FastCopy::Confirm*)lParam );

		case WM_TIMER:
			updateProgressStatus();
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
