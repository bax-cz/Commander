#pragma once

/*
	Icon and bitmap utils - helper functions to make life easier
*/

#include <gdiplus.h>
#include <Uxtheme.h>

#define BMP_HEADER_SIZE             54
#define BMP_HEADER_ALPHA_EXTRA_SIZE 16  /* for alpha info */

namespace Commander
{
	struct IconUtils
	{
		//
		// Obtain icon from given path - caller must DestroyIcon()
		//
		static HICON getFromPath( const LPCWSTR path, bool largeIcon = false )
		{
			SHFILEINFO shfi = { 0 };
			SHGetFileInfo( path, 0, &shfi, sizeof( SHFILEINFO ), SHGFI_ICON | ( largeIcon ? SHGFI_LARGEICON : SHGFI_SMALLICON ) );

			return shfi.hIcon;
		}

		//
		// Obtain icon index from given path
		//
		static int getFromPathIndex( const LPCWSTR path )
		{
			SHFILEINFO shfi = { 0 };
			SHGetFileInfo( path, 0, &shfi, sizeof( shfi ), SHGFI_SYSICONINDEX );

			return shfi.iIcon;
		}

		//
		// Obtain icon from given path with file attributes - caller must DestroyIcon()
		//
		static HICON getFromPathWithAttrib( const LPCWSTR path, DWORD fileAttr, bool largeIcon = false )
		{
			SHFILEINFO shfi = { 0 };
			SHGetFileInfo( path, fileAttr, &shfi, sizeof( shfi ), SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | ( largeIcon ? SHGFI_LARGEICON : SHGFI_SMALLICON ) );

			return shfi.hIcon;
		}

		//
		// Obtain icon index from given path with file attributes
		//
		static int getFromPathWithAttribIndex( const LPCWSTR path, DWORD fileAttr )
		{
			SHFILEINFO shfi = { 0 };
			SHGetFileInfo( path, fileAttr, &shfi, sizeof( shfi ), SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES );

			return shfi.iIcon;
		}

		//
		// Obtain icon from stock icons - caller must DestroyIcon()
		//
		static HICON getStock( SHSTOCKICONID iconId, bool largeIcon = false )
		{
			SHSTOCKICONINFO ssii = { 0 };
			ssii.cbSize = sizeof( ssii );

			SHGetStockIconInfo( iconId, SHGSI_ICON | ( largeIcon ? SHGSI_LARGEICON : SHGSI_SMALLICON ), &ssii );

			return ssii.hIcon;
		}

		//
		// Obtain icon index from stock icons
		//
		static int getStockIndex( SHSTOCKICONID iconId )
		{
			SHSTOCKICONINFO ssii = { 0 };
			ssii.cbSize = sizeof( ssii );

			SHGetStockIconInfo( iconId, SHGSI_SYSICONINDEX, &ssii );

			return ssii.iSysImageIndex;
		}

		//
		// Obtain icon from special folder - caller must DestroyIcon()
		//
		static HICON getSpecial( const HWND hw, int csidl, bool largeIcon = false )
		{
			SHFILEINFO shfi = { 0 };

			if( csidl != CSIDL_FLAG_MASK )
			{
				PIDLIST_ABSOLUTE pidl;
				auto hr = SHGetSpecialFolderLocation( hw, csidl, &pidl );
				if( SUCCEEDED( hr ) )
				{
					SHGetFileInfo( (LPCWSTR)pidl, 0, &shfi, sizeof( SHFILEINFO ), SHGFI_ICON | SHGFI_PIDL | ( largeIcon ? SHGFI_LARGEICON : SHGFI_SMALLICON ) );
					CoTaskMemFree( pidl );
				} 
			}

			return shfi.hIcon;
		}

		//
		// Obtain icon index from special folder
		//
		static int getSpecialIndex( const HWND hw, int csidl )
		{
			SHFILEINFO shfi = { 0 };

			if( csidl != CSIDL_FLAG_MASK )
			{
				PIDLIST_ABSOLUTE pidl;
				auto hr = SHGetSpecialFolderLocation( hw, csidl, &pidl );
				if( SUCCEEDED( hr ) )
				{
					SHGetFileInfo( (LPCWSTR)pidl, 0, &shfi, sizeof( SHFILEINFO ), SHGFI_SYSICONINDEX | SHGFI_PIDL );
					CoTaskMemFree( pidl );
				} 
			}

			return shfi.iIcon;
		}

		//
		// Combine two icons to make an overlay over main icon - caller must DestroyIcon()
		//
		static HICON makeOverlayIcon( const HWND hw, const HICON hIcon, const HICON hOverlay )
		{
			// create a memory DC on a bitmap
			auto hdc = GetDC( hw );
			auto hMemDc = CreateCompatibleDC( hdc );
			auto hBmp = CreateCompatibleBitmap( hdc, 16, 16 );
			auto hOldBmp = SelectObject( hMemDc, hBmp );

			// draw the two icons on top of each other
			DrawIconEx( hMemDc, 0, 0, hIcon, 16, 16, 0, NULL, DI_IMAGE );
			DrawIconEx( hMemDc, 0, 0, hOverlay, 16, 16, 0, NULL, DI_NORMAL );

			// create an icon from the bitmap
			ICONINFO ii = { 0 };
			ii.fIcon = TRUE;
			ii.hbmMask = hBmp;
			ii.hbmColor = hBmp;
			auto hIconNew = CreateIconIndirect( &ii );

			// clean up drawing
			SelectObject( hMemDc, hOldBmp );
			DeleteObject( hBmp );
			DeleteDC( hMemDc );
			ReleaseDC( hw, hdc );

			return hIconNew;
		}

		//
		// Draw small icon at given position
		//
		static void drawIconSmall( const HDC hdc, const DWORD resId, const RECT *rct )
		{
			HICON hIcon = (HICON)LoadImage( FCS::inst().getFcInstance(), MAKEINTRESOURCE( resId ), IMAGE_ICON, 16, 16, LR_SHARED );
			DrawIconEx( hdc, rct->left, rct->top, hIcon, 16, 16, 0, NULL, DI_NORMAL );
			DestroyIcon( hIcon );
		}

		//
		// Draw solid color filled focus rectangle for list-view entries panel
		//
		static void drawFocusRectangle( HDC hdc, RECT& rct )
		{
			// draw filled selection rectangle
			Rectangle( hdc, rct.left, rct.top, rct.right, rct.bottom );
			rct.left++; rct.top++; rct.right--; rct.bottom--;

			HRGN bgRgn = CreateRectRgnIndirect( &rct );
			HBRUSH bgBrush = CreateSolidBrush( FC_COLOR_FOCUSRECT );

			FillRgn( hdc, bgRgn, bgBrush );

			DeleteObject( bgBrush );
			DeleteObject( bgRgn );
		}

		//
		// Load .png from resource - Gdiplus must be initialized to use this function - caller must delete object
		//
		/*static Gdiplus::Bitmap *loadPngFromResource( HMODULE hInst, LPCTSTR pName )
		{
			Gdiplus::Bitmap *pBitmap = nullptr;

			HRSRC hResource = FindResource( hInst, pName, L"PNG" );
			if( !hResource )
				return pBitmap;

			DWORD imageSize = SizeofResource( hInst, hResource );
			if( !imageSize )
				return pBitmap;

			const void *pResourceData = LockResource( LoadResource( hInst, hResource ) );
			if( !pResourceData )
				return pBitmap;

			auto hBuffer = GlobalAlloc( GMEM_MOVEABLE, imageSize );
			if( hBuffer )
			{
				void *pBuffer = GlobalLock( hBuffer );
				if( pBuffer )
				{
					CopyMemory( pBuffer, pResourceData, imageSize );

					IStream *pStream = nullptr;
					if( CreateStreamOnHGlobal( hBuffer, FALSE, &pStream ) == S_OK )
					{
						pBitmap = Gdiplus::Bitmap::FromStream( pStream );
						pStream->Release();
					}
					GlobalUnlock( hBuffer );
				}
				GlobalFree( hBuffer );
				hBuffer = nullptr;
			}

			return pBitmap;
		}*/

		//
		// Calculate rectangle to fit inside given window
		//
		static Gdiplus::Rect calcRectFit( HWND hWnd, UINT width, UINT height )
		{
			double scale = 100.0;
			double x = 0.0, y = 0.0;
			double w = (double)width;
			double h = (double)height;

			RECT rct;
			GetClientRect( hWnd, &rct );

			double clientWid = (double)( rct.right - rct.left );
			double clientHei = (double)( rct.bottom - rct.top );

			if( clientWid < (double)width && clientHei < (double)height )
			{
				double coefW = clientWid / (double)width;
				double coefH = clientHei / (double)height;
				double coef = min( coefW, coefH );
				w = (double)width * coef;
				h = (double)height * coef;
				x = ( clientWid - w ) / 2.0;
				y = ( clientHei - h ) / 2.0;
			}
			else if( clientWid < (double)width )
			{
				double coef = clientWid / (double)width;
				w = (double)width * coef;
				h = (double)height * coef;
				x = ( clientWid - w ) / 2.0;
				y = ( clientHei - h ) / 2.0;
			}
			else if( clientHei < (double)height )
			{
				double coef = clientHei / (double)height;
				w = (double)width * coef;
				h = (double)height * coef;
				x = ( clientWid - w ) / 2.0;
				y = ( clientHei - h ) / 2.0;
			}
			else
			{
				x = ( clientWid - (double)width ) / 2.0;
				y = ( clientHei - (double)height ) / 2.0;
			}

			return Gdiplus::Rect( (UINT)x, (UINT)y, (UINT)w, (UINT)h );
		}

		//
		// Grayscale ColorMatrix definition - for inactive toolbar buttons
		//
		static std::unique_ptr<Gdiplus::ColorMatrix> getColorMatrixGrayscale()
		{
			Gdiplus::ColorMatrix colorMatrixGrayscale = {
				 .3f,  .3f,  .3f,    0,    0,
				.59f, .59f, .59f,    0,    0,
				.11f, .11f, .11f,    0,    0,
				   0,    0,    0,    1,    0,
				   0,    0,    0,    0,    1
			};

			return std::make_unique<Gdiplus::ColorMatrix>( colorMatrixGrayscale );
		}

		//
		// Masked ColorMatrix definition - for disabled toolbar buttons
		//
		static std::unique_ptr<Gdiplus::ColorMatrix> getColorMatrixMask()
		{
			Gdiplus::ColorMatrix colorMatrixMask = {
				   0,    0,    0,    0,    0,
				   0,    0,    0,    0,    0,
				   0,    0,    0,    0,    0,
				   0,    0,    0,    1,    0,
				 .7f,  .7f,  .7f,    0,    1
			};

			return std::make_unique<Gdiplus::ColorMatrix>( colorMatrixMask );
		}

		//
		// Create grayscale toolbar imagelist from bitmap
		//
		static HIMAGELIST createToolbarImageList( Gdiplus::Bitmap *bmpOrig, std::unique_ptr<Gdiplus::ColorMatrix> matrix = nullptr )
		{
			Gdiplus::Bitmap newBmp( bmpOrig->GetWidth(), bmpOrig->GetHeight() );
			Gdiplus::Graphics *g = Gdiplus::Graphics::FromImage( &newBmp );

			Gdiplus::ImageAttributes attr;
			attr.SetColorKey( Gdiplus::Color::Magenta, Gdiplus::Color::Magenta );

			if( matrix )
			{
				attr.SetColorMatrix( matrix.get() );
			}

			g->Clear( Gdiplus::Color::Transparent );
			g->DrawImage(
				bmpOrig,
				Gdiplus::Rect( 0, 0, bmpOrig->GetWidth(), bmpOrig->GetHeight() ),
				0, 0, bmpOrig->GetWidth(), bmpOrig->GetHeight(),
				Gdiplus::UnitPixel,
				&attr );

			HIMAGELIST imgList = NULL;
			HBITMAP hBmp = NULL;

			if( newBmp.GetHBITMAP( Gdiplus::Color::Transparent, &hBmp ) == Gdiplus::Status::Ok )
			{
				imgList = ImageList_Create( bmpOrig->GetHeight(), bmpOrig->GetHeight(), ILC_COLOR32, bmpOrig->GetWidth() / bmpOrig->GetHeight(), 0 );
				ImageList_Add( imgList, hBmp, NULL );

				DeleteObject( hBmp );
			}

			return imgList;
		}

		//
		// Create 32bit bitmap - for icon to bitmap conversion function
		//
		static HRESULT Create32BitHBITMAP(HDC hdc, const SIZE *psize, __deref_opt_out void **ppvBits, __out HBITMAP* phBmp)
		{
			if (psize == 0)
				return E_INVALIDARG;

			if (phBmp == 0)
				return E_POINTER;

			*phBmp = NULL;

			BITMAPINFO bmi = { 0 };
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biCompression = BI_RGB;

			bmi.bmiHeader.biWidth = psize->cx;
			bmi.bmiHeader.biHeight = psize->cy;
			bmi.bmiHeader.biBitCount = 32;

			HDC hdcUsed = hdc ? hdc : GetDC(NULL);
			if (hdcUsed)
			{
				*phBmp = CreateDIBSection(hdcUsed, &bmi, DIB_RGB_COLORS, ppvBits, NULL, 0);
				if (hdc != hdcUsed)
				{
					ReleaseDC(NULL, hdcUsed);
				}
			}

			return (NULL == *phBmp) ? E_OUTOFMEMORY : S_OK;
		}

		//
		// Helper for icon to bitmap conversion function
		//
		static HRESULT ConvertBufferToPARGB32(HPAINTBUFFER hPaintBuffer, HDC hdc, HICON hicon, SIZE& sizIcon)
		{
			RGBQUAD *prgbQuad;
			int cxRow;

			HRESULT hr = GetBufferedPaintBits(hPaintBuffer, &prgbQuad, &cxRow);
			if (FAILED(hr))
				return hr;

			Gdiplus::ARGB *pargb = reinterpret_cast<Gdiplus::ARGB *>(prgbQuad);
			if (HasAlpha(pargb, sizIcon, cxRow))
				return S_OK;

			ICONINFO info;
			if (!GetIconInfo(hicon, &info))
				return S_OK;

			if (info.hbmMask) {
				DeleteObject(info.hbmColor);
				DeleteObject(info.hbmMask);
				return ConvertToPARGB32(hdc, pargb, info.hbmMask, sizIcon, cxRow);
			}

			DeleteObject(info.hbmColor);
			DeleteObject(info.hbmMask);

			return S_OK;
		}

		//
		// Helper for icon to bitmap conversion function
		//
		static HRESULT ConvertToPARGB32(HDC hdc, __inout Gdiplus::ARGB *pargb, HBITMAP hbmp, SIZE& sizImage, int cxRow)
		{
			BITMAPINFO bmi = { 0 };
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biCompression = BI_RGB;

			bmi.bmiHeader.biWidth = sizImage.cx;
			bmi.bmiHeader.biHeight = sizImage.cy;
			bmi.bmiHeader.biBitCount = 32;

			HANDLE hHeap = GetProcessHeap();
			void *pvBits = HeapAlloc(hHeap, 0, bmi.bmiHeader.biWidth * 4 * bmi.bmiHeader.biHeight);
			if (pvBits == 0)
				return E_OUTOFMEMORY;

			if (GetDIBits(hdc, hbmp, 0, bmi.bmiHeader.biHeight, pvBits, &bmi, DIB_RGB_COLORS) != bmi.bmiHeader.biHeight) {
				HeapFree(hHeap, 0, pvBits);
				return E_UNEXPECTED;
			}

			ULONG cxDelta = cxRow - bmi.bmiHeader.biWidth;
			Gdiplus::ARGB *pargbMask = static_cast<Gdiplus::ARGB *>(pvBits);

			for (ULONG y = bmi.bmiHeader.biHeight; y; --y)
			{
				for (ULONG x = bmi.bmiHeader.biWidth; x; --x)
				{
					if (*pargbMask++)
					{
						// transparent pixel
						*pargb++ = 0;
					}
					else
					{
						// opaque pixel
						*pargb++ |= 0xFF000000;
					}
				}

				pargb += cxDelta;
			}

			HeapFree(hHeap, 0, pvBits);

			return S_OK;
		}

		//
		// Helper for icon to bitmap conversion function
		//
		static bool HasAlpha(__in Gdiplus::ARGB *pargb, SIZE& sizImage, int cxRow)
		{
			ULONG cxDelta = cxRow - sizImage.cx;
			for (ULONG y = sizImage.cy; y; --y)
			{
				for (ULONG x = sizImage.cx; x; --x)
				{
					if (*pargb++ & 0xFF000000)
					{
						return true;
					}
				}

				pargb += cxDelta;
			}

			return false;
		}

		//
		// Convert icon to bitmap (from tortoise svn)
		//
		static HBITMAP convertIconToBitmap( HICON hIcon, int width, int height )
		{
			if( !hIcon )
				return NULL;

			SIZE sizIcon;
			sizIcon.cx = width;
			sizIcon.cy = height;

			RECT rcIcon;
			SetRect( &rcIcon, 0, 0, sizIcon.cx, sizIcon.cy );

			HDC hdcDest = CreateCompatibleDC( NULL );
			if( !hdcDest )
				return nullptr;

			HBITMAP hBmp = nullptr;
			if( FAILED( Create32BitHBITMAP( hdcDest, &sizIcon, NULL, &hBmp ) ) ) {
				DeleteDC( hdcDest );
				return nullptr;
			}
			HBITMAP hbmpOld = (HBITMAP)SelectObject( hdcDest, hBmp );
			if( !hbmpOld ) {
				DeleteDC( hdcDest );
				return hBmp;
			}

			BLENDFUNCTION bfAlpha = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
			BP_PAINTPARAMS paintParams = { 0 };
			paintParams.cbSize = sizeof( paintParams );
			paintParams.dwFlags = BPPF_ERASE;
			paintParams.pBlendFunction = &bfAlpha;

			HDC hdcBuffer;
			HPAINTBUFFER hPaintBuffer = BeginBufferedPaint( hdcDest, &rcIcon, BPBF_DIB, &paintParams, &hdcBuffer );
			if( hPaintBuffer )
			{
				if( DrawIconEx( hdcBuffer, 0, 0, hIcon, sizIcon.cx, sizIcon.cy, 0, NULL, DI_NORMAL ) )
				{
					// If icon did not have an alpha channel we need to convert buffer to PARGB
					ConvertBufferToPARGB32( hPaintBuffer, hdcDest, hIcon, sizIcon );
				}

				// This will write the buffer contents to the destination bitmap
				EndBufferedPaint( hPaintBuffer, TRUE );
			}

			SelectObject( hdcDest, hbmpOld );
			DeleteDC( hdcDest );

			return hBmp;
		}

		//
		// Helper function to put 2 byte value (little endian) into the memory buffer
		//
		static void put16LE( BYTE *buf, UINT value )
		{
			buf[0] = ( value >> 0 ) & 0xFF;
			buf[1] = ( value >> 8 ) & 0xFF;
		}

		//
		// Helper function to put 4 byte value (little endian) into the memory buffer
		//
		static void put32LE( BYTE *buf, UINT value )
		{
			put16LE( buf + 0, ( value >> 0  ) & 0xFFFF );
			put16LE( buf + 2, ( value >> 16 ) & 0xFFFF );
		}

		//
		// Create and write BMP header with given Width and Height for 24/32-bit bitmap into the memory buffer
		//
		static BYTE *writeBmpHeader( BYTE *buf, int width, int height, bool has_alpha = true )
		{
			int header_size = BMP_HEADER_SIZE + ( has_alpha ? BMP_HEADER_ALPHA_EXTRA_SIZE : 0 );
			int bytes_per_px = has_alpha ? 4 : 3;
			int line_size = bytes_per_px * width;
			int bmp_stride = ( line_size + 3 ) & ~3;  // pad to 4
			int image_size = bmp_stride * height;
			int total_size = image_size + header_size;

			// bitmap file header
			put16LE( buf + 0, 0x4D42 );                // signature 'BM'
			put32LE( buf + 2, total_size );            // size including header
			put32LE( buf + 6, 0 );                     // reserved
			put32LE( buf + 10, header_size );          // offset to pixel array

			// bitmap info header
			put32LE( buf + 14, header_size - 14 );     // DIB header size
			put32LE( buf + 18, width );                // dimensions
			put32LE( buf + 22, height );               // no vertical flip
			put16LE( buf + 26, 1 );                    // number of planes
			put16LE( buf + 28, bytes_per_px * 8 );     // bits per pixel
			put32LE( buf + 30, has_alpha ? BI_BITFIELDS : BI_RGB );
			put32LE( buf + 34, image_size );
			put32LE( buf + 38, 2400 );                 // x pixels/meter
			put32LE( buf + 42, 2400 );                 // y pixels/meter
			put32LE( buf + 46, 0 );                    // number of palette colors
			put32LE( buf + 50, 0 );                    // important color count

			if( has_alpha ) // BITMAPV3INFOHEADER complement
			{
				put32LE( buf + 54, 0x00FF0000 );       // red mask
				put32LE( buf + 58, 0x0000FF00 );       // green mask
				put32LE( buf + 62, 0x000000FF );       // blue mask
				put32LE( buf + 66, 0xFF000000 );       // alpha mask
			}

			return buf + header_size;
		}

		//
		// Get relative window rect position
		//
		static RECT getControlRect( HWND hWnd )
		{
			RECT rct;
			GetWindowRect( hWnd, &rct );
			MapWindowPoints( HWND_DESKTOP, GetParent( hWnd ), (LPPOINT) &rct, 2 );

			return rct;
		}

		// do not instantiate this class/struct
		IconUtils() = delete;
		IconUtils( IconUtils const& ) = delete;
		void operator=( IconUtils const& ) = delete;
	};
}
