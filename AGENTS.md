# Potato - Simple Web Browser

Potato is a Windows web browser written in C++ with its own HTML/CSS engine: no webview or embedded browser component; parsing, styling, layout and painting are all done here. [README.md](README.md) says what it supports and how to build it. The repository holds two targets:

- **libwebvis** (`src/libwebvis`) — a standalone HTML/CSS layout and rendering engine, built as a static library. It owns parsing, the cascade, layout and the decision of what to paint where. It owns no window, no socket and no drawing surface, and it does **not** link libdesktop, so other applications can reuse it.
- **potato** (`src/potato`) — the browser itself: window, toolbar, address bar, navigation history, and the libdesktop-backed implementations of the two interfaces libwebvis needs.

## Architecture

- **HTML parsing**: a tokenizer (`html_scanner`) feeds an HTML parser (`parser`) that builds a DOM tree of `element` nodes owned by a `document`.
- **CSS engine**: stylesheets are parsed into selectors (`css`) and property maps (`style`). Lengths are `css_length`, with `calc()` support. Media queries filter rules by screen type.
- **Layout**: elements compute their box model (margins, padding, borders) and position children in block and inline flow (`box`); tables have their own grid layout (`table_grid`). `document::render(max_width)` takes no renderer and needs no window, so layout runs headless; only `document::draw` needs a `renderer`.
- **Rendering**: `renderer` draws text, backgrounds, borders and images through `webvis::device_context`, which potato implements over `render::draw_context`.
- **Networking**: libwebvis never opens a socket. It asks for a URL through `webvis::host::load_resource` and is handed back text or a decoded image. Potato downloads each resource to a temp file with libdesktop's asynchronous HTTP services and completes the request on the UI thread; a page opened from disk has its resources read straight from disk, still completing on the UI thread. The page itself is fetched through its own HTTP session.
- **UI**: `main.cpp` builds a `ui::shell`, libdesktop's one-window application, whose content is an element tree: a drawn navigation toolbar, the address `ui::edit` and a File menu bar in one row, over a scrolling page element. The menu bar is in that row rather than being the shell's own, so `potato_app::key` dispatches its accelerators. Text typed into the address bar is read as a URL, a local path or a bare host name (`example.com` opens `https://example.com`); a link to a fragment of the page on screen scrolls rather than reloads. A `text/plain` response or a local `.txt` file is shown as text, not parsed as markup.
- **Platform layer**: libdesktop's four libraries provide core values, platform services, render contexts and the drawn UI. CMake pulls libdesktop in with `FetchContent`, building against a sibling `../libdesktop` checkout when one exists. Only the **potato** target links it.

## The libwebvis boundary

Everything platform-specific reaches the engine through `src/libwebvis/webvis.h`:

- **`webvis::device_context`** — a *stateless* drawing surface. Every call carries its own coordinates and colours; there is no current pen, brush or transform to set up first. Only painting goes through it, which is what keeps layout headless. `push_clip`/`pop_clip` are the one piece of state and nest strictly.
- **`webvis::host`** — text measurement, resource loading and environment queries (screen size and DPI, URL resolution, charset decoding). Text measurement lives here rather than on the device context *precisely so that layout needs no surface*.

`webvis::set_host()` installs the host before any document is created. The engine also calls `webvis::dispatch_to_ui()`, which the app forwards to the platform task queue.

Two rules keep the split honest:

- `src/libwebvis` contains no libdesktop include and no Windows SDK header. If the engine needs something from the machine, add it to `webvis::host` or `webvis::device_context` and implement it in `src/potato/webvis_host.cpp`.
- The user-agent stylesheet is compiled *into* the library from `src/libwebvis/res/master.css` by `cmake/embed_master_css.cmake`, so a consumer needs no data file alongside it.

Engine code lives in `namespace webvis`. `main.cpp` pulls it in with a using-directive; other consumers should qualify instead.

## Where things live

| Path | What it holds |
|---|---|
| `src/libwebvis/webvis.h` | The public API: the device context and host interfaces, shared value types, font and image handles, `view_host` |
| `src/libwebvis/` | The engine: `core` (CSS enums, `css_length`, string helpers, colour parsing, charset decoding, the unit-test harness), `style` (the CSS engine and the renderer), `element` (DOM nodes and layout), `document` (scanner, parser, document model, the headless layout harness and its tests); `pch.h` is the standard library only, by design |
| `src/potato/` | The browser: `main.cpp` (the app, its element tree, the command-line modes, `dispatch_to_ui`), `webvis_host.*` (fonts, images, HTTP, screen metrics over libdesktop; the SVG placeholder), `res/` (icon and the built-in test page) |
| `tests/` | `portable.cpp` (the engine's platform-free checks) and `smoke.cpp` (a build-only placeholder) |
| `test-files/` | Saved real pages, for `--layout:` and the fixture tests |
| `cmake/` | `embed_master_css.cmake`, and `project-command.ps1`, which backs the project commands in `dd.psd1` |

## Build

```pwsh
.\dd.ps1 build                     # both configurations
.\dd.ps1 build debug
.\dd.ps1 test --label potato       # both configurations, each with the offline suite
.\dd.ps1 clean --dry-run           # dd removes only build trees it can verify
```

The vendored dd driver sets up the x64 MSVC environment itself, so no developer prompt is needed; `.\dd.ps1 doctor` reports anything missing. To drive CMake directly, from an x64 Developer PowerShell: `cmake --preset release`, then `cmake --build --preset release`.

Each preset has its own build tree (`build/debug`, `build/release`), and both link into `Exe/`, which is safe only because the file names differ (`potato-64d.exe`, `potato-64.exe`). Never point them at a shared name. A running copy makes the link fail with LNK1168, and dd does not close it; close it first: `Get-Process potato-64,potato-64d -EA SilentlyContinue | Stop-Process -Force`.

`POTATO_BUILD_APP=OFF` builds libwebvis and its portable checks without fetching libdesktop: an engine build, not a non-Windows browser.

`dd.ps1` and `.dd/` are the [dd](https://github.com/ZacWalk/dd) driver, vendored unmodified with its hashes in [docs/dd-upstream.json](docs/dd-upstream.json); never edit them. Potato's settings are in `dd.psd1`; libdesktop, the one dependency, is pinned by the `GIT_TAG` in `CMakeLists.txt`, not in `cmake/dd-dependencies.json`. `.\dd.ps1 commands --json` lists the project commands, `layout` and `analyze-wiki-css`, both `effects = 'read'`. dd documents its [CLI and MCP modes](https://github.com/ZacWalk/dd/blob/main/docs/mcp.md#two-modes) and [built-in commands](https://github.com/ZacWalk/dd/blob/main/docs/cli.md#commands).

## Testing

**Never launch a visible window to verify a change** — a popping UI steals focus and disrupts other work on the machine. `.\dd.ps1 launch` does exactly that, and so does an unfiltered `.\dd.ps1 test`, whose smoke check opens the browser for a second; use `--label headless` (`potato` remains supported). Every mode below is windowless or offscreen and writes to the console, so an agent can read the result directly.

The executable is a GUI-subsystem binary, so PowerShell does not wait for it — force the wait with `| Out-String`, or use `Start-Process -NoNewWindow -Wait`. Piping to `Select-String` without `| Out-String` first can appear to hang. Redirect **stderr** too when chasing a crash: CRT and STL debug assertions report there and are otherwise invisible.

### `--layout:<path>` — deterministic layout, no window, no message loop

```
Exe\potato-64.exe "--layout:test-files\x.html" -v --repeat:3 --width:1902 --dump:4
```

Reach for this first; `.\dd.ps1 layout --file <path>` runs it too. It lays a local file out through `layout_html_headless` and prints `<path>: <w>x<h> (parse+style N us, layout N us)` per repeat, then a `nodes:` line (element, text and image counts, tree depth, right-most edge, `display:none` subtrees) and an `anomalies:` line. Exit `0`, `11` for an unreadable file, `12` for zero height.

The anomaly scan is the fastest way to find a layout or cascade bug. It reports boxes that overflow the viewport, start left of the origin, have a negative size, are text with no height, or are an `<img>` with no width — and names the offending element, so the cause is usually obvious from the sample lines. It is ancestor-aware: a hidden subtree is not judged, and only the outermost overflowing box is reported. On the saved pages it reports known faults: BBC's `<html>` is wider than the viewport although `<body>` fits, the two Wikipedia articles lay out wider than the viewport, and the Wikipedia main page has an unsized image. Anything else is new.

`--dump:N` prints the box tree to depth N as `{label} [x,y WxH] {display}`. Pair it with a hand-written probe file that bisects a suspect declaration list into numbered variants — that turns "the page looks wrong" into a one-line answer far faster than reading layout code.

The UI queue is never drained, so no asynchronous stylesheet or image is ever applied and **dimensions are exactly reproducible** — the only sound basis for judging a layout change. It also means a fault in how a late stylesheet is applied cannot be reproduced here. `-v`/`--verbose` adds `view_host::diagnostic` (including the `MATCH`/`PARSE_STYLES`/`RENDER` phase split), `resource_started` and `resource_finished`; without it the headless view swallows them, because the base `view_host` methods are empty.

Headless is offline. `potato_app::start` opens the HTTP sessions only once the window is made, so in `--layout:` and the self-test every external stylesheet or image request fails at once: nothing is fetched, no `%TEMP%\pot*` scratch file is left behind, and timings do not depend on the network. A single timing is still noisy; trust the dimensions.

### `--test` — the regression gate

```
Exe\potato-64.exe --test-offline
Exe\potato-64.exe --test
```

Runs the unit suite — libwebvis's tests plus the browser's own, which `register_shell_tests` in `main.cpp` adds to the same table — prints every case, failure detail and actual pass/fail totals, and writes a supplementary HTML report to `%TEMP%\potXXXX.tmp`. `--test-offline` stops there; it is what `dd test --label headless` and `--label potato` run. `--test` then fetches four live URLs, so it also fails when the network is down, for reasons unrelated to a change; CTest runs it as `potato_network` under the `network` label. Exit `0`, `10` on a unit failure or empty run, or `1`–`6` for a failed fetch.

Two kinds of layout test live in the suite. The **fixture** tests assert the exact size of the pages in `test-files/`, and skip when that folder is absent. They are a change detector, not a statement that the page is correct — a recorded size can just as easily encode a bug, so never treat one as proof; when a deliberate fix moves them, re-record them. The **behavioural** tests use `layout_html_headless_probe(html, width, id)`, which lays out an inline snippet and returns the box of one element by id. Add one for every cascade or layout bug fixed: they are tiny, hermetic, and state the actual contract rather than a number that drifts.

### `--eval:<url>` — the live pipeline, offscreen

```
Exe\potato-64.exe "--eval:https://example.com/"
```

Runs the real UI path — window, message loop, asynchronous resource loading — with the window created offscreen and never activated, so it paints and lays out without appearing or taking focus. It logs `[  N ms] message` and closes after 5 s of quiet or a 60 s cap, ending with `Evaluation complete: pending=, failed=, timed_out=`.

Use it only when a bug genuinely needs the network or the UI path. Results are **not reproducible**: asynchronous arrival order varies, so the number of layout passes and the final height change from run to run. Never treat an `--eval:` delta as evidence of a regression — save the page into `test-files/` and use `--layout:` instead.

## Threading model

One thread of our own, plus whatever libdesktop's asynchronous HTTP services use internally:

- **UI thread** — all DOM, style, layout and painting. It runs the message loop in `platform::run`, which drains the `platform::run_ui` tasks.
- **HTTP worker threads** — run the HTTP callbacks. They never touch the DOM; every result reaches the UI thread through `platform::run_ui`, which always queues, never runs the task inline and never waits for it.

The locks are short leaf-level guards around a container: `document::m_fonts_mutex`, and the HTTP client's request list and per-request mutexes in `webvis_host.cpp`. The invariant to preserve:

> No lock is ever held across a callout, a queued task, or another lock.

Font creation and the HTTP completion path both release their lock before calling out; keep it that way. With no lock held while waiting, a lock-ordering deadlock is not possible — so if the app freezes, suspect an infinite loop or memory corruption. A corrupted container walked by a tree or list traversal can spin forever, and a use-after-free presents as either a hang or an access violation.

## Performance notes

Release keeps the optimisation flags in `CMakeLists.txt` because these measurements were taken against them. Measure before optimising — `--layout:<file> -v --repeat:3` prints the phase split — rather than guessing:

- Styling, not layout, is the cost. On the BBC fixture, `MATCH` and `PARSE_STYLES` each take several times as long as layout (`RENDER`), and the first pass, which also creates the fonts, is far slower than the passes after it.
- `element::parse_styles` resolves every property of every element from its declaration string through `get_style_property`; nothing is parsed into a typed value once at cascade time.
- `element::apply_stylesheet` collects its candidate selectors as `shared_ptr`s — an atomic reference count per candidate — and sorts them twice when they come from more than one bucket.
- `parse_styles` calls `props_mut()` for every element, so every element allocates its `css_props` even when nothing is set.
- Returning `string_view` from `get_style_property` was measured and rejected: few of its results are long enough to allocate, so it saves little for a change at every call site.

## Key conventions

- **Naming**: `lowercase_separated_by_underscores` for all identifiers — classes, functions, methods, variables, enum values. Member variables take an `m_` prefix, private members `_`.
- **File headers**: begin every source file with a `//` comment saying what it is for.
- **Layout**: `.clang-format` sets it — tabs, Allman braces, 120 columns. `.\dd.ps1 fmt` applies it to `include`, `src` and `tests` only, and needs clang-format on `PATH`, the only place dd looks; Visual Studio installs one in `VC\Tools\Llvm\x64\bin`. A hand-laid table, such as a run of one-line key dispatches, is fenced with `// clang-format off` and `// clang-format on`.
- **Warnings**: MSVC builds every target at `/W4` without a warning. `.clang-tidy` holds the bug-finding and performance checks the code is linted with.
- **UTF-8 `std::string` everywhere.** No `std::wstring` or `wchar_t` in this repository; libdesktop converts at the Windows SDK boundary.
- Compare CSS and HTML vocabulary case-insensitively with the portable ASCII `is_equal` / `icmp` / `nicmp` helpers and the `ltstr` / `ltstr_sv` comparators, never locale-dependent CRT comparisons.
- CSS keyword sets are semicolon-delimited string tables (such as `border_style_strings`) searched by `value_index`; `prop_id` / `prop_id_strings` do the same for property names, and a property absent from that table is discarded at parse time.
- Internal CSS properties use the `-potato-` vendor prefix.
- No external HTML or CSS library — everything is self-contained in `src/`.

### Splitting CSS text

`split_string` never splits inside quotes or inside `( )`. Real stylesheets rely on that — `url(data:image/svg+xml;base64,...)` carries the declaration separator, `margin: calc(1px + 2px) 0` carries the value separator, and `:not(a, b)` carries the selector separator. Selector parsing has its own depth-aware `find_last_combinator` for the same reason: `:nth-child(2n+1)` must not split at its `+`. A new splitter must respect nesting too, or it reintroduces a whole family of silent bugs.
