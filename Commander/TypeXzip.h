#pragma once

#include "Archiver.h"

#include "7z.h"
#include "7zCrc.h"
#include "7zFile.h"
#include "7zAlloc.h"
#include "Xz.h"
#include "XzCrc64.h"
#include "Alloc.h"

namespace Commander
{
	class CArchXzip : public CArchiver
	{
	public:
		CArchXzip();
		~CArchXzip();

		virtual bool readEntries() override;
		virtual bool extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir ) override;

	private:
		void getEntryInfo( LONGLONG fileSize, WIN32_FIND_DATA *wfd );
		size_t decodeDataChunk();

	private:
		CXzUnpacker _xzUnpacker;
		CFileInStream _archiveStream;
		ISzAlloc _allocImp;
		SRes _result;

		Byte _inputBuffer[1 << 15];
		Byte _outputBuffer[1 << 21];

		size_t _inPos;
		size_t _sizeIn;
		size_t _sizeInTotal;

		ECoderStatus _status;

		bool _finished;
	};
}
