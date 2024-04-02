#pragma once

/*
	CHistoryNavigator - go through history of entries
*/

#include <vector>

namespace Commander
{
	class CHistoryNavigator
	{
	public:
		using HistoryEntry = std::pair<std::wstring, std::wstring>;

		CHistoryNavigator();
		~CHistoryNavigator();

		void addEntry( const HistoryEntry& element );
		void updateCurrentEntry( const std::wstring& elementValue );

		std::vector<std::wstring> getList( bool backward = true );
		HistoryEntry getCurrentEntry();

		bool goForward( int step = 1 );
		bool goBackward( int step = 1 );

		void revert();

		bool canGoForward() const;
		bool canGoBackward() const;

	private:
		std::vector<HistoryEntry> _entries;

		int _pos;
		int _prevPos;
	};
}
