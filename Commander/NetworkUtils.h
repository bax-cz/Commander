#pragma once

/*
	Network utils - helper functions for networking
*/

namespace Commander
{
	struct NetUtils
	{
		//
		// Get the name of the current user for the process
		//
		static std::wstring getCurrentUserName()
		{
			WCHAR userName[MAX_PATH];
			DWORD userNameLen = MAX_PATH;

			DWORD ret = WNetGetUser( NULL, userName, &userNameLen );

			return userName;
		}

		//
		// Encode URL string
		//
		static std::wstring urlEncode( const std::wstring& url )
		{
			WCHAR urlOut[INTERNET_MAX_URL_LENGTH];
			DWORD urlOutLen = INTERNET_MAX_URL_LENGTH;

			if( FAILED( UrlEscapeW( url.c_str(), urlOut, &urlOutLen, URL_ESCAPE_PERCENT ) ) )
			{
			//	PrintDebug("Error encoding URL: %ls", url.c_str());
				return url;
			}

			return urlOut;
		}

		//
		// Encode URL string - ANSI
		//
		static std::string urlEncode( const std::string& url )
		{
			CHAR urlOut[INTERNET_MAX_URL_LENGTH];
			DWORD urlOutLen = INTERNET_MAX_URL_LENGTH;

			if( FAILED( UrlEscapeA( url.c_str(), urlOut, &urlOutLen, URL_ESCAPE_PERCENT ) ) )
			{
			//	PrintDebug("Error encoding URL: %s", url.c_str());
				return url;
			}

			return urlOut;
		}

		//
		// Decode URL string
		//
		static std::wstring urlDecode( const std::wstring& url )
		{
			WCHAR urlOut[INTERNET_MAX_URL_LENGTH];
			DWORD urlOutLen = INTERNET_MAX_URL_LENGTH;

			// UrlUnescape requires non-const string (we don't use URL_UNESCAPE_INPLACE flag)
			if( FAILED( UrlUnescapeW( const_cast<PWSTR>( &url[0] ), urlOut, &urlOutLen, 0 ) ) )
			{
			//	PrintDebug("Error decoding URL: %ls", url.c_str());
				return url;
			}

			return urlOut;
		}

		//
		// Decode URL string - ANSI
		//
		static std::string urlDecode( const std::string& url )
		{
			CHAR urlOut[INTERNET_MAX_URL_LENGTH];
			DWORD urlOutLen = INTERNET_MAX_URL_LENGTH;

			// UrlUnescape requires non-const string (we don't use URL_UNESCAPE_INPLACE flag)
			if( FAILED( UrlUnescapeA( const_cast<PSTR>( &url[0] ), urlOut, &urlOutLen, 0 ) ) )
			{
			//	PrintDebug("Error decoding URL: %ls", url.c_str());
				return url;
			}

			return urlOut;
		}

		//
		// Get SMB network shared directories list filtered
		//
		static std::vector<std::wstring> getSharedFolders( LPWSTR serverName = NULL, bool nameOnly = false, DWORD *result = nullptr )
		{
			std::vector<std::wstring> sharedItems;

			PSHARE_INFO_0 shareInfo0, pInfo0;
			PSHARE_INFO_2 shareInfo2, pInfo2;

			LPBYTE *pBufPtr = nameOnly ? (LPBYTE*)&shareInfo0 : (LPBYTE*)&shareInfo2;
			DWORD level = nameOnly ? 0 : 2;

			DWORD entriesRead = 0, totalRead = 0, resume = 0;
			NET_API_STATUS res;

			do
			{
				res = NetShareEnum( serverName, level, pBufPtr, MAX_PREFERRED_LENGTH, &entriesRead, &totalRead, &resume );
				if( res == ERROR_SUCCESS || res == ERROR_MORE_DATA )
				{
					if( nameOnly ) pInfo0 = shareInfo0;
					else pInfo2 = shareInfo2;

					for( DWORD i = 0; i < entriesRead; ++i )
					{
						if( !nameOnly && pInfo2->shi2_type == STYPE_DISKTREE )
						{
							sharedItems.push_back( pInfo2->shi2_path );
						}
						else if( nameOnly )
						{
							sharedItems.push_back( pInfo0->shi0_netname );
						}

						if( nameOnly ) pInfo0++;
						else pInfo2++;
					}

					if( nameOnly ) NetApiBufferFree( shareInfo0 );
					else NetApiBufferFree( shareInfo2 );
				}
			} while( res == ERROR_MORE_DATA );

			if( result )
				*result = res;

			return sharedItems;
		}

		//
		// Get SMB network shared directories list full - for dialog
		//
		static std::vector<std::tuple<std::wstring, std::wstring, std::wstring, DWORD>> getSharedFoldersFull( LPWSTR serverName = NULL, DWORD *result = nullptr )
		{
			PSHARE_INFO_2 shareInfo, pInfo;
			DWORD entriesRead = 0, totalRead = 0, resume = 0;
			NET_API_STATUS res;

			std::vector<std::tuple<std::wstring, std::wstring, std::wstring, DWORD>> sharedItems;

			do
			{
				res = NetShareEnum( serverName, 2, (LPBYTE*)&shareInfo, MAX_PREFERRED_LENGTH, &entriesRead, &totalRead, &resume );
				if( res == ERROR_SUCCESS || res == ERROR_MORE_DATA )
				{
					pInfo = shareInfo;
					for( DWORD i = 0; i < entriesRead; ++i )
					{
						sharedItems.push_back( std::make_tuple( pInfo->shi2_netname, pInfo->shi2_path, pInfo->shi2_remark, pInfo->shi2_type ) );
						pInfo++;
					}

					NetApiBufferFree( shareInfo );
				}
			} while( res == ERROR_MORE_DATA );

			if( result )
				*result = res;

			return sharedItems;
		}

		//
		// Get WNet extended error description
		//
		static std::wstring getExtendedError()
		{
			DWORD errorId = 0;
			WCHAR errorBuf[0x400];
			WCHAR nameBuf[0x200];

			if( WNetGetLastError( &errorId, errorBuf, sizeof( errorBuf ) / 2, nameBuf, sizeof( nameBuf ) / 2 ) == NO_ERROR )
				return FORMAT( L"Error: (%u) %ls[%ls]", errorId, errorBuf, nameBuf );
			else
				return L"Invalid buffer";
		}

		//
		// Get WNet error description
		//
		static std::wstring getErrorDescription( DWORD errorId )
		{
			std::wstring errorDescription;

			switch( errorId )
			{
			case ERROR_NOT_CONTAINER:
				errorDescription = L"The lpNetResource parameter does not point to a container.";
				break;
			case ERROR_INVALID_PARAMETER:
				errorDescription = L"Either the dwScope or the dwType parameter is invalid, or there is an invalid combination of parameters.";
				break;
			case ERROR_NO_NETWORK:
				errorDescription = L"The network is unavailable.";
				break;
			case ERROR_EXTENDED_ERROR:
				errorDescription = getExtendedError();
				break;
			case ERROR_INVALID_ADDRESS:
				errorDescription = L"A remote network resource name supplied in the NETRESOURCE structure resolved to an invalid network address.";
				break;
			default:
				errorDescription = L"No error.";
				break;
			}

			return errorDescription;
		}

		//
		// Get SMB network shared resources
		//
		static bool enumNetworkResources( LPNETRESOURCE pNetResource, CBackgroundWorker *pWorker )
		{
			HANDLE hEnum;
			DWORD res = WNetOpenEnum( RESOURCE_GLOBALNET, RESOURCETYPE_DISK, 0, pNetResource, &hEnum );
			if( res != NO_ERROR )
			{
				PrintDebug( "WNetOpenEnum (%ls): %ls", pNetResource->lpProvider, getErrorDescription( res ).c_str() );
				return false;
			}

			DWORD entriesCount = static_cast<DWORD>( -1 );
			DWORD bufferSize = 0x4000;
			LPNETRESOURCE pNetResLocal = (LPNETRESOURCE)GlobalAlloc( GPTR, bufferSize );
			do
			{
				ZeroMemory( pNetResLocal, bufferSize );

				res = WNetEnumResource( hEnum, &entriesCount, pNetResLocal, &bufferSize );
				if( res == NO_ERROR )
				{
					for( int i = 0; i < entriesCount; ++i )
					{
						if( pNetResLocal[i].dwDisplayType == RESOURCEDISPLAYTYPE_SERVER ||
							pNetResLocal[i].dwDisplayType == RESOURCEDISPLAYTYPE_SHARE )
						{
							LPWSTR remoteName =
								pNetResLocal[i].lpRemoteName ? pNetResLocal[i].lpRemoteName :
								pNetResLocal[i].lpComment ? pNetResLocal[i].lpComment : NULL;

							// update caller with the entry name and type - also skip leading backslashes "\\\\"
							pWorker->sendMessage( pNetResLocal[i].dwDisplayType, reinterpret_cast<LPARAM>( remoteName + 2 ) );
						}

						// if the NETRESOURCE structure represents a container resource, call the enumNetworkResources function recursively
						if( ( pNetResLocal[i].dwUsage & RESOURCEUSAGE_CONTAINER ) == RESOURCEUSAGE_CONTAINER )
						{
							// recursive call to itself
							enumNetworkResources( &pNetResLocal[i], pWorker );
						}
					}
				}
				else if( res != ERROR_NO_MORE_ITEMS )
				{
					PrintDebug( "WNetOpenEnum: %ls", getErrorDescription( res ).c_str() );
					break;
				}
			} while( res != ERROR_NO_MORE_ITEMS && pWorker->isRunning() );

			GlobalFree( (HGLOBAL)pNetResLocal );
			WNetCloseEnum( hEnum );

			return true;
		}

		// do not instantiate this class/struct
		NetUtils() = delete;
		NetUtils( NetUtils const& ) = delete;
		void operator=( NetUtils const& ) = delete;
	};
}
