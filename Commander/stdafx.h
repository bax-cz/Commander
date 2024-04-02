// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN // exclude rarely-used stuff from Windows headers

// Windows header files
#include <windows.h>
#include <CommCtrl.h>
#include <shellapi.h>
#include <Shlwapi.h>
#include <ShlObj.h>
#include <atlbase.h>

// Fastcopy header files
#include <wincrypt.h>
#include <Commdlg.h>

// 7-zip header files
#include "Common/Common.h"

// C runtime header files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// STL header files
#include <sstream>
#include <string>
#include <vector>
#include <map>
