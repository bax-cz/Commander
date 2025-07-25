#include "stdafx.h"

#include "Commander.h"
//#include "Common.h"
#include "PuttyInterface.h"
//#include "Exceptions.h"
//#include "Interface.h"
#include "SecureShell.h"
//#include "TextsCore.h"
//#include "HelpCore.h"
//#include "CoreMain.h"
//#include <StrUtils.hpp>
//#include <Consts.hpp>

#ifndef AUTO_WINSOCK
#include <winsock2.h>
#endif
#include <ws2ipdef.h>
#include <Ws2tcpip.h>
//---------------------------------------------------------------------------
//#pragma package(smart_init)
//---------------------------------------------------------------------------
#define MAX_BUFSIZE 32768
//---------------------------------------------------------------------------
namespace bcb {
using namespace Commander;
//---------------------------------------------------------------------------
const wchar_t HostKeyDelimiter = L';';
static std::unique_ptr<TCriticalSection> PuttyStorageSection(TraceInitPtr(new TCriticalSection()));
//---------------------------------------------------------------------------
struct TPuttyTranslation
{
	const wchar_t *Original;
	int Translation;
	UnicodeString HelpKeyword;
};
//---------------------------------------------------------------------------
struct ScpLogPolicy : public LogPolicy
{
	TSecureShell *SecureShell;
	struct Seat *Seat;
};
//---------------------------------------------------------------------------
} // namespace bcb
using namespace bcb;
Seat *get_log_seat(LogContext *logctx)
{
	ScpLogPolicy *ALogPolicy = static_cast<ScpLogPolicy *>(log_get_logpolicy(logctx));
	return ALogPolicy->Seat;
}
//---------------------------------------------------------------------------
struct callback_set *get_log_callback_set(LogContext *logctx)
{
	ScpLogPolicy *ALogPolicy = static_cast<ScpLogPolicy *>(log_get_logpolicy(logctx));
	return ALogPolicy->SecureShell->GetCallbackSet();
}
//---------------------------------------------------------------------------
namespace bcb {
/*__fastcall*/ TSecureShell::TSecureShell(/*TSessionUI *UI,*/
	TSessionData *SessionData/*, TSessionLog *Log, TConfiguration *Configuration*/)
{
	pWorker = nullptr;
//	FUI = UI;
	FSessionData = SessionData;
	FSessionData->FFSProtocol = fsSCPonly; // TODO: for now just scp-only
//	FLog = Log;
//	FConfiguration = Configuration;
	FActive = false;
	FWaiting = 0;
	FOpened = false;
	FSshImplementation = sshiUnknown;
	OutLen = 0;
	OutPtr = NULL;
	Pending = NULL;
	FBackendHandle = NULL;
	FLogPolicy = NULL;
	FSeat = NULL;
	FLogCtx = NULL;
	ResetConnection();
	FOnCaptureOutput = NULL;
	FOnReceive = NULL;
	FSocket = INVALID_SOCKET;
	FSocketEvent = CreateEvent(NULL, false, false, NULL);
	FFrozen = false;
	FSimple = false;
	FCollectPrivateKeyUsage = false;
	FWaitingForData = 0;
	FCallbackSet.reset(new callback_set());
	memset(FCallbackSet.get(), 0, sizeof(callback_set));
	FCallbackSet->ready_event = INVALID_HANDLE_VALUE;
	Mode = ssmNone;
}
//---------------------------------------------------------------------------
/*__fastcall*/ TSecureShell::~TSecureShell()
{
	DebugAssert(FWaiting == 0);
	if (FActive)
	{
		Close();
	}
	ResetConnection();
	CloseHandle(FSocketEvent);
//	delete FCallbackSet;
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::ResetConnection()
{
	FreeBackend();
	ClearStdError();
	PendLen = 0;
	PendSize = 0;
	sfree(Pending);
	Pending = NULL;
	FCWriteTemp = L"";
	ResetSessionInfo();
	FAuthenticating = false;
	FAuthenticated = false;
	FStoredPasswordTried = false;
	FStoredPasswordTriedForKI = false;
	FStoredPassphraseTried = false;
	FAuthenticationCancelled = false;
	delete FLogPolicy;
	FLogPolicy = NULL;
	delete FSeat;
	FSeat = NULL;
	if (FLogCtx != NULL)
	{
		log_free(FLogCtx);
	}
	FLogCtx = NULL;
	FClosed = false;
	FLoggedKnownHostKeys.clear();
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::ResetSessionInfo()
{
	FSessionInfoValid = false;
}
//---------------------------------------------------------------------------
inline void /*__fastcall*/ TSecureShell::UpdateSessionInfo()
{
	if (!FSessionInfoValid)
	{
		DebugAssert(get_ssh_version(FBackendHandle) == 2);
		FSessionInfo.ProtocolBaseName = L"SSH";
		FSessionInfo.ProtocolName =
			FORMAT(L"%ls-%d", FSessionInfo.ProtocolBaseName.c_str(), get_ssh_version(FBackendHandle));
		FSessionInfo.SecurityProtocolName = FSessionInfo.ProtocolName;

		FSessionInfo.CSCipher = GetCipherName(get_cscipher(FBackendHandle));
		FSessionInfo.SCCipher = GetCipherName(get_sccipher(FBackendHandle));
		FSessionInfo.CSCompression = GetCompressorName(get_cscomp(FBackendHandle));
		FSessionInfo.SCCompression = GetDecompressorName(get_sccomp(FBackendHandle));

		FSessionInfoValid = true;
	}
}
//---------------------------------------------------------------------------
const TSessionInfo& /*__fastcall*/ TSecureShell::GetSessionInfo()
{
	if (!FSessionInfoValid)
	{
		UpdateSessionInfo();
	}
	return FSessionInfo;
}
//---------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::GetHostKeyFingerprint(UnicodeString& SHA256, UnicodeString& MD5)
{
	SHA256 = FSessionInfo.HostKeyFingerprintSHA256;
	MD5 = FSessionInfo.HostKeyFingerprintMD5;
}
//---------------------------------------------------------------------
Conf /*__fastcall*/ *TSecureShell::StoreToConfig(TSessionData *Data, bool Simple)
{
	Conf *conf = conf_new();

	PuttyDefaults(conf);

	// user-configurable settings
	conf_set_str(conf, CONF_host, AnsiString(Data->GetHostNameExpanded()).c_str());
	conf_set_str(conf, CONF_username, UTF8String(Data->GetUserNameExpanded()).c_str());
	conf_set_int(conf, CONF_port, Data->FPortNumber);
	conf_set_int(conf, CONF_protocol, PROT_SSH);
	conf_set_bool(conf, CONF_change_password, Data->FChangePassword);
	// always set 0, as we will handle keepalives ourselves to avoid
	// multi-threaded issues in putty timer list
	conf_set_int(conf, CONF_ping_interval, 0);
	conf_set_bool(conf, CONF_compression, Data->FCompression);
	conf_set_bool(conf, CONF_tryagent, Data->FTryAgent);
	conf_set_bool(conf, CONF_agentfwd, Data->FAgentFwd);
	conf_set_int(conf, CONF_addressfamily, Data->FAddressFamily);
	conf_set_str(conf, CONF_ssh_rekey_data, AnsiString(Data->FRekeyData).c_str());
	conf_set_int(conf, CONF_ssh_rekey_time, Data->FRekeyTime);

	DebugAssert(CIPHER_MAX == CIPHER_COUNT);
	for (int c = 0; c < CIPHER_COUNT; c++)
	{
		int pcipher;
		// Update also CollectUsage
		switch (Data->FCiphers[c]) {
		case cipWarn: pcipher = CIPHER_WARN; break;
		case cip3DES: pcipher = CIPHER_3DES; break;
		case cipBlowfish: pcipher = CIPHER_BLOWFISH; break;
		case cipAES: pcipher = CIPHER_AES; break;
		case cipDES: pcipher = CIPHER_DES; break;
		case cipArcfour: pcipher = CIPHER_ARCFOUR; break;
		case cipChaCha20: pcipher = CIPHER_CHACHA20; break;
		case cipAESGCM: pcipher = CIPHER_AESGCM; break;
		default: DebugFail(); pcipher = NULL; // shut up
		}
		conf_set_int_int(conf, CONF_ssh_cipherlist, c, pcipher);
	}

	DebugAssert(KEX_MAX == KEX_COUNT);
	for (int k = 0; k < KEX_COUNT; k++)
	{
		int pkex;
		switch (Data->FKex[k]) {
		case kexWarn: pkex = KEX_WARN; break;
		case kexDHGroup1: pkex = KEX_DHGROUP1; break;
		case kexDHGroup14: pkex = KEX_DHGROUP14; break;
		case kexDHGroup15: pkex = KEX_DHGROUP15; break;
		case kexDHGroup16: pkex = KEX_DHGROUP16; break;
		case kexDHGroup17: pkex = KEX_DHGROUP17; break;
		case kexDHGroup18: pkex = KEX_DHGROUP18; break;
		case kexDHGEx: pkex = KEX_DHGEX; break;
		case kexRSA: pkex = KEX_RSA; break;
		case kexECDH: pkex = KEX_ECDH; break;
		case kexNTRUHybrid: pkex = KEX_NTRU_HYBRID; break;
		case kexMLKEM25519Hybrid: pkex = KEX_MLKEM_25519_HYBRID; break;
		case kexMLKEMNISTHybrid: pkex = KEX_MLKEM_NIST_HYBRID; break;
		default: DebugFail(); pkex = NULL; // shutup 
		}
		conf_set_int_int(conf, CONF_ssh_kexlist, k, pkex);
	}

	DebugAssert(HK_MAX == HOSTKEY_COUNT);
	for (int h = 0; h < HOSTKEY_COUNT; h++)
	{
		int phk = HostKeyToPutty(Data->FHostKeys[h]);
		conf_set_int_int(conf, CONF_ssh_hklist, h, phk);
	}

	DebugAssert(ngsslibs == GSSLIB_COUNT);
	for (int g = 0; g < GSSLIB_COUNT; g++)
	{
		int pgsslib;
		switch (Data->FGssLib[g]) {
		case gssGssApi32: pgsslib = 0; break;
		case gssSspi: pgsslib = 1; break;
		case gssCustom: pgsslib = 2; break;
		default: DebugFail();
		}
		conf_set_int_int(conf, CONF_ssh_gsslist, g, pgsslib);
	}
	Filename *GssLibCustomFileName = filename_from_utf8(UTF8String(Data->FGssLibCustom).c_str());
	conf_set_filename(conf, CONF_ssh_gss_custom, GssLibCustomFileName);
	filename_free(GssLibCustomFileName);

	Filename *AFileName = filename_from_utf8(UTF8String(Data->ResolvePublicKeyFile()).c_str());
	conf_set_filename(conf, CONF_keyfile, AFileName);
	filename_free(AFileName);

	AFileName = filename_from_utf8(UTF8String(ExpandEnvironmentVariables(Data->FDetachedCertificate)).c_str());
	conf_set_filename(conf, CONF_detached_cert, AFileName);
	filename_free(AFileName);

	conf_set_bool(conf, CONF_ssh2_des_cbc, Data->FSsh2DES);
	conf_set_bool(conf, CONF_ssh_no_userauth, Data->FSshNoUserAuth);
	conf_set_bool(conf, CONF_try_ki_auth, Data->FAuthKI);
	conf_set_bool(conf, CONF_try_gssapi_auth, Data->FAuthGSSAPI);
	conf_set_bool(conf, CONF_try_gssapi_kex, Data->FAuthGSSAPIKEX);
	conf_set_bool(conf, CONF_gssapifwd, Data->FGSSAPIFwdTGT);
	conf_set_bool(conf, CONF_change_username, Data->FChangeUsername);

	conf_set_int(conf, CONF_proxy_type, Data->FProxyMethod);
	conf_set_str(conf, CONF_proxy_host, AnsiString(Data->FProxyHost).c_str());
	conf_set_int(conf, CONF_proxy_port, Data->FProxyPort);
	conf_set_str(conf, CONF_proxy_username, UTF8String(Data->FProxyUsername).c_str());
	conf_set_str(conf, CONF_proxy_password, UTF8String(Data->FProxyPassword).c_str());
	if (Data->FProxyMethod == pmCmd)
	{
		conf_set_str(conf, CONF_proxy_telnet_command, AnsiString(Data->FProxyLocalCommand).c_str());
	}
	else
	{
		conf_set_str(conf, CONF_proxy_telnet_command, AnsiString(Data->FProxyTelnetCommand).c_str());
	}
	conf_set_int(conf, CONF_proxy_dns, Data->FProxyDNS);
	conf_set_bool(conf, CONF_even_proxy_localhost, Data->FProxyLocalhost);

	conf_set_int(conf, CONF_sshbug_hmac2, Data->FBugs[sbHMAC2]);
	conf_set_int(conf, CONF_sshbug_derivekey2, Data->FBugs[sbDeriveKey2]);
	conf_set_int(conf, CONF_sshbug_rsapad2, Data->FBugs[sbRSAPad2]);
	conf_set_int(conf, CONF_sshbug_rekey2, Data->FBugs[sbRekey2]);
	conf_set_int(conf, CONF_sshbug_pksessid2, Data->FBugs[sbPKSessID2]);
	conf_set_int(conf, CONF_sshbug_maxpkt2, Data->FBugs[sbMaxPkt2]);
	conf_set_int(conf, CONF_sshbug_ignore2, Data->FBugs[sbIgnore2]);
	conf_set_int(conf, CONF_sshbug_winadj, Data->FBugs[sbWinAdj]);
	conf_set_int(conf, CONF_sshbug_oldgex2, Data->FBugs[sbOldGex2]);
	conf_set_int(conf, CONF_sshbug_chanreq, Data->FBugs[sbChanReq]);

	if (!Data->FTunnelPortFwd.IsEmpty())
	{
		DebugAssert(!Simple);
		UnicodeString TunnelPortFwd = Data->FTunnelPortFwd;
		while (!TunnelPortFwd.IsEmpty())
		{
			UnicodeString Buf = CutToChar(TunnelPortFwd, L',', true);
			AnsiString Key = AnsiString(CutToChar(Buf, L'\t', true));
			AnsiString Value = AnsiString(Buf);
			conf_set_str_str(conf, CONF_portfwd, Key.c_str(), Value.c_str());
		}

		// when setting up a tunnel, do not open shell/sftp
		conf_set_bool(conf, CONF_ssh_no_shell, TRUE);
	}
	else
	{
		DebugAssert(Simple);
		conf_set_bool(conf, CONF_ssh_simple, Data->FSshSimple && Simple);

		if (Data->FFSProtocol == fsSCPonly)
		{
			conf_set_bool(conf, CONF_ssh_subsys, FALSE);
			if (Data->FShell.IsEmpty())
			{
				// Following forces Putty to open default shell
				// see ssh.c: do_ssh2_authconn() and ssh1_protocol()
				conf_set_str(conf, CONF_remote_cmd, "");
			}
			else
			{
				conf_set_str(conf, CONF_remote_cmd, AnsiString(Data->FShell).c_str());
			}
			conf_set_bool(conf, CONF_force_remote_cmd2, 0);
		}
		else
		{
			if (Data->FSftpServer.IsEmpty())
			{
				conf_set_bool(conf, CONF_ssh_subsys, TRUE);
				conf_set_str(conf, CONF_remote_cmd, "sftp");
			}
			else
			{
				conf_set_bool(conf, CONF_ssh_subsys, FALSE);
				conf_set_str(conf, CONF_remote_cmd, AnsiString(Data->FSftpServer).c_str());
			}

			if (Data->FFSProtocol != fsSFTPonly)
			{
				conf_set_bool(conf, CONF_ssh_subsys2, FALSE);
				if (Data->FShell.IsEmpty())
				{
					// Following forces Putty to open default shell
					// see ssh.c: do_ssh2_authconn() and ssh1_protocol()
					conf_set_str(conf, CONF_remote_cmd2, "");
					// PuTTY ignores CONF_remote_cmd2 set to "",
					// so we have to enforce it
					// (CONF_force_remote_cmd2 is our config option)
					conf_set_bool(conf, CONF_force_remote_cmd2, 1);
				}
				else
				{
					conf_set_str(conf, CONF_remote_cmd2, AnsiString(Data->FShell).c_str());
				}
			}
			else
			{
				if (Data->FSftpServer.IsEmpty())
				{
					// see psftp_connect() from psftp.c
					conf_set_bool(conf, CONF_ssh_subsys2, FALSE);
					conf_set_str(conf, CONF_remote_cmd2,
						"test -x /usr/lib/sftp-server && exec /usr/lib/sftp-server\n"
						"test -x /usr/local/lib/sftp-server && exec /usr/local/lib/sftp-server\n"
						"exec sftp-server");
				}
				else
				{
					conf_set_bool(conf, CONF_force_remote_cmd2, 0);
				}
			}
		}
	}

	conf_set_int(conf, CONF_connect_timeout, Data->FTimeout * MSecsPerSec);
	conf_set_int(conf, CONF_sndbuf, Data->FSendBuf);
	conf_set_str(conf, CONF_srcaddr, AnsiString(Data->FSourceAddress).c_str());

	// permanent settings
	conf_set_bool(conf, CONF_nopty, TRUE);
	conf_set_bool(conf, CONF_tcp_keepalives, 0);
	conf_set_bool(conf, CONF_ssh_show_banner, TRUE);
	conf_set_int(conf, CONF_proxy_log_to_term, FORCE_OFF);

	conf_set_str(conf, CONF_loghost, AnsiString(Data->FLogicalHostName).c_str());

	return conf;
}
//---------------------------------------------------------------------------
static void eventlog(LogPolicy *ALogPolicy, const char *string)
{
	static_cast<ScpLogPolicy *>(ALogPolicy)->SecureShell->PuttyLogEvent(string);
}
//---------------------------------------------------------------------------
static const LogPolicyVtable ScpLogPolicyVTable =
{
	eventlog,
	NULL, // Should never be called
	NULL, // Should never be called
	null_lp_verbose_no, // Should never be called
};
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::Open()
{
	ResetConnection();

	FAuthenticating = false;
	FAuthenticated = false;
	FLastSendBufferUpdate = 0;

	// do not use UTF-8 until decided otherwise (see TSCPFileSystem::DetectUtf())
	FUtfStrings = false;

	FActive = false;

	FAuthenticationLog = L"";
	FNoConnectionResponse = false;
//	FUI->Information(LoadStr(STATUS_LOOKUPHOST));

	try
	{
		char *RealHost;
		FreeBackend(); // in case we are reconnecting
		const char *InitError;
		Conf *conf = StoreToConfig(FSessionData, FSimple);
		FSendBuf = FSessionData->FSendBuf;
		FSeat = new ScpSeat(this);
		FLogPolicy = new ScpLogPolicy();
		FLogPolicy->vt = &ScpLogPolicyVTable;
		FLogPolicy->SecureShell = this;
		FLogPolicy->Seat = FSeat;
		try
		{
			FLogCtx = log_init(FLogPolicy, conf);
			InitError = backend_init(&ssh_backend, FSeat, &FBackendHandle, FLogCtx, conf,
				AnsiString(FSessionData->GetHostNameExpanded()).c_str(), FSessionData->FPortNumber, &RealHost,
				(FSessionData->FTcpNoDelay ? 1 : 0),
				conf_get_bool(conf, CONF_tcp_keepalives));
		}
		catch (...) {}
	//	__finally
		{
			conf_free(conf);
		}
		sfree(RealHost);
		if (InitError)
		{
			PuttyFatalError(InitError);
		}
	//	FUI->Information(LoadStr(STATUS_CONNECT));
		FAuthenticationCancelled = false;
		if (!FActive && DebugAlwaysTrue(HasLocalProxy()))
		{
			FActive = true;
		}
		Init();

		CheckConnection(CONNECTION_FAILED);
	}
	catch (Exception& E)
	{
		UNREFERENCED_PARAMETER(E);
		if (FNoConnectionResponse && TryFtp())
		{
		//	Configuration->Usage->Inc(L"ProtocolSuggestions");
			// HELP_FTP_SUGGESTION won't be used as all errors that set
			// FNoConnectionResponse have already their own help keyword
		//	FUI->FatalError(&E, LoadStr(FTP_SUGGESTION), HELP_FTP_SUGGESTION);
		}
		else
		{
			throw( E );
		}
	}
	FLastDataSent = Now();

	FSessionInfo.LoginTime = Now();

	FAuthenticating = false;
	FAuthenticated = true;
//	FUI->Information(LoadStr(STATUS_AUTHENTICATED));

	ResetSessionInfo();

	DebugAssert(!FSessionInfo.SshImplementation.IsEmpty());
	FOpened = true;

	UnicodeString SshImplementation = GetSessionInfo().SshImplementation;
	if (IsOpenSSH(SshImplementation))
	{
		FSshImplementation = sshiOpenSSH;
	}
	// e.g. "mod_sftp/0.9.8"
	else if (SshImplementation.Pos(L"mod_sftp") == 1)
	{
		FSshImplementation = sshiProFTPD;
	}
	// e.g. "5.25 FlowSsh: Bitvise SSH Server (WinSSHD) 6.07: free only for personal non-commercial use"
	else if (SshImplementation.Pos(L"FlowSsh") > 0)
	{
		FSshImplementation = sshiBitvise;
	}
	// e.g. "srtSSHServer_10.00"
	else if (ContainsText(SshImplementation, L"srtSSHServer"))
	{
		FSshImplementation = sshiTitan;
	}
	else if (ContainsText(FSessionInfo.SshImplementation, L"OpenVMS"))
	{
		FSshImplementation = sshiOpenVMS;
	}
	else if (ContainsText(FSessionInfo.SshImplementation, L"CerberusFTPServer"))
	{
		FSshImplementation = sshiCerberus;
	}
	else
	{
		FSshImplementation = sshiUnknown;
	}
}
//---------------------------------------------------------------------------
bool /*__fastcall*/ TSecureShell::TryFtp()
{
	bool Result;
//	if (!FConfiguration->TryFtpWhenSshFails)
//	{
//		Result = false;
//	}
//	else
	{
		if (((FSessionData->FFSProtocol != fsSFTP) && (FSessionData->FFSProtocol != fsSFTPonly)) ||
			(FSessionData->FPortNumber != SshPortNumber) ||
			FSessionData->FTunnel || (FSessionData->FProxyMethod != TProxyMethod::pmNone))
		{
			LogEvent(L"Using non-standard protocol or port, tunnel or proxy, will not knock FTP port.");
			Result = false;
		}
		else
		{
			LogEvent(L"Knocking FTP port.");

			SOCKET Socket = socket(AF_INET, SOCK_STREAM, 0);
			Result = (Socket != INVALID_SOCKET);
			if (Result)
			{
				ADDRINFOW hints{ 0 }, *addrinfo = nullptr;
				hints.ai_family = AF_INET;
				hints.ai_socktype = SOCK_STREAM;
				hints.ai_protocol = IPPROTO_TCP;
				Result = !GetAddrInfoW(FSessionData->FHostName.c_str(), L"21", &hints, &addrinfo);
				if (Result)
				{
					HANDLE Event = CreateEvent(NULL, false, false, NULL);
					Result = (WSAEventSelect(Socket, (WSAEVENT)Event, FD_CONNECT | FD_CLOSE) != SOCKET_ERROR);

					if (Result)
					{
						Result =
							(connect(Socket, addrinfo->ai_addr, static_cast<int>(addrinfo->ai_addrlen)) != SOCKET_ERROR) ||
							(WSAGetLastError() == WSAEWOULDBLOCK);
						if (Result)
						{
							Result = (WaitForSingleObject(Event, 2000) == WAIT_OBJECT_0);
						}
					}
					FreeAddrInfoW(addrinfo);
					CloseHandle(Event);
				}
				closesocket(Socket);
			}

			if (Result)
			{
				LogEvent(L"FTP port opened, will suggest using FTP protocol.");
			}
			else
			{
				LogEvent(L"FTP port did not open.");
			}
		}
	}
	return Result;
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::Init()
{
	try
	{
		try
		{
			// Recent pscp checks backend_exitcode(FBackendHandle) in the loop
			// (see comment in putty revision 8110)
			// It seems that we do not need to do it.

			while (!winscp_query(FBackendHandle, WINSCP_QUERY_MAIN_CHANNEL))
			{
			//	if (Configuration->ActualLogProtocol >= 1)
			//	{
					LogEvent(L"Waiting for the server to continue with the initialization");
			//	}
				WaitForData();
			}
		}
		catch (ESshFatal& E)
		{
			UNREFERENCED_PARAMETER(E);
			if (FAuthenticating && !FAuthenticationLog.IsEmpty())
			{
				FatalError(FMTLOAD(AUTHENTICATION_LOG, (FAuthenticationLog)));
			}
			else
			{
				throw( E );
			}
		}
	}
	catch (ESshFatal& E)
	{
		UNREFERENCED_PARAMETER(E);
		if (FAuthenticating)
		{
			FatalError(LoadStr(AUTHENTICATION_FAILED));
		}
		else
		{
			throw( E );
		}
	}
}
//---------------------------------------------------------------------------
struct callback_set *TSecureShell::GetCallbackSet()
{
	return FCallbackSet.get();
}
//---------------------------------------------------------------------------
UnicodeString /*__fastcall*/ TSecureShell::ConvertFromPutty(const char *Str, size_t Length)
{
	size_t BomLength = strlen(WINSCP_BOM);
	if ((Length >= BomLength) &&
		(strncmp(Str, WINSCP_BOM, BomLength) == 0))
	{
		return UTF8ToString(Str + BomLength, Length - BomLength);
	}
	else
	{
		return AnsiToString(Str, Length);
	}
}
//---------------------------------------------------------------------------
const UnicodeString ServerVersionMsg(L"Remote version: ");
const UnicodeString ForwardingFailureMsg(L"Forwarded connection refused by remote");
const UnicodeString LocalPortMsg(L"Local port ");
const UnicodeString ForwadingToMsg(L" forwarding to ");
const UnicodeString FailedMsg(L" failed:");
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::PuttyLogEvent(const char *AStr)
{
	UnicodeString Str = ConvertFromPutty(AStr, strlen(AStr));
	// Gross hack
	if (StartsStr(ServerVersionMsg, Str))
	{
		FSessionInfo.SshVersionString = RightStr(Str, Str.Length() - ServerVersionMsg.Length());

		const wchar_t *Ptr = wcschr(FSessionInfo.SshVersionString.c_str(), L'-');
		if (Ptr != NULL)
		{
			Ptr = wcschr(Ptr + 1, L'-');
		}
		FSessionInfo.SshImplementation = (Ptr != NULL) ? Ptr + 1 : L"";
	}
	else if (StartsStr(ForwardingFailureMsg, Str))
	{
		if (ForwardingFailureMsg == Str)
		{
			FLastTunnelError = Str;
		}
		else
		{
			FLastTunnelError = RightStr(Str, Str.Length() - ForwardingFailureMsg.Length());
			UnicodeString Prefix(L": ");
			if (StartsStr(Prefix, FLastTunnelError))
			{
				FLastTunnelError.Delete(1, Prefix.Length());
			}
			static const TPuttyTranslation Translation[] = {
				{ L"Administratively prohibited [%]", PFWD_TRANSL_ADMIN },
				{ L"Connect failed [%]", PFWD_TRANSL_CONNECT },
			};
			TranslatePuttyMessage(Translation, LENOF(Translation), FLastTunnelError);
		}
	}
	else if (StartsStr(LocalPortMsg, Str) && ContainsStr(Str, ForwadingToMsg) && ContainsStr(Str, FailedMsg))
	{
		FLastTunnelError = Str;
	}
	LogEvent(Str);
}
//---------------------------------------------------------------------------
TPromptKind /*__fastcall*/ TSecureShell::IdentifyPromptKind(UnicodeString& Name)
{
	// beware of changing order
	static const TPuttyTranslation NameTranslation[] = {
		{ L"SSH login name", USERNAME_TITLE },
		{ L"SSH key passphrase", PASSPHRASE_TITLE },
		{ L"SSH TIS authentication", SERVER_PROMPT_TITLE },
		{ L"SSH CryptoCard authentication", SERVER_PROMPT_TITLE },
		{ L"SSH server: %", SERVER_PROMPT_TITLE2 },
		{ L"SSH server authentication", SERVER_PROMPT_TITLE },
		{ L"SSH password", PASSWORD_TITLE },
		{ L"New SSH password", NEW_PASSWORD_TITLE },
		{ L"SOCKS proxy authentication", PROXY_AUTH_TITLE },
		{ L"HTTP proxy authentication", PROXY_AUTH_TITLE },
	};

	int Index = TranslatePuttyMessage(NameTranslation, LENOF(NameTranslation), Name);

	TPromptKind PromptKind;
	if (Index == 0) // username
	{
		PromptKind = pkUserName;
	}
	else if (Index == 1) // passphrase
	{
		PromptKind = pkPassphrase;
	}
	else if (Index == 2) // TIS
	{
		PromptKind = pkTIS;
	}
	else if (Index == 3) // CryptoCard
	{
		PromptKind = pkCryptoCard;
	}
	else if ((Index == 4) || (Index == 5))
	{
		PromptKind = pkKeybInteractive;
	}
	else if (Index == 6)
	{
		PromptKind = pkPassword;
	}
	else if (Index == 7)
	{
		PromptKind = pkNewPassword;
	}
	else if ((Index == 8) || (Index == 9))
	{
		PromptKind = pkProxyAuth;
	}
	else
	{
		PromptKind = pkPrompt;
		DebugFail();
	}

	return PromptKind;
}
//---------------------------------------------------------------------------
/*bool IsAuthenticationPrompt(TPromptKind Kind)
{
	return
		(Kind == pkUserName) || (Kind == pkPassphrase) || (Kind == pkTIS) ||
		(Kind == pkCryptoCard) || (Kind == pkKeybInteractive) ||
		(Kind == pkPassword) || (Kind == pkNewPassword);
}
//---------------------------------------------------------------------------
bool IsPasswordOrPassphrasePrompt(TPromptKind Kind, TStrings *Prompts)
{
	return
		(Prompts->size() == 1) && FLAGCLEAR(int(Prompts->Objects[0]), pupEcho) &&
		((Kind == pkPassword) || (Kind == pkPassphrase) || (Kind == pkKeybInteractive) ||
		(Kind == pkTIS) || (Kind == pkCryptoCard));
}
//---------------------------------------------------------------------------
bool IsPasswordPrompt(TPromptKind Kind, TStrings *Prompts)
{
	return
		IsPasswordOrPassphrasePrompt(Kind, Prompts) &&
		(Kind != pkPassphrase);
}
//---------------------------------------------------------------------------
bool FUIPromptUser(TSessionData *FSessionData, TPromptKind Kind, UnicodeString Name,
	UnicodeString Instructions, TStrings *Prompts, TStrings *Results)
{
	for( size_t i = 0; i < Prompts->size(); i++ )
	{
		CTextInput::CParams params(
			Name.get(),
			Prompts->at( i).get(),
			IsPasswordPrompt( Kind, Prompts ) ?
			FSessionData->FPassphrase.get() :
			FSessionData->FUserName.get(),
			true, IsPasswordPrompt( Kind, Prompts ) );

		auto res = MiscUtils::showTextInputBox( params );

		if( res.empty() )
			return false;

		// update Results
		Results->at( i ) = res;
	}

	return true;
}*/
//---------------------------------------------------------------------------
bool /*__fastcall*/ TSecureShell::PromptUser(bool /*ToServer*/,
	UnicodeString AName, bool /*NameRequired*/,
	UnicodeString Instructions, bool InstructionsRequired,
	TStrings *Prompts, TStrings *Results)
{
	// there can be zero prompts!

	DebugAssert(Results->size() == Prompts->size());

	UnicodeString Name = AName;
	TPromptKind PromptKind = IdentifyPromptKind(Name);

	const TPuttyTranslation *InstructionTranslation = NULL;
	const TPuttyTranslation *PromptTranslation = NULL;
	size_t PromptTranslationCount = 1;
	UnicodeString PromptDesc;

	if (PromptKind == pkUserName)
	{
		static const TPuttyTranslation UsernamePromptTranslation[] = {
			{ L"login as: ", USERNAME_PROMPT2 },
		};

		PromptTranslation = UsernamePromptTranslation;
		PromptDesc = L"username";
	}
	else if (PromptKind == pkPassphrase)
	{
		static const TPuttyTranslation PassphrasePromptTranslation[] = {
			{ L"Passphrase for key \"%\": ", PROMPT_KEY_PASSPHRASE },
		};

		PromptTranslation = PassphrasePromptTranslation;
		PromptDesc = L"passphrase";
	}
	else if (PromptKind == pkTIS)
	{
		static const TPuttyTranslation TISInstructionTranslation[] = {
			{ L"Using TIS authentication.%", TIS_INSTRUCTION },
		};
		static const TPuttyTranslation TISPromptTranslation[] = {
			{ L"Response: ", PROMPT_PROMPT },
		};

		InstructionTranslation = TISInstructionTranslation;
		PromptTranslation = TISPromptTranslation;
		PromptDesc = L"tis";
	}
	else if (PromptKind == pkCryptoCard)
	{
		static const TPuttyTranslation CryptoCardInstructionTranslation[] = {
			{ L"Using CryptoCard authentication.%", CRYPTOCARD_INSTRUCTION },
		};
		static const TPuttyTranslation CryptoCardPromptTranslation[] = {
			{ L"Response: ", PROMPT_PROMPT },
		};

		InstructionTranslation = CryptoCardInstructionTranslation;
		PromptTranslation = CryptoCardPromptTranslation;
		PromptDesc = L"cryptocard";
	}
	else if (PromptKind == pkKeybInteractive)
	{
		static const TPuttyTranslation KeybInteractiveInstructionTranslation[] = {
			{ L"Using keyboard-interactive authentication.%", KEYBINTER_INSTRUCTION },
		};
		static const TPuttyTranslation KeybInteractivePromptTranslation[] = {
			// as used by Linux-PAM (pam_exec/pam_exec.c, libpam/pam_get_authtok.c,
			// pam_unix/pam_unix_auth.c, pam_userdb/pam_userdb.c)
			{ L"Password: ", PASSWORD_PROMPT },
		};

		InstructionTranslation = KeybInteractiveInstructionTranslation;
		PromptTranslation = KeybInteractivePromptTranslation;
		PromptDesc = L"keyboard interactive";
	}
	else if (PromptKind == pkPassword)
	{
		DebugAssert(Prompts->size() == 1);
		Prompts->at(0) = LoadStr(PASSWORD_PROMPT);
		PromptDesc = L"password";
	}
	else if (PromptKind == pkNewPassword)
	{
		// Can be tested with WS_FTP server
		static const TPuttyTranslation NewPasswordPromptTranslation[] = {
			{ L"Current password (blank for previously entered password): ", NEW_PASSWORD_CURRENT_PROMPT },
			{ L"Enter new password: ", NEW_PASSWORD_NEW_PROMPT },
			{ L"Confirm new password: ", NEW_PASSWORD_CONFIRM_PROMPT },
		};
		PromptTranslation = NewPasswordPromptTranslation;
		PromptTranslationCount = LENOF(NewPasswordPromptTranslation);
		PromptDesc = L"new password";
	}
	else if (PromptKind == pkProxyAuth)
	{
		static const TPuttyTranslation ProxyAuthPromptTranslation[] = {
			{ L"Proxy username: ", PROXY_AUTH_USERNAME_PROMPT },
			{ L"Proxy password: ", PROXY_AUTH_PASSWORD_PROMPT },
		};
		PromptTranslation = ProxyAuthPromptTranslation;
		PromptTranslationCount = LENOF(ProxyAuthPromptTranslation);
		PromptDesc = L"proxy authentication";
	}
	else
	{
		PromptDesc = L"unknown";
		DebugFail();
	}

	UnicodeString InstructionsLog =
		(Instructions.IsEmpty() ? UnicodeString(L"<no instructions>") : FORMAT(L"\"%ls\"", Instructions.c_str()));
	UnicodeString PromptsLog =
		(Prompts->size() > 0 ? FORMAT(L"\"%ls\"", Prompts->at(0).c_str()) : UnicodeString(L"<no prompt>")) +
		(Prompts->size() > 1 ? FORMAT(L" + %d more", Prompts->size() - 1) : UnicodeString());
	LogEvent(FORMAT(L"Prompt (%ls, \"%ls\", %ls, %ls)", PromptDesc.c_str(), AName.c_str(), InstructionsLog.c_str(), PromptsLog.c_str()));

	Name = Name.Trim();

	if (InstructionTranslation != NULL)
	{
		TranslatePuttyMessage(InstructionTranslation, 1, Instructions);
	}

	// some servers add leading or trailing blank line to make the prompt look prettier
	// on terminal console
	Instructions = Instructions.Trim();

	for (size_t Index = 0; Index < Prompts->size(); Index++)
	{
		UnicodeString Prompt = Prompts->at(Index);
		if (PromptTranslation != NULL)
		{
			TranslatePuttyMessage(PromptTranslation, PromptTranslationCount, Prompt);
		}
		// some servers add leading blank line to make the prompt look prettier
		// on terminal console
		Prompts->at(Index) = Prompt.Trim();
	}

	bool Result = false;
	if ((PromptKind == pkTIS) || (PromptKind == pkCryptoCard) ||
		(PromptKind == pkKeybInteractive))
	{
		if (FSessionData->FAuthKIPassword && !FSessionData->FPassword.IsEmpty() &&
			!FStoredPasswordTriedForKI && (Prompts->size() == 1) /*&&
			FLAGCLEAR(int(Prompts->Objects[0]), pupEcho)*/)
		{
			LogEvent(L"Using stored password.");
		//	FUI->Information(LoadStr(AUTH_PASSWORD));
			Result = true;
			Results->at(0) = NormalizeString(FSessionData->FPassword);
			FStoredPasswordTriedForKI = true;
		}
		else if (Instructions.IsEmpty() && !InstructionsRequired && (Prompts->size() == 0))
		{
			LogEvent(L"Ignoring empty SSH server authentication request");
			Result = true;
		}
	}
	else if (PromptKind == pkPassword)
	{
		if (!FSessionData->FPassword.IsEmpty() && !FStoredPasswordTried)
		{
			LogEvent(L"Using stored password.");
		//	FUI->Information(LoadStr(AUTH_PASSWORD));
			Result = true;
			Results->at(0) = NormalizeString(FSessionData->FPassword);
			FStoredPasswordTried = true;
		}
	}
	else if (PromptKind == pkPassphrase)
	{
		if (!FSessionData->FPassphrase.IsEmpty() && !FStoredPassphraseTried)
		{
			LogEvent(L"Using configured passphrase.");
			Result = true;
			Results->at(0) = FSessionData->FPassphrase;
			FStoredPassphraseTried = true;
		}
	}
	else if (PromptKind == pkNewPassword)
	{
		if (FSessionData->FChangePassword)
		{
		//	FUI->Information(LoadStr(AUTH_CHANGING_PASSWORD));

			if (!FSessionData->FPassword.IsEmpty() && !FSessionData->FNewPassword.IsEmpty() && !FStoredPasswordTried)
			{
				LogEvent(L"Using stored password and new password.");
				Result = true;
				DebugAssert(Results->size() == 3);
				Results->at(0) = NormalizeString(FSessionData->FPassword);
				Results->at(1) = NormalizeString(FSessionData->FNewPassword);
				Results->at(2) = NormalizeString(FSessionData->FNewPassword);
				FStoredPasswordTried = true;
			}
		}
	}

	if (!Result)
	{
		LogEvent(L"Prompting user for the credentials.");
	//	Result = FUIPromptUser(FSessionData,
	//		PromptKind, Name, Instructions, Prompts, Results);

		if (!Result)
		{
			LogEvent(L"Prompt cancelled.");
			FAuthenticationCancelled = true;
		}
		else
		{
			if ((Prompts->size() >= 1) /*&&
				(FLAGSET(int(Prompts->Objects[0]), pupEcho) || Configuration->LogSensitive)*/)
			{
				LogEvent(FORMAT(L"Response: \"%ls\"", Results->at(0).c_str()));
			}
			else
			{
				LogEvent(L"Prompt responded.");
			}

			if ((PromptKind == pkUserName) && (Prompts->size() == 1))
			{
				FUserName = Results->at(0);
			}
		}
	}

	return Result;
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::GotHostKey()
{
	// due to re-key GotHostKey() may be called again later during session
	if (!FAuthenticating && !FAuthenticated)
	{
		FAuthenticating = true;
		if (!FSessionData->FChangePassword)
		{
		//	FUI->Information(LoadStr(STATUS_AUTHENTICATE));
		}
	}
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::CWrite(const char *Data, size_t Length)
{
	// some messages to stderr may indicate that something has changed with the
	// session, so reset the session info
	ResetSessionInfo();

	// We send only whole line at once, so we have to cache incoming data
	FCWriteTemp += DeleteChar(ConvertFromPutty(Data, Length), L'\r');

	UnicodeString Line;
	// Do we have at least one complete line in std error cache?
	while (FCWriteTemp.Pos(L"\n") > 0)
	{
		UnicodeString Line = CutToChar(FCWriteTemp, L'\n', false);

	//	FLog->Add(llStdError, Line);

		if (FAuthenticating)
		{
			TranslateAuthenticationMessage(Line);
			FAuthenticationLog += (FAuthenticationLog.IsEmpty() ? L"" : L"\n") + Line;
		}

	//	FUI->Information(Line);
	}
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::RegisterReceiveHandler(void(*Handler)(void*))
{
	DebugAssert(FOnReceive == NULL);
	FOnReceive = Handler;
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::UnregisterReceiveHandler(void(*Handler)(void*))
{
	DebugAssert(FOnReceive == Handler);
	DebugUsedParam(Handler);
	FOnReceive = NULL;
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::FromBackend(const unsigned char *Data, size_t Length)
{
	// Note that we do not apply ConvertFromPutty to Data yet (as opposite to CWrite).
	// as there's no use for this atm.
	CheckConnection();

//	if (Configuration->ActualLogProtocol >= 1)
	{
		LogEvent(FORMAT(L"Received %u bytes", static_cast<int>(Length)));
	}

	// Following is taken from scp.c from_backend() and modified

	const unsigned char *p = Data;
	size_t Len = Length;

	// with event-select mechanism we can now receive data even before we
	// actually expect them (OutPtr can be NULL)

	if ((OutPtr != NULL) && (OutLen > 0) && (Len > 0))
	{
		size_t Used = OutLen;
		if (Used > Len) Used = Len;
		memmove(OutPtr, p, Used);
		OutPtr += Used; OutLen -= Used;
		p += Used; Len -= Used;
	}

	if (Len > 0)
	{
		if (PendSize < PendLen + Len)
		{
			PendSize = PendLen + Len + 4096;
			Pending = (unsigned char *)
				(Pending ? srealloc(Pending, PendSize) : smalloc(PendSize));
			if (!Pending) FatalError(L"Out of memory");
		}
		memmove(Pending + PendLen, p, Len);
		PendLen += Len;
	}

	if (FOnReceive != NULL)
	{
		if (!FFrozen)
		{
			FFrozen = true;
			try
			{
				do
				{
					FDataWhileFrozen = false;
					FOnReceive(NULL);
				}
				while (FDataWhileFrozen);
			}
			catch( ESshFatal& e )
			{
				throw( e );
			}
		//	__finally
		//	{
				FFrozen = false;
		//	}
		}
		else
		{
			FDataWhileFrozen = true;
		}
	}
}
//---------------------------------------------------------------------------
bool /*__fastcall*/ TSecureShell::Peek(unsigned char *&Buf, int Len)
{
	bool Result = (int(PendLen) >= Len);

	if (Result)
	{
		Buf = Pending;
	}

	return Result;
}
//---------------------------------------------------------------------------
int /*__fastcall*/ TSecureShell::Receive(unsigned char *Buf, int Len)
{
	CheckConnection();

	if (Len > 0)
	{
		// Following is taken from scp.c ssh_scp_recv() and modified

		OutPtr = Buf;
		OutLen = Len;

		try
		{
			/*
			 * See if the pending-input block contains some of what we
			 * need.
			 */
			if (PendLen > 0)
			{
				size_t PendUsed = PendLen;
				if (PendUsed > OutLen)
				{
					PendUsed = OutLen;
				}
				memmove(OutPtr, Pending, PendUsed);
				memmove(Pending, Pending + PendUsed, PendLen - PendUsed);
				OutPtr += PendUsed;
				OutLen -= PendUsed;
				PendLen -= PendUsed;
				if (PendLen == 0)
				{
					PendSize = 0;
					sfree(Pending);
					Pending = NULL;
				}
			}

			while (OutLen > 0)
			{
			//	if (Configuration->ActualLogProtocol >= 1)
			//	{
					LogEvent(FORMAT(L"Waiting for another %u bytes", static_cast<unsigned int>(OutLen)));
			//	}
				WaitForData();
			}

			// This seems ambiguous
			if (Len <= 0) FatalError(LoadStr(LOST_CONNECTION));
		}
		catch( ESshFatal& e )
		{
			throw( e );
		}
	//	__finally
	//	{
			OutPtr = NULL;
	//	}
	}
//	if (Configuration->ActualLogProtocol >= 1)
//	{
		LogEvent(FORMAT(L"Read %u bytes (%d pending)",
			static_cast<unsigned int>(Len), static_cast<int>(PendLen)));
//	}
	return Len;
}
//---------------------------------------------------------------------------
std::wstring /*__fastcall*/ TSecureShell::ReceiveLine()
{
	unsigned Index;
	RawByteString Line;
	bool EOL = false;

	do
	{
		// If there is any buffer of received chars
		if (PendLen > 0)
		{
			Index = 0;
			// Repeat until we walk thru whole buffer or reach end-of-line
			while ((Index < PendLen) && (!Index || (Pending[Index - 1] != '\n')))
			{
				Index++;
			}
			EOL = (bool)(Index && (Pending[Index - 1] == '\n'));
			size_t PrevLen = Line.Length();
			Line.SetLength(PrevLen + Index);
			Receive(reinterpret_cast<unsigned char*>(Line.c_str()) + PrevLen, Index);
		}

		// If buffer doesn't contain end-of-line character
		// we read one more which causes receiving new buffer of chars
		if (!EOL)
		{
			unsigned char Ch;
			Receive(&Ch, 1);
			Line += static_cast<char>(Ch);
			EOL = (static_cast<char>(Ch) == '\n');
		}
	}
	while (!EOL);

	// We don't want end-of-line character
	if(!Line.IsEmpty())
		Line.SetLength(Line.Length() - 1);

	UnicodeString Result = ConvertInput(Line);
	CaptureOutput(llOutput, Result);

	return Result.get();
}
//---------------------------------------------------------------------------
UnicodeString /*__fastcall*/ TSecureShell::ConvertInput(const RawByteString& Input)
{
	UnicodeString Result;
	if (FUtfStrings)
	{
		Result = UTF8ToString(Input);
	}
	else
	{
		Result = AnsiToString(Input);
	}
	return Result;
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::SendSpecial(int Code)
{
//	if (Configuration->ActualLogProtocol >= 0)
//	{
		LogEvent(FORMAT(L"Sending special code: %d", Code));
//	}
	CheckConnection();
	backend_special(FBackendHandle, (SessionSpecialCode)Code, 0);
	CheckConnection();
	FLastDataSent = Now();
}
//---------------------------------------------------------------------------
unsigned int /*__fastcall*/ TSecureShell::TimeoutPrompt(TQueryParamsTimerEvent PoolEvent)
{
	FWaiting++;

	unsigned int Answer;
//	try
	{
		/*TQueryParams Params(qpFatalAbort | qpAllowContinueOnError | qpIgnoreAbort);
		Params.HelpKeyword = HELP_MESSAGE_HOST_IS_NOT_COMMUNICATING;
		Params.Timer = 500;
		Params.TimerEvent = PoolEvent;
		Params.TimerMessage = MainInstructionsFirstParagraph(FMTLOAD(TIMEOUT_STILL_WAITING3, (FSessionData->Timeout)));
		Params.TimerAnswers = qaAbort;
		Params.TimerQueryType = qtInformation;*/
	//	if (FConfiguration->SessionReopenAutoStall > 0)
	//	{
	//		Params.Timeout = FConfiguration->SessionReopenAutoStall;
	//		Params.TimeoutAnswer = qaAbort;
	//		Params.TimeoutResponse = qaNo;
	//	}
	//	Answer = FUI->QueryUser(MainInstructions(FMTLOAD(CONFIRM_PROLONG_TIMEOUT3, (FSessionData->Timeout))),
	//		NULL, qaRetry | qaAbort, &Params);
//		auto res = MessageBox(pWorker->getHwnd(), MainInstructions(FMTLOAD(CONFIRM_PROLONG_TIMEOUT3,
//			(IntToStr(FSessionData->FTimeout)))).c_str(), L"PuTTY Connection Timed Out", MB_ICONQUESTION | MB_ABORTRETRYIGNORE);
		Answer = /*(res == IDABORT ? qaAbort : (res == IDRETRY ?*/ qaRetry /*: qaOK))*/;
	}
//	__finally
	{
		FWaiting--;
	}
	return Answer;
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::SendBuffer(unsigned int& Result)
{
	// for comments see PoolForData
	if (!FActive)
	{
		Result = qaRetry;
	}
	else
	{
		try
		{
			if (backend_sendbuffer(FBackendHandle) <= MAX_BUFSIZE)
			{
				Result = qaOK;
			}
		}
		catch (...)
		{
			Result = qaRetry;
		}
	}
}
//---------------------------------------------------------------------------
void TSecureShell::TimeoutAbort(unsigned int Answer, bool Sending)
{
	UnicodeString CounterName;
	if (Mode == ssmUploading)
	{
		CounterName = Sending ? L"TimeoutUploadingSending" : L"TimeoutUploadingReceiving";
	}
	else if (Mode == ssmDownloading)
	{
		CounterName = Sending ? L"TimeoutDownloadingSending" : L"TimeoutDownloadingReceiving";
	}
	if (!CounterName.IsEmpty())
	{
	//	Configuration->Usage->Inc(CounterName);
	}

	FatalError(MainInstructions(LoadStr(Answer == qaAbort ? USER_TERMINATED : TIMEOUT_ERROR)));
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::DispatchSendBuffer(size_t BufSize)
{
	TDateTime Start = Now();
	do
	{
		CheckConnection();
	//	if (Configuration->ActualLogProtocol >= 1)
	//	{
			LogEvent(FORMAT(L"There are %zu bytes remaining in the send buffer, "
				"need to send at least another %zu bytes",
				BufSize, BufSize - MAX_BUFSIZE));
	//	}
		EventSelectLoop(100, false, NULL);
		BufSize = backend_sendbuffer(FBackendHandle);
	//	if (Configuration->ActualLogProtocol >= 1)
	//	{
			LogEvent(FORMAT(L"There are %zu bytes remaining in the send buffer", BufSize));
	//	}

		if (Now() - Start > (TDateTime)((double)(FSessionData->FTimeout) / SecsPerDay))
		{
			LogEvent(L"Waiting for dispatching send buffer timed out, asking user what to do.");
			unsigned int Answer = TimeoutPrompt(std::bind(&TSecureShell::SendBuffer, this, std::placeholders::_1));
			switch (Answer)
			{
			case qaRetry:
				Start = Now();
				break;

			case qaOK:
				BufSize = 0;
				break;

			default:
				DebugFail();
				// fallthru

			case qaAbort:
			case qaNo:
				TimeoutAbort(Answer, true);
				return;
			}
		}
	}
	while (BufSize > MAX_BUFSIZE);
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::Send(const char *Buf, size_t Len)
{
	CheckConnection();
	backend_send(FBackendHandle, const_cast<char *>(reinterpret_cast<const char *>(Buf)), Len);
	size_t BufSize = backend_sendbuffer(FBackendHandle);
//	if (Configuration->ActualLogProtocol >= 1)
//	{
		LogEvent(FORMAT(L"Sent %u bytes", static_cast<unsigned int>(Len)));
		LogEvent(FORMAT(L"There are %zu bytes remaining in the send buffer", BufSize));
//	}
	FLastDataSent = Now();
	// among other forces receive of pending data to free the servers's send buffer
	EventSelectLoop(0, false, NULL);

	if (BufSize > MAX_BUFSIZE)
	{
		DispatchSendBuffer(BufSize);
	}
	CheckConnection();
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::SendNull()
{
	LogEvent(L"Sending NULL.");
	const char Null = 0;
	Send(&Null, 1);
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::SendLine(const UnicodeString& Line)
{
	CheckConnection();
	RawByteString Buf;
	if (FUtfStrings)
	{
		Buf = RawByteString(UTF8String(Line));
	}
	else
	{
		Buf = RawByteString(AnsiString(Line));
	}
	Buf += "\n";

//	FLog->Add(llInput, Line);
	Send(/*reinterpret_cast<const unsigned char *>*/(Buf.c_str()), Buf.Length());
}
//---------------------------------------------------------------------------
std::vector<std::wstring> TSecureShell::SendCommandFull( TFSCommand cmd, const std::wstring& param1, const std::wstring& param2, const std::wstring& param3 )
{
	if( !pWorker || !pWorker->isRunning() )
		FatalError( "Aborted by user." );

	SendCommand( cmd, param1, param2, param3 );

	skipFirstLine();

	// TODO: what to do with return code?
	int returnCode = 0;

	return ReadCommandOutput( returnCode );
}
//---------------------------------------------------------------------------
void TSecureShell::SendCommand( TFSCommand cmd, const std::wstring& param1, const std::wstring& param2, const std::wstring& param3 )
{
	if( !pWorker || !pWorker->isRunning() )
		FatalError( "Aborted by user." );

	std::wstring cmdSrc = DefaultCommandSet[cmd].Command;

	// replace %ls with params
	wchar_t tpl[] = L"%ls";

	if( cmdSrc.find( tpl ) != std::wstring::npos )
		cmdSrc.replace( cmdSrc.find( tpl ), 3, param1 );
	if( cmdSrc.find( tpl ) != std::wstring::npos )
		cmdSrc.replace( cmdSrc.find( tpl ), 3, param2 );
	if( cmdSrc.find( tpl ) != std::wstring::npos )
		cmdSrc.replace( cmdSrc.find( tpl ), 3, param3 );

	UnicodeString retVar = FSessionData->FReturnVar.IsEmpty() ? L"0" : L"$" + FSessionData->FReturnVar;

	auto cmdStr = FORMAT( DefaultCommandSet[fsFirstLine].Command, SSH_FIRST_LINE );
	cmdStr += L"; ";
	cmdStr += cmdSrc;
	cmdStr += L"; ";
	cmdStr += FORMAT( DefaultCommandSet[fsLastLine].Command, SSH_LAST_LINE, retVar.c_str() );

	PrintDebug( "Sent command: %ls", cmdStr.c_str() );

	SendLine( cmdStr );
}
//---------------------------------------------------------------------------
std::vector<std::wstring> TSecureShell::ReadCommandOutput( int& returnCode )
{
	std::vector<std::wstring> response;
	std::wstring line;

	// read until "fsLastLine"
	while( pWorker && pWorker->isRunning() )
	{
		line = ReceiveLine();

		if( isLastLine( line ) )
		{
			returnCode = getReturnCode( line );
			break;
		}
		else
			response.push_back( line );
	}

	return response;
}
//---------------------------------------------------------------------------
int TSecureShell::getReturnCode( const std::wstring& line )
{
	int returnCode = 0;

	auto idx = line.find_last_of( LastLineSeparator );
	if( idx != std::wstring::npos )
	{
		try
		{
			returnCode = std::stoi( line.substr( idx + 1 ) );
			PrintDebug( "Return code: %d", returnCode );
		}
		catch( const std::invalid_argument& e )
		{
			e.what();
			PrintDebug( "Invalid return code '%ls'", line.substr( idx + 1 ).c_str() );
		}
	}
	else
		PrintDebug( "Invalid last line: %ls", line.c_str() );

	return returnCode;
}
//---------------------------------------------------------------------------
void TSecureShell::skipFirstLine()
{
	std::wstring line;

	// skip up to "fsFirstLine"
	while( pWorker && pWorker->isRunning() )
	{
		line = ReceiveLine();

		if( line.find( SSH_FIRST_LINE ) == std::wstring::npos )
			PrintDebug( "Skipping: %ls", line.c_str() );
		else
			break;
	}
}
//---------------------------------------------------------------------------
bool TSecureShell::isLastLine( const std::wstring& line )
{
	if( line.find( SSH_LAST_LINE ) != std::wstring::npos )
		return true;

	return false;
}
//---------------------------------------------------------------------------
int /*__fastcall*/ TSecureShell::TranslatePuttyMessage(
	const TPuttyTranslation *Translation, size_t Count, UnicodeString& Message,
	UnicodeString *HelpKeyword)
{
	int Result = -1;
	for (unsigned int Index = 0; Index < Count; Index++)
	{
		const wchar_t *Original = Translation[Index].Original;
		const wchar_t *Div = wcschr(Original, L'%');
		if (Div == NULL)
		{
			if (wcscmp(Message.c_str(), Original) == 0)
			{
				Message = LoadStr(Translation[Index].Translation);
				Result = int(Index);
				break;
			}
		}
		else
		{
			size_t OriginalLen = wcslen(Original);
			size_t PrefixLen = Div - Original;
			size_t SuffixLen = OriginalLen - PrefixLen - 1;
			if ((Message.Length() >= OriginalLen - 1) &&
				(wcsncmp(Message.c_str(), Original, PrefixLen) == 0) &&
				(wcsncmp(Message.c_str() + Message.Length() - SuffixLen, Div + 1, SuffixLen) == 0))
			{
				Message = FMTLOAD(Translation[Index].Translation,
					(Message.SubString(PrefixLen + 1, Message.Length() - PrefixLen - SuffixLen).TrimRight()));
				Result = int(Index);
				break;
			}
		}
	}

	if ((HelpKeyword != NULL) && (Result >= 0))
	{
		*HelpKeyword = Translation[Result].HelpKeyword;
	}

	return Result;
}
//---------------------------------------------------------------------------
int /*__fastcall*/ TSecureShell::TranslateAuthenticationMessage(
	UnicodeString& Message, UnicodeString *HelpKeyword)
{
	static const TPuttyTranslation Translation[] = {
		{ L"Using username \"%\".", AUTH_TRANSL_USERNAME },
		{ L"Using keyboard-interactive authentication.", AUTH_TRANSL_KEYB_INTER }, // not used anymore
		{ L"Authenticating with public key \"%\" from agent", AUTH_TRANSL_PUBLIC_KEY_AGENT },
		{ L"Authenticating with public key \"%\"", AUTH_TRANSL_PUBLIC_KEY },
		{ L"Authenticated using RSA key \"%\" from agent", AUTH_TRANSL_PUBLIC_KEY_AGENT },
		{ L"Wrong passphrase", AUTH_TRANSL_WRONG_PASSPHRASE },
		{ L"Wrong passphrase.", AUTH_TRANSL_WRONG_PASSPHRASE },
		{ L"Access denied", AUTH_TRANSL_ACCESS_DENIED },
		{ L"Trying public key authentication.", AUTH_TRANSL_TRY_PUBLIC_KEY },
		{ L"Server refused our public key.", AUTH_TRANSL_KEY_REFUSED/*, HELP_AUTH_TRANSL_KEY_REFUSED*/ }, // help mapping probably never used
		{ L"Server refused our key", AUTH_TRANSL_KEY_REFUSED/*, HELP_AUTH_TRANSL_KEY_REFUSED*/ }
	};

	int Result = TranslatePuttyMessage(Translation, LENOF(Translation), Message, HelpKeyword);

	if ((Result == 2) || (Result == 3) || (Result == 4))
	{
		FCollectPrivateKeyUsage = true;
	}

	return Result;
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::AddStdError(const char *Data, size_t Length)
{
	UnicodeString Str = ConvertInput(RawByteString(Data, Length));
	FStdError += Str;

	size_t P;
	Str = DeleteChar(Str, L'\r');
	// We send only whole line at once to log, so we have to cache
	// incoming std error data
	FStdErrorTemp += Str;
	UnicodeString Line;
	// Do we have at least one complete line in std error cache?
	while ((P = FStdErrorTemp.Pos(L"\n")) > 0)
	{
		Line = FStdErrorTemp.SubString(1, P-1);
		FStdErrorTemp.Delete(1, P);
		AddStdErrorLine(Line);
	}
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::AddStdErrorLine(const UnicodeString& Str)
{
	if (FAuthenticating)
	{
		FAuthenticationLog += (FAuthenticationLog.IsEmpty() ? L"" : L"\n") + Str;
	}
	CaptureOutput(llStdError, Str);
}
//---------------------------------------------------------------------------
const UnicodeString& /*__fastcall*/ TSecureShell::GetStdError()
{
	return FStdError;
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::ClearStdError()
{
	// Flush std error cache
	if (!FStdErrorTemp.IsEmpty())
	{
		if (FAuthenticating)
		{
			FAuthenticationLog +=
				(FAuthenticationLog.IsEmpty() ? L"" : L"\n") + FStdErrorTemp;
		}
		CaptureOutput(llStdError, FStdErrorTemp);
		FStdErrorTemp = L"";
	}
	FStdError = L"";
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::CaptureOutput(TLogLineType Type, const UnicodeString& Line)
{
	if (FOnCaptureOutput != NULL)
	{
		FOnCaptureOutput(const_cast<UnicodeString&>(Line).get(), (Type == llStdError) ? cotError : cotOutput);
	}
//	FLog->Add(Type, Line);
}
//---------------------------------------------------------------------------
int /*__fastcall*/ TSecureShell::TranslateErrorMessage(
	UnicodeString& Message, UnicodeString *HelpKeyword)
{
	static const TPuttyTranslation Translation[] = {
		{ L"Remote side unexpectedly closed network connection", UNEXPECTED_CLOSE_ERROR/*, HELP_UNEXPECTED_CLOSE_ERROR*/ },
		{ L"Network error: Connection refused", NET_TRANSL_REFUSED2/*, HELP_NET_TRANSL_REFUSED*/ },
		{ L"Network error: Connection reset by peer", NET_TRANSL_RESET/*, HELP_NET_TRANSL_RESET*/ },
		{ L"Network error: Connection timed out", NET_TRANSL_TIMEOUT2/*, HELP_NET_TRANSL_TIMEOUT*/ },
		{ L"Network error: No route to host", NET_TRANSL_NO_ROUTE2/*, HELP_NET_TRANSL_NO_ROUTE*/ },
		{ L"Network error: Software caused connection abort", NET_TRANSL_CONN_ABORTED/*, HELP_NET_TRANSL_CONN_ABORTED*/ },
		{ L"Host does not exist", NET_TRANSL_HOST_NOT_EXIST2/*, HELP_NET_TRANSL_HOST_NOT_EXIST*/ },
		{ L"Incoming packet was garbled on decryption", NET_TRANSL_PACKET_GARBLED/*, HELP_NET_TRANSL_PACKET_GARBLED*/ },
	};

	int Index = TranslatePuttyMessage(Translation, LENOF(Translation), Message, HelpKeyword);

	if ((Index == 0) || (Index == 1) || (Index == 2) || (Index == 3))
	{
		FNoConnectionResponse = true;
	}

	Message = ReplaceStr(Message, L"%HOST%", FSessionData->GetHostNameExpanded().c_str());

	return Index;
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::PuttyFatalError(UnicodeString Error)
{
	UnicodeString HelpKeyword;
	TranslateErrorMessage(Error, &HelpKeyword);
	if (!FClosed)
	{
		FatalError(Error, HelpKeyword);
	}
	else
	{
		LogEvent(FORMAT(L"Ignoring an error from the server while closing: %ls", Error.c_str()));
	}
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::FatalError(UnicodeString Error, UnicodeString HelpKeyword)
{
	if (FActive)
	{
		MessageBox(pWorker->getHwnd(), Error.c_str(), L"PuTTY Fatal Error", MB_ICONEXCLAMATION | MB_OK);

		// We log this instead of exception handler, because Close() would
		// probably cause exception handler to loose pointer to TShellLog()
		LogEvent(L"Attempt to close connection due to fatal exception:");
	//	Log->Add(llException, Msg);
	//	Log->AddException(E);

		if (FActive)
		{
			Close();
		}
	}

	throw ESshFatal(AnsiString(Error).c_str(), AnsiString(HelpKeyword).c_str());
}
//---------------------------------------------------------------------------
void /*__fastcall*/ /*inline*/ TSecureShell::LogEvent(const UnicodeString& Str)
{
	OutputDebugString(L"-->");
	OutputDebugString(Str.c_str());
	OutputDebugString(L"\n");
//	if (FLog->Logging)
//	{
//		FLog->Add(llMessage, Str);
//	}
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::SocketEventSelect(SOCKET Socket, HANDLE Event, bool Enable)
{
	int Events;

	if (Enable)
	{
		Events = (FD_CONNECT | FD_READ | FD_WRITE | FD_OOB | FD_CLOSE | FD_ACCEPT);
	}
	else
	{
		Events = 0;
	}

//	if (Configuration->ActualLogProtocol >= 2)
//	{
		LogEvent(FORMAT(L"Selecting events %d for socket %d", Events, (int)Socket));
//	}

	if (WSAEventSelect(Socket, (WSAEVENT)Event, Events) == SOCKET_ERROR)
	{
	//	if (Configuration->ActualLogProtocol >= 2)
	//	{
			LogEvent(FORMAT(L"Error selecting events %d for socket %d", Events, (int)Socket));
	//	}

		if (Enable)
		{
			FatalError(FMTLOAD(EVENT_SELECT_ERROR, (SysUtils::getErrorMessage(WSAGetLastError()))));
		}
	}
}
//---------------------------------------------------------------------------
bool TSecureShell::HasLocalProxy()
{
	return (FSessionData->FProxyMethod == pmCmd);
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::UpdateSocket(SOCKET value, bool Enable)
{
	if (!FActive && !Enable)
	{
		// no-op
		// Remove the branch eventually:
		// When TCP connection fails, PuTTY does not release the memory allocated for
		// socket. As a simple hack we call sk_tcp_close() in ssh.c to release the memory,
		// until they fix it better. Unfortunately sk_tcp_close calls do_select,
		// so we must filter that out.
	}
	else
	{
		DebugAssert(value);
		DebugAssert((FActive && (FSocket == value)) || (!FActive && Enable));

		// filter our "local proxy" connection, which have no socket
		if (value != INVALID_SOCKET)
		{
			SocketEventSelect(value, FSocketEvent, Enable);
		}
		else
		{
			DebugAssert(HasLocalProxy());
		}

		if (Enable)
		{
			FSocket = value;
			FActive = true;
		}
		else
		{
			FSocket = INVALID_SOCKET;
			Discard();
		}
	}
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::UpdatePortFwdSocket(SOCKET value, bool Enable)
{
	if (value != INVALID_SOCKET)
	{
	//	if (Configuration->ActualLogProtocol >= 2)
	//	{
			LogEvent(FORMAT(L"Updating forwarding socket %d (%d)", (int)value, (int)Enable));
	//	}

		SocketEventSelect(value, FSocketEvent, Enable);

		if (Enable)
		{
			FPortFwdSockets.insert(value);
		}
		else
		{
			FPortFwdSockets.erase(value);
		}
	}
	else
	{
		DebugAssert(FSessionData->FAgentFwd);
	}
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::FreeBackend()
{
	if (FBackendHandle != NULL)
	{
		backend_free(FBackendHandle);
		FBackendHandle = NULL;

		// After destroying backend, ic_pktin_free should be the only remaining callback.
		if (is_idempotent_callback_pending(FCallbackSet.get(), FCallbackSet->ic_pktin_free))
		{
			// This releases the callback and should be noop otherwise.
			run_toplevel_callbacks(FCallbackSet.get());
		}

		sfree(FCallbackSet->ic_pktin_free);
		FCallbackSet->ic_pktin_free = NULL;

		// Not checking that cbcurr is NULL. It may be non-null, when (fatal?) exception occurs, while the callback is called.
		FCallbackSet->cbcurr = NULL;
		DebugAssert(FCallbackSet->cbhead == NULL);
		DebugAssert(FCallbackSet->cbtail == NULL);

		if (FCallbackSet->pktin_freeq_head != NULL)
		{
			DebugAssert(FCallbackSet->pktin_freeq_head->next == FCallbackSet->pktin_freeq_head);
			sfree(FCallbackSet->pktin_freeq_head);
			FCallbackSet->pktin_freeq_head = NULL;
		}

		if (FCallbackSet->handlewaits_tree_real != NULL)
		{
			DebugAssert(count234(FCallbackSet->handlewaits_tree_real) <= 1);
			while (count234(FCallbackSet->handlewaits_tree_real) > 0)
			{
				HandleWait *AHandleWait = static_cast<HandleWait*>(index234(FCallbackSet->handlewaits_tree_real, 0));
				delete_handle_wait(FCallbackSet.get(), AHandleWait);
			}

			freetree234(FCallbackSet->handlewaits_tree_real);
			FCallbackSet->handlewaits_tree_real = NULL;
		}

		if (FCallbackSet->ready_event != INVALID_HANDLE_VALUE)
		{
			CloseHandle(FCallbackSet->ready_event);
			FCallbackSet->ready_event = INVALID_HANDLE_VALUE;
		}
	}
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::Discard()
{
	bool WasActive = FActive;
	FActive = false;
	FOpened = false;

	if (WasActive)
	{
	//	FUI->Closed();
	}
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::Close()
{
	LogEvent(L"Closing connection.");
	DebugAssert(FActive);
	FClosed = true;

	// Without main channel SS_EOF is ignored and would get stuck waiting for exit code.
	if ((backend_exitcode(FBackendHandle) < 0) && winscp_query(FBackendHandle, WINSCP_QUERY_MAIN_CHANNEL))
	{
		// this is particularly necessary when using local proxy command
		// (e.g. plink), otherwise it hangs in sk_localproxy_close
		SendSpecial(SS_EOF);
		// Try waiting for the EOF exchange to complete (among other to avoid packet queue memory leaks)
		int Timeout = 500;
		while ((backend_exitcode(FBackendHandle) < 0) && (Timeout > 0))
		{
			const int Step = 100;
			if (!EventSelectLoop(Step, false, NULL))
			{
				Timeout -= Step;
			}
		}
	}

	FreeBackend();

	Discard();
}
//---------------------------------------------------------------------------
inline void /*__fastcall*/ TSecureShell::CheckConnection(int Message)
{
	if (!FActive || (backend_exitcode(FBackendHandle) >= 0))
	{
		UnicodeString Str;
		UnicodeString HelpKeyword;

		int ExitCode = backend_exitcode(FBackendHandle);
		if (Message >= 0)
		{
			Str = LoadStr(Message);
		}
		else
		{
			if ((ExitCode == 0) && FAuthenticationCancelled)
			{
				// This should be improved to check if the prompt for specific credential 
				// was cancelled after it was unsuccessfully answered before 
				if (GetStoredCredentialsTried())
				{
					Str = LoadStr(AUTHENTICATION_FAILED);
				}
				else
				{
					Str = LoadStr(CREDENTIALS_NOT_SPECIFIED);
				}
				// The 0 code is not coming from the server 
				ExitCode = -1;
			}
			else
			{
				Str = LoadStr(NOT_CONNECTED);
			//	HelpKeyword = HELP_NOT_CONNECTED;
			}
		}

		Str = MainInstructions(Str);

		if (ExitCode >= 0)
		{
			Str += L" " + FMTLOAD(SSH_EXITCODE, (IntToStr(ExitCode)));
		}
		if (!FClosed)
		{
			FatalError(Str, HelpKeyword);
		}
		else
		{
			LogEvent(FORMAT(L"Ignoring closed connection: %ls", Str.c_str()));
		}
	}
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::PoolForData(WSANETWORKEVENTS& Events, unsigned int& Result)
{
	if (!FActive)
	{
		// see comment below
		Result = qaRetry;
	}
	else
	{
		try
		{
		//	if (Configuration->ActualLogProtocol >= 2)
		//	{
				LogEvent(L"Pooling for data in case they finally arrives");
		//	}

			// in extreme condition it may happen that send buffer is full, but there
			// will be no data coming and we may not empty the send buffer because we
			// do not process FD_WRITE until we receive any FD_READ
			if (EventSelectLoop(0, false, &Events))
			{
				LogEvent(L"Data has arrived, closing query to user.");
				Result = qaOK;
			}
		}
		catch (...)
		{
			// if we let the exception out, it may popup another message dialog
			// in whole event loop, another call to PoolForData from original dialog
			// would be invoked, leading to an infinite loop.
			// by retrying we hope (that probably fatal) error would repeat in WaitForData.
			// anyway now once no actual work is done in EventSelectLoop,
			// hardly any exception can occur actually
			Result = qaRetry;
		}
	}
}
//---------------------------------------------------------------------------
class TPoolForDataEvent
{
public:
	/*__fastcall*/ TPoolForDataEvent(TSecureShell *SecureShell, WSANETWORKEVENTS& Events) :
		FSecureShell(SecureShell),
		FEvents(Events)
	{
	}

	void /*__fastcall*/ PoolForData(unsigned int& Result)
	{
		FSecureShell->PoolForData(FEvents, Result);
	}

private:
	TSecureShell *FSecureShell;
	WSANETWORKEVENTS& FEvents;
};
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::WaitForData()
{
	// see winsftp.c
	bool IncomingData;

	do
	{
	//	if (Configuration->ActualLogProtocol >= 2)
	//	{
			LogEvent(L"Looking for incoming data");
	//	}

		IncomingData = EventSelectLoop(FSessionData->FTimeout * MSecsPerSec, true, NULL);
		if (!IncomingData)
		{
			DebugAssert(FWaitingForData == 0);
			TAutoNestingCounter NestingCounter(FWaitingForData);

			WSANETWORKEVENTS Events;
			memset(&Events, 0, sizeof(Events));
			TPoolForDataEvent Event(this, Events);

			LogEvent(L"Waiting for data timed out, asking user what to do.");
			unsigned int Answer = TimeoutPrompt(std::bind(&TPoolForDataEvent::PoolForData, Event, std::placeholders::_1));
			switch (Answer)
			{
			case qaRetry:
				// noop
				break;

			case qaOK:
				// read event was already captured in PoolForData(),
				// make sure we do not try to select it again as it would timeout
				// unless another read event occurs
				IncomingData = true;
				HandleNetworkEvents(FSocket, Events);
				break;

			default:
				DebugFail();
				// fallthru

			case qaAbort:
			case qaNo:
				TimeoutAbort(Answer, false);
				break;
			}
		}
	}
	while (!IncomingData);
}
//---------------------------------------------------------------------------
bool /*__fastcall*/ TSecureShell::SshFallbackCmd() const
{
	return ssh_fallback_cmd(FBackendHandle);
}
//---------------------------------------------------------------------------
bool /*__fastcall*/ TSecureShell::EnumNetworkEvents(SOCKET Socket, WSANETWORKEVENTS& Events)
{
//	if (Configuration->ActualLogProtocol >= 2)
//	{
		LogEvent(FORMAT(L"Enumerating network events for socket %d", (int)Socket));
//	}

	// see winplink.c
	WSANETWORKEVENTS AEvents;
	if (WSAEnumNetworkEvents(Socket, NULL, &AEvents) == 0)
	{
		noise_ultralight(NOISE_SOURCE_IOID, (unsigned long)Socket);
		noise_ultralight(NOISE_SOURCE_IOID, AEvents.lNetworkEvents);

		Events.lNetworkEvents |= AEvents.lNetworkEvents;
		for (int Index = 0; Index < FD_MAX_EVENTS; Index++)
		{
			if (AEvents.iErrorCode[Index] != 0)
			{
				Events.iErrorCode[Index] = AEvents.iErrorCode[Index];
			}
		}

	//	if (Configuration->ActualLogProtocol >= 2)
	//	{
			LogEvent(FORMAT(L"Enumerated %d network events making %d cumulative events for socket %d",
				(int)AEvents.lNetworkEvents, (int)Events.lNetworkEvents, (int)Socket));
	//	}
	}
	else
	{
	//	if (Configuration->ActualLogProtocol >= 2)
	//	{
			LogEvent(FORMAT(L"Error enumerating network events for socket %d", (int)Socket));
	//	}
	}

	bool Result =
		FLAGSET(Events.lNetworkEvents, FD_READ) ||
		FLAGSET(Events.lNetworkEvents, FD_CLOSE);
	return Result;
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::HandleNetworkEvents(SOCKET Socket, WSANETWORKEVENTS& Events)
{
	static const struct { int Bit, Mask; const wchar_t *Desc; } EventTypes[] =
	{
		{ FD_WRITE_BIT, FD_WRITE, L"write" },
		{ FD_OOB_BIT, FD_OOB, L"oob" },
		{ FD_ACCEPT_BIT, FD_ACCEPT, L"accept" },
		{ FD_CONNECT_BIT, FD_CONNECT, L"connect" },
		{ FD_CLOSE_BIT, FD_CLOSE, L"close" },
		// Read goes last, as it can cause an exception.
		// Though a correct solution would be to process all events, even if one causes exception
		{ FD_READ_BIT, FD_READ, L"read" },
	};

	for (unsigned int Event = 0; Event < LENOF(EventTypes); Event++)
	{
		if (FLAGSET(Events.lNetworkEvents, EventTypes[Event].Mask))
		{
			int Err = Events.iErrorCode[EventTypes[Event].Bit];
		//	if (Configuration->ActualLogProtocol >= 2)
		//	{
				LogEvent(FORMAT(L"Handling network %ls event on socket %d with error %d",
					EventTypes[Event].Desc, (int)Socket, Err));
		//	}
		//	#pragma option push -w-prc
			LPARAM SelectEvent = WSAMAKESELECTREPLY(EventTypes[Event].Mask, Err);
		//	#pragma option pop
			select_result((WPARAM)Socket, SelectEvent);
			CheckConnection();
		}
	}
}
//---------------------------------------------------------------------------
bool /*__fastcall*/ TSecureShell::ProcessNetworkEvents(SOCKET Socket)
{
	WSANETWORKEVENTS Events;
	memset(&Events, 0, sizeof(Events));
	bool Result = EnumNetworkEvents(Socket, Events);
	HandleNetworkEvents(Socket, Events);
	return Result;
}
//---------------------------------------------------------------------------
bool /*__fastcall*/ TSecureShell::EventSelectLoop(unsigned int MSec, bool ReadEventRequired,
	WSANETWORKEVENTS *Events)
{
	bool Result = false;

	do
	{
	//	if (Configuration->ActualLogProtocol >= 2)
	//	{
			LogEvent(L"Looking for network events");
	//	}
		unsigned int TicksBefore = GetTickCount();
		HandleWaitList *WaitList = NULL;

		try
		{
			unsigned int Timeout = MSec;

			unsigned int WaitResult;
			do
			{
				CheckConnection();
				unsigned int TimeoutStep = min(GUIUpdateInterval, Timeout);
				if (toplevel_callback_pending(GetCallbackSet()))
				{
					TimeoutStep = 0;
				}
				Timeout -= TimeoutStep;
				if (WaitList != NULL)
				{
					handle_wait_list_free(WaitList);
				}
				// It returns only busy handles, so the set can change with every call to run_toplevel_callbacks.
				WaitList = get_handle_wait_list(FCallbackSet.get());
				DebugAssert(WaitList->nhandles < MAXIMUM_WAIT_OBJECTS);
				WaitList->handles[WaitList->nhandles] = FSocketEvent;
				WaitResult = WaitForMultipleObjects(WaitList->nhandles + 1, WaitList->handles, FALSE, TimeoutStep);
			//	FUI->ProcessGUI();
				// run_toplevel_callbacks can cause processing of pending raw data, so:
				// 1) Check for changes in our pending buffer - wait criteria in Receive()
				int PrevDataLen = (-static_cast<int>(OutLen) + static_cast<int>(PendLen));
				// 2) Changes in session state - wait criteria in Init()
				unsigned int HadMainChannel = winscp_query(FBackendHandle, WINSCP_QUERY_MAIN_CHANNEL);
				if (run_toplevel_callbacks(GetCallbackSet()) &&
					(((-static_cast<int>(OutLen) + static_cast<int>(PendLen)) > PrevDataLen) ||
					(HadMainChannel != winscp_query(FBackendHandle, WINSCP_QUERY_MAIN_CHANNEL))))
				{
					// Note that we still may process new network event now
					Result = true;
				}
			} while ((WaitResult == WAIT_TIMEOUT) && (Timeout > 0) && !Result);

			if (WaitResult < WAIT_OBJECT_0 + WaitList->nhandles)
			{
				if (handle_wait_activate(FCallbackSet.get(), WaitList, WaitResult - WAIT_OBJECT_0))
				{
					Result = true;
				}
			}
			else if (WaitResult == WAIT_OBJECT_0 + WaitList->nhandles)
			{
			//	if (Configuration->ActualLogProtocol >= 1)
			//	{
					LogEvent(L"Detected network event");
			//	}

				DebugAssert(FSocket != INVALID_SOCKET);
				if (Events == NULL)
				{
					if (ProcessNetworkEvents(FSocket))
					{
						Result = true;
					}
				}
				else
				{
					if (EnumNetworkEvents(FSocket, *Events))
					{
						Result = true;
					}
				}

				{
					TSockets::iterator i = FPortFwdSockets.begin();
					while (i != FPortFwdSockets.end())
					{
						ProcessNetworkEvents(*i);
						i++;
					}
				}
			}
			else if (WaitResult == WAIT_TIMEOUT)
			{
			//	if (Configuration->ActualLogProtocol >= 2)
			//	{
					LogEvent(L"Timeout waiting for network events");
			//	}

				MSec = 0;
			}
			else
			{
			//	if (Configuration->ActualLogProtocol >= 2)
			//	{
					LogEvent(FORMAT(L"Unknown waiting result %d", (int)WaitResult));
			//	}

				MSec = 0;
			}
		}
		catch( ESshFatal& e )
		{
			throw( e );
		}
	//	__finally
	//	{
			if (WaitList != NULL)
			{
				handle_wait_list_free(WaitList);
			}
	//	}

		unsigned int TicksAfter = GetTickCount();
		// ticks wraps once in 49.7 days
		if (TicksBefore < TicksAfter)
		{
			unsigned int Ticks = TicksAfter - TicksBefore;
			if (Ticks > MSec)
			{
				MSec = 0;
			}
			else
			{
				MSec -= Ticks;
			}
		}

		if ((FSocket != INVALID_SOCKET) &&
			(FSendBuf > 0) && (TicksAfter - FLastSendBufferUpdate >= 1000))
		{
			DWORD BufferLen = 0;
			DWORD outLen = 0;
			if (WSAIoctl(FSocket, SIO_IDEAL_SEND_BACKLOG_QUERY, NULL, 0, &BufferLen, sizeof(BufferLen), &outLen, 0, 0) == 0)
			{
				DebugAssert(outLen == sizeof(BufferLen));
				if (FSendBuf < static_cast<int>(BufferLen))
				{
					LogEvent(FORMAT(L"Increasing send buffer from %d to %d", FSendBuf, static_cast<int>(BufferLen)));
					FSendBuf = BufferLen;
					setsockopt(FSocket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char *>(&BufferLen), sizeof(BufferLen));
				}
			}
			FLastSendBufferUpdate = TicksAfter;
		}
	}
	while (ReadEventRequired && (MSec > 0) && !Result);

	return Result;
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::Idle(unsigned int MSec)
{
	noise_regular();

	winscp_query(FBackendHandle, WINSCP_QUERY_TIMER);

	// if we are actively waiting for data in WaitForData,
	// do not read here, otherwise we swallow read event and never wake
	if (FWaitingForData <= 0)
	{
		EventSelectLoop(MSec, false, NULL);
	}
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::KeepAlive()
{
	if (FActive && (FWaiting == 0))
	{
		LogEvent(L"Sending null packet to keep session alive.");
		SendSpecial(SS_PING);
	}
	else
	{
		// defer next keepalive attempt
		FLastDataSent = Now();
	}
}
//---------------------------------------------------------------------------
unsigned long /*__fastcall*/ TSecureShell::MaxPacketSize()
{
	if (!FSessionInfoValid)
	{
		UpdateSessionInfo();
	}

	return winscp_query(FBackendHandle, WINSCP_QUERY_REMMAXPKT);
}
//---------------------------------------------------------------------------
UnicodeString /*__fastcall*/ TSecureShell::FormatKeyStr(UnicodeString KeyStr)
{
	size_t Index = 1;
	int Digits = 0;
	while (Index <= KeyStr.Length())
	{
		if (IsHex(KeyStr[Index]))
		{
			Digits++;
			if (Digits >= 16)
			{
				KeyStr.Insert(L" ", Index + 1);
				Index++;
				Digits = 0;
			}
		}
		else
		{
			Digits = 0;
		}
		Index++;
	}
	return KeyStr;
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::GetRealHost(UnicodeString& Host, int& Port)
{
	if (FSessionData->FTunnel)
	{
		// Now that we set the CONF_loghost, the hostname is correct already
		Host = FSessionData->FOrigHostName;
		Port = FSessionData->FOrigPortNumber;
	}
}
//---------------------------------------------------------------------------
bool TSecureShell::HaveAcceptNewHostKeyPolicy()
{
	return SameText(FSessionData->FHostKey.Trim(), L"acceptnew");
}
//---------------------------------------------------------------------------
THierarchicalStorage *TSecureShell::GetHostKeyStorage()
{
	if (/*!Configuration->Persistent &&*/ HaveAcceptNewHostKeyPolicy())
	{
	//	return Configuration->CreateConfigRegistryStorage();
	}
	else
	{
	//	return Configuration->CreateConfigStorage();
	}
	return nullptr;
}
//---------------------------------------------------------------------------
UnicodeString /*__fastcall*/ TSecureShell::RetrieveHostKey(const UnicodeString& Host, int Port, const UnicodeString& KeyType)
{
//	std::unique_ptr<THierarchicalStorage> Storage(GetHostKeyStorage());
//	Storage->AccessMode = smRead;
//	TGuard Guard(PuttyStorageSection.get());
	DebugAssert(PuttyStorage == NULL);
//	TValueRestorer<THierarchicalStorage *> StorageRestorer(PuttyStorage);
//	PuttyStorage = Storage.get();

	AnsiString AnsiStoredKeys;
	AnsiStoredKeys.SetLength(10240);
	UnicodeString Result;
	if (retrieve_host_key(AnsiString(Host).c_str(), Port, AnsiString(KeyType).c_str(),
		AnsiStoredKeys.c_str(), static_cast<int>(AnsiStoredKeys.Length())) == 0)
	{
		PackStr(AnsiStoredKeys);
		Result = UnicodeString(AnsiStoredKeys);
	}
	return Result;
}
//---------------------------------------------------------------------------
static bool DoVerifyFingerprint(const UnicodeString& AFingerprintFromUser, const UnicodeString& AFingerprintFromHost, bool Base64)
{
	UnicodeString FingerprintFromUser = AFingerprintFromUser;
	UnicodeString FingerprintFromHost = AFingerprintFromHost;
	UnicodeString FingerprintFromUserName, FingerprintFromHostName;
	NormalizeFingerprint(FingerprintFromUser, FingerprintFromUserName);
	NormalizeFingerprint(FingerprintFromHost, FingerprintFromHostName);
	bool Result;
	if (DebugAlwaysFalse(FingerprintFromHostName.IsEmpty()))
	{
		Result = false;
	}
	else
	{
		// In all below three formats, the checksum can be any of these formats:
		// MD5 (case insensitive):
		// xx:xx:xx
		// xx-xx-xx
		// SHA-256 (case sensitive):
		// xxxx+xx/xxx=
		// xxxx+xx/xxx
		// xxxx-xx_xxx=
		// xxxx-xx_xxx

		// Full fingerprint format "type bits checksum"
		if (!FingerprintFromUserName.IsEmpty())
		{
			Result =
				SameText(FingerprintFromUserName, FingerprintFromHostName) &&
				SameChecksum(FingerprintFromUser, FingerprintFromHost, Base64);
		}
		else
		{
			// normalized format "type-checksum"
			UnicodeString NormalizedPrefix = FingerprintFromHost + NormalizedFingerprintSeparator;
			if (StartsText(NormalizedPrefix, FingerprintFromUser))
			{
				FingerprintFromUser.Delete(1, NormalizedPrefix.Length());
				Result = SameChecksum(FingerprintFromUser, FingerprintFromHost, Base64);
			}
			else
			{
				// last resort: "checksum" only
				Result = SameChecksum(FingerprintFromUser, FingerprintFromHost, Base64);
			}
		}
	}
	return Result;
}
//---------------------------------------------------------------------------
static bool VerifyFingerprint(
	const UnicodeString& FingerprintFromUser, const UnicodeString& FingerprintFromHostMD5, const UnicodeString& FingerprintFromHostSHA256)
{
	return
		DoVerifyFingerprint(FingerprintFromUser, FingerprintFromHostMD5, false) ||
		DoVerifyFingerprint(FingerprintFromUser, FingerprintFromHostSHA256, true);
}
//---------------------------------------------------------------------------
/*struct TPasteKeyHandler
{
	UnicodeString KeyStr;
	UnicodeString FingerprintMD5;
	UnicodeString FingerprintSHA256;
	TSessionUI *UI;

	void __fastcall Paste(TObject *Sender, unsigned int& Answer);
};
//---------------------------------------------------------------------------
void __fastcall TPasteKeyHandler::Paste(TObject *Sender, unsigned int& Answer)
{
	UnicodeString ClipboardText;
	if (TextFromClipboard(ClipboardText, true))
	{
	if (VerifyFingerprint(ClipboardText, FingerprintMD5, FingerprintSHA256) ||
		SameText(ClipboardText, KeyStr))
	{
		Answer = qaYes;
	}
	else
	{
		const struct ssh_keyalg *Algorithm;
		try
		{
			UnicodeString Key = ParseOpenSshPubLine(ClipboardText, Algorithm);
			if (Key == KeyStr)
			{
				Answer = qaYes;
			}
		}
		catch (...)
		{
		// swallow
		}
	}
	}

	if (Answer == 0)
	{
		UI->QueryUser(LoadStr(HOSTKEY_NOT_MATCH_CLIPBOARD), NULL, qaOK, NULL, qtError);
	}
}*/
//---------------------------------------------------------------------------
bool TSecureShell::VerifyCachedHostKey(
	const UnicodeString& StoredKeys, const UnicodeString& KeyStr, const UnicodeString& FingerprintMD5, const UnicodeString& FingerprintSHA256)
{
	bool Result = false;
	UnicodeString Buf = StoredKeys;
	while (!Result && !Buf.IsEmpty())
	{
		UnicodeString StoredKey = CutToChar(Buf, HostKeyDelimiter, false);
		// skip leading ECDH subtype identification
		int P = StoredKey.Pos(L",");
		// Start from beginning or after the comma, if there's any.
		// If it does not start with 0x, it's probably a fingerprint (stored by TSessionData::CacheHostKey).
		bool Fingerprint = (StoredKey.SubString(P + 1, 2) != L"0x");
		if (!Fingerprint && (StoredKey == KeyStr))
		{
			LogEvent(L"Host key matches cached key");
			Result = true;
		}
		else if (Fingerprint && VerifyFingerprint(StoredKey, FingerprintMD5, FingerprintSHA256))
		{
			LogEvent(L"Host key matches cached key fingerprint");
			Result = true;
		}
		else
		{
		//	if (Configuration->ActualLogProtocol >= 1)
		//	{
				UnicodeString FormattedKey = Fingerprint ? StoredKey : FormatKeyStr(StoredKey);
				LogEvent(FORMAT(L"Host key does not match cached key %ls", FormattedKey.c_str()));
		//	}
		//	else
		//	{
		//		LogEvent(L"Host key does not match cached key");
		//	}
		}
	}
	return Result;
}
//---------------------------------------------------------------------------
UnicodeString TSecureShell::StoreHostKey(const UnicodeString& Host, int Port, const UnicodeString& KeyType, const UnicodeString& KeyStr)
{
//	TGuard Guard(PuttyStorageSection.get());
	DebugAssert(PuttyStorage == NULL);
//	TValueRestorer<THierarchicalStorage *> StorageRestorer(PuttyStorage);
//	std::unique_ptr<THierarchicalStorage> Storage(GetHostKeyStorage());
//	Storage->AccessMode = smReadWrite;
//	PuttyStorage = Storage.get();
	store_host_key(FSeat, AnsiString(Host).c_str(), Port, AnsiString(KeyType).c_str(), AnsiString(KeyStr).c_str());
	return L"";//Storage->Source;
}
//---------------------------------------------------------------------------
void TSecureShell::ParseFingerprint(const UnicodeString& Fingerprint, UnicodeString& SignKeyType, UnicodeString& Hash)
{
	UnicodeString Buf = Fingerprint;
	UnicodeString SignKeyAlg = CutToChar(Buf, L' ', false);
	UnicodeString SignKeySize = CutToChar(Buf, L' ', false);
	SignKeyType = SignKeyAlg + L' ' + SignKeySize;
	Hash = Buf;
}
//---------------------------------------------------------------------------
UnicodeString /*__fastcall*/ GetKeyTypeHuman(const UnicodeString& KeyType)
{
	std::string keyType = AnsiString(KeyType).get();
	UnicodeString Result;
	if (keyType == ssh_dsa.cache_id)
	{
		Result = L"DSA";
	}
	else if ((keyType == ssh_rsa.cache_id) ||
		(keyType == "rsa")) // SSH1
	{
		Result = L"RSA";
	}
	else if (keyType == ssh_ecdsa_ed25519.cache_id)
	{
		Result = L"Ed25519";
	}
	else if (keyType == ssh_ecdsa_nistp256.cache_id)
	{
		Result = L"ECDSA/nistp256";
	}
	else if (keyType == ssh_ecdsa_nistp384.cache_id)
	{
		Result = L"ECDSA/nistp384";
	}
	else if (keyType == ssh_ecdsa_nistp521.cache_id)
	{
		Result = L"ECDSA/nistp521";
	}
	else
	{
		DebugFail();
		Result = KeyType;
	}
	return Result;
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::VerifyHostKey(
	const UnicodeString& AHost, int Port, const UnicodeString& KeyType, const UnicodeString& KeyStr,
	const UnicodeString& AFingerprintSHA256, const UnicodeString& AFingerprintMD5,
	bool IsCertificate, int CACount, bool AlreadyVerified)
{
//	if (Configuration->ActualLogProtocol >= 1)
//	{
		UnicodeString HostKeyAction = AlreadyVerified ? L"Got" : L"Verifying";
		LogEvent(FORMAT(L"%ls host key %ls %ls with fingerprints %ls, %ls", HostKeyAction.c_str(), KeyType.c_str(), FormatKeyStr(KeyStr).c_str(), AFingerprintSHA256.c_str(), AFingerprintMD5.c_str()));
//	}

	GotHostKey();

	DebugAssert(KeyStr.Pos(HostKeyDelimiter) == 0);

	UnicodeString Host = AHost;
	GetRealHost(Host, Port);

	UnicodeString SignKeyType, MD5, SHA256;
	ParseFingerprint(AFingerprintMD5, SignKeyType, MD5);

	DebugAssert(get_ssh_version(FBackendHandle) == 2);
	UnicodeString SignKeyTypeSHA256;
	ParseFingerprint(AFingerprintSHA256, SignKeyTypeSHA256, SHA256);
	DebugAssert(SignKeyTypeSHA256 == SignKeyType);
	if (DebugAlwaysTrue(StartsText(L"SHA256:", SHA256)))
	{
		CutToChar(SHA256, L':', false);
	}

	UnicodeString FingerprintMD5 = SignKeyType + L' ' + MD5;
	DebugAssert(AFingerprintMD5 == FingerprintMD5);
	UnicodeString FingerprintSHA256 = SignKeyType + L' ' + SHA256;
	DebugAssert(ReplaceStr(AFingerprintSHA256, L"SHA256:", EmptyStr) == FingerprintSHA256);

	FSessionInfo.HostKeyFingerprintSHA256 = FingerprintSHA256;
	FSessionInfo.HostKeyFingerprintMD5 = FingerprintMD5;

	if (FSessionData->FFingerprintScan)
	{
		return;//Abort();
	}

	if (!AlreadyVerified)
	{
		bool AcceptNew = HaveAcceptNewHostKeyPolicy();
		UnicodeString ConfigHostKey;
		if (!AcceptNew)
		{
			ConfigHostKey = FSessionData->FHostKey;
		}

		UnicodeString StoredKeys = RetrieveHostKey(Host, Port, KeyType);
		bool Result = VerifyCachedHostKey(StoredKeys, KeyStr, FingerprintMD5, FingerprintSHA256);
		if (!Result && AcceptNew)
		{
			if (!StoredKeys.IsEmpty()) // optimization + avoiding the log message
			{
				AcceptNew = false;
			}
			else if (have_any_ssh2_hostkey(FSeat, AnsiString(Host).c_str(), Port))
			{
				LogEvent(L"Host key not found in the cache, but other key types found, cannot accept new key");
				AcceptNew = false;
			}
		}

		bool ConfiguredKeyNotMatch = false;

		if (!Result && !ConfigHostKey.IsEmpty() &&
			// Should test have_any_ssh2_hostkey + No need to bother with AcceptNew, as we never get here
			(StoredKeys.IsEmpty() || FSessionData->FOverrideCachedHostKey))
		{
			UnicodeString Buf = ConfigHostKey;
			while (!Result && !Buf.IsEmpty())
			{
				UnicodeString ExpectedKey = CutToChar(Buf, HostKeyDelimiter, false);
				if (ExpectedKey == L"*")
				{
				//	UnicodeString Message = LoadStr(ANY_HOSTKEY);
				//	FUI->Information(Message);
				//	FLog->Add(llException, Message);
					Result = true;
				}
				else if (VerifyFingerprint(ExpectedKey, FingerprintMD5, FingerprintSHA256))
				{
					LogEvent(L"Host key matches configured key fingerprint");
					Result = true;
				}
				else
				{
					LogEvent(FORMAT(L"Host key does not match configured key fingerprint %ls", ExpectedKey.c_str()));
				}
			}

			if (!Result)
			{
				ConfiguredKeyNotMatch = true;
			}
		}

	//	if (!Result && AcceptNew && DebugAlwaysTrue(ConfigHostKey.IsEmpty()))
	//	{
	//		try
	//		{
	//			UnicodeString StorageSource = StoreHostKey(Host, Port, KeyType, KeyStr);
	//			UnicodeString StoredKeys = RetrieveHostKey(Host, Port, KeyType);
	//			if (StoredKeys != KeyStr)
	//			{
	//				throw Exception(UnicodeString());
	//			}
	//			Configuration->Usage->Inc(L"HostKeyNewAccepted");
	//			LogEvent(FORMAT(L"Warning: Stored new host key to %ls - This should occur only on the first connection", StorageSource.c_str()));
	//			Result = true;
	//		}
	//		catch (Exception& E)
	//		{
	//			FUI->FatalError(&E, LoadStr(STORE_NEW_HOSTKEY_ERROR));
	//		}
	//	}

		if (!Result)
		{
			bool Verified;
			if (ConfiguredKeyNotMatch /*|| Configuration->DisableAcceptingHostKeys*/)
			{
				Verified = false;
			}
			// no point offering manual verification, if we cannot persist the verified key
		//	else if (!Configuration->Persistent && Configuration->Scripting)
		//	{
		//		Verified = false;
		//	}
			else
			{
				static wchar_t mbtitle[] = L"%ls Security Alert";

				static wchar_t absentmsg[] =
					L"**Continue connecting to an unknown server and "
					L"add its host key to a cache?**\n"
					L"\n"
					L"The server's host key was not found in the cache. You have no guarantee "
					L"that the server is the computer you think it is.\n"
					L"\n"
					L"The server's %ls key details are:\n"
					L"\n"
					L"%ls\n"
					L"\n"
					L"If you trust this host, press Yes. "
					L"To connect without adding host key to the cache, press No. "
					L"To abandon the connection press Cancel.";

				static wchar_t wrongmsg[] =
					L"**WARNING - POTENTIAL SECURITY BREACH!**\n"
					L"\n"
					L"The server's host key does not match the one WinSCP has "
					L"in cache. This means that either the "
					L"server administrator has changed the host key, "
					L"the server presents different key under certain circumstance, "
					L"or you have actually connected to another computer pretending "
					L"to be the server.\n"
					L"\n"
					L"The new %ls key details are:\n"
					L"\n"
					L"%ls\n"
					L"\n"
					L"If you were expecting this change, trust the new key and "
					L"want to continue connecting to the server, "
					L"either press Update to update cache, or "
					L"press Add to add the new key to the cache while keeping the old one(s). "
					L"If you want to carry on connecting but without updating "
					L"the cache, press Skip. "
					L"If you want to abandon the connection completely, press "
					L"Cancel. Pressing Cancel is the ONLY guaranteed safe "
					L"choice.";

				// We should not offer caching if !Configuration->Persistent,
				// but as scripting mode is handled earlier and in GUI it hardly happens,
				// it's a small issue.
			//	TClipboardHandler ClipboardHandler;
			//	ClipboardHandler.Text = FingerprintSHA256 + L"\n" + FingerprintMD5;
			//	TPasteKeyHandler PasteKeyHandler;
			//	PasteKeyHandler.KeyStr = KeyStr;
			//	PasteKeyHandler.FingerprintMD5 = FingerprintMD5;
			//	PasteKeyHandler.FingerprintSHA256 = FingerprintSHA256;
			//	PasteKeyHandler.UI = FUI;

				bool Unknown = StoredKeys.IsEmpty();

			//	int Answers;
			//	int AliasesCount;
			//	TQueryButtonAlias Aliases[4];
			//	Aliases[0].Button = qaRetry;
			//	Aliases[0].Alias = LoadStr(COPY_KEY_BUTTON);
			//	Aliases[0].ActionAlias = LoadStr(COPY_KEY_ACTION);
			//	Aliases[0].OnSubmit = &ClipboardHandler.Copy;
			//	Aliases[1].Button = qaIgnore;
			//	Aliases[1].Alias = LoadStr(PASTE_KEY_BUTTON);
			//	Aliases[1].OnSubmit = &PasteKeyHandler.Paste;
			//	Aliases[1].GroupWith = qaYes;
			//	Answers = qaYes | qaCancel | qaRetry | qaIgnore;
			//	AliasesCount = 2;
			//	if (!Unknown)
			//	{
			//		Aliases[2].Button = qaYes;
			//		Aliases[2].Alias = LoadStr(UPDATE_KEY_BUTTON);
			//		Aliases[3].Button = qaOK;
			//		Aliases[3].Alias = LoadStr(ADD_KEY_BUTTON);
			//		AliasesCount += 2;
			//		Answers |= qaSkip | qaOK;
			//	}
			//	else
			//	{
			//		Answers |= qaNo;
			//	}

			//	TQueryParams Params(qpWaitInBatch);
			//	Params.NoBatchAnswers = qaYes | qaRetry | qaSkip | qaOK;
			//	Params.HelpKeyword = (Unknown ? HELP_UNKNOWN_KEY : HELP_DIFFERENT_KEY);
			//	Params.Aliases = Aliases;
			//	Params.AliasesCount = AliasesCount;

				UnicodeString KeyTypeHuman = GetKeyTypeHuman(KeyType);
				UnicodeString KeyDetails = FORMAT(L"Algorithm: %ls\nSHA-256: %ls\nMD5: %ls", SignKeyType.c_str(), SHA256.c_str(), MD5.c_str());
				UnicodeString Message = FORMAT((Unknown ? absentmsg : wrongmsg), KeyTypeHuman.c_str(), KeyDetails.c_str());
				UnicodeString AppName = UnicodeString(appname);
			//	if (Configuration->Scripting)
			//	{
			//		AddToList(Message, LoadStr(SCRIPTING_USE_HOSTKEY), L"\n");
			//	}

				unsigned int R = MessageBox(pWorker->getHwnd(), Message.c_str(), FORMAT(mbtitle, AppName.c_str()).c_str(), MB_ICONWARNING | MB_YESNOCANCEL | MB_DEFBUTTON3);
				UnicodeString StoreKeyStr = KeyStr;

				switch (R) {
				case IDOK:
					DebugAssert(!Unknown);
					StoreKeyStr = (StoredKeys + HostKeyDelimiter + StoreKeyStr);
					// fall through
				case IDYES:
					StoreHostKey(Host, Port, KeyType, StoreKeyStr);
					Verified = true;
					break;

				case IDCANCEL:
					Verified = false;
					throw(Exception(L"Canceled by user"));
					break;

				default:
					Verified = true;
					break;
				}
			}

			if (!Verified)
			{
			//	Configuration->Usage->Inc(L"HostNotVerified");

				UnicodeString Message;
				if (ConfiguredKeyNotMatch)
				{
					Message = FMTLOAD(CONFIGURED_KEY_NOT_MATCH, (ConfigHostKey));
				}
			//	else if (!Configuration->Persistent && Configuration->Scripting)
			//	{
			//		Message = LoadStr(HOSTKEY_NOT_CONFIGURED);
			//	}
				else
				{
					Message = LoadStr(KEY_NOT_VERIFIED);
				}

			//	Exception *E = new Exception(MainInstructions(Message));
			//	try
			//	{
					FatalError(MainInstructions(Message), FMTLOAD(HOSTKEY, (FingerprintSHA256)));
			//	}
			//	__finally
			//	{
			//		delete E;
			//	}
			}
		}
	}

//	Configuration->RememberLastFingerprint(FSessionData->SiteKey, SshFingerprintType, FingerprintSHA256);
}
//---------------------------------------------------------------------------
bool /*__fastcall*/ TSecureShell::HaveHostKey(UnicodeString Host, int Port, const UnicodeString KeyType)
{
	// Return true, if we have any host key fingerprint of a particular type

	GetRealHost(Host, Port);

	UnicodeString StoredKeys = RetrieveHostKey(Host, Port, KeyType);
	bool Result = !StoredKeys.IsEmpty();

	if (!FSessionData->FHostKey.IsEmpty())
	{
		UnicodeString Buf = FSessionData->FHostKey;
		while (!Result && !Buf.IsEmpty())
		{
			UnicodeString ExpectedKey = CutToChar(Buf, HostKeyDelimiter, false);
			UnicodeString ExpectedKeyType = KeyTypeFromFingerprint(ExpectedKey);
			Result = SameText(ExpectedKeyType, KeyType);
		}
	}

	if (Result &&
		(FLoggedKnownHostKeys.find(KeyType) == FLoggedKnownHostKeys.end()))
	{
		LogEvent(FORMAT(L"Have a known host key of type %ls", KeyType.c_str()));
		// This is called multiple times for the same cached key since PuTTY 0.76 (support for rsa-sha2*?).
		// Avoid repeated log entries.
		FLoggedKnownHostKeys.insert(KeyType);
	}

	return Result;
}
//---------------------------------------------------------------------------
void TSecureShell::AskAlg(const UnicodeString& AAlgType, const UnicodeString& AlgName, int WeakCryptoReason)
{
	// beware of changing order
	static const TPuttyTranslation AlgTranslation[] = {
		{ L"cipher", CIPHER_TYPE_BOTH2 },
		{ L"client-to-server cipher", CIPHER_TYPE_CS2 },
		{ L"server-to-client cipher", CIPHER_TYPE_SC2 },
		{ L"key-exchange algorithm", KEY_EXCHANGE_ALG },
		{ L"hostkey type", KEYKEY_TYPE },
	};

	UnicodeString AlgType = AAlgType;
	TranslatePuttyMessage(AlgTranslation, LENOF(AlgTranslation), AlgType);

	UnicodeString Msg;
	UnicodeString NewLine = UnicodeString(sLineBreak);
	switch (WeakCryptoReason)
	{
	default:
		DebugFail();
	case 0:
		Msg = FMTLOAD(ALG_BELOW_TRESHOLD2, (AlgType, AlgName));
		break;

	case 1:
	case 2:
		Msg = FMTLOAD(CIPHER_TERRAPIN, (AlgType, AlgName));
		if (WeakCryptoReason == 2)
		{
			Msg += NewLine + NewLine + FMTLOAD(CIPHER_TERRAPIN_AVOID, (AlgName));
		}
		break;
	}

	Msg =
		MainInstructions(LoadStr(WEAK_ALG_TITLE)) + NewLine + NewLine +
		Msg + NewLine + NewLine +
		LoadStr(WEAK_ALG_RISK_CONFIRM);

	if (MessageBox(pWorker->getHwnd(), Msg.c_str(), L"Confirm risk", MB_ICONWARNING | MB_YESNO) == IDNO/*FUI->QueryUser(Msg, NULL, qaYes | qaNo, NULL, qtWarning) == qaNo*/)
	{
		UnicodeString Error = FMTLOAD(ALG_NOT_VERIFIED, (AlgType, AlgName));
		FatalError(Error);//FUI->FatalError(NULL, Error);
	}
}
//---------------------------------------------------------------------------
void /*__fastcall*/ TSecureShell::DisplayBanner(const UnicodeString& Banner)
{
	// Since 0.77 PuTTY calls this again with CRLF if the actual banner does not end with one.
	if (!Banner.Trim().IsEmpty())
	{
		MessageBox(pWorker->getHwnd(), Banner.c_str(), L"PuTTY Info", MB_ICONINFORMATION | MB_OK);
	//	FUI->DisplayBanner(Banner);
	}
}
//---------------------------------------------------------------------------
bool /*__fastcall*/ TSecureShell::GetStoredCredentialsTried()
{
	return FStoredPasswordTried || FStoredPasswordTriedForKI || FStoredPassphraseTried;
}
//---------------------------------------------------------------------------
bool /*__fastcall*/ TSecureShell::GetReady()
{
	return FOpened && (FWaiting == 0);
}
//---------------------------------------------------------------------------
/*void __fastcall TSecureShell::CollectUsage()
{
	if (FCollectPrivateKeyUsage)
	{
		Configuration->Usage->Inc(L"OpenedSessionsPrivateKey2");
	}

	Configuration->Usage->Inc(L"OpenedSessionsSSH2");

	if (SshImplementation == sshiOpenSSH)
	{
		Configuration->Usage->Inc(L"OpenedSessionsSSHOpenSSH");
	}
	else if (SshImplementation == sshiProFTPD)
	{
		Configuration->Usage->Inc(L"OpenedSessionsSSHProFTPD");
	}
	else if (SshImplementation == sshiBitvise)
	{
		Configuration->Usage->Inc(L"OpenedSessionsSSHBitvise");
	}
	else if (SshImplementation == sshiTitan)
	{
		Configuration->Usage->Inc(L"OpenedSessionsSSHTitan");
	}
	else if (SshImplementation == sshiOpenVMS)
	{
		Configuration->Usage->Inc(L"OpenedSessionsSSHOpenVMS");
	}
	else if (ContainsText(FSessionInfo.SshImplementation, L"Serv-U"))
	{
		Configuration->Usage->Inc(L"OpenedSessionsSSHServU");
	}
	else if (SshImplementation == sshiCerberus)
	{
		// Ntb, Cerberus can also be detected using vendor-id extension
		// Cerberus FTP Server 7.0.5.3 (70005003) by Cerberus, LLC
		Configuration->Usage->Inc(L"OpenedSessionsSSHCerberus");
	}
	else if (ContainsText(FSessionInfo.SshImplementation, L"WS_FTP"))
	{
		Configuration->Usage->Inc(L"OpenedSessionsSSHWSFTP");
	}
	// SSH-2.0-1.36_sshlib GlobalSCAPE
	else if (ContainsText(FSessionInfo.SshImplementation, L"GlobalSCAPE"))
	{
		Configuration->Usage->Inc(L"OpenedSessionsSSHGlobalScape");
	}
	// SSH-2.0-CompleteFTP-8.1.3
	else if (ContainsText(FSessionInfo.SshImplementation, L"CompleteFTP"))
	{
		Configuration->Usage->Inc(L"OpenedSessionsSSHComplete");
	}
	// SSH-2.0-CoreFTP-0.3.3
	else if (ContainsText(FSessionInfo.SshImplementation, L"CoreFTP"))
	{
		Configuration->Usage->Inc(L"OpenedSessionsSSHCore");
	}
	// SSH-2.0-SSHD-CORE-0.11.0 (value is configurable, this is a default)
	// (Apache Mina SSHD, e.g. on brickftp.com)
	else if (ContainsText(FSessionInfo.SshImplementation, L"SSHD-CORE"))
	{
		Configuration->Usage->Inc(L"OpenedSessionsSSHApache");
	}
	// SSH-2.0-Syncplify_Me_Server
	else if (ContainsText(FSessionInfo.SshImplementation, L"Syncplify"))
	{
		Configuration->Usage->Inc(L"OpenedSessionsSSHSyncplify");
	}
	else if (ContainsText(FSessionInfo.SshImplementation, L"zFTPServer"))
	{
		Configuration->Usage->Inc(L"OpenedSessionsSSHzFTP");
	}
	else
	{
		Configuration->Usage->Inc(L"OpenedSessionsSSHOther");
	}

	int CipherGroup = GetCipherGroup(get_cscipher(FBackendHandle));
	switch (CipherGroup)
	{
	case CIPHER_3DES: Configuration->Usage->Inc(L"OpenedSessionsSSHCipher3DES"); break;
	case CIPHER_BLOWFISH: Configuration->Usage->Inc(L"OpenedSessionsSSHCipherBlowfish"); break;
	case CIPHER_AES: Configuration->Usage->Inc(L"OpenedSessionsSSHCipherAES"); break;
	// All following miss "Cipher"
	case CIPHER_DES: Configuration->Usage->Inc(L"OpenedSessionsSSHDES"); break;
	case CIPHER_ARCFOUR: Configuration->Usage->Inc(L"OpenedSessionsSSHArcfour"); break;
	case CIPHER_CHACHA20: Configuration->Usage->Inc(L"OpenedSessionsSSHChaCha20"); break;
	case CIPHER_AESGCM: Configuration->Usage->Inc(L"OpenedSessionsSSHAESGCM"); break;
	default: DebugFail(); break;
	}
}*/
//---------------------------------------------------------------------------
bool /*__fastcall*/ TSecureShell::CanChangePassword()
{
	return
		// These major SSH servers explicitly do not support password change.
		(FSshImplementation != sshiOpenSSH) && // See userauth_passwd
		(FSshImplementation != sshiProFTPD); // See sftp_auth_password
}
} // namespace bcb
