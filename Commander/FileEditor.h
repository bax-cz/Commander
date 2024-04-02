#pragma once

#include <regex>

#include "BaseViewer.h"
#include "FindText.h"

namespace Commander
{
	class CFileEditor : public CBaseViewer
	{
	public:
		static const UINT resouceIdTemplate = IDD_IMAGEVIEWER;

	public:
		virtual bool viewFile( const std::wstring& dirName, const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel = nullptr ) override;

		virtual void onInit() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;
		LRESULT CALLBACK wndProcEdit( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

	public:
		void setParams( const CFindText::CParams& params, const std::wstring& text );

	private:
		void viewFileCore( StringUtils::EUtfBom encoding );
		void hiliteText();

	private:
		bool readFromDisk( const std::wstring& fileName );
		bool readFileText( std::istream& inStream );

	private:
		void openFileDialog();
		bool saveFileDialog( std::wstring& fileName, DWORD& encodingId );
		void findTextDialog( bool replace = false );
		void gotoLineDialog();
		bool askSaveChanges( bool modified );

		HWND createEditControl();
		HWND createStatusBar();

		void showStatusBar();
		void swapActiveEdit();
		void updateMenuEdit();
		void updateMenuView();
		void updateMenuEncoding();
		void updateWindowTitle();
		void updateStatusBarCaretPos( bool force = false );
		void updateLayout( int width, int height );
		bool selectCurrentWord( LONG x, LONG y );
		bool selectCurrentLine( LONG x, LONG y );
		void selectAll();
		void setZoom( int value );
		void viewFileAnsi( WORD menuId );
		void handleMenuCommands( WORD menuId );
		void handleDropFiles( HDROP hDrop );
		bool handleKeyboardShortcuts( WPARAM wParam );
		bool getEditCaretPosStr( HWND hEdit, std::wstring& pos );
		bool findText( wchar_t *buf, size_t buf_len, const std::wstring& text, int offset, bool reverse, int& idxBeg, int& idxEnd );

		bool preSearchText();
		bool searchText( bool reverse = false );
		void replaceText( bool replaceAll = false );

		bool preSaveFile();
		bool saveFile( const std::wstring& fileName, DWORD encodingId = IDM_VIEWER_ENCODING_AUTO , bool selectionOnly = false );
		bool saveFileAs( bool selectionOnly = false );

	private:
		void onFindDialogNotify( int cmd );
		void onWorkerFinished( bool retVal );
		void onReaderFinished( bool retVal );
		bool _workerProc();

	private:
		std::unique_ptr<std::wregex> _regex;

		CFindText *_pFindDlg;
		CFindText::CParams _findParams;

		int _findFromIdxBeg;
		int _findFromIdxEnd;

		bool _wordWrap;
		bool _sortLines;
		bool _hiliteText;

		StringUtils::EUtfBom _fileBom; // detected BOM
		StringUtils::EUtfBom _useEncoding;
		StringUtils::EEol _fileEol; // end of line char(s)

		UINT _useCodePage;
		int _currentZoom;

		POINT _tripleClickPos;
		UINT _tripleClickTime;

		std::wstring _textToFind;
		std::wstring _textHexToFind;
		std::wstring _textToReplace;
		std::wstring _tempFileName;

		HWINEVENTHOOK _hEventHook; // to monitor caret location change event
		LONGLONG _fileSize;
		HLOCAL _hEditData;
		HWND _hStatusBar;
		HWND _hActiveEdit;
		WNDPROC _defaultEditProc;
		HMENU _hMenu;

	private:
		static LRESULT CALLBACK wndProcEditSubclass( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR data )
		{
			return reinterpret_cast<CFileEditor*>( data )->wndProcEdit( hWnd, message, wParam, lParam );
		}

		static void CALLBACK eventProcEditControl( HWINEVENTHOOK hEventHook, DWORD event, HWND hWnd, LONG objectId, LONG childId, DWORD eventThread, DWORD eventTime )
		{
			SendNotifyMessage( FCS::inst().getActiveDialog(), UM_CARETPOSCHNG, 0, 0 );
		}
	};
}
