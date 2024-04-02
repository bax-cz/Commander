#include "stdafx.h"

#include "Commander.h"
#include "ImageViewer.h"
#include "ImageTypes.h"
#include "IconUtils.h"
#include "MiscUtils.h"

#include "../OpenJPEG/openjpeg.h"
#include "../OpenJPEG/convert.h"
#include "../Pcx/pcx.h"
#include "../Tga/tga.h"

namespace Commander
{

	// Sample quiet callback expecting no client object
	static void quiet_callback( const char *msg, void *client_data )
	{
		(void)msg;
		(void)client_data;
	}

	HGLOBAL decodeJp2( const std::wstring& fileName )
	{
		HGLOBAL hDataBuf = nullptr;

		opj_stream_t *l_stream = opj_stream_create_default_file_stream( fileName.c_str(), OPJ_TRUE );

		if( l_stream )
		{
			/* Set codec according to file extension - .j2k or .jp2 */
			opj_codec_t *l_codec = opj_create_decompress( StringUtils::endsWith( fileName, L".j2k" ) ? OPJ_CODEC_J2K : OPJ_CODEC_JP2 );

			/* Set all callbacks to quiet */
			opj_set_info_handler( l_codec, quiet_callback, nullptr );
			opj_set_warning_handler( l_codec, quiet_callback, nullptr );
			opj_set_error_handler( l_codec, quiet_callback, nullptr );

			opj_dparameters params;
			opj_set_default_decoder_parameters( &params );

			/* Setup the decoder decoding parameters using user parameters */
			if( opj_setup_decoder( l_codec, &params ) )
			{
				opj_codec_set_threads( l_codec, opj_get_num_cpus() );

				opj_image_t *image = nullptr;

				/* Read the main header of the codestream and if necessary the JP2 boxes*/
				if( opj_read_header( l_stream, l_codec, &image ) )
				{
					/* Get the decoded image */
					if( !( opj_decode( l_codec, l_stream, image ) && opj_end_decompress( l_codec, l_stream ) ) )
					{
						PrintDebug("ERROR -> opj_decompress: failed to decode image!");
					}
					else
					{
						_ASSERTE( image->numcomps );

						auto fSize = getbmpsize( image );
						hDataBuf = GlobalAlloc( GMEM_MOVEABLE, static_cast<SIZE_T>( fSize ) );

						unsigned char *pBuffer = nullptr;
						if( hDataBuf && ( pBuffer = static_cast<unsigned char*>( GlobalLock( hDataBuf ) ) ) )
						{
							imagetobmpbuf( image, pBuffer );
						}
					}
					opj_image_destroy( image );
				}
				else
				{
					PrintDebug("ERROR -> opj_decompress: failed to read the header");
				}
			}
			else
			{
				PrintDebug("ERROR -> opj_decompress: failed to setup the decoder");
			}

			opj_stream_destroy( l_stream );
			opj_destroy_codec( l_codec );
		}
		else
		{
			PrintDebug("ERROR -> failed to create the stream from the file %ls", fileName.c_str());
		}

		return hDataBuf;
	}

	void TGAError_callback(TGA *tga, int err)
	{
		PrintDebug("TGA Error: %ls", StringUtils::convert2W( TGAStrError( err ) ).c_str());
	}

	size_t writeBmp(TGA *tga, TGAData *data, tbyte *pbuf)
	{
		int w = (int)tga->hdr.width;
		int h = (int)tga->hdr.height;

		tbyte *p = IconUtils::writeBmpHeader24b( pbuf, w, h );

		int bpp = (tga->hdr.depth == 15) ? 2 : tga->hdr.depth / 8;

		for (int i = tga->hdr.vert ? h - 1 : 0; (tga->hdr.vert && i >= 0) || (!tga->hdr.vert && i < h); i += tga->hdr.vert ? -1 : 1)
		{
			for (int j = 0; j < w * bpp; j += bpp)
			{
				tbyte r, g, b;
				if (tga->hdr.depth == 8)
				{
					if (TGA_IS_MAPPED(tga))
					{
						// read color table
						tuint16 idx = data->img_data[i*w*bpp + j] * tga->hdr.map_entry / 8;
						if (tga->hdr.map_entry == 8)
						{
							// invalid image ??
							_ASSERTE("Invalid color table depth" && 1 != 1);
							r = g = b = data->cmap[idx];
						}
						else if (tga->hdr.map_entry == 15 || tga->hdr.map_entry == 16)
						{
							tuint16 px = data->cmap[idx] + data->cmap[idx + 1] * 256;
							r = (tbyte)((((px >> 10) & 0x1F) * 255) / 31);
							g = (tbyte)((((px >>  5) & 0x1F) * 255) / 31);
							b = (tbyte)((((px >>  0) & 0x1F) * 255) / 31);
						}
						else
						{
							r = data->cmap[idx + 2];
							g = data->cmap[idx + 1];
							b = data->cmap[idx];
						}
					}
					else
						r = g = b = data->img_data[i*w*bpp + j];
				}
				else if (tga->hdr.depth == 15 || tga->hdr.depth == 16) // 16-bit depth
				{
					tuint16 px = data->img_data[i*w*bpp + j] + data->img_data[i*w*bpp + j + 1] * 256;
					r = (tbyte)((((px >> 10) & 0x1F) * 255) / 31);
					g = (tbyte)((((px >>  5) & 0x1F) * 255) / 31);
					b = (tbyte)((((px >>  0) & 0x1F) * 255) / 31);
				}
				else // 24 or 32-bit depth - ignore alpha
				{
					r = data->img_data[i*w*bpp + j];
					g = data->img_data[i*w*bpp + j + 1];
					b = data->img_data[i*w*bpp + j + 2];
				}
				*p++ = b; *p++ = g; *p++ = r;
			}
			for (int pad = ((3 * w) % 4) ? (4 - (3 * w) % 4) : 0; pad > 0; pad--) // row padding 24-bit
				*p++ = 0;
		}

		return ( p - pbuf );
	}

	HGLOBAL readTga( const std::wstring& fileName )
	{
		HGLOBAL hDataBuf = nullptr;

		TGA *tga = TGAOpen( fileName.c_str(), L"rb" );
		if( tga )
		{
			tga->error = TGAError_callback;

			TGAData data = { nullptr, nullptr, nullptr, TGA_IMAGE_ID | TGA_IMAGE_DATA | TGA_RGB };

			if( TGAReadImage( tga, &data ) == TGA_OK )
			{
				int bpp = 3; // always generate 24bit bmp instead of tga->hdr.depth / 8;
				auto fSize = 54 + tga->hdr.width * tga->hdr.height * bpp;

				if( ( bpp * tga->hdr.width ) % 4 ) // row padding
					fSize += ( 4 - ( bpp * tga->hdr.width ) % 4 ) * tga->hdr.height;

				hDataBuf = GlobalAlloc( GMEM_MOVEABLE, static_cast<SIZE_T>( fSize ) );

				unsigned char *pBuffer = nullptr;
				if( hDataBuf && ( pBuffer = static_cast<unsigned char*>( GlobalLock( hDataBuf ) ) ) )
				{
					auto ret = writeBmp( tga, &data, pBuffer );
					_ASSERTE( ret == fSize );
				}

				TGAFreeData( &data );
			}
			TGAClose( tga );
		}

		return hDataBuf;
	}

	HGLOBAL readPcx( const std::wstring& fileName )
	{
		HGLOBAL hDataBuf = nullptr;

		PCXContext *ctx = PCXOpen( fileName.c_str() );
		if( ctx )
		{
			unsigned char *imgData = PCXReadImage( ctx );
			if( imgData )
			{
				/* Allocate buffer */
				unsigned long rowsize = ctx->imageWidth * 3; // always generate 24-bit bmp

				if( rowsize % 4 ) // row padding
					rowsize += ( 4 - (rowsize) % 4 );

				auto bmpsize = 54 + rowsize * ctx->imageHeight; // 54 bmp header length

				hDataBuf = GlobalAlloc( GMEM_MOVEABLE, static_cast<SIZE_T>( bmpsize ) );

				unsigned char *pBuffer = nullptr;
				if( hDataBuf && ( pBuffer = static_cast<BYTE*>( GlobalLock( hDataBuf ) ) ) )
				{
					const char white = (const char)0xFF;
					int w = ctx->imageWidth, h = ctx->imageHeight;

					BYTE *p = IconUtils::writeBmpHeader24b( pBuffer, w, h );

					int bpp = ctx->pcxHdr.BitsPerPixel * ctx->pcxHdr.NumBitPlanes;

					/* Render image */
					for( int i = h - 1; i >= 0; --i )
					{
						BYTE r, g, b;
						for( int j = 0; j < ctx->pcxHdr.BytesPerLine; ++j )
						{
							if( ctx->pcxHdr.BitsPerPixel == 1 && j * 8 < w )
							{
								if( ctx->pcxHdr.NumBitPlanes == 1 )
								{
									/* Black and white */
									for( int k = 7; k >= 0 && j * 8 + (7 - k) < w; --k )
									{
										r = g = b = ( ( imgData[i*ctx->scanLineLength + j] >> k ) & 1 ) ? 0xFF : 0x00;
										*p++ = b; *p++ = g; *p++ = r;
									}
								}
								else if( ctx->pcxHdr.NumBitPlanes == 2 )
								{
									/* 4-color EGA palette */
									BYTE pl1 = imgData[i*ctx->scanLineLength + j + 0 * ctx->pcxHdr.BytesPerLine];
									BYTE pl2 = imgData[i*ctx->scanLineLength + j + 1 * ctx->pcxHdr.BytesPerLine];

									for( int k = 7; k >= 0 && j * 8 + (7 - k) < w; --k )
									{
										WORD idx = 0;
										idx |= ( ( ( pl1 >> k ) & 1 ) & 0x0F ) << 0;
										idx |= ( ( ( pl2 >> k ) & 1 ) & 0x0F ) << 1;
										r = ctx->pcxHdr.Palette[3*idx + 0];
										g = ctx->pcxHdr.Palette[3*idx + 1];
										b = ctx->pcxHdr.Palette[3*idx + 2];
										*p++ = b; *p++ = g; *p++ = r;
									}
								}
								else if( ctx->pcxHdr.NumBitPlanes == 3 )
								{
									/* 8-color EGA palette */
									BYTE pl1 = imgData[i*ctx->scanLineLength + j + 0 * ctx->pcxHdr.BytesPerLine];
									BYTE pl2 = imgData[i*ctx->scanLineLength + j + 1 * ctx->pcxHdr.BytesPerLine];
									BYTE pl3 = imgData[i*ctx->scanLineLength + j + 2 * ctx->pcxHdr.BytesPerLine];

									for( int k = 7; k >= 0 && j * 8 + (7 - k) < w; --k )
									{
										WORD idx = 0;
										idx |= ( ( ( pl1 >> k ) & 1 ) & 0x0F ) << 0;
										idx |= ( ( ( pl2 >> k ) & 1 ) & 0x0F ) << 1;
										idx |= ( ( ( pl3 >> k ) & 1 ) & 0x0F ) << 2;
										r = ctx->pcxHdr.Palette[3*idx + 0];
										g = ctx->pcxHdr.Palette[3*idx + 1];
										b = ctx->pcxHdr.Palette[3*idx + 2];
										*p++ = b; *p++ = g; *p++ = r;
									}
								}
								else if( ctx->pcxHdr.NumBitPlanes == 4 )
								{
									/* 16-color EGA palette */
									BYTE pl1 = imgData[i*ctx->scanLineLength + j + 0 * ctx->pcxHdr.BytesPerLine];
									BYTE pl2 = imgData[i*ctx->scanLineLength + j + 1 * ctx->pcxHdr.BytesPerLine];
									BYTE pl3 = imgData[i*ctx->scanLineLength + j + 2 * ctx->pcxHdr.BytesPerLine];
									BYTE pl4 = imgData[i*ctx->scanLineLength + j + 3 * ctx->pcxHdr.BytesPerLine];

									for( int k = 7; k >= 0 && j * 8 + (7 - k) < w; --k )
									{
										WORD idx = 0;
										idx |= ( ( ( pl1 >> k ) & 1 ) & 0x0F ) << 0;
										idx |= ( ( ( pl2 >> k ) & 1 ) & 0x0F ) << 1;
										idx |= ( ( ( pl3 >> k ) & 1 ) & 0x0F ) << 2;
										idx |= ( ( ( pl4 >> k ) & 1 ) & 0x0F ) << 3;
										r = ctx->pcxHdr.Palette[3*idx + 0];
										g = ctx->pcxHdr.Palette[3*idx + 1];
										b = ctx->pcxHdr.Palette[3*idx + 2];
										*p++ = b; *p++ = g; *p++ = r;
									}
								}
							}
							else if( ctx->pcxHdr.BitsPerPixel == 2 && j * 4 < w )
							{
								/* 4-color CGA palette: TODO */

								/* Get the CGA background color */
								BYTE cgaBackgroundColor = ctx->pcxHdr.Palette[0] >> 4;  /* 0 to 15 */

								/* Get the CGA foreground palette */
								BYTE cgaColorBurstEnable = (ctx->pcxHdr.Palette[3] & 0x80) >> 7;  /* 0 or 1 */
								BYTE cgaPaletteValue = (ctx->pcxHdr.Palette[3] & 0x40) >> 6;  /* 0 or 1 */
								BYTE cgaIntensityValue = (ctx->pcxHdr.Palette[3] & 0x20) >> 5;  /* 0 or 1 */

								for( int k = 3; k >= 0 && j * 4 + (3 - k) < w; --k )
								{
									WORD idx = ( imgData[i*ctx->scanLineLength + j] >> ( 2 * k ) ) & 3;
									r = ctx->pcxHdr.Palette[3*idx + 0];
									g = ctx->pcxHdr.Palette[3*idx + 1];
									b = ctx->pcxHdr.Palette[3*idx + 2];
									*p++ = b; *p++ = g; *p++ = r;
								}
							}
							else if( ctx->pcxHdr.BitsPerPixel == 4 && j * 2 < w )
							{
								/* 16-color EGA palette */
								WORD idx = ( ( imgData[i*ctx->scanLineLength + j] >> 4 ) & 0x0F );
								r = ctx->pcxHdr.Palette[3*idx + 0];
								g = ctx->pcxHdr.Palette[3*idx + 1];
								b = ctx->pcxHdr.Palette[3*idx + 2];
								*p++ = b; *p++ = g; *p++ = r;

								if( j * 2 + 1 < w )
								{
									idx = imgData[i*ctx->scanLineLength + j] & 0x0F;
									r = ctx->pcxHdr.Palette[3*idx + 0];
									g = ctx->pcxHdr.Palette[3*idx + 1];
									b = ctx->pcxHdr.Palette[3*idx + 2];
									*p++ = b; *p++ = g; *p++ = r;
								}
							}
							else if( bpp == 8 && j < w )
							{
								if(ctx->paletteVga)
								{
									/* 256-color VGA palette */
									WORD idx = imgData[i*ctx->scanLineLength + j] * 3;
									r = ctx->paletteVga[idx + 0];
									g = ctx->paletteVga[idx + 1];
									b = ctx->paletteVga[idx + 2];
								}
								else /* Gray-scale */
									r = g = b = imgData[i*ctx->scanLineLength + j];

								*p++ = b; *p++ = g; *p++ = r;
							}
							else if( bpp == 24 && j < w )
							{
								/* 24-bit true color */
								r = imgData[i*ctx->scanLineLength + j];
								g = imgData[i*ctx->scanLineLength + j + ctx->pcxHdr.BytesPerLine];
								b = imgData[i*ctx->scanLineLength + j + 2 * ctx->pcxHdr.BytesPerLine];
								*p++ = b; *p++ = g; *p++ = r;
							}
						}
						for( int pad = ((3 * w) % 4) ? (4 - (3 * w) % 4) : 0; pad > 0; pad-- ) // row padding 24-bit
							*p++ = 0;
					}
					_ASSERTE( ( p - pBuffer ) == bmpsize );
				}
			}

			PCXClose( ctx );
		}

		return hDataBuf;
	}

	void CImageViewer::onInit()
	{
		SendMessage( _hDlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>( IconUtils::getStock( SIID_IMAGEFILES ) ) );
		SendMessage( _hDlg, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>( IconUtils::getStock( SIID_IMAGEFILES, true ) ) );

		MiscUtils::centerOnWindow( FCS::inst().getFcWindow(), _hDlg );

		// tell windows that we can handle drag and drop
		DragAcceptFiles( _hDlg, TRUE );

		Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		Gdiplus::GdiplusStartup( &_gdiplusToken, &gdiplusStartupInput, NULL );

		_pBrush = new Gdiplus::SolidBrush( Gdiplus::Color::Black );
		_pGraph = nullptr;
		_pBitmap = nullptr;
		_pStream = nullptr;
		_hDataBuf = nullptr;

		// get dialog menu handle
		//_hMenu = GetMenu( _hDlg );

		_worker.init( [this] { return _loadImageData(); }, _hDlg, UM_STATUSNOTIFY );
	}

	bool CImageViewer::onClose()
	{
		show( SW_HIDE ); // hide dialog

		_worker.stop();

		return true;
	}

	void CImageViewer::onDestroy()
	{
		cleanUp();

		delete _pBrush;

		Gdiplus::GdiplusShutdown( _gdiplusToken );

		DestroyIcon( reinterpret_cast<HICON>( SendMessage( _hDlg, WM_GETICON, ICON_SMALL, 0 ) ) );
		DestroyIcon( reinterpret_cast<HICON>( SendMessage( _hDlg, WM_GETICON, ICON_BIG, 0 ) ) );
	}

	void CImageViewer::cleanUp()
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

			_pBitmap = nullptr;
			_pStream = nullptr;
			_hDataBuf = nullptr;
		}
	}

	bool CImageViewer::_loadImageData()
	{
		// testing - convert jp2, pcx, tga to bmp
		if( ImageType::getType( _tempName ) == ImageType::EImageType::FMT_JP2K )
			_hDataBuf = decodeJp2( PathUtils::getExtendedPath( _tempName ) );
		else if( ImageType::getType( _tempName ) == ImageType::EImageType::FMT_PCX )
			_hDataBuf = readPcx( PathUtils::getExtendedPath( _tempName ) );
		else if( ImageType::getType( _tempName ) == ImageType::EImageType::FMT_TGA )
			_hDataBuf = readTga( PathUtils::getExtendedPath( _tempName ) );
		else
		{
			WIN32_FIND_DATA wfd;
			if( FsUtils::getFileInfo( _tempName, wfd ) )
			{
				auto fSize = FsUtils::getFileSize( &wfd );

				// load image data into memory
				HANDLE hFile = CreateFile( PathUtils::getExtendedPath( _tempName ).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
				if( hFile != INVALID_HANDLE_VALUE )
				{
					_hDataBuf = GlobalAlloc( GMEM_MOVEABLE, static_cast<SIZE_T>( fSize ) );

					bool ret = false;
					void *pBuffer = nullptr;
					if( _hDataBuf && ( pBuffer = GlobalLock( _hDataBuf ) ) )
					{
						DWORD read = 0;
						ReadFile( hFile, pBuffer, static_cast<DWORD>( fSize ), &read, NULL );

						ret = ( read == static_cast<DWORD>( fSize ) );
					}

					CloseHandle( hFile );

					return ret;
				}
			}
		}

		return !!_hDataBuf;
	}

	bool CImageViewer::viewFile( const std::wstring& dirName, const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel )
	{
		_fileName = PathUtils::addDelimiter( dirName ) + fileName;

		if( spPanel )
		{
			_spPanel = spPanel;
			_entries.clear();

			for( auto entry : spPanel->getDataManager()->getFileEntries() )
			{
				if( ImageType::isKnownType( entry->wfd.cFileName ) )
					_entries.push_back( spPanel->getDataManager()->getEntryPathFull( entry->wfd.cFileName ) );
			}

			return _spPanel->getDataManager()->getDataProvider()->readToFile( _fileName, spPanel, _hDlg );
		}
		else
			viewFileCore( _fileName );

		return true;
	}

	void CImageViewer::viewFileCore( const std::wstring& fileName )
	{
		_tempName = fileName;

		_worker.stop();
		cleanUp();

		std::wostringstream sstrTitle;
		sstrTitle << _tempName << L" - Viewer";
		
		SetWindowText( _hDlg, sstrTitle.str().c_str() );

		SetCursor( LoadCursor( NULL, IDC_WAIT ) );

		_worker.start();
	}

	void CImageViewer::viewNext()
	{
		auto it = std::find( _entries.begin(), _entries.end(), _fileName );

		if( it != _entries.end() && it + 1 != _entries.end() )
		{
			_fileName = *( it + 1 );
			_spPanel->getDataManager()->getDataProvider()->readToFile( _fileName, _spPanel, _hDlg );
		}
	}

	void CImageViewer::viewPrevious()
	{
		auto it = std::find( _entries.begin(), _entries.end(), _fileName );

		if( it != _entries.end() && it != _entries.begin() )
		{
			_fileName = *( it - 1 );
			_spPanel->getDataManager()->getDataProvider()->readToFile( _fileName, _spPanel, _hDlg );
		}
	}

	void CImageViewer::displayImage()
	{
		if( _pBitmap && _pBitmap->GetLastStatus() == Gdiplus::Status::Ok )
		{
			//Gdiplus::ImageAttributes attr;
			//attr.SetColorKey( Gdiplus::Color::Magenta, Gdiplus::Color::Magenta );
			//attr.Reset();

			auto imgRect = IconUtils::calcRectFit( _hDlg, _pBitmap->GetWidth(), _pBitmap->GetHeight() );

			RECT wndRect;
			GetClientRect( _hDlg, &wndRect );

			// border strips width and height
			int w = ( wndRect.right - imgRect.Width ) / 2;
			int h = ( wndRect.bottom - imgRect.Height ) / 2;

			_pGraph = Gdiplus::Graphics::FromHWND( _hDlg );

			// draw border strips
			_pGraph->FillRectangle( _pBrush, 0, 0, wndRect.right, h ); // top
			_pGraph->FillRectangle( _pBrush, 0, h + imgRect.Height - 1, wndRect.right, h + 2 ); // bottom
			_pGraph->FillRectangle( _pBrush, 0, h, w, imgRect.Height ); // left
			_pGraph->FillRectangle( _pBrush, w + imgRect.Width - 1, h, w + 2, imgRect.Height ); // right

			// redraw entire window
		//	_pGraph->FillRectangle( _pBrush, 0, 0, wndRect.right, wndRect.bottom );

			_pGraph->DrawImage(
				_pBitmap,
				imgRect,
				0, 0, _pBitmap->GetWidth(), _pBitmap->GetHeight(),
				Gdiplus::UnitPixel,
				nullptr );
				//&attr );
		}
	}

	void CImageViewer::saveImage()
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
				sstr << PathUtils::stripFileExtension( PathUtils::stripPath( _fileName ) ) << L".bmp";

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
				ofn.lpstrTitle = L"Save Image As";
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
							// use the first extension
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

	void CImageViewer::handleDropFiles( HDROP hDrop )
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

	void CImageViewer::handleContextMenu( POINT pt )
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
			AppendMenu( hMenu, MF_STRING, 3, L"Fullscreen\tF11" );
			AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );
			AppendMenu( hMenu, MF_STRING, 4, L"Save As...\tCtrl+S" );

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
				fullscreen( !_isFullscreen );
				break;
			case 4:
				saveImage();
				break;
			default:
				break;
			}

			DestroyMenu( hMenu );
			hMenu = NULL;
		}
	}

	INT_PTR CALLBACK CImageViewer::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_COMMAND:
			//handleMenuCommands( LOWORD( wParam ) );
			break;

		case WM_KEYDOWN:
			switch( wParam )
			{
			case VK_F11:
			case 0x46: // 'F' - go fullscreen
				fullscreen( !_isFullscreen );
				break;
			case 0x53: // 'S' - save image
				if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
					saveImage();
				break;
			case VK_SPACE:
				viewNext();
				break;
			case VK_BACK:
				viewPrevious();
				break;
			case VK_INSERT:
				if( _spPanel )
					_spPanel->markEntry( PathUtils::stripPath( _fileName ) );
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
				MessageBox( _hDlg, sstr.str().c_str(), L"View Image", MB_OK | MB_ICONEXCLAMATION );
				close();
			}
			break;

		case UM_STATUSNOTIFY:
			SetCursor( NULL );
			if( wParam )
			{
				// create IStream from global data buffer
				if( CreateStreamOnHGlobal( _hDataBuf, FALSE, &_pStream ) == S_OK )
				{
					_pBitmap = Gdiplus::Bitmap::FromStream( _pStream );
					displayImage();
				}
			}
			else
			{
				std::wstringstream sstr;
				sstr << L"Cannot open \"" << _fileName << L"\" file.";
				MessageBox( _hDlg, sstr.str().c_str(), L"View Image", MB_OK | MB_ICONEXCLAMATION );
				close();
			}
			break;

		case WM_DROPFILES:
			handleDropFiles( (HDROP)wParam );
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

		case WM_CONTEXTMENU:
			handleContextMenu( POINT { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) } );
			break;

		case WM_PAINT:
		case WM_SIZE:
			displayImage();
			break;
		}

		return (INT_PTR)false;
	}
}
