/* SPDX-License-Identifier: GPL-3.0-only */
#include "shell.h"
static bool equal(const char *a, const char *b) {
    unsigned i = 0;
    for (; a[i] && a[i] == b[i]; i++) {
    }
    return a[i] == b[i];
}
void terminal_number(struct Terminal *t, uint64_t n, uint8_t color) {
    char rev[24], out[24];
    unsigned i = 0, j = 0;
    do {
        rev[i++] = (char)('0' + n % 10);
        n /= 10;
    } while (n);
    while (i)
        out[j++] = rev[--i];
    out[j] = 0;
    terminal_write(t, out, color);
}
static void metric(struct Terminal *t, const char *label, uint64_t n) {
    terminal_write(t, label, 7);
    terminal_number(t, n, 14);
    terminal_write(t, "\n", 7);
}
void shell_execute(struct Terminal *t, const char *line, const struct ShellInfo *info) {
    char command[32];
    unsigned n = 0;
    while (*line == ' ' || *line == '\t')
        line++;
    while (*line && *line != ' ' && *line != '\t') {
        if (n < 31)
            command[n++] = *line;
        line++;
    }
    command[n] = 0;
    while (*line == ' ' || *line == '\t')
        line++;
    if (!n)
        return;
    if (*line) {
        terminal_write(t, "These commands take no arguments. Use help.\n", 12);
        return;
    }
    if (equal(command, "help")) {
        terminal_write(t,
                       "Commands: help clear sysinfo meminfo uptime\nLeft/Right Home/End Delete: "
                       "edit; Up/Down: history\nPageUp/PageDown: scroll; Ctrl+C: cancel; Ctrl+L: "
                       "clear\nAlt+Tab: switch; Alt+F4: close; Ctrl+Alt+T: Terminal\n",
                       7);
    } else if (equal(command, "clear"))
        terminal_clear(t);
    else if (equal(command, "sysinfo")) {
        terminal_write(t, "LamOS ", 10);
        terminal_write(t, info->version, 10);
        terminal_write(t, " / Phase I Core GUI Foundation\nFirmware: ", 7);
        terminal_write(t, info->firmware, 14);
        terminal_write(t, "\nCPU: ", 7);
        terminal_write(t, info->cpu, 14);
        terminal_write(t, "\nFramebuffer: ", 7);
        terminal_number(t, info->width, 14);
        terminal_write(t, "x", 7);
        terminal_number(t, info->height, 14);
        terminal_write(t, "\nSingle CPU; kernel GUI; PS/2 input\n", 7);
    } else if (equal(command, "meminfo")) {
        metric(t, "PMM managed KiB: ", info->frame_total * 4);
        metric(t, "PMM free KiB: ", info->frame_free * 4);
        metric(t, "Heap used bytes: ", info->heap.used);
        metric(t, "Heap free bytes: ", info->heap.free);
        metric(t, "Heap peak bytes: ", info->heap.peak);
        metric(t, "Heap largest block: ", info->heap.largest);
        metric(t, "Live allocations: ", info->heap.allocations);
    } else if (equal(command, "uptime")) {
        terminal_write(t, "Uptime: ", 7);
        terminal_number(t, info->ticks / 100, 14);
        terminal_write(t, " seconds\n", 7);
    } else
        terminal_write(t, "Unknown command. Type help.\n", 12);
}
