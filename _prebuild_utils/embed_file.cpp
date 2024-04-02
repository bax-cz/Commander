#include <algorithm>
#include <fstream>
#include <iomanip>
#include <string>

#include "../Zlib/zlib.h"
//#include "../Commander/Resources/fb2_xsl.h" /* for testing */

#define BYTES_PER_LINE  40
#define PUT_HEX_BYTE( x ) "\\x" << std::setw( 2 ) << std::hex << std::setfill('0') << std::uppercase << ( x & 0xFF )

void makeFileName( std::string& fname, std::string& outpath, std::string& outname )
{
	// normalize path delimiters
	std::replace( fname.begin(), fname.end(), '/', '\\' );

	size_t off = fname.find_last_of( '\\' );
	outpath = ( off != std::string::npos ? fname.substr( 0, off + 1 ) : "" );
	outname = ( off != std::string::npos ? fname.substr( off + 1 ) : fname );

	// replace the dots and spaces with underscore character
	std::replace( outname.begin(), outname.end(), '.', '_' );
	std::replace( outname.begin(), outname.end(), ' ', '_' );

	// check whether the outname does start with a number
	if( isdigit( outname[0] ) )
		outname = "_" + outname;
}

// TODO: this won't work on unicode paths
int make_include_file( std::string fname, Bytef *buf, uLongf buf_len, size_t fsize )
{
	_ASSERTE( !fname.empty() );

	std::string outpath, outname;

	// make a new name by stripping path and replacing dots and spaces with underscore character
	makeFileName( fname, outpath, outname );

	// open the include file for writing
	std::ofstream fs( outpath + outname + ".h" );

	if( fs.is_open() )
	{
		fs << "/*" << std::endl << "\tGenerated from: \"" << fname << "\"" << std::endl << "*/" << std::endl << std::endl;
		fs << "const long " << outname << "_length = " << fsize << "; /* uncompressed data length */" << std::endl;
		fs << "const char " << outname << "_data[] = \\";

		for( uLongf i = 0; i < buf_len; i++ )
		{
			if( ( i % BYTES_PER_LINE ) == 0 )
				fs << ( i != 0 ? "\"" : "" ) << std::endl << "\t\"";

			fs << PUT_HEX_BYTE( buf[i] );
		}

		fs << "\";" << std::endl;

		return 0;
	}

	return 1;
}

int main( int argc, char *argv[] )
{
	if( argc > 1 )
	{
		std::ifstream fs( argv[1], std::ios::binary );

		if( fs.is_open() )
		{
			// get file size
			fs.seekg( 0, std::ios::end );
			size_t fsize = fs.tellg();
			fs.seekg( 0, std::ios::beg );

			// read the data
			std::string data;
			data.resize( fsize );
			fs.read( &data[0], fsize );

			// allocate memory buffer
			uLongf buf_len = compressBound( static_cast<uLong>( data.length() ) );
			Bytef *buf = new Bytef[buf_len];

			// compress the data
			int ret = compress( buf, &buf_len, reinterpret_cast<Bytef*>( &data[0] ), static_cast<uLongf>( data.length() ) );

			// create the include file from the compressed data
			if( ret == Z_OK )
			{
				ret = make_include_file( argv[1], buf, buf_len, fsize );

				// test decompression
				/*Bytef *buf2 = new Bytef[data.length()];
				uLongf buf2_len = static_cast<uLongf>( data.length() );
				ret = uncompress( buf2, &buf2_len, reinterpret_cast<const Bytef*>( fb2_xsl_data ), sizeof( fb2_xsl_data ) );
				_ASSERTE( memcmp( &data[0], buf2, buf2_len ) == 0 );
				delete[] buf2;*/
			}

			delete[] buf;

			return ret;
		}
	}

	return 1;
}
