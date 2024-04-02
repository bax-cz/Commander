#include "stdafx.h"

#include "Commander.h"
#include "BaseReader.h"

namespace Commander
{
	//
	// Sort entries using particular sorting function (alpha, extension, date, size)
	//
	void CBaseReader::sortEntries()
	{
		std::lock_guard<std::mutex> lock( _mutex );

		// skip first double-dotted directory when sorting
		size_t offset = ( !_dirEntries.empty() && PathUtils::isDirectoryDoubleDotted( _dirEntries[0]->wfd.cFileName ) ) ? 1 : 0;

		switch( _sortMode )
		{
		case ESortMode::ByName:
			std::sort( _dirEntries.begin() + offset, _dirEntries.end(), []( std::shared_ptr<EntryData> a, std::shared_ptr<EntryData> b ) {
				return SortUtils::byName( a->wfd, b->wfd );
			} );
			std::sort( _fileEntries.begin(), _fileEntries.end(), []( std::shared_ptr<EntryData> a, std::shared_ptr<EntryData> b ) {
				return SortUtils::byName( a->wfd, b->wfd );
			} );
			break;
		case ESortMode::ByExt:
			std::sort( _dirEntries.begin() + offset, _dirEntries.end(), []( std::shared_ptr<EntryData> a, std::shared_ptr<EntryData> b ) {
				return SortUtils::byName( a->wfd, b->wfd );
			} );
			std::sort( _fileEntries.begin(), _fileEntries.end(), []( std::shared_ptr<EntryData> a, std::shared_ptr<EntryData> b ) {
				return SortUtils::byExt( a->wfd, b->wfd );
			} );
			break;
		case ESortMode::ByTime:
			std::sort( _dirEntries.begin() + offset, _dirEntries.end(), []( std::shared_ptr<EntryData> a, std::shared_ptr<EntryData> b ) {
				return SortUtils::byTime( a->wfd, b->wfd );
			} );
			std::sort( _fileEntries.begin(), _fileEntries.end(), []( std::shared_ptr<EntryData> a, std::shared_ptr<EntryData> b ) {
				return SortUtils::byTime( a->wfd, b->wfd );
			} );
			break;
		case ESortMode::BySize:
			std::sort( _dirEntries.begin() + offset, _dirEntries.end(), []( std::shared_ptr<EntryData> a, std::shared_ptr<EntryData> b ) {
				return SortUtils::bySize( a->wfd, b->wfd );
			} );
			std::sort( _fileEntries.begin(), _fileEntries.end(), []( std::shared_ptr<EntryData> a, std::shared_ptr<EntryData> b ) {
				return SortUtils::bySize( a->wfd, b->wfd );
			} );
			break;
		case ESortMode::ByAttr:
			std::sort( _dirEntries.begin() + offset, _dirEntries.end(), []( std::shared_ptr<EntryData> a, std::shared_ptr<EntryData> b ) {
				return SortUtils::byAttr( a->wfd, b->wfd );
			} );
			std::sort( _fileEntries.begin(), _fileEntries.end(), []( std::shared_ptr<EntryData> a, std::shared_ptr<EntryData> b ) {
				return SortUtils::byAttr( a->wfd, b->wfd );
			} );
			break;
		}
	}

	//
	// Get general entry data pointer
	//
	std::shared_ptr<EntryData> CBaseReader::getEntry( int entryIndex )
	{
		if( entryIndex != -1 )
		{
			std::lock_guard<std::mutex> lock( _mutex );

			size_t idx = static_cast<size_t>( entryIndex ); 
			size_t filesCount = _fileEntries.size();
			size_t dirsCount = _dirEntries.size();

			if( dirsCount > 0 && idx < dirsCount )
			{
				// get directory entry's data
				return _dirEntries[idx];
			}
			else if( filesCount && ( idx - dirsCount ) < filesCount )
			{
				// get file entry's data
				return _fileEntries[idx - dirsCount];
			}
		}

		_ASSERTE( "Invalid entryIndex" );

		return nullptr;
	}

	//
	// Get entry internal index
	//
	int CBaseReader::getEntryIndex( const std::wstring& entryName )
	{
		std::lock_guard<std::mutex> lock( _mutex );

		auto pred = [&entryName]( const std::shared_ptr<EntryData>& data ) { return entryName == data->wfd.cFileName; };

		auto it = std::find_if( _dirEntries.begin(), _dirEntries.end(), pred );
		if( it != _dirEntries.end() )
		{
			return static_cast<int>( it - _dirEntries.begin() );
		}

		it = std::find_if( _fileEntries.begin(), _fileEntries.end(), pred );
		if( it != _fileEntries.end() )
		{
			return static_cast<int>( it - _fileEntries.begin() + _dirEntries.size() );
		}

		PrintDebug( "Entry %ls not found", entryName.c_str() );

		return -1;
	}
}
