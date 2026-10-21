/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMOS_INPUT_H
#define LAMOS_INPUT_H
#include <stdint.h>
#include <stdbool.h>
enum EventType { KEY_DOWN = 1, KEY_UP, TEXT_INPUT, MOUSE_MOVE, BUTTON_DOWN, BUTTON_UP };
struct Event {
    enum EventType type;
    uint16_t code;
    char text;
    int16_t dx, dy;
    uint8_t buttons;
};
#define EVENT_CAPACITY 128
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
#endif
