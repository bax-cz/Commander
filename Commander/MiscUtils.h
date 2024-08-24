#pragma once

/*
	Miscellaneous utils - helper functions to make life easier
*/

#include "TextInput.h"

namespace Commander
{
	struct MiscUtils
	{
		//
		// Center window on its parent
		//
		static void centerOnWindow( HWND hWndParent, HWND hWnd )
		{
			_ASSERTE( IsWindow( hWnd ) );

			// get coordinates of the window relative to its parent
			RECT rcDlg, rcArea, rcCenter;
			GetWindowRect( hWnd, &rcDlg );

			// don't center against invisible or minimized windows
			if( hWndParent != NULL )
			{
				DWORD dwAlternateStyle = GetWindowStyle( hWndParent );
				if( !( dwAlternateStyle & WS_VISIBLE ) || ( dwAlternateStyle & WS_MINIMIZE ) )
					hWndParent = NULL;
			}

			MONITORINFO mi;
			mi.cbSize = sizeof( mi );

			// center within appropriate monitor coordinates
			if( hWndParent == NULL )
			{
				HWND hwDefault = FCS::inst().getFcWindow();

				GetMonitorInfo( MonitorFromWindow( hwDefault, MONITOR_DEFAULTTOPRIMARY ), &mi );
				rcCenter = mi.rcWork;
				rcArea = mi.rcWork;
			}
			else
			{
				GetWindowRect( hWndParent, &rcCenter );
				GetMonitorInfo( MonitorFromWindow( hWndParent, MONITOR_DEFAULTTONEAREST ), &mi );
				rcArea = mi.rcWork;
			}

			// find dialog's upper left based on rcCenter
			int dlgWidth = rcDlg.right - rcDlg.left;
			int dlgHeight = rcDlg.bottom - rcDlg.top;
			int xLeft = ( rcCenter.left + rcCenter.right ) / 2 - dlgWidth / 2;
			int yTop = ( rcCenter.top + rcCenter.bottom ) / 2 - dlgHeight / 2;

			// if the dialog is outside the screen, move it inside
			if( xLeft + dlgWidth > rcArea.right )
				xLeft = rcArea.right - dlgWidth;
			if( xLeft < rcArea.left )
				xLeft = rcArea.left;

			if( yTop + dlgHeight > rcArea.bottom )
				yTop = rcArea.bottom - dlgHeight;
			if( yTop < rcArea.top )
				yTop = rcArea.top;

			// map screen coordinates to child coordinates
			SetWindowPos( hWnd, NULL, xLeft, yTop, -1, -1, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
		}

		//
		// Text input dialog box helper
		//
		static std::wstring showTextInputBox( CTextInput::CParams& params, HWND hWnd = NULL )
		{
			params.setParent( hWnd ? hWnd : FCS::inst().getFcWindow() );
			return CBaseDialog::createModal<CTextInput>( params._hParentWnd, (LPARAM)&params );
		}

		//
		// Browse for file (or folder when FOS_PICKFOLDERS specified) dialog
		//
		static bool openFileDialog( HWND hWnd, const std::wstring& initialDir, std::wstring& result, FILEOPENDIALOGOPTIONS opt = 0ul )
		{
			CComPtr<IFileOpenDialog> pDlg;
			HRESULT hr = pDlg.CoCreateInstance( __uuidof( FileOpenDialog ) );

			if( SUCCEEDED( hr ) )
			{
				FILEOPENDIALOGOPTIONS pfos;
				pDlg->GetOptions( &pfos );
				pDlg->SetOptions( pfos | opt );

				CComPtr<IShellItem> pFolder;
				hr = SHCreateItemFromParsingName( initialDir.c_str(), nullptr, IID_PPV_ARGS( &pFolder ) );
				pDlg->SetFolder( pFolder );
				pDlg->SetFileName( result.c_str() );

				// show "select file (or folder)" dialog
				hr = pDlg->Show( hWnd );

				if( SUCCEEDED( hr ) )
				{
					CComPtr<IShellItem> pItem;
					hr = pDlg->GetResult( &pItem );

					if( SUCCEEDED( hr ) )
					{
						LPOLESTR pwsz = NULL;
						hr = pItem->GetDisplayName( SIGDN_FILESYSPATH, &pwsz );

						if( SUCCEEDED( hr ) )
						{
							result = PathUtils::getFullPath( pwsz );
							CoTaskMemFree( pwsz );

							return true;
						}
					}
				}
			}

			return false;
		}

		//
		// The "Save as" file dialog
		//
		static bool saveFileDialog( HWND hWnd, const std::wstring& initialDir, std::wstring& result, FILEOPENDIALOGOPTIONS opt = 0ul )
		{
			CComPtr<IFileSaveDialog> pDlg;
			HRESULT hr = pDlg.CoCreateInstance( __uuidof( FileSaveDialog ) );

			if( SUCCEEDED( hr ) )
			{
				FILEOPENDIALOGOPTIONS pfos;
				pDlg->GetOptions( &pfos ); // get default options (includes FOS_OVERWRITEPROMPT)
				pfos |= opt;
				pDlg->SetOptions( ( opt & FOS_OVERWRITEPROMPT ? pfos : ( pfos & ~FOS_OVERWRITEPROMPT ) ) );

				CComPtr<IShellItem> pFolder;
				hr = SHCreateItemFromParsingName( initialDir.c_str(), nullptr, IID_PPV_ARGS( &pFolder ) );
				pDlg->SetFolder( pFolder );
				pDlg->SetFileName( result.c_str() );

				COMDLG_FILTERSPEC fileTypes[] = {
					{ L"All Files (*.*)", L"*.*" }
				};

				pDlg->SetFileTypes( _countof( fileTypes ), fileTypes );

				// show "save file" dialog
				hr = pDlg->Show( hWnd );

				if( SUCCEEDED( hr ) )
				{
					CComPtr<IShellItem> pItem;
					hr = pDlg->GetResult( &pItem );

					if( SUCCEEDED( hr ) )
					{
						LPOLESTR pwsz = NULL;
						hr = pItem->GetDisplayName( SIGDN_FILESYSPATH, &pwsz );

						if( SUCCEEDED( hr ) )
						{
							result = PathUtils::getFullPath( pwsz );
							CoTaskMemFree( pwsz );

							return true;
						}
					}
				}
			}

			return false;
		}

		//
		// Measure text length from given window
		//
		static int getTextExtent( HWND hw, const std::wstring& text )
		{
			HDC hdc = GetDC( hw );
			HFONT hfont = reinterpret_cast<HFONT>( SendMessage( hw, WM_GETFONT, 0, 0 ) );
			HGDIOBJ hfontOld = SelectObject( hdc, hfont );

			SIZE textLen = { 0 };
			GetTextExtentPoint32( hdc, text.c_str(), static_cast<int>( text.length() ), &textLen );

			SelectObject( hdc, hfontOld );
			ReleaseDC( hw, hdc );

			return static_cast<int>( textLen.cx );
		}

		//
		// Make compact name from path surrounded by some text
		//
		static std::wstring makeCompactPath( HWND hw, const std::wstring& path, const std::wstring& textLeft = L"", const std::wstring& textRight = L"", LONG padding = 0l )
		{
			RECT rct;
			GetClientRect( hw, &rct );

			if( rct.right )
			{
				HDC hdc = GetDC( hw );
				HFONT hfont = reinterpret_cast<HFONT>( SendMessage( hw, WM_GETFONT, 0, 0 ) );
				HGDIOBJ hfontOld = SelectObject( hdc, hfont );

				SIZE leftLen = { 0 };
				SIZE rightLen = { 0 };

				if( !textLeft.empty() )
					GetTextExtentPoint32( hdc, textLeft.c_str(), static_cast<int>( textLeft.length() ), &leftLen );

				if( !textRight.empty() )
					GetTextExtentPoint32( hdc, textRight.c_str(), static_cast<int>( textRight.length() ), &rightLen );

				TCHAR pathOut[MAX_PATH + 1] = { 0 };
				wcsncpy( pathOut, path.c_str(), MAX_PATH );

				LONG dx = rct.right - leftLen.cx - rightLen.cx - 6 - padding;
				PathCompactPath( hdc, pathOut, max( dx, 0 ) );

				SelectObject( hdc, hfontOld );
				ReleaseDC( hw, hdc );

				return textLeft + pathOut + textRight;
			}

			return L"";
		}

		//
		// Get clipboard unicode text
		//
		static std::wstring getClipboardText( HWND hWnd )
		{
			std::wstring str;

			if( OpenClipboard( hWnd ) )
			{
				HANDLE hGlobal = GetClipboardData( CF_UNICODETEXT );
				if( hGlobal )
				{
					wchar_t *pStr = static_cast<wchar_t*>( GlobalLock( hGlobal ) );
					if( pStr)
						str = pStr;

					GlobalUnlock( hGlobal );
				}

				CloseClipboard();
			}

			return str;
		}

		//
		// Set clipboard unicode text
		//
		static bool setClipboardText( HWND hWnd, const std::wstring& str )
		{
			HANDLE hGlobal = GlobalAlloc( GHND, ( str.length() + 1 ) * sizeof( wchar_t ) );
			if( hGlobal )
			{
				bool ret = false;

				wchar_t *pStr = static_cast<wchar_t*>( GlobalLock( hGlobal ) );
				if( pStr )
				{
					wcsncpy( pStr, str.c_str(), static_cast<int>( str.length() ) + 1 );

					GlobalUnlock( hGlobal );

					OpenClipboard( hWnd );
					EmptyClipboard();

					if( SetClipboardData( CF_UNICODETEXT, hGlobal ) )
						ret = true;

					CloseClipboard();
				}

				if( !ret )
					GlobalFree( hGlobal );

				return ret;
			}

			return false;
		}

		//
		// Read value from registry as string
		//
		static std::wstring readRegistryValue( HKEY hKey, const std::wstring& valName, DWORD valType, DWORD valLen )
		{
			std::wostringstream sstr;

			std::string str; str.resize( valLen );
			DWORD dwVal = 0;
			ULONGLONG ullVal = 0ull;

			switch( valType )
			{
			case REG_DWORD: // same as REG_DWORD_LITTLE_ENDIAN
				RegQueryValueEx( hKey, valName.c_str(), NULL, &valType, (BYTE*)&dwVal, &valLen );
				sstr << L"DWORD: 0x" << std::setw( 8 ) << std::setfill( L'0' ) << std::hex << std::uppercase << dwVal << std::dec << L" (" << dwVal << L")";
				break;
			case REG_DWORD_BIG_ENDIAN:
				RegQueryValueEx( hKey, valName.c_str(), NULL, &valType, (BYTE*)&dwVal, &valLen );
				dwVal = ( ( ( dwVal >> 24 ) & 0x000000FF ) | ( ( dwVal >> 8 ) & 0x0000FF00 ) | ( ( dwVal << 8 ) & 0x00FF0000 ) | ( ( dwVal << 24 ) & 0xFF000000 ) );
				sstr << L"DWORD (BE): 0x" << std::setw( 8 ) << std::setfill( L'0' ) << std::hex << std::uppercase << dwVal << std::dec << L" (" << dwVal << L")";
				break;
			case REG_RESOURCE_LIST:
				RegQueryValueEx( hKey, valName.c_str(), NULL, &valType, (BYTE*)&str[0], &valLen );
				sstr << L"RESOURCE LIST: " << StringUtils::convert2W( str );
				break;
			case REG_FULL_RESOURCE_DESCRIPTOR:
				RegQueryValueEx( hKey, valName.c_str(), NULL, &valType, (BYTE*)&str[0], &valLen );
				sstr << L"RESOURCE DESCRIPTOR: " << StringUtils::convert2W( str );
				break;
			case REG_RESOURCE_REQUIREMENTS_LIST:
				RegQueryValueEx( hKey, valName.c_str(), NULL, &valType, (BYTE*)&str[0], &valLen );
				sstr << L"RESOURCE REQUIREMENTS LIST: " << StringUtils::convert2W( str );
				break;
			case REG_QWORD: // same as REG_QWORD_LITTLE_ENDIAN
				RegQueryValueEx( hKey, valName.c_str(), NULL, &valType, (BYTE*)&ullVal, &valLen );
				sstr << L"QWORD: 0x" << std::setw( 16 ) << std::setfill( L'0' ) << std::hex << std::uppercase << ullVal << std::dec << L" (" << ullVal << L")";
				break;
			default:
				_ASSERTE( "Unsupported value type." );
				break;
			}

			return sstr.str();
		}

		//
		// Generate a random number within given interval
		//
		static int getRand( int numMin, int numMax )
		{
			int range = numMax - numMin + 1;
			int numOut = rand() % range + numMin;

			return numOut;
		}

		//
		// Calculate round value
		//
		static double round( double num )
		{
			return ( ( num > 0.0 ) ? floor( num + 0.5 ) : ceil( num - 0.5 ) );
		}

		//
		// Get checked radio button id from group of buttons
		//
		static UINT getCheckedRadio( HWND hDlg, const std::vector<UINT>& itemIds )
		{
			for( const auto& id : itemIds )
			{
				if( IsDlgButtonChecked( hDlg, id ) == BST_CHECKED )
					return id;
			}

			return -1;
		}

		//
		// Count visible lines in an edit-box
		//
		static int countVisibleLines( HWND hEdit )
		{
			RECT rct = { 0 };
			SendMessage( hEdit, EM_GETRECT, 0, (LPARAM)&rct );

			LONG_PTR numerator = 0, denominator = 0;
			SendMessage( hEdit, EM_GETZOOM, reinterpret_cast<WPARAM>( &numerator ), reinterpret_cast<LPARAM>( &denominator ) );

			// when zoom is not available (prior to Win10)
			if( numerator == 0 && denominator == 0 )
				numerator = denominator = 1;

			SIZE sizeOut = { 0 };
			HDC hdc = GetDC( hEdit );
			GetTextExtentPoint32( hdc, L"W", 1, &sizeOut );
			ReleaseDC( hEdit, hdc );

			return (int)( (double)rct.bottom / ( (double)sizeOut.cy * (double)numerator / (double)denominator ) );
		}

		//
		// Get text in an edit-box or other windows
		//
		static bool getWindowText( HWND hWnd, std::wstring& outStr )
		{
			outStr.resize( GetWindowTextLength( hWnd ) + 1 );

			GetWindowText( hWnd, &outStr[0], static_cast<int>( outStr.length() + 1 ) );
			outStr.pop_back(); // remove ending L'\0'

			return !outStr.empty();
		}

		//
		// Sanitize Hexadecimal text in an edit-box
		//
		static void sanitizeHexText( HWND hWnd, bool& fireEvent )
		{
			std::wstring text;

			if( getWindowText( hWnd, text ) )
			{
				auto outStr = StringUtils::formatHex( text );

				fireEvent = false;
				SetWindowText( hWnd, outStr.c_str() );
				fireEvent = true;

				SendMessage( hWnd, EM_SETSEL,
					static_cast<WPARAM>( outStr.length() ),
					static_cast<LPARAM>( outStr.length() ) );
			}
		}

		//
		// Get currently selected text in a single-line edit-box control
		//
		static std::wstring getEditSelectionSingle( HWND hEdit )
		{
			std::wstring text;

			DWORD idxBeg = 0, idxEnd = 0;
			SendMessage( hEdit, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );

			if( idxBeg != idxEnd )
			{
				auto startLineIdx = static_cast<int>( SendMessage( hEdit, EM_LINEFROMCHAR, idxBeg, 0 ) );
				auto idxLineInChars = static_cast<int>( SendMessage( hEdit, EM_LINEINDEX, startLineIdx, 0 ) );
				auto len = SendMessage( hEdit, EM_LINELENGTH, idxLineInChars, 0 );

				wchar_t *line = new wchar_t[len + 1];

				((WORD*)line)[0] = (WORD)len;

				auto read = SendMessage( hEdit, EM_GETLINE, startLineIdx, reinterpret_cast<LPARAM>( line ) );
				line[read] = L'\0';

				text.assign( line + idxBeg - idxLineInChars, min( idxEnd - idxBeg, len ) );

				delete[] line;
			}

			return text;
		}

		//
		// Compare two vectors' content
		//
		static bool compareValues( const std::vector<std::wstring>& vec1, const std::vector<std::wstring>& vec2 )
		{
			if( vec1.size() == vec2.size() )
			{
				for( size_t i = 0; i < vec1.size(); ++i )
				{
					if( vec1[i] != vec2[i] )
						return false;
				}

				return true;
			}

			return false;
		}

		//
		// Calculate Crc32 checksum of given file
		//
		static uLong calcCrc32( HANDLE hFile, CBackgroundWorker *pWorker = nullptr )
		{
			unsigned char dataBuf[0x8000];

			SetFilePointer( hFile, 0, NULL, FILE_BEGIN );

			uLong crc = crc32( 0L, Z_NULL, 0 );
			DWORD read = 0;

			while( ReadFile( hFile, dataBuf, sizeof( dataBuf ), &read, NULL ) && read != 0 )
			{
				crc = crc32( crc, dataBuf, read );

				if( pWorker && !pWorker->isRunning() )
					break;
			}

			return crc;
		}

		//
		// Compare two files' content
		//
		static bool compareContent( const std::wstring& fileName1, const std::wstring& fileName2, CBackgroundWorker *pWorker = nullptr )
		{
			bool retval = false;

			unsigned char dataBuf1[0x8000];
			unsigned char dataBuf2[0x8000];

			HANDLE hFile1 = CreateFile( PathUtils::getExtendedPath( fileName1 ).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
			if( hFile1 != INVALID_HANDLE_VALUE )
			{
				HANDLE hFile2 = CreateFile( PathUtils::getExtendedPath( fileName2 ).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
				if( hFile2 != INVALID_HANDLE_VALUE )
				{
					DWORD read1 = 0, read2 = 0;

					ReadFile( hFile1, dataBuf1, sizeof( dataBuf1 ), &read1, NULL );
					ReadFile( hFile2, dataBuf2, sizeof( dataBuf2 ), &read2, NULL );

					if( read1 == read2 && !memcmp( dataBuf1, dataBuf2, (size_t)read1 ) )
					{
						uLong crc1 = calcCrc32( hFile1, pWorker );
						uLong crc2 = calcCrc32( hFile2, pWorker );

						retval = ( crc1 == crc2 );
					}

					CloseHandle( hFile2 );
				}

				CloseHandle( hFile1 );
			}

			return retval;
		}

		// do not instantiate this class/struct
		MiscUtils() = delete;
		MiscUtils( MiscUtils const& ) = delete;
		void operator=( MiscUtils const& ) = delete;
	};
}
