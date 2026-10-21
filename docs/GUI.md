# Phase I GUI specification

## Resources and adapters

All GUI code runs at ring 0 on one CPU. `lib/` modules are host-testable and use callbacks for allocation, presentation, timing, logging and system snapshots. Kernel adapters own hardware access. No filesystem is required. The original embedded font covers ASCII 32–126 in 8×16 cells; unsupported characters use `?`. Procedural Terminal, Folder, Settings and Close icons share graphics primitives.

The default configuration has 256 MiB guest RAM, a 128 MiB heap and at most 16 windows. The desktop starts empty. Terminal instances are independent; Folder and Settings reuse and restore their existing instance. All session state disappears at reboot.

## Surfaces and rendering

`Surface` records width, height, byte stride, buffer size, pixel format, clip rectangle and ownership. Owned surfaces free through their original allocator; views never free their buffer and cannot outlive it. Dimensions are limited to 4096; size/stride checks precede access/allocation. Signed rectangles represent half-open ranges. Every primitive intersects the surface clip.

XRGB8888 pixels carry opaque RGB; ARGB8888 stores premultiplied channels. Source-over uses integer channel rounding `(value + 127) / 255`; alpha zero preserves the destination and alpha 255 replaces it. Geometry includes clipped pixels, filled/outlined rectangles, lines, circles and even-odd filled polygons. Copy blits support overlapping rows/different strides; overlapping alpha blends and incompatible overlapping views are rejected. Text supports left/center/right alignment, line spacing and a separate clip region.

## Compositor

Background, composition and front surfaces are independent. Each window owns its full opaque surface including decorations. For each damage rectangle the compositor reconstructs background, shadows and Z-ordered windows. Overlay order is taskbar, launcher, interaction outline, notifications/profiling and cursor. The cursor is always last. Presentation copies changed regions from composition/front state to the firmware framebuffer, respecting pitch and channel masks; hardware pixels are never a scene source.

Damage includes old/new extents on geometry, visibility and lifetime changes. Intersecting/touching rectangles merge. The fixed list holds 64 entries; overflow becomes full-screen damage. The kernel limits changed frames to approximately 30 Hz using PIT ticks, and sleeps while idle. There is no continuous animation or blur.

## Window lifecycle and input

A handle encodes slot and generation; stale generations fail lookup. Create/destroy, focus/raise, minimize, maximize/restore and move/resize operate within the work area above the 32-pixel taskbar. Minimum outer size is 320×200. Hit testing walks top to bottom and distinguishes client, titlebar, controls and resize edges/corners.

Titlebar/edge press captures the pointer and displays an outline. Release commits; Escape cancels. Resize allocates replacement storage before releasing the old surface. On failure geometry/storage stay unchanged and an allocation-free notification appears. Close/minimize/maximize require a matching valid release and show hover/pressed state. Closing/hiding focus selects the top remaining visible window; desktop click removes focus.

Input overflow cancels capture, discards stale events, restores decoder snapshots and reports loss. GUI and ISR paths allocate no event storage. See [input contracts](INPUT.md).

## Terminal and shell

Each Terminal owns a bounded 160-column backing model with 1,024 stored lines, colored cells, 256-character editor, 32-command history and a visible-cell cache. Narrowing hides columns without discarding history; widening reveals them. The current prompt wraps to keep the caret visible. Output/editor changes invalidate changed row spans; full repaint preserves the model.

Left/Right/Home/End/Delete/Backspace edit; Enter submits. Up/Down restore history and the saved draft. PageUp/PageDown scroll; new input returns to the active prompt. Clear removes output/scrollback but preserves command history. Multiple Terminals share no editor/history state.

The parser trims whitespace and recognizes exactly `help`, `clear`, `sysinfo`, `meminfo`, `uptime`. Commands reject arguments, unknown names report an error, and output flows through bounded Terminal storage. There are no pipelines, redirection, environment variables or executable processes. System information is supplied by kernel callbacks.

## Desktop and reliability

Three procedural backgrounds and dark/light themes are session-only. Desktop icons open on double-click. The launcher supports mouse and keyboard selection. Taskbar buttons focus/restore or minimize the focused window; overflow arrows scroll the window list. Uptime refreshes once per second. Folder exposes embedded About, Commands and README documents and explicitly identifies them as virtual resources. Settings changes theme/background/profiling without replacing window state.

Profiling uses TSC calibrated against PIT and records frame sample count, average, rolling 128-sample p95, maximum, render/composite/present costs and damaged pixels/rectangles. Emulator measurements are diagnostics, not physical-hardware performance claims. Allocation counters/bytes and largest free block must recover after create/resize/destroy cycles. Host tests inject allocation failure and perform 10,000 deterministic GUI operations.
