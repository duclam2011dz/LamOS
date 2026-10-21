# Input and display contracts

## Events

`include/input.h` defines the event API. `Event.type` is one of:

| Value | Event | Fields |
| --- | --- | --- |
| 1 | KEY_DOWN | `code` |
| 2 | KEY_UP | `code` |
| 3 | TEXT_INPUT | ASCII `text` |
| 4 | MOUSE_MOVE | signed `dx`, `dy`, current `buttons` |
| 5 | BUTTON_DOWN | changed button `code`, current `buttons` |
| 6 | BUTTON_UP | changed button `code`, current `buttons` |

Key codes are set-1 scan codes without the release bit; extended keys add 128. They are physical-key identifiers, not Unicode or ASCII. Left/right Ctrl and Alt contribute to aggregate modifier state. Caps Lock toggles only on a new press. Shift and Caps Lock combine for letters; shifted punctuation follows US QWERTY. Ctrl/Alt suppress text events. The Pause sequence is consumed; not every special key or keypad combination produces text.

Mouse button masks are 1 left, 2 right, 4 middle. Y movement is positive downward. Overflowed packets are discarded, synchronization resumes at a byte with bit 3 set, and the three-byte protocol has no wheel support.

The ring has 128 storage slots and 127 usable entries. The producer runs in bounded IRQ context. Main-context pops disable interrupts; overflow drops the newest event and increments `dropped`. Overflow does not promise perfect modifier/button state continuity, so a consumer must not use this v1 queue as a lossless input recorder.

## Dashboard behavior

The editable line has 79 ASCII bytes plus a terminator. Printable characters append until full. Backspace removes the last byte; Enter logs a submission and clears the line. The viewport displays the tail that fits its width. The original font uses uppercase letter shapes for both cases; serial logs preserve case.

The cursor starts at (100,100), follows relative mouse events and clamps to the framebuffer. Button activity changes the dashboard indicator. Mouse events and key press/release/text are logged independently for emulator acceptance tests.

## Graphics

Canvas pixels use `0x00RRGGBB`. Pixel, rectangle and line operations clip to the canvas; text supports scale 1–4 and stops at the right edge. Firmware pitch/channel positions determine hardware encoding. The backbuffer is independent of hardware pixel layout.

The dashboard paints static elements once and damages dynamic regions on input or nominal 10 Hz updates. The scene backbuffer restores the old cursor footprint before the new cursor is composited into hardware. Panic can repaint the whole display after graphics is ready.
