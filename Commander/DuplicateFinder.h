#pragma once

#include <vector>
#include <mutex>

#include "BackgroundWorker.h"

namespace Commander
{
	class CDuplicateFinder : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_FINDITEMS;

	public:
		struct Entry {
			Entry( WIN32_FIND_DATA& wfd, std::wstring& path ) { _wfd = wfd; _path = path; _crc = 0; }
			Entry() { _crc = 0; }
			WIN32_FIND_DATA _wfd;
			ULONGLONG _crc;
			std::wstring _path;
		};

		struct EntryData : Entry {
			EntryData( const Entry& entry, const std::wstring& path, int idx, std::wstring& size, std::wstring& date, std::wstring& time, std::wstring& attr ) {
				_wfd = entry._wfd; _path = entry._path; _crc = entry._crc; _dupls.push_back( path );_imgIndex = idx;
				_count = L"2"; _fileSize = size; _fileDate = date; _fileTime = time; _fileAttr = attr;
			}
			std::vector<std::wstring> _dupls; // file full path list
			int _imgIndex;
			std::wstring _count;
			std::wstring _fileSize;
			std::wstring _fileDate;
			std::wstring _fileTime;
			std::wstring _fileAttr;
		};

	public:
		CDuplicateFinder();
		~CDuplicateFinder();

		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		void findDuplicates();
		bool findEntries( const std::wstring& mask, const std::wstring& dirName );
		bool compareFiles( Entry& entry1, Entry& entry2 );
		void checkEntry( Entry& entry );
		void duplicateFound( Entry& entry, const Entry& entryPrev );
		void sortItems( int code );
		void findFilesDone();
		void updateLayout( int width, int height );

		ULONGLONG calcFileChecksum( const std::wstring& fileName );

		void onExecItem( HWND hwListView );
		bool onContextMenu( HWND hWnd, POINT pt );
		bool onNotifyMessage( LPNMHDR notifyHeader );
		bool onDrawDispItems( NMLVDISPINFO *lvDisp );
		int  onFindItemIndex( LPNMLVFINDITEM lvFind );

	private:
		bool _findFilesCore();

	private:
		CBackgroundWorker _worker;

		std::recursive_mutex _mutex;

		std::vector<Entry> _entries;
		std::vector<EntryData> _duplicateEntries;

		std::wstring _findMask;
		std::wstring _findInDir;
		bool _searchSubdirs;
		bool _findByContent;
		ULONGLONG _findSize;

		HWND _hStatusBar;
	};
}
