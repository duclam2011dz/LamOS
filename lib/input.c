/* SPDX-License-Identifier: GPL-3.0-only */
#include "input.h"
bool event_push(struct EventQueue *q, struct Event e) {
    unsigned next = (q->head + 1) % EVENT_CAPACITY;
    if (next == q->tail) {
        q->dropped++;
        return false;
    }
    q->events[q->head] = e;
    q->head = next;
    return true;
}
bool event_pop(struct EventQueue *q, struct Event *e) {
    if (q->tail == q->head)
        return false;
    *e = q->events[q->tail];
    q->tail = (q->tail + 1) % EVENT_CAPACITY;
    return true;
}
static const char plain[128] = {
    [2] = '1',  [3] = '2',  [4] = '3',   [5] = '4',  [6] = '5',   [7] = '6',   [8] = '7',
    [9] = '8',  [10] = '9', [11] = '0',  [12] = '-', [13] = '=',  [14] = '\b', [15] = '\t',
    [16] = 'q', [17] = 'w', [18] = 'e',  [19] = 'r', [20] = 't',  [21] = 'y',  [22] = 'u',
    [23] = 'i', [24] = 'o', [25] = 'p',  [26] = '[', [27] = ']',  [28] = '\n', [30] = 'a',
    [31] = 's', [32] = 'd', [33] = 'f',  [34] = 'g', [35] = 'h',  [36] = 'j',  [37] = 'k',
    [38] = 'l', [39] = ';', [40] = '\'', [41] = '`', [43] = '\\', [44] = 'z',  [45] = 'x',
    [46] = 'c', [47] = 'v', [48] = 'b',  [49] = 'n', [50] = 'm',  [51] = ',',  [52] = '.',
    [53] = '/', [55] = '*', [57] = ' '};
static const char shifted[128] = {
    [2] = '!',  [3] = '@',  [4] = '#',  [5] = '$',  [6] = '%',  [7] = '^',  [8] = '&',
    [9] = '*',  [10] = '(', [11] = ')', [12] = '_', [13] = '+', [26] = '{', [27] = '}',
    [39] = ':', [40] = '"', [41] = '~', [43] = '|', [51] = '<', [52] = '>', [53] = '?'};
void keyboard_byte(struct Keyboard *k, struct EventQueue *q, uint8_t raw) {
    if (k->pause_left) {
        k->pause_left--;
        return;
    }
    if (raw == 0xe1) {
        k->pause_left = 5;
        return;
    }
    if (raw == 0xe0) {
        k->extended = true;
        return;
    }
    if (raw == 0xfa || raw == 0xfe)
        return;
    bool up = (raw & 0x80) != 0;
    unsigned scan = raw & 0x7f, code = scan + (k->extended ? 128 : 0);
    bool was = k->pressed[code];
    k->pressed[code] = !up;
    if (!k->extended && (scan == 42 || scan == 54)) {
        uint8_t mask = scan == 42 ? 1 : 2;
        if (up)
            k->shifts &= (uint8_t)~mask;
        else
            k->shifts |= mask;
    }
    if (scan == 29)
        k->ctrl = k->pressed[29] || k->pressed[157];
    if (scan == 56)
        k->alt = k->pressed[56] || k->pressed[184];
    if (!k->extended && scan == 58 && !up && !was)
        k->caps = !k->caps;
    uint8_t mods = (k->shifts ? MOD_SHIFT : 0) | (k->ctrl ? MOD_CTRL : 0) | (k->alt ? MOD_ALT : 0) |
                   (k->caps ? MOD_CAPS : 0);
    event_push(q, (struct Event){
                      .type = up ? KEY_UP : KEY_DOWN, .code = (uint16_t)code, .modifiers = mods});
    if (!up && !k->extended && !k->ctrl && !k->alt) {
        char c = plain[scan];
        if (c >= 'a' && c <= 'z') {
            if ((k->shifts != 0) != k->caps)
                c = (char)(c - 'a' + 'A');
        } else if (k->shifts && shifted[scan])
            c = shifted[scan];
        if (c >= 32 && c <= 126)
            event_push(q, (struct Event){.type = TEXT_INPUT, .text = c, .modifiers = mods});
    }
    k->extended = false;
}
void mouse_byte(struct Mouse *m, struct EventQueue *q, uint8_t b) {
    if (!m->index && !(b & 8))
        return;
    m->packet[m->index++] = b;
    if (m->index < 3)
        return;
    m->index = 0;
    uint8_t flags = m->packet[0];
    if (flags & 0xc0)
        return;
    int16_t dx = (int16_t)((int)m->packet[1] - ((flags & 16) ? 256 : 0)),
            dy = (int16_t)(-((int)m->packet[2] - ((flags & 32) ? 256 : 0)));
    uint8_t buttons = flags & 7;
    if (dx || dy)
        event_push(q, (struct Event){.type = MOUSE_MOVE, .dx = dx, .dy = dy, .buttons = buttons});
    for (unsigned i = 0; i < 3; i++)
        if ((buttons ^ m->buttons) & (1u << i))
            event_push(q, (struct Event){.type = (buttons & (1u << i)) ? BUTTON_DOWN : BUTTON_UP,
                                         .code = (uint16_t)(1u << i),
                                         .buttons = buttons});
    m->buttons = buttons;
}
