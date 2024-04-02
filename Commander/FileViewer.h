#pragma once

#include <regex>

#include "BaseViewer.h"
#include "FindText.h"

namespace Commander
{
	class CFileViewer : public CBaseViewer
	{
	public:
		static const UINT resouceIdTemplate = IDD_FILEVIEWER;

	private:
		static const int BYTES_ROW = 16; // bytes per row in hex-view

	public:
		virtual bool viewFile( const std::wstring& dirName, const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel = nullptr ) override;

		virtual void onInit() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;
		LRESULT CALLBACK wndProcEdit( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

	public:
		void setParams( const CFindText::CParams& params, const std::wstring& hiliteText );

	public:
		static char getPrintableChar( char ch );
		static UINT getEncodingId( StringUtils::EUtfBom encoding, StringUtils::EUtfBom bom = StringUtils::NOBOM );
		static StringUtils::EUtfBom getEncodingFromId( DWORD encodingId );
		static void reportRegexError( HWND hWnd, const std::regex_error& e, const std::wstring& expression );

	private:
		bool viewFileCore( bool isHexMode, StringUtils::EUtfBom bom );
		void hiliteText( bool reverse = false );

	private:
		bool readFromDisk( const std::wstring& fileName );
		bool readFromRegistry( const std::wstring& fileName );

	private:
		void openFileDialog();
		void saveFileDialog( const std::wstring& text );
		void findTextDialog();
		void gotoLineDialog();

		void swapActiveEdit();
		void createStatusBar();
		void showStatusBar();
		void updateMenuEncoding();
		void updateWindowTitle();
		void updateStatusBar();
		bool selectCurrentWord();
		void selectCurrentLine();
		void copyTextToFile();
		void setZoom( int value );
		void updateScrollBar( UINT fMask );
		void updateLayout( int width, int height );
		void scrollToPos( std::streamoff pos, bool refresh = false );
		void viewFileAnsi( WORD menuId );
		void handleMenuCommands( WORD menuId );
		void handleDropFiles( HDROP hDrop );
		void handleContextMenu( POINT pt );
		void handleScroll( WORD pos, WORD request );
		bool handleKeyboardShortcuts( WPARAM wParam );
		bool parseOffsetnumber( const std::wstring& text, std::streamoff& pos );
		bool findTextInEditBox( const std::wstring& text, int fromIdx, bool reverse, int& idxBeg, int& idxEnd );
		void searchText();
		int findOffsetInEditBox( std::streamoff offset );
		std::streamoff getCurrentOffsetInEditBox();
		int calcScrollRange();
		int calcScrollPos();

		bool readFileMediaInfo( const std::wstring& fileName );
		void readFile( const std::wstring& fileName );
		bool readFileText( std::istream& iStream );
		bool readFileHex( std::istream& iStream );

	private:
		bool _findTextInFile();

	private:
		std::unique_ptr<CBaseDataProvider> _upDataProvider;

		std::unique_ptr<std::regex> _regexA;
		std::unique_ptr<std::wregex> _regexW;

		CFindText *_pFindDlg;
		CFindText::CParams _findParams;

		char _buf[0x10000];
		int _findFromIdxBeg;
		int _findFromIdxEnd;

		bool _wordWrap;
		bool _isHex;

		StringUtils::EUtfBom _fileBom; // detected BOM
		StringUtils::EUtfBom _useEncoding;
		StringUtils::EEol _fileEol; // end of line char(s)

		UINT _useCodePage;
		int _currentZoom;

		POINT _tripleClickPos;
		UINT _tripleClickTime;

		std::wstring _textToFind;
		std::string _dataToFind;
		std::wstring _tempFileName;
		std::wstring _outText;
		std::ifstream _fileStream;
		std::streamoff _fileStreamPos;
		std::streamoff _findFromOffset;

		int _linesCountPerPage;
		int _lineLength;

		LARGE_INTEGER _fileSize;
		HWND _hStatusBar;
		HWND _hActiveEdit;
		WNDPROC _defaultEditProc;
		HMENU _hMenu;

	private:
		static LRESULT CALLBACK wndProcEditSubclass( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR data )
		{
			return reinterpret_cast<CFileViewer*>( data )->wndProcEdit( hWnd, message, wParam, lParam );
		}
	};
}
