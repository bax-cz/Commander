#pragma once

/*
	Data types definitions
*/

namespace Commander
{
	//
	// Disk entry data
	//
	struct EntryData
	{
		WIN32_FIND_DATA wfd;
		std::wstring size;
		std::wstring date;
		std::wstring time;
		std::wstring attr;
		std::wstring link;
		int imgSystem;
		int imgOverlay;
		DWORD hardLinkCnt;
	};

	//
	// Entry type file, directory
	//
	enum class EEntryType {
		NotFound = -1,
		Directory = 0,
		File,
		Reparse
	};

	//
	// List-view's view mode
	//
	enum class EViewMode {
		Brief,
		Detailed
	};

	//
	// Data manager's reading mode
	//
	enum class EReadMode {
		Disk,
		Archive,
		Network,
		Putty,
		Ftp,
		Reged
	};

	//
	// Reader status
	//
	enum class EReaderStatus {
		Invalid = -1,
		DataError = 0,
		DataOk,
		ReadingData
	};

	//
	// Sorting entries mode
	//
	enum class ESortMode {
		ByName,
		ByExt,
		ByTime,
		BySize,
		ByAttr
	};
}
