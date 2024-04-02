#pragma once

#include "BaseDialog.h"

namespace Commander
{
	class CCombineFiles : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_COMBINEFILES;

	public:
		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		void initItems( HWND hWndListbox );
		void updateGuiStatus( bool enable );
		void updateProgressStatus();
		void updateWindowTitle( const std::wstring& title = L"" );
		bool parseBatchFile( const std::wstring& fileName );
		bool writeFilePart( std::ofstream& fStrIn, const std::wstring& fileName, uLong& crc );
		void handleDropFiles( HDROP hDrop );
		void onDateTimeChange( LPNMDATETIMECHANGE pDtChange );
		void onChooseFileName();
		void onMoveItemUp();
		void onMoveItemDown();
		void onAddItem();
		void onRemoveItem();
		void focusSelectedItem();
		bool _workerProc();

	private:
		CBackgroundWorker _worker;

		std::vector<std::wstring> _items;

		std::wstring _windowTitle;
		std::wstring _errorMessage;
		std::wstring _fileName;
		std::wstring _fileNameOverwrite; // a filename confirmed by the user that it can be overwritten
		std::wstring _outDir;
		std::wstring _inDir;

		ULONGLONG _fileSize;
		SYSTEMTIME _stLocal;
		uLong _crcOrig;
		uLong _crcCalc;

		LONG_PTR _progressStyle;
		ULONGLONG _bytesProcessed;

		char _dataBuf[0xFFFF];
		bool _isInitialized;
		bool _calcCrcOnly;
		bool _batchFileParsed;
		bool _operationCanceled;
	};
}
