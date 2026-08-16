# Potato - Simple Web Browser

Potato is a lightweight Win32 web browser written in C++ that implements its own HTML/CSS rendering engine. It does not use a webview or embedded browser component — all parsing, styling, layout, and painting are done from scratch.

## Architecture

- **HTML parsing**: A tokenizer (`scanner`) feeds an HTML parser (`parser`) that builds a DOM tree of `element` nodes owned by a `document`.
- **CSS engine**: Stylesheets are parsed into selectors (`css`) and property maps (`style`). CSS values use `css_length` with `calc()` support. Media queries filter rules by screen type.
- **Layout**: Elements compute their box model (margins, padding, borders) and position children using block/inline flow (`box`). Tables have dedicated grid layout (`table_grid`). Layout is independent of any drawing surface — `document::render(max_width)` takes no renderer and needs no window, so it can run headless. Only `document::draw` requires a `render_win32`.
- **Rendering**: `render_win32` draws text, backgrounds, borders and images through the `pf::draw_context` abstraction, which platform-h implements over GDI and WIC.
- **Networking**: An async WinHTTP client (`http`) fetches pages, CSS and images. Every result is marshalled back to the UI thread with `pf::run_ui`.
- **UI**: `main.cpp` builds the frame: `content_reactor` hosts the rendered document with scrolling, and `main_frame_reactor` owns the toolbar, address bar, menu and navigation history.
- **Platform layer**: the `pf::` surface (windows, drawing, fonts, files, HTTP)
  lives in the separate **platform-h** repository, shared with the other apps in
  this workspace. CMake pulls it in with `FetchContent`, building against a
  sibling `../platform-h` checkout when one exists.

## Project structure

```
CMakeLists.txt       — Build definition (MSVC + Ninja). Declares the app with
                       platform_add_app(): icon, manifest, version info and the
                       embedded res/ files are generated, so there is no .rc
                       and no resource.h in this repo
CMakePresets.json    — `debug` and `release` configure/build presets
dd.ps1               — Developer commands: build, run, test, layout, clean

src/
├── pch.h            — Precompiled header: STL, platform.h and core.h. Contains
│                      NO Windows SDK headers by design
├── targetver.h      — Windows SDK version targeting
├── core.h           — Foundational types: geometry (position, recti, size_i),
│                      CSS enums, css_length with calc() support, web_color,
│                      string helpers, font metrics, the unit-test harness
├── core.cpp         — CSS colour parsing (hex/rgb/hsl/named), value_index
│                      lookups, split_string, byte-stream charset decoding
├── style.h          — CSS engine: border/background structs, the render_win32
│                      renderer, style property map, media queries, CSS
│                      selectors with specificity, stylesheet container
├── style.cpp        — CSS shorthand parsing, media query evaluation, selector
│                      parsing and matching, at-rule handling, and painting of
│                      text, borders, backgrounds and images
├── element.h        — DOM element node, box layout model (block/inline),
│                      table grid layout, tree iterators
├── element.cpp      — Element lifecycle, CSS property resolution,
│                      block/inline/flex/table layout, float positioning,
│                      background/border painting, selector matching
├── document.h       — Async HTTP client, HTML scanner/tokenizer, HTML parser
│                      with implicit tag closing, and the document model that
│                      owns the DOM tree and manages fonts/stylesheets
├── document.cpp     — Downloads, HTML entity table, the parser, document
│                      rendering, font caching, stylesheet application, and the
│                      headless layout harness plus its tests
├── main.cpp         — app_init, the browser frame and content view, the CLI
│                      modes (--test, --layout, --eval) and dispatch_to_ui
└── res/
    ├── master.css   — Default user-agent stylesheet (embedded via EMBED)
    ├── potato.ico   — Application icon (embedded via ICON)
    └── test.htm     — Built-in test page (embedded via EMBED)
```

## Build

```
.\dd.ps1 build              # Release (default)
.\dd.ps1 build -Config Debug
```

`dd.ps1` locates Visual Studio with vswhere, enters the x64 MSVC environment,
and falls back to the CMake and Ninja that ship with VS when neither is on
PATH — so nothing extra needs installing.

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
.\dd.ps1 run                       # build, then launch the browser
.\dd.ps1 test                      # build, then run the regression gate
.\dd.ps1 layout test-files\x.html --width:1902 --dump:4
.\dd.ps1 clean                     # remove build/ and Exe/
```

## Testing

**Never launch a visible window to verify a change** — a popping UI steals focus
and disrupts other work on the machine. `.\dd.ps1 run` does exactly that, so it
is a command for the human, not a verification step. Every mode below is
windowless or offscreen, and every one writes to the console, so an agent can
read the result directly. No mode requires a human to look at a screen.

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

Headless is **not** offline. The document constructor calls `m_http.open()`, so
external CSS and images still issue real WinHTTP requests whose results merely
never land, and **timings vary with network weather even though dimensions do
not**. Trust the dimensions; treat a single timing as noisy.

### `--test` — the regression gate

```
Exe\potato-64.exe --test
```

Runs the unit suite and writes an HTML report to `%TEMP%\potXXXX.tmp`; prints
`Unit tests: PASS|FAIL` plus the report path. Exit `0`, or `10` on unit failure.
It then fetches four live URLs, so it also fails when the network is down, for
reasons unrelated to a change.

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
(`cs_ui`, `http::m_mutex`, `document::m_fonts_mutex`, the WinHTTP request
mutexes). The invariant to preserve:

> No lock is ever held across a callout, a queued task, or another lock.

Font creation and the HTTP completion path both deliberately release their lock
before calling out. Preserve that when editing. Since nothing blocks waiting on
another thread, a lock-ordering deadlock is not possible by construction — so if
the app freezes, suspect an infinite loop or memory corruption, not a deadlock.
A corrupted container walked by a tree/list traversal can spin forever, and a
use-after-free presents as either a hang or an access violation.

## Key conventions

- **Naming**: `lowercase_separated_by_underscores` for all identifiers — classes, functions, methods, variables, enum values. Member variables use `m_` prefix, private members use `_` prefix.
- **File headers**: Every source file begins with a `//` comment describing its purpose.
- **UTF-8 `std::string` everywhere.** `std::wstring` exists only inside the platform layer and the `pf::utf8_to_utf16` / `utf16_to_utf8` helpers, which convert at the Windows SDK boundary. Never let `wchar_t` leak into the engine.
- Case-insensitive comparison via `is_equal` / `icmp` (`_stricmp`, `_strnicmp`) and the `ltstr` / `ltstr_sv` container comparators.
- CSS keyword sets are semicolon-delimited string tables (e.g. `border_style_strings`) searched by `value_index`; `prop_id` / `prop_id_strings` do the same for property names, and anything absent from that table is discarded at parse time.
- Custom CSS properties use the `-potato-` vendor prefix.
- No external HTML/CSS library dependencies — everything is self-contained in `src/`.

### Splitting CSS text

`split_string` never splits inside quotes or inside `( )`. Real stylesheets rely
on that — `url(data:image/svg+xml;base64,...)` carries the declaration
separator, `margin: calc(1px + 2px) 0` carries the value separator, and
`:not(a, b)` carries the selector separator. Selector parsing has its own
depth-aware `find_last_combinator` for the same reason: `:nth-child(2n+1)` must
not split at its `+`. If you add a new splitter, respect nesting or you will
reintroduce a whole family of silent bugs.
