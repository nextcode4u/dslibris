// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>

// Invalid or truncated input consumes one byte, so callers always make progress.
static inline size_t utf8_decode(const char *text, size_t available, uint32_t *code)
{
    if (!available) { *code = 0; return 0; }
    const unsigned char *s = (const unsigned char *)text;
    unsigned count = s[0] < 0x80 ? 1 : s[0] >= 0xc2 && s[0] <= 0xdf ? 2
        : s[0] >= 0xe0 && s[0] <= 0xef ? 3 : s[0] >= 0xf0 && s[0] <= 0xf4 ? 4 : 0;
    *code = 0xfffd;
    if (!count || count > available) return 1;
    uint32_t value = s[0] & (count == 1 ? 0x7f : (1u << (7 - count)) - 1);
    for (unsigned i = 1; i < count; ++i) {
        if ((s[i] & 0xc0) != 0x80) return 1;
        value = (value << 6) | (s[i] & 0x3f);
    }
    if ((count == 2 && value < 0x80) || (count == 3 && value < 0x800)
        || (count == 4 && value < 0x10000) || value > 0x10ffff
        || (value >= 0xd800 && value <= 0xdfff)) return 1;
    *code = value;
    return count;
}

static inline size_t utf8_encode(uint32_t code, char out[4])
{
    if (code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) code = 0xfffd;
    if (code < 0x80) { out[0] = (char)code; return 1; }
    if (code < 0x800) { out[0] = (char)(0xc0 | (code >> 6)); out[1] = (char)(0x80 | (code & 63)); return 2; }
    if (code < 0x10000) {
        out[0] = (char)(0xe0 | (code >> 12)); out[1] = (char)(0x80 | ((code >> 6) & 63));
        out[2] = (char)(0x80 | (code & 63)); return 3;
    }
    out[0] = (char)(0xf0 | (code >> 18)); out[1] = (char)(0x80 | ((code >> 12) & 63));
    out[2] = (char)(0x80 | ((code >> 6) & 63)); out[3] = (char)(0x80 | (code & 63)); return 4;
}

#ifdef __cplusplus
// Return a byte boundary that fits the requested width, never half a codepoint.
template <class Advance>
size_t utf8_prefix_width(const char *text, size_t length, unsigned pixels, Advance advance)
{
    size_t used = 0;
    unsigned width = 0;
    while (used < length) {
        uint32_t code;
        size_t bytes = utf8_decode(text + used, length - used, &code);
        unsigned next = advance(code);
        if (next > pixels - width) break;
        width += next;
        used += bytes;
    }
    return used;
}
#endif
