#include "stdafx.h"

#include "Commander.h"
#include "SshDataProvider.h"
#include "SshTransfer.h"
#include "FileSystemUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CSshDataProvider::CSshDataProvider()
	{
		//_upSshData = std::move( upData );

		//_worker.init( [this] { return _sshReadCore(); }, hwndNotify, UM_READERNOTIFY );

		//_hWndNotify = hwndNotify;
		//_root = ReaderType::getModePrefix( EReadMode::Putty );

		if( _upSshData ) {
			_upSshData->shell.pWorker = &_worker;
			_upSshData->shell.FOnCaptureOutput = std::bind( &CSshDataProvider::onCaptureOutput, this, std::placeholders::_1, std::placeholders::_2 );
		}
	}

	CSshDataProvider::~CSshDataProvider()
	{
		// stop workers
		_worker.stop();

		if( _upSshData && _upSshData->shell.FOpened )
			_upSshData->shell.Close();
	}

	void CSshDataProvider::onCaptureOutput( const std::wstring& str, bcb::TCaptureOutputType outputType )
	{
		if( outputType == bcb::TCaptureOutputType::cotError )
		{
			_errorMessage = str;
		}
	}

	bool CSshDataProvider::readData( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify )
	{
		_worker.init( [this] { return _sshReadCore(); }, hWndNotify, UM_READERNOTIFY );
		_readToFile = false;

		// TODO
		return false; //_worker.start();
	}

	bool CSshDataProvider::readToFile( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify )
	{
		_readToFile = true;

		auto tempPath = FCS::inst().getTempPath();
		_result = tempPath + PathUtils::stripPath( fileName );

		// ssh transfer sends UM_READERNOTIFY notification when done
		CBaseDialog::createModeless<CSshTransfer>( hWndNotify )->transferFiles(
			EFcCommand::CopyItems,
			{ fileName },
			FCS::inst().getApp().getActivePanel().getActiveTab()->getDataManager()->getMarkedEntriesSize(),
			tempPath,
			FCS::inst().getApp().getActivePanel().getActiveTab() );

		return true; //_worker.start();
	}

	//
	// Connect to ssh server in background worker
	//
	bool CSshDataProvider::_sshReadCore()
	{
		try
		{
			static wchar_t *nationalVars[] = { L"LANG", L"LANGUAGE", L"LC_CTYPE", L"LC_COLLATE", L"LC_MONETARY", L"LC_NUMERIC", L"LC_TIME", L"LC_MESSAGES", L"LC_ALL", L"HUMAN_BLOCKS" };
			static wchar_t *lsLinux = L"ls -la --time-style=+\"%Y-%m-%d %H:%M:%S\"";
			static wchar_t *lsFrBsd = L"ls -laD \"%Y-%m-%d %H:%M:%S\"";
			static wchar_t *lsMacos = L"ls -laT";
			static wchar_t *lsDeflt = L"ls -la";

			if( _isFirstRun )
			{
				_isFirstRun = false;
				_upSshData->shell.FUtfStrings = true;

				// detect system language (utf-8 set by default)
				auto lang = _upSshData->shell.SendCommandFull( bcb::fsLang );
			}
		}
		catch( bcb::ESshFatal& e )
		{
			_errorMessage = StringUtils::convert2W( e.what() );
			return false;
		}

		return _errorMessage.empty();
	}

}
