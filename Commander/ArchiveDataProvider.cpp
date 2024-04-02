#include "stdafx.h"

#include "Commander.h"
#include "ArchiveDataProvider.h"
#include "FileUnpacker.h"
#include "FileSystemUtils.h"
#include "MiscUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CArchiveDataProvider::CArchiveDataProvider()
	{
		//_worker.init( [this] { return _openArchiveCore(); }, _hWndNotify, UM_ARCHIVOPENED );
	}

	CArchiveDataProvider::~CArchiveDataProvider()
	{
		// stop workers
		_worker.stop();
	}

	//
	// Instantiate particular archiver according to extension
	//
	//bool CArchiveDataProvider::createArchiver( const std::wstring& fileName )
	//{
	//	_upArchiver = ArchiveType::createArchiver( fileName );

	//	if( _upArchiver == nullptr )
	//	{
	//		_errorMessage = FORMAT( L"Unknown archive type: '%ls'", PathUtils::stripPath( fileName ).c_str() );
	//		return false;
	//	}

	//	return true;
	//}

	//
	// Read archive content in background worker
	//
	//bool CArchiveDataProvider::_openArchiveCore()
	//{
	//	if( FsUtils::isPathFile( _upArchiver->getName() ) )
	//		return _upArchiver->readEntries();

		//_status = EReaderStatus::Invalid;

	//	return false;
	//}

	bool CArchiveDataProvider::readData( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify )
	{
		//_worker.init( [this] { return _openArchiveCore(); }, hWndNotify, UM_READERNOTIFY );
		_readToFile = false;

		// TODO
		return false; //_worker.start();
	}

	bool CArchiveDataProvider::readToFile( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify )
	{
		_readToFile = true;

		auto tempPath = FCS::inst().getTempPath();
		_result = PathUtils::getFullPath( tempPath + PathUtils::stripPath( fileName ) );

		// file extractor sends UM_READERNOTIFY notification when done
		CBaseDialog::createModeless<CFileExtractor>( hWndNotify )->extract(
			fileName,
			tempPath,
			FCS::inst().getApp().getActivePanel().getActiveTab(),
			CArchiver::EExtractAction::Overwrite );

		return true; //_worker.start();
	}

	/*void CArchiveDataProvider::onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal )
	{
		if( msgId == UM_ARCHIVOPENED )
		{
			switch( retVal )
			{
			case FC_ARCHDONEOK:
				if( _status == EReaderStatus::ReadingData )
					_status = EReaderStatus::DataOk;
				break;
			case FC_ARCHDONEFAIL:
				_status = EReaderStatus::DataError;
				_errorMessage = _upArchiver->getErrorMessage();
				break;
			case FC_ARCHNEEDPASSWORD:
				{
					CTextInput::CParams params( L"Password", L"Enter password:", L"", true, true );
					auto res = MiscUtils::showTextInputBox( params, _hWndNotify );
					if( !res.empty() )
						wcsncpy( reinterpret_cast<wchar_t*>( workerId ), res.c_str(), MAXPASSWORD );
					else
						wcscpy( reinterpret_cast<wchar_t*>( workerId ), L"\0" );
				}
				break;
			case FC_ARCHNEEDNEXTVOLUME:
				{
					CTextInput::CParams params( L"Next Volume", L"Next volume required:", (LPCWSTR)workerId );
					auto res = MiscUtils::showTextInputBox( params, _hWndNotify );
					if( !res.empty() )
						wcsncpy( reinterpret_cast<wchar_t*>( workerId ), res.c_str(), MAX_PATH );
					else
						wcscpy( reinterpret_cast<wchar_t*>( workerId ), L"\0" );
				}
				break;
			default:
				break;
			}
		}
	}*/
}
