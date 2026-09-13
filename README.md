# Potato

[![Build](https://github.com/ZacWalk/potato/actions/workflows/build.yml/badge.svg)](https://github.com/ZacWalk/potato/actions/workflows/build.yml)

A web browser written from scratch in C++20 for Windows. No dependencies, simple
Win32 GDI rendering, some CSS but no JavaScript.

Back in the 2000s, I needed to move from C++ to work on a Web project. I wanted to learn CSS better, so I decided the best thing to do, was write my own Web Browser in C++.

Here is what I came up with, probably the worst web browser in the world. I try to keep it working well enough to display Wikipedia.

## What is in the box

About 24,000 lines of C++20 with no third-party HTML or CSS library, split into a
reusable engine and the browser that drives it:

- **`src/libwebvis`** — the rendering engine, built as a standalone static
  library. An HTML tokenizer and parser that survives real-world markup
  (implicit tag closing, raw-text elements, entity decoding, charset sniffing);
  a CSS engine with selectors, specificity, combinators, the cascade, media
  queries, `calc()` and custom properties; and block, inline, float, flex and
  table layout.
- **`src/potato`** — the browser: window, toolbar, address bar, navigation
  history, GDI/WIC painting and asynchronous loading over WinHTTP.

libwebvis has no platform dependency at all — it does not even link
[platform-h](https://github.com/ZacWalk/platform-h). Everything it needs from the
machine arrives through two interfaces in `src/libwebvis/webvis.h` that the host
implements: `webvis::device_context`, a stateless drawing surface where every
call carries its own coordinates and colours, and `webvis::host` for text
measurement, resource loading and environment queries.

Text measurement sits on the host rather than the drawing surface on purpose:
layout therefore needs no surface at all, which is what makes the headless modes
below possible, and what lets libwebvis render a web view inside other
applications.

## Building

Requires Windows x64 and Visual Studio with the Desktop C++ workload. The
vendored [dd](https://github.com/ZacWalk/dd) runtime locates Visual Studio and
uses the CMake and Ninja that ship with it.

```powershell
.\dd.ps1 build        # both configurations
.\dd.ps1 test         # build and run the suite
.\dd.ps1 run          # build, then launch
```

Output is `Exe\potato-64.exe`, or `potato-64d.exe` for Debug.

## Diagnostics

Potato is a GUI application, but every diagnostic mode writes to the console, so
all of them can be scripted.

**Lay out a local page**, with no window and no message loop. Nothing is
downloaded, so the same input always produces the same output — this is the mode
to use when judging a layout change:

```powershell
.\dd.ps1 layout --file test-files/page.html --width 1902 --dump 4
```

It prints the document size and stage timings, a summary of the box tree, and a
scan for geometry no correct layout should produce: boxes that overflow the
viewport, start left of the origin, have a negative size, are text with no
height, or are an `<img>` with no width.

**Extract Wikipedia's stylesheet variables** for comparison against what the
cascade produces:

```powershell
.\dd.ps1 analyze-wiki-css
```

`Exe\potato-64.exe --eval:<url>` runs a real URL through the full loading and
rendering path with the window parked offscreen. Because resources arrive in a
different order every run the numbers are not reproducible; save the page and use
`layout` if you need to compare.

## Documentation

[AGENTS.md](AGENTS.md) — conventions for contributors and coding agents.

## License

See [LICENSE](LICENSE).
