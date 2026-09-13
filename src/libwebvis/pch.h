// pch.h - Precompiled header for libwebvis. STL only: the library is a
// standalone rendering and layout engine, so this deliberately pulls in no
// platform headers at all. Everything the engine needs from the host arrives
// through the interfaces in webvis.h.

#pragma once

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <deque>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "webvis.h"
#include "core.h"
