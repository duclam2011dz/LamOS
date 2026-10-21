/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMOS_INPUT_H
#define LAMOS_INPUT_H
#include <stdint.h>
#include <stdbool.h>
enum KeyCode {
    KEY_BACKSPACE = 14,
    KEY_TAB = 15,
    KEY_ENTER = 28,
    KEY_ESCAPE = 1,
    KEY_F4 = 62,
    KEY_HOME = 199,
    KEY_UP_ARROW = 200,
    KEY_PAGE_UP = 201,
    KEY_LEFT = 203,
    KEY_RIGHT = 205,
    KEY_END = 207,
    KEY_DOWN_ARROW = 208,
    KEY_PAGE_DOWN = 209,
    KEY_DELETE = 211,
    KEY_T = 20,
    KEY_C = 46,
    KEY_L = 38
};
enum KeyModifier { MOD_SHIFT = 1, MOD_CTRL = 2, MOD_ALT = 4, MOD_CAPS = 8 };
enum EventType { KEY_DOWN = 1, KEY_UP, TEXT_INPUT, MOUSE_MOVE, BUTTON_DOWN, BUTTON_UP };
struct Event {
    enum EventType type;
    uint16_t code;
    char text;
    int16_t dx, dy;
    uint8_t buttons;
    uint8_t modifiers;
};
#define EVENT_CAPACITY 512
struct EventQueue {
    struct Event events[EVENT_CAPACITY];
    unsigned head, tail;
    uint64_t dropped;
};
bool event_push(struct EventQueue *, struct Event);
bool event_pop(struct EventQueue *, struct Event *);
struct Keyboard {
    bool extended;
    uint8_t pause_left;
    bool pressed[256];
    uint8_t shifts;
    bool caps, ctrl, alt;
};
void keyboard_byte(struct Keyboard *, struct EventQueue *, uint8_t);
struct Mouse {
    uint8_t packet[3], index, buttons;
};
void mouse_byte(struct Mouse *, struct EventQueue *, uint8_t);
bool input_next(struct Event *);
uint64_t input_dropped(void);
void mouse_init(void);
struct InputSnapshot {
    uint8_t modifiers, buttons;
    uint64_t dropped;
};
struct InputSnapshot input_recover(void);
#endif
