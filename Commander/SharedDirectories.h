#pragma once

#include "BaseDialog.h"
#include "BackgroundWorker.h"

namespace Commander
{
	class CSharedDirectories : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_SHAREDDIRS;

	public:
		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		void addItems();
		void focusSelectedDirectory();
		void selectionChanged( LPNMLISTVIEW lvData );
		bool onNotifyMessage( LPNMHDR notifyHeader );

	private:
		bool _getSharedDirsCore();

	private:
		CBackgroundWorker _worker;
	};
}
