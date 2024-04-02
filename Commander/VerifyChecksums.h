#pragma once

#include "BaseDialog.h"
#include "ChecksumTypes.h"
#include "BackgroundWorker.h"

namespace Commander
{
	class CVerifyChecksums : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_CALCCHECKSUM;

	public:
		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		bool loadChecksums();
		bool verifyChecksums();
		bool calcChecksums( const std::wstring& fileName, ChecksumType::ChecksumData& checksumData );
		bool getChecksumData( const std::wstring& fileName, const std::wstring& sum, ChecksumType::ChecksumData& data );
		bool openChecksumsFile( ChecksumType::EChecksumType type, const std::wstring& inFileName );
		bool parseLine( ChecksumType::EChecksumType type, const std::wstring& line, std::wstring& fileName, std::wstring& sum );
		bool parseClipboardData( std::wstring& inFileName );
		bool checkResult( size_t idx, bool& result );
		void reportResult();
		void focusSelectedFile();
		void updateProgressStatus();
		bool handleContextMenu( HWND hWnd, POINT pt );
		bool onCustomDraw( LPNMLVCUSTOMDRAW lvcd );
		bool onDrawDispItems( NMLVDISPINFO *lvDisp );
		void selectionChanged( LPNMLISTVIEW lvData );
		bool onNotifyMessage( LPNMHDR notifyHeader );
		void updateLayout( int width, int height );
		void updateDialogTitle( const std::wstring& status );

	private:
		ChecksumType::EChecksumType detectChecksumType( const std::wstring& fileName );
		ChecksumType::EChecksumType tryParseLine( const std::wstring& line );

	private:
		bool _workerProc();

	private:
		CBackgroundWorker _worker;
		std::wstring _dialogTitle;
		std::wstring _currentDirectory;
		std::wstring _clipboardText;
		std::vector<std::wstring> _items;
		std::vector<ChecksumType::ChecksumData> _checksums;
		LONG_PTR _progressStyle;

		UINT _itemsOk;
		UINT _itemsFail;
		UINT _itemsMissing;

		ULONGLONG _bytesTotal;
		std::atomic<ULONGLONG> _bytesProcessed;
		std::atomic<int> _processingItem;
		unsigned char _dataBuf[0x8000];
	};
}
