#pragma once

#include "BaseDialog.h"

namespace Commander
{
	class CSplitFile : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_SPLITFILE;

	public:
		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		bool writeFilePart( std::ifstream& fStrIn, ULONG partNum, uLong& crc );
		bool generateBatchFile( uLong& crc );
		void recalcPartSize( bool selectionChanged );
		void updateWindowTitle( const std::wstring& title = L"" );
		void updateGuiStatus( bool enable );
		void updateProgressStatus();
		bool _workerProc();

	private:
		CBackgroundWorker _worker;

		std::wstring _windowTitle;
		std::wstring _fileName;
		std::wstring _outDir;

		std::wstring _errorMessage;

		WIN32_FIND_DATA _wfd;

		ULONGLONG _fileSize;
		ULONGLONG _partSize;
		ULONGLONG _lastSize;
		ULONG _numParts;

		LONG_PTR _progressStyle;
		ULONGLONG _bytesProcessed;
		ULONGLONG _freeSpace;

		bool _isInitialized;
		bool _fireEvent;
		bool _autoDetect;

		char _dataBuf[0xFFFF];
	};
}
