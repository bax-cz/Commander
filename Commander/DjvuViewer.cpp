#include "stdafx.h"

#include "Commander.h"
#include "DjvuViewer.h"
#include "IconUtils.h"
#include "MiscUtils.h"

#include "../Djvu/miniexp.h"

#include "../Tiff/tiff.h"
#include "../Tiff/tiffio.h"
#include "../Tiff/tiffconf.h"
#include "../Tiff/tiffio.hxx"

namespace Commander
{
	void CDjvuViewer::onInit()
	{
		SendMessage( _hDlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>( IconUtils::getStock( SIID_DOCASSOC ) ) );
		SendMessage( _hDlg, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>( IconUtils::getStock( SIID_DOCASSOC, true ) ) );

		MiscUtils::centerOnWindow( FCS::inst().getFcWindow(), _hDlg );

		// tell windows that we can handle drag and drop
		DragAcceptFiles( _hDlg, TRUE );

		Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		Gdiplus::GdiplusStartup( &_gdiplusToken, &gdiplusStartupInput, NULL );

		//const char *p = ddjvu_get_version_string();
		std::wstring product, version;
		SysUtils::getProductAndVersion( product, version );

		_djvuCtx = ddjvu_context_create( StringUtils::convert2A( product ).c_str() );

		_pagesCnt = 0;
		_pageIdx = 0;
		_buffLen = 0;

		_pBrush = new Gdiplus::SolidBrush( Gdiplus::Color::Black );
		_pGraph = nullptr;
		_djvuDoc = nullptr;
		_pBitmap = nullptr;
		_pStream = nullptr;
		_pBuff = nullptr;
		_hDataBuf = nullptr;

		// get dialog menu handle
		//_hMenu = GetMenu( _hDlg );

		_worker.init( [this] { return _loadDjvuData(); }, _hDlg, UM_STATUSNOTIFY );
	}

	bool CDjvuViewer::onClose()
	{
		show( SW_HIDE ); // hide dialog

		_worker.stop();

		return true;
	}

	void CDjvuViewer::onDestroy()
	{
		if( _djvuDoc )
			ddjvu_document_release( _djvuDoc );

		ddjvu_context_release( _djvuCtx );

		cleanUp();

		delete _pBrush;

		Gdiplus::GdiplusShutdown( _gdiplusToken );

		DestroyIcon( reinterpret_cast<HICON>( SendMessage( _hDlg, WM_GETICON, ICON_SMALL, 0 ) ) );
		DestroyIcon( reinterpret_cast<HICON>( SendMessage( _hDlg, WM_GETICON, ICON_BIG, 0 ) ) );
	}

	void CDjvuViewer::cleanUp()
	{
		// free memory
		if( _hDataBuf )
		{
			if( _pBitmap )
				delete _pBitmap;

			if( _pStream )
				_pStream->Release();

			GlobalUnlock( _hDataBuf );
			GlobalFree( _hDataBuf );

			_buffLen = 0;
			_pBuff = nullptr;
			_pBitmap = nullptr;
			_pStream = nullptr;
			_hDataBuf = nullptr;
		}
	}

	void CDjvuViewer::refresh()
	{
		SetCursor( LoadCursor( NULL, IDC_WAIT ) );

		_worker.stop();
		cleanUp();
		_worker.start();
	}

	bool CDjvuViewer::renderPage( ddjvu_page_t *page, int pageno )
	{
		unsigned int iw = ddjvu_page_get_width( page );
		unsigned int ih = ddjvu_page_get_height( page );
		int dpi = ddjvu_page_get_resolution( page );

		/* Process size specification */
		ddjvu_rect_t prect = { 0, 0, iw, ih };

		/* Process segment specification */
		ddjvu_rect_t rrect = prect;

		/* Process mode specification */
		ddjvu_render_mode_t mode = DDJVU_RENDER_COLOR;

		/* Determine output pixel format and compression */
	//	ddjvu_page_type_t type = ddjvu_page_get_type( page ); // TODO: optimize when DDJVU_PAGETYPE_BITONAL
		ddjvu_format_style_t style = DDJVU_FORMAT_BGR24;

		ddjvu_format_t *fmt;
		if( !( fmt = ddjvu_format_create( style, 0, 0 ) ) )
			PrintDebug("Cannot determine pixel style for page %d", pageno);

		ddjvu_format_set_row_order( fmt, 0 );

		/* Allocate buffer */
		unsigned long rowsize = rrect.w * 3; // always generate 24-bit BMP

		if( rowsize % 4 ) // row padding
			rowsize += ( 4 - (rowsize) % 4 );

		auto bmpsize = BMP_HEADER_SIZE + rowsize * rrect.h;

		_hDataBuf = GlobalAlloc( GMEM_MOVEABLE, static_cast<SIZE_T>( bmpsize ) );

		if( _hDataBuf && ( _pBuff = static_cast<BYTE*>( GlobalLock( _hDataBuf ) ) ) )
		{
			/* Make BMP header */
			BYTE *p = IconUtils::writeBmpHeader( _pBuff, rrect.w, rrect.h, false );
			_buffLen = bmpsize;

			/* Render page */
			if( !ddjvu_page_render( page, mode, &prect, &rrect, fmt, rowsize, (char*)p ) )
				memset( p, (BYTE)0xFF/*white*/, rowsize * rrect.h );
		}

		/* Free */
		ddjvu_format_release( fmt );

		return ( _hDataBuf != nullptr );
	}

#define DJVU_STATUS_READINGDOCUMENT 1
#define DJVU_STATUS_DECODINGPAGE    2
#define DJVU_STATUS_RENDERINGPAGE   3

	// try to export page data as text
	void CDjvuViewer::getPageText( int pageno )
	{
		miniexp_t r = miniexp_nil;
		const char *lvl = "page";
		while( ( r = ddjvu_document_get_pagetext( _djvuDoc, pageno, lvl ) ) == miniexp_dummy )
		{
			const ddjvu_message_t *msg = ddjvu_message_wait( _djvuCtx );
			while( ( msg = ddjvu_message_peek(_djvuCtx ) ) )
			{
				switch( msg->m_any.tag )
				{
				case DDJVU_ERROR:
					PrintDebug("ddjvu: %s", msg->m_error.message);
					if( msg->m_error.filename )
						PrintDebug("ddjvu: '%s:%d'", msg->m_error.filename, msg->m_error.lineno);
				default:
					break;
				}
				ddjvu_message_pop( _djvuCtx );
			}
		}

		if( ( r = miniexp_nth( 5, r ) ) && miniexp_stringp( r ) )
		{
			const char *s = miniexp_to_str( r );
			_pageText = StringUtils::convert2W( s );
		}
		else
			_pageText.clear();
	}

	void CDjvuViewer::saveCurrentPageAsImage()
	{
		if( _worker.isFinished() && _pStream )
		{
			UINT encodersCnt = 0, encodersSize = 0;
			if( Gdiplus::GetImageEncodersSize( &encodersCnt, &encodersSize ) != Gdiplus::Status::Ok )
				return;

			// MSDN: Pointer to a buffer that receives the array of ImageCodecInfo objects.
			// You must allocate memory for this buffer.
			// Call GetImageEncodersSize to determine the size of the required buffer.
			Gdiplus::ImageCodecInfo *encoders = (Gdiplus::ImageCodecInfo*)new BYTE[encodersCnt * encodersSize];
			if( encoders && Gdiplus::GetImageEncoders( encodersCnt, encodersSize, encoders ) == Gdiplus::Status::Ok )
			{
				std::wostringstream sstr;
				sstr << PathUtils::stripFileExtension( PathUtils::stripPath( _fileName ) ) << L"_" << std::to_wstring( _pageIdx + 1 ) << L".bmp";

				std::wstring targetDir = PathUtils::getExtendedPath( PathUtils::stripFileName( _fileName ) );
				WCHAR szFileName[MAX_WPATH] = { 0 };
				wcsncpy( szFileName, sstr.str().c_str(), MAX_WPATH );

				// construct the "double zero ending filter" string from available encoders
				WCHAR szFilter[MAX_WPATH] = { 0 }, *p = szFilter;
				for( UINT i = 0; i < encodersCnt; i++ )
				{
					wcscpy( p, encoders[i].FormatDescription ); p += wcslen( encoders[i].FormatDescription );
					wcscpy( p, L" (" ); p += 2;
					wcscpy( p, encoders[i].FilenameExtension ); p += wcslen( encoders[i].FilenameExtension );
					wcscpy( p, L")\0" ); p += 2;
					wcscpy( p, encoders[i].FilenameExtension ); p += wcslen( encoders[i].FilenameExtension );
					wcscpy( p++, L"\0" );
				}

				OPENFILENAME ofn = { 0 };
				ofn.lStructSize = sizeof( ofn );
				ofn.hwndOwner = _hDlg;
				ofn.lpstrInitialDir = targetDir.c_str();
				ofn.lpstrTitle = L"Save Page As Image";
				ofn.lpstrFilter = szFilter;
				ofn.lpstrDefExt = L"bmp";
				ofn.lpstrFile = szFileName;
				ofn.nMaxFile = MAX_WPATH;
				ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY;

				if( GetSaveFileName( &ofn ) )
				{
					if( ofn.nFilterIndex > 0 && ofn.nFilterIndex <= encodersCnt )
					{
						_ASSERTE( _pBitmap );

						std::wstring outName = szFileName;
						const CLSID clsid = encoders[ofn.nFilterIndex - 1].Clsid;
						auto extArray = StringUtils::split( encoders[ofn.nFilterIndex - 1].FilenameExtension, L";" );
						if( !extArray.empty() )
						{
							std::wstring ext = StringUtils::removeAll( extArray[0], L'*' );
							outName = PathUtils::stripFileExtension( outName ) + StringUtils::convert2Lwr( ext );
						}

						_pBitmap->Save( PathUtils::getExtendedPath( outName ).c_str(), &clsid );
					}
				}

				delete[] encoders;
			}
		}
	}

	bool CDjvuViewer::_loadDjvuData()
	{
		ShellUtils::CComInitializer _com( COINIT_APARTMENTTHREADED );

		if( _djvuDoc == nullptr )
		{
			_djvuDoc = ddjvu_document_create_by_filename_utf8( _djvuCtx,
				StringUtils::convert2A( PathUtils::getExtendedPath( _tempName ) ).c_str(), TRUE );

			if( _djvuDoc == nullptr || !_worker.isRunning() )
				return false;

			// decode djvu document
			_worker.sendNotify( 2, DJVU_STATUS_READINGDOCUMENT );
			while( !ddjvu_document_decoding_done( _djvuDoc ) )
			{
				const ddjvu_message_t *msg = ddjvu_message_wait( _djvuCtx );
				while( ( msg = ddjvu_message_peek(_djvuCtx ) ) )
				{
					switch( msg->m_any.tag )
					{
					case DDJVU_ERROR:
						PrintDebug("ddjvu: %s", msg->m_error.message);
						if( msg->m_error.filename )
							PrintDebug("ddjvu: '%s:%d'", msg->m_error.filename, msg->m_error.lineno);
					default:
						break;
					}
					ddjvu_message_pop( _djvuCtx );
				}
			}

			if( ddjvu_document_decoding_error( _djvuDoc ) )
			{
				PrintDebug("Cannot decode document.");
				return false;
			}

			_pageIdx = 0;
		}

		// get pages count
		_pagesCnt = ddjvu_document_get_pagenum( _djvuDoc );

		bool pageRendered = false;

		if( _worker.isRunning() && _pagesCnt )
		{
			_worker.sendNotify( 2, DJVU_STATUS_DECODINGPAGE );

			// get plain text from current page
			getPageText( _pageIdx );

			ddjvu_page_t *page = ddjvu_page_create_by_pageno( _djvuDoc, _pageIdx );
			while( !ddjvu_page_decoding_done( page ) )
			{
				const ddjvu_message_t *msg = ddjvu_message_wait( _djvuCtx );
				switch( msg->m_any.tag )
				{
				case DDJVU_ERROR:
					PrintDebug("ddjvu: %s", msg->m_error.message);
					if( msg->m_error.filename )
						PrintDebug("ddjvu: '%s:%d'", msg->m_error.filename, msg->m_error.lineno);
				default:
					break;
				}
				ddjvu_message_pop( _djvuCtx );

				if( !_worker.isRunning() )
				{
					// try to stop current job
					ddjvu_job_stop(ddjvu_page_job(page));
					ddjvu_page_release( page );
					return false;
				}
			}

			if( _worker.isRunning() )
			{
				if( ddjvu_page_decoding_error( page ) )
				{
					PrintDebug("Cannot decode page.");
				}

				// render page as a tiff image
				_worker.sendNotify( 2, DJVU_STATUS_RENDERINGPAGE );

				pageRendered = renderPage( page, _pageIdx );
			}

			ddjvu_page_release( page );
		}

		return pageRendered;
	}

	bool CDjvuViewer::viewFile( const std::wstring& dirName, const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel )
	{
		_fileName = PathUtils::addDelimiter( dirName ) + fileName;

		if( spPanel )
		{
			_spPanel = spPanel;
			_entries.clear();

			for( auto entry : spPanel->getDataManager()->getFileEntries() )
			{
				if( StringUtils::endsWith( entry->wfd.cFileName, L".djvu" ) )
					_entries.push_back( spPanel->getDataManager()->getEntryPathFull( entry->wfd.cFileName ) );
			}

			return _spPanel->getDataManager()->getDataProvider()->readToFile( _fileName, spPanel, _hDlg );
		}
		else
			viewFileCore( _fileName );

		return true;
	}

	void CDjvuViewer::viewFileCore( const std::wstring& fileName )
	{
		_tempName = fileName;

		_worker.stop();
		cleanUp();

		if( _djvuDoc )
		{
			ddjvu_document_release( _djvuDoc );
			_djvuDoc = nullptr;
		}

		std::wostringstream sstrTitle;
		sstrTitle << _tempName << L" - Viewer";
		
		SetWindowText( _hDlg, sstrTitle.str().c_str() );

		refresh();
	}

	void CDjvuViewer::viewNext()
	{
		auto it = std::find( _entries.begin(), _entries.end(), _fileName );

		if( it != _entries.end() && it + 1 != _entries.end() )
		{
			_fileName = *( it + 1 );
			_spPanel->getDataManager()->getDataProvider()->readToFile( _fileName, _spPanel, _hDlg );
		}
	}

	void CDjvuViewer::viewPrevious()
	{
		auto it = std::find( _entries.begin(), _entries.end(), _fileName );

		if( it != _entries.end() && it != _entries.begin() )
		{
			_fileName = *( it - 1 );
			_spPanel->getDataManager()->getDataProvider()->readToFile( _fileName, _spPanel, _hDlg );
		}
	}

	void CDjvuViewer::gotoPage()
	{
		CTextInput::CParams params(
				L"Go to Page",
				FORMAT( L"Page number (1 - %d):", _pagesCnt ),
				std::to_wstring( _pageIdx + 1 ) );

		auto text = MiscUtils::showTextInputBox( params, _hDlg );
		if( !text.empty() )
		{
			int idx = 0;
			try {
				idx = std::stoi( text );
			}
			catch( const std::invalid_argument& e )
			{
				e.what();
				return;
			}
			idx = idx <= 0 ? 0 : idx > _pagesCnt - 1 ? _pagesCnt - 1 : idx - 1;
			if( idx != _pageIdx )
			{
				_pageIdx = idx;
				refresh();
			}
		}
	}

	void CDjvuViewer::handleDropFiles( HDROP hDrop )
	{
		auto items = ShellUtils::getDroppedItems( hDrop );
	
		DragFinish( hDrop );

		if( !items.empty() )
		{
			_fileName = items[0];

			auto it = std::find( _entries.begin(), _entries.end(), _fileName );

			if( it == _entries.end() )
				_entries.push_back( _fileName );

			_spPanel->getDataManager()->getDataProvider()->readToFile( _fileName, _spPanel, _hDlg );
		}
	}

	void CDjvuViewer::handleKeyboard( WPARAM key )
	{
		switch( key )
		{
		case VK_SPACE:
			viewNext();
			break;
		case VK_BACK:
			viewPrevious();
			break;
		case VK_HOME:
			if( _pageIdx )
			{
				_pageIdx = 0;
				refresh();
			}
			break;
		case VK_END:
			if( _pageIdx < _pagesCnt - 1 )
			{
				_pageIdx = _pagesCnt - 1;
				refresh();
			}
			break;
		case VK_PRIOR:
			if( _pageIdx )
			{
				_pageIdx--;
				refresh();
			}
			break;
		case VK_NEXT:
			if( _pageIdx < _pagesCnt - 1 )
			{
				_pageIdx++;
				refresh();
			}
			break;
		case VK_F11:
		case 0x46: // 'F' - go fullscreen
			fullscreen( !_isFullscreen );
			break;
		case 0x53: // 'S' - save current page as image
			if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
				saveCurrentPageAsImage();
			break;
		case 0x47: // 'G' - go to page
			if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
				gotoPage();
			break;
		case 0x43: // 'C' - save text to clipboard
			if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
				MiscUtils::setClipboardText( FCS::inst().getFcWindow(), _pageText );
			break;
		default:
			break;
		}
	}

	void CDjvuViewer::displayImage()
	{
		if( _pBitmap && _pBitmap->GetLastStatus() == Gdiplus::Status::Ok )
		{
			//Gdiplus::ImageAttributes attr;
			//attr.SetColorKey( Gdiplus::Color::Magenta, Gdiplus::Color::Magenta );
			//attr.Reset();

			RECT wnd;
			GetClientRect( _hDlg, &wnd );

			auto rct = IconUtils::calcRectFit( _hDlg, _pBitmap->GetWidth(), _pBitmap->GetHeight() );

			// border strips width and height
			int w = ( wnd.right - rct.Width ) / 2;
			int h = ( wnd.bottom - rct.Height ) / 2;

			_pGraph = Gdiplus::Graphics::FromHWND( _hDlg );

			// draw border strips
			_pGraph->FillRectangle( _pBrush, 0, 0, wnd.right, h ); // top
			_pGraph->FillRectangle( _pBrush, 0, h + rct.Height - 1, wnd.right, h + 2 ); // bottom
			_pGraph->FillRectangle( _pBrush, 0, h, w, rct.Height ); // left
			_pGraph->FillRectangle( _pBrush, w + rct.Width - 1, h, w + 2, rct.Height ); // right

			_pGraph->DrawImage(
				_pBitmap,
				rct,
				0, 0, _pBitmap->GetWidth(), _pBitmap->GetHeight(),
				Gdiplus::UnitPixel,
				nullptr );
				//&attr );
		}
	}

	void CDjvuViewer::handleContextMenu( POINT pt )
	{
		if( _worker.isRunning() )
			return;

		if( pt.x == -1 || pt.y == -1 )
		{
			// get dialog's center position
			RECT rct;
			GetClientRect( _hDlg, &rct );
			pt.x = ( rct.left + rct.right ) / 2;
			pt.y = ( rct.top + rct.bottom ) / 2;
			ClientToScreen( _hDlg, &pt );
		}

		// show context menu for currently selected items
		auto hMenu = CreatePopupMenu();
		if( hMenu )
		{
			AppendMenu( hMenu, MF_STRING, 1, L"Previous\tBackspace" );
			AppendMenu( hMenu, MF_STRING, 2, L"Next\tSpace" );
			AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );
			AppendMenu( hMenu, MF_STRING | ( _pageIdx ? 0 : MF_GRAYED ), 3, L"Previous Page\tPage up" );
			AppendMenu( hMenu, MF_STRING | ( _pageIdx < _pagesCnt - 1 ? 0 : MF_GRAYED ), 4, L"Next Page\tPage down" );
			AppendMenu( hMenu, MF_STRING, 5, L"Go to Page...\tCtrl+G" );
			AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );
			AppendMenu( hMenu, MF_STRING, 6, L"Fullscreen\tF11" );
			AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );
			AppendMenu( hMenu, MF_STRING, 7, L"Save Page As...\tCtrl+S" );
			AppendMenu( hMenu, MF_STRING | ( _pageText.empty() ? MF_GRAYED : 0 ), 8, L"Text to &Clipboard\tCtrl+C" );

			UINT cmdId = TrackPopupMenu( hMenu, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, _hDlg, NULL );
			switch( cmdId )
			{
			case 1:
				viewPrevious();
				break;
			case 2:
				viewNext();
				break;
			case 3:
				if( _pageIdx )
				{
					_pageIdx--;
					refresh();
				}
				break;
			case 4:
				if( _pageIdx < _pagesCnt - 1 )
				{
					_pageIdx++;
					refresh();
				}
				break;
			case 5:
				gotoPage();
				break;
			case 6:
				fullscreen( !_isFullscreen );
				break;
			case 7:
				saveCurrentPageAsImage();
				break;
			case 8:
				MiscUtils::setClipboardText( FCS::inst().getFcWindow(), _pageText );
				break;
			default:
				break;
			}

			DestroyMenu( hMenu );
			hMenu = NULL;
		}
	}

	INT_PTR CALLBACK CDjvuViewer::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_COMMAND:
			PrintDebug("msg: 0x%04X, wpar: 0x%04X, lpar: 0x%04X", message, wParam, lParam);
			//handleMenuCommands( LOWORD( wParam ) );
			break;

		case WM_KEYDOWN:
			switch( wParam )
			{
			case VK_INSERT:
				if( _spPanel )
					_spPanel->markEntry( PathUtils::stripPath( _fileName ) );
				break;
			default:
				handleKeyboard( wParam );
				break;
			}
			break;

		case WM_LBUTTONDBLCLK:
			fullscreen( !_isFullscreen );
			break;

		case WM_MOUSEWHEEL:
			if( GET_WHEEL_DELTA_WPARAM( wParam ) < 0 )
				viewNext();
			else
				viewPrevious();
			break;

		case UM_READERNOTIFY:
			if( wParam == 1 )
			{
				viewFileCore( _spPanel->getDataManager()->getDataProvider()->getResult() );
			}
			else
			{
				SetCursor( NULL );
				std::wstringstream sstr;
				sstr << L"Cannot open \"" << _fileName << L"\" file.\r\n" << _spPanel->getDataManager()->getDataProvider()->getErrorMessage();
				MessageBox( _hDlg, sstr.str().c_str(), L"View Djvu", MB_OK | MB_ICONEXCLAMATION );
				close();
			}
			break;

		case UM_STATUSNOTIFY:
			if( wParam )
			{
				std::wstring status;
				switch( lParam )
				{
				case DJVU_STATUS_READINGDOCUMENT:
					status = L"Reading document: ";
					break;
				case DJVU_STATUS_DECODINGPAGE:
					status = L"Decoding page: ";
					break;
				case DJVU_STATUS_RENDERINGPAGE:
					status = L"Rendering page: ";
					break;
				}

				std::wostringstream sstrTitle;
				sstrTitle << status << _fileName << L" (" << _pageIdx + 1 << L" of " << _pagesCnt << L") - Viewer";
				SetWindowText( _hDlg, sstrTitle.str().c_str() );

				// create IStream from global data buffer
				if( wParam == 1 && ( _pStream = SHCreateMemStream( _pBuff, _buffLen ) ) )
				{
					SetCursor( NULL );
					_pBitmap = Gdiplus::Bitmap::FromStream( _pStream );
					displayImage();
				}
			}
			else
			{
				SetCursor( NULL );
				std::wstringstream sstr;
				sstr << L"Cannot open \"" << _fileName << L"\" file.";
				MessageBox( _hDlg, sstr.str().c_str(), L"View Djvu", MB_OK | MB_ICONEXCLAMATION );
				close();
			}
			break;

		case WM_DROPFILES:
			handleDropFiles( (HDROP)wParam );
			break;

		case WM_CONTEXTMENU:
			handleContextMenu( POINT { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) } );
			break;

		case WM_SETCURSOR:
			if( _worker.isRunning() )
			{
				SetCursor( LoadCursor( NULL, IDC_WAIT ) );
				return 0;
			}
			break;

		case WM_CTLCOLORDLG:
		case WM_CTLCOLORSTATIC:
		//	SetTextColor( reinterpret_cast<HDC>( wParam ), FC_COLOR_FOCUSRECT );
		//	SetBkColor( reinterpret_cast<HDC>( wParam ), FC_COLOR_TEXT );
			return reinterpret_cast<LRESULT>( GetStockBrush( BLACK_BRUSH ) );

		case WM_PAINT:
		case WM_SIZE:
			displayImage();
			break;
		}

		return (INT_PTR)false;
	}
}
