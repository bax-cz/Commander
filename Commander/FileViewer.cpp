#include "stdafx.h"

#include "Commander.h"
#include "FileViewer.h"
#include "TextFileReader.h"
#include "FileUnpacker.h"
#include "FtpTransfer.h"
#include "SshTransfer.h"
#include "EditUtils.h"
#include "IconUtils.h"
#include "MenuUtils.h"
#include "MiscUtils.h"
#include "ViewerTypes.h"
#include "PalmDataProvider.h"

namespace Commander
{
	void CFileViewer::onInit()
	{
		SendMessage( _hDlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>( IconUtils::getFromPathWithAttrib( L".txt", FILE_ATTRIBUTE_NORMAL ) ) );
		SendMessage( _hDlg, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>( IconUtils::getFromPathWithAttrib( L".txt", FILE_ATTRIBUTE_NORMAL, true ) ) );

		_isHex = false;
		_wordWrap = false;
		_findParams = { false };

		// get the handle of currently active edit control
		_hActiveEdit = GetDlgItem( _hDlg, _wordWrap ? IDE_VIEWER_CONTENTWRAP : IDE_VIEWER_CONTENTNOWRAP );

		// create status-bar
		createStatusBar();

		// get dialog menu handle
		_hMenu = GetMenu( _hDlg );

		// make edit controls zoomable (only supported in Windows 10 1809 and later)
		SendDlgItemMessage( _hDlg, IDE_VIEWER_CONTENTWRAP, EM_SETEXTENDEDSTYLE, ES_EX_ZOOMABLE, ES_EX_ZOOMABLE );
		SendDlgItemMessage( _hDlg, IDE_VIEWER_CONTENTNOWRAP, EM_SETEXTENDEDSTYLE, ES_EX_ZOOMABLE, ES_EX_ZOOMABLE );

		// subclass both edit controls and store its default window procedure
		SetWindowSubclass( GetDlgItem( _hDlg, IDE_VIEWER_CONTENTWRAP ), wndProcEditSubclass, 0, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDE_VIEWER_CONTENTNOWRAP ), wndProcEditSubclass, 1, reinterpret_cast<DWORD_PTR>( this ) );

		// set font for viewer
		SendDlgItemMessage( _hDlg, IDE_VIEWER_CONTENTWRAP, WM_SETFONT, reinterpret_cast<WPARAM>( FCS::inst().getApp().getViewFont() ), TRUE );
		SendDlgItemMessage( _hDlg, IDE_VIEWER_CONTENTNOWRAP, WM_SETFONT, reinterpret_cast<WPARAM>( FCS::inst().getApp().getViewFont() ), TRUE );

		// determine whether edit control supports zooming
		LONG_PTR numerator = 0, denominator = 0;
		SendMessage( _hActiveEdit, EM_GETZOOM, reinterpret_cast<WPARAM>( &numerator ), reinterpret_cast<LPARAM>( &denominator ) );
		MenuUtils::enableItemByPos( GetSubMenu( _hMenu, 3 ), 3, numerator && denominator );

		// remove "save" from main menu, it's not needed in viewer
		MenuUtils::removeItem( GetSubMenu( _hMenu, 0 ), 1 );

		MiscUtils::centerOnWindow( FCS::inst().getFcWindow(), _hDlg );

		// tell windows that we can handle drag and drop
		DragAcceptFiles( _hDlg, TRUE );

		// create find dialog instance
		_pFindDlg = CBaseDialog::createModeless<CFindText>( _hDlg );

		_worker.init( [this] { return _findTextInFile(); }, _hDlg, UM_FINDTEXTDONE );

		_tripleClickPos = { 0, 0 };
		_tripleClickTime = 0;
		_currentZoom = 100;
		_linesCountPerPage = 0;
		_lineLength = 0;
		_fileSize.QuadPart = 0ll;
		_fileStreamPos = 0ll;
		_fileBom = StringUtils::NOBOM;
		_useEncoding = StringUtils::NOBOM;
		_useCodePage = GetACP(); // system default codepage

		// show status-bar by default
		showStatusBar();
	}

	bool CFileViewer::onClose()
	{
		show( SW_HIDE ); // hide dialog

		_worker.stop();

		return true;//MessageBox( NULL, L"Really exit?", L"Question", MB_YESNO | MB_ICONQUESTION ) == IDYES;
	}

	void CFileViewer::onDestroy()
	{
		if( _fileStream.is_open() )
			_fileStream.close();

		if( !_tempFileName.empty() )
			DeleteFile( _tempFileName.c_str() );

		DestroyIcon( reinterpret_cast<HICON>( SendMessage( _hDlg, WM_GETICON, ICON_SMALL, 0 ) ) );
		DestroyIcon( reinterpret_cast<HICON>( SendMessage( _hDlg, WM_GETICON, ICON_BIG, 0 ) ) );

		RemoveWindowSubclass( GetDlgItem( _hDlg, IDE_VIEWER_CONTENTWRAP ), wndProcEditSubclass, 0 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDE_VIEWER_CONTENTNOWRAP ), wndProcEditSubclass, 1 );
	}

	// TODO: regex, match-case, whole-words, reverse
	bool CFileViewer::_findTextInFile()
	{
		// The code page is set to the value returned by GetACP.
		setlocale( LC_ALL, "" ); // needed for _memicmp function

		std::ifstream fs( PathUtils::getExtendedPath( _tempFileName.empty() ? _fileName : _tempFileName ), std::ios::binary );

		if( fs.is_open() )
		{
			auto bom = ( _useEncoding == StringUtils::NOBOM ) ? StringUtils::ANSI : _useEncoding;

			if( bom == StringUtils::ANSI )
			{
				// check if '_textToFind' can be converted to multibyte string using given code-page
				std::string tmp = StringUtils::convert2A( _textToFind, _useCodePage );
				std::wstring strWide = StringUtils::convert2W( tmp, _useCodePage );

				// use UTF-8 as default (assuming other encodings would be using BOM anyway)
				if( strWide != _textToFind )
					bom = StringUtils::UTF8;
			}

			_useEncoding = bom;

			// set multibyte string
			std::string str;
			if( _findParams.hexMode && !_dataToFind.empty() ) // hex data to find
				str = _dataToFind;
			else // convert unicode string to multibyte
				StringUtils::utf16ToMultiByte( _textToFind, str, ( bom == StringUtils::ANSI ? _useCodePage : bom ) );

			std::streamsize read = 0ll;
			const char *p = nullptr;
			char buf[0x8000];

			fs.seekg( _findFromOffset );

			if( !_findParams.matchCase )
				CharUpperBuffA( &str[0], static_cast<DWORD>( str.length() ) );

			while( _worker.isRunning() && ( read = fs.read( buf, sizeof( buf ) ).gcount() ) )
			{
				if( _findParams.matchCase )
					p = StringUtils::memstr( buf, read, str );
				else
					p = StringUtils::memstri( buf, read, str );

				if( p )
				{
					LPARAM offset = ( fs.eof() ? _fileSize.QuadPart : fs.tellg() );
					offset -= read - ( p - buf );
					_worker.sendNotify( 2, offset );
					return true;
				}

				if( !fs.eof() )
				{
					std::streamoff offset = (std::streamoff)( str.length() - 1 );
					fs.seekg( offset * -1, std::ios::cur );
				}
			}
		}

		return false;
	}

	UINT CFileViewer::getEncodingId( StringUtils::EUtfBom encoding, StringUtils::EUtfBom bom )
	{
		UINT id = 0;

		switch( encoding )
		{
		case StringUtils::ANSI:
			id = IDM_VIEWER_ENCODING_ANSI;
			break;
		case StringUtils::UTF16BE:
			id = IDM_VIEWER_ENCODING_UTF16BE;
			break;
		case StringUtils::UTF16LE:
			id = IDM_VIEWER_ENCODING_UTF16LE;
			break;
		case StringUtils::UTF32BE:
			id = IDM_VIEWER_ENCODING_UTF32BE;
			break;
		case StringUtils::UTF32LE:
			id = IDM_VIEWER_ENCODING_UTF32LE;
			break;
		case StringUtils::UTF8:
			if( bom == StringUtils::UTF8 )
				id = IDM_VIEWER_ENCODING_UTF8BOM;
			else
				id = IDM_VIEWER_ENCODING_UTF8;
			break;
		case StringUtils::NOBOM:
		default:
			id = IDM_VIEWER_ENCODING_AUTO;
			break;
		}

		return id;
	}

	StringUtils::EUtfBom CFileViewer::getEncodingFromId( DWORD encodingId )
	{
		StringUtils::EUtfBom bom = StringUtils::NOBOM;

		switch( encodingId )
		{
		case IDM_VIEWER_ENCODING_ANSI:
			bom = StringUtils::ANSI;
			break;
		case IDM_VIEWER_ENCODING_UTF16BE:
			bom = StringUtils::UTF16BE;
			break;
		case IDM_VIEWER_ENCODING_UTF16LE:
			bom = StringUtils::UTF16LE;
			break;
		case IDM_VIEWER_ENCODING_UTF32BE:
			bom = StringUtils::UTF32BE;
			break;
		case IDM_VIEWER_ENCODING_UTF32LE:
			bom = StringUtils::UTF32LE;
			break;
		case IDM_VIEWER_ENCODING_UTF8:
		case IDM_VIEWER_ENCODING_UTF8BOM:
			bom = StringUtils::UTF8;
			break;
		case IDM_VIEWER_ENCODING_AUTO:
		default:
			bom = StringUtils::NOBOM;
			break;
		}

		return bom;
	}

	void CFileViewer::reportRegexError( HWND hWnd, const std::regex_error& e, const std::wstring& expression )
	{
		std::wstringstream sstr;

		sstr << L"Expression: \"" << expression << L"\"\r\n\r\n";
		sstr << StringUtils::convert2W( e.what() );

		MessageBox( hWnd, sstr.str().c_str(), L"Regular Expressions Error", MB_ICONEXCLAMATION | MB_OK );
	}

	void CFileViewer::updateWindowTitle()
	{
		std::wostringstream sstrTitle;
		sstrTitle << ( _tempFileName.empty() ? _fileName : _tempFileName ) << L" - Viewer";
		
		SetWindowText( _hDlg, sstrTitle.str().c_str() );
	}

	void CFileViewer::createStatusBar()
	{
		_hStatusBar = CreateWindowEx( 0, STATUSCLASSNAME, (PCTSTR)NULL, SBARS_SIZEGRIP | WS_CHILD | WS_CLIPSIBLINGS,
			0, 0, 0, 0, _hDlg, (HMENU)101, FCS::inst().getFcInstance(), NULL );

		// initialize status-bar parts
		RECT rct;
		GetClientRect( _hDlg, &rct );

		int sbParts[4] = { rct.right - 250, rct.right - 200, rct.right - 100, rct.right };
	
		SendMessage( _hStatusBar, SB_SETPARTS, 4, (LPARAM)sbParts );
	}

	void CFileViewer::updateStatusBar()
	{
		// set status-bar current zoom as text
		std::wstring zoom = std::to_wstring( _currentZoom ) + L"%";
		SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 1, SBT_NOBORDERS ), (LPARAM)zoom.c_str() );

		// set status-bar End of line as text
		std::wstring eol = StringUtils::eolToStr( _fileEol );
		SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 2, SBT_NOBORDERS ), (LPARAM)eol.c_str() );

		// set status-bar current encoding as text
		std::wstring bom = StringUtils::encodingToStr( _isHex ? StringUtils::ANSI : _useEncoding );
		SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 3, SBT_NOBORDERS ), (LPARAM)bom.c_str() );
	}

	bool CFileViewer::viewFile( const std::wstring& path, const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel )
	{
		_worker.stop();

		// user specified ansi codepage (or hexview) and an encoding
		WORD codePage = LOWORD( _dlgUserDataPtr );
		WORD encoding = HIWORD( _dlgUserDataPtr );

		_fileName = PathUtils::addDelimiter( path ) + fileName;
		_findFromIdxBeg = _findFromIdxEnd = 0;
		_findFromOffset = 0ll;
		_isHex = static_cast<bool>( codePage == 1 ); // 1 means view as hex
		_useCodePage = ( codePage > 1 ) ? static_cast<UINT>( codePage ) : _useCodePage;
		_useEncoding = static_cast<StringUtils::EUtfBom>( encoding );
		_outText = L"Please wait..";

		if( spPanel )
		{
			_localDirectory = spPanel->getDataManager()->getCurrentDirectory();
			_spPanel = spPanel;
		}

		SetWindowText( _hActiveEdit, _outText.c_str() );

		updateWindowTitle();

		// delete temp file when reading different file
		if( !_tempFileName.empty() )
		{
			if( _fileStream.is_open() )
				_fileStream.close();

			DeleteFile( _tempFileName.c_str() );

			_tempFileName.clear();
		}

		// TODO: we still read registry data directly, even though we have data provider for that..
		if( _spPanel && !_spPanel->getDataManager()->isInRegedMode() )
		{
			return _spPanel->getDataManager()->getDataProvider()->readToFile( _fileName, _spPanel, _hDlg );
		}
		else
		{
			if( _spPanel == nullptr && ArchiveType::isKnownType( path ) )
			{
				_tempFileName = FCS::inst().getTempPath() + PathUtils::stripPath( fileName );

				// file extractor sends UM_READERNOTIFY notification when done
				CBaseDialog::createModeless<CFileUnpacker>( _hDlg )->unpack(
					path, fileName, FCS::inst().getTempPath(), CArchiver::EExtractAction::Overwrite );
			}
			else // read file directly
				SendMessage( _hDlg, UM_READERNOTIFY, FC_ARCHDONEOK, 0 );
		}

		return true;
	}

	void CFileViewer::setParams( const CFindText::CParams& params, const std::wstring& hiliteText )
	{
		if( params.hexMode )
			StringUtils::convertHex( hiliteText, _dataToFind );

		_textToFind = hiliteText;
		_findParams = params;
	}

	bool CFileViewer::viewFileCore( bool isHexMode, StringUtils::EUtfBom bom )
	{
		DWORD idxBeg = 0, idxEnd = 0;

		// reset starting index/offset when changing modes/encodings
		if( _isHex != isHexMode || _useEncoding != bom )
		{
			_findFromIdxBeg = _findFromIdxEnd = 0;
			_findFromOffset = 0ll;
		}
		else // store caret position and current selection
			SendMessage( _hActiveEdit, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );

		_fileEol = StringUtils::NONE;
		_useEncoding = bom;
		_isHex = isHexMode;

		_worker.stop();

		updateWindowTitle();
		updateMenuEncoding();

		SetCursor( LoadCursor( NULL, IDC_WAIT ) );

		bool ret = false;

		// read data from file or from registry
		// TODO: registry data provider rewrite?
		if( _spPanel && _spPanel->getDataManager()->isInRegedMode() )
			ret = readFromRegistry( _fileName );
		else
			ret = readFromDisk( _tempFileName.empty() ? _fileName : _tempFileName );

		if( ret )
		{
			DWORD errorId = 0;

			if( !SetWindowText( _hActiveEdit, _outText.c_str() ) )
			{
				errorId = GetLastError();
				_isHex = true;
				ret = false;
			}

			PrintDebug("file successfully read(%u): %zu", errorId, _outText.length());

			SetCursor( NULL );

			if( !ret )
			{
				std::wostringstream sstr;
				sstr << L"Path: \"" << _fileName << L"\"\r\n";
				sstr << SysUtils::getErrorMessage( errorId );

				MessageBox( _hDlg, sstr.str().c_str(), L"Cannot Read File", MB_ICONEXCLAMATION | MB_OK );
			}

			updateStatusBar();

			if( _isHex )
			{
				if( _fileStream.is_open() )
					updateScrollBar( SIF_POS | SIF_RANGE | SIF_PAGE );

				// update menu when text mode failed
				updateMenuEncoding();
			}
			else
			{
				// restore caret position and current selection
				SendMessage( _hActiveEdit, EM_SETSEL, static_cast<WPARAM>( idxBeg ), static_cast<LPARAM>( idxEnd ) );
				SendMessage( _hActiveEdit, EM_SCROLLCARET, 0, 0 );
			}
		}

		return ret;
	}

	bool CFileViewer::readFromDisk( const std::wstring& fileName )
	{
		HANDLE hFile = CreateFile( PathUtils::getExtendedPath( fileName ).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );

		if( hFile == INVALID_HANDLE_VALUE )
		{
			DWORD errorId = GetLastError();
			std::wostringstream sstr;
			sstr << L"Path: \"" << fileName << L"\"\r\n";
			sstr << SysUtils::getErrorMessage( errorId );

			MessageBox( _hDlg, sstr.str().c_str(), L"Cannot Read File", MB_ICONEXCLAMATION | MB_OK );

			return false;
		}
		else
		{
			GetFileSizeEx( hFile, &_fileSize );
			CloseHandle( hFile );

			try {
				// read text data from file
				readFile( fileName );
			}
			catch( const std::bad_alloc& e ) {
				MessageBox( _hDlg, FORMAT( L"Exception thrown: %ls",
					StringUtils::convert2W( e.what() ).c_str() ).c_str(),
					L"Out of Memory", MB_ICONEXCLAMATION | MB_OK );

				return false;
			}
		}

		return true;
	}

	bool CFileViewer::readFromRegistry( const std::wstring& fileName )
	{
		std::map<std::wstring, HKEY> _rootKeys;
		INITIALIZE_ROOTKEY(HKEY_CLASSES_ROOT);
		INITIALIZE_ROOTKEY(HKEY_CURRENT_USER);
		INITIALIZE_ROOTKEY(HKEY_LOCAL_MACHINE);
		INITIALIZE_ROOTKEY(HKEY_USERS);
		INITIALIZE_ROOTKEY(HKEY_PERFORMANCE_DATA);
		INITIALIZE_ROOTKEY(HKEY_CURRENT_CONFIG);

		auto localPath = _localDirectory.empty()
			? fileName.substr( wcslen( ReaderType::getModePrefix( EReadMode::Reged ) ) + 1 )
			: _localDirectory.substr( wcslen( ReaderType::getModePrefix( EReadMode::Reged ) ) + 1 );
		auto rootName = localPath.substr( 0, localPath.find_first_of( L'\\' ) );
		auto keyName = _localDirectory.empty()
			? PathUtils::stripFileName( localPath.substr( rootName.length() + 1 ) )
			: PathUtils::stripDelimiter( localPath.substr( rootName.length() + 1 ) );
		auto valName = StringUtils::endsWith( fileName, L"(Default)" ) ? L"" : _localDirectory.empty()
			? PathUtils::stripPath( fileName )
			: fileName.substr( _localDirectory.length() );

		HKEY hKey = nullptr;
		DWORD valueType, valueLen = 0;

		LSTATUS ret = RegOpenKeyEx( _rootKeys[rootName], keyName.c_str(), 0, KEY_READ, &hKey );

		if( ret == ERROR_SUCCESS )
		{
			ret = RegQueryValueEx( hKey, valName.c_str(), NULL, &valueType, NULL, &valueLen );

			_fileSize.QuadPart = (LONGLONG)valueLen;
			_fileStreamPos = 0ll;
			_linesCountPerPage = 0;
			_lineLength = 0;

			_outText.clear();
		}

		if( ret != ERROR_SUCCESS && ret != ERROR_MORE_DATA )
		{
			if( hKey )
				RegCloseKey( hKey );

			std::wostringstream sstr;
			sstr << L"Root key: " << rootName << L"\r\n";
			sstr << L"Key: " << keyName << L"\r\n";
			sstr << L"Value: " << valName << L"\r\n\r\n";
			sstr << SysUtils::getErrorMessage( ret );

			MessageBox( _hDlg, sstr.str().c_str(), L"Reading Registry Key Error", MB_ICONEXCLAMATION | MB_OK );

			return false; // key does not exist in registry
		}

		if( !_isHex && _useEncoding == StringUtils::NOBOM && valueType != REG_NONE && valueType != REG_BINARY )
		{
			if( valueType == REG_SZ || valueType == REG_EXPAND_SZ || valueType == REG_MULTI_SZ || valueType == REG_LINK )
			{
				_ASSERTE( ( valueLen % sizeof( WCHAR ) ) == 0 );

				_outText.resize( ( valueLen / sizeof( WCHAR ) ) /*+ ( ( valueLen % 2 ) != 0 ? 1 : 0 )*/ );
				ret = RegQueryValueEx( hKey, valName.c_str(), NULL, &valueType, (BYTE*)&_outText[0], &valueLen );
				//_outText[valueLen / sizeof( WCHAR )] = L'\0';

				if( valueType == REG_MULTI_SZ )
					_outText = StringUtils::getValueMulti( _outText.c_str(), valueLen / sizeof( WCHAR ) );
			}
			else
				_outText = MiscUtils::readRegistryValue( hKey, valName, valueType, valueLen );
		}
		else
		{
			std::string str; str.resize( valueLen );
			RegQueryValueEx( hKey, valName.c_str(), NULL, &valueType, (BYTE*)&str[0], &valueLen );

			// try to display file as text
			if( !_isHex && !readFileText( std::istringstream( str, valueLen ) ) )
				_isHex = true;

			if( _isHex )
				readFileHex( std::istringstream( str, valueLen ) );
		}

		RegCloseKey( hKey );

		return true;
	}

	bool CFileViewer::readFileText( std::istream& inStream )
	{
		CTextFileReader reader( inStream, nullptr/*TODO: &_worker*/, _useEncoding, _useCodePage );

		if( !reader.isText() && _useEncoding == StringUtils::NOBOM )
			return false;

		_useEncoding = reader.getEncoding();
		_fileBom = reader.getByteOrderMarker();
		_fileEol = reader.getEol();

		// ask user what to do when text file is too big (over 16MB)
		if( reader.getFileSize() > 0xFFFFFFll &&
			MessageBox(
				_hDlg,
				L"File is too big to display as text.\nOperation may take a long time.\nContinue displaying as text?", L"File too large",
				MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2 ) == IDNO )
			return false;

		CheckMenuRadioItem( GetSubMenu( _hMenu, 4 ), IDM_VIEWER_ENCODING_AUTO, IDM_VIEWER_ENCODING_ANSI, getEncodingId( _useEncoding ), MF_BYCOMMAND | MF_CHECKED );

		// read file content as text
		DWORD errorId = NO_ERROR;

		while( ( errorId = reader.readLine() ) == ERROR_SUCCESS )
		{
			_outText += reader.getLineRef();

			if( reader.isEolEnding() )
				_outText += L"\r\n";
		}

		if( errorId != ERROR_SUCCESS && errorId != ERROR_HANDLE_EOF && errorId != ERROR_BAD_FORMAT )
		{
			std::wostringstream sstr;
			sstr << L"Path: \"" << _fileName << L"\"\r\n";
			sstr << L"Encoding: \"" << StringUtils::encodingToStr( _useEncoding ) << L"\"\r\n";
			sstr << SysUtils::getErrorMessage( errorId );

			MessageBox( _hDlg, sstr.str().c_str(), L"Cannot Read File", MB_ICONEXCLAMATION | MB_OK );
		}

		return errorId != ERROR_BAD_FORMAT;
	}

	void CFileViewer::readFile( const std::wstring& fileName )
	{
		// close and open file stream as a binary stream
		if( _fileStream.is_open() )
			_fileStream.close();

		_fileStreamPos = 0ll;
		_outText.clear();

		// try to read file info of known types - mp3, avi, doc, etc.
		if( !_isHex )
			readFileMediaInfo( fileName ); // TODO: doesn't work with long paths

		if( _outText.empty() )
		{
			_fileStream.open( PathUtils::getExtendedPath( fileName ), std::ios::binary );

			// try to display file as text
			if( !_isHex && !readFileText( _fileStream ) )
				_isHex = true;

			if( _isHex )
			{
				_linesCountPerPage = MiscUtils::countVisibleLines( _hActiveEdit );
				_lineLength = 4 * BYTES_ROW + BYTES_ROW / 4 + (int)FsUtils::getOffsetNumber( _fileSize, 0ll ).length();

				// display file content in hexadecimal format
				readFileHex( _fileStream );
			}
		}
	}

	char CFileViewer::getPrintableChar( char ch )
	{
		if( ( ch >= ' ' || ch < '\0' ) && ch != '\x7F' && ch != '\x80' && ch != '\x81' && ch != '\x83' && ch != '\x88' && ch != '\x90' && ch != '\x98' )
			return ch;
		else
			return '\xB7'; // middle-dot char (in CP1250)
	}

	bool CFileViewer::readFileHex( std::istream& inStream )
	{
		std::ostringstream sstr;

		if( inStream.eof() )
			inStream.clear();

		if( inStream.fail() )
		{
			PrintDebug("fail");
			return false;
		}

		//PrintDebug("Pos: %zu, EOF: %d", _fileStreamPos, iStream.eof());
		inStream.seekg( _fileStreamPos, std::ifstream::_Seekbeg );

		_ASSERTE( sizeof( _buf ) >= _linesCountPerPage * BYTES_ROW );

		std::streamoff bytesPerLine;
		std::streamsize bytesRead = inStream.read( _buf, _fileStream.is_open() ? ( _linesCountPerPage * BYTES_ROW ) : sizeof( _buf ) ).gcount();
		//PrintDebug("End: %zu, read: %zu, EOF: %d", (std::streamoff)iStream.tellg(), bytesRead, iStream.eof());

		for( std::streamoff offset = 0ll;/**/; offset += BYTES_ROW )
		{
			bytesPerLine = min( bytesRead - offset, BYTES_ROW );

			if( bytesPerLine <= 0 )
				break;

			sstr << std::hex << std::setfill( '0' ) << std::uppercase;

			// write out the offset number
			sstr << FsUtils::getOffsetNumber( _fileSize, _fileStreamPos + offset );

			// write out extra space between every 4 bytes and ascii data at the end
			for( std::streamsize i = 1; i < BYTES_ROW + 1; ++i )
			{
				// write out one BYTE
				if( i < bytesPerLine + 1 )
					sstr << std::setw( 2 ) << ( _buf[offset+i-1] & 0xFF ) << " ";
				else
					sstr << "   ";

				// spacing between bytes
				if( ( i % 4 ) == 0 )
					sstr << " ";
			}

			// write out the remaining bits of data
			for( std::streamoff i = 0; i < bytesPerLine; ++i )
				sstr << getPrintableChar( _buf[offset+i] );

			sstr << "\r\n";
		}

		_outText = StringUtils::convert2W( sstr.str(), _useCodePage );
		_fileEol = StringUtils::NONE;

		return true;
	}

	bool CFileViewer::readFileMediaInfo( const std::wstring& fileName )
	{
		std::wostringstream sstr;

		auto mediaInfo = ShellUtils::getMediaInfo( fileName );
		for( auto it = mediaInfo.begin(); it != mediaInfo.end(); ++it )
		{
			sstr << *it << "\r\n";
		}

		_outText = sstr.str();

		return true;
	}

	void CFileViewer::hiliteText( bool reverse )
	{
		if( !_textToFind.empty() )
		{
			SetCursor( LoadCursor( NULL, IDC_WAIT ) );

			std::wostringstream sstr;
			sstr << L"Searching: " << _textToFind;
			SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 0, SBT_POPOUT ), (LPARAM)sstr.str().c_str() );

			if( _isHex && _fileStream.is_open() )
			{
				_worker.start();
				return;
			}

			int idxBeg = -1, idxEnd = -1;
			if( findTextInEditBox( _textToFind, reverse ? _findFromIdxBeg : _findFromIdxEnd, reverse, idxBeg, idxEnd ) )
			{
				SendMessage( _hActiveEdit, EM_SETSEL, static_cast<WPARAM>( idxBeg ), static_cast<LPARAM>( idxEnd ) );
				SendMessage( _hActiveEdit, EM_SCROLLCARET, 0, 0 );

				SetCursor( NULL );
				SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 0, SBT_POPOUT ), (LPARAM)L"" );

				_findFromIdxBeg = idxBeg;
				_findFromIdxEnd = idxEnd;
			}
			else
			{
				SetCursor( NULL );
				SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 0, SBT_POPOUT ), (LPARAM)L"Not found" );

				std::wstringstream sstr;
				sstr << L"Cannot find \"" << _textToFind << L"\".";
				_pFindDlg->showMessageBox( sstr.str() );
			}
		}
		else
			findTextDialog();
	}

	void CFileViewer::searchText()
	{
		if( _findParams.hexMode )
		{
			if( !_isHex )
				viewFileCore( true, _useEncoding ); // switch to hex-mode

			if( !StringUtils::convertHex( _textToFind, _dataToFind, _useCodePage ) )
			{
				std::wstringstream sstr;
				sstr << L"Error converting hexadecimal string.";

				MessageBox( _hDlg, sstr.str().c_str(), L"Conversion Failed", MB_ICONEXCLAMATION | MB_OK );

				return;
			}
		}
		else if( _findParams.regexMode )
		{
			auto flags = _findParams.matchCase ? std::regex_constants::ECMAScript : std::regex_constants::icase;

			try {
				_regexA = std::make_unique<std::regex>( StringUtils::convert2A( _textToFind ), flags ); // TODO: search in hex-mode
				_regexW = std::make_unique<std::wregex>( _textToFind, flags );
			}
			catch( const std::regex_error& e )
			{
				CFileViewer::reportRegexError( _hDlg, e, _textToFind );

				return;
			}
		}

		hiliteText( _findParams.reverse );
	}

	bool CFileViewer::findTextInEditBox( const std::wstring& text, int fromIdx, bool reverse, int& idxBeg, int& idxEnd )
	{
		_ASSERTE( !text.empty() );
		_ASSERTE( fromIdx <= _outText.length() );

	//	auto hEditData = (HLOCAL)SendMessage( _hActiveEdit, EM_GETHANDLE, 0, 0 );

		bool ret = false;
		idxBeg = idxEnd = -1;

	//	if( hEditData )
	//	{
	//		TCHAR *buf = (TCHAR*)LocalLock( hEditData );
	//		if( buf )
	//		{
				if( _findParams.regexMode )
				{
					try {
						// TODO: reverse search very inefficient
						auto it = std::wsregex_iterator( _outText.begin() + ( reverse ? 0 : fromIdx ), ( reverse ? _outText.begin() + fromIdx : _outText.end() ), *_regexW );

						for( ; it != std::wsregex_iterator(); ++it )
						{
							auto idx = it->position() + ( reverse ? 0 : fromIdx );
							auto len = it->length();

							if( !_findParams.wholeWords || ( ( idx == 0 || !StringUtils::isAlphaNum( _outText[idx] ) || !StringUtils::isAlphaNum( _outText[idx - 1] ) ) &&
								( !len || _outText.length() == idx + len || !StringUtils::isAlphaNum( _outText[idx + len - 1] ) || !StringUtils::isAlphaNum( _outText[idx + len] ) ) ) )
							{
								idxBeg = static_cast<int>( idx );
								idxEnd = idxBeg + static_cast<int>( len );

								ret = true;

								if( !reverse )
									break;
							}
						}
					}
					catch( const std::regex_error& e )
					{
						CFileViewer::reportRegexError( _hDlg, e, _textToFind );

						return false;
					}
				}
				else
				{
					bool bounds[2], wholeWords = _findParams.wholeWords;

					bounds[0] = StringUtils::isAlphaNum( text[0] );
					bounds[1] = StringUtils::isAlphaNum( text[text.length() - 1] );

					// when there are non-alpha-numeric characters (bounds left and right) in the source string, we don't need wholewords flag
					if( wholeWords && !bounds[0] && !bounds[1] )
						wholeWords = false;

					const wchar_t *p = StringUtils::findStr( &_outText[0], _outText.length(), text, _findParams.matchCase, wholeWords, bounds, (size_t)fromIdx, reverse );

					if( p )
					{
						idxBeg = static_cast<int>( p - &_outText[0] );
						idxEnd = idxBeg + static_cast<int>( text.length() );
						ret = true;
					}
				}
	//			LocalUnlock( hEditData );
	//		}
	//	}

		return ret;
	}

	std::wstring getOffsetStr( LARGE_INTEGER& fileSize, std::streamoff offset )
	{
		std::wstring outText;
		wchar_t line[10];

		if( fileSize.HighPart > 0xFFFF ) {
			swprintf( line, L"%04X ", (DWORD)( ( offset >> 48 ) & 0xFFFF ) );
			outText += line;
		}
		if( fileSize.HighPart ) {
			swprintf( line, L"%04X ", (DWORD)( ( offset >> 32 ) & 0xFFFF ) );
			outText += line;
		}
		if( fileSize.LowPart > 0xFFFF ) {
			swprintf( line, L"%04X ", (DWORD)( ( offset >> 16 ) & 0xFFFF ) );
			outText += line;
		}

		swprintf( line, L"%04X", (DWORD)( offset & 0xFFFF ) );
		outText += line;

		return outText;
	}

	std::streamoff CFileViewer::getCurrentOffsetInEditBox()
	{
		auto lineIdx = (int)SendMessage( _hActiveEdit, EM_GETFIRSTVISIBLELINE, 0, 0 );
		auto lineIdxInChars = (int)SendMessage( _hActiveEdit, EM_LINEINDEX, lineIdx, 0 );
		auto lineLen = SendMessage( _hActiveEdit, EM_LINELENGTH, lineIdxInChars, 0 );

		std::wstring line;
		line.resize( lineLen );

		((WORD*)&line[0])[0] = (WORD)lineLen;
		auto read = SendMessage( _hActiveEdit, EM_GETLINE, lineIdx, (LPARAM)&line[0] );

		_ASSERTE( read == lineLen );

		std::streamoff pos = 0ll;

		// parse text and return offset
		if( !line.empty() && parseOffsetnumber( StringUtils::cutToChar( line, L':' ), pos ) )
		{
			return pos;
		}

		return 0;
	}

	int CFileViewer::findOffsetInEditBox( std::streamoff offset )
	{
		std::wstring line;
		line.resize( _lineLength );

		auto strOffset = getOffsetStr( _fileSize, offset / BYTES_ROW * BYTES_ROW );

		int cnt = (int)SendMessage( _hActiveEdit, EM_GETLINECOUNT, 0, 0 );
		for( int i = 0; i < cnt; ++i )
		{
			auto idxLineInChars = (int)SendMessage( _hActiveEdit, EM_LINEINDEX, i, 0 );
			auto len = SendMessage( _hActiveEdit, EM_LINELENGTH, idxLineInChars, 0 );

			if( line.size() != len )
				line.resize( len );

			((WORD*)&line[0])[0] = (WORD)len;

			auto read = SendMessage( _hActiveEdit, EM_GETLINE, i, reinterpret_cast<LPARAM>( &line[0] ) );

			_ASSERTE( read == len );

			if( StringUtils::startsWith( line, strOffset ) )
				return idxLineInChars + _lineLength - ( BYTES_ROW - ( offset % BYTES_ROW ) );
		}

		return -1;
	}

	void CFileViewer::handleDropFiles( HDROP hDrop )
	{
		auto items = ShellUtils::getDroppedItems( hDrop );
	
		DragFinish( hDrop );

		if( !items.empty() )
			viewFile( PathUtils::stripFileName( items[0] ), PathUtils::stripPath( items[0] ) );
	}

	bool CFileViewer::parseOffsetnumber( const std::wstring& text, std::streamoff& pos )
	{
		auto strNum = StringUtils::removeAll( text, L' ' );

		try {
			pos = _isHex ? std::stoll( strNum, nullptr, 16 ) : std::stoll( strNum );
		}
		catch( std::invalid_argument ) {
			MessageBox( _hDlg, FORMAT( L"Cannot convert \"%ls\" to number.", strNum.c_str() ).c_str(), L"Invalid Number", MB_ICONEXCLAMATION | MB_OK );
			return false;
		}
		catch( std::out_of_range ) {
			MessageBox( _hDlg, FORMAT( L"Number \"%ls\" is out of range.", strNum.c_str() ).c_str(), L"Number Out of Range", MB_ICONEXCLAMATION | MB_OK );
			return false;
		}

		return true;
	}

	void CFileViewer::gotoLineDialog()
	{
		auto offsetMax = getOffsetStr( _fileSize, _fileSize.QuadPart / BYTES_ROW * BYTES_ROW );
		auto lineCount = (int)SendMessage( _hActiveEdit, EM_GETLINECOUNT, 0, 0 );
		auto firstLine = (int)SendMessage( _hActiveEdit, EM_GETFIRSTVISIBLELINE, 0, 0 );

		CTextInput::CParams params(
			_isHex ?
				L"Go To Offset" :
				L"Go To Line",
			_isHex ?
				FORMAT( L"Offset hex (0 - %ls):", offsetMax.c_str() ) :
				FORMAT( L"Line number (1 - %d):", lineCount ),
			_isHex && _fileStream.is_open() ?
				getOffsetStr( _fileSize, _fileStreamPos ) :
			_isHex ?
				getOffsetStr( _fileSize, firstLine * BYTES_ROW ) :
				std::to_wstring( firstLine + 1 ) );

		auto text = MiscUtils::showTextInputBox( params, _hDlg );

		std::streamoff pos = 0ll;
		StringUtils::trim( text );

		// parse text and scroll into requested position
		if( !text.empty() && parseOffsetnumber( text, pos ) )
		{
			if( _isHex && _fileStream.is_open() )
				scrollToPos( pos / BYTES_ROW * BYTES_ROW );
			else
			{
				std::streamoff lineIdx = pos - firstLine - 1;

				if( _isHex )
					lineIdx = pos / BYTES_ROW - firstLine;

				SendMessage( _hActiveEdit, EM_LINESCROLL, 0, (LPARAM)lineIdx );
			}
		}
	}

	void CFileViewer::findTextDialog()
	{
		std::wstring text = _textToFind;

		if( !_findParams.hexMode && !_findParams.regexMode )
			text = EdUtils::getSelection( _hActiveEdit );

		_pFindDlg->updateText( text.empty() ? _textToFind : text );
		_pFindDlg->updateParams( _findParams );
		_pFindDlg->show();
	}

	void CFileViewer::swapActiveEdit()
	{
		SetCursor( LoadCursor( NULL, IDC_WAIT ) );

		_wordWrap = !_wordWrap;

		MenuUtils::checkItem( GetSubMenu( _hMenu, 3 ), IDM_VIEWER_VIEW_WRAP, _wordWrap );

		// store client rect, caret position and current selection
		RECT rct;
		GetClientRect( _hDlg, &rct );

		DWORD idxBeg = 0, idxEnd = 0;
		SendMessage( _hActiveEdit, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );

		// switch currently active edit-box handle
		_hActiveEdit = GetDlgItem( _hDlg, _wordWrap ? IDE_VIEWER_CONTENTWRAP : IDE_VIEWER_CONTENTNOWRAP );

		// update newly active edit-box size
		updateLayout( rct.left + rct.right, rct.top + rct.bottom );

		// update text in newly active (no)wrap edit-box
		SetWindowText( _hActiveEdit, _outText.c_str() );

		// swap edit-boxes at once
		HDWP hdwp = BeginDeferWindowPos( 2 );

		if( hdwp ) hdwp = DeferWindowPos( hdwp, GetDlgItem( _hDlg, IDE_VIEWER_CONTENTWRAP ), NULL, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | ( _wordWrap ? SWP_SHOWWINDOW : ( SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOZORDER | SWP_NOOWNERZORDER ) ) );

		if( hdwp ) hdwp = DeferWindowPos( hdwp, GetDlgItem( _hDlg, IDE_VIEWER_CONTENTNOWRAP ), NULL, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | ( _wordWrap ? ( SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOZORDER | SWP_NOOWNERZORDER ) : SWP_SHOWWINDOW ) );

		if( hdwp ) EndDeferWindowPos( hdwp );

		// restore selection and caret position from previously active edit-box
		SendMessage( _hActiveEdit, EM_SETSEL, static_cast<WPARAM>( idxBeg ), static_cast<LPARAM>( idxEnd ) );
		SendMessage( _hActiveEdit, EM_SCROLLCARET, 0, 0 );

		// restore current zoom
		SendMessage( _hActiveEdit, EM_SETZOOM, (WPARAM)( ( (double)_currentZoom / 100. ) * 985. ), (LPARAM)1000 );

		SetFocus( _hActiveEdit );

		SetCursor( NULL );
	}

	void CFileViewer::showStatusBar()
	{
		bool isVisible = !!IsWindowVisible( _hStatusBar );
		ShowWindow( _hStatusBar, isVisible ? SW_HIDE : SW_SHOWNORMAL );

		MenuUtils::checkItem( GetSubMenu( _hMenu, 3 ), IDM_VIEWER_VIEW_STATUSBAR, !isVisible );

		RECT rct;
		GetClientRect( _hDlg, &rct );

		// update layout
		updateLayout( rct.left + rct.right, rct.top + rct.bottom );
	}

	void CFileViewer::setZoom( int value )
	{
		if( ( value == 0 ) || ( value > 0 && _currentZoom < 500 ) || ( value < 0 && _currentZoom > 10 ) )
		{
			_currentZoom += value > 0 ? 10 : value < 0 ? -10 : -_currentZoom + 100;

			if( SendMessage( _hActiveEdit, EM_SETZOOM, (WPARAM)( ( (double)_currentZoom / 100. ) * 985. ), (LPARAM)1000 ) )
			{
				updateStatusBar();

				if( _isHex && _fileStream.is_open() )
				{
					auto cnt = MiscUtils::countVisibleLines( _hActiveEdit );
					if( cnt != _linesCountPerPage )
					{
						_linesCountPerPage = cnt;
						scrollToPos( _fileStreamPos, true );
					}
					else
						updateScrollBar( SIF_RANGE | SIF_POS );
				}
			}
			else
				_currentZoom = 100;
		}
	}

	bool CFileViewer::selectCurrentWord()
	{
		if( _outText.empty() )
			return false;

		LONG_PTR idxBeg = 0, idxEnd = 0;
		SendMessage( _hActiveEdit, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );

		if( StringUtils::isAlphaNum( _outText[idxBeg] ) )
		{
			// find word boundaries
			while( idxBeg > 0 && StringUtils::isAlphaNum( _outText[idxBeg - 1] ) )
				--idxBeg;

			while( idxEnd + 1 <= (LONG_PTR)_outText.length() && StringUtils::isAlphaNum( _outText[idxEnd] ) )
				++idxEnd;

			SendMessage( _hActiveEdit, EM_SETSEL, static_cast<WPARAM>( idxBeg ), static_cast<LPARAM>( idxEnd ) );

			return true;
		}

		return false;
	}

	void CFileViewer::selectCurrentLine()
	{
		if( _outText.empty() )
			return;

		// look for CRLF characters to get the current line's boundaries
		LONG_PTR idxBeg = 0, idxEnd = 0;
		SendMessage( _hActiveEdit, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );

		while( idxBeg > 0 && !( _outText[idxBeg - 1] == L'\n' && idxBeg > 1 && _outText[idxBeg - 2] == L'\r' ) )
			--idxBeg;

		while( idxEnd + 1 <= (LONG_PTR)_outText.length() && !( _outText[idxEnd] == L'\r' && idxEnd + 2 <= (LONG_PTR)_outText.length() && _outText[idxEnd + 1] == L'\n' ) )
			++idxEnd;

		SendMessage( _hActiveEdit, EM_SETSEL, static_cast<WPARAM>( idxBeg ), static_cast<LPARAM>( idxEnd ) );
		PrintDebug("Triple click! beg: %d, end: %d", idxBeg, idxEnd);
	}

	void CFileViewer::copyTextToFile()
	{
		// check if there is some selected text
		DWORD idxBeg = 0, idxEnd = 0;
		SendMessage( _hActiveEdit, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );

		saveFileDialog( idxBeg != idxEnd ? _outText.substr( idxBeg, idxEnd - idxBeg ) : _outText );
	}

	LRESULT CALLBACK CFileViewer::wndProcEdit( HWND hWnd, UINT message, WPARAM wp, LPARAM lp )
	{
	//	PrintDebug("msg: 0x%04X, wpar: 0x%04X, lpar: 0x%04X", message, wp, lp);
		switch( message )
		{
		case WM_KEYDOWN:
			if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
			{
				if( handleKeyboardShortcuts( wp ) )
					return 0;

				if( _isHex && _fileStream.is_open() )
				{
					switch( wp )
					{
					case VK_HOME:
					case VK_PRIOR:
						handleScroll( 0, SB_TOP );
						return 0;
					case VK_END:
					case VK_NEXT:
						handleScroll( 0, SB_BOTTOM );
						return 0;
					}
				}
			}
			else switch( wp )
			{
			case VK_F3:
			{
				// invert direction when holding shift key
				bool shift = ( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0 );
				hiliteText( ( _findParams.reverse && !shift ) || ( !_findParams.reverse && shift ) );
				break;
			}
			case VK_UP:
			case VK_DOWN:
				if( _isHex && _fileStream.is_open() )
				{
					handleScroll( 0, ( wp == VK_UP ) ? SB_LINEUP : SB_LINEDOWN );
					return 0;
				}
				break;
			case VK_HOME:
			case VK_END:
				if( _isHex && _fileStream.is_open() )
				{
					handleScroll( 0, ( wp == VK_HOME ) ? SB_TOP : SB_BOTTOM );
					return 0;
				}
				break;
			case VK_SPACE:
				PrintDebug("space");
				break;
			case VK_BACK:
				PrintDebug("backspace");
				break;
			}
			break;

		case WM_MOUSEWHEEL:
			if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
			{
				setZoom( GET_WHEEL_DELTA_WPARAM( wp ) );
			//	LONG_PTR numerator = 0, denominator = 0;
			//	SendMessage( _hActiveEdit, EM_GETZOOM, reinterpret_cast<WPARAM>( &numerator ), reinterpret_cast<LPARAM>( &denominator ) );
			//	PrintDebug("Num: %d, denom: %d", numerator, denominator);
			}
		case WM_VSCROLL:
			if( _isHex && _fileStream.is_open() )
			{
				handleScroll( HIWORD( wp ), ( message == WM_MOUSEWHEEL ) ? message : LOWORD( wp ) );
				return 0;
			}
			break;

		case WM_LBUTTONDBLCLK:
		{
			bool ret = selectCurrentWord();
			_tripleClickPos.x = GET_X_LPARAM( lp );
			_tripleClickPos.y = GET_Y_LPARAM( lp );
			_tripleClickTime = GetTickCount();

			if( ret )
				return 0;
			break;
		}
		case WM_LBUTTONDOWN: // check for a triple-click
			if( _tripleClickTime && ( _tripleClickPos.x == GET_X_LPARAM( lp ) && _tripleClickPos.y == GET_Y_LPARAM( lp ) )
				&& GetTickCount() - _tripleClickTime < GetDoubleClickTime() )
			{
				// select entire line at caret position
				selectCurrentLine();

				_tripleClickTime = 0;
				_tripleClickPos = { 0, 0 };

				return 0;
			}
			else
			{
				_tripleClickTime = 0;
				_tripleClickPos = { 0, 0 };
			}
			break;

		case WM_KEYUP:
		case WM_LBUTTONUP:
			if( message == WM_LBUTTONUP || wp == VK_LEFT || wp == VK_RIGHT || wp == VK_UP || wp == VK_DOWN || wp == VK_PRIOR || wp == VK_NEXT || wp == VK_HOME || wp == VK_END )
			{
				DWORD idxBeg = 0, idxEnd = 0;
				SendMessage( hWnd, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );
				_findFromIdxBeg = _findFromIdxEnd = idxBeg;
				if( _isHex )
					_findFromOffset = getCurrentOffsetInEditBox();
			}
			break;

		case WM_SETCURSOR:
			if( _isHex && _worker.isRunning() )
			{
				SetCursor( LoadCursor( NULL, IDC_WAIT ) );
				return 0;
			}
			break;

		case WM_CONTEXTMENU:
			handleContextMenu( POINT { GET_X_LPARAM( lp ), GET_Y_LPARAM( lp ) } );
			return 0;
		}

		return DefSubclassProc( hWnd, message, wp, lp ) & ( ( message == WM_GETDLGCODE ) ? ~DLGC_HASSETSEL : ~0 );
	}

	void CFileViewer::handleContextMenu( POINT pt )
	{
		if( pt.x == -1 || pt.y == -1 )
		{
			// get edit control's center position
			RECT rct;
			GetClientRect( _hActiveEdit, &rct );
			pt.x = ( rct.left + rct.right ) / 2;
			pt.y = ( rct.top + rct.bottom ) / 2;
			ClientToScreen( _hActiveEdit, &pt );
		}

		// get text selection from edit control
		DWORD idxBeg = 0, idxEnd = 0;
		SendMessage( _hActiveEdit, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );

		auto hMenu = CreatePopupMenu();
		if( hMenu )
		{
			AppendMenu( hMenu, MF_STRING | ( idxBeg == idxEnd ? MF_GRAYED : 0 ), IDM_VIEWER_EDIT_COPY, L"&Copy" );
			AppendMenu( hMenu, MF_STRING, IDM_VIEWER_EDIT_COPYTOFILE, L"C&opy to File..." );
			AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );
			AppendMenu( hMenu, MF_STRING, IDM_VIEWER_EDIT_SELECTALL, L"Select &All" );
			AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );
			AppendMenu( hMenu, MF_STRING, IDM_VIEWER_SEARCH_GOTO, ( _isHex ? L"&Go to Offset..." : L"&Go to Line..." ) );
			AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );
			AppendMenu( hMenu, MF_STRING, IDM_VIEWER_SEARCH_FIND, L"&Find..." );
			AppendMenu( hMenu, MF_STRING, IDM_VIEWER_SEARCH_FINDNEXT, L"Find &Next" );
			AppendMenu( hMenu, MF_STRING, IDM_VIEWER_SEARCH_FINDPREVIOUS, L"Find &Previous" );
			AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );
			AppendMenu( hMenu, MF_STRING, IDM_VIEWER_VIEW_HEX, L"&Hex" );
			AppendMenu( hMenu, MF_STRING, IDM_VIEWER_VIEW_TEXT, L"&Text" );
			AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );
			AppendMenu( hMenu, MF_STRING | ( _isHex ? MF_GRAYED : 0 ), IDM_VIEWER_VIEW_WRAP, L"&Word Wrap" );

			CheckMenuRadioItem( hMenu, IDM_VIEWER_VIEW_HEX, IDM_VIEWER_VIEW_TEXT, _isHex ? IDM_VIEWER_VIEW_HEX : IDM_VIEWER_VIEW_TEXT, MF_BYCOMMAND | MF_CHECKED );
			MenuUtils::checkItem( hMenu, IDM_VIEWER_VIEW_WRAP, _wordWrap );

			handleMenuCommands( TrackPopupMenu( hMenu, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, _hDlg, NULL ) );

			DestroyMenu( hMenu );
		}
	}

	bool CFileViewer::handleKeyboardShortcuts( WPARAM wParam )
	{
		switch( wParam )
		{
		case VK_NUMPAD0: // 0 - restore default zoom
		case 0x30:
			setZoom( 0 );
			break;
		case VK_ADD: // [+] - zoom-in
		case 0xBB:
			setZoom( 1 );
			break;
		case VK_SUBTRACT: // [-] - zoom-out
		case 0xBD:
			setZoom( -1 );
			break;
		case 0x41: // A - select all
			SendMessage( _hActiveEdit, EM_SETSEL, 0, (LPARAM)-1 );
			break;
		case 0x42: // B - show status-bar
			showStatusBar();
			break;
		case 0x43: // C - save file dialog
			if( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0 )
				copyTextToFile(); // Copy to file: Ctrl+Shift+C
			else
				return false;
			break;
		case 0x46: // F - show find dialog
			findTextDialog();
			break;
		case 0x47: // G - goto line dialog
			gotoLineDialog();
			break;
		case 0x48: // H - hex mode
			viewFileCore( true, _useEncoding );
			break;
		case 0x4E: // N - find next
			hiliteText( _findParams.reverse );
			break;
		case 0x4F: // O - open file
			openFileDialog();
			break;
		case 0x50: // P - find previous
			hiliteText( !_findParams.reverse );
			break;
		case 0x52: // R - refresh
			viewFileCore( _isHex, _useEncoding );
			break;
		case 0x53: // S - save file dialog
			if( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0 )
				saveFileDialog( _outText ); // SaveAs: Ctrl+Shift+S
			else
				return false;
			break;
		case 0x54: // T - text mode
			viewFileCore( false, StringUtils::ANSI );
			break;
		case 0x57: // W - word wrap
			swapActiveEdit();
			break;
		default:
		//	PrintDebug("button: 0x%02X", wParam);
			return false;
		}

		return true;
	}

	void CFileViewer::handleScroll( WORD pos, WORD request )
	{
		std::streamoff streamOffset = _fileStreamPos;

		switch( request )
		{
		case WM_MOUSEWHEEL:
			if( (short)pos < 0 )
				streamOffset += 3 * BYTES_ROW; // mouse wheel down
			else if( (short)pos > 0 )
				streamOffset -= 3 * BYTES_ROW; // mouse wheel up
			break;

		case SB_TOP:
			streamOffset = 0ll;
			break;

		case SB_BOTTOM:
			streamOffset = _fileSize.QuadPart - 1;
			break;

		case SB_LINEUP:
			streamOffset -= BYTES_ROW;
			break;

		case SB_PAGEUP:
			streamOffset -= ( ( _linesCountPerPage - 1 ) * BYTES_ROW );
			break;

		case SB_LINEDOWN:
			streamOffset += BYTES_ROW;
			break;

		case SB_PAGEDOWN:
			streamOffset += ( ( _linesCountPerPage - 1 ) * BYTES_ROW );
			break;

		//case SB_THUMBPOSITION: // The user has dragged the scroll box(thumb) and released the mouse button.The pos indicates the position of the scroll box at the end of the drag operation.
		//	PrintDebug( "pos: 0x%04X", pos );
		//	streamOffset = pos;
		//	break;

		case SB_THUMBTRACK: // The user is dragging the scroll box.This message is sent repeatedly until the user releases the mouse button.The pos indicates the position that the scroll box has been dragged to.
		{
			SCROLLINFO si;
			si.cbSize = sizeof( si );
			si.fMask = SIF_ALL;
			GetScrollInfo( _hActiveEdit, SB_VERT, &si );

		//	PrintDebug( "Track pos: 0x%04X (0x%04X)", si.nTrackPos, pos );
			streamOffset = (std::streamoff)si.nTrackPos * _lineLength;
			streamOffset = streamOffset / BYTES_ROW * BYTES_ROW;
			PrintDebug("Pos: 0x%08X", streamOffset);
			break;
		}
		case SB_ENDSCROLL:
			PrintDebug("Ends scroll.");
			break;
		}

		scrollToPos( streamOffset );
	}

	void CFileViewer::scrollToPos( std::streamoff pos, bool refresh )
	{
		if( ( ( _linesCountPerPage - 1 ) * BYTES_ROW ) >= _fileSize.QuadPart / BYTES_ROW * BYTES_ROW )
			return;

		if( pos + ( ( _linesCountPerPage - 1 ) * BYTES_ROW ) > _fileSize.QuadPart - 1 )
		{
			if( _fileSize.QuadPart / BYTES_ROW * BYTES_ROW == _fileSize.QuadPart )
				pos = _fileSize.QuadPart - _linesCountPerPage * BYTES_ROW;
			else if( _fileSize.QuadPart > ( ( _linesCountPerPage - 1 ) * BYTES_ROW ) )
				pos = ( _fileSize.QuadPart / BYTES_ROW * BYTES_ROW ) - ( ( _linesCountPerPage - 1 ) * BYTES_ROW );
		}

		if( pos < 0ll )
			pos = 0ll;

		if( _fileStreamPos != pos || refresh )
		{
			_fileStreamPos = pos;

			readFileHex( _fileStream );
			SetWindowText( _hActiveEdit, _outText.c_str() );

			updateScrollBar( SIF_RANGE | SIF_POS | ( refresh ? SIF_PAGE : 0 ) );
		}
	}

	void CFileViewer::openFileDialog()
	{
		TCHAR szFileName[MAX_PATH] = { 0 };
		auto targetDir = PathUtils::stripFileName( _fileName );

		OPENFILENAME ofn = { 0 };
		ofn.lStructSize = sizeof( ofn );
		ofn.hwndOwner = _hDlg;
		ofn.lpstrInitialDir = targetDir.c_str();
		ofn.lpstrFilter = L"All Files (*.*)\0*.*\0";
		ofn.lpstrDefExt = L"*.*";
		ofn.lpstrFile = szFileName;
		ofn.nMaxFile = MAX_PATH;
		ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY;

		if( GetOpenFileName( &ofn ) )
		{
			viewFile( PathUtils::stripFileName( szFileName ), PathUtils::stripPath( szFileName ) );
		}
	}

	void CFileViewer::saveFileDialog( const std::wstring& text )
	{
		CComPtr<IFileSaveDialog> pDlg;
		HRESULT hr = pDlg.CoCreateInstance( __uuidof( FileSaveDialog ) );

		if( SUCCEEDED( hr ) )
		{
			COMDLG_FILTERSPEC fileTypes[] = {
				{ L"Text Files (*.txt)", L"*.txt" },
				{ L"All Files (*.*)", L"*.*" }
			};

			pDlg->SetFileTypes( _countof( fileTypes ), fileTypes );

			FILEOPENDIALOGOPTIONS pfos;
			pDlg->GetOptions( &pfos );
			pDlg->SetOptions( pfos | FOS_OVERWRITEPROMPT );

			CComPtr<IShellItem> pFolder;
			auto dirName = PathUtils::stripFileName( _fileName );
			hr = SHCreateItemFromParsingName( dirName.c_str(), nullptr, IID_PPV_ARGS( &pFolder ) );
			pDlg->SetFolder( pFolder );

			auto fileName = PathUtils::stripPath( _fileName );
			pDlg->SetFileName( fileName.c_str() );
			pDlg->SetDefaultExtension( L"txt" );

			// get an IFileDialogCustomize interface and add text encoding combo-box
			CComQIPtr<IFileDialogCustomize> pfdc = pDlg;
			if( pfdc )
			{
				// setting label to combo-box only works through visual groups
				pfdc->StartVisualGroup( IDC_SAVEDIALOG_COMBO - 1, L"&Encoding:" );
				pfdc->AddComboBox( IDC_SAVEDIALOG_COMBO );
				pfdc->EndVisualGroup();
				pfdc->AddControlItem( IDC_SAVEDIALOG_COMBO, IDM_VIEWER_ENCODING_ANSI, L"ANSI" );
				pfdc->AddControlItem( IDC_SAVEDIALOG_COMBO, IDM_VIEWER_ENCODING_UTF8, L"UTF-8" );
				pfdc->AddControlItem( IDC_SAVEDIALOG_COMBO, IDM_VIEWER_ENCODING_UTF8BOM, L"UTF-8 with BOM" );
				pfdc->AddControlItem( IDC_SAVEDIALOG_COMBO, IDM_VIEWER_ENCODING_UTF16LE, L"UTF-16 LE" );
				pfdc->AddControlItem( IDC_SAVEDIALOG_COMBO, IDM_VIEWER_ENCODING_UTF16BE, L"UTF-16 BE" );
				pfdc->AddControlItem( IDC_SAVEDIALOG_COMBO, IDM_VIEWER_ENCODING_UTF32LE, L"UTF-32 LE" );
				pfdc->AddControlItem( IDC_SAVEDIALOG_COMBO, IDM_VIEWER_ENCODING_UTF32BE, L"UTF-32 BE" );
				pfdc->SetSelectedControlItem( IDC_SAVEDIALOG_COMBO, getEncodingId( _useEncoding, _fileBom ) );
			}

			hr = pDlg->Show( _hDlg ); // show the dialog
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
						if( pfdc )
						{
							DWORD dwEncodingId = 0;
							pfdc->GetSelectedControlItem( IDC_SAVEDIALOG_COMBO, &dwEncodingId );

							switch( dwEncodingId )
							{
							case IDM_VIEWER_ENCODING_ANSI:
								FsUtils::saveFileAnsi( pwsz, &text[0], text.length() );
								break;
							case IDM_VIEWER_ENCODING_UTF8:
								FsUtils::saveFileUtf8( pwsz, &text[0], text.length() );
								break;
							case IDM_VIEWER_ENCODING_UTF8BOM:
								FsUtils::saveFileUtf8( pwsz, &text[0], text.length(), true );
								break;
							case IDM_VIEWER_ENCODING_UTF16LE:
								FsUtils::saveFileUtf16( pwsz, &text[0], text.length() );
								break;
							case IDM_VIEWER_ENCODING_UTF16BE:
								FsUtils::saveFileUtf16( pwsz, &text[0], text.length(), true );
								break;
							case IDM_VIEWER_ENCODING_UTF32LE:
								FsUtils::saveFileUtf32( pwsz, &text[0], text.length() );
								break;
							case IDM_VIEWER_ENCODING_UTF32BE:
								FsUtils::saveFileUtf32( pwsz, &text[0], text.length(), true );
								break;
							}
						}
						else
							FsUtils::saveFileUtf8( pwsz, &text[0], text.length() );

						CoTaskMemFree( pwsz );
					}
				}
			}
		}
	}

	int CFileViewer::calcScrollRange()
	{
		//(int)_fileSize.QuadPart > INT_MAX ? 0 : 0;
		LONGLONG offset = _linesCountPerPage;

		if( _fileSize.QuadPart / BYTES_ROW * BYTES_ROW == _fileSize.QuadPart )
			offset = _fileSize.QuadPart;// - _linesCountPerPage * BYTES_ROW;
		else if( _fileSize.QuadPart > ( ( _linesCountPerPage - 1 ) * BYTES_ROW ) )
			offset = ( _fileSize.QuadPart / BYTES_ROW * BYTES_ROW )+1;// - ( ( _linesCountPerPage - 1 ) * BYTES_ROW );

		return (int)( offset / _lineLength );
	}

	int CFileViewer::calcScrollPos()
	{
		return (int)( _fileStreamPos / _lineLength );
	}

	void CFileViewer::updateScrollBar( UINT fMask )
	{
		if( _linesCountPerPage )
		{
			SCROLLINFO sci;
			sci.cbSize = sizeof( sci );
			sci.fMask = fMask;
			sci.nMin = 0;
			sci.nMax = calcScrollRange() + 22;
			sci.nPos = calcScrollPos();
			sci.nPage = _linesCountPerPage - 1;

			SetScrollInfo( _hActiveEdit, SB_VERT, &sci, TRUE );

			SCROLLINFO si = { 0 };
			si.cbSize = sizeof( si );
			si.fMask = SIF_ALL;
			GetScrollInfo( _hActiveEdit, SB_VERT, &si );

			PrintDebug("Range: %u(%u), Pos: %u(%u), Trackpos: %u", sci.nMax, si.nMax, sci.nPos, si.nPos, si.nTrackPos);
		}
	}

	void CFileViewer::handleMenuCommands( WORD menuId )
	{
		switch( menuId )
		{
		case IDM_VIEWER_FILE_OPEN:
			openFileDialog();
			break;
		case IDM_VIEWER_FILE_SAVE:
			// do nothing in viewer
			break;
		case IDM_VIEWER_FILE_SAVEAS:
			saveFileDialog( _outText );
			break;
		case IDM_VIEWER_FILE_REFRESH:
			viewFileCore( _isHex, _useEncoding );
			break;
		case IDM_VIEWER_FILE_EXIT:
			close(); // close dialog
			break;
		case IDM_VIEWER_EDIT_COPY:
			SendMessage( _hActiveEdit, WM_COPY, 0, 0 );
			break;
		case IDM_VIEWER_EDIT_COPYTOFILE:
			copyTextToFile();
			break;
		case IDM_VIEWER_EDIT_SELECTALL:
			SendMessage( _hActiveEdit, EM_SETSEL, 0, (LPARAM)-1 );
			break;
		case IDM_VIEWER_SEARCH_FIND:
			findTextDialog();
			break;
		case IDM_VIEWER_SEARCH_FINDNEXT:
			hiliteText( _findParams.reverse );
			break;
		case IDM_VIEWER_SEARCH_FINDPREVIOUS:
			hiliteText( !_findParams.reverse );
			break;
		case IDM_VIEWER_SEARCH_GOTO:
			gotoLineDialog();
			break;
		case IDM_VIEWER_VIEW_HEX:
			viewFileCore( true, _useEncoding );
			break;
		case IDM_VIEWER_VIEW_TEXT:
			viewFileCore( false, StringUtils::ANSI );
			break;
		case IDM_VIEWER_VIEW_ZOOM_IN:
			setZoom( 1 );
			break;
		case IDM_VIEWER_VIEW_ZOOM_OUT:
			setZoom( -1 );
			break;
		case IDM_VIEWER_VIEW_ZOOM_RESTORE:
			setZoom( 0 );
			break;
		case IDM_VIEWER_VIEW_WRAP:
			swapActiveEdit();
			break;
		case IDM_VIEWER_VIEW_STATUSBAR:
			showStatusBar();
			break;
		case IDM_VIEWER_ENCODING_AUTO:
			viewFileCore( false, StringUtils::NOBOM );
			break;
		case IDM_VIEWER_ENCODING_UTF8:
			viewFileCore( false, StringUtils::UTF8 );
			break;
		case IDM_VIEWER_ENCODING_UTF16LE:
			viewFileCore( false, StringUtils::UTF16LE );
			break;
		case IDM_VIEWER_ENCODING_UTF16BE:
			viewFileCore( false, StringUtils::UTF16BE );
			break;
		case IDM_VIEWER_ENCODING_UTF32LE:
			viewFileCore( false, StringUtils::UTF32LE );
			break;
		case IDM_VIEWER_ENCODING_UTF32BE:
			viewFileCore( false, StringUtils::UTF32BE );
			break;
		case IDM_VIEWER_ENCODING_ANSI:
			viewFileCore( false, StringUtils::ANSI );
			break;
		case IDM_VIEWER_ENCODING_CP1250:
		case IDM_VIEWER_ENCODING_ISO88592:
		case IDM_VIEWER_ENCODING_CP1252:
		case IDM_VIEWER_ENCODING_ISO88591:
		case IDM_VIEWER_ENCODING_CP1251:
		case IDM_VIEWER_ENCODING_ISO88595:
		case IDM_VIEWER_ENCODING_CP866:
		case IDM_VIEWER_ENCODING_KOI8:
		case IDM_VIEWER_ENCODING_IBM437:
			viewFileAnsi( menuId );
			break;
		}
	}

	void CFileViewer::updateMenuEncoding()
	{
		// update encodings state
		HMENU hMenu4 = GetSubMenu( _hMenu, 4 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_CP1250, _useCodePage == 1250 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_CP1251, _useCodePage == 1251 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_CP1252, _useCodePage == 1252 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_ISO88591, _useCodePage == 28591 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_ISO88592, _useCodePage == 28592 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_ISO88595, _useCodePage == 28595 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_CP866, _useCodePage == 866 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_KOI8, _useCodePage == 20866 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_IBM437, _useCodePage == 437 );

		// update word wrap menu item state
		HMENU hMenu3 = GetSubMenu( _hMenu, 3 );
		MenuUtils::enableItem( hMenu3, IDM_VIEWER_VIEW_WRAP, !_isHex );

		// update menu radio buttons state
		CheckMenuRadioItem( hMenu3, IDM_VIEWER_VIEW_HEX, IDM_VIEWER_VIEW_TEXT,
			_isHex ? IDM_VIEWER_VIEW_HEX : IDM_VIEWER_VIEW_TEXT, MF_BYCOMMAND | MF_CHECKED );

		CheckMenuRadioItem( hMenu4, IDM_VIEWER_ENCODING_AUTO, IDM_VIEWER_ENCODING_ANSI,
			getEncodingId( _useEncoding ), MF_BYCOMMAND | MF_CHECKED );
	}

	void CFileViewer::viewFileAnsi( WORD menuId )
	{
		switch( menuId )
		{
		case IDM_VIEWER_ENCODING_CP1250:
			_useCodePage = 1250;
			break;
		case IDM_VIEWER_ENCODING_ISO88592:
			_useCodePage = 28592;
			break;
		case IDM_VIEWER_ENCODING_CP1252:
			_useCodePage = 1252;
			break;
		case IDM_VIEWER_ENCODING_ISO88591:
			_useCodePage = 28591;
			break;
		case IDM_VIEWER_ENCODING_CP1251:
			_useCodePage = 1251;
			break;
		case IDM_VIEWER_ENCODING_ISO88595:
			_useCodePage = 28595;
			break;
		case IDM_VIEWER_ENCODING_CP866:
			_useCodePage = 866;
			break;
		case IDM_VIEWER_ENCODING_KOI8:
			_useCodePage = 20866;
			break;
		case IDM_VIEWER_ENCODING_IBM437:
			_useCodePage = 437;
			break;
		}

		viewFileCore( _isHex, StringUtils::ANSI );
	}

	INT_PTR CALLBACK CFileViewer::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_COMMAND:
			switch( HIWORD( wParam ) )
			{
			case EN_ERRSPACE:
				PrintDebug("EN_ERRSPACE");
				break;
			case EN_SETFOCUS:
				HideCaret( reinterpret_cast<HWND>( lParam ) );
				break;
			case EN_VSCROLL:
				//PrintDebug("wpar: 0x%04X, lpar: 0x%04X", wParam, lParam);
				if( _isHex && _fileStream.is_open() )
					updateScrollBar( SIF_POS | SIF_RANGE );
				return (INT_PTR)false;
			default:
				handleMenuCommands( LOWORD( wParam ) );
				break;
			}
			break;

		case WM_MENUSELECT:
			if( LOWORD( wParam ) == 1 /* submenu Edit */ )
			{
				// disable menu when there is no text selected
				DWORD idxBeg = 0, idxEnd = 0;
				SendMessage( _hActiveEdit, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );
				MenuUtils::enableItem( GetSubMenu( _hMenu, 1 ), IDM_VIEWER_EDIT_COPY, ( idxBeg != idxEnd ) );
			}
			else if( LOWORD( wParam ) == 3 /* submenu View */ )
			{
				// dis/enable zoom submenu items
				MenuUtils::enableItem( GetSubMenu( _hMenu, 3 ), IDM_VIEWER_VIEW_ZOOM_IN, ( _currentZoom < 500 ) );
				MenuUtils::enableItem( GetSubMenu( _hMenu, 3 ), IDM_VIEWER_VIEW_ZOOM_OUT, ( _currentZoom > 10 ) );
			}
			break;

		case UM_REPORTMSGBOX:
			_pFindDlg->messageBoxClosed();
			break;

		case UM_FINDLGNOTIFY:
			if( wParam == IDE_FINDTEXT_TEXTFIND ) // from CFindText dialog
			{
				switch( lParam )
				{
				case 0: // CFindText dialog is being destroyed
					_pFindDlg = nullptr;
					break;
				case 5: // search previous
				case 4: // search next
					hiliteText( lParam == 4 ? _findParams.reverse : !_findParams.reverse );
					break;
				case 1: // perform search text
					_textToFind = _pFindDlg->result();
					_findParams = _pFindDlg->getParams();

					searchText();
					break;
				default:
					PrintDebug("unknown command: %d", lParam);
					break;
				}
			}
			break;

		case UM_FINDTEXTDONE:
			if( wParam == 2 ) // hex text found
			{
				SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 0, SBT_POPOUT ), (LPARAM)L"" );
				_findFromOffset = lParam + 1;
				scrollToPos( lParam / BYTES_ROW * BYTES_ROW );
				auto byteOffset = findOffsetInEditBox( lParam );
				if( byteOffset != -1 )
					SendMessage( _hActiveEdit, EM_SETSEL, byteOffset, byteOffset + 1 );
			}
			else
			{
				SetCursor( NULL );
				if( wParam == 0 ) // hex search worker finished without success
				{
					SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 0, SBT_POPOUT ), (LPARAM)L"Not found" );
					std::wstringstream sstr;
					sstr << L"Cannot find (" << StringUtils::encodingToStr( _useEncoding ) << ") \"" << _textToFind << L"\".";
					_pFindDlg->showMessageBox( sstr.str() );
				}
			}
			break;

		case UM_READERNOTIFY:
			if( wParam )
			{
				if( wParam == FC_ARCHDONEOK )
				{
					std::wstring result = _fileName;

					if( _spPanel )
						result = _spPanel->getDataManager()->getDataProvider()->getResult();

					if( _upDataProvider )
						result = _upDataProvider->getResult();

					// TODO: temporary solution
					if( !_isHex && ViewerType::getType( result ) == ViewerType::EViewType::FMT_PALM )
					{
						_upDataProvider = std::make_unique<CPalmDataProvider>();
						_upDataProvider->readToFile( result, _spPanel ? _spPanel : nullptr, _hDlg );
					}
					else
					{
						if( result != _fileName )
							_tempFileName = result;

						if( viewFileCore( _isHex, _useEncoding ) )
						{
							if( !_textToFind.empty() )
								searchText(); // hilite text
						}
						else
							close(); // close dialog
					}
				}
			}
			else {
				std::wstringstream sstr;
				sstr << L"Error opening \"" << ( _tempFileName.empty() ? _fileName : _tempFileName ) << L"\" file.";
				if( _upDataProvider ) sstr << L"\r\n" << _upDataProvider->getErrorMessage();
				MessageBox( _hDlg, sstr.str().c_str(), L"View File - Error", MB_OK | MB_ICONEXCLAMATION );
				close(); // close dialog
			}
			break;

		case WM_DROPFILES:
			handleDropFiles( (HDROP)wParam );
			break;

		case WM_SIZE:
			updateLayout( LOWORD( lParam ), HIWORD( lParam ) );
			break;
		}

		return (INT_PTR)false;
	}

	void CFileViewer::updateLayout( int width, int height )
	{
		int statusBarHeight = 0;

		// get status-bar height when visible
		if( IsWindowVisible( _hStatusBar ) )
		{
			int borderWidths[3];
			RECT rct;

			if( SendMessage( _hStatusBar, SB_GETRECT, 0, (LPARAM)&rct ) &&
				SendMessage( _hStatusBar, SB_GETBORDERS, 0, (LPARAM)borderWidths ) )
			{
				statusBarHeight = rct.bottom - rct.top + borderWidths[1];
			}

			// initialize status-bar parts
			const int numParts = 4;
			const int partWidths[numParts] = { 270, 220, 120, 0 };

			// calculate right edges of parts according to current width
			int statusBarParts[numParts] = {
				width > partWidths[0] ? width - partWidths[0] : 1,
				width > partWidths[1] ? width - partWidths[1] : 1,
				width > partWidths[2] ? width - partWidths[2] : 1,
				width > partWidths[3] ? width - partWidths[3] : 1 };

			// update status-bar window placement
			SendMessage( _hStatusBar, SB_SETPARTS, numParts, (LPARAM)statusBarParts );
			SendMessage( _hStatusBar, WM_SIZE, 0, 0 );
		}

		// update layout accordingly
		MoveWindow( _hActiveEdit, 0, 0, width, height - statusBarHeight, TRUE );

		if( _isHex && _fileStream.is_open() )
		{
			auto cnt = MiscUtils::countVisibleLines( _hActiveEdit );
			if( cnt != _linesCountPerPage )
			{
				_linesCountPerPage = cnt;
				scrollToPos( _fileStreamPos, true );
			}
			else
				updateScrollBar( SIF_RANGE | SIF_POS );
		}
	}
}
