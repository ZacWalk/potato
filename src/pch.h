// pch.h - Precompiled header. Includes Windows SDK, GDI+, WinHTTP, STL headers,
// and the core types header shared by all translation units.

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define ISOLATION_AWARE_ENABLED 1
#define GDIPVER 0x0110

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <winhttp.h>
#include <gdiplus.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <deque>
#include <format>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core.h"
