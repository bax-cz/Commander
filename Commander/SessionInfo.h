#pragma once

namespace bcb {
struct TSessionInfo
{
	inline TSessionInfo() { LoginTime = Now(); CertificateVerifiedManually = false; }

	TDateTime LoginTime;
	UnicodeString ProtocolBaseName;
	UnicodeString ProtocolName;
	UnicodeString SecurityProtocolName;

	UnicodeString CSCipher;
	UnicodeString CSCompression;
	UnicodeString SCCipher;
	UnicodeString SCCompression;

	UnicodeString SshVersionString;
	UnicodeString SshImplementation;
	UnicodeString HostKeyFingerprintSHA256;
	UnicodeString HostKeyFingerprintMD5;

	UnicodeString CertificateFingerprintSHA1;
	UnicodeString CertificateFingerprintSHA256;
	UnicodeString Certificate;
	bool CertificateVerifiedManually;
};
//---------------------------------------------------------------------------
enum TPromptKind { pkPrompt, pkFileName, pkUserName, pkPassphrase, pkTIS, pkCryptoCard, pkKeybInteractive, pkPassword, pkNewPassword, pkProxyAuth };
enum TLogLineType { llOutput, llInput, llStdError, llMessage, llException };
enum TLogAction
{
	laUpload, laDownload, laTouch, laChmod, laMkdir, laRm, laMv, laCp, laCall, laLs,
	laStat, laChecksum, laCwd, laDifference
};
enum TCaptureOutputType { cotOutput, cotError, cotExitCode };
using TCaptureOutputEvent = std::function<void(const std::wstring& str, TCaptureOutputType outputType)>;
//---------------------------------------------------------------------------
} // namespace bcb
