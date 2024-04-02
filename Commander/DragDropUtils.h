#pragma once

#include <strsafe.h>
#include <commoncontrols.h>

#include "ShellUtils.h"

namespace Commander
{
	// encapsulation of the shell drag drop presentation and Drop handling functionality
	// this provides the following features 1) drag images, 2) drop tips,
	// 3) ints OLE and registers drop target, 4) conversion of the data object item into shell items
	//
	// to use this
	//      1) have the object that represents the main window of your app derive
	//         from CDragDropHelper exposing it as public.
	//         "class CAppClass : public CDragDropHelper"
	//      2) add IDropTarget to the QueryInterface() implementation of your class
	//         that is a COM object itself
	//      3) in your WM_INITDIALOG handling call
	//         InitializeDragDropHelper(_hdlg, DROPIMAGE_LINK, NULL) passing
	//         the dialog window and drop tip template and type
	//      4) in your WM_DESTROY handler add a call to TerminateDragDropHelper(). note not
	//         doing this will lead to a leak of your object so this is important
	//      5) add the delaration and implementation of onDrop() to your class, this
	//         gets called when the drop happens

	class CDragDropHelper : public IDropTarget
	{
		// for debugging!!
		/*#define PrintDebug2( format, ... ) printDebug2( __FUNCTIONW__, _T(format), __VA_ARGS__ )
		__inline void printDebug2( wchar_t *funcName, wchar_t *fmt, ... )
		{
			va_list args;
			va_start( args, fmt );

			// extract string
			const size_t MAXENTRYSIZE = 64000;
			wchar_t text[MAXENTRYSIZE];
			int ret = vswprintf( text, MAXENTRYSIZE, fmt, args );

			va_end( args );

			// format string stream
			std::wostringstream sstr;
			sstr << L"[" << funcName << L"] " << text << L"\n";

			// write to debug output
			OutputDebugString( sstr.str().c_str() );
		} // delete this !!*/

	public:
		CDragDropHelper() : _pdth( NULL ), _pdtobj( NULL ), _hrOleInit( OleInitialize( NULL ) ), _hwndRegistered( NULL ), _dropImageType( DROPIMAGE_LABEL ), _pszDropTipTemplate( NULL )
		{
			CoCreateInstance( CLSID_DragDropHelper, NULL, CLSCTX_INPROC, IID_PPV_ARGS( &_pdth ) );
		}

		~CDragDropHelper()
		{
			ShellUtils::safeRelease( &_pdth );
			if( SUCCEEDED( _hrOleInit ) )
			{
				OleUninitialize();
			}
		}

		void SetDropTipTemplate( PCWSTR pszDropTipTemplate )
		{
			_pszDropTipTemplate = pszDropTipTemplate;
		}

		void InitializeDragDropHelper( HWND hwnd, DROPIMAGETYPE dropImageType, PCWSTR pszDropTipTemplate )
		{
			if( SUCCEEDED( _hrOleInit ) )
			{
				_dropImageType = dropImageType;
				_pszDropTipTemplate = pszDropTipTemplate;
				if( SUCCEEDED( RegisterDragDrop( hwnd, this ) ) )
				{
					_hwndRegistered = hwnd;
				}
			}
		}

		void TerminateDragDropHelper()
		{
			if( _hwndRegistered )
			{
				RevokeDragDrop( _hwndRegistered );
				_hwndRegistered = NULL;
			}
		}

		HRESULT GetDragDropHelper( REFIID riid, void **ppv )
		{
			*ppv = NULL;
			return _pdth ? _pdth->QueryInterface( riid, ppv ) : E_NOINTERFACE;
		}

		// direct access to the data object, if you don't want to use the shell item array
		IDataObject *GetDataObject()
		{
			return _pdtobj;
		}

		// IDropTarget
		IFACEMETHODIMP DragEnter( IDataObject *pdtobj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect )
		{
			//PrintDebug2( "effect: %u", *pdwEffect );

			if( _pdth )
			{
				POINT ptT = { pt.x, pt.y };
				_pdth->DragEnter( _hwndRegistered, pdtobj, &ptT, *pdwEffect );
			}

			_droppedItems = getDroppedData( pdtobj );

			// leave *pdwEffect unchanged, except when there is no file dropped
			if( _droppedItems.empty() || _dropImageType == DROPIMAGE_NONE )
				*pdwEffect = 0;

			//_pdtobj = pdtobj;
			setInterface( &_pdtobj, pdtobj );

			EFcCommand cmd;
			_dropImageType = getDropImageType( grfKeyState, *pdwEffect, &cmd );

			setDropTip( _pdtobj, _dropImageType, getDropTipText( cmd ).c_str(), _dropLocationText.c_str() );

			/*_droppedItems.clear();

			IShellItem *psi;
			HRESULT hr = CreateItemFromObject(pdtobj, IID_PPV_ARGS(&psi));
			if (SUCCEEDED(hr))
			{
				PWSTR pszName;
				hr = psi->GetDisplayName(SIGDN_NORMALDISPLAY, &pszName);
				if (SUCCEEDED(hr))
				{
					PrintDebug2("0x%08X, %ls (%ls)", grfKeyState, _pszDropTipTemplate, pszName);
					_dropItemName = pszName;
					_dropImageType = getDropImageType( grfKeyState, *pdwEffect );
					SetDropTip(pdtobj, _dropImageType, ( _pszDropTipTemplate ? _pszDropTipTemplate : L"%1" ), pszName);
					CoTaskMemFree(pszName);
				}
				psi->Release();
			}
			else
			{
				FORMATETC fmte = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
				STGMEDIUM medium;
				hr = pdtobj->GetData(&fmte, &medium);
				if (SUCCEEDED(hr))
				{
					HDROP hDrop = (HDROP)GlobalLock(medium.hGlobal);
					if (hDrop)
					{
						_droppedItems = ShellUtils::getDroppedItems(hDrop);

						GlobalUnlock(medium.hGlobal);

						_dropItemName = _droppedItems[0];
						_dropImageType = getDropImageType( grfKeyState, *pdwEffect );
						SetDropTip(pdtobj, _dropImageType, (_pszDropTipTemplate ? _pszDropTipTemplate : L"%1"), _dropItemName.c_str());
						PrintDebug2("%ls", _droppedItems[0].c_str());
					}
					ReleaseStgMedium(&medium);
				}
			}*/

			return S_OK;
		}

		IFACEMETHODIMP DragOver( DWORD grfKeyState, POINTL pt, DWORD *pdwEffect )
		{
			if( _pdth )
			{
				POINT ptT = { pt.x, pt.y };
				_pdth->DragOver( &ptT, *pdwEffect );
			}

			// leave *pdwEffect unchanged, except when there is no file dropped
			if( _droppedItems.empty() || _dropImageType == DROPIMAGE_NONE )
				*pdwEffect = 0;

			EFcCommand cmd;
			_dropImageType = getDropImageType( grfKeyState, *pdwEffect, &cmd );

			setDropTip( _pdtobj, _dropImageType, getDropTipText( cmd ).c_str(), _dropLocationText.c_str() );

			return S_OK;
		}

		IFACEMETHODIMP DragLeave()
		{
			//PrintDebug2( "Leave" );

			if( _pdth )
			{
				_pdth->DragLeave();
			}

			//_dropItemName = L"";

			clearDropTip( _pdtobj );
			ShellUtils::safeRelease( &_pdtobj );

			return S_OK;
		}

		IFACEMETHODIMP Drop( IDataObject *pdtobj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect )
		{
			//PrintDebug2( "items size: %zu, effect: %u", _droppedItems.size(), *pdwEffect );

			if( _pdth )
			{
				POINT ptT = { pt.x, pt.y };
				_pdth->Drop( pdtobj, &ptT, *pdwEffect );
			}

			/*if( _droppedItems.empty() )
			{
				IShellItemArray *psia;
				HRESULT hr = SHCreateShellItemArrayFromDataObject( _pdtobj, IID_PPV_ARGS( &psia ) );

				if( SUCCEEDED( hr ) )
				{
					_droppedItems = ShellUtils::getItemsFromShellArray( psia );
					psia->Release();
				}
				else
				{
					onDropError( _pdtobj );
					return hr;
				}
			}*/

			EFcCommand cmd;
			getDropImageType( grfKeyState, *pdwEffect, &cmd );

			switch( cmd ) {
			case EFcCommand::MoveItems:
				*pdwEffect = DROPEFFECT_NONE;
				break;
			case EFcCommand::CopyItems:
				*pdwEffect = DROPEFFECT_COPY;
				break;
			case EFcCommand::LinkItem:
				*pdwEffect = DROPEFFECT_LINK;
				break;
			}

			onDrop( getDroppedData( pdtobj ), cmd );

			return S_OK;
		}

	private:
		// client provides
		virtual bool onDrop( const std::vector<std::wstring>& items, EFcCommand cmd ) { return false; }
		/*virtual bool onDropError( IDataObject *pdtobj )
		{
			UNREFERENCED_PARAMETER( pdtobj );
			PrintDebug2( "Drop Error." );
			return false;
		}*/

		// assign an interface pointer, release old, capture ref to new, can be used to set to zero too
		template<typename T> HRESULT setInterface( T **ppT, IUnknown *punk )
		{
			ShellUtils::safeRelease( ppT );
			return punk ? punk->QueryInterface( ppT ) : E_NOINTERFACE;
		}

		// declare a static CLIPFORMAT and pass that that by ref as the first param
		__inline CLIPFORMAT getClipboardFormat( CLIPFORMAT *pcf, PCWSTR pszFormat )
		{
			if( *pcf == 0 )
			{
				*pcf = (CLIPFORMAT)RegisterClipboardFormat( pszFormat );
			}
			return *pcf;
		}

		// get droptip text from command
		__inline std::wstring getDropTipText( EFcCommand cmd )
		{
			std::wstring dropTip = L"%1";

			switch( cmd ) {
			case EFcCommand::MoveItems:
				dropTip = L"Move to %1";
				break;
			case EFcCommand::CopyItems:
				dropTip = L"Copy to %1";
				break;
			case EFcCommand::LinkItem:
				dropTip = L"Create link in %1";
				break;
			}

			return dropTip;
		}

		// get image according to modifier keys' state, fast-copy command for file commander
		__inline DROPIMAGETYPE getDropImageType( DWORD keyState, DWORD effect, EFcCommand *cmd = nullptr )
		{
			DROPIMAGETYPE dropImageType = DROPIMAGE_MOVE;
			if( cmd ) { *cmd = EFcCommand::MoveItems; }

			if( ( effect & ( DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK ) ) == ( DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK ) )
			{
				if( ( keyState & ( MK_SHIFT | MK_CONTROL ) ) == ( MK_SHIFT | MK_CONTROL ) || ( keyState & MK_ALT ) == MK_ALT ) {
					if( cmd ) { *cmd = EFcCommand::LinkItem; }
					dropImageType = DROPIMAGE_LINK;
				}
				else if( ( keyState & MK_CONTROL ) == MK_CONTROL ) {
					if( cmd ) { *cmd = EFcCommand::CopyItems; }
					dropImageType = DROPIMAGE_COPY;
				}
			}
			else if( ( effect & ( DROPEFFECT_COPY | DROPEFFECT_MOVE ) ) == ( DROPEFFECT_COPY | DROPEFFECT_MOVE ) )
			{
				if( ( keyState & MK_CONTROL ) == MK_CONTROL ) {
					if( cmd ) { *cmd = EFcCommand::CopyItems; }
					dropImageType = DROPIMAGE_COPY;
				}
			}
			else if( ( effect & DROPEFFECT_COPY ) == DROPEFFECT_COPY )
			{
				if( cmd ) { *cmd = EFcCommand::CopyItems; }
				dropImageType = DROPIMAGE_COPY;
			}
			else
			{
				dropImageType = DROPIMAGE_NONE;
			}

			return dropImageType;
		}

		__inline HRESULT setBlob( IDataObject *pdtobj, CLIPFORMAT cf, const void *pvBlob, UINT cbBlob )
		{
			void *pv = GlobalAlloc( GPTR, cbBlob );
			HRESULT hr = pv ? S_OK : E_OUTOFMEMORY;
			if( SUCCEEDED( hr ) )
			{
				CopyMemory( pv, pvBlob, cbBlob );

				FORMATETC fmte = { cf, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };

				// The STGMEDIUM structure is used to define how to handle a global memory transfer.
				// This structure includes a flag, tymed, which indicates the medium
				// to be used, and a union comprising pointers and a handle for getting whichever
				// medium is specified in tymed.
				STGMEDIUM medium = {};
				medium.tymed = TYMED_HGLOBAL;
				medium.hGlobal = pv;

				hr = pdtobj->SetData( &fmte, &medium, TRUE );
				if( FAILED( hr ) )
				{
					GlobalFree( pv );
				}
			}

			return hr;
		}

		__inline void setDropTip( IDataObject *pdtobj, DROPIMAGETYPE type, PCWSTR pszMsg, PCWSTR pszInsert )
		{
			DROPDESCRIPTION dd = { type };
			StringCchCopyW( dd.szMessage, ARRAYSIZE( dd.szMessage ), pszMsg );
			StringCchCopyW( dd.szInsert, ARRAYSIZE( dd.szInsert ), pszInsert ? pszInsert : L"" );

			static CLIPFORMAT s_cfDropDescription = 0;
			setBlob( pdtobj, getClipboardFormat( &s_cfDropDescription, CFSTR_DROPDESCRIPTION ), &dd, sizeof( dd ) );
		}

		__inline void clearDropTip( IDataObject *pdtobj )
		{
			setDropTip( pdtobj, DROPIMAGE_INVALID, L"", NULL );
		}

		__inline std::vector<std::wstring> getDroppedData( IDataObject *pdtobj )
		{
			std::vector<std::wstring> dataOut;

			FORMATETC fmte = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
			STGMEDIUM medium;

			HRESULT hr = pdtobj->GetData( &fmte, &medium );
			if( SUCCEEDED ( hr ) )
			{
				HDROP hDrop = (HDROP)GlobalLock( medium.hGlobal );
				if( hDrop )
				{
					dataOut = ShellUtils::getDroppedItems( hDrop );
					GlobalUnlock( medium.hGlobal );
				}
				ReleaseStgMedium( &medium );
			}

			return dataOut;
		}

		// helper to convert a data object with HIDA format or folder into a shell item
		// note: if the data object contains more than one item this function will fail
		// if you want to operate on the full selection use SHCreateShellItemArrayFromDataObject

		__inline HRESULT CreateItemFromObject( IUnknown *punk, REFIID riid, void **ppv )
		{
			*ppv = NULL;

			PIDLIST_ABSOLUTE pidl;
			HRESULT hr = SHGetIDListFromObject(punk, &pidl);
			if (SUCCEEDED(hr))
			{
				hr = SHCreateItemFromIDList(pidl, riid, ppv);
				ILFree(pidl);
			}
			else
			{
				// perhaps the input is from IE and if so we can construct an item from the URL
				IDataObject *pdo;
				hr = punk->QueryInterface(IID_PPV_ARGS(&pdo));
				if (SUCCEEDED(hr))
				{
					static CLIPFORMAT g_cfURL = 0;

					FORMATETC fmte = { getClipboardFormat(&g_cfURL, CFSTR_SHELLURL), NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
					STGMEDIUM medium;
					hr = pdo->GetData(&fmte, &medium);
					if (SUCCEEDED(hr))
					{
						PCSTR pszURL = (PCSTR)GlobalLock(medium.hGlobal);
						if (pszURL)
						{
							WCHAR szURL[2048];
							SHAnsiToUnicode(pszURL, szURL, ARRAYSIZE(szURL));
							hr = SHCreateItemFromParsingName(szURL, NULL, riid, ppv);
							GlobalUnlock(medium.hGlobal);
						}
						ReleaseStgMedium(&medium);
					}
					pdo->Release();
				}
				/*
				// enumerate the available formats supported by the object
				IEnumFORMATETC* pEnumFmt;
				hr = pdo->EnumFormatEtc(DATADIR_GET, &pEnumFmt);
				FORMATETC fmt;
				TCHAR szBuf[100];
				while (S_OK == pEnumFmt->Next(1, &fmt, NULL))
				{
					GetClipboardFormatName(fmt.cfFormat, szBuf, sizeof(szBuf));
					// remaining entries read from "fmt" members

					STGMEDIUM medium;
					hr = pdo->GetData(&fmt, &medium);
					if (SUCCEEDED(hr))
					{
						PCSTR pszURL = (PCSTR)GlobalLock(medium.hGlobal);
						if (pszURL)
						{
							WCHAR szURL[2048] = { 0 };
							SHAnsiToUnicode(pszURL, szURL, ARRAYSIZE(szURL));
							hr = SHCreateItemFromParsingName(szURL, NULL, riid, ppv);

							szURL[20] = L'\0'; // shorten for debug
							PrintDebug2("%ls [%ls]", szBuf, szURL);

							GlobalUnlock(medium.hGlobal);
						}
						ReleaseStgMedium(&medium);
					}
				}

				pEnumFmt->Release();
				pdo->Release();*/
			}
			return hr;
		}

	protected:
		std::wstring _dropLocationText;

	private:
		IDropTargetHelper *_pdth;
		IDataObject *_pdtobj;
		DROPIMAGETYPE _dropImageType;
		PCWSTR _pszDropTipTemplate;
		HWND _hwndRegistered;
		HRESULT _hrOleInit;
		//std::wstring _dropItemName;
		std::vector<std::wstring> _droppedItems;
	};
}
