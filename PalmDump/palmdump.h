#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/* Dump a Palm OS .pdb or .prc file. */
int dumpPalmFile(const wchar_t *inname, const wchar_t *outname);

#ifdef __cplusplus
}
#endif // __cplusplus
