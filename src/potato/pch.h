// pch.h - Precompiled header for the potato application. Includes the
// libdesktop abstraction and the libwebvis public API. Intentionally does NOT
// include any Windows SDK headers (windows.h, windowsx.h, commctrl.h,
// shlwapi.h, strsafe.h, winhttp.h); those live only in the libdesktop backend.

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
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "platform.h"
#include "webvis.h"
