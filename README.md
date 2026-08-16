Potato
======

[![Build](https://github.com/ZacWalk/potato/actions/workflows/build.yml/badge.svg)](https://github.com/ZacWalk/potato/actions/workflows/build.yml)

Back in the 2000s, I needed to move from C++ to work on a Web project. I wanted to learn CSS better, so I decided the best thing to do, was write my own Web Browser in C++.

Here is what I came up with, probably the worst web browser in the world. I try to keep it working well enough to display Wikipedia.

No dependencies, simple Win32 GDI rendering, some CSS but no JavaScript support.

What is in the box
------------------

About 24,000 lines of C++20 in `src/`, with no third-party HTML or CSS library:

- An HTML tokenizer and parser that survives real-world markup — implicit tag
  closing, raw-text elements, entity decoding and charset sniffing.
- A CSS engine with selectors, specificity, combinators, the cascade, media
  queries, `calc()` and custom properties.
- Block, inline, float, flex and table layout.
- Text, background, border and image painting over GDI and WIC.
- Asynchronous page, stylesheet and image loading over WinHTTP.

The Windows SDK is confined to the shared
[platform-h](https://github.com/ZacWalk/platform-h) repository, which every app
in this workspace builds against; the engine here never sees a `HWND`. Layout
needs no drawing surface at all, which is what makes the headless modes below
possible.

Building
--------

You need Visual Studio with the "Desktop development with C++" workload, and
nothing else — `dd.ps1` finds the CMake and Ninja that ship with Visual Studio
when they are not on your PATH.

```
.\dd.ps1 build                 # Release  -> Exe\potato-64.exe
.\dd.ps1 build -Config Debug   # Debug    -> Exe\potato-64d.exe
.\dd.ps1 run                   # build, then launch the browser
.\dd.ps1 test                  # build, then run the regression gate
.\dd.ps1 clean
```

Or drive CMake directly from an x64 Developer PowerShell:

```
cmake --preset release
cmake --build --preset release
```

Diagnostics
-----------

Potato is a GUI application, but every diagnostic mode writes to the console, so
they can all be scripted. PowerShell does not wait on a GUI-subsystem binary by
default; pipe through `| Out-String` to make it.

**Lay out a local page, with no window and no message loop.** Nothing is
downloaded, so the same input always produces the same output — this is the mode
to use when judging a layout change.

```
Exe\potato-64.exe "--layout:test-files\page.html" --width:1902 --dump:4
```

It prints the document size and stage timings, a summary of the box tree, and a
scan for geometry no correct layout should produce: boxes that overflow the
viewport, start left of the origin, have a negative size, are text with no
height, or are an `<img>` with no width. `--dump:N` prints the box tree to depth
N, `--repeat:N` re-runs the layout, `--dump-json` emits machine-readable probe
geometry, and `-v` adds per-stage diagnostics.

**Run the unit and layout regression suite.**

```
Exe\potato-64.exe --test
```

Writes an HTML report to `%TEMP%` and also fetches a few live URLs to exercise
the HTTP path, so it needs a network connection.

**Run a real URL through the full loading and rendering path.**

```
Exe\potato-64.exe "--eval:https://en.wikipedia.org/wiki/Main_Page"
```

The window is parked offscreen so it never appears or steals focus. Navigation,
resource, stylesheet and layout activity is logged with timestamps, and the
process exits once things settle. Because resources arrive in a different order
every run, the numbers are not reproducible — save the page and use `--layout:`
if you need to compare.

Licence
-------

See [LICENSE](LICENSE).
