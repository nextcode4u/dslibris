// SPDX-License-Identifier: MIT
#ifdef DSL_DOCUMENTS
#include "document.h"
#include "text_input.h"
#include "utf8.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace document {
namespace {
int hex(int32_t c)
{
    if (c >= '0' && c <= '9') return c-'0';
    if (c >= 'a' && c <= 'f') return c-'a'+10;
    if (c >= 'A' && c <= 'F') return c-'A'+10;
    return -1;
}
class PalmMarkup {
    TextInput input;
    Sink &sink;
    bool bold = false, italic = false, hidden = false, heading = false;
    bool carriage = false;
    int32_t held = -1;
    int error = Ok;
    uint16_t high = 0;
    int32_t next() { if (held < 0) return input.next(); int32_t c = held; held = -1; return c; }
    bool emit(uint32_t code) {
        if (hidden) return true;
        char bytes[4];
        return sink.text(bytes, utf8_encode(code, bytes));
    }
    bool flush_surrogate() {
        if (!high) return true;
        high = 0;
        return emit(0xfffd);
    }
    bool unicode(uint16_t unit) {
        if (unit >= 0xdc00 && unit <= 0xdfff && high) {
            uint32_t code = 0x10000 + ((high-0xd800) << 10) + unit-0xdc00;
            high = 0;
            return emit(code);
        }
        if (!flush_surrogate()) return false;
        if (unit >= 0xd800 && unit <= 0xdbff) { high = unit; return true; }
        return emit(unit >= 0xdc00 && unit <= 0xdfff ? 0xfffd : unit);
    }
    bool parameter() {
        int32_t c = next();
        if (c != '=') { held = c; return true; }
        if (next() != '"') { error = Invalid; return false; }
        for (size_t i = 0; i < 2048; ++i) {
            c = next();
            if (c == '"') return true;
            if (c < 0 || c == '\n' || c == '\r') { error = Invalid; return false; }
        }
        error = TooLarge;
        return false;
    }
    bool command() {
        int32_t c = next();
        if (c < 0) { error = Invalid; return false; }
        if (c == 'U' || c == 'a') {
            unsigned number = 0, count = c == 'U' ? 4 : 3;
            for (unsigned i = 0; i < count; ++i) {
                int32_t digit = next();
                int value = c == 'U' ? hex(digit) : digit >= '0' && digit <= '9' ? digit-'0' : -1;
                if (value < 0) { error = Invalid; return false; }
                number = number*(c == 'U' ? 16 : 10) + value;
            }
            return c == 'U' ? unicode(number) : flush_surrogate() && emit(number < 256 ? decode_byte(number, Encoding::Windows1252) : number);
        }
        if (!flush_surrogate()) return false;
        if (c == '\\') return emit('\\');
        if (c == 'v') { hidden = !hidden; return true; }
        if (hidden) return parameter();
        if (c == 'b' || c == 'B') bold = !bold;
        else if (c == 'i' || c == 'I') italic = !italic;
        else if (c == 'x' || c == 'X') {
            if (c == 'X') {
                int32_t level = next();
                if (level < '0' || level > '4') { error = Invalid; return false; }
            }
            heading = !heading;
            if (!sink.newline()) return false;
            if (!parameter()) return false;
        } else if (c == 'C') {
            int32_t level = next();
            if (level < '0' || level > '4') { error = Invalid; return false; }
            return parameter();
        } else if (c == 'p' || c == 'w') {
            return parameter() && sink.newline();
        } else if (c == 'm' || c == 'q' || c == 'Q' || c == 'T') return parameter();
        else if (c == 'F' || c == 'S') {
            int32_t kind = next();
            bool valid = c == 'F' ? kind == 'n' || kind == 'N' : kind == 'p' || kind == 'b' || kind == 'd' || kind == 'B';
            if (!valid) { error = Unsupported; return false; }
            if (!parameter()) return false;
            if ((c == 'F' && kind == 'N') || (c == 'S' && kind == 'B')) return sink.newline();
        } else if (c == 'c' || c == 'r' || c == 't' || c == 'u' || c == 'o'
                   || c == 'l' || c == 'k' || c == 'n' || c == 's') {
            // Alignment, size, underline and strikeout do not change text order.
        } else { error = Unsupported; return false; }
        return sink.style(bold || heading, italic);
    }
public:
    PalmMarkup(FILE *file, Sink &sink, Limits limits) : input(file, limits.input_bytes, Encoding::Windows1252), sink(sink) {}
    PalmMarkup(const char *data, size_t size, Sink &sink, Limits limits) : input(data, size, limits.input_bytes, Encoding::Windows1252), sink(sink) {}
    int run() {
        if (!sink.preformatted(false)) return LayoutError;
        for (int32_t c; (c = next()) >= 0;) {
            if (c == '\\') {
                if (!command()) return input.status() ? input.status() : error ? error : LayoutError;
                carriage = false;
            } else if (c == '\r' || c == '\n') {
                if (!(c == '\n' && carriage) && !hidden && (!flush_surrogate() || !sink.newline())) return LayoutError;
                carriage = c == '\r';
            } else {
                carriage = false;
                if (!flush_surrogate() || !emit(c)) return LayoutError;
            }
        }
        if (input.status()) return input.status();
        if (hidden) return Invalid;
        return flush_surrogate() && sink.style(false,false) ? Ok : LayoutError;
    }
};
}
int read_pml(FILE *file, Sink &sink, Limits limits) { return PalmMarkup(file,sink,limits).run(); }

int read_pml_data(const char *data, size_t size, Sink &sink, Limits limits) { return PalmMarkup(data,size,sink,limits).run(); }

int read_tcr(FILE *file, Sink &sink, Limits limits)
{
    size_t consumed = 0;
    int error = Ok;
    auto byte = [&]() {
        int c = fgetc(file);
        if (c == EOF) { if (ferror(file)) error = ReadError; return -1; }
        if (consumed == limits.input_bytes) { error = TooLarge; return -1; }
        ++consumed;
        return c;
    };
    for (const char *p = "!!8-Bit!!"; *p; ++p) if (byte() != *p) return error ? error : Invalid;
    std::vector<std::string> dictionary(256);
    for (auto &entry : dictionary) {
        int length = byte();
        if (length < 0) return error ? error : Invalid;
        for (int i = 0; i < length; ++i) {
            int c = byte();
            if (c < 0) return error ? error : Invalid;
            entry += char(c);
        }
    }
    std::string text;
    size_t maximum = std::min(limits.input_bytes, limits.output_bytes);
    for (int c; (c = byte()) >= 0;) {
        const auto &entry = dictionary[c];
        if (entry.size() > maximum-text.size()) return TooLarge;
        text += entry;
    }
    if (error) return error;
    // TCR has no charset field. Prefer valid UTF-8, otherwise use Windows-1252.
    Encoding encoding = Encoding::Utf8;
    for (size_t i = 0; i < text.size();) {
        uint32_t code;
        size_t n = utf8_decode(text.data()+i, text.size()-i, &code);
        if (n == 1 && (unsigned char)text[i] >= 128) { encoding = Encoding::Windows1252; break; }
        i += n;
    }
    return read_text_data(text.data(), text.size(), sink, {maximum,limits.output_bytes,limits.pages}, encoding);
}
}
#endif
