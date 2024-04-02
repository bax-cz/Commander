#pragma once

#include "TarFileBase.h"

#include "7z.h"
#include "7zCrc.h"
#include "7zFile.h"
#include "7zAlloc.h"
#include "Xz.h"
#include "XzCrc64.h"
#include "Alloc.h"

namespace TarLib
{
	class CTarFileXzip : public CTarFileBase
	{
	public:
		CTarFileXzip();
		~CTarFileXzip();

		virtual bool open( const std::wstring& fileName ) override;
		virtual bool isOpen() override;
		virtual void seek( long long pos, int direction ) override;
		virtual long long tell() override;
		virtual long long read( char *buffer, size_t bytesToRead ) override;
		virtual void close() override;

	private:
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

		bool _open;
		bool _finished;
	};
}
