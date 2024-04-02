#include "stdafx.h"

#include "Commander.h"
#include "ChangeAttributes.h"

namespace Commander
{
	void CChangeAttributes::onInit()
	{
		_items = FCS::inst().getApp().getActivePanel().getActiveTab()->getSelectedItemsPathFull();

		if( _items.size() == 1 )
			restoreDateTimeFromFile( _items[0] );

		restoreAttributes();

		EnableWindow( GetDlgItem( _hDlg, IDC_DATE_MODIFIED_TIME ), false );
		EnableWindow( GetDlgItem( _hDlg, IDC_DATE_CREATED_TIME  ), false );
		EnableWindow( GetDlgItem( _hDlg, IDC_DATE_ACCESSED_TIME ), false );

		DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_DATE_MODIFIED_DATE ), GDT_NONE, NULL );
		DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_DATE_CREATED_DATE  ), GDT_NONE, NULL );
		DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_DATE_ACCESSED_DATE ), GDT_NONE, NULL );

		EnableWindow( GetDlgItem( _hDlg, IDC_ATTR_INCLUDESUBDIRS ), FsUtils::isDirectoryInList( _items ) );
	}

	void CChangeAttributes::setAttributeData( UINT itemId, DWORD attrib, CAttributeData& attrData )
	{
		auto state = Button_GetCheck( GetDlgItem( _hDlg, itemId ) );

		switch( state )
		{
		case BST_CHECKED:
			attrData.dwFileAttributes |= attrib;
			break;
		case BST_INDETERMINATE:
			attrData.dwFileAttributesIgnore |= attrib;
			break;
		}
	}

	bool CChangeAttributes::onOk()
	{
		_includeSubDirectories = IsDlgButtonChecked( _hDlg, IDC_ATTR_INCLUDESUBDIRS ) == BST_CHECKED;

		CAttributeData attrData = { 0 };
		setAttributeData( IDC_ATTR_ARCHIVE,    FILE_ATTRIBUTE_ARCHIVE, attrData );
		setAttributeData( IDC_ATTR_HIDDEN,     FILE_ATTRIBUTE_HIDDEN, attrData );
		setAttributeData( IDC_ATTR_READONLY,   FILE_ATTRIBUTE_READONLY, attrData );
		setAttributeData( IDC_ATTR_SYSTEM,     FILE_ATTRIBUTE_SYSTEM, attrData );
		setAttributeData( IDC_ATTR_COMPRESSED, FILE_ATTRIBUTE_COMPRESSED, attrData );
		setAttributeData( IDC_ATTR_ENCRYPTED,  FILE_ATTRIBUTE_ENCRYPTED, attrData );

		SYSTEMTIME stUtc, stLocal;
		if( DateTime_GetSystemtime( GetDlgItem( _hDlg, IDC_DATE_MODIFIED_DATE ), &stLocal ) == GDT_VALID ) {
			TzSpecificLocalTimeToSystemTime( NULL, &stLocal, &stUtc );
			SystemTimeToFileTime( &stUtc, &attrData.ftLastWriteTime );
		}
		if( DateTime_GetSystemtime( GetDlgItem( _hDlg, IDC_DATE_CREATED_DATE  ), &stLocal ) == GDT_VALID ) {
			TzSpecificLocalTimeToSystemTime( NULL, &stLocal, &stUtc );
			SystemTimeToFileTime( &stUtc, &attrData.ftCreationTime );
		}
		if( DateTime_GetSystemtime( GetDlgItem( _hDlg, IDC_DATE_ACCESSED_DATE ), &stLocal ) == GDT_VALID ) {
			TzSpecificLocalTimeToSystemTime( NULL, &stLocal, &stUtc );
			SystemTimeToFileTime( &stUtc, &attrData.ftLastAccessTime );
		}

		for( const auto& item : _items )
		{
			doJob( item, attrData );
		}

		return true;
	}

	bool CChangeAttributes::doJob( const std::wstring& item, const CAttributeData& attrData )
	{
		doJobOnItem( item, attrData );

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
							doJob( pathFind + wfd.cFileName, attrData );
						}
					}
					else
					{
						doJobOnItem( pathFind + wfd.cFileName, attrData );
					}
				} while( FindNextFile( hFile, &wfd ) );

				FindClose( hFile );
			}
		}

		return false;
	}

	void CChangeAttributes::doJobOnItem( const std::wstring& item, const CAttributeData& attrData )
	{
		// firstly unmask all attributes that could be set, while leaving others intact
		DWORD mask = FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_COMPRESSED | FILE_ATTRIBUTE_ENCRYPTED;
		mask &= ( ~( attrData.dwFileAttributesIgnore & 0x7FFF ) );

		WIN32_FIND_DATA wfd = { 0 };
		FsUtils::getFileInfo( item, wfd );

		DWORD attrNew = ( wfd.dwFileAttributes & ( ~( mask & 0x7FFF ) ) );

		// TODO: Error reporting
		// set requested attributes (compressed and encrypted attributes will be set later)
		SetFileAttributes( PathUtils::getExtendedPath( item ).c_str(), ( attrNew | attrData.dwFileAttributes ) );

		// set file and date times - modified, created and accessed
		HANDLE hFile = CreateFile( PathUtils::getExtendedPath( item ).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
		SetFileTime( hFile, &attrData.ftCreationTime, &attrData.ftLastAccessTime, &attrData.ftLastWriteTime );
		CloseHandle( hFile );

		// un/set encrypted attribute -> encrypt/decrypt file (file must not be compressed before encrypting)
		if( !FsUtils::checkAttrib( attrData.dwFileAttributesIgnore, FILE_ATTRIBUTE_ENCRYPTED ) )
			setFileAttrEncrypted( item, attrData, wfd.dwFileAttributes );

		// un/set compressed attribute -> de/compress file
		if( !FsUtils::checkAttrib( attrData.dwFileAttributesIgnore, FILE_ATTRIBUTE_COMPRESSED ) )
			setFileAttrCompressed( item, attrData, wfd.dwFileAttributes );
	}

	void CChangeAttributes::setFileAttrEncrypted( const std::wstring& item, const CAttributeData& attrData, DWORD& attrOrig )
	{
		if( !FsUtils::checkAttrib( attrOrig, FILE_ATTRIBUTE_ENCRYPTED ) && FsUtils::checkAttrib( attrData.dwFileAttributes, FILE_ATTRIBUTE_ENCRYPTED ) )
		{
			// check whether file is not compressed, decompress first otherwise
			if( FsUtils::checkAttrib( attrOrig, FILE_ATTRIBUTE_COMPRESSED ) )
			{
				if( FsUtils::setCompressedAttrib( item.c_str(), false ) )
					attrOrig &= ( ~( attrOrig & FILE_ATTRIBUTE_COMPRESSED ) );
			}
			EncryptFile( PathUtils::getExtendedPath( item ).c_str() );
		}
		else if( FsUtils::checkAttrib( attrOrig, FILE_ATTRIBUTE_ENCRYPTED ) && !FsUtils::checkAttrib( attrData.dwFileAttributes, FILE_ATTRIBUTE_ENCRYPTED ) )
		{
			DecryptFile( PathUtils::getExtendedPath( item ).c_str(), 0 );
		}
	}

	void CChangeAttributes::setFileAttrCompressed( const std::wstring& item, const CAttributeData& attrData, DWORD& attrOrig )
	{
		// To set a file's compression state, use the DeviceIoControl function with the FSCTL_SET_COMPRESSION operation
		if( !FsUtils::checkAttrib( attrOrig, FILE_ATTRIBUTE_COMPRESSED ) && FsUtils::checkAttrib( attrData.dwFileAttributes, FILE_ATTRIBUTE_COMPRESSED ) )
		{
			FsUtils::setCompressedAttrib( item.c_str() );
		}
		else if( FsUtils::checkAttrib( attrOrig, FILE_ATTRIBUTE_COMPRESSED ) && !FsUtils::checkAttrib( attrData.dwFileAttributes, FILE_ATTRIBUTE_COMPRESSED ) )
		{
			FsUtils::setCompressedAttrib( item.c_str(), false );
		}
	}

	void CChangeAttributes::restoreDateTimeFromFile( const std::wstring& item )
	{
		WIN32_FIND_DATA wfd = { 0 };
		FsUtils::getFileInfo( item, wfd );

		SYSTEMTIME stUtc, stLocal;
		FileTimeToSystemTime( &wfd.ftLastWriteTime, &stUtc );
		SystemTimeToTzSpecificLocalTime( NULL, &stUtc, &stLocal );
		DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_DATE_MODIFIED_DATE ), GDT_VALID, &stLocal );
		DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_DATE_MODIFIED_TIME ), GDT_VALID, &stLocal );

		FileTimeToSystemTime( &wfd.ftCreationTime, &stUtc );
		SystemTimeToTzSpecificLocalTime( NULL, &stUtc, &stLocal );
		DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_DATE_CREATED_DATE  ), GDT_VALID, &stLocal );
		DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_DATE_CREATED_TIME  ), GDT_VALID, &stLocal );

		FileTimeToSystemTime( &wfd.ftLastAccessTime, &stUtc );
		SystemTimeToTzSpecificLocalTime( NULL, &stUtc, &stLocal );
		DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_DATE_ACCESSED_DATE ), GDT_VALID, &stLocal );
		DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_DATE_ACCESSED_TIME ), GDT_VALID, &stLocal );
	}

	void CChangeAttributes::restoreAttributes()
	{
		UINT attrArchive = 0, attrHidden = 0, attrReadonly = 0, attrSystem = 0, attrCompress = 0, attrEncrypt = 0;
		UINT itemCount = static_cast<UINT>( _items.size() );

		for( const auto& item : _items )
		{
			WIN32_FIND_DATA wfd = { 0 };
			FsUtils::getFileInfo( item, wfd );

			if( wfd.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE )    attrArchive++;
			if( wfd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN )     attrHidden++;
			if( wfd.dwFileAttributes & FILE_ATTRIBUTE_READONLY )   attrReadonly++;
			if( wfd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM )     attrSystem++;
			if( wfd.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED ) attrCompress++;
			if( wfd.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED )  attrEncrypt++;
		}

		Button_SetCheck( GetDlgItem( _hDlg, IDC_ATTR_ARCHIVE ),    get3State( itemCount, attrArchive ) );
		Button_SetCheck( GetDlgItem( _hDlg, IDC_ATTR_HIDDEN ),     get3State( itemCount, attrHidden ) );
		Button_SetCheck( GetDlgItem( _hDlg, IDC_ATTR_READONLY ),   get3State( itemCount, attrReadonly ) );
		Button_SetCheck( GetDlgItem( _hDlg, IDC_ATTR_SYSTEM ),     get3State( itemCount, attrSystem ) );
		Button_SetCheck( GetDlgItem( _hDlg, IDC_ATTR_COMPRESSED ), get3State( itemCount, attrCompress ) );
		Button_SetCheck( GetDlgItem( _hDlg, IDC_ATTR_ENCRYPTED ),  get3State( itemCount, attrEncrypt ) );
	}

	void CChangeAttributes::updateSetCurrentButtonStatus( bool checked )
	{
		if( !checked )
		{
			SYSTEMTIME st = { 0 };
			if( DateTime_GetSystemtime( GetDlgItem( _hDlg, IDC_DATE_MODIFIED_DATE ), &st ) == GDT_NONE &&
				DateTime_GetSystemtime( GetDlgItem( _hDlg, IDC_DATE_CREATED_DATE  ), &st ) == GDT_NONE &&
				DateTime_GetSystemtime( GetDlgItem( _hDlg, IDC_DATE_ACCESSED_DATE ), &st ) == GDT_NONE )
			{
				EnableWindow( GetDlgItem( _hDlg, IDC_DATE_SETCURRENT ), false );
			}
		}
		else
			EnableWindow( GetDlgItem( _hDlg, IDC_DATE_SETCURRENT ), true );
	}

	void CChangeAttributes::setCurrentDateTime()
	{
		SYSTEMTIME stUtc, stLocal;
		GetSystemTime( &stUtc );
		SystemTimeToTzSpecificLocalTime( NULL, &stUtc, &stLocal );

		if( DateTime_GetSystemtime( GetDlgItem( _hDlg, IDC_DATE_MODIFIED_DATE ), &stUtc ) == GDT_VALID )
		{
			DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_DATE_MODIFIED_DATE ), GDT_VALID, &stLocal );
			DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_DATE_MODIFIED_TIME ), GDT_VALID, &stLocal );
		}
		if( DateTime_GetSystemtime( GetDlgItem( _hDlg, IDC_DATE_CREATED_DATE  ), &stUtc ) == GDT_VALID )
		{
			DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_DATE_CREATED_DATE ), GDT_VALID, &stLocal );
			DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_DATE_CREATED_TIME ), GDT_VALID, &stLocal );
		}
		if( DateTime_GetSystemtime( GetDlgItem( _hDlg, IDC_DATE_ACCESSED_DATE ), &stUtc ) == GDT_VALID )
		{
			DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_DATE_ACCESSED_DATE ), GDT_VALID, &stLocal );
			DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_DATE_ACCESSED_TIME ), GDT_VALID, &stLocal );
		}
	}

	void CChangeAttributes::updateDateTimePickers( UINT dtpFirstId, UINT dtpSecondId, bool checked )
	{
		if( checked )
		{
			SYSTEMTIME stLocal;
			if( DateTime_GetSystemtime( GetDlgItem( _hDlg, dtpFirstId ), &stLocal ) == GDT_VALID )
			{
				DateTime_SetSystemtime( GetDlgItem( _hDlg, dtpSecondId ), GDT_VALID, &stLocal );
			}
		}
	}

	INT_PTR CChangeAttributes::onDateTimeChange( LPNMDATETIMECHANGE pDtChange )
	{
		bool checked = ( pDtChange->dwFlags == GDT_VALID );

		updateSetCurrentButtonStatus( checked );

		switch( pDtChange->nmhdr.idFrom )
		{
		case IDC_DATE_MODIFIED_DATE:
			updateDateTimePickers( IDC_DATE_MODIFIED_DATE, IDC_DATE_MODIFIED_TIME, checked );
			EnableWindow( GetDlgItem( _hDlg, IDC_DATE_MODIFIED_TIME ), checked );
			break;
		case IDC_DATE_MODIFIED_TIME:
			updateDateTimePickers( IDC_DATE_MODIFIED_TIME, IDC_DATE_MODIFIED_DATE, checked );
			break;
		case IDC_DATE_CREATED_DATE:
			updateDateTimePickers( IDC_DATE_CREATED_DATE, IDC_DATE_CREATED_TIME, checked );
			EnableWindow( GetDlgItem( _hDlg, IDC_DATE_CREATED_TIME ), checked );
			break;
		case IDC_DATE_CREATED_TIME:
			updateDateTimePickers( IDC_DATE_CREATED_TIME, IDC_DATE_CREATED_DATE, checked );
			break;
		case IDC_DATE_ACCESSED_DATE:
			updateDateTimePickers( IDC_DATE_ACCESSED_DATE, IDC_DATE_ACCESSED_TIME, checked );
			EnableWindow( GetDlgItem( _hDlg, IDC_DATE_ACCESSED_TIME ), checked );
			break;
		case IDC_DATE_ACCESSED_TIME:
			updateDateTimePickers( IDC_DATE_ACCESSED_TIME, IDC_DATE_ACCESSED_DATE, checked );
			break;
		}

		return (INT_PTR)false;
	}

	INT_PTR CChangeAttributes::onNotifyMessage( LPNMHDR pNmhdr )
	{
		switch( pNmhdr->code )
		{
		case DTN_DATETIMECHANGE:
			return onDateTimeChange( reinterpret_cast<LPNMDATETIMECHANGE>( pNmhdr ) );
		}

		return (INT_PTR)false;
	}

	INT_PTR CALLBACK CChangeAttributes::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_NOTIFY:
			return onNotifyMessage( reinterpret_cast<LPNMHDR>( lParam ) );

		case WM_COMMAND:
			if( HIWORD( wParam ) == BN_CLICKED )
			{
				switch( LOWORD( wParam ) )
				{
				case IDC_ATTR_COMPRESSED:
					Button_SetCheck( GetDlgItem( _hDlg, IDC_ATTR_ENCRYPTED ), BST_UNCHECKED );
					break;
				case IDC_ATTR_ENCRYPTED:
					Button_SetCheck( GetDlgItem( _hDlg, IDC_ATTR_COMPRESSED ), BST_UNCHECKED );
					break;
				case IDC_DATE_SETCURRENT:
					setCurrentDateTime();
					break;
				}
			}
			break;
		}

		return (INT_PTR)false;
	}
}
