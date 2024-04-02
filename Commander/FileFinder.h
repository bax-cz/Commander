#pragma once

#include <fstream>
#include <memory>
#include <mutex>
#include <regex>

#include "BaseDialog.h"
#include "BackgroundWorker.h"
#include "DragDropUtils.h"

namespace Commander
{
	class CFileFinder : public CBaseDialog, public CDragDropHelper
	{
	public:
		static const UINT resouceIdTemplate = IDD_FINDITEMS;

	private:
		struct FindItemData {
			FindItemData( const std::wstring& Path, const WIN32_FIND_DATA& Wfd, StringUtils::EUtfBom Bom, int ImgIdx )
			{
				path = Path; wfd = Wfd; bom = Bom; imgIdx = ImgIdx;
			}

			WIN32_FIND_DATA wfd;
			std::wstring path;
			int imgIdx;
			StringUtils::EUtfBom bom;
		};

		struct ItemDataCache {
			std::wstring fileSize;
			std::wstring fileDate;
			std::wstring fileTime;
			std::wstring fileAttr;
		};

		using ItemDataPtr = std::shared_ptr<FindItemData>;

	public:
		CFileFinder()
			: _searchSubdirs( false )
			, _searchArchives( false )
			, _searchDocuments( false )
			, _matchCase( false )
			, _wholeWords( false )
			, _hexMode( false )
			, _regexMode( false )
			, _fireEvent( true )
			, _encoding( 0 )
			, _encodingTemp( 0 )
			, _hStatusBar( nullptr )
			, _cRef( 1 ) {}

		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

		// IUnknown interface methods - for drag&drop
		inline IFACEMETHODIMP QueryInterface( REFIID riid, void **ppv ) {
			static const QITAB qit[] = { QITABENT( CFileFinder, IDropTarget ), { 0 } };
			return QISearch( this, qit, riid, ppv );
		}

		inline IFACEMETHODIMP_(ULONG) AddRef() {
			return InterlockedIncrement( &_cRef );
		}

		inline IFACEMETHODIMP_(ULONG) Release() {
			long cRef = InterlockedDecrement( &_cRef );
			if( !cRef )
				delete this;
			return cRef;
		}

	private:
		void startFindFiles();
		void stopFindFiles();
		void findFilesDone();
		bool _findFilesCore();

	private:
		template<typename T>
		bool findTextUnicode( T lf, std::ifstream& fStream, const std::wstring& textToFind, bool words, StringUtils::EUtfBom& bom );
		bool findTextInUnicodeString( const std::wstring& textToFind, bool words );
		bool findTextAnsi( std::ifstream& fStream, const std::string& textToFind, bool words );
		bool findTextAnsiRegex( std::ifstream& fStream, const std::string& textToFind, bool words );
		bool findTextInFile( const std::wstring& fileName, StringUtils::EUtfBom& bom );
		bool findItems( const std::wstring& mask, const std::wstring& dirName );
		bool convertToAnsi( const std::wstring& text, std::string& outStr );
		void searchArchive( const std::wstring& archiveName, const std::wstring& mask );
		void storeFoundItem( const WIN32_FIND_DATA& wfd, const std::wstring& path, StringUtils::EUtfBom bom = StringUtils::NOBOM );
		bool initSearchContent( const TCHAR *text );
		void updateStatus();
		void updateLayout( int width, int height );
		void enableHexMode( bool enable );
		void showSearchGroup( bool show = true );
		void enableSearchGroup( bool enable = true );
		void sortItems( int code );
		int  findItemIndex( ItemDataPtr itemData );

		ItemDataPtr getItemData( int index );
		ItemDataCache& getCachedData( int index );
		std::vector<std::wstring> getSelectedItems( HWND hwListView );

		UINT onContextMenu( HWND hWnd, POINT& pt );
		bool onNotifyMessage( LPNMHDR notifyHeader );
		void onHandleVirtualKeys( LPNMLVKEYDOWN vKey );
		void onPrepareCache( LPNMLVCACHEHINT lvCache );
		bool onDrawDispItems( NMLVDISPINFO *lvDisp );
		int  onFindItemIndex( LPNMLVFINDITEM lvFind );
		void onBeginDrag( LPNMLISTVIEW lvData );
		void onExecItem( HWND hwListView );
		void onFocusItem( HWND hwListView );
		void onViewItem( HWND hwListView, bool viewHex = false );
		void onEditItem( HWND hwListView );
		void onDeleteItems( HWND hwListView );

		void handleKeyboardAction( LPNMLVKEYDOWN vKey );
		void handleKeyboardActionWithCtrl( LPNMLVKEYDOWN vKey );
		void handleKeyboardActionWithAlt( LPNMLVKEYDOWN vKey );
		void handleKeyboardActionWithShift( LPNMLVKEYDOWN vKey );

	private:
		CShellContextMenu _contextMenu;
		CBackgroundWorker _worker;

		std::vector<ItemDataPtr> _foundItems;
		std::map<int, ItemDataCache> _cachedData;
		std::recursive_mutex _mutex;

		std::unique_ptr<std::regex> _regexA;
		std::unique_ptr<std::wregex> _regexW;

		std::wstring _findMask;
		std::wstring _findInDir;
		std::string  _findTextA;
		std::wstring _findTextW;
		std::wstring _findText;

		std::wstring _currentPath;

		std::wstring _strOut;
		std::string  _line;

		char _buf[0x8000];

		bool _searchSubdirs;
		bool _searchArchives;
		bool _searchDocuments; // TODO: ebooks etc.
		bool _matchCase;
		bool _wholeWords;
		bool _hexMode;
		bool _fireEvent; // temp variable to avoid firing EN_CHANGE when programatically changing text in an edit control
		bool _regexMode;
		bool _srcBounds[2]; // source string left and/or right bounds are alphanumeric (used for whole-words matching)
		UINT _encoding;
		UINT _encodingTemp;

		HWND _hStatusBar;
		DWORD _timeSpent;

		long _cRef;
	};
}
