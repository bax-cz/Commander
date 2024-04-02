#include "stdafx.h"

#include "Commander.h"
#include "CompareDirectories.h"
#include "TextFileReader.h"
#include "MiscUtils.h"

#define isFlag( flags, pos ) ( ( ( flags ) & ( 1 << ( pos ) ) ) != 0 )

namespace Commander
{
	CCompareDirectories::CCompareDirectories()
		: _dirsLeft( FCS::inst().getApp().getPanelLeft().getActiveTab()->getDataManager()->getDirEntries() )
		, _filesLeft( FCS::inst().getApp().getPanelLeft().getActiveTab()->getDataManager()->getFileEntries() )
		, _dirsRight( FCS::inst().getApp().getPanelRight().getActiveTab()->getDataManager()->getDirEntries() )
		, _filesRight( FCS::inst().getApp().getPanelRight().getActiveTab()->getDataManager()->getFileEntries() )
		, _path1( FCS::inst().getApp().getPanelLeft().getActiveTab()->getDataManager()->getCurrentDirectory() )
		, _path2( FCS::inst().getApp().getPanelRight().getActiveTab()->getDataManager()->getCurrentDirectory() )
		, _flags( 0 )
	{
	}

	CCompareDirectories::~CCompareDirectories()
	{
	}

	void CCompareDirectories::onInit()
	{
		Button_SetCheck( GetDlgItem( _hDlg, IDC_CMPDIR_NAME ), BST_CHECKED );

		updateGroupStatus();

		_worker.init( [this] { return _workerProc(); }, _hDlg, UM_WORKERNOTIFY );
	}

	bool CCompareDirectories::onOk()
	{
		_flags = 0;

		_flags |= Button_GetCheck( GetDlgItem( _hDlg, IDC_CMPDIR_NAME     ) ) ? ( 1 << 0 ) : 0;
		_flags |= Button_GetCheck( GetDlgItem( _hDlg, IDC_CMPDIR_SIZE     ) ) ? ( 1 << 1 ) : 0;
		_flags |= Button_GetCheck( GetDlgItem( _hDlg, IDC_CMPDIR_DATE     ) ) ? ( 1 << 2 ) : 0;
		_flags |= Button_GetCheck( GetDlgItem( _hDlg, IDC_CMPDIR_ATTR     ) ) ? ( 1 << 3 ) : 0;
		_flags |= Button_GetCheck( GetDlgItem( _hDlg, IDC_CMPDIR_CONTENT  ) ) ? ( 1 << 4 ) : 0;
		_flags |= Button_GetCheck( GetDlgItem( _hDlg, IDC_CMPDIR_SUBDIRS  ) ) ? ( 1 << 5 ) : 0;
		_flags |= Button_GetCheck( GetDlgItem( _hDlg, IDC_CMPDIR_IGNOREOL ) ) ? ( 1 << 6 ) : 0;

		updateGroupStatus( FALSE );

		_worker.start();

		return false; // do not close dialog
	}

	bool CCompareDirectories::onClose()
	{
		_worker.stop();

		return true;
	}

	bool getEntriesList( const std::wstring& dirName, std::map<std::wstring, WIN32_FIND_DATA>& dirs, std::map<std::wstring, WIN32_FIND_DATA>& files, CBackgroundWorker *pWorker )
	{
		WIN32_FIND_DATA wfd = { 0 };

		HANDLE hFile = FindFirstFileEx( ( PathUtils::getExtendedPath( dirName + L"\\*" ) ).c_str(), FindExInfoStandard, &wfd, FindExSearchNameMatch, NULL, FCS::inst().getApp().getFindFLags() );

		// get sorted files and directories lists in std::map
		if( hFile != INVALID_HANDLE_VALUE )
		{
			do
			{
				if( ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
				{
					if( !PathUtils::isDirectoryDotted( wfd.cFileName ) && !PathUtils::isDirectoryDoubleDotted( wfd.cFileName ) )
						dirs[wfd.cFileName] = wfd;
				}
				else
					files[wfd.cFileName] = wfd;

			} while( FindNextFile( hFile, &wfd ) && pWorker->isRunning() );

			FindClose( hFile );

			return true;
		}

		return false;
	}

	// TODO: report files that cannot be opened
	bool CCompareDirectories::compareFilesContents( const std::wstring& fileName1, const std::wstring& fileName2 )
	{
		if( isFlag( _flags, 6 ) ) // ignore EOL and encoding for text files
		{
			// open files as binary streams
			std::ifstream fs1( fileName1.c_str(), std::ifstream::binary );
			std::ifstream fs2( fileName2.c_str(), std::ifstream::binary );

			if( fs1.is_open() && fs2.is_open() )
			{
				CTextFileReader reader1( fs1, &_worker );
				CTextFileReader reader2( fs2, &_worker );

				// check whether both files are a text files
				if( reader1.isText() && reader2.isText() )
				{
					DWORD ret1 = ERROR_SUCCESS, ret2 = 0;

					while( _worker.isRunning() && ret1 == ERROR_SUCCESS )
					{
						ret1 = reader1.readLine();
						ret2 = reader2.readLine();

						// compare files line by line
						if( ret1 != ret2 || reader1.getLineRef() != reader2.getLineRef() )
							return false;
					}

					return true;
				}
				else if( reader1.isText() || reader2.isText() )
				{
					// one of the files is not a text file
					return false;
				}
			}
		}

		return MiscUtils::compareContent( fileName1, fileName2, &_worker );
	}

	bool CCompareDirectories::traverseSubDirectories( const std::wstring& dirName1, const std::wstring& dirName2 )
	{
		auto path1 = _path1 + dirName1;
		auto path2 = _path2 + dirName2;

		// get sorted directories lists in std::map
		std::map<std::wstring, WIN32_FIND_DATA> dirsList1, filesList1;
		std::map<std::wstring, WIN32_FIND_DATA> dirsList2, filesList2;

		if( !getEntriesList( path1, dirsList1, filesList1, &_worker ) || !getEntriesList( path2, dirsList2, filesList2, &_worker ) )
			return false;

		auto pred = []( const auto& a, const auto& b ) { return a.first == b.first; };

		// compare file and dir names
		if( dirsList1.size() != dirsList2.size() || !std::equal( dirsList1.begin(), dirsList1.end(), dirsList2.begin(), pred ) )
			return false;

		if( filesList1.size() != filesList2.size() || !std::equal( filesList1.begin(), filesList1.end(), filesList2.begin(), pred ) )
			return false;

		// compare size and content of files
		for( auto it = filesList1.begin(); it != filesList1.end() && _worker.isRunning(); ++it )
		{
			const auto& wfd = filesList2[it->first];

			// compare size only when the "ignore EOL/Encoding" is not checked
			if( ( isFlag( _flags, 1 ) || !isFlag( _flags, 6 ) ) && ( it->second.nFileSizeLow != wfd.nFileSizeLow || it->second.nFileSizeHigh != wfd.nFileSizeHigh ) )
				return false;

			if( !compareFilesContents( PathUtils::addDelimiter( path1 ) + it->second.cFileName, PathUtils::addDelimiter( path2 ) + wfd.cFileName ) )
				return false;
		}

		// recursive traverse dirs
		for( const auto& dirName : dirsList1 )
		{
			if( !traverseSubDirectories( PathUtils::addDelimiter( dirName1 ) + dirName.first, PathUtils::addDelimiter( dirName2 ) + dirName.first ) )
				return false;
		}

		return true;
	}

	bool CCompareDirectories::calculateDirectorySize( const std::wstring& dirName, ULONGLONG *dirSize )
	{
		// exclude reparse points from calculation
		if( FsUtils::checkPathAttrib( dirName, FILE_ATTRIBUTE_REPARSE_POINT ) )
			return false;

		WIN32_FIND_DATA wfd = { 0 };

		// read file system structure
		HANDLE hFile = FindFirstFileEx( ( PathUtils::getExtendedPath( dirName + L"\\*" ) ).c_str(), FindExInfoStandard, &wfd, FindExSearchNameMatch, NULL, FCS::inst().getApp().getFindFLags() );
		if( hFile != INVALID_HANDLE_VALUE )
		{
			do
			{
				if( ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
				{
					if( !PathUtils::isDirectoryDotted( wfd.cFileName )
					 && !PathUtils::isDirectoryDoubleDotted( wfd.cFileName ) )
					{
						calculateDirectorySize( PathUtils::addDelimiter( dirName ) + wfd.cFileName, dirSize );
					}
				}
				else
				{
					*dirSize += FsUtils::getFileSize( &wfd );
				}
			} while( FindNextFile( hFile, &wfd ) && _worker.isRunning() );

			FindClose( hFile );
		}

		return true;
	}

	bool CCompareDirectories::compareDirectoriesSize( const std::wstring& dirName1, const std::wstring& dirName2 )
	{
		ULONGLONG size1 = 0ull, size2 = 0ull;

		calculateDirectorySize( _path1 + dirName1, &size1 );
		calculateDirectorySize( _path2 + dirName2, &size2 );

		return size1 == size2;
	}

	bool CCompareDirectories::compareDirs()
	{
		// skip double-dotted directory when there is one
		size_t offset = ( !_dirsLeft.empty() && PathUtils::isDirectoryDoubleDotted( _dirsLeft[0]->wfd.cFileName ) ? 1 : 0 );

		for( auto it = _dirsLeft.begin() + offset; it != _dirsLeft.end() && _worker.isRunning(); ++it )
		{
			auto pred = [this,&it]( const std::shared_ptr<EntryData>& ed ) {
				if( PathUtils::isDirectoryDoubleDotted( ed->wfd.cFileName ) )
					return false;
				if( isFlag( _flags, 0 ) && wcsicmp( (*it)->wfd.cFileName, ed->wfd.cFileName ) )
					return false;
				if( ( isFlag( _flags, 1 ) || ( !isFlag( _flags, 6 ) && isFlag( _flags, 4 ) ) ) && !compareDirectoriesSize( (*it)->wfd.cFileName, ed->wfd.cFileName ) )
					return false;
				if( isFlag( _flags, 2 ) && memcmp( &(*it)->wfd.ftLastWriteTime, &ed->wfd.ftLastWriteTime, sizeof( FILETIME ) ) )
					return false;
				if( isFlag( _flags, 3 ) && (*it)->wfd.dwFileAttributes != ed->wfd.dwFileAttributes )
					return false;
				if( isFlag( _flags, 4 ) && ( !traverseSubDirectories( (*it)->wfd.cFileName, ed->wfd.cFileName ) ) )
					return false;

				return true;
			};

			auto it2 = std::find_if( _dirsRight.begin(), _dirsRight.end(), pred );

			if( it2 != _dirsRight.end() )
			{
				_entriesLeft.insert( static_cast<int>( it - _dirsLeft.begin() ) );
				_entriesRight.insert( static_cast<int>( it2 - _dirsRight.begin() ) );
			}
		}

		return !_entriesLeft.empty() || !_entriesRight.empty();
	}

	bool CCompareDirectories::compareFiles()
	{
		for( auto it = _filesLeft.begin(); it != _filesLeft.end() && _worker.isRunning(); ++it )
		{
			auto pred = [this,&it]( const std::shared_ptr<EntryData>& ed ) {
				if( isFlag( _flags, 0 ) && wcsicmp( (*it)->wfd.cFileName, ed->wfd.cFileName ) )
					return false;
				if( ( isFlag( _flags, 1 ) || ( !isFlag( _flags, 6 ) && isFlag( _flags, 4 ) ) ) && ( (*it)->wfd.nFileSizeLow != ed->wfd.nFileSizeLow || (*it)->wfd.nFileSizeHigh != ed->wfd.nFileSizeHigh ) )
					return false;
				if( isFlag( _flags, 2 ) && memcmp( &(*it)->wfd.ftLastWriteTime, &ed->wfd.ftLastWriteTime, sizeof( FILETIME ) ) )
					return false;
				if( isFlag( _flags, 3 ) && (*it)->wfd.dwFileAttributes != ed->wfd.dwFileAttributes )
					return false;
				if( isFlag( _flags, 4 ) && !compareFilesContents( _path1 + (*it)->wfd.cFileName, _path2 + ed->wfd.cFileName ) )
					return false;

				return true;
			};

			auto it2 = std::find_if( _filesRight.begin(), _filesRight.end(), pred );

			if( it2 != _filesRight.end() )
			{
				_entriesLeft.insert( static_cast<int>( it - _filesLeft.begin() + _dirsLeft.size() ) );
				_entriesRight.insert( static_cast<int>( it2 - _filesRight.begin() + _dirsRight.size() ) );
			}
		}

		return !_entriesLeft.empty() || !_entriesRight.empty();
	}

	//
	// Look for duplicates in two lists
	//
	bool CCompareDirectories::_workerProc()
	{
		bool ret = compareFiles();

		if( isFlag( _flags, 5 ) )
			ret = compareDirs() || ret;

		return ret;
	}

	void CCompareDirectories::updateGroupStatus( BOOL enable )
	{
		EnableWindow( GetDlgItem( _hDlg, IDC_CMPDIR_NAME     ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_CMPDIR_SIZE     ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_CMPDIR_DATE     ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_CMPDIR_ATTR     ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_CMPDIR_CONTENT  ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_CMPDIR_IGNOREOL ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDOK ), enable );

		// check whether there are any directories to compare
		bool cmpDirs = _dirsLeft.empty() || ( _dirsLeft.size() == 1 && PathUtils::isDirectoryDoubleDotted( _dirsLeft[0]->wfd.cFileName ) )
					|| _dirsRight.empty() || ( _dirsRight.size() == 1 && PathUtils::isDirectoryDoubleDotted( _dirsRight[0]->wfd.cFileName ) );

		EnableWindow( GetDlgItem( _hDlg, IDC_CMPDIR_SUBDIRS ), !cmpDirs && enable );

		// check whether the user wants to compare text files while ignoring EOL/Encoding
		EnableWindow( GetDlgItem( _hDlg, IDC_CMPDIR_IGNOREOL ), enable && IsDlgButtonChecked( _hDlg, IDC_CMPDIR_CONTENT ) == BST_CHECKED );
	}

	void CCompareDirectories::updateOkButtonStatus()
	{
		int status = 0;

		status += Button_GetCheck( GetDlgItem( _hDlg, IDC_CMPDIR_NAME ) );
		status += Button_GetCheck( GetDlgItem( _hDlg, IDC_CMPDIR_SIZE ) );
		status += Button_GetCheck( GetDlgItem( _hDlg, IDC_CMPDIR_DATE ) );
		status += Button_GetCheck( GetDlgItem( _hDlg, IDC_CMPDIR_ATTR ) );
		status += Button_GetCheck( GetDlgItem( _hDlg, IDC_CMPDIR_CONTENT ) );

		EnableWindow( GetDlgItem( _hDlg, IDOK ), !!status && !_worker.isRunning() );
	}

	INT_PTR CALLBACK CCompareDirectories::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case UM_WORKERNOTIFY:
			if( wParam == 1 )
			{
				FCS::inst().getApp().getPanelLeft().getActiveTab()->markEntries( { _entriesLeft.begin(), _entriesLeft.end() } );
				FCS::inst().getApp().getPanelRight().getActiveTab()->markEntries( { _entriesRight.begin(), _entriesRight.end() } );

				close();
			}
			else
			{
				MessageBox( _hDlg, L"No same files were found.", L"Compare Directories", MB_ICONINFORMATION | MB_OK );

				updateGroupStatus();
			}
			break;

		case WM_COMMAND:
			if( HIWORD( wParam ) == BN_CLICKED )
			{
				updateGroupStatus();
				updateOkButtonStatus();
			}
			break;
		}

		return (INT_PTR)false;
	}
}
