#pragma once

#include "BaseDialog.h"

/*
	Compare directories dialog
*/

namespace Commander
{
	class CCompareDirectories : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_COMPAREDIRS;

	public:
		CCompareDirectories();
		~CCompareDirectories();

		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		void updateGroupStatus( BOOL enable = TRUE );
		void updateOkButtonStatus();
		bool compareDirs();
		bool compareFiles();
		bool _workerProc();

	private:
		bool compareDirectoriesSize( const std::wstring& dirName1, const std::wstring& dirName2 );
		bool calculateDirectorySize( const std::wstring& dirName, ULONGLONG *dirSize );
		bool traverseSubDirectories( const std::wstring& dirName1, const std::wstring& dirName2 );
		bool compareFilesContents( const std::wstring& fileName1, const std::wstring& fileName2 );

	private:
		CBackgroundWorker _worker;

		std::vector<std::shared_ptr<EntryData>>& _dirsLeft;
		std::vector<std::shared_ptr<EntryData>>& _filesLeft;
		std::vector<std::shared_ptr<EntryData>>& _dirsRight;
		std::vector<std::shared_ptr<EntryData>>& _filesRight;

		std::set<int> _entriesLeft;
		std::set<int> _entriesRight;

		std::wstring _path1;
		std::wstring _path2;

		BYTE _flags;
	};
}
