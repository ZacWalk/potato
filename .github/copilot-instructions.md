# Potato - Simple Web Browser

Potato is a lightweight Win32 web browser written in C++ that implements its own HTML/CSS rendering engine. It does not use a webview or embedded browser component — all parsing, styling, layout, and painting are done from scratch.

## Architecture

- **HTML parsing**: A tokenizer (`scanner`) feeds an HTML parser (`parser`) that builds a DOM tree of `element` nodes owned by a `document`.
- **CSS engine**: Stylesheets are parsed into selectors (`css`) and property maps (`style`). CSS values use `css_length` with `calc()` support. Media queries filter rules by screen type.
- **Layout**: Elements compute their box model (margins, padding, borders) and position children using block/inline flow (`box`). Tables have dedicated grid layout (`table_grid`).
- **Rendering**: `render_win32` draws text, backgrounds, borders, and images using GDI+.
- **Networking**: An async WinHTTP client (`http`) fetches pages, CSS, and images. A task queue (`tasks`) marshals callbacks to the UI thread.
- **UI**: `html_view` is the main window hosting the rendered document with scrolling and navigation. `toolbar` provides the address bar and controls. `web_history` tracks back/forward navigation.

## Project structure

```
src/
├── pch.h            — Precompiled header: Windows SDK, GDI+, WinHTTP, STL,
│                      and core types header shared by all translation units
├── pch.cpp          — Precompiled header compilation unit
├── targetver.h      — Windows SDK version targeting
├── resource.h       — Resource identifiers for icons, menus, accelerators,
│                      toolbar buttons, and dialog controls
├── potato.rc        — Resource script (icons, menus, dialogs)
├── core.h           — Foundational types and utilities: geometry (position,
│                      recti, size_i), CSS enums, css_length with calc()
│                      support, web_color, string helpers, font metrics
├── core.cpp         — String normalization, CSS color parsing (hex/rgb/hsl/
│                      named), value_index lookups, split_string, text_match
│                      rendering
├── style.h          — CSS engine: border/background structs, render_win32
│                      GDI+ renderer, style property map, media queries,
│                      CSS selectors with specificity, stylesheet container
├── style.cpp        — CSS shorthand parsing, media query evaluation, selector
│                      matching, at-rule handling, stylesheet loading, GDI+
│                      rendering of text, borders, backgrounds, and images
├── element.h        — DOM element node, box layout model (block/inline),
│                      table grid layout, tree iterators
├── element.cpp      — Element lifecycle, CSS property resolution,
│                      block/inline/table layout, float positioning,
│                      background/border painting, selector matching
├── document.h       — Async HTTP client (WinHTTP), HTML scanner/tokenizer,
│                      HTML parser with implicit tag closing, document model
│                      that owns the DOM tree and manages fonts/stylesheets
├── document.cpp     — WinHTTP async downloads, HTML entity table, parser
│                      with implicit tag closing, document rendering, font
│                      caching via GDI+, stylesheet application
├── ui.h             — Win32 window framework (base_window CRTP,
│                      subclassed_window), task queue for UI thread
│                      marshaling, web_history, html_view rendering host,
│                      toolbar with address bar, autocomplete UI
├── main.cpp         — Application entry point (wWinMain), main frame window,
│                      html_view implementation, web_history, toolbar command
│                      wiring, and the message loop
└── res/
    ├── master.css   — Default user-agent stylesheet
    └── test.htm     — Built-in test page
```

## Build

```
msbuild potato.sln /p:Configuration=Debug /p:Platform=x64 /m
```

Requires Visual Studio with C++ desktop workload and Windows SDK.

## Key conventions

- **Naming**: `lowercase_separated_by_underscores` for all identifiers — classes, functions, methods, variables, enum values. Member variables use `m_` prefix, private members use `_` prefix.
- **File headers**: Every source file begins with a `//` comment describing its purpose.
- Wide strings (`std::wstring`, `wchar_t`) throughout — the `_t()` macro wraps string literals.
- Case-insensitive comparisons via `_wcsicmp` and the `ltstr` comparator.
- CSS property names are semicolon-delimited string tables (e.g. `border_style_strings`) searched by `value_index`.
- Custom CSS properties use the `-potato-` vendor prefix.
- Amalgamated headers — each `.h` file is composed from originally separate headers, marked with `// ---- original_name.h ----` section comments.
- No external HTML/CSS library dependencies — everything is self-contained in `src/`.
