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
		void selectActiveFrame();
		void generateFrame();
		void saveImage();
		void cleanUp();
		bool loadGifInfo();
		bool _loadImageData();

	private:
		ULONG_PTR _gdiplusToken;
		HGLOBAL _hDataBuf;
		IStream *_pStream;

		std::unique_ptr<Gdiplus::Graphics> _upGraphics;
		std::unique_ptr<Gdiplus::SolidBrush> _upBrush;
		std::unique_ptr<Gdiplus::Image> _upImage;
		std::unique_ptr<Gdiplus::Bitmap> _upFrameBuffer;

		std::vector<std::pair<HGLOBAL, UINT>> _frames;
		int _frameIdx;

		std::vector<std::wstring> _entries;

		std::wstring _tempName;

		bool _isAnimated;
		bool _isPlaying;

		//HMENU _hMenu;
	};
}
