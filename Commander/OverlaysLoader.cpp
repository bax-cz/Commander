#include "stdafx.h"

#include "Commander.h"
#include "OverlaysLoader.h"
#include "IconUtils.h"
#include "ImageTypes.h"
#include "NetworkUtils.h"

namespace Commander
{
	COverlaysLoader::COverlaysLoader()
		: cx( GetSystemMetrics( SM_CXSMICON ) )
		, cy( GetSystemMetrics( SM_CYSMICON ) )
	{
	}

	COverlaysLoader::~COverlaysLoader()
	{
		std::lock_guard<std::mutex> lock( _mutex );

		for( auto it = _overlays.begin(); it != _overlays.end(); )
		{
			if( it->second.spWorker->isRunning() )
				it->second.spWorker->stop();

			if( it->second.imgList )
				ImageList_Destroy( it->second.imgList );

			it = _overlays.erase( it );
		}

		purgeInactive( true );
	}

	void COverlaysLoader::stop( HWND hWnd )
	{
		std::lock_guard<std::mutex> lock( _mutex );

		auto it = _overlays.find( hWnd );
		if( it != _overlays.end() )
		{
			if( it->second.spWorker->isRunning() )
				it->second.spWorker->stop_async();

			// move to graveyard
			_graveyard.push_back( std::move( it->second ) );

			// remove from the list
			_overlays.erase( it );
		}

		purgeInactive(); // purge non-running workers and clean-up
	}

	void COverlaysLoader::start( HWND hWnd, const std::wstring& path, std::vector<std::shared_ptr<EntryData>>& dirs, std::vector<std::shared_ptr<EntryData>>& files )
	{
		// stop the possible previously started worker
		stop( hWnd );

		std::lock_guard<std::mutex> lock( _mutex );

		auto spWorker = std::make_shared<CBackgroundWorker>();
		spWorker->init( [this, hWnd] { return _loadOverlaysCore( hWnd ); }, hWnd, UM_OVERLAYSDONE );

		_overlays[hWnd] = { path, dirs, files, spWorker, nullptr };
		spWorker->start();
	}

	//
	// Purge non-running overlay workers from the list
	//
	void COverlaysLoader::purgeInactive( bool force )
	{
		// locked already, it's called from the stop (and dtor) function(s)
		for( auto it = _graveyard.begin(); it != _graveyard.end(); )
		{
			if( it->spWorker->isFinished() || force )
			{
				if( force )
					it->spWorker->stop();

				if( it->imgList )
					ImageList_Destroy( it->imgList );

				it = _graveyard.erase( it );
			}
			else
				++it;
		}
	}

	void COverlaysLoader::loadOverlaysFromDirectories( COverlaysLoader::OverlayData& overlayData, UINT flags )
	{
		SHFILEINFO shfi = { 0 };

		auto sharedItems = NetUtils::getSharedFolders();
		bool doubleDottedAdded = false;

		for( auto& dirEntry : overlayData.dirs )
		{
			if( !overlayData.spWorker->isRunning() )
				return;

			WCHAR *dirName = dirEntry->wfd.cFileName;
			std::wstring path = overlayData.path + dirName;

			if( !doubleDottedAdded && PathUtils::isDirectoryDoubleDotted( dirName ) )
			{
				doubleDottedAdded = true;

				HICON hIcon = (HICON)LoadImage( FCS::inst().getFcInstance(), MAKEINTRESOURCE( IDI_PARENTDIR ), IMAGE_ICON, cx, cy, LR_SHARED );
				ImageList_AddIcon( overlayData.imgList, hIcon );
				DestroyIcon( hIcon );
			}
			else
			{
				SHGetFileInfo( path.c_str(), 0, &shfi, sizeof( SHFILEINFO ), flags ); // MAX_PATH limited

				if( std::find( sharedItems.begin(), sharedItems.end(), path ) != sharedItems.end() )
				{
					// add shared-directory overlay icon
					HICON hOverlay = (HICON)LoadImage( FCS::inst().getFcInstance(), MAKEINTRESOURCE( IDI_SHAREDIR ), IMAGE_ICON, cx, cy, LR_SHARED );
					HICON hIcon = IconUtils::makeOverlayIcon( FCS::inst().getFcWindow(), shfi.hIcon, hOverlay );
					ImageList_AddIcon( overlayData.imgList, hIcon );
					DestroyIcon( shfi.hIcon );
					DestroyIcon( hOverlay );
					DestroyIcon( hIcon );
				}
				else
				{
					if( !shfi.hIcon ) // SHGFI_USEFILEATTRIBUTES means do not access disk, just pretend the file exists
						SHGetFileInfo( path.c_str(), dirEntry->wfd.dwFileAttributes, &shfi, sizeof( SHFILEINFO ), flags | SHGFI_USEFILEATTRIBUTES );

					ImageList_AddIcon( overlayData.imgList, shfi.hIcon );
				}

				DestroyIcon( shfi.hIcon );
			}
		}
	}

	void COverlaysLoader::loadOverlaysFromFiles( COverlaysLoader::OverlayData& overlayData, UINT flags )
	{
		SHFILEINFO shfi = { 0 };

		for( auto& fileEntry : overlayData.files )
		{
			if( !overlayData.spWorker->isRunning() )
				return;

			WCHAR *fileName = fileEntry->wfd.cFileName;
			std::wstring path = overlayData.path + fileName;

			if( ArchiveType::isKnownType( path ) ) // TODO: add overlays
				shfi.hIcon = IconUtils::getStock( SIID_ZIPFILE );
			else if( ImageType::isKnownType( path ) )
				shfi.hIcon = IconUtils::getStock( SIID_IMAGEFILES );
			else
			{
				SHGetFileInfo( path.c_str(), 0, &shfi, sizeof( SHFILEINFO ), flags );
			}

			if( !shfi.hIcon ) // SHGFI_USEFILEATTRIBUTES means do not access disk, just pretend the file exists
				SHGetFileInfo( path.c_str(), fileEntry->wfd.dwFileAttributes, &shfi, sizeof( SHFILEINFO ), flags | SHGFI_USEFILEATTRIBUTES );

			ImageList_AddIcon( overlayData.imgList, shfi.hIcon );

			DestroyIcon( shfi.hIcon );
		}
	}

	bool COverlaysLoader::_loadOverlaysCore( HWND hWndNotify )
	{
		OverlayData overlayData;

		{
			std::lock_guard<std::mutex> lock( _mutex );

			auto it = _overlays.find( hWndNotify );
			overlayData = std::move( it->second );
			it->second.spWorker = overlayData.spWorker;
			it->second.imgList = nullptr;
		}

		if( PathUtils::isExtendedPath( overlayData.path ) )
			return false; // TODO: overlay icons on longpaths ??

		ShellUtils::CComInitializer _com( COINIT_APARTMENTTHREADED );

		// create new imagelist
		overlayData.imgList = ImageList_Create( cx, cy, ILC_COLOR32, static_cast<int>( overlayData.dirs.size() + overlayData.files.size() ), 0 );

		// process directories first, followed by files
		UINT flags = SHGFI_ICON | SHGFI_SMALLICON | SHGFI_ADDOVERLAYS;
		loadOverlaysFromDirectories( overlayData, flags );
		loadOverlaysFromFiles( overlayData, flags );

		{
			std::lock_guard<std::mutex> lock( _mutex );

			if( overlayData.spWorker->isRunning() )
			{
				// notify successful imagelist load
				auto it = _overlays.find( hWndNotify );
				it->second = std::move( overlayData );
				it->second.spWorker->sendNotify( 2, reinterpret_cast<LPARAM>( it->second.imgList ) );
			}
			else
				ImageList_Destroy( overlayData.imgList );
		}

		return false; // sending notification is not needed
	}
}
