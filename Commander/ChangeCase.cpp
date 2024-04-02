#include "stdafx.h"

#include "Commander.h"
#include "ChangeCase.h"
#include "MiscUtils.h"

namespace Commander
{
	void CChangeCase::onInit()
	{
		_items = FCS::inst().getApp().getActivePanel().getActiveTab()->getSelectedItemsPathFull();

		CheckRadioButton( _hDlg, IDC_CASE_LOWER, IDC_CASE_MIXED, IDC_CASE_LOWER );
		CheckRadioButton( _hDlg, IDC_CASE_WHOLENAME, IDC_CASE_ONLYEXT, IDC_CASE_WHOLENAME );

		updatePreview();

		EnableWindow( GetDlgItem( _hDlg, IDC_CASE_INCLUDESUBDIRS ), FsUtils::isDirectoryInList( _items ) );
	}

	bool CChangeCase::onOk()
	{
		auto reqCase = MiscUtils::getCheckedRadio( _hDlg, { IDC_CASE_LOWER, IDC_CASE_UPPER, IDC_CASE_PARTMIXED, IDC_CASE_MIXED } );
		auto reqFile = MiscUtils::getCheckedRadio( _hDlg, { IDC_CASE_WHOLENAME, IDC_CASE_ONLYNAME, IDC_CASE_ONLYEXT } );

		_includeSubDirectories = IsDlgButtonChecked( _hDlg, IDC_CASE_INCLUDESUBDIRS ) == BST_CHECKED;

		for( const auto& item : _items )
		{
			doJob( item, reqCase, reqFile );
		}

		return true;
	}

	bool CChangeCase::doJob( const std::wstring& item, UINT reqCase, UINT reqFile )
	{
		doJobOnItem( item, reqCase, reqFile );

		if( FsUtils::isPathDirectory( item ) && _includeSubDirectories )
		{
			auto pathFind = PathUtils::addDelimiter( item );

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
							doJob( pathFind + wfd.cFileName, reqCase, reqFile );
						}
					}
					else
					{
						doJobOnItem( pathFind + wfd.cFileName, reqCase, reqFile );
					}
				} while( FindNextFile( hFile, &wfd ) );

				FindClose( hFile );
			}
		}

		return false;
	}

	void CChangeCase::doJobOnItem( const std::wstring& item, UINT reqCase, UINT reqFile )
	{
		auto newName = PathUtils::addDelimiter( PathUtils::stripFileName( item ) ) + getItemNewName( item, reqCase, reqFile );

		if( !FsUtils::renameFile( item, newName ) )
			PrintDebug( "renameFile error" );
	}

	void CChangeCase::updatePreview()
	{
		auto reqCase = MiscUtils::getCheckedRadio( _hDlg, { IDC_CASE_LOWER, IDC_CASE_UPPER, IDC_CASE_PARTMIXED, IDC_CASE_MIXED } );
		auto reqFile = MiscUtils::getCheckedRadio( _hDlg, { IDC_CASE_WHOLENAME, IDC_CASE_ONLYNAME, IDC_CASE_ONLYEXT } );

		auto preview = getItemNewName( _items[0], reqCase, reqFile );

		SetDlgItemText( _hDlg, IDC_CASE_PREVIEW, MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_CASE_PREVIEW ), preview ).c_str() );
	}

	std::wstring CChangeCase::getItemNewName( const std::wstring& item, UINT reqCase, UINT reqFile )
	{
		auto fName = PathUtils::stripFileExtension( PathUtils::stripPath( item ) );
		auto fExt = PathUtils::getFileNameExtension( item );

		switch( reqCase )
		{
		case IDC_CASE_LOWER:
			convertToLower( fName, fExt, reqFile );
			break;
		case IDC_CASE_UPPER:
			convertToUpper( fName, fExt, reqFile );
			break;
		case IDC_CASE_PARTMIXED:
			convertToPartMixed( fName, fExt, reqFile );
			break;
		case IDC_CASE_MIXED:
			convertToMixed( fName, fExt, reqFile );
			break;
		}

		return fName + ( fExt.empty() ? L"" : L"." ) + fExt;
	}

	void CChangeCase::convertToLower( std::wstring& fname, std::wstring& fext, UINT reqFile )
	{
		if( reqFile == IDC_CASE_WHOLENAME || reqFile == IDC_CASE_ONLYNAME )
			StringUtils::convert2LwrInPlace( fname );

		if( reqFile == IDC_CASE_WHOLENAME || reqFile == IDC_CASE_ONLYEXT )
			StringUtils::convert2LwrInPlace( fext );
	}

	void CChangeCase::convertToUpper( std::wstring& fname, std::wstring& fext, UINT reqFile )
	{
		if( reqFile == IDC_CASE_WHOLENAME || reqFile == IDC_CASE_ONLYNAME )
			StringUtils::convert2UprInPlace( fname );

		if( reqFile == IDC_CASE_WHOLENAME || reqFile == IDC_CASE_ONLYEXT )
			StringUtils::convert2UprInPlace( fext );
	}

	void CChangeCase::convertToPartMixed( std::wstring& fname, std::wstring& fext, UINT reqFile )
	{
		if( reqFile == IDC_CASE_WHOLENAME || reqFile == IDC_CASE_ONLYNAME )
			StringUtils::convert2MixInPlace( fname );

		if( reqFile == IDC_CASE_WHOLENAME || reqFile == IDC_CASE_ONLYEXT )
			StringUtils::convert2LwrInPlace( fext );
	}

	void CChangeCase::convertToMixed( std::wstring& fname, std::wstring& fext, UINT reqFile )
	{
		if( reqFile == IDC_CASE_WHOLENAME || reqFile == IDC_CASE_ONLYNAME )
			StringUtils::convert2MixInPlace( fname );

		if( reqFile == IDC_CASE_WHOLENAME || reqFile == IDC_CASE_ONLYEXT )
			StringUtils::convert2MixInPlace( fext );
	}

	INT_PTR CALLBACK CChangeCase::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_COMMAND:
			if( HIWORD( wParam ) == BN_CLICKED ) {
				updatePreview();
			}
			break;
		}

		return (INT_PTR)false;
	}
}
