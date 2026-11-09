// SPDX-License-Identifier: MIT
#ifdef DSL_DOCUMENTS
#include "text_input.h"
#include "utf8.h"

namespace document {
uint32_t decode_byte(unsigned char byte, Encoding encoding)
{
    static const uint32_t windows1252[32] = {
        0x20ac,0xfffd,0x201a,0x0192,0x201e,0x2026,0x2020,0x2021,
        0x02c6,0x2030,0x0160,0x2039,0x0152,0xfffd,0x017d,0xfffd,
        0xfffd,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,
        0x02dc,0x2122,0x0161,0x203a,0x0153,0xfffd,0x017e,0x0178
    };
    // Unicode mappings for the Windows-1251 upper half (ASCII is unchanged).
    static const uint32_t windows1251[128] = {
        0x0402, 0x0403, 0x201a, 0x0453, 0x201e, 0x2026, 0x2020, 0x2021,
        0x20ac, 0x2030, 0x0409, 0x2039, 0x040a, 0x040c, 0x040b, 0x040f,
        0x0452, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
        0xfffd, 0x2122, 0x0459, 0x203a, 0x045a, 0x045c, 0x045b, 0x045f,
        0x00a0, 0x040e, 0x045e, 0x0408, 0x00a4, 0x0490, 0x00a6, 0x00a7,
        0x0401, 0x00a9, 0x0404, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x0407,
        0x00b0, 0x00b1, 0x0406, 0x0456, 0x0491, 0x00b5, 0x00b6, 0x00b7,
        0x0451, 0x2116, 0x0454, 0x00bb, 0x0458, 0x0405, 0x0455, 0x0457,
        0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
        0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e, 0x041f,
        0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
        0x0428, 0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f,
        0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
        0x0438, 0x0439, 0x043a, 0x043b, 0x043c, 0x043d, 0x043e, 0x043f,
        0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
        0x0448, 0x0449, 0x044a, 0x044b, 0x044c, 0x044d, 0x044e, 0x044f,
    };
    if (encoding == Encoding::Windows1251 && byte >= 128) return windows1251[byte - 128];
    if (encoding == Encoding::Windows1252 && byte >= 128 && byte < 160) return windows1252[byte - 128];
    return byte;
}

TextInput::TextInput(FILE *source, size_t maximum, Encoding preferred)
    : file(source), limit(maximum), encoding(preferred)
{
    initialize();
}
TextInput::TextInput(const char *data, size_t size, size_t maximum, Encoding preferred)
    : memory((const unsigned char*)data), memory_size(size), limit(maximum), encoding(preferred)
{
    initialize();
}
int TextInput::raw_byte()
{
    if (file) return fgetc(file);
    return memory_position < memory_size ? memory[memory_position++] : -1;
}
void TextInput::initialize()
{
    for (unsigned i = 0; i < 3; ++i) {
        int c = raw_byte();
        if (c == EOF) { if (file && ferror(file)) error = ReadError; break; }
        if (consumed == limit) { error = TooLarge; break; }
        ++consumed;
        prefix[prefix_size++] = c;
    }
    if (prefix_size >= 2 && prefix[0] == 0xff && prefix[1] == 0xfe) {
        encoding = Encoding::Utf16LE; position = 2; bom = true;
    } else if (prefix_size >= 2 && prefix[0] == 0xfe && prefix[1] == 0xff) {
        encoding = Encoding::Utf16BE; position = 2; bom = true;
    } else if (prefix_size == 3 && prefix[0] == 0xef && prefix[1] == 0xbb && prefix[2] == 0xbf) {
        encoding = Encoding::Utf8; position = 3; bom = true;
    }
}

bool TextInput::set_encoding(Encoding requested)
{
    if (!bom) encoding = requested;
    return true;
}

int TextInput::byte()
{
    if (error) return -1;
    if (held_byte >= 0) { int c = held_byte; held_byte = -1; return c; }
    if (position < prefix_size) return prefix[position++];
    int c = raw_byte();
    if (c == EOF) { if (file && ferror(file)) error = ReadError; return -1; }
    if (consumed == limit) { error = TooLarge; return -1; }
    ++consumed;
    return c;
}

int TextInput::unit()
{
    if (held_unit >= 0) { int value = held_unit; held_unit = -1; return value; }
    int a = byte();
    if (a < 0) return -1;
    int b = byte();
    if (b < 0) { if (!error) error = Invalid; return -1; }
    return encoding == Encoding::Utf16LE ? a | (b << 8) : (a << 8) | b;
}

int32_t TextInput::next()
{
    if (encoding == Encoding::Utf16LE || encoding == Encoding::Utf16BE) {
        int high = unit();
        if (high < 0) return -1;
        if (high >= 0xd800 && high <= 0xdbff) {
            int low = unit();
            if (low >= 0xdc00 && low <= 0xdfff) return 0x10000 + ((high - 0xd800) << 10) + low - 0xdc00;
            held_unit = low;
            return 0xfffd;
        }
        return high >= 0xdc00 && high <= 0xdfff ? 0xfffd : high;
    }
    int first = byte();
    if (first < 0) return -1;
    if (encoding != Encoding::Utf8) return decode_byte(first, encoding);
    char bytes[4] = {(char)first};
    unsigned count = first < 0x80 ? 1 : first >= 0xc2 && first <= 0xdf ? 2
        : first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 1;
    for (unsigned i = 1; i < count; ++i) {
        int c = byte();
        if (c < 0) return 0xfffd;
        if ((c & 0xc0) != 0x80) { held_byte = c; return 0xfffd; }
        bytes[i] = c;
    }
    uint32_t code;
    utf8_decode(bytes, count, &code);
    return code;
}
}
#endif
