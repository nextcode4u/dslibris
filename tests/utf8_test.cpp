// SPDX-License-Identifier: MIT
#include "utf8.h"
#include <cassert>
#include <cstring>
int main()
{
    const uint32_t values[] = {0, 0x41, 0x7f, 0x80, 0x7ff, 0x800, 0xd7ff, 0xe000, 0xffff, 0x10000, 0x1f319, 0x10ffff};
    for (auto expected : values) {
        char bytes[4]; uint32_t actual;
        size_t count = utf8_encode(expected, bytes);
        assert(utf8_decode(bytes, count, &actual) == count && actual == expected);
        for (size_t limit = 1; limit < count; ++limit)
            assert(utf8_decode(bytes, limit, &actual) == 1 && actual == 0xfffd);
    }
    const char *invalid[] = {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xff", "\xe2x"};
    for (auto bytes : invalid) { uint32_t code; assert(utf8_decode(bytes, strlen(bytes), &code) == 1 && code == 0xfffd); }
    auto width = [](uint32_t) { return 1u; };
    const char title[] = "A\xc3\xa9\xf0\x9f\x8c\x99";
    assert(utf8_prefix_width(title, sizeof(title)-1, 1, width) == 1);
    assert(utf8_prefix_width(title, sizeof(title)-1, 2, width) == 3);
    assert(utf8_prefix_width(title, sizeof(title)-1, 3, width) == 7);
    uint32_t code; assert(utf8_decode(nullptr, 0, &code) == 0);
    // Exact-sized byte arrays exercise the old page decoder's missing terminator case.
    char truncated[] = {(char)0xf0};
    assert(utf8_decode(truncated, sizeof(truncated), &code) == 1 && code == 0xfffd);
}
