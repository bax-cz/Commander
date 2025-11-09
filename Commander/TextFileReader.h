#pragma once

/*
	File commander's Unicode text from file reader
*/

#include "StringUtils.h"

#include <iostream>
#include <fstream>

namespace Commander
{
	class CTextFileReader
	{
	public:
		enum EOptions { none, skipAnalyze, forceAnsi };

	public:
		CTextFileReader( std::istream& inStream,
			CBackgroundWorker *pWorker = nullptr,
			StringUtils::EUtfBom bom = StringUtils::NOBOM,
			UINT codePage = CP_ACP,
			EOptions = none );

		~CTextFileReader();

	public:
		DWORD readLine();
		inline std::wstring& getLineRef() { return _lineOut; } // reference to the converted unicode line
		inline std::string& getLineRefA() { return _lineIn; }  // reference to the original ANSI line
		inline StringUtils::EUtfBom getEncoding() const { return _useEncoding; }
		inline StringUtils::EUtfBom getByteOrderMarker() const { return _fileBom; }
		inline StringUtils::EEol getEol() const { return _fileEol; }
		inline std::streamoff getFileSize() const { return _fileSize; }
		inline bool isEolEnding() const { return _isEolEnding; } // EOL is present at the end of the line
		inline bool isText() const { return _isFileText; }

	private:
		template<typename T>
		DWORD readLineUnicode( T eol, UINT encoding, bool removeNull = false );
		DWORD readLineAnsi();

	private:
		std::streamsize readDataChunk();
		bool isDataChunkUtf8( bool& hasAnsiChar );
		void analyzeFileData();
		StringUtils::EUtfBom analyzeCrlf();
		StringUtils::EEol guessEol( int lfCnt, int crCnt, std::streamsize len );

	private:
		CBackgroundWorker *_pWorker;

		std::istream&  _inStream;
		std::streamoff _fileSize;

		StringUtils::EUtfBom _fileBom;
		StringUtils::EEol    _fileEol;
		StringUtils::EUtfBom _useEncoding;
		UINT                 _useCodePage;
		EOptions             _options;

		int _fileBomLen;

		bool _isFileText;
		bool _isFileUtf8;
		bool _isNullChar;
		bool _isEolEnding;

		char _buffer[0x30000]; // buffer size should be a multiple of 4 (size of char32_t)
		char _eolChar;

		std::streamoff _offset;
		std::streamsize _bytesRead;
		std::string _lineIn;
		std::wstring _lineOut;
	};
}
