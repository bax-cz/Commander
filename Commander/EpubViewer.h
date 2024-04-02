#pragma once

#include "BaseViewer.h"
#include "WebBrowser.h"

namespace Commander
{
	class CEpubViewer : public CBaseViewer
	{
	public:
		static const UINT resouceIdTemplate = IDD_IMAGEVIEWER;

	private:
		struct EpubData {
			// file names and location
			std::wstring opfFileName; // opf location is the epub's data root
			std::wstring ncxFileName;

			// content.opf data
			std::wstring opfTitle;
			std::wstring opfCreator;
			std::wstring opfSubject;
			std::wstring opfLanguage;
			std::wstring opfDate;

			std::vector<std::pair<std::wstring, std::wstring>> opfSpine;
			std::vector<std::pair<std::wstring, std::wstring>> opfGuide;

			// toc.ncx data
			std::wstring ncxDocTitle;
			std::wstring ncxDocAuthor;
			std::wstring ncxXmlData;
			std::vector<std::pair<std::wstring, std::wstring>> ncxNavPoints;
		};

	public:
		virtual bool viewFile( const std::wstring& dirName, const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel = nullptr ) override;

		virtual void onInit() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		void viewHtml( const std::wstring& fileName );
		void viewEpub( const std::wstring& fileName );

		void webBrowserProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

	private:
		std::wstring getUrlLink( const std::wstring& linkName );
		std::wstring getLocalDirectoryName();
		void applyRenamedFiles();
		//void handleMenuCommands( WORD menuId );
		//void handleDropFiles( HDROP hDrop );
		//void handleContextMenu( POINT pt );

	private:
		void viewNext();
		void viewPrevious();
		void handleKeyboard( WPARAM key );
		bool readEpubData();
		bool extractEpubData();
		bool parseContainerFile( const std::wstring& fileName );
		bool parsePackageFile( const std::wstring& fileName );
	//	bool parseNavigationFile( const std::wstring& fileName );
		void loadNavigationFile( const std::wstring& fileName );
		void readNavPoints( IXMLDOMNodeList *pNodeList, HTREEITEM hRoot );
		void initXmlEntitiesMap();
		void initTreeView();

		bool _workerProc();

	private:
		std::unique_ptr<CWebBrowser> _upWebBrowser;
		std::unique_ptr<CArchiver> _upArchiver;

		std::map<std::wstring, std::wstring> _xmlEntities;
		std::set<std::pair<std::wstring, std::wstring>> _duplicateFiles; // renamed files due to case-insensitive filesystem
		std::set<std::pair<std::wstring, std::wstring>> _renamedFiles; // renamed .html to .xhtml files
		std::wstring _processingPath;

		EpubData _epubData;

		HWND _hTreeView;
		RECT _rctWbObj;

		ULONGLONG _scrollTicks;
		bool _canScrollPage; // can we scroll to the next page (cooldown period elapsed)

		//HMENU _hMenu;
	};
}
