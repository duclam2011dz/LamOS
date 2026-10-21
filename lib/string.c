/* SPDX-License-Identifier: GPL-3.0-only */
#include <stddef.h>
void *memcpy(void *d, const void *s, size_t n) {
    for (size_t i = 0; i < n; i++)
        ((unsigned char *)d)[i] = ((const unsigned char *)s)[i];
    return d;
}
void *memset(void *d, int c, size_t n) {
    for (size_t i = 0; i < n; i++)
        ((unsigned char *)d)[i] = (unsigned char)c;
    return d;
}
size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}
