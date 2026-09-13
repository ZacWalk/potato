# Potato — pending work

## Correctness

- **`<html>` overflows the viewport by 2px on BBC** (1904 vs 1902) while `<body>`
  is correct. No BBC CSS targets `html`, so the cause is internal. Suspect a
  `border:2px` box with `box-sizing:border-box` not being honoured.
- **Wikipedia overflow-x**: 4 boxes on the main page, 66 on the web-browser
  article, 61 on the comparison table. Main page also lays out 2732 wide at a
  1902 viewport. One `negative-x` box on every fixture, and 1 unsized image on
  the main page.
- **No real CSS Grid.** `grid`/`inline-grid` are approximated as block with
  blockified children. `grid-template-columns`, `grid-column`, `grid-row`,
  `column-gap` and `row-gap` are all ignored.
- **`min()` / `max()` / `clamp()` are not parsed** — they fall back to the
  predefined default. Safe, but wrong whenever the function would bind.
- **`margin-inline`, `min-width:fit-content`, `object-fit`** unsupported.
- **`data:` URLs unsupported** — the `failed=3` in every `--eval:` run are
  `data:image/svg+xml` images.

## Performance

Measured, do not re-guess — see `/memories/repo/potato-style-perf-attribution.md`.
`PARSE_STYLES` dominates (~45ms on BBC) and has **no single hotspot**.

- **Parse CSS declarations into typed values once at cascade time** instead of
  re-parsing strings per element. `element::parse_styles` currently performs
  ~100 independent string-based resolutions per element at ~200ns each. This is
  the only change large enough to matter; everything smaller is noise.
- `element::apply_stylesheet` builds `vector<shared_ptr<css_selector>>` — an
  atomic refcount per `push_back` plus two sorts per element. Switching the
  candidate lists to `const css_selector*` removes every atomic.
- `props_mut()` is called unconditionally, so every element allocates the ~1KB
  `css_props` even when nothing is set. Defeats its own lazy-allocation design.
- Rejected after measurement: converting `get_style_property` to return
  `string_view`. Only 429 heap copies on BBC, 0 on Wikipedia — worth ~10% at
  most for a ~90-call-site change.

## Test harness

- **Headless is not offline.** The `document` constructor calls `m_http.open()`,
  so `--layout:` still issues real WinHTTP requests (~22 on BBC) that can never
  land. Leaves `%TEMP%\pot*` files behind and makes timings network-dependent;
  dimensions stay reproducible. Add a flag to skip it.
- **No headless external-CSS path**, so async-stylesheet behaviour cannot be
  reproduced deterministically. BBC is unaffected (all CSS is inline).
- **`--eval:`'s window is now vestigial** — layout no longer needs one. Pointing
  eval at `do_layout()` directly would drop the window entirely and let it force
  a final layout after quiescence, making output stable instead of a sample of
  an async race.
- `--test` fetches four live URLs, so it fails when the network is down for
  reasons unrelated to the change under test.

## Architecture

- **Layout is free of the window but not thread-safe.** `measure_dc` is
  `thread_local`, so laying out off the UI thread would silently build a second
  font cache. Needs DirectWrite before layout can move off-thread.
- Split `document` into `resource_loader` / `font_cache` / `document` /
  `layout_engine`.
- Drop the HTTP temp-file round trip.
- Duplicated 160-byte `css_border_radius`; shrink `css_length` from 20 to 16
  bytes.

## Docs

- **`AGENTS.md` "Key conventions" is stale** — it still claims wide strings
  (`std::wstring`, `_t()`), a `ui.h` that no longer exists, and a GDI+
  `render_win32`. The codebase is UTF-8 `std::string` throughout.
