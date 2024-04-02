#pragma once

#include "BaseDialog.h"
#include "ChecksumTypes.h"
#include "BackgroundWorker.h"

namespace Commander
{
	class CCalcChecksums : public CBaseDialog
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
		bool processItem( const std::wstring& itemName );
		bool processFileName( const std::wstring& fileName );
		bool calcChecksums( ChecksumType::ChecksumData& checksumData );
		void saveChecksumsFile();
		bool writeFile( const std::wstring& fileName, DWORD type );
		void focusSelectedFile();
		void updateProgressStatus();
		bool handleContextMenu( HWND hWnd, POINT pt );
		void selectionChanged( LPNMLISTVIEW lvData );
		bool onDrawDispItems( NMLVDISPINFO *lvDisp );
		bool onNotifyMessage( LPNMHDR notifyHeader );
		void updateLayout( int width, int height );
		void updateDialogTitle( const std::wstring& status );

	private:
		bool _workerProc();

	private:
		CBackgroundWorker _worker;
		std::vector<std::wstring> _items;
		std::wstring _dialogTitle;
		std::wstring _currentDirectory;
		std::vector<ChecksumType::ChecksumData> _checksums;
		LONG_PTR _progressStyle;
		UINT _itemsProcessed;
		ULONGLONG _bytesTotal;
		std::atomic<ULONGLONG> _bytesProcessed;
		unsigned char _dataBuf[0x00008000];
	};
}
