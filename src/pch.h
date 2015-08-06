// pch.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN    // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <stdint.h>

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#define _ATL_NO_COM
#define _ATL_NO_COM_SUPPORT
#define _ATL_NO_PERF_SUPPORT
#define _ATL_APARTMENT_THREADED
#define _ATL_ALL_WARNINGS

#include <atlbase.h>
#include <atlstr.h>
#include <atlwin.h>
#include <AtlSync.h>

#define ISOLATION_AWARE_ENABLED 1
#define GDIPVER 0x0110



#include <commctrl.h>
#include <ctype.h>
#include <functional>
#include <gdiplus.h>
#include <malloc.h>
#include <math.h>
#include <memory.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include <tchar.h>
#include <winhttp.h>
#include <winsock2.h>
#include <assert.h>

#undef min
#undef max

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <locale>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>


#include "types.h"
#include "Geometry.h"



