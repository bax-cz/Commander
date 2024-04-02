#include "stdafx.h"

#include "Commander.h"
#include "DiskDataProvider.h"
#include "FileSystemUtils.h"
#include "NetworkUtils.h"
#include "MiscUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CDiskDataProvider::CDiskDataProvider()
	{
		//_worker.init( [this] { return _readDataCore(); }, hwndNotify, UM_READERNOTIFY );
	}

	CDiskDataProvider::~CDiskDataProvider()
	{
		// stop workers
		_worker.stop();
	}

	bool CDiskDataProvider::readData( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify )
	{
		_worker.init( [this] { return _readDataCore(); }, hWndNotify, UM_READERNOTIFY );
		_readToFile = false;

		// TODO
		return false; //_worker.start();
	}

	bool CDiskDataProvider::readToFile( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify )
	{
		_worker.init( [this] { return _readDataCore(); }, hWndNotify, UM_READERNOTIFY );
		_readToFile = true;

		_result = fileName;

		return _worker.start();
	}

	bool CDiskDataProvider::_readDataCore()
	{
		return true;
	}
}
