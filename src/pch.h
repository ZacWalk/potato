// pch.h - Precompiled header. Includes the platform abstraction header and
// STL headers. Intentionally does NOT include any Windows SDK headers
// (windows.h, windowsx.h, commctrl.h, shlwapi.h, strsafe.h, winhttp.h);
// those live only in the platform-h backend.

#pragma once

#include "targetver.h"


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

#include "platform.h"
#include "core.h"
