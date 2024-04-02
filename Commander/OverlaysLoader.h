#pragma once

namespace Commander
{
	class COverlaysLoader
	{
		struct OverlayData
		{
			std::wstring path;
			std::vector<std::shared_ptr<EntryData>> dirs;
			std::vector<std::shared_ptr<EntryData>> files;
			std::shared_ptr<CBackgroundWorker> spWorker;
			HIMAGELIST imgList;
		};

	public:
		COverlaysLoader();
		~COverlaysLoader();

	public:
		void start( HWND hWnd, const std::wstring& path, std::vector<std::shared_ptr<EntryData>>& dirs, std::vector<std::shared_ptr<EntryData>>& files ); // load imagelist
		void stop( HWND hWnd ); // destroy imagelist

	private:
		void purgeInactive( bool force = false );
		void loadOverlaysFromDirectories( OverlayData& overlayData, UINT flags );
		void loadOverlaysFromFiles( OverlayData& overlayData, UINT flags );
		bool _loadOverlaysCore( HWND hWndNotify );

	private:
		std::mutex _mutex;
		std::map<HWND, OverlayData> _overlays;
		std::vector<OverlayData> _graveyard;

		const int cx; // small icon dimensions - width
		const int cy; // height
	};
}
