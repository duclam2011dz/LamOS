# Input contracts

`include/input.h` defines key down/up, text input, relative mouse movement and button down/up events. Named KeyCode values preserve set-1 physical identifiers; extended keys add 128. Key events include a modifier snapshot: Shift, Ctrl, Alt and Caps Lock. The keyboard is US QWERTY; printable ASCII is a separate TEXT_INPUT event, while Enter/Backspace/Delete/navigation are named keys. Ctrl/Alt suppress text generation. Pause is consumed; wheel and complete keypad semantics are outside this version.

Mouse masks are left=1, right=2, middle=4. Positive Y points down. The three-byte decoder rejects overflow and resynchronizes using packet bit 3. The GUI clamps its software cursor to display bounds.

The ring has 512 slots (511 usable). IRQ producers perform bounded operations without allocation or rendering. Main-context queue access disables interrupts. On overflow, the producer drops the newest event and increments a counter. The consumer discards queued events, copies actual modifier/button decoder state, cancels capture/outline, suppresses held-button gestures until release and displays a loss notification. Lost text is never reconstructed.

System shortcuts are handled before focused-window routing: Ctrl+Alt+T opens Terminal, Alt+Tab cycles/restores windows, Alt+F4 closes focus, and Escape cancels capture or closes the launcher. Desktop clicks clear focus; client keyboard events go only to the focused visible window.

See [GUI contracts](GUI.md) for editor, capture and damage semantics.
