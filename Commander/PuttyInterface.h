#pragma once

#include <string>
#include <memory>

#ifdef __cplusplus
extern "C" {
#endif

#include "../Putty/putty.h"
#include "../Putty/version.h"

// To rename ssh1_cipheralg::new member, what is a keyword in C++
#define new _new_
#include "../Putty/ssh.h"
#undef new
#include "../Putty/puttyexp.h"
#include "../Putty/proxy/proxy.h"
#include "../Putty/storage.h"

// Defined in marshal.h - Conflicts with xml.xmldom.hpp
#undef get_data
#undef put_data

#ifdef __cplusplus
}
#endif

#include "bcb_stuff.h"
#include "PuttyTools.h"
#include "SecureShell.h"

namespace bcb {
void PuttyInitialize();
void PuttyFinalize();
void DontSaveRandomSeed();

UnicodeString GetCipherName(const ssh_cipher *Cipher);
UnicodeString GetCompressorName(const ssh_compressor *Compressor);
UnicodeString GetDecompressorName(const ssh_decompressor *Decompressor);
void PuttyDefaults(Conf *conf);
int GetCipherGroup(const ssh_cipher *TheCipher);
//---------------------------------------------------------------------------
class TSecureShell;
struct ScpSeat : public Seat
{
	TSecureShell *SecureShell;

	ScpSeat(TSecureShell *SecureShell);
};
//---------------------------------------------------------------------------
extern THierarchicalStorage *PuttyStorage;
//---------------------------------------------------------------------------
} // namespace bcb
