#include "stdafx.h"

#include "Commander.h"
#include "SshReader.h"
#include "FileSystemUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CSshReader::CSshReader( ESortMode mode, HWND hwndNotify, std::shared_ptr<SshData> spData )
		: CBaseReader( mode, hwndNotify )
		, _contentLoaded( false )
		, _isFirstRun( true )
		, _osType( EOsTypes::Default )
	{
		_spSshData = spData;

		_hWndNotify = hwndNotify;
		_root = ReaderType::getModePrefix( EReadMode::Putty );

		_worker.init( [this] { return _sshReadCore(); }, _hWndNotify, UM_READERNOTIFY );

		if( _spSshData ) {
			_spSshData->shell.pWorker = &_worker;
			_spSshData->shell.FOnCaptureOutput = std::bind( &CSshReader::onCaptureOutput, this, std::placeholders::_1, std::placeholders::_2 );
		}

		LvUtils::setImageList( hwndNotify, FCS::inst().getApp().getSystemImageList(), LVSIL_SMALL );
	}

	CSshReader::~CSshReader()
	{
		// stop workers
		_worker.stop();

		if( _spSshData && _spSshData->shell.FOpened )
			_spSshData->shell.Close();
	}

	//
	// Connect to ssh server in background worker
	//
	bool CSshReader::_sshReadCore()
	{
		try
		{
			static wchar_t *nationalVars[] = { L"LANG", L"LANGUAGE", L"LC_CTYPE", L"LC_COLLATE", L"LC_MONETARY", L"LC_NUMERIC", L"LC_TIME", L"LC_MESSAGES", L"LC_ALL", L"HUMAN_BLOCKS" };
			static wchar_t *lsLinux = L"ls -la --time-style=+\"%Y-%m-%d %H:%M:%S\"";
			static wchar_t *lsFrBsd = L"ls -laD \"%Y-%m-%d %H:%M:%S\"";
			static wchar_t *lsMacos = L"ls -laT";
			static wchar_t *lsDeflt = L"ls -la";

			if( _isFirstRun )
			{
				_isFirstRun = false;
				_spSshData->shell.FUtfStrings = true;

				// detect system language (utf-8 is set by default)
				auto lang = _spSshData->shell.SendCommandFull( bcb::fsLang );

				if( lang.empty() || lang[0].empty() )
					lang = _spSshData->shell.SendCommandFull( bcb::fsAnyCommand, L"echo $LC_CTYPE" );

				if( !lang.empty() )
					_lang = lang[0];

				//if( !_lang.empty() && ( _lang.find( L"UTF-8" ) != std::wstring::npos || _lang.find( L"utf8" ) != std::wstring::npos ) )
				//	_spSshData->shell.FUtfStrings = true;

				// detect operating system
				auto ostype = _spSshData->shell.SendCommandFull( bcb::fsAnyCommand, L"echo $OSTYPE" ); // darwin16, linux-gnu, FreeBSD

				if( ostype.empty() || ostype[0].empty() )
					ostype = _spSshData->shell.SendCommandFull( bcb::fsAnyCommand, L"uname" );

				if( !ostype.empty() )
					_osType = getOsType( ostype[0] );

				// unset national variables to force english language
				for( int i = 0; i < ARRAYSIZE( nationalVars ); i++ )
					_spSshData->shell.SendCommandFull( bcb::fsUnset, nationalVars[i] );

				// test listing command
				std::vector<std::wstring> list;
				switch( _osType )
				{
				case EOsTypes::Default: // try linux when OS was not detected
				case EOsTypes::Linux:
				case EOsTypes::Solaris:
					list = _spSshData->shell.SendCommandFull( bcb::fsAnyCommand, lsLinux );
					break;
				case EOsTypes::Macos:
				case EOsTypes::NetBsd:
				case EOsTypes::OpenBsd:
					list = _spSshData->shell.SendCommandFull( bcb::fsAnyCommand, lsMacos );
					break;
				case EOsTypes::FreeBsd:
					list = _spSshData->shell.SendCommandFull( bcb::fsAnyCommand, lsFrBsd );
					break;
				}

				// fall back to default on error
				if( list.empty() || list[0].empty() || !_errorMsg.empty() )
					_osType = EOsTypes::Default;
				else
					_osType = ( _osType == EOsTypes::Default ? EOsTypes::Linux : _osType );

				_errorMsg.clear();
			}

			auto cwd = _spSshData->shell.SendCommandFull( bcb::fsChangeDirectory, _currentDirectory );
			auto pwd = _spSshData->shell.SendCommandFull( bcb::fsCurrentDirectory );
			auto frs = _spSshData->shell.SendCommandFull( bcb::fsAnyCommand, L"df -k ." );

			std::vector<std::wstring> list;
			switch( _osType )
			{
			case EOsTypes::Default:
				list = _spSshData->shell.SendCommandFull( bcb::fsAnyCommand, lsDeflt );
				break;
			case EOsTypes::Linux:
			case EOsTypes::Solaris:
				list = _spSshData->shell.SendCommandFull( bcb::fsAnyCommand, lsLinux );
				break;
			case EOsTypes::Macos:
			case EOsTypes::NetBsd:
			case EOsTypes::OpenBsd:
				list = _spSshData->shell.SendCommandFull( bcb::fsAnyCommand, lsMacos );
				break;
			case EOsTypes::FreeBsd:
				list = _spSshData->shell.SendCommandFull( bcb::fsAnyCommand, lsFrBsd );
				break;
			}

			// do parse the ls command output
			parseEntries( list );

			// do parse the df command output
			parseFreeSpace( frs );

			// do sorting
			sortEntries();
		}
		catch( bcb::ESshFatal& e )
		{
			_errorMsg = StringUtils::convert2W( e.what() );
			return false;
		}

		return _errorMsg.empty();
	}

	CSshReader::EOsTypes CSshReader::getOsType( const std::wstring& str )
	{
		EOsTypes osType = EOsTypes::Default;

		if( StringUtils::startsWith( str, L"linux" ) )
			osType = EOsTypes::Linux;
		else if( StringUtils::startsWith( str, L"darwin" ) )
			osType = EOsTypes::Macos;
		else if( StringUtils::startsWith( str, L"FreeBSD" ) )
			osType = EOsTypes::FreeBsd;
		else if( StringUtils::startsWith( str, L"NetBSD" ) )
			osType = EOsTypes::NetBsd;
		else if( StringUtils::startsWith( str, L"OpenBSD" ) )
			osType = EOsTypes::OpenBsd;
		else if( StringUtils::startsWith( str, L"solaris" ) || StringUtils::startsWith( str, L"SunOS" ) )
			osType = EOsTypes::Solaris;

		return osType;
	}

	//
	// Calculate directory size in backgroud worker
	//
	bool CSshReader::_sshCalcSizeCore()
	{
		try
		{
			ULONGLONG fileSizeResult = 0;

			for( const auto& idx : _markedEntries )
			{
				if( !_worker.isRunning() )
					return false;

				auto entryData = getEntry( idx );
				if( entryData )
				{
					if( IsDir( entryData->wfd.dwFileAttributes ) )
					{
						auto res = _spSshData->shell.SendCommandFull( bcb::fsAnyCommand,
							FORMAT( L"du -ks \"%ls\"", entryData->wfd.cFileName ) ); // on linux L"du -s --apparent-size --block-size=1 %ls"

						if( !res.empty() && !res[0].empty() )
						{
							auto sizeStr = StringUtils::cutToChar( res[0], L'\t' );
							if( !sizeStr.empty() )
							{
								auto sizeResult = std::stoull( sizeStr );
								fileSizeResult += ( sizeResult * 1024 );
							}
						}
					}
					else
					{
						fileSizeResult += FsUtils::getFileSize( &entryData->wfd );
					}
				}
			}

			_worker.sendNotify( 2, fileSizeResult );
		}
		catch(...)
		{
			return false;
		}

		return true;
	}

	void CSshReader::onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal )
	{
		if( msgId == UM_READERNOTIFY && _worker.checkId( workerId ) )
		{
			_contentLoaded = !!retVal;
			_status = _contentLoaded ? EReaderStatus::DataOk : EReaderStatus::DataError;
		}
		else if( msgId == UM_CALCSIZEDONE )
		{
			if( retVal == 2 )
				_resultSize = workerId;
		}
	}

	//
	// Check whether mode is still valid for given path
	//
	bool CSshReader::isPathValid( const std::wstring& path )
	{
		if( StringUtils::startsWith( path, ReaderType::getModePrefix( EReadMode::Putty ) ) && _spSshData && _spSshData->shell.FOpened )
			return true;

		return false;
	}

	WORD getMonth( const std::wstring& str )
	{
		static WCHAR *monthsEn[] = { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" };

		for( WORD i = 0; i < ARRAYSIZE( monthsEn ); ++i )
		{
			if( !wcsicmp( monthsEn[i], str.c_str() ) )
				return i + 1;
		}

		PrintDebug( "Invalid data: %ls", str.c_str() );

		return 0;
	}

	FILETIME getDateTime( const std::wstring& day, const std::wstring& month, const std::wstring& year, ULONGLONG loginTime )
	{
		SYSTEMTIME st = { 0 };

		bool isYear = ( year.find( L':' ) == std::wstring::npos );

		st.wMonth = getMonth( month );
		st.wDay = std::stoi( day );

		if( !isYear )
		{
			SYSTEMTIME slogt = { 0 };
			FILETIME flogt{ ( loginTime & 0xFFFFFFFF ), ( ( loginTime >> 32 ) & 0xFFFFFFFF ) };
			FileTimeToSystemTime( &flogt, &slogt ); // TODO: local/UTC conversion

			auto ftime = StringUtils::split( year, L":" );
			st.wHour = std::stoi( ftime[0] );
			st.wMinute = std::stoi( ftime[1] );
			st.wYear = slogt.wYear;
		}
		else
			st.wYear = std::stoi( year );

		FILETIME ft;
		SystemTimeToFileTime( &st, &ft );

		return ft;
	}

	FILETIME parseDateTimeLinux( const std::wstring& sDate, const std::wstring& sTime )
	{
		SYSTEMTIME st = { 0 };

		swscanf( sDate.c_str(), L"%hu-%hu-%hu", &st.wYear, &st.wMonth, &st.wDay );
		swscanf( sTime.c_str(), L"%hu:%hu:%hu", &st.wHour, &st.wMinute, &st.wSecond );

		FILETIME ft;
		SystemTimeToFileTime( &st, &ft );

		return ft;
	}

	FILETIME parseDateTimeMacos( const std::wstring& day, const std::wstring& month, const std::wstring& year, const std::wstring& sTime )
	{
		SYSTEMTIME st = { 0 };

		st.wMonth = getMonth( month );
		swscanf( day.c_str(), L"%hu", &st.wDay );
		swscanf( year.c_str(), L"%hu", &st.wYear );
		swscanf( sTime.c_str(), L"%hu:%hu:%hu", &st.wHour, &st.wMinute, &st.wSecond );

		FILETIME ft;
		SystemTimeToFileTime( &st, &ft );

		return ft;
	}

	EntryData CSshReader::parseEntry( const std::vector<std::wstring>& entry )
	{
		EntryData data = { 0 };

		auto entryLen = entry.size();

		ULARGE_INTEGER fSize;

		switch( _osType )
		{
		case EOsTypes::Default:
			swscanf( entry[entryLen - 5].c_str(), L"%zu", &fSize.QuadPart );
			data.wfd.ftLastWriteTime = getDateTime( entry[entryLen - 3], entry[entryLen - 4], entry[entryLen - 2], _spSshData->shell.FSessionInfo.LoginTime );
			break;
		case EOsTypes::Linux:
		case EOsTypes::Solaris:
		case EOsTypes::FreeBsd:
			swscanf( entry[entryLen - 4].c_str(), L"%zu", &fSize.QuadPart );
			data.wfd.ftLastWriteTime = parseDateTimeLinux( entry[entryLen - 3], entry[entryLen - 2] );
			break;
		case EOsTypes::Macos:
		case EOsTypes::NetBsd:
		case EOsTypes::OpenBsd:
			swscanf( entry[entryLen - 6].c_str(), L"%zu", &fSize.QuadPart );
			data.wfd.ftLastWriteTime = parseDateTimeMacos( entry[entryLen - 4], entry[entryLen - 5], entry[entryLen - 2], entry[entryLen - 3] );
			break;
		}

		data.wfd.nFileSizeLow = fSize.LowPart;
		data.wfd.nFileSizeHigh = fSize.HighPart;

		data.link.clear();

		// b = block device, c = character device, p = named pipe, s = socket
		// D = door, on Solaris, not common on Linux systems, but has been ported

		if( entry[0][0] == L'd' ) // d (directory)
		{
			data.wfd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
			wcsncpy( data.wfd.cFileName, entry[entryLen - 1].c_str(), ARRAYSIZE( data.wfd.cFileName ) );
		}
		else if( entry[0][0] == L'l' ) // l (symlink)
		{
			data.wfd.dwFileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
			wcsncpy( data.wfd.cFileName, PathUtils::getLinkTarget( entry[entryLen - 1], data.link ).c_str(), ARRAYSIZE( data.wfd.cFileName ) );
		}
		else
		{
			data.wfd.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
			wcsncpy( data.wfd.cFileName, entry[entryLen - 1].c_str(), ARRAYSIZE( data.wfd.cFileName ) );
		}

		if( entry[entryLen - 1][0] == L'.' && entry[entryLen - 1][1] != L'.' )
			data.wfd.dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;

		data.imgSystem = CDiskReader::getIconIndex( data.wfd.cFileName, data.wfd.dwFileAttributes );

		data.date = FsUtils::getFormatStrFileDate( &data.wfd, false );
		data.time = FsUtils::getFormatStrFileTime( &data.wfd, false );
		data.size = FsUtils::getFormatStrFileSize( &data.wfd );
		data.attr = entry[0];

	//	PrintDebug( "%ls", data.wfd.cFileName );

		return data;
	}

	EntryData CSshReader::parseLine( std::wstring line )
	{
		std::vector<std::wstring> entry;
		std::wstring prevLine = line;

		int columnCnt = 8;

		switch( _osType )
		{
		case EOsTypes::Default:
			columnCnt = 8;
			break;
		case EOsTypes::Linux:
		case EOsTypes::Solaris:
		case EOsTypes::FreeBsd:
			columnCnt = 7;
			break;
		case EOsTypes::Macos:
		case EOsTypes::NetBsd:
		case EOsTypes::OpenBsd:
			columnCnt = 9;
			break;
		}

		for( int i = 0; i < columnCnt; ++i )
		{
			entry.push_back( StringUtils::cutToChar( line, L' ' ) );
		}

		if( !entry.empty() && !entry[0].empty() )
		{
			// block/character devices add extra column (except on FreeBSD)
			if( _osType != EOsTypes::FreeBsd && ( entry[0][0] == L'b' || entry[0][0] == L'c' ) )
			{
				entry.push_back( StringUtils::cutToChar( line, L' ' ) );
			}

			// add the rest of the string as a filename
			entry.push_back( line );
		}
		else
		{
			PrintDebug( "Error parsing line: %ls", prevLine.c_str() );
			throw( std::invalid_argument( "empty line" ) );
		}

		return parseEntry( entry );
	}

	void CSshReader::parseEntries( const std::vector<std::wstring>& entries )
	{
		if( !entries.empty() && StringUtils::startsWith( entries[0], L"total" ) )
		{
			for( auto it = entries.begin() + 1; it != entries.end(); ++it )
			{
				if( !_worker.isRunning() )
					return;

				try
				{
					auto entry = parseLine( *it );

					if( FsUtils::checkAttrib( entry.wfd.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY ) )
					{
						if( !PathUtils::isDirectoryDotted( entry.wfd.cFileName ) && !( _currentDirectory.length() == 1 && PathUtils::isDirectoryDoubleDotted( entry.wfd.cFileName ) ) )
						{
							_dirEntries.push_back( std::make_shared<EntryData>( entry ) );
						}
					}
					else
						_fileEntries.push_back( std::make_shared<EntryData>( entry ) );
				}
				catch( std::invalid_argument& e )
				{
					e.what();
				}
			}
		}
	}

	void CSshReader::parseFreeSpace( const std::vector<std::wstring>& diskFree )
	{
		if( diskFree.size() > 1 )
		{
			auto str = diskFree[1];

			// read the 3rd column: "Available"
			for( int i = 0; i < 3; ++i )
			{
				StringUtils::cutToChar( str, L' ' );
			}

			// size is in 1 kilobyte block-size
			_freeSpace = std::stoull( StringUtils::cutToChar( str, L' ' ) );
			_freeSpace *= 1024;
		}
	}

	//
	// On error oputput
	//
	void CSshReader::onCaptureOutput( const std::wstring& str, bcb::TCaptureOutputType outputType )
	{
		if( outputType == bcb::TCaptureOutputType::cotError )
		{
			_errorMsg = str;

			PrintDebug("[Error] %ls", str.c_str());
		}
	}

	/*int OpenConnection(const char *hostname, int port)
	{
		int sd, err;
		struct addrinfo hints = {}, *addrs;
		char port_str[16] = {};

		hints.ai_family = AF_INET; // Since your original code was using sockaddr_in and
								   // PF_INET, I'm using AF_INET here to match.  Use
								   // AF_UNSPEC instead if you want to allow getaddrinfo()
								   // to find both IPv4 and IPv6 addresses for the hostname.
								   // Just make sure the rest of your code is equally family-
								   // agnostic when dealing with the IP addresses associated
								   // with this connection. For instance, make sure any uses
								   // of sockaddr_in are changed to sockaddr_storage,
								   // and pay attention to its ss_family field, etc...
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		sprintf(port_str, "%d", port);

		err = getaddrinfo(hostname, port_str, &hints, &addrs);
		if (err != 0)
		{
			fprintf(stderr, "%s: %s\n", hostname, gai_strerror(err));
			abort();
		}

		for (struct addrinfo *addr = addrs; addr != NULL; addr = addr->ai_next)
		{
			sd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
			if (sd == -1)
			{
				err = errno;
				break; // if using AF_UNSPEC above instead of AF_INET/6 specifically,
					   // replace this 'break' with 'continue' instead, as the 'ai_family'
					   // may be different on the next iteration...
			}

			if (connect(sd, addr->ai_addr, addr->ai_addrlen) == 0)
				break;

			err = errno;

			close(sd);
			sd = -1;
		}

		freeaddrinfo(addrs);

		if (sd == -1)
		{
			fprintf(stderr, "%s: %s\n", hostname, strerror(err));
			abort();
		}

		return sd;
	}*/
	//
	// Read data
	//
	bool CSshReader::readEntries( std::wstring& path )
	{
		if( isPathValid( path ) )
		{
			if( _worker.isRunning() || ( !_worker.isRunning() && !_contentLoaded ) )
			{
				_status = EReaderStatus::ReadingData;
				_contentLoaded = false;

			//	if( _worker.isRunning() )
			//		_spSshData->shell.SendSpecial( SS_SIGINT );

				_worker.stop();
				_errorMsg.clear();

				_worker.init( [this] { return _sshReadCore(); }, _hWndNotify, UM_READERNOTIFY );

				_currentDirectory = path.substr( wcslen( ReaderType::getModePrefix( EReadMode::Putty ) ) );
				PathUtils::unifyDelimiters( _currentDirectory, false );

				_dirEntries.clear();
				_fileEntries.clear();

				/*std::vector<std::wstring> entries = {
					L"total 970116",
					L"drwxr-xr-x  23 root root      4096 záø  8 15:08 .",
					L"drwxr-xr-x  23 root root      4096 záø  8 15:08 ..",
					L"drwxr-xr-x   2 root root      4096 srp 11 19:30 bin",
					L"drwxr-xr-x   4 root root      4096 záø  8 15:09 boot",
					L"drwxrwxr-x   2 root root      4096 záø 19  2019 cdrom",
					L"drwxr-xr-x  18 root root      3960 záø  9 12:24 dev",
					L"drwxr-xr-x 128 root root     12288 záø  9 12:24 etc",
					L"drwxr-xr-x   3 root root      4096 záø 19  2019 home",
					L"lrwxrwxrwx   1 root root        34 záø  8 15:08 initrd.img -> boot/initrd.img-4.15.0-117-generic",
					L"lrwxrwxrwx   1 root root        34 záø  8 15:08 initrd.img.old -> boot/initrd.img-4.15.0-115-generic",
					L"drwxr-xr-x  21 root root      4096 záø 19  2019 lib",
					L"drwxr-xr-x   2 root root      4096 èec  9 17:12 lib64",
					L"drwx------   2 root root     16384 záø 19  2019 lost+found",
					L"drwxr-xr-x   3 root root      4096 záø 19  2019 media",
					L"drwxr-xr-x   2 root root      4096 øíj  3  2018 mnt",
					L"drwxr-xr-x   2 root root      4096 øíj  3  2018 opt",
					L"dr-xr-xr-x 231 root root         0 záø  9 12:24 proc",
					L"drwx------   3 root root      4096 øíj 16  2018 root",
					L"drwxr-xr-x  32 root root       960 záø  9 23:05 run",
					L"drwxr-xr-x   2 root root     12288 srp 29 10:44 sbin",
					L"drwxr-xr-x   2 root root      4096 øíj  3  2018 srv",
					L"-rw-------   1 root root 993244160 záø 19  2019 swapfile",
					L"dr-xr-xr-x  13 root root         0 záø  9 20:44 sys",
					L"drwxrwxrwt  15 root root      4096 záø  9 21:00 tmp",
					L"drwxr-xr-x  11 root root      4096 lis  1  2019 usr",
					L"drwxr-xr-x  12 root root      4096 øíj 16  2018 var",
					L"lrwxrwxrwx   1 root root        31 záø  8 15:08 vmlinuz -> boot/vmlinuz-4.15.0-117-generic",
					L"lrwxrwxrwx   1 root root        31 záø  8 15:08 vmlinuz.old -> boot/vmlinuz-4.15.0-115-generic",
					L"-rw-r--r--   1 root root     15202 kvì  6 22:31 .VolumeIcon.icns",
					L"-rw-r--r--   1 root root     21984 kvì  6 22:31 .VolumeIcon.png",
					L"prw-rw-r--   1 choo choo         0 Nov  7 01:59 prog_pipe",
					L"srwxrwxrwx   1 root root         0 Dec 18 18:03 /tmp/.X11-unix/X0",
					L"crw-r-----   1 root tty      4,  0 Sep 23 12:51 tty0",
					L"brw-rw-----  1 root disk     8,  1 Sep 23 12:51 sda0",
					L"brw-rw-----  1 root disk     8,  a 10  b  12:51 sda0",
					L""
				};

				parseEntries( entries );*/

				//data.FHostNameExpanded = ExpandEnvironmentVariables(data.FHostName);
				//data.FUserNameExpanded = ExpandEnvironmentVariables(data.FUserName);

				return _worker.start();
			}

			_contentLoaded = false;

			return true;
		}

		_status = EReaderStatus::Invalid;

		return false;
	}

	//
	//
	//
	bool CSshReader::copyEntries( const std::vector<std::wstring>& entries, const std::wstring& path )
	{
		return false;
	}

	//
	//
	//
	bool CSshReader::moveEntries( const std::vector<std::wstring>& entries, const std::wstring& path )
	{
		return false;
	}

	//
	//
	//
	bool CSshReader::renameEntry( const std::wstring& path, const std::wstring& newName )
	{
		return false;
	}

	//
	//
	//
	bool CSshReader::deleteEntries( const std::vector<std::wstring>& entries )
	{
		return false;
	}

	//
	// Calculate size - return false means in-thread async operation (empty entries means stop calculation)
	//
	bool CSshReader::calculateSize( const std::vector<int>& entries )
	{
		_worker.stop();

		_resultSize = 0ull;

		if( !entries.empty() )
		{
			_worker.init( [this] { return _sshCalcSizeCore(); }, _hWndNotify, UM_CALCSIZEDONE );
			_markedEntries = entries;

			_worker.start();
		}

		return false;
	}
}
