// SPDX-License-Identifier: MIT
#pragma once
#include "document.h"

namespace document {
enum class Encoding { Utf8, Utf16LE, Utf16BE, Windows1252, Windows1251, Latin1 };
uint32_t decode_byte(unsigned char byte, Encoding encoding);

// Bounded, incremental text decoding. A BOM takes precedence over a supplied encoding.
class TextInput {
public:
    TextInput(FILE *file, size_t limit, Encoding encoding = Encoding::Utf8);
    TextInput(const char *data, size_t size, size_t limit, Encoding encoding = Encoding::Utf8);
    int32_t next();
    bool set_encoding(Encoding encoding);
    int status() const { return error; }
private:
    FILE *file = nullptr;
    const unsigned char *memory = nullptr;
    size_t memory_size = 0, memory_position = 0;
    size_t limit, consumed = 0;
    Encoding encoding;
    unsigned char prefix[3]{};
    unsigned prefix_size = 0, position = 0;
    int held_byte = -1, held_unit = -1;
    int error = Ok;
    bool bom = false;
    void initialize();
    int raw_byte();
    int byte();
    int unit();
};
}
