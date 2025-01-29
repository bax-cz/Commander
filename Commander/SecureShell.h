#pragma once

#include <set>
//#include "Configuration.h"
#include "SessionData.h"
#include "SessionInfo.h"
#include "BackgroundWorker.h"
//---------------------------------------------------------------------------
//#ifndef PuttyIntfH
//struct Backend_vtable;
//struct Backend;
//struct Conf;
//#endif
//---------------------------------------------------------------------------
struct _WSANETWORKEVENTS;
typedef struct _WSANETWORKEVENTS WSANETWORKEVENTS;
typedef UINT_PTR SOCKET;
struct LogContext;
struct callback_set;

namespace bcb {
typedef std::set<SOCKET> TSockets;
struct TPuttyTranslation;
enum TSshImplementation { sshiUnknown, sshiOpenSSH, sshiProFTPD, sshiBitvise, sshiTitan, sshiOpenVMS, sshiCerberus };
struct ScpLogPolicy;
struct ScpSeat;
enum TSecureShellMode { ssmNone, ssmUploading, ssmDownloading };
//---------------------------------------------------------------------------
enum TFSCommand { fsNull = 0, fsVarValue, fsLastLine, fsFirstLine,
  fsCurrentDirectory, fsChangeDirectory, fsListDirectory, fsListCurrentDirectory,
  fsListFile, fsLookupUsersGroups, fsCopyToRemote, fsCopyToLocal, fsDeleteFile,
  fsRenameFile, fsCreateDirectory, fsChangeMode, fsChangeGroup, fsChangeOwner,
  fsHomeDirectory, fsUnset, fsUnalias, fsCreateLink, fsCopyFile,
  fsAnyCommand, fsLang, fsReadSymlink, fsChangeProperties, fsMoveFile,
  fsLock };
//---------------------------------------------------------------------------
#define LastLineSeparator L":"
#define SSH_LAST_LINE L"CommanderSsh_LastLine"
#define SSH_FIRST_LINE L"CommanderSsh_FirstLine"
#define MaxShellCommand fsLang
#define ShellCommandCount MaxShellCommand + 1
#define MaxCommandLen 40
struct TCommandType
{
	int MinLines;
	int MaxLines;
	bool ModifiesFiles;
	bool ChangesDirectory;
	bool InteractiveCommand;
	wchar_t Command[MaxCommandLen];
};
//---------------------------------------------------------------------------
#define F false
#define T true
// TODO: remove "mf" and "cd", it is implemented in TTerminal already
const TCommandType DefaultCommandSet[ShellCommandCount] = {
//                       min max mf cd ia  command
/*Null*/                { -1, -1, F, F, F, L"" },
/*VarValue*/            { -1, -1, F, F, F, L"echo \"$%ls\"" /* variable */ },
/*LastLine*/            { -1, -1, F, F, F, L"echo \"%ls" LastLineSeparator L"%ls\"" /* last line, return var */ },
/*FirstLine*/           { -1, -1, F, F, F, L"echo \"%ls\"" /* first line */ },
/*CurrentDirectory*/    {  1,  1, F, F, F, L"pwd" },
/*ChangeDirectory*/     {  0,  0, F, T, F, L"cd \"%ls\"" /* directory */ },
// list directory can be empty on permission denied, this is handled in ReadDirectory
/*ListDirectory*/       { -1, -1, F, F, F, L"%ls %ls \"%ls\"" /* listing command, options, directory */ },
/*ListCurrentDirectory*/{ -1, -1, F, F, F, L"%ls %ls" /* listing command, options */ },
/*ListFile*/            {  1,  1, F, F, F, L"%ls -d %ls \"%ls\"" /* listing command, options, file/directory */ },
/*LookupUserGroups*/    {  0,  1, F, F, F, L"groups" },
/*CopyToRemote*/        { -1, -1, T, F, T, L"scp -r %ls -d -t \"%ls\"" /* options, directory */ },
/*CopyToLocal*/         { -1, -1, F, F, T, L"scp -r %ls -d -f \"%ls\"" /* options, file */ },
/*DeleteFile*/          {  0,  0, T, F, F, L"rm -f -r \"%ls\"" /* file/directory */},
/*RenameFile*/          {  0,  0, T, F, F, L"mv -f \"%ls\" \"%ls\"" /* file/directory, new name*/},
/*CreateDirectory*/     {  0,  0, T, F, F, L"mkdir \"%ls\"" /* new directory */},
/*ChangeMode*/          {  0,  0, T, F, F, L"chmod %ls %ls \"%ls\"" /* options, mode, filename */},
/*ChangeGroup*/         {  0,  0, T, F, F, L"chgrp %ls \"%ls\" \"%ls\"" /* options, group, filename */},
/*ChangeOwner*/         {  0,  0, T, F, F, L"chown %ls \"%ls\" \"%ls\"" /* options, owner, filename */},
/*HomeDirectory*/       {  0,  0, F, T, F, L"cd" },
/*Unset*/               {  0,  0, F, F, F, L"unset \"%ls\"" /* variable */ },
/*Unalias*/             {  0,  0, F, F, F, L"unalias \"%ls\"" /* alias */ },
/*CreateLink*/          {  0,  0, T, F, F, L"ln %ls \"%ls\" \"%ls\"" /*symbolic (-s), filename, point to*/},
/*CopyFile*/            {  0,  0, T, F, F, L"cp -p -r -f %ls \"%ls\" \"%ls\"" /* file/directory, target name*/},
/*AnyCommand*/          {  0, -1, T, T, F, L"%ls" },
/*Lang*/                {  0,  1, F, F, F, L"printenv LANG"}
};
#undef F
#undef T
//---------------------------------------------------------------------------
class TSecureShell
{
	friend class TPoolForDataEvent;

public:
	Commander::CBackgroundWorker *pWorker;
	SOCKET FSocket;
	HANDLE FSocketEvent;
	TSockets FPortFwdSockets;
//	TSessionUI *FUI;
	TSessionData *FSessionData;
	bool FActive;
	TSessionInfo FSessionInfo;
	bool FSessionInfoValid;
	TDateTime FLastDataSent;
	Backend *FBackendHandle;
	TNotifyEvent FOnReceive;
	bool FFrozen;
	bool FDataWhileFrozen;
	bool FStoredPasswordTried;
	bool FStoredPasswordTriedForKI;
	bool FStoredPassphraseTried;
	bool FAuthenticationCancelled;
	bool FOpened;
	bool FClosed;
	int FWaiting;
	bool FSimple;
	bool FNoConnectionResponse;
	bool FCollectPrivateKeyUsage;
	int FWaitingForData;
	TSshImplementation FSshImplementation;

	/*unsigned*/size_t PendLen;
	/*unsigned*/size_t PendSize;
	/*unsigned*/size_t OutLen;
	unsigned char *OutPtr;
	unsigned char *Pending;
//	TSessionLog *FLog;
//	TConfiguration *FConfiguration;
	bool FAuthenticating;
	bool FAuthenticated;
	UnicodeString FStdErrorTemp;
	UnicodeString FStdError;
	UnicodeString FCWriteTemp;
	UnicodeString FAuthenticationLog;
	UnicodeString FLastTunnelError;
	UnicodeString FUserName;
	bool FUtfStrings;
	DWORD FLastSendBufferUpdate;
	int FSendBuf;
	/*callback_set*/std::unique_ptr<callback_set> FCallbackSet;
	ScpLogPolicy *FLogPolicy;
	ScpSeat *FSeat;
	LogContext *FLogCtx;
	std::set<UnicodeString> FLoggedKnownHostKeys;

	void /*__fastcall*/ Init();
	void /*__fastcall*/ SetActive(bool value);
	void inline /*__fastcall*/ CheckConnection(int Message = -1);
	void /*__fastcall*/ WaitForData();
	void /*__fastcall*/ Discard();
	void /*__fastcall*/ FreeBackend();
	void /*__fastcall*/ PoolForData(WSANETWORKEVENTS& Events, unsigned int& Result);
	inline void /*__fastcall*/ CaptureOutput(TLogLineType Type, const UnicodeString& Line);
	void /*__fastcall*/ ResetConnection();
	void /*__fastcall*/ ResetSessionInfo();
	void /*__fastcall*/ SocketEventSelect(SOCKET Socket, HANDLE Event, bool Enable);
	bool /*__fastcall*/ EnumNetworkEvents(SOCKET Socket, WSANETWORKEVENTS& Events);
	void /*__fastcall*/ HandleNetworkEvents(SOCKET Socket, WSANETWORKEVENTS& Events);
	bool /*__fastcall*/ ProcessNetworkEvents(SOCKET Socket);
	bool /*__fastcall*/ EventSelectLoop(unsigned int MSec, bool ReadEventRequired,
		WSANETWORKEVENTS *Events);
	void /*__fastcall*/ UpdateSessionInfo();
	bool /*__fastcall*/ GetReady();
	void /*__fastcall*/ DispatchSendBuffer(size_t BufSize);
	void /*__fastcall*/ SendBuffer(unsigned int& Result);
//	unsigned int /*__fastcall*/ TimeoutPrompt(TQueryParamsTimerEvent PoolEvent);
	void TimeoutAbort(unsigned int Answer);
	bool /*__fastcall*/ TryFtp();
	UnicodeString /*__fastcall*/ ConvertInput(const RawByteString& Input);
	void /*__fastcall*/ GetRealHost(UnicodeString& Host, int& Port);
	UnicodeString /*__fastcall*/ RetrieveHostKey(const UnicodeString& Host, int Port, const UnicodeString& KeyType);
	bool HaveAcceptNewHostKeyPolicy();
	THierarchicalStorage *GetHostKeyStorage();
	bool VerifyCachedHostKey(
		const UnicodeString& StoredKeys, const UnicodeString& KeyStr, const UnicodeString& FingerprintMD5, const UnicodeString& FingerprintSHA256);
	UnicodeString StoreHostKey(
		const UnicodeString& Host, int Port, const UnicodeString& KeyType, const UnicodeString& KeyStr);
	bool HasLocalProxy();

//protected:
//	TCaptureOutputEvent FOnCaptureOutput;
	std::function<void(const std::wstring& str, TCaptureOutputType sutputType)> FOnCaptureOutput;

	// TODO: SCP specific - move somewhere else
	std::vector<std::wstring> SendCommandFull( TFSCommand cmd,
		const std::wstring& param1 = L"",
		const std::wstring& param2 = L"",
		const std::wstring& param3 = L"" );
	void SendCommand( TFSCommand cmd,
		const std::wstring& param1 = L"",
		const std::wstring& param2 = L"",
		const std::wstring& param3 = L"" );
	std::vector<std::wstring> ReadCommandOutput( int& returnCode );
	int getReturnCode( const std::wstring& line );
	bool isLastLine( const std::wstring& line );
	void skipFirstLine();

protected:
	void /*__fastcall*/ GotHostKey();
	int /*__fastcall*/ TranslatePuttyMessage(const TPuttyTranslation *Translation,
		size_t Count, UnicodeString& Message, UnicodeString *HelpKeyword = NULL);
	int /*__fastcall*/ TranslateAuthenticationMessage(UnicodeString& Message, UnicodeString *HelpKeyword = NULL);
	int /*__fastcall*/ TranslateErrorMessage(UnicodeString& Message, UnicodeString *HelpKeyword = NULL);
	void /*__fastcall*/ AddStdErrorLine(const UnicodeString& Str);
	void /*__fastcall*/ /*inline*/ LogEvent(const UnicodeString& Str);
	void /*__fastcall*/ FatalError(UnicodeString Error, UnicodeString HelpKeyword = L"");
	UnicodeString /*__fastcall*/ FormatKeyStr(UnicodeString KeyStr);
	void ParseFingerprint(const UnicodeString& Fingerprint, UnicodeString& SignKeyType, UnicodeString& Hash);
	static Conf /*__fastcall*/ *StoreToConfig(TSessionData *Data, bool Simple);

public:
	/*__fastcall*/ TSecureShell(/*TSessionUI *UI,*/ TSessionData *SessionData/*,
	TSessionLog *Log, TConfiguration *Configuration*/);
	/*__fastcall*/ ~TSecureShell();
	void /*__fastcall*/ Open();
	void /*__fastcall*/ Close();
	void /*__fastcall*/ KeepAlive();
	int /*__fastcall*/ Receive(unsigned char *Buf, int Len);
	bool /*__fastcall*/ Peek(unsigned char *&Buf, int Len);
	std::wstring /*__fastcall*/ ReceiveLine();
	void /*__fastcall*/ Send(const char *Buf, size_t Len);
	void /*__fastcall*/ SendSpecial(int Code);
	void /*__fastcall*/ Idle(unsigned int MSec = 0);
	void /*__fastcall*/ SendLine(const UnicodeString& Line);
	void /*__fastcall*/ SendNull();

	const TSessionInfo& /*__fastcall*/ GetSessionInfo();
	void /*__fastcall*/ GetHostKeyFingerprint(UnicodeString& SHA256, UnicodeString& MD5);
	bool /*__fastcall*/ SshFallbackCmd() const;
	unsigned long /*__fastcall*/ MaxPacketSize();
	void /*__fastcall*/ ClearStdError();
	bool /*__fastcall*/ GetStoredCredentialsTried();
	//  void /*__fastcall*/ CollectUsage();
	bool /*__fastcall*/ CanChangePassword();

	void /*__fastcall*/ RegisterReceiveHandler(void(*Handler)(void*));
	void /*__fastcall*/ UnregisterReceiveHandler(void(*Handler)(void*));

	// interface to PuTTY core
	void /*__fastcall*/ UpdateSocket(SOCKET value, bool Enable);
	void /*__fastcall*/ UpdatePortFwdSocket(SOCKET value, bool Enable);
	void /*__fastcall*/ PuttyFatalError(UnicodeString Error);
	TPromptKind /*__fastcall*/ IdentifyPromptKind(UnicodeString& Name);
	bool /*__fastcall*/ PromptUser(bool ToServer,
		UnicodeString AName, bool NameRequired,
		UnicodeString Instructions, bool InstructionsRequired,
		TStrings *Prompts, TStrings *Results);
	void /*__fastcall*/ FromBackend(const unsigned char *Data, size_t Length);
	void /*__fastcall*/ CWrite(const char *Data, size_t Length);
	void /*__fastcall*/ AddStdError(const char *Data, size_t Length);
	const UnicodeString& /*__fastcall*/ GetStdError();
	void /*__fastcall*/ VerifyHostKey(
		const UnicodeString& Host, int Port, const UnicodeString& KeyType, const UnicodeString& KeyStr,
		const UnicodeString& FingerprintSHA256, const UnicodeString& FingerprintMD5,
		bool IsCertificate, int CACount, bool AlreadyVerified);
	bool /*__fastcall*/ HaveHostKey(UnicodeString Host, int Port, const UnicodeString KeyType);
	void AskAlg(const UnicodeString& AlgType, const UnicodeString& AlgName, int WeakCryptoReason);
	void /*__fastcall*/ DisplayBanner(const UnicodeString& Banner);
	void /*__fastcall*/ PuttyLogEvent(const char *Str);
	UnicodeString /*__fastcall*/ ConvertFromPutty(const char *Str, size_t Length);
	struct callback_set *GetCallbackSet();

	/*__property bool Active = { read = FActive };
	__property bool Ready = { read = GetReady };
	__property TCaptureOutputEvent OnCaptureOutput = { read = FOnCaptureOutput, write = FOnCaptureOutput };
	__property TDateTime LastDataSent = { read = FLastDataSent };
	__property UnicodeString LastTunnelError = { read = FLastTunnelError };
	__property UnicodeString UserName = { read = FUserName };
	__property bool Simple = { read = FSimple, write = FSimple };
	__property TSshImplementation SshImplementation = { read = FSshImplementation };
	__property bool UtfStrings = { read = FUtfStrings, write = FUtfStrings };*/
	TSecureShellMode Mode;
};
//---------------------------------------------------------------------------
} // namespace bcb
