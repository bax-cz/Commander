#include "stdafx.h"

#include "Commander.h"
#include "MarkdownDataProvider.h"
#include "FileSystemUtils.h"
#include "NetworkUtils.h"
#include "MiscUtils.h"
#include "ViewerTypes.h"

#include "../discount/mkd2html.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CMarkdownDataProvider::CMarkdownDataProvider()
	{
	}

	CMarkdownDataProvider::~CMarkdownDataProvider()
	{
		// stop workers
		_worker.stop();
	}

	bool CMarkdownDataProvider::readData( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify )
	{
		_worker.init( [this] { return _readDataCore(); }, hWndNotify, UM_READERNOTIFY );
		_readToFile = false;

		// TODO

		return false; //_worker.start();
	}

	bool CMarkdownDataProvider::readToFile( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify )
	{
		_worker.init( [this] { return _readDataCore(); }, hWndNotify, UM_READERNOTIFY );

		_readToFile = true;
		_fileName = fileName;

		return _worker.start();
	}

	bool CMarkdownDataProvider::_readDataCore()
	{
		std::wstring outFileName = FCS::inst().getTempPath() + PathUtils::stripPath( _fileName );
		_result = PathUtils::stripFileExtension( outFileName ) + L".html";

		// convert .md to .html
		int ret = mkd2html( PathUtils::getExtendedPath( _fileName ).c_str(), _result.c_str() );

		switch( ret )
		{
		case 1:
			_errorMessage = L"can't open file";
			break;
		case 2:
			_errorMessage = L"can't write to output file";
			break;
		case 3:
			_errorMessage = L"can't read file";
			break;
		case 4:
			_errorMessage = L"couldn't compile input";
			break;
		}

		return !ret;
	}
}
