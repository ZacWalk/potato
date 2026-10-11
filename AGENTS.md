# Potato - Simple Web Browser

Potato is a lightweight Win32 web browser written in C++ that implements its own HTML/CSS rendering engine. It does not use a webview or embedded browser component — all parsing, styling, layout, and painting are done from scratch.

The repository holds two targets:

- **libwebvis** (`src/libwebvis`) — a standalone HTML/CSS layout and rendering
  engine, built as a static library. It owns parsing, the cascade, layout and
  the decision of what to paint where. It owns no window, no socket and no
  drawing surface, and it does **not** link platform-h. It is meant to be
  reused by other apps in this workspace.
- **potato** (`src/potato`) — the browser itself: window, toolbar, address bar,
  navigation history, and the platform-h backed implementations of the two
  interfaces libwebvis needs.

## Architecture

- **HTML parsing**: A tokenizer (`scanner`) feeds an HTML parser (`parser`) that builds a DOM tree of `element` nodes owned by a `document`.
- **CSS engine**: Stylesheets are parsed into selectors (`css`) and property maps (`style`). CSS values use `css_length` with `calc()` support. Media queries filter rules by screen type.
- **Layout**: Elements compute their box model (margins, padding, borders) and position children using block/inline flow (`box`). Tables have dedicated grid layout (`table_grid`). Layout is independent of any drawing surface — `document::render(max_width)` takes no renderer and needs no window, so it can run headless. Only `document::draw` requires a `renderer`.
- **Rendering**: `renderer` draws text, backgrounds, borders and images through `webvis::device_context`, which potato implements over `pf::draw_context`.
- **Networking**: libwebvis never opens a socket. It asks for a URL through
  `webvis::host::load_resource` and is handed back either text or a decoded
  image. Potato backs that with an async WinHTTP client that downloads to a
  temp file and marshals the completion to the UI thread with `pf::run_ui`;
  the resources of a page opened from disk are read straight from disk, still
  completing on the UI thread.
- **UI**: `main.cpp` builds the frame: `content_reactor` hosts the rendered document with scrolling (wheel, scrollbar, and the arrow, Page, Space, Home and End keys once the page has focus), and `main_frame_reactor` owns the toolbar, address bar, menu and navigation history. Text typed into the address bar is read as a URL, a local path or a bare host name (`example.com` opens `https://example.com`); a link to a fragment of the page on screen scrolls rather than reloads. A `text/plain` response or a local `.txt` file is shown as text, not parsed as markup.
- **Platform layer**: the `pf::` surface (windows, drawing, fonts, files, HTTP)
  lives in the separate **platform-h** repository, shared with the other apps in
  this workspace. CMake pulls it in with `FetchContent`, building against a
  sibling `../platform-h` checkout when one exists. Only the **potato** target
  links it.

## The libwebvis boundary

Everything platform-specific reaches the engine through `src/libwebvis/webvis.h`:

- **`webvis::device_context`** — a *stateless* drawing surface. Every call
  carries its own coordinates and colours; there is no current pen, brush or
  transform to set up first. Only painting goes through it, which is what keeps
  layout headless. `push_clip`/`pop_clip` are the one piece of state and nest
  strictly.
- **`webvis::host`** — text measurement, resource loading and environment
  queries (screen size/DPI, URL resolution, charset decoding). Text measurement
  lives here rather than on the device context *precisely so that layout needs
  no surface*.

`webvis::set_host()` installs the host before any document is created. The
engine also calls `webvis::dispatch_to_ui()`, which the app forwards to the
platform task queue.

Two rules keep the split honest:

- `src/libwebvis` must contain no `pf::`, no `platform.h` and no Windows SDK
  header. Grep for `pf::` there; the answer must stay empty.
- The user-agent stylesheet is compiled *into* the library from
  `src/libwebvis/res/master.css` by `cmake/embed_master_css.cmake`, so a
  consumer needs no data file alongside it.

Engine code lives in `namespace webvis`. `main.cpp` pulls it in with a
using-directive; other consumers should qualify instead.

## Project structure

```
CMakeLists.txt       — Build definition (MSVC + Ninja). Declares the webvis
                       static library and the potato app. platform_add_app()
                       generates the icon, manifest, version info and embedded
                       res/ files, so there is no .rc and no resource.h
CMakePresets.json    — `debug` and `release` configure/build presets
dd.ps1               — Developer commands: build, run, test, layout, clean
cmake/
└── embed_master_css.cmake — Compiles res/master.css into the library

src/libwebvis/       — The engine. No platform dependency.
├── webvis.h         — PUBLIC API: the device_context and host interfaces, the
│                      shared value types (position, size, web_color,
│                      font_metrics), font/image handles, and view_host
├── pch.h            — Precompiled header: STL only, by design
├── core.h           — CSS enums, css_length with calc() support, string
│                      helpers, the unit-test harness
├── core.cpp         — CSS colour parsing (hex/rgb/hsl/named), value_index
│                      lookups, split_string, byte-stream charset decoding,
│                      and the host global
├── style.h          — CSS engine: border/background structs, the renderer,
│                      style property map, media queries, CSS selectors with
│                      specificity, stylesheet container
├── style.cpp        — CSS shorthand parsing, media query evaluation, selector
│                      parsing and matching, at-rule handling, and painting of
│                      text, borders, backgrounds and images
├── element.h        — DOM element node, box layout model (block/inline),
│                      table grid layout, tree iterators
├── element.cpp      — Element lifecycle, CSS property resolution,
│                      block/inline/flex/table layout, float positioning,
│                      background/border painting, selector matching
├── document.h       — HTML scanner/tokenizer, HTML parser with implicit tag
│                      closing, and the document model that owns the DOM tree
│                      and manages fonts/stylesheets
├── document.cpp     — HTML entity table, the parser, document rendering, font
│                      caching, stylesheet application, and the headless
│                      layout harness plus its tests
└── res/
    └── master.css   — Default user-agent stylesheet (compiled into the lib)

src/potato/          — The browser. Implements what libwebvis asks for.
├── pch.h            — Precompiled header: STL, platform.h and webvis.h.
│                      Contains NO Windows SDK headers by design
├── targetver.h      — Windows SDK version targeting
├── webvis_host.h    — potato_device_context, potato_host, potato_image
├── webvis_host.cpp  — GDI fonts, WIC image decoding, the async WinHTTP
│                      client, the SVG placeholder, screen metrics
├── main.cpp         — app_init, the browser frame and content view, the CLI
│                      modes (--test, --layout, --eval) and dispatch_to_ui
└── res/
    ├── potato.ico   — Application icon (embedded via ICON)
    └── test.htm     — Built-in test page (embedded via EMBED)
```

## Build

```
.\dd.ps1 build              # both configs
.\dd.ps1 build debug
```

The vendored dd driver sets up the x64 MSVC environment itself, so no developer
prompt is needed; `.\dd.ps1 doctor` reports anything missing.

To drive CMake directly, from an x64 Developer PowerShell:

```
cmake --preset release
cmake --build --preset release
```

Each preset gets its own build tree (`build/debug`, `build/release`); both link
into `Exe/`, which is safe only because the file names differ (`potato-64d.exe`
and `potato-64.exe`). Never point them at a shared name.

Other commands:

```
.\dd.ps1 launch                    # build Release, then start the browser
.\dd.ps1 test --label potato       # build both configs, then the offline self-test
.\dd.ps1 test                      # also the network self-test and a window smoke check
.\dd.ps1 layout --file test-files\x.html --width 1902 --dump 4
.\dd.ps1 clean --dry-run           # dd removes only build trees it can verify
```

The dd driver is vendored from [ZacWalk/dd](https://github.com/ZacWalk/dd) as
`dd.ps1` and `.dd/`; [docs/dd-upstream.json](docs/dd-upstream.json) records the
hashes. Potato-specific configuration is in `dd.psd1`; platform-h, the one
dependency, is pinned by the `GIT_TAG` in `CMakeLists.txt`, not in
`cmake/dd-dependencies.json`. `.\dd.ps1 commands --json` prints this
project's command list. Upstream dd owns the documentation for
[CLI and MCP mode](https://github.com/ZacWalk/dd/blob/main/docs/mcp.md#two-modes)
and for [built-in commands](https://github.com/ZacWalk/dd/blob/main/docs/cli.md#commands).

Project commands ported from the pre-dd driver; both declare `effects = 'read'`:

```pwsh
.\dd.ps1 layout --file test-files/wikipedia-main-page.html --width 1200
.\dd.ps1 analyze-wiki-css
```

## Testing

**Never launch a visible window to verify a change** — a popping UI steals focus
and disrupts other work on the machine. `.\dd.ps1 launch` does exactly that, and
so does an unfiltered `.\dd.ps1 test`, whose smoke check opens the browser for a
second; use `--label potato`. Every mode below is windowless or offscreen, and
every one writes to the console, so an agent can read the result directly. No
mode requires a human to look at a screen.

All three attach to the parent console via `AttachConsole` and rebind the std
handles, so output is readable either inline or redirected. The exe is a
GUI-subsystem binary, so PowerShell does not wait for it by default — force the
wait with `| Out-String`, or use `Start-Process -NoNewWindow -Wait`. Piping to
`Select-String` without `| Out-String` first can appear to hang. Redirect
**stderr** too when chasing a crash: CRT and STL debug assertions report there
and are otherwise invisible.

### `--layout:<path>` — deterministic layout, no window, no message loop

```
Exe\potato-64.exe "--layout:test-files\x.html" -v --repeat:3 --width:1902 --dump:4
```

Reach for this first. Lays a local file out through `layout_html_headless` and
prints `<path>: <w>x<h> (parse+style N us, layout N us)` per repeat, followed by
a `nodes:` line (element/text/image counts, tree depth, right-most edge, count of
`display:none` subtrees) and an `anomalies:` line. Exit `0`, `11` unreadable
file, `12` zero height.

The anomaly scan is the fastest way to find a layout or cascade bug. It reports
boxes that overflow the viewport, start left of the origin, have a negative
size, are text with no height, or are an `<img>` with no width — and it names
the offending element, so the cause is usually obvious from the sample lines.
It is ancestor-aware: a hidden subtree is not judged, and only the outermost
overflowing box is reported rather than every descendant it drags with it.

`--dump:N` prints the box tree to depth N as `{label} [x,y WxH] {display}`.
Pair it with a hand-written probe file that bisects a suspect declaration list
into numbered variants — that turns "the page looks wrong" into a one-line
answer far faster than reading layout code.

Because the UI queue is never drained, no async stylesheet or image can ever be
applied, so **dimensions are exactly reproducible** — this is the only sound
basis for judging a layout change. `-v`/`--verbose` adds `view_host::diagnostic`
(including the `MATCH`/`PARSE_STYLES`/`RENDER` phase split), `resource_started`
and `resource_finished`; without it the headless view swallows all of them,
because the base `view_host` methods are empty.

Headless is offline. `app_init` opens the HTTP session only when it starts the
window, so in `--layout:` and the self-test every external stylesheet or image
request fails at once: nothing is fetched, no `%TEMP%\pot*` scratch file is left
behind, and timings no longer depend on the network. (The live checks of
`--test` use their own session.) A single timing is still noisy; trust the
dimensions.

### `--test` — the regression gate

```
Exe\potato-64.exe --test
Exe\potato-64.exe --test-offline
```

Runs the unit suite and writes an HTML report to `%TEMP%\potXXXX.tmp`; prints
`Unit tests: PASS|FAIL` plus the report path. Exit `0`, or `10` on unit failure.
The suite is libwebvis's (`webvis::run_tests`) plus the browser's own cases,
which `register_shell_tests` in `main.cpp` adds to the same table — address-bar
parsing, plain-text pages and the download scratch files.
It then fetches four live URLs, so it also fails when the network is down, for
reasons unrelated to a change. `--test-offline` stops after the unit suite; it is
what `dd test --label potato` runs, while the full `--test` runs as
`potato_network` under the `network` label.

Two kinds of layout test live here. The **fixture** tests assert the exact size
of the pages in `test-files/`; they silently skip when that folder is absent.
They are a change detector, not a statement that the page is correct — a
recorded size can just as easily encode a bug, so never treat one as proof.
When a deliberate fix moves them, re-record them. The **behavioural** tests use
`layout_html_headless_probe(html, width, id)`, which lays out an inline snippet
and returns the box of one element by id. Prefer adding one of these for every
cascade or layout bug fixed: they are tiny, hermetic, and they state the actual
contract rather than a number that drifts.

### `--eval:<url>` — the live pipeline, offscreen

```
Exe\potato-64.exe "--eval:https://example.com/"
```

Runs the real UI path — window, message loop, async resource loading — but the
window is parked at `-32000,-32000` with `SW_SHOWNOACTIVATE`, so it paints and
lays out without ever appearing or taking focus. Logs `[  N ms] message` and
self-terminates after 5 s of quiet or a 60 s cap, ending with
`Evaluation complete: pending=, failed=, timed_out=`.

Use it only when a bug genuinely needs the network or UI path. Results are **not
reproducible**: the same binary has produced both 5 and 8 layout passes and
final heights differing by ~30px, because async arrival order varies. Never
treat an `--eval:` delta as evidence of a regression — save the page into
`test-files/` and use `--layout:` instead.

### Build hygiene

A running instance makes the link fail with LNK1168. `dd.ps1` kills stale
instances for you; when driving CMake directly, do it yourself:
`Get-Process potato-64,potato-64d -EA SilentlyContinue | Stop-Process -Force`

## Threading model

One thread of our own, plus whatever WinHTTP uses internally:

- **UI thread** — all DOM, style, layout and painting. Runs the message loop in
  `pf::platform_run`, draining `ui_tasks` whenever `ui_event_h` signals.
- **WinHTTP pool threads** — invoke the async HTTP callbacks. They never touch
  the DOM; every result is handed to the UI thread with `pf::run_ui`, which
  always queues, never runs the task inline and never waits for it.

Locks exist, but every one is a short leaf-level guard around a container
(`cs_ui`, the HTTP client's request list, `document::m_fonts_mutex`, the
WinHTTP request mutexes). The invariant to preserve:

> No lock is ever held across a callout, a queued task, or another lock.

Font creation and the HTTP completion path both deliberately release their lock
before calling out. Preserve that when editing. Since nothing blocks waiting on
another thread, a lock-ordering deadlock is not possible by construction — so if
the app freezes, suspect an infinite loop or memory corruption, not a deadlock.
A corrupted container walked by a tree/list traversal can spin forever, and a
use-after-free presents as either a hang or an access violation.

## Performance notes

Release keeps the optimisation flags in `CMakeLists.txt` because the style
measurements in [todo.md](todo.md#performance) were taken against them.
Measure before optimising: those measurements found no single hotspot, so
intuition is not a guide.

## Key conventions

- **Naming**: `lowercase_separated_by_underscores` for all identifiers — classes, functions, methods, variables, enum values. Member variables use `m_` prefix, private members use `_` prefix.
- **File headers**: Every source file begins with a `//` comment describing its purpose.
- **UTF-8 `std::string` everywhere.** `std::wstring` exists only inside the platform layer and the `pf::utf8_to_utf16` / `utf16_to_utf8` helpers, which convert at the Windows SDK boundary. Never let `wchar_t` leak into the engine.
- Case-insensitive comparison via `is_equal` / `icmp` (`_stricmp`, `_strnicmp`) and the `ltstr` / `ltstr_sv` container comparators.
- CSS keyword sets are semicolon-delimited string tables (e.g. `border_style_strings`) searched by `value_index`; `prop_id` / `prop_id_strings` do the same for property names, and anything absent from that table is discarded at parse time.
- Custom CSS properties use the `-potato-` vendor prefix.
- No external HTML/CSS library dependencies — everything is self-contained in `src/`.
- **libwebvis stays platform-free.** If the engine needs something from the
  machine, add it to `webvis::host` or `webvis::device_context` and implement it
  in `src/potato/webvis_host.cpp` — never reach for `pf::` from `src/libwebvis`.

### Splitting CSS text

`split_string` never splits inside quotes or inside `( )`. Real stylesheets rely
on that — `url(data:image/svg+xml;base64,...)` carries the declaration
separator, `margin: calc(1px + 2px) 0` carries the value separator, and
`:not(a, b)` carries the selector separator. Selector parsing has its own
depth-aware `find_last_combinator` for the same reason: `:nth-child(2n+1)` must
not split at its `+`. If you add a new splitter, respect nesting or you will
reintroduce a whole family of silent bugs.
