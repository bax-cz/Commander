#include "stdafx.h"

#include "HistoryNavigator.h"

namespace Commander
{
	CHistoryNavigator::CHistoryNavigator()
		: _pos( 0 )
		, _prevPos( 0 )
	{
	}

	CHistoryNavigator::~CHistoryNavigator()
	{
	}

	void CHistoryNavigator::addEntry( const HistoryEntry& entry )
	{
		if( _pos + 1 < static_cast<int>( _entries.size() ) )
			_entries.resize( _pos + 1 );

		_entries.push_back( entry );
		_pos = static_cast<int>( _entries.size() ) - 1;
	}

	void CHistoryNavigator::updateCurrentEntry( const std::wstring& entryValue )
	{
		if( _pos >= 0 && _pos < static_cast<int>( _entries.size() ) )
			_entries[_pos].second = entryValue;
	}

	std::vector<std::wstring> CHistoryNavigator::getList( bool backward )
	{
		std::vector<std::wstring> list;

		if( backward )
		{
			for( int i = _pos - 1; i >= 0; --i )
				list.push_back( _entries[i].first );
		}
		else
		{
			for( int i = _pos + 1; i < static_cast<int>( _entries.size() ); ++i )
				list.push_back( _entries[i].first );
		}

		return list;
	}

	CHistoryNavigator::HistoryEntry CHistoryNavigator::getCurrentEntry()
	{
		_ASSERTE( static_cast<int>( _entries.size() ) > _pos );
		return _entries[_pos];
	}

	bool CHistoryNavigator::goForward( int step )
	{
		if( canGoForward() )
		{
			_prevPos = _pos;
			_pos += step;
			return true;
		}

		return false;
	}

	bool CHistoryNavigator::goBackward( int step )
	{
		if( canGoBackward() )
		{
			_prevPos = _pos;
			_pos -= step;
			return true;
		}

		return false;
	}

	void CHistoryNavigator::revert()
	{
		_pos = _prevPos;
	}

	bool CHistoryNavigator::canGoForward() const
	{
		return ( _pos + 1 < static_cast<int>( _entries.size() ) );
	}

	bool CHistoryNavigator::canGoBackward() const
	{
		return ( _pos > 0 );
	}
}
