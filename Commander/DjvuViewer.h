#pragma once

#include "BaseViewer.h"

#include "../Djvu/ddjvuapi.h"

namespace Commander
{
	class CDjvuViewer : public CBaseViewer
	{
	public:
		static const UINT resouceIdTemplate = IDD_IMAGEVIEWER;

	public:
		virtual bool viewFile( const std::wstring& dirName, const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel = nullptr ) override;

		virtual void onInit() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		void handleKeyboard( WPARAM key );
		void handleContextMenu( POINT pt );
		void handleDropFiles( HDROP hDrop );
		//void handleMenuCommands( WORD menuId );

	private:
		void viewFileCore( const std::wstring& fileName );
		void viewNext();
		void viewPrevious();
		void displayImage();
		void gotoPage();
		void refresh();
		void cleanUp();

		void saveCurrentPageAsImage();
		void getPageText( int pageno );

		bool renderPage( ddjvu_page_t *page, int pageno );
		bool _loadDjvuData();

	private:
		ddjvu_context_t *_djvuCtx;
		ddjvu_document_t *_djvuDoc;

		int _pagesCnt;
		int _pageIdx;

		ULONG_PTR _gdiplusToken;
		HGLOBAL _hDataBuf;
		BYTE *_pBuff;
		UINT _buffLen;
		IStream *_pStream;

		Gdiplus::Graphics *_pGraph;
		Gdiplus::Bitmap *_pBitmap;
		Gdiplus::SolidBrush *_pBrush;

		std::vector<std::wstring> _entries;
		std::wstring _tempName;

		std::wstring _pageText;

		//HMENU _hMenu;
	};
}
