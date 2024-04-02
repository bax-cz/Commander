#pragma once

/*
	Shell utils - helper functions for windows system shell
*/

#include <vector>
#include <comdef.h>
#include <propkey.h>
#include <Propvarutil.h>
#include <strsafe.h>

#include "StringUtils.h"

// Workaround for VS2015 - QITABENT() throws warning C4838: conversion from 'DWORD' to 'int'
// https://connect.microsoft.com/VisualStudio/feedback/details/1587319/shlwapi-h-qitabent-throws-warning-c4838-conversion-from-dword-to-int
#if defined(OFFSETOFCLASS) && _MSC_VER <= 1900
  #undef OFFSETOFCLASS
  #define OFFSETOFCLASS(base, derived) ((int)((unsigned __int64)((base*)((derived*)8))-8ULL))
#endif

namespace Commander
{
	// {fc0a77e6-9d70-4258-9783-6dab1d0fe31e}
	static const CLSID CLSID_UnknownJunction = { 0xfc0a77e6, 0x9d70, 0x4258,{ 0x97, 0x83, 0x6d, 0xab, 0x1d, 0x0f, 0xe3, 0x1e } };

	struct ShellUtils
	{
		#define PidlNext(pidl) ((LPITEMIDLIST)((((BYTE*)(pidl))+(pidl->mkid.cb))))

		// Vector to pidl items type
		typedef std::vector<LPITEMIDLIST> TPidl;

		//
		// COM initializer helper class
		//
		class CComInitializer
		{
		public:
			CComInitializer() { CoInitialize( NULL ); }
			CComInitializer( DWORD dwCoInit ) { CoInitializeEx( NULL, dwCoInit ); }
			~CComInitializer() { CoUninitialize(); }
		private:
			CComInitializer( const CComInitializer& ) = delete;
			CComInitializer& operator=( const CComInitializer& ) = delete;
		};

		//
		// Safely release shell interface template
		//
		template<class T>
		static void safeRelease( T **ppT )
		{
			if( *ppT )
			{
				(*ppT)->Release();
				*ppT = nullptr;
			}
		}

		//
		// Format HRESULT error message as text
		//
		static std::wstring getErrorMessage( HRESULT hr )
		{
			std::wstring errorMessage;

			if( FAILED( hr ) )
			{
				_com_error error( hr );
				errorMessage = error.ErrorMessage();
			}

			return errorMessage;
		}

		// Helper class to bind data context
		class CFileSysBindData : public IFileSystemBindData2
		{
		public:
			CFileSysBindData() : _cRef( 1 ), _clsidJunction( CLSID_UnknownJunction )
			{
				ZeroMemory( &_fd, sizeof( _fd ) );
				ZeroMemory( &_liFileID, sizeof( _liFileID ) );
			}

			// IUnknown
			IFACEMETHODIMP QueryInterface( REFIID riid, void **ppv )
			{
				static const QITAB qit[] = {
					QITABENT( CFileSysBindData, IFileSystemBindData ),
					QITABENT( CFileSysBindData, IFileSystemBindData2 ),
					{ 0 },
				};
				return QISearch( this, qit, riid, ppv );
			}

			IFACEMETHODIMP_(ULONG) AddRef()
			{
				return InterlockedIncrement( &_cRef );
			}

			IFACEMETHODIMP_(ULONG) Release()
			{
				long cRef = InterlockedDecrement( &_cRef );
				if( !cRef )
					delete this;
				return cRef;
			}

			// IFileSystemBindData
			IFACEMETHODIMP SetFindData( const WIN32_FIND_DATAW *pfd )
			{
				_fd = *pfd;
				return S_OK;
			}

			IFACEMETHODIMP GetFindData( WIN32_FIND_DATAW *pfd )
			{
				*pfd = _fd;
				return S_OK;
			}

			// IFileSystemBindData2
			IFACEMETHODIMP SetFileID( LARGE_INTEGER liFileID )
			{
				_liFileID = liFileID;
				return S_OK;
			}

			IFACEMETHODIMP GetFileID( LARGE_INTEGER *pliFileID )
			{
				*pliFileID = _liFileID;
				return S_OK;
			}

			IFACEMETHODIMP SetJunctionCLSID( REFCLSID clsid )
			{
				_clsidJunction = clsid;
				return S_OK;
			}

			IFACEMETHODIMP GetJunctionCLSID( CLSID *pclsid )
			{
				HRESULT hr;
				if( CLSID_UnknownJunction == _clsidJunction )
				{
					*pclsid = CLSID_NULL;
					hr = E_FAIL;
				}
				else
				{
					*pclsid = _clsidJunction; // may be CLSID_NULL (no junction handler case)
					hr = S_OK;
				}
				return hr;
			}

		private:
			long _cRef;
			WIN32_FIND_DATAW _fd;
			LARGE_INTEGER _liFileID;
			CLSID _clsidJunction;
		};

		//
		// Perform system file execution
		//
		static bool executeCommand( const std::wstring& command, const std::wstring& dirName, const std::wstring& params = L"" )
		{
			SHELLEXECUTEINFO sei = { 0 };
			sei.cbSize = sizeof( sei );
			sei.lpVerb = L"open";
			sei.lpFile = command.c_str();
			sei.lpDirectory = dirName.c_str();
			sei.lpParameters = params.c_str();
			sei.nShow = SW_SHOWNORMAL;

			return !!ShellExecuteEx( &sei );
		}

		//
		// Perform system file execution through pidl
		//
		static bool executeFile( const std::wstring& fileName, const std::wstring& dirName, const std::wstring& params = L"" )
		{
			PIDLIST_ABSOLUTE pidl = ILCreateFromPath( ( dirName + fileName ).c_str() );

			if( pidl )
			{
				SHELLEXECUTEINFO sei = { 0 };
				sei.cbSize = sizeof( sei );
				sei.fMask = SEE_MASK_IDLIST;
				sei.lpVerb = L"open";
				sei.lpIDList = pidl;
				sei.lpDirectory = dirName.c_str();
				sei.lpParameters = params.c_str();
				sei.nShow = SW_SHOWNORMAL;

				bool ret = !!ShellExecuteEx( &sei );

				ILFree( pidl );

				return ret;
			}

			return false;
		}

		//
		// Invoke command using context menu interface
		//
		static HRESULT invokeCommand( const LPCONTEXTMENU pCm, UINT cmdId, const POINT& pt )
		{
			if( pCm )
			{
				CMINVOKECOMMANDINFOEX ici = { 0 };
				ici.cbSize = sizeof( ici );
				ici.fMask = CMIC_MASK_UNICODE | CMIC_MASK_PTINVOKE;
				ici.lpVerb = MAKEINTRESOURCEA( cmdId );
				ici.lpVerbW = MAKEINTRESOURCEW( cmdId );
				ici.nShow = SW_SHOWNORMAL;
				ici.ptInvoke.x = pt.x;
				ici.ptInvoke.y = pt.y;
	
				return pCm->InvokeCommand( reinterpret_cast<CMINVOKECOMMANDINFO*>( &ici ) );
			}

			return E_INVALIDARG;
		}

		//
		// Invoke command using context menu interface (creates context menu by itself)
		//
		static HRESULT invokeCommand( const std::string& cmdVerb, const std::vector<std::wstring>& items )
		{
			TPidl pidls;
			auto shellFolder = getPidlsFromFileList( items, pidls );

			if( shellFolder )
			{
				// get context menu interface
				LPCONTEXTMENU pCm = nullptr;
				shellFolder->GetUIObjectOf( NULL, (UINT)pidls.size(), (LPCITEMIDLIST*)&pidls[0], IID_IContextMenu, NULL, (void**)&pCm );

				if( pCm )
				{
					// create and show shell context menu
					auto hMenu = CreatePopupMenu();
					pCm->QueryContextMenu( hMenu, 0, 1, 0x7FFF, CMF_NORMAL );

					CMINVOKECOMMANDINFOEX ici = { 0 };
					ici.cbSize = sizeof( ici );
					ici.lpVerb = cmdVerb.c_str();
					ici.nShow = SW_HIDE;
	
					HRESULT hr = pCm->InvokeCommand( reinterpret_cast<CMINVOKECOMMANDINFO*>( &ici ) );

					pCm->Release();
					shellFolder->Release();
					DestroyMenu( hMenu );

					freePidls( pidls );

					return hr;
				}
			}

			return E_FAIL;
		}

		//
		// Create new item (doesn't work for long paths)
		//
		static HRESULT createItem( const std::wstring& dirName, const std::wstring& itemName, DWORD attributes, bool *aborted = nullptr, HWND hWndOwner = nullptr )
		{
			BOOL operationAborted = FALSE;

			CComPtr<IFileOperation> pfo;
			HRESULT hr = CoCreateInstance( CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS( &pfo ) );

			if( SUCCEEDED( hr ) )
			{
				CComPtr<IShellItem> psiaItem;

				// create an IShellItem from the supplied sources path
				hr = SHCreateItemFromParsingName( dirName.c_str(), nullptr, IID_PPV_ARGS( &psiaItem ) );

				if( SUCCEEDED( hr ) )
				{
					// set flags
					hr = pfo->SetOperationFlags( FOF_NOCONFIRMMKDIR );
					hr = pfo->SetOwnerWindow( hWndOwner );

					// add the operation
					hr = pfo->NewItem( psiaItem, attributes, itemName.c_str(), nullptr, nullptr );

					if( SUCCEEDED( hr ) )
					{
						// perform the operation to delete the items
						hr = pfo->PerformOperations();

						if( SUCCEEDED( hr ) )
							hr = pfo->GetAnyOperationsAborted( &operationAborted );
					}
				}
			}

			if( aborted )
				*aborted = !!operationAborted;

			return hr;
		}

		//
		// Delete items or send them to recycle bin - called from separate thread
		//
		static bool deleteItems( const std::vector<std::wstring>& entries, bool recycle = false, HWND hWndOwner = nullptr )
		{
			CComInitializer _com( COINIT_APARTMENTTHREADED );

			BOOL operationAborted = FALSE;

			CComPtr<IFileOperation> pfo;
			HRESULT hr = CoCreateInstance( CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS( &pfo ) );

			if( SUCCEEDED( hr ) )
			{
				CComPtr<IShellItemArray> psiaItems;

				// create an IShellItem from the supplied sources path
				hr = ShellUtils::createShellItemArrayFromPaths( entries, &psiaItems );
				if( SUCCEEDED( hr ) )
				{
					// set flags
					hr = pfo->SetOperationFlags( ( recycle ? FOF_ALLOWUNDO | FOFX_RECYCLEONDELETE : 0 ) );
					hr = pfo->SetOwnerWindow( hWndOwner );

					// add the operation
					hr = pfo->DeleteItems( psiaItems );
				}

				if( SUCCEEDED( hr ) )
				{
					// perform the operation to delete the items
					hr = pfo->PerformOperations();

					if( SUCCEEDED( hr ) )
						hr = pfo->GetAnyOperationsAborted( &operationAborted );
				}
			}

			return SUCCEEDED( hr ) && !operationAborted;
		}

		//
		// Copy/move items file or directory - called from separate thread
		//
		static bool copyItems( const std::vector<std::wstring>& entries, const std::wstring& target, bool move = false, HWND hWndOwner = nullptr )
		{
			CComInitializer _com( COINIT_APARTMENTTHREADED );

			BOOL operationAborted = FALSE;

			CComPtr<IFileOperation> pfo;
			HRESULT hr = CoCreateInstance( CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS( &pfo ) );

			if( SUCCEEDED( hr ) )
			{
				CComPtr<IShellItemArray> psiaItems;
				CComPtr<IShellItem> psiTo;
				CComPtr<IBindCtx> pbc;
				CComPtr<CFileSysBindData> ifs;

				// create binding context to pretend there is a folder there
				if( !FsUtils::isPathExisting( target ) )
				{
					WIN32_FIND_DATA wfd = { 0 };

					if( entries.size() == 1 && !StringUtils::endsWith( target, L"\\" ) && FsUtils::getFileInfo( entries[0], wfd ) )
					{
						// TODO: copying a file to the target directory with a different name does not work
						hr = SHCreateItemFromParsingName( PathUtils::stripFileName( target ).c_str(), pbc, IID_PPV_ARGS( &psiTo ) );
						wcsncpy( wfd.cFileName, PathUtils::stripPath( target ).c_str(), MAX_PATH );
						wfd.cAlternateFileName[0] = L'\0';
					}
					else
						wfd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;

					if( ( ifs = new (std::nothrow)CFileSysBindData() ) != nullptr )
						hr = ifs->SetFindData( &wfd );
					if( SUCCEEDED( hr ) )
						hr = CreateBindCtx( 0, &pbc );
					if( SUCCEEDED( hr ) )
						hr = pbc->RegisterObjectParam( STR_FILE_SYS_BIND_DATA, ifs );
				}

				// create an IShellItem from the supplied target path
				if( SUCCEEDED( hr ) && psiTo == nullptr )
					hr = SHCreateItemFromParsingName( target.c_str(), pbc, IID_PPV_ARGS( &psiTo ) );

				if( SUCCEEDED( hr ) )
				{
					hr = ShellUtils::createShellItemArrayFromPaths( entries, &psiaItems );
					if( SUCCEEDED( hr ) )
					{
						// set flags
						hr = pfo->SetOperationFlags( FOF_NOCONFIRMMKDIR | FOF_ALLOWUNDO );
						hr = pfo->SetOwnerWindow( hWndOwner );

						// add the operation
						if( !move )
							hr = pfo->CopyItems( psiaItems, psiTo );
						else
							hr = pfo->MoveItems( psiaItems, psiTo );

						if( SUCCEEDED( hr ) )
						{
							// perform the operation to copy/move the items
							hr = pfo->PerformOperations();

							if( SUCCEEDED( hr ) )
								hr = pfo->GetAnyOperationsAborted( &operationAborted );
						}
					}
				}
			}

			return SUCCEEDED( hr ) && !operationAborted;
		}

		//
		// Rename file or directory
		//
		static bool renameItem( const std::wstring& oldName, const std::wstring& newName, BOOL *aborted = nullptr )
		{
			CComPtr<IFileOperation> pfo;

			auto hr = CoCreateInstance( CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS( &pfo ) );
			if( SUCCEEDED( hr ) )
			{
				// create an IShellItem from the supplied source path
				CComPtr<IShellItem> psiFrom;
				hr = SHCreateItemFromParsingName( oldName.c_str(), nullptr, IID_PPV_ARGS( &psiFrom ) );
				if( SUCCEEDED( hr ) )
				{
					// set flags
					pfo->SetOperationFlags( FOFX_NOCOPYHOOKS );

					// add the operation
					hr = pfo->RenameItem( psiFrom, PathUtils::stripPath( newName ).c_str(), nullptr );
				}

				if( SUCCEEDED( hr ) )
				{
					// perform the operation to rename the file
					hr = pfo->PerformOperations();

					if( SUCCEEDED( hr ) && aborted )
						hr = pfo->GetAnyOperationsAborted( aborted );
				}
			}

			return SUCCEEDED( hr );
		}

		//
		// Copy/cut items into clipboard through IDataObject interface
		//
		static bool copyItemsToClipboard( const std::vector<std::wstring>& items, bool copyOnly = true )
		{
			TPidl pidls;
			auto shellFolder = getPidlsFromFileList( items, pidls );

			if( shellFolder )
			{
				CComPtr<IDataObjectAsyncCapability> pAsyncOperation;
				CComPtr<IDataObject> pData;

				auto hr = shellFolder->GetUIObjectOf( nullptr, (UINT)pidls.size(), (LPCITEMIDLIST*)&pidls[0], IID_IDataObject, nullptr, (void**)&pData );

				if( SUCCEEDED( hr ) )
				{
					FORMATETC fmte = { (CLIPFORMAT)RegisterClipboardFormat( CFSTR_PREFERREDDROPEFFECT ), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
					STGMEDIUM stgm = { 0 };
					stgm.tymed = TYMED_HGLOBAL;
					stgm.pUnkForRelease = nullptr;
					stgm.hGlobal = GlobalAlloc( GMEM_FIXED, sizeof( DWORD ) );

					DWORD *pDropEffect = (DWORD*)GlobalLock( stgm.hGlobal );
					*pDropEffect = ( copyOnly ? DROPEFFECT_COPY : DROPEFFECT_MOVE );
					GlobalUnlock( stgm.hGlobal );

					hr = pData->SetData( &fmte, &stgm, TRUE ); // receiver must call ReleaseStgMedium( &stgm );

					// make operation asynchronous
					hr = pData->QueryInterface( IID_IDataObjectAsyncCapability, (void **)&pAsyncOperation );
					hr = pAsyncOperation->SetAsyncMode( VARIANT_TRUE );

					if( SUCCEEDED( hr ) )
					{
						hr = OleSetClipboard( pData );
						if( SUCCEEDED( hr ) )
						{
							hr = OleFlushClipboard();
						}
					}
				}

				shellFolder->Release();
				freePidls( pidls );

				return SUCCEEDED( hr );
			}

			return false;
		}

		//
		// Paste items from clipboard into given directory - called from separate thread
		//
		static void copyItemsFromClipboard( const std::wstring& targetPath, HWND hWndOwner = nullptr )
		{
			HRESULT hr = OleInitialize( NULL );

			LPDATAOBJECT pData;
			hr = OleGetClipboard( &pData );

			if( SUCCEEDED( hr ) )
			{
				CComPtr<IFileOperation> pfo;
				CComPtr<IShellItem> psiTo;

				hr = SHCreateItemFromParsingName( targetPath.c_str(), nullptr, IID_PPV_ARGS( &psiTo ) );

				if( SUCCEEDED( hr ) )
				{
					hr = CoCreateInstance( CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS( &pfo ) );
					if( SUCCEEDED( hr ) )
					{
						FORMATETC fmte = { (CLIPFORMAT)RegisterClipboardFormat( CFSTR_PREFERREDDROPEFFECT ), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
						STGMEDIUM stgm = { 0 }; // defend against buggy data object

						hr = pfo->SetOperationFlags( FOF_NOCONFIRMMKDIR | FOF_ALLOWUNDO | FOF_RENAMEONCOLLISION );
						hr = pfo->SetOwnerWindow( hWndOwner );
						hr = pData->GetData( &fmte, &stgm );

						if( SUCCEEDED( hr ) && stgm.hGlobal != nullptr )
						{
							DWORD *pDropEffect = (DWORD*)GlobalLock( stgm.hGlobal );
							if( pDropEffect != nullptr )
							{
								if( ( *pDropEffect & DROPEFFECT_MOVE ) == DROPEFFECT_MOVE )
									hr = pfo->MoveItems( pData, psiTo );
								else
									hr = pfo->CopyItems( pData, psiTo );

								GlobalUnlock( stgm.hGlobal );
							}

							ReleaseStgMedium( &stgm );
						}
					}
				}

				pData->Release();

				if( SUCCEEDED( hr ) )
					hr = pfo->PerformOperations();
			}

			OleUninitialize();
		}

		//
		// Get items names from IShellItemArray object
		//
		static std::vector<std::wstring> getItemsFromShellArray( IShellItemArray *psia )
		{
			std::vector<std::wstring> itemsOut;

			DWORD itemsCount;
			HRESULT hr = psia->GetCount( &itemsCount );
			if( SUCCEEDED( hr ) )
			{
				for( DWORD i = 0; i < itemsCount; ++i )
				{
					IShellItem *ppsi;
					hr = psia->GetItemAt( i, &ppsi );
					if( SUCCEEDED( hr ) )
					{
						PIDLIST_ABSOLUTE pidl;
						hr = SHGetIDListFromObject( ppsi, &pidl );
						if( SUCCEEDED( hr ) )
						{
							itemsOut.push_back( getPathFromPidl( pidl ) );
							CoTaskMemFree( pidl );
						}

						safeRelease( &ppsi );
					}
				}
			}

			return itemsOut;
		}

		//
		// Run moniker as elevated
		//
		static HRESULT CoCreateInstanceAsAdmin( HWND hwnd, REFCLSID rclsid, REFIID riid, __out void **ppv )
		{
			BIND_OPTS3 bo;
			WCHAR wszCLSID[50];
			WCHAR wszMonikerName[300];

			StringFromGUID2( rclsid, wszCLSID, ARRAYSIZE( wszCLSID ) );

			HRESULT hr = StringCchPrintf( wszMonikerName, ARRAYSIZE( wszMonikerName ), L"Elevation:Administrator!new:%s", wszCLSID );
			if( FAILED( hr ) )
				return hr;

			memset( &bo, 0, sizeof( bo ) );
			bo.cbStruct = sizeof( bo );
			bo.hwnd = hwnd;
			bo.dwClassContext = CLSCTX_LOCAL_SERVER;

			return CoGetObject( wszMonikerName, &bo, riid, ppv );
		}

		//
		// Open explorer with items selected
		//
		static HRESULT selectItemsInExplorer( const std::wstring& path, std::vector<std::wstring>& items )
		{
			TPidl pidls;
			auto shellFolder = getPidlsFromFileList( items, pidls );

			HRESULT hr = E_NOT_SET;
			if( shellFolder )
			{
				PIDLIST_ABSOLUTE pidlFolder = ILCreateFromPath( path.c_str() );
				if( pidlFolder )
				{
					hr = SHOpenFolderAndSelectItems( pidlFolder, (UINT)pidls.size(), (LPCITEMIDLIST*)&pidls[0], 0 );

					ILFree( pidlFolder );
					shellFolder->Release();
					freePidls( pidls );
				}
			}

			return hr;
		}

		// Get property description (used by getMediaInfo function)
		//
		static std::wstring getPropertyName( const PROPERTYKEY& propKey )
		{
			std::wstring propName;

			IPropertyDescription *pPropDesc;
			if( SUCCEEDED( PSGetPropertyDescription( propKey, IID_PPV_ARGS( &pPropDesc ) ) ) )
			{
				PWSTR pszLabel;
				if( SUCCEEDED( pPropDesc->GetDisplayName( &pszLabel ) ) )
				{
					propName = pszLabel;
					CoTaskMemFree( pszLabel );
				}

				pPropDesc->Release();
			}

			return propName;
		}

		//
		// Get property value (used by getMediaInfo function)
		//
		static std::wstring getPropertyValue( IPropertyStore *store, const PROPERTYKEY& propKey )
		{
			std::wstring propValue;

			PROPVARIANT propVar;
			if( SUCCEEDED( store->GetValue( propKey, &propVar ) ) )
			{
				if( propKey == PKEY_Media_Duration )
				{
					ULONGLONG uVal = 0;
					PropVariantToUInt64( propVar, &uVal );
					propValue = StringUtils::formatTime( uVal / 10000000 );
				}
				else if( propKey == PKEY_Audio_EncodingBitrate )
				{
					ULONG uVal = 0;
					PropVariantToUInt32( propVar, &uVal );
					propValue = std::to_wstring( uVal / 1000 );
				}
				else
				{
					BSTR bstrVal;
					PropVariantToBSTR( propVar, &bstrVal );
					propValue = bstrVal;
					SysFreeString( bstrVal );
				}
			}

			return propValue;
		}

		//
		// Try to get file media info
		//
		static std::vector<std::wstring> getMediaInfo( const std::wstring& fileName )
		{
			std::vector<std::wstring> infoOut;

			HRESULT hr = S_OK;
			CComPtr<IPropertyStore> store;
			if( SUCCEEDED( SHGetPropertyStoreFromParsingName( fileName.c_str(), NULL, GPS_READWRITE, IID_PPV_ARGS( &store ) ) ) )
			{
				DWORD propCount = 0;
				hr = store->GetCount( &propCount );

				for( DWORD i = 0; i < propCount; ++i )
				{
					PROPERTYKEY propKey;
					if( SUCCEEDED( store->GetAt( i, &propKey ) ) )
					{
						auto propName = getPropertyName( propKey );
						if( propName.length() )
						{
							auto propValue = getPropertyValue( store, propKey );
							if( propValue.length() )
							{
								infoOut.push_back( propName + L": " + propValue );
							}
						}
					}
				}
			}

			return infoOut;
		}

		//
		// Get drag & dropped files and/or directories as a string array
		//
		static std::vector<std::wstring> getDroppedItems( const HDROP& hDrop, POINT *ppt = nullptr )
		{
			TCHAR buf[MAX_PATH];
			std::vector<std::wstring> items;

			if( ppt )
				DragQueryPoint( hDrop, ppt );

			auto dropCnt = DragQueryFile( hDrop, 0xFFFFFFFF, NULL, 0 );
			for( UINT i = 0; i < dropCnt; ++i )
			{
				if( DragQueryFile( hDrop, i, buf, MAX_PATH ) )
				{
					items.push_back( buf );
				}
			}

			return items;
		}

		//
		// Create an IShellItemArray from a bunch of file paths
		//
		static HRESULT createShellItemArrayFromPaths( const std::vector<std::wstring>& items, IShellItemArray **ppsia )
		{
			*ppsia = nullptr;

			PIDLIST_ABSOLUTE *rgpidl = new PIDLIST_ABSOLUTE[items.size()];
			HRESULT hr = rgpidl ? S_OK : E_OUTOFMEMORY;

			for( size_t cpidl = 0; SUCCEEDED( hr ) && cpidl < items.size(); ++cpidl )
			{
				hr = SHParseDisplayName( items[cpidl].c_str(), nullptr, &rgpidl[cpidl], 0, nullptr );
			}

			if( SUCCEEDED( hr ) )
				hr = SHCreateShellItemArrayFromIDLists( (UINT)items.size(), (PCIDLIST_ABSOLUTE*)rgpidl, ppsia );

			for( size_t i = 0; i < items.size(); ++i )
			{
				CoTaskMemFree( rgpidl[i] );
			}

			delete[] rgpidl;

			return hr;
		}

		//
		// Get Pidl for a file item with wildcards
		//
		static IShellFolder *getPidlsFromFileName( const TCHAR *pszRelSpec, TPidl& pidls )
		{
			IShellFolder* psfParent = nullptr;
			LPITEMIDLIST pidlRel;

			// build fully qualified path name
			TCHAR *pszFullSpec = _wfullpath( nullptr, pszRelSpec, 0 );
			if( pszFullSpec == nullptr )
			{
				return nullptr;
			}

			// make copy to expand wildcards in
			TCHAR szExpandedPath[MAX_PATH];
			lstrcpy( szExpandedPath, pszFullSpec );
			TCHAR *pszWildcard = wcsrchr( szExpandedPath, L'\\' );
			if( pszWildcard )
				++pszWildcard;
			else
				pszWildcard = szExpandedPath + lstrlen( szExpandedPath );

			// Problem:
			// we could be passed a drive, a file, or a group of files (wildcard).
			// (do we take multiple filenames on command line?  if not, should we?)
			// On drive, findfirstfile will fail, but parsedisplayname will give us a pidl
			// on normal file, both will succeed
			// on wildcard, findfirstfile will always succeed, but parsedisplayname will work on 9x and not on NT.
			// So which do we do first?  in NT case, either one failing has to be nonfatal.
			// And, to make things worse, on Win9x (but not NT), FindFirstFile ("E:\")
			// will behave as if we'd passed E:\* if E: is a mapped network drive,
			// so they can't get props for any mapped network drive.  Hence the hack strlen
			// comparison in the initialization of bDone.

			WIN32_FIND_DATA wfd;
			HANDLE hFind;
			hFind = FindFirstFile( pszFullSpec, &wfd );
			bool bDone = ( hFind == INVALID_HANDLE_VALUE ) || ( lstrlen( pszFullSpec ) <= 3 );

			// expand wildcards -- get all files matching spec
			while( !bDone )
			{
				// build psfParent from the first full filename
				if( !psfParent )
				{
					lstrcpy( pszWildcard, wfd.cFileName );
					getPidlAndShellFolder( szExpandedPath, &psfParent, &pidlRel );
				}

				// then use psfParent to pidl-ize them
				if( psfParent )
				{
					LPITEMIDLIST pidl;
					ULONG cch;

					// TODO -- could optionally mask hidden, system, etc. files here

					if( SUCCEEDED( psfParent->ParseDisplayName( NULL, NULL, wfd.cFileName, &cch, &pidl, NULL ) ) )
					{
						pidls.push_back( pidl );
					}
				}

				bDone = FindNextFile( hFind, &wfd ) ? false : true;
			}

			FindClose( hFind );

			// if we didn't get anything from FindFirstFile, maybe it's a drive letter...
			if( pidls.empty() )
			{
				if( !psfParent )
					getPidlAndShellFolder( pszFullSpec, &psfParent, &pidlRel );

				if( psfParent )
				{
					// we were able to find a pidl directly from the given path,
					// but not from FindFirstFile.  This is what happens when the
					// path is, i.e., a drive (C:\)
					// so just use the first pidl we got.
					pidls.push_back( pidlRel );
				}
			}

			free( pszFullSpec );

			return psfParent;
		}

		//
		// Get Pidls for a files item list
		//
		static IShellFolder *getPidlsFromFileList( const std::vector<std::wstring>& items, TPidl& pidls )
		{
			LPSHELLFOLDER psfParent = nullptr;

			LPMALLOC lpMalloc = nullptr;
			SHGetMalloc( &lpMalloc );

			size_t pidlCount = 0;
			for( size_t i = 0; i < items.size(); ++i )
			{
				LPITEMIDLIST pidlRel = nullptr;
				getPidlAndShellFolder( items[i].c_str(), &psfParent, &pidlRel );

				if( pidlRel )
				{
					pidls.push_back( copyPidl( pidlRel, lpMalloc ) );
					lpMalloc->Free( pidlRel );
				}
			}

			lpMalloc->Release();

			return psfParent;
		}

		//
		// Get string path from pidl
		//
		static std::wstring getPathFromPidl( LPITEMIDLIST pidl )
		{
			std::wstring strOut;

			TCHAR sPath[MAX_PATH] = { 0 };
			if( pidl && SHGetPathFromIDList( pidl, sPath ) )
			{
				strOut = sPath;
			}

			return strOut;
		}

		//
		// Cleanup all Pidls
		//
		static void freePidls( TPidl& pidls )
		{
			IMalloc* shMalloc = nullptr;
			if( SUCCEEDED( SHGetMalloc( &shMalloc ) ) )
			{
				for( size_t i = 0; i < pidls.size(); i++ )
				{
					shMalloc->Free( pidls[i] );
				}

				pidls.clear();

				shMalloc->Release();
			}
		}

		//
		// Allocate memory and copy Pidl
		//
		static LPITEMIDLIST copyPidl( LPITEMIDLIST pidl, IMalloc* shMalloc )
		{
			int cb = pidl->mkid.cb + 2; // add two for terminator

			if( PidlNext( pidl )->mkid.cb != 0 )
			{
				return nullptr; // must be relative (1-length) pidl!
			}

			LPITEMIDLIST pidlNew = (LPITEMIDLIST)shMalloc->Alloc( cb );
			if( pidlNew != nullptr )
			{
				memcpy( pidlNew, pidl, cb );
			}

			return pidlNew;
		}

		//
		// Uses the Shell's IShellLink and IPersistFile interfaces to create and store a shortcut to the specified object
		//
		static HRESULT createShortcut( const std::wstring& pathObj, const std::wstring& shortcutPath, const std::wstring& desc = L"" )
		{
			HRESULT hr;
			IShellLink *psl;

			// Get a pointer to the IShellLink interface. It is assumed that CoInitialize
			// has already been called.
			hr = CoCreateInstance( CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl );
			if( SUCCEEDED( hr ) )
			{
				IPersistFile *ppf;

				// Set the path to the shortcut target and add the description. 
				psl->SetPath( pathObj.c_str() );
				psl->SetDescription( desc.c_str() );

				// Query IShellLink for the IPersistFile interface, used for saving the 
				// shortcut in persistent storage. 
				hr = psl->QueryInterface( IID_IPersistFile, (LPVOID*)&ppf );

				if( SUCCEEDED( hr ) )
				{
					// Save the link by calling IPersistFile::Save. 
					hr = ppf->Save( shortcutPath.c_str(), TRUE );
					ppf->Release();
				}
				psl->Release();
			}
			return hr;
		}

		//
		// Resolve a ShellLink (i.e. c:\path\shortcut.lnk) to a real path
		//
		static std::wstring getShortcutTarget( const std::wstring& shortcutPath )
		{
			std::wstring targetPath;

			// assume failure
			SHFILEINFO shfi;
			if( ( SHGetFileInfo( shortcutPath.c_str(), 0, &shfi, sizeof( SHFILEINFO ), SHGFI_ATTRIBUTES ) == 0 )
			  || !( shfi.dwAttributes & SFGAO_LINK ) )
			{
				return L"";
			}
 
			// obtain the IShellLink interface
			IShellLink *psl;
			if( SUCCEEDED( CoCreateInstance( CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl ) ) )
			{
				IPersistFile *ppf;
				if( SUCCEEDED( psl->QueryInterface( IID_IPersistFile, (LPVOID*)&ppf ) ) )
				{
					if( SUCCEEDED( ppf->Load( shortcutPath.c_str(), STGM_READ ) ) )
					{
						// resolve the link, this may post UI to find the link
						if( SUCCEEDED( psl->Resolve( 0, SLR_NO_UI ) ) )
						{
							TCHAR pszFilePath[MAX_PATH];
							psl->GetPath( pszFilePath, MAX_PATH, NULL, 0 );
							targetPath = pszFilePath;
							//psl->GetDescription( pszFilePath, MAX_PATH );
						}
					}
 					ppf->Release();
				}
 				psl->Release();
			}

			return targetPath;
		}

		//
		// Get network neighborhood computers list
		//
		static std::vector<std::wstring> getNeighborhoodList()
		{
			std::vector<std::wstring> netItems;

			LPSHELLFOLDER shellFolder = nullptr;
			if( SUCCEEDED( SHGetDesktopFolder( &shellFolder ) ) )
			{
				LPITEMIDLIST networkPidl = nullptr;
				if( SUCCEEDED( SHGetSpecialFolderLocation( NULL, CSIDL_NETWORK, &networkPidl ) ) )
				{
					LPSHELLFOLDER networkFolder = nullptr;
					if( SUCCEEDED( shellFolder->BindToObject( networkPidl, NULL, IID_IShellFolder, (void**)&networkFolder ) ) )
					{
						IEnumIDList *networkEnum = nullptr;
						if( SUCCEEDED( networkFolder->EnumObjects( NULL, SHCONTF_FOLDERS, &networkEnum ) ) )
						{
							TCHAR szName[MAX_PATH];
							if( SUCCEEDED( networkEnum->Reset() ) )
							{
								LPITEMIDLIST childPidl = nullptr;
								while( networkEnum->Next( 1, &childPidl, NULL ) == S_OK )
								{
									STRRET str;
									if( SUCCEEDED( networkFolder->GetDisplayNameOf( childPidl, SHGDN_NORMAL, &str ) ) )
									{
										StrRetToBuf( &str, childPidl, szName, MAX_PATH );
										netItems.push_back( szName );
									}
									CoTaskMemFree( childPidl );
									childPidl = nullptr;
								}
							}
							networkEnum->Release();
						}
						networkFolder->Release();
					}
					CoTaskMemFree( networkPidl );
				}
				shellFolder->Release();
			}

			return netItems;
		}

		//
		// Get known system and user folders path - usage: getKnownFolderPath( FOLDERID_Documents )
		//
		static std::wstring getKnownFolderPath( const KNOWNFOLDERID& rfid )
		{
			std::wstring knownPath;

			PWSTR outPath = nullptr;
			if( SUCCEEDED( SHGetKnownFolderPath( rfid, 0, NULL, &outPath ) ) )
			{
				knownPath = outPath;
				CoTaskMemFree( outPath );
			}

			return knownPath;
		}

		//
		// Try to get Pidl for an item
		//
		static bool getPidlAndShellFolder( const TCHAR *pszPath, IShellFolder **ppsfOut, LPITEMIDLIST *ppidlOut )
		{
			IShellFolder *pDesktop = nullptr;
			IMalloc *shMalloc = nullptr;
			LPITEMIDLIST pidlFull = nullptr;
			ULONG cch;
			ULONG attrs;
			bool retVal = false;

			if( !SUCCEEDED( SHGetDesktopFolder( &pDesktop ) ) )
				goto BailOut;

			if( !SUCCEEDED( SHGetMalloc( &shMalloc ) ) )
				goto BailOut;

			// get fully-qualified pidl
			if( !SUCCEEDED( pDesktop->ParseDisplayName( NULL, NULL, const_cast<TCHAR*>( pszPath ), &cch, &pidlFull, &attrs ) ) )
				goto BailOut;
	
			IShellFolder *psfCurr, *psfNext;
			if( !SUCCEEDED( pDesktop->QueryInterface( IID_IShellFolder, (LPVOID*)&psfCurr ) ) )
				goto BailOut;

			// for each pidl component, bind to folder
			LPITEMIDLIST pidlNext, pidlLast;
			pidlNext = PidlNext( pidlFull );
			pidlLast = pidlFull;
	
			while( pidlNext->mkid.cb != 0 )
			{
				UINT uSave = pidlNext->mkid.cb; // stop the chain temporarily
				pidlNext->mkid.cb = 0;          // so we can bind to the next folder 1 deeper

				if( !SUCCEEDED( psfCurr->BindToObject( pidlLast, NULL, IID_IShellFolder, (LPVOID*)&psfNext ) ) )
					goto BailOut;

				pidlNext->mkid.cb = uSave; // restore the chain

				psfCurr->Release(); // and set up to work with the next-level folder
				psfCurr = psfNext;
				pidlLast = pidlNext;

				pidlNext = PidlNext( pidlNext ); // advance to next pidl
			}

			retVal = true;

			*ppidlOut = copyPidl( pidlLast, shMalloc );
			*ppsfOut = psfCurr;

		BailOut:
			// cleanup
			if( pidlFull != nullptr && shMalloc != nullptr )
				shMalloc->Free( pidlFull ); // other pidl's were only offsets into this, and don't need freeing

			if( pDesktop != nullptr )
				pDesktop->Release();

			if( shMalloc != nullptr )
				shMalloc->Release();

			return retVal;
		}

		// do not instantiate this class/struct
		ShellUtils() = delete;
		ShellUtils( ShellUtils const& ) = delete;
		void operator=( ShellUtils const& ) = delete;
	};
}
