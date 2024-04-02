#pragma once

/*
	Edit control utils - helper functions to make life easier
*/

#include "StringUtils.h"

namespace Commander
{
	class CEditControlHelper
	{
	public:
		// ctor
		inline CEditControlHelper( HWND hEdit )
			: _hEdit( hEdit )
			, _hEditData( nullptr )
			, _buffer( nullptr )
			, _bufferLength( 0ull )
			, _success( false )
		{
			// get length of text in an edit control
			_bufferLength = static_cast<ULONGLONG>( SendMessage( _hEdit, WM_GETTEXTLENGTH, 0, 0 ) );

			// get handle to the edit control's data buffer
			_hEditData = reinterpret_cast<HLOCAL>( SendMessage( _hEdit, EM_GETHANDLE, 0, 0 ) );

			if( _hEditData )
			{
				// get data buffer
				_buffer = reinterpret_cast<wchar_t*>( LocalLock( _hEditData ) );
				_success = ( _buffer != nullptr );
			}
		}

		// dtor
		inline ~CEditControlHelper()
		{
			if( _success )
				LocalUnlock( _hEditData );

			_buffer = nullptr;
			_hEditData = nullptr;
		}

	public:
		inline wchar_t *getText() { return _buffer; }              // get text data buffer pointer
		inline ULONGLONG getTextLength() { return _bufferLength; } // get text data buffer length

		// Get substring at the offset from text
		inline std::wstring getSubstr( size_t offset, size_t length )
		{
			std::wstring strOut;

			if( _success && offset + length <= _bufferLength )
			{
				strOut.assign( _buffer + offset, length );
			}

			return strOut;
		}

		// Get currently selected text
		inline std::wstring getSelection( size_t max_len = std::wstring::npos )
		{
			if( _success )
			{
				size_t idxBeg = 0, idxEnd = 0;
				SendMessage( _hEdit, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );

				if( idxBeg != idxEnd )
					return getSubstr( idxBeg, min( idxEnd - idxBeg, max_len ) );
			}

			return L"";
		}

		// Swap data handle - free the previous one first
		inline bool swapDataHandle( HLOCAL hData )
		{
			if( _success && hData )
			{
				DWORD errorId = NO_ERROR;

				if( LocalUnlock( _hEditData ) == FALSE )
					errorId = GetLastError();

				// free edit control's internal memory buffer
				_hEditData = LocalFree( _hEditData );

				// set the edit control's new memory handle
				SendMessage( _hEdit, EM_SETHANDLE, (WPARAM)hData, 0 );
				_hEditData = hData;

				if( _hEditData )
				{
					// get data buffer
					_buffer = reinterpret_cast<wchar_t*>( LocalLock( _hEditData ) );
					_success = ( _buffer != nullptr );

					return _success;
				}
			}

			return false;
		}

		// Try to select word at the caret's position
		inline bool selectCurrentWord()
		{
			if( _success )
			{
				LONG_PTR idxBeg = 0, idxEnd = 0;
				SendMessage( _hEdit, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );

				if( StringUtils::isAlphaNum( _buffer[idxBeg] ) )
				{
					// find word boundaries
					while( idxBeg > 0 && StringUtils::isAlphaNum( _buffer[idxBeg - 1] ) )
						--idxBeg;

					while( idxEnd + 1 <= (LONG_PTR)_bufferLength && StringUtils::isAlphaNum( _buffer[idxEnd] ) )
						++idxEnd;

					SendMessage( _hEdit, EM_SETSEL, static_cast<WPARAM>( idxBeg ), static_cast<LPARAM>( idxEnd ) );

					return true;
				}
			}

			return false;
		}

		// Try to select entire line at the caret's position
		inline bool selectCurrentLine()
		{
			if( _success )
			{
				LONG_PTR idxBeg = 0, idxEnd = 0;
				SendMessage( _hEdit, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );

				// look for CRLF characters to get the current line's boundaries
				while( idxBeg > 0 && !( _buffer[idxBeg - 1] == L'\n' && idxBeg > 1 && _buffer[idxBeg - 2] == L'\r' ) )
					--idxBeg;

				while( idxEnd + 1 <= (LONG_PTR)_bufferLength && !( _buffer[idxEnd] == L'\r' && idxEnd + 2 <= (LONG_PTR)_bufferLength && _buffer[idxEnd + 1] == L'\n' ) )
					++idxEnd;

				SendMessage( _hEdit, EM_SETSEL, static_cast<WPARAM>( idxBeg ), static_cast<LPARAM>( idxEnd ) );

				return true;
			}

			return false;
		}

	private:
		HWND   _hEdit;
		HLOCAL _hEditData;

		wchar_t  *_buffer;
		ULONGLONG _bufferLength;

		bool _success;

	private:
		CEditControlHelper( CEditControlHelper const& ) = delete;
		void operator=( CEditControlHelper const& ) = delete;
	};

	/*
		Edit control helper utilities
	*/
	struct EdUtils
	{
		//
		// Get currently selected text in an edit control
		//
		static std::wstring getSelection( HWND hEdit )
		{
			CEditControlHelper edh( hEdit );

			return edh.getSelection( 1024 );
		}

		// do not instantiate this class/struct
		EdUtils() = delete;
		EdUtils( EdUtils const& ) = delete;
		void operator=( EdUtils const& ) = delete;
	};
}
