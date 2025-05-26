#pragma once

#include "BaseDialog.h"
#include "FindText.h"

#include "../WinMerge/DiffWrapper.h"

/*
	Compare two files' content dialog
*/

namespace Commander
{
	class CCompareFiles : public CBaseDialog
	{
		HMODULE _hRichEditLib; // needed for Rich Edit 2.0 control

		enum MERGE_LINEFLAGS : DWORD
		{
			LF_DIFF = 0x00200000UL,
			LF_GHOST = 0x00400000UL,
			LF_TRIVIAL = 0x00800000UL,
			LF_MOVED = 0x01000000UL,
			LF_SNP = 0x02000000UL,
			LF_INVISIBLE = 0x80000000UL
		};

		static const int BYTES_ROW = 16; // bytes per row in hex-view

	public:
		static const UINT resouceIdTemplate = IDD_COMPAREFILES;

	public:
		CCompareFiles();
		~CCompareFiles();

		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;
		LRESULT CALLBACK wndProcRichEdit( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId );

	private:
		void primeTextBuffers();
		void moveLine( std::vector<std::pair<DWORD, std::string>>& buff, int line1, int line2, int newline1 );
		void setLineFlag( std::vector<std::pair<DWORD, std::string>>& buff, int line, DWORD flag, bool set );
		void writeRtfDocument( const std::vector<std::pair<DWORD, std::string>>& buff1, const std::vector<std::pair<DWORD, std::string>>& buff2, std::string& outBuff );
		void writeRtfDocumentHex( const std::string& buf1, const std::string& buf2, std::streamsize offset, std::string& outBuff );

	private:
		void initFilePaths();
		void initImageLists();
		void createToolbar();
		void addToolbarButtons();
		void handleDropFiles( HDROP hDrop, bool leftRichEdit );
		void focusDiffPrev();
		void focusDiffNext();
		void focusDiffCurrent();
		void focusDiffFirst();
		void focusDiffLast();
		void focusLine( int lineIdx );
		void updateOptions();
		void updateGuiStatus();
		void updateWindowTitle();
		void setZoom( int value );
		void refresh();
		void findTextDialog();
		void searchText( bool reverse );
		bool loadFilesAsText();
		bool compareFilesText();
		bool compareFilesBinary();

	private:
		bool findDiffBinary( std::ifstream& fs1, std::ifstream& fs2, std::streamoff& offset );

	private:
		void onFindDialogNotify( int cmd );
		bool _workerProc();

	private:
		CBackgroundWorker _worker;

		CFindText *_pFindDlg;
		CFindText::CParams _findParams;

		int _findFromIdxBeg;
		int _findFromIdxEnd;

		int _linesCountPerPage;
		bool _binaryMode;

		CDiffWrapper _diffWrapper;
		
		DiffList _diffs;
		DIFFOPTIONS _options;
		DIFFSTATUS _status;

		int _curDiff;
		int _curZoom;

		std::vector<std::pair<DWORD, std::string>> _buff[2];
		UINT _trivialDiffs;

		std::wstring _dialogTitle;
		std::wstring _textToFind;
		std::wstring _textHexToFind;

		std::wstring _path1;
		std::wstring _path2;

		std::string  _diffLeft;
		std::string  _diffRight;

		HWND _hActiveEdit;
		HWND _hToolBar;

		HWINEVENTHOOK _hEventHook; // to monitor caret location change event

		HIMAGELIST _imgListToolbar;
		HIMAGELIST _imgListToolbarHot;
		HIMAGELIST _imgListToolbarDisabled;

	private:
		static LRESULT CALLBACK wndProcRichEditSubclass( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR data )
		{
			return reinterpret_cast<CCompareFiles*>( data )->wndProcRichEdit( hWnd, message, wParam, lParam, subclassId );
		}

		static void CALLBACK eventProcEditControl( HWINEVENTHOOK hEventHook, DWORD event, HWND hWnd, LONG objectId, LONG childId, DWORD eventThread, DWORD eventTime )
		{
			SendNotifyMessage( FCS::inst().getActiveDialog(), UM_CARETPOSCHNG, 0, 0 );
		}
	};
}
