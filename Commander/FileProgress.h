#pragma once

#include "BaseDialog.h"

namespace Commander
{
	class CFileProgress : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_PROGRESS;

	public:
		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	public:
		int getCurrentProgressValue() { return _doneRatePercent; }
		void runJob( EFcCommand cmd, std::vector<std::wstring> items, std::wstring dirTo = L"" );

	private:
		INT_PTR onFastCopyInfo( WPARAM message, FastCopy::Confirm *confirm );

	private:
		void initFastCopy();
		void stopFastCopy();
		void pauseFastCopy();
		bool isListingOnly();
		void updateProgressStatus();
		void drawBackground( HDC hdc );
		void calcInfoTotal( double *doneRate, int *remain_sec, int *total_sec );
		int calcInfoPerFile(); // TODO

		double sizeToCoef( double ave_size, int64 files );

	private:
		FastCopy _fastCopy;
		FastCopy::Info _fastCopyInfo;
		FastCopy::Mode _fastCopyMode;
		TransInfo _ti;

		//CBackgroundWorker _worker;

		EFcCommand _command;
		std::vector<std::wstring> _entries;
		std::wstring _targetDir;

		ULONGLONG _bytesProcessed;

		bool _bSuspended;
		int _calcTimes;
		int _lastTotalSec;
		int _doneRatePercent;
	};
}
