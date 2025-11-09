#include "stdafx.h"

#include "Commander.h"
#include "FileFinder.h"
#include "FileEditor.h"
#include "ViewerTypes.h"
#include "TextFileReader.h"

#include "IconUtils.h"
#include "ListViewUtils.h"
#include "ShellUtils.h"
#include "StringUtils.h"
#include "MiscUtils.h"

#define FC_FINDITEMDIR    101

namespace Commander
{
	void CFileFinder::onInit()
	{
		SendMessage( _hDlg, WM_SETICON, ICON_SMALL, (LPARAM)IconUtils::getStock( SIID_AUTOLIST ) );
		SendMessage( _hDlg, WM_SETICON, ICON_BIG, (LPARAM)IconUtils::getStock( SIID_AUTOLIST, true ) );

		// create status-bar
		_hStatusBar = CreateWindowEx( 0, STATUSCLASSNAME, (PCTSTR)NULL, SBARS_SIZEGRIP | WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
			0, 0, 0, 0, _hDlg, (HMENU)102, FCS::inst().getFcInstance(), NULL );

		// limit text to long max path (32000 characters)
		SendDlgItemMessage( _hDlg, IDC_FIND_NAME, EM_SETLIMITTEXT, (WPARAM)MAX_WPATH, 0 );
		SendDlgItemMessage( _hDlg, IDC_FIND_INDIR, EM_SETLIMITTEXT, (WPARAM)MAX_WPATH, 0 );
		SetDlgItemText( _hDlg, IDC_FIND_NAME, L"*.*" );
		SetDlgItemText( _hDlg, IDC_FIND_INDIR, FCS::inst().getApp().getActivePanel().getActiveTab()->getDataManager()->getCurrentDirectory().c_str() );
		SetWindowText( _hStatusBar, L"Ready" );

		_dropLocationText = L"Find results";

		CheckRadioButton( _hDlg, IDC_FIND_RB_AUTO, IDC_FIND_RB_UTF32, IDC_FIND_RB_AUTO );
		CheckDlgButton( _hDlg, IDC_FIND_SEARCHSUBDIR, BST_CHECKED );

		showSearchGroup( false );

		LvUtils::setExtStyle( GetDlgItem( _hDlg, IDC_FIND_RESULT ), LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT );
		LvUtils::setImageList( GetDlgItem( _hDlg, IDC_FIND_RESULT ), FCS::inst().getApp().getSystemImageList(), LVSIL_SMALL );

		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_FIND_RESULT ), L"Name" );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_FIND_RESULT ), L"Path" );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_FIND_RESULT ), L"Size", 64, false );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_FIND_RESULT ), L"Date", 68, false );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_FIND_RESULT ), L"Time", 56, false );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_FIND_RESULT ), L"Attr", 32, false );

		MiscUtils::centerOnWindow( FCS::inst().getFcWindow(), _hDlg );

		// init drag & drop helper
		InitializeDragDropHelper( GetDlgItem( _hDlg, IDC_FIND_RESULT ), DROPIMAGE_NONE, L"" );

		_line.reserve( 0x4000 );
		_strOut.reserve( 0x2000 );

		show(); // show dialog

		_worker.init( [this] { return _findFilesCore(); }, _hDlg, UM_STATUSNOTIFY );
	}

	bool CFileFinder::onOk()
	{
		if( _worker.isRunning() )
			stopFindFiles();
		else
			startFindFiles();

		return false;
	}

	bool CFileFinder::onClose()
	{
		show( SW_HIDE ); // hide dialog

		KillTimer( _hDlg, FC_TIMER_GUI_ID );

		_worker.stop();

		_foundItems.clear();
		_cachedData.clear();

		return true;//MessageBox( NULL, L"Really exit?", L"Question", MB_YESNO | MB_ICONQUESTION ) == IDYES;
	}

	void CFileFinder::onDestroy()
	{
		TerminateDragDropHelper();

		DestroyIcon( (HICON)SendMessage( _hDlg, WM_GETICON, ICON_SMALL, 0 ) );
		DestroyIcon( (HICON)SendMessage( _hDlg, WM_GETICON, ICON_BIG, 0 ) );
	}

	//
	// Enable/disable search group windows
	//
	void CFileFinder::enableSearchGroup( bool enable )
	{
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_CONTAINCAPTION ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_CONTENT ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_RB_ANSI ), enable );

		if( !enable || ( enable && IsDlgButtonChecked( _hDlg, IDC_FIND_REGEXMODE ) == BST_UNCHECKED ) )
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_HEXMODE ), enable );

		if( !enable || ( enable && IsDlgButtonChecked( _hDlg, IDC_FIND_HEXMODE ) == BST_UNCHECKED ) )
		{
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_RB_AUTO ), enable );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_RB_UTF8 ), enable );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_RB_UTF16 ), enable );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_RB_UTF32 ), enable );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_MATCHCASE ), enable );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_WHOLEWORD ), enable );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_REGEXMODE ), enable );
		}
	}

	//
	// Enable/disable search group windows
	//
	void CFileFinder::showSearchGroup( bool show )
	{
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_CONTAINCAPTION ), show );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_CONTENT ), show );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_RB_AUTO ), show );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_RB_ANSI ), show );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_RB_UTF8 ), show );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_RB_UTF16 ), show );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_RB_UTF32 ), show );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_MATCHCASE ), show );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_WHOLEWORD ), show );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_HEXMODE ), show );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_REGEXMODE ), show );

		RECT rct;
		GetClientRect( _hDlg, &rct );
		updateLayout( rct.right - rct.left, rct.bottom - rct.top );
	}

	//
	// Enable/disable UI elements when hex-mode is de/selected
	//
	void CFileFinder::enableHexMode( bool enable )
	{
		if( enable )
		{
			_encodingTemp = MiscUtils::getCheckedRadio( _hDlg, { IDC_FIND_RB_AUTO, IDC_FIND_RB_ANSI, IDC_FIND_RB_UTF8, IDC_FIND_RB_UTF16, IDC_FIND_RB_UTF32 } );
			CheckRadioButton( _hDlg, IDC_FIND_RB_AUTO, IDC_FIND_RB_UTF32, IDC_FIND_RB_ANSI );
			CheckDlgButton( _hDlg, IDC_FIND_MATCHCASE, BST_CHECKED );
			CheckDlgButton( _hDlg, IDC_FIND_WHOLEWORD, BST_UNCHECKED );

			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_RB_AUTO ), false );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_RB_UTF8 ), false );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_RB_UTF16 ), false );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_RB_UTF32 ), false );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_MATCHCASE ), false );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_WHOLEWORD ), false );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_REGEXMODE ), false );
		}
		else
		{
			CheckRadioButton( _hDlg, IDC_FIND_RB_AUTO, IDC_FIND_RB_UTF32, _encodingTemp );
			enableSearchGroup();
		}
	}

	//
	// Get encoding value, use UTF-8 when no BOM detected
	//
	UINT getTextEncoding( StringUtils::EUtfBom bom )
	{
		UINT textEncoding = 0;

		switch( bom )
		{
		case StringUtils::UTF16BE:
		case StringUtils::UTF16LE:
			textEncoding = IDC_FIND_RB_UTF16;
			break;
		case StringUtils::UTF32BE:
		case StringUtils::UTF32LE:
			textEncoding = IDC_FIND_RB_UTF32;
			break;
		case StringUtils::UTF8:
		case StringUtils::NOBOM:
		default:
			textEncoding = IDC_FIND_RB_UTF8;
			break;
		}

		return textEncoding;
	}

	//
	// Find case-sensitive string in a memory buffer, even partial
	//
	const char *findCaseSensitive( const char *buf, std::streamsize len, const std::string& str, bool word, bool *bounds, std::size_t *pos )
	{
		const char *p = buf;

		if( *pos != std::string::npos )
		{
			// check for whole-words
			if( *pos == str.length() && !StringUtils::isAlphaNum( *p ) )
				return p;

			// continue matching the rest from previous call
			if( *pos < str.length() )
			{
				size_t offset = str.length() - *pos;

				if( offset <= len && !memcmp( p, &str[*pos], offset ) )
				{
					if( !word || offset == len || ( word && ( !bounds[1] || !StringUtils::isAlphaNum( *( p + offset ) ) ) ) )
						return p;
				}
			}

			*pos = std::string::npos;
		}

		while( ( ( p - buf ) < len ) && ( p = reinterpret_cast<const char*>( memchr( p, str[0], static_cast<size_t>( len - ( p - buf ) ) ) ) ) )
		{
			size_t remainder = (size_t)( len - ( p - buf ) );

			if( remainder < str.length() )
			{
				// partial text found
				if( !memcmp( p, &str[0], remainder ) )
				{
					if( !word || ( word && ( p == buf || !bounds[0] || !StringUtils::isAlphaNum( *( p - 1 ) ) ) ) )
					{
						*pos = remainder;
						break;
					}
				}
			}
			else if( !memcmp( p, &str[0], str.length() ) )
			{
				// complete text found
				if( remainder == str.length() )
				{
					if( word )
					{
						if( p == buf || !bounds[0] || !StringUtils::isAlphaNum( *( p - 1 ) ) )
						{
							if( !bounds[1] )
								return p;

							// check for whole-words in the next call
							*pos = remainder;
							break;
						}
					}
					else
						return p;
				}
				else if( !word || ( word && ( ( p == buf || !bounds[0] || !StringUtils::isAlphaNum( *( p - 1 ) ) ) && ( !bounds[1] || !StringUtils::isAlphaNum( *( p + str.length() ) ) ) ) ) )
					return p;
			}

			++p;
		}

		return nullptr;
	}

	//
	// Find case-insensitive text in a memory buffer
	// NOTE: This function relies on 'str' to be an uppercase string
	const char *findCaseInsensitive( const char *buf, std::streamsize len, const std::string& str, bool word, bool *bounds, std::size_t *pos )
	{
		const char *p = buf;

		if( *pos != std::string::npos )
		{
			// check for whole-words
			if( *pos == str.length() && !StringUtils::isAlphaNum( *p ) )
				return p;

			// continue matching the rest from previous call
			if( *pos < str.length() )
			{
				size_t offset = str.length() - *pos;

				if( offset <= len && !_memicmp( p, &str[*pos], offset ) )
				{
					if( !word || offset == len || ( word && ( !bounds[1] || !StringUtils::isAlphaNum( *( p + offset ) ) ) ) )
						return p;
				}
			}

			*pos = std::string::npos;
		}

		char cLower = StringUtils::convertChar2Lwr( str[0] );

		auto p1 = reinterpret_cast<const char*>( memchr( p, str[0], static_cast<size_t>( len ) ) );
		auto p2 = reinterpret_cast<const char*>( memchr( p, cLower, static_cast<size_t>( len ) ) );

		if( !p1 || !p2 )
			p = max( p1, p2 );
		else
			p = min( p1, p2 );

		while( p && ( p - buf ) < len )
		{
			size_t remainder = (size_t)( len - ( p - buf ) );

			if( remainder < str.length() )
			{
				// partial text found
				if( !_memicmp( p, &str[0], remainder ) )
				{
					if( !word || ( word && ( p == buf || !bounds[0] || !StringUtils::isAlphaNum( *( p - 1 ) ) ) ) )
					{
						*pos = remainder;
						break;
					}
				}
			}
			else if( !_memicmp( p, &str[0], str.length() ) )
			{
				// complete text found
				if( remainder == str.length() )
				{
					if( word )
					{
						if( p == buf || !bounds[0] || !StringUtils::isAlphaNum( *( p - 1 ) ) )
						{
							if( !bounds[1] )
								return p;

							// check for whole-words in the next call
							*pos = remainder;
							break;
						}
					}
					else
						return p;
				}
				else if( !word || ( word && ( ( p == buf || !bounds[0] || !StringUtils::isAlphaNum( *( p - 1 ) ) ) && ( !bounds[1] || !StringUtils::isAlphaNum( *( p + str.length() ) ) ) ) ) )
					return p;
			}

			if( p == p1 )
				p1 = reinterpret_cast<const char*>( memchr( p + 1, str[0], static_cast<size_t>( remainder - 1 ) ) );
			else
				p2 = reinterpret_cast<const char*>( memchr( p + 1, cLower, static_cast<size_t>( remainder - 1 ) ) );

			if( !p1 || !p2 )
				p = max( p1, p2 );
			else
				p = min( p1, p2 );
		}

		return nullptr;
	}

	//
	// Find text in given file - ANSI
	//
	bool CFileFinder::findTextAnsi( std::ifstream& fStream, const std::string& textToFind, bool words )
	{
		if( fStream.is_open() )
		{
			std::streamsize read = 0ll;
			std::size_t pos = std::string::npos;

			while( _worker.isRunning() && ( read = fStream.read( _buf, sizeof( _buf ) ).gcount() ) )
			{
				if( _matchCase && findCaseSensitive( _buf, read, textToFind, words, _srcBounds, &pos ) )
					return true;
				else if( !_matchCase && findCaseInsensitive( _buf, read, textToFind, words, _srcBounds, &pos ) )
					return true;
			}

			// check if there was a matched text at the end of the buffer/file
			if( words && pos == textToFind.length() )
				return true;
		}

		return false;
	}

	//
	// Find text in given unicode wstring
	//
	bool CFileFinder::findTextInUnicodeString( const std::wstring& textToFind, bool words )
	{
		if( _regexMode ) // TODO: multiline string match
		{
			if( words )
			{
				for( auto it = std::wsregex_iterator( _strOut.begin(), _strOut.end(), *_regexW ); _worker.isRunning() && it != std::wsregex_iterator(); ++it )
				{
					auto idx = it->position();
					auto len = it->length();

					if( ( idx == 0 || !StringUtils::isAlphaNum( _strOut[idx] ) || !StringUtils::isAlphaNum( _strOut[idx - 1] ) ) &&
						( _strOut.length() == idx + len || !StringUtils::isAlphaNum( _strOut[idx + len - 1] ) || !StringUtils::isAlphaNum( _strOut[idx + len] ) ) )
					{
						return true;
					}
				}
			}
			else
			{
				std::wsmatch match;
				std::regex_search( _strOut, match, *_regexW );

				if( !match.empty() )
					return true;
			}
		}
		else
		{
			if( StringUtils::findStr( &_strOut[0], _strOut.length(), textToFind, _matchCase, words, _srcBounds ) )
				return true;
		}

		return false;
	}

	//
	// Find text in a file stream with given line-feed char - Unicode (UTF8/16/32)
	//
	template<typename T>
	bool CFileFinder::findTextUnicode( T lf, std::ifstream& fStream, const std::wstring& textToFind, bool words, StringUtils::EUtfBom& bom )
	{
		std::streamsize read = 0ll;
		std::streamoff offset = 0ll;

		T *buf = (T*)_buf;
		T *p = nullptr;

		while( _worker.isRunning() && ( read = fStream.read( _buf, sizeof( _buf ) ).gcount() ) )
		{
			_ASSERTE( ( read % sizeof( T ) ) == 0 );

			read /= sizeof( T );
			offset = 0ll;
			p = buf;

			while( _worker.isRunning() && offset < read && ( ( p = std::find( p, buf + read, lf ) ) != ( buf + read ) ) )
			{
				_line.append( (char*)( buf + offset ), (char*)++p ); // include EOL char

				if( StringUtils::convert2Utf16( _line, _strOut, bom ) )
				{
					if( findTextInUnicodeString( textToFind, words ) )
						return true;
				}

				offset = ( p - buf );
				_line.clear();
			}

			if( offset < read )
				_line.append( (char*)( buf + offset ), (char*)( buf + read ) );
		}

		if( _worker.isRunning() && !_line.empty() && StringUtils::convert2Utf16( _line, _strOut, bom ) )
		{
			if( findTextInUnicodeString( textToFind, words ) )
				return true;
		}

		return false;
	}

	//
	// Find text in file using Regular Expressions - ANSI
	// TODO: multiline string match
	bool CFileFinder::findTextAnsiRegex( std::ifstream& fStream, const std::string& textToFind, bool words )
	{
		_ASSERTE( _regexA != nullptr );

		CTextFileReader reader( fStream, &_worker, StringUtils::ANSI, CP_ACP, CTextFileReader::EOptions::skipAnalyze );

		std::string& str = reader.getLineRefA();

		while( _worker.isRunning() && reader.readLine() == ERROR_SUCCESS )
		{
			if( words )
			{
				for( auto it = std::sregex_iterator( str.begin(), str.end(), *_regexA ); _worker.isRunning() && it != std::sregex_iterator(); ++it )
				{
					auto idx = it->position();
					auto len = it->length();

					if( ( idx == 0 || !StringUtils::isAlphaNum( str[idx] ) || !StringUtils::isAlphaNum( str[idx - 1] ) ) &&
						( str.length() == idx + len || !StringUtils::isAlphaNum( str[idx + len - 1] ) || !StringUtils::isAlphaNum( str[idx + len] ) ) )
					{
						return true;
					}
				}
			}
			else
			{
				std::smatch match;
				std::regex_search( reader.getLineRefA(), match, *_regexA );

				if( !match.empty() )
					return true;
			}
		}

		return false;
	}

	//
	// Find text in given file
	//
	bool CFileFinder::findTextInFile( const std::wstring& fileName, StringUtils::EUtfBom& bom )
	{
		// open file as binary stream
		std::ifstream file( fileName.c_str(), std::ifstream::binary );

		// TODO: add options to skip binary files
		/*CTextFileReader reader( file, &_worker );

		if( reader.isText() )
		{
			while( _worker.isRunning() && reader.readLine() == ERROR_SUCCESS )
			{
				if( StringUtils::findCaseInsensitive( reader.getLineRef(), _findTextW ) != std::wstring::npos )
					return true;
			}
		}

		return false;*/

		int bomLen = 0;

		if( !_hexMode )
			bom = FsUtils::readByteOrderMarker( file, bomLen );

		// disable whole words matching when there are non-alpha-numeric bounds in the source string
		bool wholeWords = _wholeWords;

		if( !_srcBounds[0] && !_srcBounds[1] )
			wholeWords = false;

		// try ANSI system code-page first when Auto has been selected and no BOM detected
		if( _hexMode || ( _encoding == IDC_FIND_RB_AUTO && bom == StringUtils::NOBOM && !_findTextA.empty() ) || _encoding == IDC_FIND_RB_ANSI )
		{
			if( ( !_regexMode && findTextAnsi( file, _findTextA, wholeWords ) )
				|| _regexMode && findTextAnsiRegex( file, _findTextA, wholeWords ) )
			{
				bom = StringUtils::ANSI;
				return true;
			}
		}

		if( _encoding != IDC_FIND_RB_ANSI )
		{
			// try according to BOM (UTF-8 when no BOM detected) on Auto, or according to user selection
			auto textEncoding = ( _encoding != IDC_FIND_RB_AUTO ) ? _encoding : getTextEncoding( bom );

			file.clear();

			// skip BOM
			file.seekg( bomLen, std::ios::beg );

			_line.clear();
			_strOut.clear();

			switch( textEncoding )
			{
			case IDC_FIND_RB_UTF8:
				bom = StringUtils::UTF8;

				return findTextUnicode<char>( 0x0A, file, _findTextW, wholeWords, bom );

			case IDC_FIND_RB_UTF16:
				if( bom != StringUtils::UTF16LE && bom != StringUtils::UTF16BE )
					bom = StringUtils::UTF16LE;

				return findTextUnicode<wchar_t>( ( bom == StringUtils::UTF16LE ? 0x0A : 0x0A00 ), file, _findTextW, wholeWords, bom );

			case IDC_FIND_RB_UTF32:
				if( bom != StringUtils::UTF32LE && bom != StringUtils::UTF32BE )
					bom = StringUtils::UTF32LE;

				return findTextUnicode<char32_t>( ( bom == StringUtils::UTF32LE ? 0x0A : 0x0A000000 ), file, _findTextW, wholeWords, bom );
			}
		}

		return false;
	}

	//
	// Find file or directory recursively on disk, wildcards supported
	//
	bool CFileFinder::findItems( const std::wstring& mask, const std::wstring& dirName )
	{
		auto pathFind = PathUtils::addDelimiter( dirName );

		WIN32_FIND_DATA wfd = { 0 };
		StringUtils::EUtfBom bom;

		// read file system structure
		HANDLE hFile = FindFirstFileEx( ( PathUtils::getExtendedPath( pathFind ) + L"*" ).c_str(), FindExInfoStandard, &wfd, FindExSearchNameMatch, NULL, FCS::inst().getApp().getFindFLags() );
		if( hFile != INVALID_HANDLE_VALUE )
		{
			do
			{
				if( ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
				{
					if( !( wfd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT )
						&& !PathUtils::isDirectoryDotted( wfd.cFileName )
						&& !PathUtils::isDirectoryDoubleDotted( wfd.cFileName ))
					{
						_worker.sendNotify( FC_FINDITEMDIR, (LPARAM)SysAllocString( pathFind.c_str() ) );

						// try to match directory name with mask and add to list-box
						if( _findTextW.empty() && PathUtils::matchFileNameWild( mask, StringUtils::convert2Lwr( wfd.cFileName ) ) )
						{
							storeFoundItem( wfd, pathFind );
						}

						if( _searchSubdirs )
							findItems( mask, pathFind + wfd.cFileName );
					}
				}
				else
				{
					// try to match filename with mask and add to list-box
					if( PathUtils::matchFileNameWild( mask, StringUtils::convert2Lwr( wfd.cFileName ) ) )
					{
						if( !_findTextW.empty() )
							_worker.sendNotify( FC_FINDITEMDIR, (LPARAM)SysAllocString( ( pathFind + wfd.cFileName ).c_str() ) );

						bom = StringUtils::NOBOM;

						if( _findTextW.empty() || findTextInFile( PathUtils::getExtendedPath( pathFind + wfd.cFileName ), bom ) )
						{
							storeFoundItem( wfd, pathFind, bom );
						}
					}

					// try to search in archives
					if( _searchArchives && ArchiveType::isKnownType( wfd.cFileName ) )
					{
						searchArchive( pathFind + wfd.cFileName, mask );
					}

					// TODO: ebooks, zipped (docx, etc.)
					if( _searchDocuments )
						;
				}
			} while( FindNextFile( hFile, &wfd ) && _worker.isRunning() );

			FindClose( hFile );
		}

		return true;
	}

	void CFileFinder::searchArchive( const std::wstring& archiveName, const std::wstring& mask )
	{
		auto upArchiver = ArchiveType::createArchiver( archiveName );
		upArchiver->init( archiveName, nullptr, nullptr, &_worker );

		_worker.sendNotify( FC_FINDITEMDIR, (LPARAM)SysAllocString( archiveName.c_str() ) );

		if( upArchiver && upArchiver->readEntries() )
		{
			for( const auto& entry : upArchiver->getEntriesRaw() )
			{
				if( !_worker.isRunning() )
					return;

				std::wstring entryName = PathUtils::stripPath( entry.wfd.cFileName );

				if( PathUtils::matchFileNameWild( mask, StringUtils::convert2Lwr( entryName ) ) )
				{
					if( !_findTextW.empty() )
					{
						if( IsDir( entry.wfd.dwFileAttributes ) )
							continue;

						std::wstring fullEntryName = PathUtils::addDelimiter( archiveName ) + entry.wfd.cFileName;

						_worker.sendNotify( FC_FINDITEMDIR, (LPARAM)SysAllocString( fullEntryName.c_str() ) );

						if( upArchiver->extractEntries( { fullEntryName }, FCS::inst().getTempPath() ) )
						{
							StringUtils::EUtfBom bom = StringUtils::NOBOM;
							std::wstring tempName = FCS::inst().getTempPath() + entryName;

							if( findTextInFile( tempName, bom ) )
								storeFoundItem( entry.wfd, archiveName, bom );

							DeleteFile( tempName.c_str() );
						}
					}
					else 
						storeFoundItem( entry.wfd, archiveName );
				}
			}
		}
	}

	void CFileFinder::storeFoundItem( const WIN32_FIND_DATA& wfd, const std::wstring& path, StringUtils::EUtfBom bom )
	{
		auto idx = CDiskReader::getIconIndex( wfd.cFileName, wfd.dwFileAttributes );
		auto itemData = std::make_shared<FindItemData>( PathUtils::stripDelimiter( path ), wfd, bom, idx );

		std::lock_guard<std::recursive_mutex> locker( _mutex );

		_foundItems.push_back( itemData );
	}

	//
	// Background worker procedure
	//
	bool CFileFinder::_findFilesCore()
	{
		ShellUtils::CComInitializer _com( COINIT_APARTMENTTHREADED );

		// The code page is set to the value returned by GetACP.
		setlocale( LC_ALL, "" ); // needed for _memicmp function

		ULONGLONG tickCount = GetTickCount64();

		bool ret = false;

		try {
			ret = findItems( PathUtils::preMatchWild( _findMask ), _findInDir );
		}
		catch( const std::regex_error& e )
		{
			CFileViewer::reportRegexError( _hDlg, e, _findTextW );

			ret = false;
		}

		_timeSpent = static_cast<DWORD>( GetTickCount64() - tickCount );

		return ret;
	}

	CFileFinder::ItemDataPtr CFileFinder::getItemData( int index )
	{
		std::lock_guard<std::recursive_mutex> locker( _mutex );

		ItemDataPtr itemData;
		if( index != -1 && index < _foundItems.size() )
		{
			itemData = _foundItems[index];
		}

		return itemData;
	}

	std::vector<std::wstring> CFileFinder::getSelectedItems( HWND hwListView )
	{
		std::vector<std::wstring> itemsOut;

		auto itemIndices = LvUtils::getSelectedItems( hwListView );
		for( const int& itemIdx : itemIndices )
		{
			auto itemData = getItemData( itemIdx );
			itemsOut.push_back( PathUtils::addDelimiter( itemData->path ) + itemData->wfd.cFileName );
		}

		return itemsOut;
	}

	void CFileFinder::onExecItem( HWND hwListView )
	{
		auto itemData = getItemData( LvUtils::getSelectedItem( hwListView ) );

		if( itemData )
		{
			ShellUtils::executeFile( itemData->wfd.cFileName, PathUtils::addDelimiter( itemData->path ) );
		}
	}

	void CFileFinder::onFocusItem( HWND hwListView )
	{
		auto itemData = getItemData( LvUtils::getSelectedItem( hwListView ) );

		if( itemData )
		{
			FCS::inst().getApp().getActivePanel().getActiveTab()->changeDirectory( itemData->path, itemData->wfd.cFileName );
		}
	}

	void CFileFinder::onViewItem( HWND hwListView, bool viewHex )
	{
		auto itemData = getItemData( LvUtils::getSelectedItem( hwListView ) );

		if( itemData && FsUtils::isPathFile( PathUtils::addDelimiter( itemData->path ) + itemData->wfd.cFileName ) )
		{
			if( !_findText.empty() )
			{
				// little hack when we want to highlight text in the file
				auto pViewer = CBaseDialog::createModeless<CFileViewer>( nullptr, MAKELPARAM( viewHex, itemData->bom ) );
				pViewer->setParams( { _matchCase, _wholeWords, _regexMode, _hexMode, false }, _findText );
				pViewer->viewFile( itemData->path, itemData->wfd.cFileName );
			}
			else
			{
				auto pViewer = ViewerType::createViewer( itemData->wfd.cFileName, viewHex );
				pViewer->viewFile( itemData->path, itemData->wfd.cFileName );
			}
		}
	}

	void CFileFinder::onEditItem( HWND hwListView )
	{
		auto itemData = getItemData( LvUtils::getSelectedItem( hwListView ) );

		if( itemData && FsUtils::isPathFile( PathUtils::addDelimiter( itemData->path ) + itemData->wfd.cFileName ) )
		{
			//ShellUtils::executeCommand( L"notepad", itemData->path, itemData->wfd.cFileName );
			auto pEditor = CBaseDialog::createModeless<CFileEditor>( nullptr, itemData->bom );

			if( !_findText.empty() )
				pEditor->setParams( { _matchCase, _wholeWords, _regexMode, _hexMode, false }, _findText );

			pEditor->viewFile( itemData->path, itemData->wfd.cFileName );
		}
	}

	void CFileFinder::onDeleteItems( HWND hwListView )
	{
		auto itemIndices = LvUtils::getSelectedItems( hwListView );
		if( !itemIndices.empty() )
		{
			std::wostringstream sstr; sstr << L"Delete " << itemIndices.size() << L" item(s)?";
			CTextInput::CParams params( L"Delete", sstr.str(), L".", false, false, 400 );
			auto result = MiscUtils::showTextInputBox( params, _hDlg );

			if( !result.empty() )
			{
				std::lock_guard<std::recursive_mutex> locker( _mutex );

				EnableWindow( hwListView, FALSE );

				FCS::inst().getFastCopyManager().start( EFcCommand::DeleteItems, getSelectedItems( hwListView ) );
				for( auto it = itemIndices.rbegin(); it != itemIndices.rend(); ++it )
				{
					LvUtils::deleteItem( hwListView, *it );
					_foundItems.erase( _foundItems.begin() + *it );
					_cachedData.clear();
				}

				EnableWindow( hwListView, TRUE );

				std::wostringstream status;
				status << LvUtils::getItemCount ( hwListView ) << L" item(s) found";
				SetWindowText( _hStatusBar, status.str().c_str() );
			}
		}
	}

	void CFileFinder::updateStatus()
	{
		if( _worker.isRunning() )
		{
			std::wostringstream sstr;
			sstr << L"Searching (" << _foundItems.size() << L"): ";

			SetWindowText( _hStatusBar, MiscUtils::makeCompactPath( _hStatusBar, _currentPath, sstr.str(), L"", 12 ).c_str() );

			std::lock_guard<std::recursive_mutex> locker( _mutex );

			LvUtils::setItemCount( GetDlgItem( _hDlg, IDC_FIND_RESULT ), _foundItems.size(), LVSICF_NOSCROLL );
		}
	}

	int CFileFinder::findItemIndex( ItemDataPtr itemData )
	{
		std::lock_guard<std::recursive_mutex> locker( _mutex );

		auto it = std::find_if( _foundItems.begin(), _foundItems.end(), [itemData]( ItemDataPtr ptr ) { return itemData == ptr; } );

		if( it != _foundItems.end() )
			return static_cast<int>( it - _foundItems.begin() );

		return 0;
	}

	void CFileFinder::sortItems( int code )
	{
		auto itemData = getItemData( LvUtils::getFocusedItem( GetDlgItem( _hDlg, IDC_FIND_RESULT ) ) );

		{
			std::lock_guard<std::recursive_mutex> locker( _mutex );

			switch( code )
			{
			case 0: // sort by Name
			case VK_F3:
				std::sort( _foundItems.begin(), _foundItems.end(), []( ItemDataPtr a, ItemDataPtr b ) { return SortUtils::byName( a->wfd, b->wfd ); } );
				break;
			case 1: // sort by Path
			case VK_F4:
				std::sort( _foundItems.begin(), _foundItems.end(), []( ItemDataPtr a, ItemDataPtr b ) { return SortUtils::byPath( a->path, b->path ); } );
				break;
			case 2: // sort by Size;
			case VK_F6:
				std::sort( _foundItems.begin(), _foundItems.end(), []( ItemDataPtr a, ItemDataPtr b ) { return SortUtils::bySize( a->wfd, b->wfd ); } );
				break;
			case 3: // sort by Date
			case 4: // sort by Time
			case VK_F5:
				std::sort( _foundItems.begin(), _foundItems.end(), []( ItemDataPtr a, ItemDataPtr b ) { return SortUtils::byTime( a->wfd, b->wfd ); } );
				break;
			case 5: // sort by Attr
				std::sort( _foundItems.begin(), _foundItems.end(), []( ItemDataPtr a, ItemDataPtr b ) { return SortUtils::byAttr( a->wfd, b->wfd ); } );
				break;
			}
		}

		_cachedData.clear();

		LvUtils::cleanUp( GetDlgItem( _hDlg, IDC_FIND_RESULT ) );
		LvUtils::setItemCount( GetDlgItem( _hDlg, IDC_FIND_RESULT ), _foundItems.size(), 0 );

		if( itemData )
			LvUtils::focusItem( GetDlgItem( _hDlg, IDC_FIND_RESULT ), findItemIndex( itemData ) );
	}

	void CFileFinder::findFilesDone()
	{
		LvUtils::setItemCount( GetDlgItem( _hDlg, IDC_FIND_RESULT ), _foundItems.size(), LVSICF_NOSCROLL );
		
		std::wostringstream status;

		if( !_foundItems.empty() )
			status << _foundItems.size() << L" item(s) found in " << StringUtils::formatTime( _timeSpent );
		else
			status << L"Not found";

		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_NAME ), TRUE );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_INDIR ), TRUE );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_USECONTENT ), TRUE );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_SEARCHSUBDIR ), TRUE );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_SEARCHARCHIVE ), TRUE );

		enableSearchGroup();

		SetWindowText( _hStatusBar, status.str().c_str() );
		SetDlgItemText( _hDlg, IDOK, L"&Find" );

		// focus first edit control window
		SetFocus( GetDlgItem( _hDlg, IDC_FIND_NAME ) );
	}

	UINT CFileFinder::onContextMenu( HWND hWnd, POINT& pt )
	{
		// get current cursor (or selected item) position
		LvUtils::getItemPosition( hWnd, LvUtils::getSelectedItem( hWnd ), &pt );

		// show context menu for currently selected items
		auto items = getSelectedItems( hWnd );

		// check if only files are selected
		auto indices = LvUtils::getSelectedItems( hWnd );
		bool isFile = !indices.empty();

		for( int idx : indices )
		{
			auto itemData = getItemData( idx );
			if( FsUtils::checkAttrib( itemData->wfd.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY ) )
			{
				isFile = false;
				break;
			}
		}

		// show context menu for currently selected items
		UINT ret = FCS::inst().getApp().processContextMenu( _contextMenu, _hDlg, items, pt, isFile );

		if( ret == (UINT)~0 )
		{
			auto verb = _contextMenu.getResultVerb();

			if( verb == "cut" || verb == "copy" )
				ShellUtils::invokeCommand( verb, items );
		}

		return ret;
	}

	bool CFileFinder::convertToAnsi( const std::wstring& text, std::string& outStr )
	{
		if( !_hexMode )
		{
			// try conversion to ANSI, using system code-page
			outStr = StringUtils::convert2A( text, CP_ACP );
			std::wstring strWide = StringUtils::convert2W( outStr, CP_ACP );

			if( strWide != text )
			{
				outStr.clear();
				return false;
			}

			return true;
		}

		return StringUtils::convertHex( text, outStr, CP_ACP );
	}

	bool CFileFinder::initSearchContent( const TCHAR *text )
	{
		_findText = _findTextW = text;

		// convert text to ANSI (or ASCII data buffer when in HEX mode)
		if( !convertToAnsi( _findTextW, _findTextA ) && _encoding == IDC_FIND_RB_ANSI )
		{
			std::wstringstream sstr;

			if( _hexMode )
				sstr << L"Error converting hexadecimal string.";
			else
				sstr << L"Error converting \"" << text << L"\" to ANSI (Codepage: " << GetACP() << L").";

			MessageBox( _hDlg, sstr.str().c_str(), L"Conversion Failed", MB_ICONEXCLAMATION | MB_OK );

			return false;
		}

		if( _regexMode )
		{
			auto flags = _matchCase ? std::regex_constants::ECMAScript : std::regex_constants::icase;

			try {
				_regexA = _findTextA.empty() ? nullptr : std::make_unique<std::regex>( _findTextA, flags );
				_regexW = std::make_unique<std::wregex>( _findTextW, flags );
			}
			catch( const std::regex_error& e )
			{
				CFileViewer::reportRegexError( _hDlg, e, text );

				return false;
			}
		}
		else
		{
			if( _wholeWords && !_findTextW.empty() )
			{
				_srcBounds[0] = StringUtils::isAlphaNum( _findTextW[0] );
				_srcBounds[1] = StringUtils::isAlphaNum( _findTextW[_findTextW.length() - 1] );
			}

			if( !_matchCase )
			{
				StringUtils::convert2UprInPlace( _findTextA );
				StringUtils::convert2UprInPlace( _findTextW );
			}
		}

		return true;
	}

	//
	// Start finding files process
	//
	void CFileFinder::startFindFiles()
	{
		LvUtils::cleanUp( GetDlgItem( _hDlg, IDC_FIND_RESULT ) );

		_foundItems.clear();
		_cachedData.clear();

		TCHAR mask[MAX_WPATH + 1], inDir[MAX_WPATH + 1], text[MAX_WPATH + 1];
		GetDlgItemText( _hDlg, IDC_FIND_NAME, mask, MAX_WPATH );
		GetDlgItemText( _hDlg, IDC_FIND_INDIR, inDir, MAX_WPATH );
		GetDlgItemText( _hDlg, IDC_FIND_CONTENT, text, MAX_WPATH );
		SetWindowText( _hStatusBar, L"Ready" );

		_encoding = MiscUtils::getCheckedRadio( _hDlg, { IDC_FIND_RB_AUTO, IDC_FIND_RB_ANSI, IDC_FIND_RB_UTF8, IDC_FIND_RB_UTF16, IDC_FIND_RB_UTF32 } );
		_searchSubdirs = IsDlgButtonChecked( _hDlg, IDC_FIND_SEARCHSUBDIR ) == BST_CHECKED;
		_searchArchives = IsDlgButtonChecked( _hDlg, IDC_FIND_SEARCHARCHIVE ) == BST_CHECKED;
		_searchDocuments = false; // TODO: ebooks, zipped (docx, etc.)
		_matchCase = IsDlgButtonChecked( _hDlg, IDC_FIND_MATCHCASE ) == BST_CHECKED;
		_wholeWords = IsDlgButtonChecked( _hDlg, IDC_FIND_WHOLEWORD ) == BST_CHECKED;
		_hexMode = IsDlgButtonChecked( _hDlg, IDC_FIND_HEXMODE ) == BST_CHECKED;
		_regexMode = IsDlgButtonChecked( _hDlg, IDC_FIND_REGEXMODE ) == BST_CHECKED;
		_srcBounds[0] = _srcBounds[1] = true;
		_findMask = mask;
		_findInDir = inDir;
		_findTextA.clear();
		_findTextW.clear();
		_findText.clear();
		_currentPath.clear();
		_timeSpent = 0;

		if( IsDlgButtonChecked( _hDlg, IDC_FIND_USECONTENT ) == BST_CHECKED )
		{
			if( !initSearchContent( text ) )
				return;
		}

		SetWindowText( _hStatusBar, L"Searching.." );
		SetDlgItemText( _hDlg, IDOK, L"&Stop" );

		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_NAME ), FALSE );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_INDIR ), FALSE );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_USECONTENT ), FALSE );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_SEARCHSUBDIR ), FALSE );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_SEARCHARCHIVE ), FALSE );

		enableSearchGroup( false );

		SetTimer( _hDlg, FC_TIMER_GUI_ID, FC_TIMER_GUI_TICK, NULL );

		// focus the stop button window
		SetFocus( GetDlgItem( _hDlg, IDOK ) );

		_worker.start();
	}

	//
	// Stop finding files process
	//
	void CFileFinder::stopFindFiles()
	{
		KillTimer( _hDlg, FC_TIMER_GUI_ID );

		SetWindowText( _hStatusBar, L"Stopping.." );

		_worker.stop();

		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_NAME ), TRUE );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_INDIR ), TRUE );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_USECONTENT ), TRUE );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_SEARCHSUBDIR ), TRUE );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_SEARCHARCHIVE ), TRUE );

		enableSearchGroup();

		SetWindowText( _hStatusBar, L"Stopped" );
		SetDlgItemText( _hDlg, IDOK, L"&Find" );
	}

	//
	// Handle keyboard actions without any modificator key pressed
	//
	void CFileFinder::handleKeyboardAction( LPNMLVKEYDOWN vKey )
	{
		switch( vKey->wVKey )
		{
		case VK_F3:
			onViewItem( vKey->hdr.hwndFrom, _hexMode );
			break;
		case VK_F4:
			onEditItem( vKey->hdr.hwndFrom );
			break;
		case VK_F8:
		case VK_DELETE:
			onDeleteItems( vKey->hdr.hwndFrom );
			break;
		case VK_SPACE:
			onFocusItem( vKey->hdr.hwndFrom );
			break;
		}
	}

	//
	// Handle keyboard action with Ctrl key pressed
	//
	void CFileFinder::handleKeyboardActionWithCtrl( LPNMLVKEYDOWN vKey )
	{
		switch( vKey->wVKey )
		{
		//
		// sorting actions
		case VK_F3:
		case VK_F4:
		case VK_F5:
		case VK_F6:
			sortItems( vKey->wVKey );
			break;
		//
		// clipboard actions
		case 0x43: // "C - copy"
			ShellUtils::copyItemsToClipboard( getSelectedItems( vKey->hdr.hwndFrom ) );
			break;
		case 0x58: // "X - cut"
			ShellUtils::copyItemsToClipboard( getSelectedItems( vKey->hdr.hwndFrom ), false );
			break;
		case 0x41: // "A - select all"
			LvUtils::selectAllItems( vKey->hdr.hwndFrom );
			break;
		case 0x4D: // "M - files list to clipboard"
			MiscUtils::setClipboardText( FCS::inst().getFcWindow(), StringUtils::join( getSelectedItems( vKey->hdr.hwndFrom ), L"\r\n" ) );
			break;
		}
	}

	//
	// Handle keyboard action with Alt key pressed
	//
	void CFileFinder::handleKeyboardActionWithAlt( LPNMLVKEYDOWN vKey )
	{
		switch( vKey->wVKey )
		{
		case VK_F3:
			onViewItem( vKey->hdr.hwndFrom, true );
			break;
		}
	}

	//
	// Handle keyboard action with Shift key pressed
	//
	void CFileFinder::handleKeyboardActionWithShift( LPNMLVKEYDOWN vKey )
	{
		// nothing yet
	}

	void CFileFinder::onHandleVirtualKeys( LPNMLVKEYDOWN vKey )
	{
		if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
		{
			// CTRL modifier key
			handleKeyboardActionWithCtrl( vKey );
		}
		else if( ( GetAsyncKeyState( VK_MENU ) & 0x8000 ) != 0 )
		{
			// ALT modifier key
			handleKeyboardActionWithAlt( vKey );
		}
		else if( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0 )
		{
			// SHIFT modifier key
			handleKeyboardActionWithShift( vKey );
		}
		else
		{
			// no modifier key
			handleKeyboardAction( vKey );
		}
	}

	//
	// Load the cache with the recommended range
	//
	void CFileFinder::onPrepareCache( LPNMLVCACHEHINT lvCache )
	{
	//	auto row = ListView_GetCountPerPage( GetDlgItem( _hDlg, IDC_FIND_RESULT ) );
	//	auto top = ListView_GetTopIndex( GetDlgItem( _hDlg, IDC_FIND_RESULT ) );

	//	PrintDebug("Request: %d - %d (Rows:%d, top idx:%d)",
	//		lvCache->iFrom, lvCache->iTo, row, top);

		if( !_cachedData.empty() )
		{
			// check if we already cached data within this interval
			auto beg = _cachedData.begin()->first;
			auto end = _cachedData.rbegin()->first;

			if( beg <= lvCache->iFrom && end >= lvCache->iTo )
				return;

			_cachedData.clear();
		}

		int cacheFrom = lvCache->iFrom;//min( lvCache->iFrom, top );
		int cacheTo = lvCache->iTo;

		for( int i = cacheFrom; i <= cacheTo; i++ )
		{
			auto itemData = getItemData( i );

			_cachedData[i] = {
				FsUtils::getFormatStrFileSize( &itemData->wfd ),
				FsUtils::getFormatStrFileDate( &itemData->wfd ),
				FsUtils::getFormatStrFileTime( &itemData->wfd ),
				FsUtils::getFormatStrFileAttr( &itemData->wfd )
			};
		}

	//	PrintDebug("Cached: %d - %d (Size: %zu)", cacheFrom, cacheTo,
	//		_cachedData.size());
	}

	CFileFinder::ItemDataCache& CFileFinder::getCachedData( int index )
	{
		if( _cachedData.empty() || _cachedData.find( index ) == _cachedData.end() )
		{
			// item at index is not cached - add to the cache
			auto itemData = getItemData( index );

			_cachedData[index] = {
				FsUtils::getFormatStrFileSize( &itemData->wfd ),
				FsUtils::getFormatStrFileDate( &itemData->wfd ),
				FsUtils::getFormatStrFileTime( &itemData->wfd ),
				FsUtils::getFormatStrFileAttr( &itemData->wfd )
			};
		}

		return _cachedData[index];
	}

	//
	// Draw items in virtual list-view
	//
	bool CFileFinder::onDrawDispItems( NMLVDISPINFO *lvDisp )
	{
		auto itemData = getItemData( lvDisp->item.iItem );

		if( itemData )
		{
			const wchar_t *str = nullptr;

			switch( lvDisp->item.iSubItem )
			{
			case 0:
				str = itemData->wfd.cFileName;
				break;
			case 1:
				str = itemData->path.c_str();
				break;
			case 2:
				str = getCachedData( lvDisp->item.iItem ).fileSize.c_str();
				break;
			case 3:
				str = getCachedData( lvDisp->item.iItem ).fileDate.c_str();
				break;
			case 4:
				str = getCachedData( lvDisp->item.iItem ).fileTime.c_str();
				break;
			case 5:
				str = getCachedData( lvDisp->item.iItem ).fileAttr.c_str();
				break;
			}

			lvDisp->item.pszText = const_cast<LPWSTR>( str );
			lvDisp->item.iImage = itemData->imgIdx;
		}

		return true;
	}

	//
	// Find the index according to LVFINDINFO
	//
	int CFileFinder::onFindItemIndex( LPNMLVFINDITEM lvFind )
	{
		std::lock_guard<std::recursive_mutex> locker( _mutex );

		int itemsCount = static_cast<int>( _foundItems.size() );

		size_t findTokenLen = wcslen( lvFind->lvfi.psz );

		for( int i = lvFind->iStart; i < itemsCount; ++i )
		{
			if( _wcsnicmp( _foundItems[i]->wfd.cFileName, lvFind->lvfi.psz, findTokenLen ) == 0 )
			{
				return i;
			}
		}

		for( int i = 0; i < lvFind->iStart; ++i )
		{
			if( _wcsnicmp( _foundItems[i]->wfd.cFileName, lvFind->lvfi.psz, findTokenLen ) == 0 )
			{
				return i;
			}
		}

		return -1;
	}

	//
	// Begin drag & drop items in list-view
	//
	void CFileFinder::onBeginDrag( LPNMLISTVIEW lvData )
	{
		IShellItemArray *psiaItems;
		HRESULT hr = ShellUtils::createShellItemArrayFromPaths( getSelectedItems( lvData->hdr.hwndFrom ), &psiaItems );

		IDataObject *ppdtobj = NULL;
		psiaItems->BindToHandler( NULL, BHID_DataObject, IID_PPV_ARGS( &ppdtobj ) );
		psiaItems->Release();

		DWORD effect = 0;
		hr = SHDoDragDrop( lvData->hdr.hwndFrom, ppdtobj, NULL, DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK, &effect );
		ppdtobj->Release();

		if( hr == DRAGDROP_S_DROP && effect == DROPEFFECT_NONE )
		{
			auto itemIndices = LvUtils::getSelectedItems( lvData->hdr.hwndFrom );
			PrintDebug( "items: %zu, effect: %u", itemIndices.size(), effect );

			for( auto it = itemIndices.rbegin(); it != itemIndices.rend(); ++it )
			{
				//auto itemData = getItemData( *it );
				//if( !FsUtils::isPathExisting( PathUtils::addDelimiter( itemData->path ) + itemData->wfd.cFileName ) )
				//{
					LvUtils::deleteItem( lvData->hdr.hwndFrom, *it );
					_foundItems.erase( _foundItems.begin() + *it );
					_cachedData.clear();
				//}
				//else PrintDebug( "file: %ls", itemData->wfd.cFileName );
			}

			std::wostringstream status;
			status << LvUtils::getItemCount ( lvData->hdr.hwndFrom ) << L" item(s) found";
			SetWindowText( _hStatusBar, status.str().c_str() );
		}
	}

	bool CFileFinder::onNotifyMessage( LPNMHDR notifyHeader )
	{
		switch( notifyHeader->code )
		{
		case NM_RETURN:
		case NM_DBLCLK:
			onExecItem( notifyHeader->hwndFrom );
			break;

		case LVN_KEYDOWN:
			onHandleVirtualKeys( reinterpret_cast<LPNMLVKEYDOWN>( notifyHeader ) );
			break;

		case LVN_GETDISPINFO:
			return onDrawDispItems( reinterpret_cast<NMLVDISPINFO*>( notifyHeader ) );

		case LVN_ODFINDITEM:
			SetWindowLongPtr( _hDlg, DWLP_MSGRESULT, onFindItemIndex( reinterpret_cast<LPNMLVFINDITEM>( notifyHeader ) ) );
			return true;

		case LVN_ODCACHEHINT:
			onPrepareCache( reinterpret_cast<LPNMLVCACHEHINT>( notifyHeader ) );
			return 0;

		case LVN_COLUMNCLICK:
			sortItems( reinterpret_cast<LPNMLISTVIEW>( notifyHeader )->iSubItem );
			break;

		case LVN_BEGINDRAG:
			onBeginDrag( reinterpret_cast<LPNMLISTVIEW>( notifyHeader ) );
			break;
		}

		return false;
	}

	INT_PTR CALLBACK CFileFinder::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_TIMER:
			updateStatus();
			break;

		case UM_STATUSNOTIFY:
			switch( wParam )
			{
			case FC_ARCHNEEDPASSWORD:
				break;
			case FC_ARCHNEEDNEWNAME:
				break;
			case FC_ARCHPROCESSINGPATH:
				break;
			case FC_ARCHNEEDNEXTVOLUME:
				break;
			case FC_ARCHPROCESSINGVOLUME:
				break;

			case FC_FINDITEMDIR:
				_currentPath = reinterpret_cast<BSTR>( lParam );
				SysFreeString( (BSTR)lParam );
				break;

			case 0: // fail
			case 1:
				findFilesDone();
				break;
			}
			break;

		case WM_NOTIFY:
			return onNotifyMessage( reinterpret_cast<LPNMHDR>( lParam ) );

		case WM_COMMAND:
			switch( LOWORD( wParam ) )
			{
				case IDC_FIND_CONTENT:
					if( _fireEvent && HIWORD( wParam ) == EN_CHANGE && IsDlgButtonChecked( _hDlg, IDC_FIND_HEXMODE ) == BST_CHECKED )
						MiscUtils::sanitizeHexText( GetDlgItem( _hDlg, IDC_FIND_CONTENT ), _fireEvent );
					break;
				case IDC_FIND_USECONTENT:
					showSearchGroup( IsDlgButtonChecked( _hDlg, IDC_FIND_USECONTENT ) == BST_CHECKED );
					break;
				case IDC_FIND_HEXMODE:
					enableHexMode( IsDlgButtonChecked( _hDlg, IDC_FIND_HEXMODE ) == BST_CHECKED );
					break;
				case IDC_FIND_REGEXMODE:
					EnableWindow( GetDlgItem( _hDlg, IDC_FIND_HEXMODE ), IsDlgButtonChecked( _hDlg, IDC_FIND_REGEXMODE ) == BST_UNCHECKED );
					break;
			}
			break;

		case WM_CONTEXTMENU:
			if( onContextMenu( reinterpret_cast<HWND>( wParam ), POINT { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) } ) == 2 )
				FCS::inst().getApp().getActivePanel().getActiveTab()->compareFiles();
			break;

		case WM_DRAWITEM:
		case WM_MEASUREITEM:
		case WM_MENUCHAR:
		case WM_INITMENUPOPUP:
		case WM_MENUSELECT:
		{
			auto ret = _contextMenu.handleMessages( _hDlg, message, wParam, lParam );
			if( ret ) return ret;
			else break;
		}

		case WM_SIZE:
			updateLayout( LOWORD( lParam ), HIWORD( lParam ) );
			break;
		}

		return (INT_PTR)false;
	}

	void CFileFinder::updateLayout( int width, int height )
	{
		bool searchContent = IsDlgButtonChecked( _hDlg, IDC_FIND_USECONTENT ) == BST_CHECKED;

		MoveWindow( GetDlgItem( _hDlg, IDC_FIND_NAME ), 68, 20, width - 166, 20, TRUE );
		MoveWindow( GetDlgItem( _hDlg, IDC_FIND_INDIR ), 68, 49, width - 166, 20, TRUE );
		MoveWindow( GetDlgItem( _hDlg, IDOK ), width - 86, 18, 75, 23, TRUE );
		MoveWindow( GetDlgItem( _hDlg, IDCANCEL ), width - 86, 47, 75, 23, TRUE );
		MoveWindow( GetDlgItem( _hDlg, IDC_FIND_CONTENT ), 68, 115, width - 80, 20, TRUE );
		MoveWindow( GetDlgItem( _hDlg, IDC_FIND_RESULT ), 11, searchContent ? 165 : 117, width - 23, height - ( searchContent ? 187 : 139 ), TRUE );

		SendMessage( _hStatusBar, WM_SIZE, 0, 0 );

		auto columnWidth = ( width > 324 + 100 ? width - 324 : 100 ); // first two columns
		LvUtils::setColumnWidth( GetDlgItem( _hDlg, IDC_FIND_RESULT ), columnWidth / 2, 0 );
		LvUtils::setColumnWidth( GetDlgItem( _hDlg, IDC_FIND_RESULT ), columnWidth / 2, 1 );
	}
}
