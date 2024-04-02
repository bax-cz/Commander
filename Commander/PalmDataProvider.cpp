#include "stdafx.h"

#include "Commander.h"
#include "PalmDataProvider.h"
#include "FileSystemUtils.h"
#include "NetworkUtils.h"
#include "MiscUtils.h"
#include "ViewerTypes.h"

#include "../PalmDump/palmdump.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CPalmDataProvider::CPalmDataProvider()
		: _read( 0ll )
	{
	}

	CPalmDataProvider::~CPalmDataProvider()
	{
		// stop workers
		_worker.stop();
	}

	bool CPalmDataProvider::readData( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify )
	{
		_worker.init( [this] { return _readDataCore(); }, hWndNotify, UM_READERNOTIFY );
		_readToFile = false;

		// TODO

		return false; //_worker.start();
	}

	bool CPalmDataProvider::readToFile( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify )
	{
		_worker.init( [this] { return _readDataCore(); }, hWndNotify, UM_READERNOTIFY );

		_readToFile = true;
		_fileName = fileName;

		return _worker.start();
	}

	/*std::string getImgTag( const std::string& line, size_t idxBeg )
	{
		size_t idxEnd = line.find( "</img>", idxBeg );
		size_t offset = 6;

		if( idxEnd == std::string::npos )
		{
			idxEnd = line.find( "/>", idxBeg );
			offset = 2;

			if( idxEnd == std::string::npos )
			{
			//	PrintDebug("Invalid tag");
				return std::string();
			}
		}

		return line.substr( idxBeg, idxEnd - idxBeg + offset );
	}*/

	std::string replaceTag( const std::wstring& path, const std::string& tag )
	{
		std::string tagOut = tag;

		size_t idx1 = tag.find( "recindex=\"" );
		size_t idx2 = std::string::npos;

		if( idx1 != std::string::npos )
			idx2 = tag.find_first_of( '\"', idx1 + 10 );

		if( idx2 != std::string::npos )
		{
			std::string fname = tag.substr( idx1 + 10, idx2 - ( idx1 + 10 ) ) + ".";

			WIN32_FIND_DATA wfd;
			if( FsUtils::getFileInfo( path + StringUtils::convert2W( fname ) + L"*", wfd ) )
			{
				fname += StringUtils::convert2A( PathUtils::getFileNameExtension( wfd.cFileName ) );
				tagOut = tag.substr( 0, 5 ); // "<img "
				tagOut += "src=\"img/" + fname + "\" ";
				tagOut += tag.substr( 5 );
			}
		}
		else
		{
		//	PrintDebug("Invalid tag: no recindex");
		}

		return tagOut;
	}

	// Chunked version of memstr function
	const char *memstr2( const char *buf, std::streamsize len, const std::string& str, size_t *pos )
	{
		const char *p = buf;

		if( *pos != std::string::npos )
		{
			// continue matching the rest from previous call
			if( *pos < str.length() )
			{
				size_t offset = str.length() - *pos;

				if( offset <= len && !memcmp( p, &str[*pos], offset ) )
					return p;
			}

			*pos = std::string::npos;
		}

		while( ( ( p - buf ) < len ) && ( p = reinterpret_cast<const char*>( memchr( p, str[0], static_cast<size_t>( len - ( p - buf ) ) ) ) ) )
		{
			size_t remainder = (size_t)( len - ( p - buf ) );

			if( remainder < str.length() )
			{
				// partial text found
				if( !memcmp( p, &str[0], remainder ) )
				{
					*pos = remainder;
					break;
				}
			}
			else if( !memcmp( p, &str[0], str.length() ) )
			{
				// complete text found
				return p;
			}

			++p;
		}

		return nullptr;
	}

	// Process html file and return image tag when found
	const char *CPalmDataProvider::processHtml( std::ifstream& fIn, size_t off, std::string& tag, std::ofstream& fOut )
	{
		_ASSERTE( _read - off >= 0 );

		const char *p1 = nullptr, *p2 = nullptr;
		std::size_t pos = std::string::npos;

		tag.clear();

		// try to find the beginning of the "<img>" tag
		do
		{
			if( ( p1 = memstr2( _buf + off, _read - off, "<img", &pos ) ) )
			{
				if( !tag.empty() && p1 != _buf )
				{
					fOut.write( tag.c_str(), tag.length() );
					tag.clear();
				}

				fOut.write( _buf + off, ( p1 - _buf ) - off );

				break;
			}

			if( pos != std::string::npos )
			{
				fOut.write( _buf + off, ( _read - pos ) - off );
				tag.assign( _buf + ( _read - pos ), pos );
				off = 0;
			}
			else
			{
				if( !tag.empty() )
					fOut.write( tag.c_str(), tag.length() );

				fOut.write( _buf + off, _read - off );

				return nullptr;
			}

		} while( _worker.isRunning() && ( _read = fIn.read( _buf, sizeof( _buf ) ).gcount() ) );

		// try to find the end of the tag 
		do
		{
			if( ( p2 = reinterpret_cast<const char*>( memchr( p1, '>', static_cast<size_t>( _read - ( p1 - _buf ) ) ) ) ) )
			{
				tag.append( p1, ++p2 );
				break;
			}
			else
			{
				tag.append( p1, _read - ( p1 - _buf ) );
				p1 = _buf;
			}

		} while( _worker.isRunning() && ( _read = fIn.read( _buf, sizeof( _buf ) ).gcount() ) );

		return p2;
	}

	void CPalmDataProvider::addImageSources( const std::wstring& fileName )
	{
		std::wstring tempFileName = PathUtils::stripFileExtension( fileName ) + L".tmp";
		std::wstring path = PathUtils::stripFileName( fileName ) + L"\\img\\";

		_wrename( fileName.c_str(), tempFileName.c_str() );

		std::ofstream fOut( fileName, std::ios::binary );

		if( fOut.is_open() )
		{
			std::ifstream fIn( tempFileName, std::ios::binary );
			std::string tag;

			while( _worker.isRunning() && ( _read = fIn.read( _buf, sizeof( _buf ) ).gcount() ) )
			{
				const char *p = _buf;

				while( ( p = processHtml( fIn, ( p - _buf ), tag, fOut ) ) )
				{
					fOut << replaceTag( path, tag );
				}
			}
		}

		_wremove( tempFileName.c_str() );
	}

	bool CPalmDataProvider::_readDataCore()
	{
		bool isMobi = ViewerType::getType( _fileName ) == ViewerType::EViewType::FMT_MOBI;
		std::wstring outFileName = FCS::inst().getTempPath() + PathUtils::stripPath( _fileName );

		_result = PathUtils::stripFileExtension( outFileName ) + ( isMobi ? L".html" : L".txt" );

		if( isMobi )
			FsUtils::deleteDirectory( FCS::inst().getTempPath() + L"img", FCS::inst().getApp().getFindFLags() );

		// extract palm/mobi data
		int ret = dumpPalmFile( PathUtils::getExtendedPath( _fileName ).c_str(), _result.c_str() );

		switch( ret )
		{
		case 10:
			_errorMessage = L"Cannot read Palm file header.";
			break;
		case 11:
			_errorMessage = L"Cannot open input file.";
			break;
		case 12:
			_errorMessage = L"Cannot open output file.";
			break;
		default:
			// insert all images' paths taken from "recindex" attributes
			if( isMobi )
				addImageSources( _result );
			break;
		}

		return !ret;
	}
}
