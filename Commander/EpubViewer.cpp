#include "stdafx.h"

#include "Commander.h"
#include "EpubViewer.h"
#include "IconUtils.h"
#include "MiscUtils.h"
#include "NetworkUtils.h"
#include "TextFileReader.h"
#include "TreeViewUtils.h"
#include "XmlUtils.h"

namespace Commander
{
	void CEpubViewer::onInit()
	{
		SendMessage( _hDlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>( IconUtils::getStock( SIID_DOCASSOC ) ) );
		SendMessage( _hDlg, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>( IconUtils::getStock( SIID_DOCASSOC, true ) ) );

		GetClientRect( _hDlg, &_rctWbObj );

		// create tree-view navigation pane
		_hTreeView = CreateWindowEx(
			0,
			WC_TREEVIEW,
			L"",
			WS_CHILD | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS /*| TVS_FULLROWSELECT | TVS_NONEVENHEIGHT*/ | TVS_DISABLEDRAGDROP,
			_rctWbObj.left, _rctWbObj.top, _rctWbObj.left + 200, _rctWbObj.bottom - _rctWbObj.top,
			_hDlg, NULL, FCS::inst().getFcInstance(), NULL );

		// set font
		SendMessage( _hTreeView, WM_SETFONT, reinterpret_cast<WPARAM>( FCS::inst().getApp().getGuiFont() ), TRUE );

		// set tree-view extended style
		TvUtils::setExtStyle( _hTreeView, TVS_EX_DOUBLEBUFFER );

		ShowWindow( _hTreeView, SW_HIDE );

		MiscUtils::centerOnWindow( FCS::inst().getFcWindow(), _hDlg );

		SetWindowText( _hDlg, L"Epub Viewer" );

		// create and initialize webbrowser control
		_upWebBrowser = std::make_unique<CWebBrowser>( _hDlg );

		_upWebBrowser->setWbProcCallback( [this]( HWND hw, UINT msg, WPARAM wp, LPARAM lp ) {
			return webBrowserProc( hw, msg, wp, lp );
		} );

		// get dialog menu handle
		//_hMenu = GetMenu( _hDlg );

		_worker.init( [this] { return _workerProc(); }, _hDlg, UM_STATUSNOTIFY );
	}

	bool CEpubViewer::onClose()
	{
		show( SW_HIDE ); // hide dialog

		_worker.stop();

		return true;
	}

	void CEpubViewer::onDestroy()
	{
		_upWebBrowser = nullptr;

		DestroyWindow( _hTreeView );

		DestroyIcon( reinterpret_cast<HICON>( SendMessage( _hDlg, WM_GETICON, ICON_SMALL, 0 ) ) );
		DestroyIcon( reinterpret_cast<HICON>( SendMessage( _hDlg, WM_GETICON, ICON_BIG, 0 ) ) );
	}

	void CEpubViewer::initXmlEntitiesMap()
	{
		if( _xmlEntities.empty() )
		{
			// Load DTD replacable entities list from resource
			WCHAR xmlTempStr[2048];

			LoadString( FCS::inst().getFcInstance(), IDS_XMLDTDNAME, xmlTempStr, ARRAYSIZE( xmlTempStr ) );
			auto xmlNames = StringUtils::split( xmlTempStr, L";" );

			LoadString( FCS::inst().getFcInstance(), IDS_XMLDTDVALUE, xmlTempStr, ARRAYSIZE( xmlTempStr ) );
			auto xmlValues = StringUtils::split( xmlTempStr, L";" );

			_ASSERTE( xmlNames.size() == xmlValues.size() );

			for( size_t i = 0; i < xmlNames.size(); i++ )
			{
				if( !xmlNames[i].empty() )
					_xmlEntities[xmlNames[i]] = xmlValues[i];
			}
		}
	}

	void CEpubViewer::initTreeView()
	{
		HTREEITEM hRoot = TVI_ROOT;

		if( !_epubData.ncxXmlData.empty() )
		{
			CComPtr<IXMLDOMDocument2> pXMLDoc;
			HRESULT hr = CoCreateInstance( __uuidof( DOMDocument30 ), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &pXMLDoc ) );

			if( pXMLDoc )
			{
				VARIANT_BOOL fLoaded = VARIANT_FALSE;

				// disable validation and externals
				pXMLDoc->put_validateOnParse( VARIANT_FALSE );
				pXMLDoc->put_resolveExternals( VARIANT_FALSE );

				// load DOM document from xml data
				hr = pXMLDoc->loadXML( &_epubData.ncxXmlData[0], &fLoaded );

				if( SUCCEEDED( hr ) && fLoaded == VARIANT_TRUE )
				{
					// insert root Navigation item
					hRoot = TvUtils::addItem( _hTreeView, hRoot, TVI_FIRST, TVIF_TEXT, L"Navigation" );

					// read document info
					_epubData.ncxDocTitle = XmlUtils::getElementText( pXMLDoc, L"/ncx/docTitle" );
					_epubData.ncxDocAuthor = XmlUtils::getElementText( pXMLDoc, L"/ncx/docAuthor" );

					// read navigation points
					CComPtr<IXMLDOMNodeList> pNodeListNavPoints;
					pXMLDoc->selectNodes( L"/ncx/navMap/navPoint", &pNodeListNavPoints );
					readNavPoints( pNodeListNavPoints, hRoot );

					TreeView_Expand( _hTreeView, hRoot, TVE_EXPAND );
				}
				else
					_errorMessage = XmlUtils::getParserError( pXMLDoc );
			}
		}

		if( !_epubData.opfGuide.empty() )
		{
			// insert root Guide item
			hRoot = TvUtils::addItem( _hTreeView, TVI_ROOT, hRoot, TVIF_TEXT, L"Guide" );

			// insert Guide items
			for( long i = 0; i < (long)_epubData.opfGuide.size(); i++ )
				TvUtils::addItem( _hTreeView, hRoot, TVI_LAST, TVIF_TEXT | TVIF_PARAM, &_epubData.opfGuide[i].first[0], (LPARAM)&_epubData.opfGuide[i].second[0] );
		}

		if( !_epubData.opfSpine.empty() )
		{
			// insert root Content item
			hRoot = TvUtils::addItem( _hTreeView, TVI_ROOT, hRoot, TVIF_TEXT, L"Content" );

			// insert Content items
			for( long i = 0; i < (long)_epubData.opfSpine.size(); i++ )
				TvUtils::addItem( _hTreeView, hRoot, TVI_LAST, TVIF_TEXT | TVIF_PARAM, &_epubData.opfSpine[i].first[0], (LPARAM)&_epubData.opfSpine[i].second[0] );
		}

		ShowWindow( _hTreeView, SW_SHOW );
	}

	std::wstring CEpubViewer::getUrlLink( const std::wstring& linkName )
	{
		std::wstring link = NetUtils::urlDecode( linkName );

		PathUtils::unifyDelimiters( link );

		// rename only html files (xml can be ignored)
		if( link.find( L".xml" ) == std::wstring::npos )
		{
			size_t idx;
			if( ( idx = link.find_first_of( L'#' ) ) != std::wstring::npos )
			{
				size_t extIdx = link.find_last_of( L'.', idx );

				if( extIdx != std::wstring::npos )
				{
					std::wstring tmp = link.substr( 0, extIdx ) + L".xhtml";
					link = tmp + link.substr( idx );
				}
			}
			else if( StringUtils::endsWith( link, L".htm" ) || StringUtils::endsWith( link, L".html" ) )
				link = PathUtils::stripFileExtension( link ) + L".xhtml";
		}

		return link;
	}

	void CEpubViewer::readNavPoints( IXMLDOMNodeList *pNodeList, HTREEITEM hRoot )
	{
		if( pNodeList )
		{
			long len = 0;
			pNodeList->get_length( &len );

			for( long i = 0; i < len; i++ )
			{
				CComPtr<IXMLDOMNode> pNode;
				pNodeList->get_item( i, &pNode );

				CComPtr<IXMLDOMNode> pNodeLabel;
				pNode->selectSingleNode( L"navLabel", &pNodeLabel );

				CComBSTR labelBstr;
				if( pNodeLabel && SUCCEEDED( pNodeLabel->get_text( &labelBstr ) ) )
				{
					std::wstring link;

					CComPtr<IXMLDOMNode> pNodeContent;
					if( SUCCEEDED( pNode->selectSingleNode( L"content", &pNodeContent ) ) )
					{
						link = getUrlLink( XmlUtils::getNodeAttrValue( pNodeContent, L"src" ) );
					}

					// add to list of nav points
					_epubData.ncxNavPoints.push_back( std::make_pair( labelBstr.m_str, link ) );

					// and insert into the tree-view
					auto hItem = TvUtils::addItem( _hTreeView, hRoot, TVI_LAST, TVIF_TEXT | TVIF_PARAM,
						&_epubData.ncxNavPoints.back().first[0], (LPARAM)&_epubData.ncxNavPoints.back().second[0] );

					CComPtr<IXMLDOMNodeList> pChildNodeList;
					pNode->selectNodes( L"navPoint", &pChildNodeList );

					// try to read child nav points if there are any
					readNavPoints( pChildNodeList, hItem );
				}
			}
		}
	}

	//
	// Make shortened directory name so we don't exceeed WebBrowser's MAXPATH limit
	//
	std::wstring CEpubViewer::getLocalDirectoryName()
	{
		std::wstring dirName = FCS::inst().getTempPath() + L"epub\\";

		// make a hash from the original filename
		uLong crc = crc32( 0L, Z_NULL, 0 );
		crc = crc32( crc, (Bytef*)_fileName.c_str(), (uInt)_fileName.length() * sizeof( wchar_t ) );

		dirName += PathUtils::stripPath( _fileName ).substr( 0, 4 ); // append first 4 chars
		dirName += L"_" + StringUtils::formatCrc32( crc );

		// validate epub temp directory name
		dirName = PathUtils::addDelimiter( PathUtils::getFullPath( dirName ) );

		return dirName;
	}

	bool CEpubViewer::_workerProc()
	{
		// initialize COM
		ShellUtils::CComInitializer _com( COINIT_APARTMENTTHREADED );

		initXmlEntitiesMap();

		_localDirectory = getLocalDirectoryName();

		// extract and read epub data
		return extractEpubData() ? readEpubData() : false;
	}

	bool CEpubViewer::extractEpubData()
	{
		_ASSERTE( _upArchiver != nullptr );

		// verify epub data in temp directory when it already exists
		if( FsUtils::isPathExisting( _localDirectory ) && _upArchiver->readEntries() )
		{
			bool found = true;
			for( const auto& entry : _upArchiver->getEntriesRaw() )
			{
				std::wstring entryName = entry.wfd.cFileName;

				// files must have xhtml (or at least xml) extension, otherwise IWebBrowser won't display them correctly
				if( StringUtils::endsWith( entryName, L".htm" ) || StringUtils::endsWith( entryName, L".html" ) )
					entryName = PathUtils::getFullPath( PathUtils::stripFileExtension( entryName ) ) + L".xhtml";

				if( !FsUtils::isPathExisting( _localDirectory + entryName ) )
				{
					// some file is missing, delete data and extract from archive
					FsUtils::deleteDirectory( _localDirectory, FCS::inst().getApp().getFindFLags() );
					found = false;
					break;
				}
			}

			// all files were found
			if( found )
				return true;
		}

		// extract epub data into the directory
		if( _upArchiver->extractEntries( { /*empty - extract all*/ }, _localDirectory ) )
		{
			// deal with conflicting filenames (due to case-insensitive filesystem)
			if( !_duplicateFiles.empty() )
			{
				// zip unpacker deals with unique names
			}

			return true;
		}
		else
			_errorMessage = _upArchiver->getErrorMessage();

		return false;
	}

	bool CEpubViewer::parseContainerFile( const std::wstring& fileName )
	{
		CComPtr<IXMLDOMDocument2> pXMLDoc;
		HRESULT hr = CoCreateInstance( __uuidof( DOMDocument30 ), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &pXMLDoc ) );

		if( pXMLDoc )
		{
			VARIANT_BOOL fLoaded = VARIANT_FALSE;
			CComVariant varFile = &fileName[0];

			// disable validation and externals
			pXMLDoc->put_validateOnParse( VARIANT_FALSE );
			pXMLDoc->put_resolveExternals( VARIANT_FALSE );

			hr = pXMLDoc->load( varFile, &fLoaded );
			if( SUCCEEDED( hr ) && fLoaded == VARIANT_TRUE )
			{
				// read OPF location
				CComPtr<IXMLDOMNodeList> pNodeRootList;
				pXMLDoc->selectNodes( L"/container/rootfiles/rootfile", &pNodeRootList );

				long len = 0;
				if( pNodeRootList && SUCCEEDED( pNodeRootList->get_length( &len ) ) && len )
				{
					if( len > 1 )
						PrintDebug("More than one .opf found - using the first one.");

					CComPtr<IXMLDOMNode> pNode;
					if( SUCCEEDED( pNodeRootList->get_item( 0, &pNode ) ) )
					{
						_epubData.opfFileName = XmlUtils::getNodeAttrValue( pNode, L"full-path" );
						PathUtils::unifyDelimiters( _epubData.opfFileName );

						return true;
					}
				}
			}
			else
				_errorMessage = XmlUtils::getParserError( pXMLDoc );
		}

		return false;
	}

	bool CEpubViewer::parsePackageFile( const std::wstring& fileName )
	{
		CComPtr<IXMLDOMDocument2> pXMLDoc;
		HRESULT hr = CoCreateInstance( __uuidof( DOMDocument30 ), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &pXMLDoc ) );

		if( pXMLDoc )
		{
			VARIANT_BOOL fLoaded = VARIANT_FALSE;
			CComVariant varFile = &fileName[0];

			// disable validation and externals
			pXMLDoc->put_validateOnParse( VARIANT_FALSE );
			pXMLDoc->put_resolveExternals( VARIANT_FALSE );

			hr = pXMLDoc->load( varFile, &fLoaded );
			if( SUCCEEDED( hr ) && fLoaded == VARIANT_TRUE )
			{
				// read package metadata
				_epubData.opfTitle    = XmlUtils::getElementText( pXMLDoc, L"/package/metadata/dc:title" );
				_epubData.opfCreator  = XmlUtils::getElementText( pXMLDoc, L"/package/metadata/dc:creator" );
				_epubData.opfSubject  = XmlUtils::getElementText( pXMLDoc, L"/package/metadata/dc:subject" );
				_epubData.opfLanguage = XmlUtils::getElementText( pXMLDoc, L"/package/metadata/dc:language" );
				_epubData.opfDate     = XmlUtils::getElementText( pXMLDoc, L"/package/metadata/dc:date" );

				// read NCX file location
				CComPtr<IXMLDOMNode> pNodeToc;
				pXMLDoc->selectSingleNode( L"/package/spine|/opf:package/opf:spine", &pNodeToc );
				auto tocId = XmlUtils::getNodeAttrValue( pNodeToc, L"toc" );

				CComPtr<IXMLDOMNode> pNodeNcx;
				CComBSTR query = FORMAT( L"/package/manifest/*[@id='%ls']|/opf:package/opf:manifest/*[@id='%ls']", tocId.c_str(), tocId.c_str() ).c_str();
				pXMLDoc->selectSingleNode( query, &pNodeNcx );

				_epubData.ncxFileName = XmlUtils::getNodeAttrValue( pNodeNcx, L"href" );

				// read spine
				CComPtr<IXMLDOMNodeList> pNodeSpineChildren;
				pXMLDoc->selectNodes( L"/package/spine/itemref|/opf:package/opf:spine/opf:itemref", &pNodeSpineChildren );

				long len = 0;
				pNodeSpineChildren->get_length( &len );

				for( long i = 0; i < len; i++ )
				{
					CComPtr<IXMLDOMNode> pNode;
					pNodeSpineChildren->get_item( i, &pNode );

					std::wstring id = XmlUtils::getNodeAttrValue( pNode, L"idref" );

					CComPtr<IXMLDOMNode> pNodeResult;
					CComBSTR query = FORMAT( L"/package/manifest/*[@id='%ls']|/opf:package/opf:manifest/*[@id='%ls']", id.c_str(), id.c_str() ).c_str();

					pXMLDoc->selectSingleNode( query, &pNodeResult );
					std::wstring link = getUrlLink( XmlUtils::getNodeAttrValue( pNodeResult, L"href" ) );

					_epubData.opfSpine.push_back( std::make_pair( id, link ) );
				}

				// read guide
				CComPtr<IXMLDOMNodeList> pNodeGuideChildren;
				pXMLDoc->selectNodes( L"/package/guide/reference|/opf:package/guide/reference", &pNodeGuideChildren );

				if( pNodeGuideChildren && SUCCEEDED( pNodeGuideChildren->get_length( &len ) ) )
				{
					for( long i = 0; i < len; i++ )
					{
						CComPtr<IXMLDOMNode> pNode;
						pNodeGuideChildren->get_item( i, &pNode );

						std::wstring title = XmlUtils::getNodeAttrValue( pNode, L"title" );
						std::wstring link = getUrlLink( XmlUtils::getNodeAttrValue( pNode, L"href" ) );

						_epubData.opfGuide.push_back( std::make_pair( title, link ) );
					}
				}

				return true;
			}
			else
				_errorMessage = XmlUtils::getParserError( pXMLDoc );
		}

		return false;
	}

	std::wstring getEntityName( const std::wstring& xmlData, size_t begIdx )
	{
		_ASSERTE( begIdx != std::wstring::npos );

		size_t endIdx = xmlData.find_first_of( L';', begIdx );

		_ASSERTE( endIdx != std::wstring::npos );
		_ASSERTE( endIdx - begIdx < 10 );

		return xmlData.substr( begIdx + 1, endIdx - begIdx - 1 );
	}

	// Replace named entities by its code (eg: &nbsp; -> &#160;)
	void CEpubViewer::loadNavigationFile( const std::wstring& fileName )
	{
		// load .ncx file data into string
		std::ifstream fs( PathUtils::getExtendedPath( fileName ), std::ios::binary );
		if( fs.fail() )
		{
			PrintDebug("Cannot read ncx file: %ls", fileName.c_str());
			return;
		}

		CTextFileReader reader( fs, &_worker, StringUtils::UTF8 );
		std::wstring& line = reader.getLineRef();

		while( reader.readLine() == ERROR_SUCCESS )
			_epubData.ncxXmlData += line;

		// get entities used in .ncx file
		size_t begIdx = 0ull;
		if( ( begIdx = _epubData.ncxXmlData.find( L"<!DOCTYPE" ) ) != std::wstring::npos )
		{
			size_t endIdx = _epubData.ncxXmlData.find_first_of( L'>', begIdx );
			_ASSERTE( endIdx != std::wstring::npos );

			size_t idx = endIdx;
			std::set<std::wstring> entitiesList;

			while( ( idx = _epubData.ncxXmlData.find_first_of( L'&', idx + 1 ) ) != std::wstring::npos )
				entitiesList.insert( getEntityName( _epubData.ncxXmlData, idx ) );

			for( const auto& entity : entitiesList )
			{
				if( _xmlEntities.find( entity ) != _xmlEntities.end() )
				{
					std::wstring name = L"&" + entity + L";";
					std::wstring value = L"&#" + _xmlEntities[entity] + L";";

					StringUtils::replaceAll( _epubData.ncxXmlData, name.c_str(), value.c_str() );
				}
				else
				{
					PrintDebug("not found: %ls", entity.c_str());
				}
			}

			// remove DTD section from xml
			_epubData.ncxXmlData.replace( begIdx, endIdx - begIdx + 1, L"" );
		}
	}

	void CEpubViewer::applyRenamedFiles()
	{
		for( const auto& entry : _epubData.opfSpine )
		{
			std::wstring outText;
			{
				std::ifstream fs( PathUtils::getExtendedPath( PathUtils::combinePath( _localDirectory, entry.second ) ), std::ios::binary );
				if( fs.good() )
				{
					CTextFileReader reader( fs, &_worker, StringUtils::UTF8 );
					std::wstring& line = reader.getLineRef();

					while( reader.readLine() == ERROR_SUCCESS )
					{
						// replace links to images first
						for( const auto& dupf : _duplicateFiles )
						{
							// TODO: check for local vs internet links (e.g. not beginning with 'http')
							StringUtils::replaceAll( line, dupf.first, dupf.second );
						}

						// replace links to xhtml content
						for( const auto& link : _renamedFiles )
						{
							StringUtils::replaceAll( line, link.first, link.second );
						}

						outText += line + L"\r\n";
					}
				}
			}

			if( !outText.empty() )
				FsUtils::saveFileUtf8( PathUtils::combinePath( _localDirectory, entry.second ), &outText[0], outText.length() );
		}
	}

	bool CEpubViewer::readEpubData()
	{
		const std::wstring rootFileName = L"META-INF\\container.xml";

		// check extracted path
		if( !FsUtils::isPathExisting( _localDirectory + rootFileName ) )
		{
			_errorMessage = L"File '" + rootFileName + L"' does not exist.";
			return false;
		}

		// parse 'container.xml' file
		if( parseContainerFile( _localDirectory + rootFileName ) )
		{
			if( !_epubData.opfFileName.empty() )
			{
				// parse OPF file
				if( parsePackageFile( _localDirectory + _epubData.opfFileName ) )
				{
					auto localRoot = PathUtils::stripFileName( _epubData.opfFileName );

					// update root directory once and for all
					_localDirectory += localRoot.empty() ? L"" : PathUtils::addDelimiter( localRoot );

					// load NCX file data
					loadNavigationFile( _localDirectory + _epubData.ncxFileName );

					// replace link names in the source xhtml files
					if( !_duplicateFiles.empty() || !_renamedFiles.empty() )
						applyRenamedFiles();

					return true;
				}
			}
			else
				_errorMessage = L"Invalid OPF file.";
		}

		return false;
	}

	bool CEpubViewer::viewFile( const std::wstring& dirName, const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel )
	{
		_fileName = PathUtils::addDelimiter( dirName ) + fileName;

		if( spPanel )
		{
			_spPanel = spPanel;
			return _spPanel->getDataManager()->getDataProvider()->readToFile( _fileName, spPanel, _hDlg );
		}

		// view epub file
		viewEpub( _fileName );

		return true;
	}

	void CEpubViewer::viewHtml( const std::wstring& fileName )
	{
		_fileName = fileName;
		_scrollTicks = GetTickCount64();
		_canScrollPage = false;

		std::wostringstream sstrTitle;
		sstrTitle << fileName << L" - Viewer";
		
		SetWindowText( _hDlg, sstrTitle.str().c_str() );

		_upWebBrowser->navigate( fileName );
	}

	void CEpubViewer::viewEpub( const std::wstring& fileName )
	{
		std::wostringstream sstrTitle;
		sstrTitle << L"Loading " << fileName << L" - Viewer";
		
		SetWindowText( _hDlg, sstrTitle.str().c_str() );

		SetCursor( LoadCursor( NULL, IDC_WAIT ) );

		// intialize extraction worker
		_upArchiver = std::make_unique<CArchZip>();
		_upArchiver->init( PathUtils::getExtendedPath( fileName ), nullptr, nullptr, &_worker, CArchiver::EExtractAction::Rename );

		// extract epub data into temp directory
		_worker.start();

		_rctWbObj.left += 202;
		_upWebBrowser->updateRect( &_rctWbObj );
	}

	void CEpubViewer::viewNext()
	{
		HTREEITEM hRootItem, hItem;
		long idx = -1;

		if( idx == -1 )
		{
			for( auto it = _epubData.opfSpine.begin(); it != _epubData.opfSpine.end(); ++it )
			{
				if( StringUtils::endsWith( _fileName, it->second ) )
				{
					idx = (long)( it - _epubData.opfSpine.begin() );
					hRootItem = TvUtils::findRootItem( _hTreeView, L"Content" );
					if( idx != -1 && idx + 1 < (long)_epubData.opfSpine.size() )
					{
						hItem = TvUtils::findChildItem( _hTreeView, hRootItem, &_epubData.opfSpine[idx + 1].first[0] );
						TreeView_SelectItem( _hTreeView, hItem );
					}
					break;
				}
			}
		}
	}

	void CEpubViewer::viewPrevious()
	{
		HTREEITEM hRootItem, hItem;
		long idx = -1;

		if( idx == -1 )
		{
			for( auto it = _epubData.opfSpine.begin(); it != _epubData.opfSpine.end(); ++it )
			{
				if( StringUtils::endsWith( _fileName, it->second ) )
				{
					idx = (long)( it - _epubData.opfSpine.begin() );
					hRootItem = TvUtils::findRootItem( _hTreeView, L"Content" );
					if( idx != -1 && idx - 1 >= 0 )
					{
						hItem = TvUtils::findChildItem( _hTreeView, hRootItem, &_epubData.opfSpine[idx - 1].first[0] );
						TreeView_SelectItem( _hTreeView, hItem );
					}
					break;
				}
			}
		}
	}

	void CEpubViewer::handleKeyboard( WPARAM key )
	{
		switch( key )
		{
		case VK_SPACE:
			break;
		case VK_BACK:
			break;
		//case VK_HOME:
		//	break;
		//case VK_END:
		//	break;
		//case VK_PRIOR:
		//	break;
		//case VK_NEXT:
		//	break;
		default:
			break;
		}
	}

	void CEpubViewer::webBrowserProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
	{
		if( message == WM_MOUSEWHEEL )
		{
			// cooldown period 500 ms
			_canScrollPage = ( _scrollTicks + 500 < GetTickCount64() );
			_scrollTicks = GetTickCount64();
		}
	}

	INT_PTR CALLBACK CEpubViewer::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_COMMAND:
			PrintDebug("msg: 0x%04X, wpar: 0x%04X, lpar: 0x%04X", message, wParam, lParam);
			//handleMenuCommands( LOWORD( wParam ) );
			break;

		case WM_KEYDOWN:
			switch( wParam )
			{
			case VK_INSERT:
				//if( _spPanel )
				//	_spPanel->markEntry( PathUtils::stripPath( _fileName ) );
				break;
			default:
				handleKeyboard( wParam );
				break;
			}
			break;

		case WM_NOTIFY:
			switch( reinterpret_cast<LPNMHDR>( lParam )->code )
			{
			//case NM_CUSTOMDRAW:
			//	return onCustomDraw( reinterpret_cast<LPNMLVCUSTOMDRAW>( lParam ) );

			case TVN_SELCHANGED:
			{
				TV_ITEM item = TvUtils::getSelectedItem( _hTreeView );

				if( item.lParam )
					viewHtml( _localDirectory + (LPWSTR)item.lParam );
				break;
			}
			}
			break;

		case WM_MOUSEWHEEL: // fired only when the top (or bottom) of the page is hit
			if( _canScrollPage && _scrollTicks + 10 > GetTickCount64() ) // this prevents partial scroll when near the edge
			{
				// TODO: only when mouse hovers over the webbrowser's window (pre win10)
				if( GET_WHEEL_DELTA_WPARAM( wParam ) < 0 )
					viewNext();
				else
					viewPrevious();
			}
			break;

		case UM_READERNOTIFY:
			if( wParam == 1 )
			{
				_fileName = _spPanel->getDataManager()->getDataProvider()->getResult();
				viewEpub( _fileName );
			}
			else
			{
				SetCursor( NULL );
				std::wstringstream sstr;
				sstr << L"Cannot open \"" << _fileName << L"\" file.\r\n" << _spPanel->getDataManager()->getDataProvider()->getErrorMessage();
				MessageBox( _hDlg, sstr.str().c_str(), L"View Html", MB_OK | MB_ICONEXCLAMATION );
				close();
			}
			break;

		case UM_STATUSNOTIFY:
			if( wParam )
			{
				if( wParam == FC_ARCHDONEOK )
				{
					initTreeView();
					SetCursor( NULL );
					viewHtml( _localDirectory + _epubData.opfSpine[0].second );
				}
				else if( wParam == FC_ARCHPROCESSINGPATH ) // rename html files to xhtml
				{
					WCHAR *fileName = reinterpret_cast<WCHAR*>( lParam );
					std::wstring tmpName = fileName;
					_processingPath = PathUtils::addDelimiter( _localDirectory + PathUtils::stripFileName( tmpName ) );
					if( StringUtils::endsWith( tmpName, L".htm" ) || StringUtils::endsWith( tmpName, L".html" ) )
					{
						std::wstring newFileName = PathUtils::stripFileExtension( tmpName ) + L".xhtml";
						_renamedFiles.insert( std::make_pair( PathUtils::stripPath( tmpName ), PathUtils::stripPath( newFileName ) ) );
						wcsncpy( fileName, newFileName.c_str(), MAX_PATH );
					}
				}
				else if( wParam == FC_ARCHNEEDNEWNAME ) // rename conflicting filename
				{
					WCHAR *fileName = reinterpret_cast<WCHAR*>( lParam );
					std::wstring newFileName = FsUtils::getNextFileName( _processingPath, fileName );
					_duplicateFiles.insert( std::make_pair( fileName, newFileName ) );
					wcsncpy( fileName, newFileName.c_str(), MAX_PATH );
				}
			}
			else
			{
				SetCursor( NULL );
				std::wstringstream sstr;
				sstr << L"Cannot read \"" << _localDirectory << L"\" content.\r\n" << _errorMessage;
				MessageBox( _hDlg, sstr.str().c_str(), L"View Epub", MB_OK | MB_ICONEXCLAMATION );
				close();
			}
			break;

		case WM_DROPFILES:
			//handleDropFiles( (HDROP)wParam );
			break;

		case WM_SIZE:
			if( _upWebBrowser )
			{
				_rctWbObj.right = LOWORD( lParam );
				_rctWbObj.bottom = HIWORD( lParam );

				if( !_localDirectory.empty() )
				{
					// we are reading epub - update tree-view
					_rctWbObj.left = 202;
					SetWindowPos( _hTreeView, NULL, 0, 0, _rctWbObj.left - 2, _rctWbObj.bottom, SWP_NOMOVE | SWP_NOZORDER );
				}

				_upWebBrowser->updateRect( &_rctWbObj );
			}
			break;
		}

		return (INT_PTR)false;
	}
}
