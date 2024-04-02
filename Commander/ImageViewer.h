#pragma once

#include "BaseViewer.h"

namespace Commander
{
	class CImageViewer : public CBaseViewer
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
		//void handleMenuCommands( WORD menuId );
		void handleDropFiles( HDROP hDrop );
		void handleContextMenu( POINT pt );

	private:
		void viewFileCore( const std::wstring& fileName );
		void viewNext();
		void viewPrevious();
		void displayImage();
		void saveImage();
		void cleanUp();
		bool _loadImageData();

	private:
		ULONG_PTR _gdiplusToken;
		HGLOBAL _hDataBuf;
		IStream *_pStream;

		Gdiplus::Graphics *_pGraph;
		Gdiplus::Bitmap *_pBitmap;
		Gdiplus::SolidBrush *_pBrush;

		std::vector<std::wstring> _entries;

		std::wstring _tempName;

		//HMENU _hMenu;
	};
}
