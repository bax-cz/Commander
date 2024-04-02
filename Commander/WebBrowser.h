#pragma once

#include "BaseViewer.h"
#include "FindText.h"

#include <ExDisp.h>
#include <ExDispid.h>

//struct IHTMLTxtRange;

namespace Commander
{
	class CWebBrowser : public IOleClientSite, public IOleInPlaceSite, public IStorage, public IDispatch
	{
	public:
		CWebBrowser( HWND hWndParent );
		~CWebBrowser();

		void navigate( const std::wstring& url );

		void hiliteText( const std::wstring& text, const CFindText::CParams& params );
		void setEncoding( const std::wstring& encoding );
		std::wstring getEncoding();

		inline void goBack() { if( _pWebBrowser ) _pWebBrowser->GoBack(); }
		inline void goForward() { if( _pWebBrowser ) _pWebBrowser->GoForward(); }
		inline void refresh() { if( _pWebBrowser ) _pWebBrowser->Refresh(); }

		inline void updateRect( RECT *rct ) { if( _pOleInPlaceObj ) _pOleInPlaceObj->SetObjectRects( rct, rct ); }
		inline void setWbProcCallback( std::function<void(HWND, UINT, WPARAM, LPARAM)> procCallback ) { _wbProcCallback = procCallback; }

		bool webBrowserProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

	private:
		void hiliteTextCore( bool reverse = false );
		void searchText();
		void findTextDialog();
		bool onBeforeNavigate();
		void onNavigateComplete();
		void onDocumentComplete();
		HWND findControlWindow();

	public:
		// ---------- IUnknown ----------
		inline IFACEMETHODIMP QueryInterface( REFIID riid, void**ppvObject ) override {
			if( riid == __uuidof( IUnknown ) )
				(*ppvObject) = static_cast<IOleClientSite*>( this );
			else if( riid == __uuidof( IOleInPlaceSite ) )
				(*ppvObject) = static_cast<IOleInPlaceSite*>( this );
			else if( riid == __uuidof( IStorage ) )
				(*ppvObject) = static_cast<IStorage*>( this );
			else if( riid == DIID_DWebBrowserEvents2 )
				(*ppvObject) = static_cast<IDispatch*>( this );
			else
				return E_NOINTERFACE;

			AddRef();
			return S_OK;
		}

		inline IFACEMETHODIMP_(ULONG) AddRef()
		{
			return InterlockedIncrement( &_cRef );
		}

		inline IFACEMETHODIMP_(ULONG) Release()
		{
			long cRef = InterlockedDecrement( &_cRef );
			if( !cRef )
			{
				PrintDebug("released");
			}
			return cRef;
		}

		// ---------- IOleWindow ----------
		inline IFACEMETHODIMP GetWindow( __RPC__deref_out_opt HWND *phwnd ) override
		{
			(*phwnd) = _hWndParent;
			return S_OK;
		}

		inline IFACEMETHODIMP ContextSensitiveHelp( BOOL fEnterMode) override
		{
			return E_NOTIMPL;
		}

		// ---------- IOleInPlaceSite ----------
		inline IFACEMETHODIMP CanInPlaceActivate() override
		{
			return S_OK;
		}

		inline IFACEMETHODIMP OnInPlaceActivate() override
		{
			OleLockRunning( _pOleObj, TRUE, FALSE );

			_pOleObj->QueryInterface( &_pOleInPlaceObj );
			_pOleInPlaceObj->SetObjectRects( &_rctObj, &_rctObj );

			return S_OK;
		}

		inline IFACEMETHODIMP OnUIActivate() override
		{
			return S_OK;
		}

		inline IFACEMETHODIMP GetWindowContext( __RPC__deref_out_opt IOleInPlaceFrame **ppFrame, __RPC__deref_out_opt IOleInPlaceUIWindow **ppDoc, __RPC__out LPRECT lprcPosRect, __RPC__out LPRECT lprcClipRect, __RPC__inout LPOLEINPLACEFRAMEINFO lpFrameInfo ) override
		{
			(*ppFrame) = nullptr;
			(*ppDoc) = nullptr;

			(*lprcPosRect).left = _rctObj.left;
			(*lprcPosRect).top = _rctObj.top;
			(*lprcPosRect).right = _rctObj.right;
			(*lprcPosRect).bottom = _rctObj.bottom;
			*lprcClipRect = *lprcPosRect;

			lpFrameInfo->fMDIApp = FALSE;
			lpFrameInfo->hwndFrame = _hWndParent;
			lpFrameInfo->haccel = nullptr;
			lpFrameInfo->cAccelEntries = 0;

			return S_OK;
		}

		inline IFACEMETHODIMP Scroll( SIZE scrollExtant ) override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP OnUIDeactivate( BOOL fUndoable ) override
		{
			return S_OK;
		}

		inline IFACEMETHODIMP OnInPlaceDeactivate() override
		{
			if( IsWindow( _hWndControl ) )
				RemoveWindowSubclass( _hWndControl, wndProcWebBrowserSubclass, 0 );

			_hWndControl = nullptr;

			return S_OK;
		}

		inline IFACEMETHODIMP DiscardUndoState() override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP DeactivateAndUndo() override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP OnPosRectChange( __RPC__in LPCRECT lprcPosRect ) override
		{
			return E_NOTIMPL;
		}

		// ---------- IOleClientSite ----------
		inline IFACEMETHODIMP SaveObject() override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP GetMoniker( DWORD dwAssign, DWORD dwWhichMoniker, __RPC__deref_out_opt IMoniker **ppmk ) override
		{
			if( ( dwAssign == OLEGETMONIKER_ONLYIFTHERE ) && ( dwWhichMoniker == OLEWHICHMK_CONTAINER ) )
				return E_FAIL;

			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP GetContainer( __RPC__deref_out_opt IOleContainer **ppContainer ) override
		{
			return E_NOINTERFACE;
		}

		inline IFACEMETHODIMP ShowObject() override
		{
			return S_OK;
		}

		inline IFACEMETHODIMP OnShowWindow( BOOL fShow ) override
		{
			return S_OK;
		}

		inline IFACEMETHODIMP RequestNewObjectLayout() override
		{
			return E_NOTIMPL;
		}

		// ---------- IStorage ----------
		inline IFACEMETHODIMP CreateStream( __RPC__in_string const OLECHAR *pwcsName, DWORD grfMode, DWORD reserved1, DWORD reserved2, __RPC__deref_out_opt IStream **ppstm ) override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP OpenStream( const OLECHAR *pwcsName, void *reserved1, DWORD grfMode, DWORD reserved2, IStream **ppstm ) override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP CreateStorage( __RPC__in_string const OLECHAR *pwcsName, DWORD grfMode, DWORD reserved1, DWORD reserved2, __RPC__deref_out_opt IStorage **ppstg ) override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP OpenStorage( __RPC__in_opt_string const OLECHAR *pwcsName, __RPC__in_opt IStorage *pstgPriority, DWORD grfMode, __RPC__deref_opt_in_opt SNB snbExclude, DWORD reserved, __RPC__deref_out_opt IStorage **ppstg ) override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP CopyTo( DWORD ciidExclude, const IID *rgiidExclude, __RPC__in_opt  SNB snbExclude, IStorage *pstgDest ) override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP MoveElementTo( __RPC__in_string const OLECHAR *pwcsName, __RPC__in_opt IStorage *pstgDest, __RPC__in_string const OLECHAR *pwcsNewName, DWORD grfFlags ) override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP Commit( DWORD grfCommitFlags ) override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP Revert() override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP EnumElements( DWORD reserved1, void *reserved2, DWORD reserved3, IEnumSTATSTG **ppenum ) override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP DestroyElement( __RPC__in_string const OLECHAR *pwcsName ) override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP RenameElement( __RPC__in_string const OLECHAR *pwcsOldName, __RPC__in_string const OLECHAR *pwcsNewName ) override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP SetElementTimes( __RPC__in_opt_string const OLECHAR *pwcsName, __RPC__in_opt const FILETIME *pctime, __RPC__in_opt const FILETIME *patime, __RPC__in_opt const FILETIME *pmtime ) override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP SetClass( __RPC__in REFCLSID clsid ) override
		{
			return S_OK;
		}

		inline IFACEMETHODIMP SetStateBits( DWORD grfStateBits, DWORD grfMask ) override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP Stat( __RPC__out STATSTG *pstatstg, DWORD grfStatFlag ) override
		{
			return E_NOTIMPL;
		}

		// ---------- IDispatch ----------
		inline IFACEMETHODIMP GetTypeInfoCount( UINT *pctinfo ) override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP GetTypeInfo( UINT itinfo, LCID lcid, ITypeInfo **pptinfo ) override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP GetIDsOfNames( REFIID riid, LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgdispid ) override
		{
			return E_NOTIMPL;
		}

		inline IFACEMETHODIMP Invoke( DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pvarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr ) override
		{
			switch( dispidMember )
			{
			case DISPID_BEFORENAVIGATE:
			case DISPID_BEFORENAVIGATE2: // hyperlink clicked on
				onBeforeNavigate();
				break;

			case DISPID_KEYDOWN:
				PrintDebug("Key pressed: %u", *pvarResult->puintVal);
				break;

			case DISPID_DOCUMENTCOMPLETE:
				onDocumentComplete();
				break;

			case DISPID_NAVIGATECOMPLETE:
			case DISPID_NAVIGATECOMPLETE2:
				onNavigateComplete();
				break;

			default:
				//PrintDebug("DispId: %d", dispidMember);
				//return DISP_E_MEMBERNOTFOUND;
				break;
			}

			return S_OK;
		}

	private:
		static LRESULT CALLBACK wndProcWebBrowserSubclass( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR data )
		{
			if( reinterpret_cast<CWebBrowser*>( data )->webBrowserProc( hWnd, message, wParam, lParam ) )
				return FALSE;

			return DefSubclassProc( hWnd, message, wParam, lParam );
		}

	private:
		IWebBrowser2 *_pWebBrowser;
		IOleObject *_pOleObj;
		IOleInPlaceObject *_pOleInPlaceObj;

		std::function<void(HWND, UINT, WPARAM, LPARAM)> _wbProcCallback;

		CFindText *_pFindDlg;
		CFindText::CParams _findParams;
		std::wstring _textToFind;

		HWND _hWndParent;
		HWND _hWndControl;
		RECT _rctObj;
		DWORD _dwCookie;
		LONG _cRef;
	};
}
