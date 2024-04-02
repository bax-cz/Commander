#pragma once

/*
	XML utils - helper functions for dealing with IXMLDOMDocument
*/

#include <MsXml2.h>

namespace Commander
{
	struct XmlUtils
	{
		//
		// Get parser error reason as text
		//
		static std::wstring getParserError( IXMLDOMDocument2 *pXMLDoc )
		{
			IXMLDOMParseError *pXMLErr = nullptr;
			pXMLDoc->get_parseError( &pXMLErr );

			CComBSTR bstrErr;
			pXMLErr->get_reason( &bstrErr );

			return bstrErr ? bstrErr.m_str : std::wstring();
		}

		//
		// Get XML document prefix
		//
		static std::wstring getPrefix( IXMLDOMDocument2 *pXMLDoc )
		{
			CComBSTR prefix;
			if( pXMLDoc && SUCCEEDED( pXMLDoc->get_prefix( &prefix ) ) && prefix )
			{
				return prefix.m_str;
			}

			return std::wstring();
		}

		//
		// Get list of namespaces in XML document
		//
		static std::vector<std::wstring> getNamespaces( IXMLDOMDocument2 *pXMLDoc )
		{
			std::vector<std::wstring> outNss;

			CComPtr<IXMLDOMSchemaCollection> nss;
			if( pXMLDoc && SUCCEEDED( pXMLDoc->get_namespaces( &nss ) ) && nss )
			{
				long length = 0;
				if( SUCCEEDED( nss->get_length( &length ) ) )
				{
					for( long i = 0; i < length; i++ )
					{
						CComBSTR ns;
						if( SUCCEEDED( nss->get_namespaceURI( i, &ns ) ) && ns )
						{
							outNss.push_back( ns.m_str );
						}
					}
				}
			}

			return outNss;
		}

		// Read element internal text 
		//
		static std::wstring getElementText( IXMLDOMDocument2 *pXMLDoc, const BSTR tagName )
		{
			std::wstring outText;

			CComPtr<IXMLDOMNode> pNodeResult;
			if( pXMLDoc && SUCCEEDED( pXMLDoc->selectSingleNode( tagName, &pNodeResult ) ) )
			{
				CComBSTR text;
				if( pNodeResult && SUCCEEDED( pNodeResult->get_text( &text ) ) )
				{
					outText = text;
				}
			}

			return outText;
		}

		//
		// Read node's attribute value as string
		//
		static std::wstring getNodeAttrValue( IXMLDOMNode *pNode, const BSTR attrName )
		{
			std::wstring outValue;

			CComPtr<IXMLDOMNamedNodeMap> pAttrs;
			if( pNode && SUCCEEDED( pNode->get_attributes( &pAttrs ) ) )
			{
				CComPtr<IXMLDOMNode> pNamedItem;
				if( pAttrs && SUCCEEDED( pAttrs->getNamedItem( attrName, &pNamedItem ) ) )
				{
					CComVariant value;
					if( pNamedItem && SUCCEEDED( pNamedItem->get_nodeValue( &value ) ) )
					{
						outValue = value.bstrVal;
					}
				}
			}

			return outValue;
		}

		//
		// Set node's attribute value as string
		//
		static bool setNodeAttrValue( IXMLDOMNode *pNode, const BSTR attrName, const BSTR newVal )
		{
			std::wstring outValue;

			CComPtr<IXMLDOMNamedNodeMap> pAttrs;
			if( pNode && SUCCEEDED( pNode->get_attributes( &pAttrs ) ) )
			{
				CComPtr<IXMLDOMNode> pNamedItem;
				if( pAttrs && SUCCEEDED( pAttrs->getNamedItem( attrName, &pNamedItem ) ) )
				{
					CComVariant value( newVal );
					if( pNamedItem && SUCCEEDED( pNamedItem->put_nodeValue( value ) ) )
					{
						return true;
					}
				}
			}

			return false;
		}


		// do not instantiate this class/struct
		XmlUtils() = delete;
		XmlUtils( XmlUtils const& ) = delete;
		void operator=( XmlUtils const& ) = delete;
	};
}
