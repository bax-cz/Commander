#pragma once

/*
	Supported file types for viewers - text/binary, image, html, djvu, epub..
*/

#include <memory>

#include "ImageTypes.h"
#include "StringUtils.h"

#include "DjvuViewer.h"
#include "FileViewer.h"
#include "EpubViewer.h"
#include "HtmlViewer.h"
#include "ImageViewer.h"

namespace Commander
{
	struct ViewerType
	{
		enum class EViewType {
			FMT_TEXT,
			FMT_BINARY,
			FMT_IMAGE,
			FMT_HTML,
			FMT_EPUB,
			FMT_DJVU,
			FMT_PALM,
			FMT_MOBI,
			FMT_FCBK2,
			FMT_MARKD,
			FMT_NFO
		};

		//
		// Get viewer type from file name
		//
		static EViewType getType( const std::wstring& fileName )
		{
			if( StringUtils::endsWith( fileName, L".djvu" ) )
				return EViewType::FMT_DJVU;
			else if( StringUtils::endsWith( fileName, L".epub" ) )
				return EViewType::FMT_EPUB;
			else if( StringUtils::endsWith( fileName, L".htm" ) || StringUtils::endsWith( fileName, L".html" ) || StringUtils::endsWith( fileName, L".xhtml" ) )
				return EViewType::FMT_HTML;
			else if( StringUtils::endsWith( fileName, L".txt" ) )
				return EViewType::FMT_TEXT;
			else if( StringUtils::endsWith( fileName, L".pdb" ) )
				return EViewType::FMT_PALM;
			else if( StringUtils::endsWith( fileName, L".mobi" ) || StringUtils::endsWith( fileName, L".azw" ) || StringUtils::endsWith( fileName, L".azw3" ) )
				return EViewType::FMT_MOBI;
			else if( StringUtils::endsWith( fileName, L".fb2" ) )
				return EViewType::FMT_FCBK2;
			else if( StringUtils::endsWith( fileName, L".md" ) )
				return EViewType::FMT_MARKD;
			else if( StringUtils::endsWith( fileName, L".nfo" ) )
				return EViewType::FMT_NFO;
			else if( ImageType::isKnownType( fileName ) )
				return EViewType::FMT_IMAGE;

			return EViewType::FMT_BINARY;
		}

		//
		// Instantiate particular viewer and view file content
		//
		static CBaseViewer *createViewer( const std::wstring& fileName, bool viewHex )
		{
			LPARAM param = MAKELPARAM( viewHex, StringUtils::NOBOM );

			if( !viewHex )
			{
				switch( getType( fileName ) )
				{
				case EViewType::FMT_DJVU:
					return CBaseDialog::createModeless<CDjvuViewer>();
				case EViewType::FMT_EPUB:
					return CBaseDialog::createModeless<CEpubViewer>();
				case EViewType::FMT_HTML:
				case EViewType::FMT_MOBI:
				case EViewType::FMT_FCBK2:
				case EViewType::FMT_MARKD:
					return CBaseDialog::createModeless<CHtmlViewer>();
				case EViewType::FMT_IMAGE:
					return CBaseDialog::createModeless<CImageViewer>();
				case EViewType::FMT_NFO:
					param = viewHex ? param : MAKELPARAM( 437, StringUtils::NOBOM ); /* codepage IBM437 */
				case EViewType::FMT_PALM:
				case EViewType::FMT_TEXT:
				case EViewType::FMT_BINARY:
				default:
					break;
				}
			}

			return CBaseDialog::createModeless<CFileViewer>( nullptr, param );
		}

		// do not instantiate this class/struct
		ViewerType() = delete;
		ViewerType( ViewerType const& ) = delete;
		void operator=( ViewerType const& ) = delete;
	};
}
