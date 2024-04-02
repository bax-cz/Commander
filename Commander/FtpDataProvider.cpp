#include "stdafx.h"

#include "Commander.h"
#include "FtpDataProvider.h"
#include "FtpTransfer.h"
#include "FileSystemUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CFtpDataProvider::CFtpDataProvider()
	{
		//_upFtpData = std::move( upData );

		//_root = ReaderType::getModePrefix( EReadMode::Ftp );

		//_worker.init( [this] { return _ftpReadCore(); }, _hWndNotify, UM_READERNOTIFY );
	}

	CFtpDataProvider::~CFtpDataProvider()
	{
		// stop workers
		//_worker.stop();

		//if( _upFtpData && _upFtpData->_ftpClient.IsConnected() )
		//	_upFtpData->_ftpClient.Logout();
	}

	bool CFtpDataProvider::readData( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify )
	{
		_worker.init( [this] { return _ftpReadCore(); }, hWndNotify, UM_READERNOTIFY );
		_readToFile = false;

		// TODO
		return false; //_worker.start();
	}

	bool CFtpDataProvider::readToFile( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify )
	{
		_readToFile = true;

		auto tempPath = FCS::inst().getTempPath();
		_result = tempPath + PathUtils::stripPath( fileName );

		// ftp transfer sends UM_READERNOTIFY notification when done
		CBaseDialog::createModeless<CFtpTransfer>( hWndNotify )->transferFiles(
			EFcCommand::CopyItems,
			{ fileName },
			FCS::inst().getApp().getActivePanel().getActiveTab()->getDataManager()->getMarkedEntriesSize(),
			tempPath,
			FCS::inst().getApp().getActivePanel().getActiveTab() );

		return true;
	}

	//
	// Try to connect to Ftp server using background worker
	//
	bool CFtpDataProvider::_ftpReadCore()
	{
		/*nsFTP::TFTPFileStatusShPtrVec files, filesAll;
		_upFtpData->ftpClient.List( _currentDirectory, files, _upFtpData->session.passive );
		_upFtpData->ftpClient.ListAll( _currentDirectory, filesAll, _upFtpData->session.passive );

		//_currentDirectory = _logonInfo.FwUsername() + L"@" + _logonInfo.Hostname();

		EntryData data = { 0 };

		for( auto& entry : filesAll )
		{
			// mark entries that are present in 'filesAll' but not present in 'files' as hidden
			auto pred = [&entry]( const nsFTP::TFTPFileStatusShPtr data ) { return entry->Name() == data->Name(); };
			bool isHidden = std::find_if( files.begin(), files.end(), pred ) == files.end();

			data.wfd.nFileSizeLow = data.wfd.nFileSizeHigh = 0;
			data.wfd.ftLastWriteTime = FsUtils::unixTimeToFileTime( entry->MTime() );

			data.date = FsUtils::getFormatStrFileDate( &data.wfd, false );
			data.time = FsUtils::getFormatStrFileTime( &data.wfd, false );
			wcsncpy( data.wfd.cFileName, entry->Name().c_str(), MAX_PATH );

			if( !entry->Attributes().empty() )
				data.attr = entry->Attributes();
		}*/

		return true;
	}
}
