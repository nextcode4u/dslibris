// SPDX-License-Identifier: MIT
#ifdef DSL_DOCUMENTS
#include "document.h"
#include "text_input.h"
#include "utf8.h"
#include <cctype>
#include <cstring>
#include <vector>

namespace document {
namespace {
struct RtfState {
    bool hidden = false, bold = false, italic = false;
    unsigned fallback = 1;
    Encoding encoding = Encoding::Windows1252;
};
class Rtf {
    FILE *file;
    Sink &sink;
    Limits limits;
    RtfState state;
    std::vector<RtfState> groups;
    size_t consumed = 0;
    int held = -1, error = Ok;
    unsigned fallback = 0;
    uint16_t high = 0;
    bool header = false, closed = false;
    int get() {
        if (error) return -1;
        if (held >= 0) { int c = held; held = -1; return c; }
        int c = fgetc(file);
        if (c == EOF) { if (ferror(file)) error = ReadError; return -1; }
        if (consumed == limits.input_bytes) { error = TooLarge; return -1; }
        ++consumed;
        return c;
    }
    bool emit(uint32_t code) {
        if (state.hidden) return true;
        char bytes[4];
        return sink.text(bytes, utf8_encode(code, bytes));
    }
    bool flush_surrogate() {
        if (!high) return true;
        high = 0;
        return emit(0xfffd);
    }
    bool byte(unsigned char c) {
        if (fallback) { --fallback; return true; }
        return flush_surrogate() && emit(decode_byte(c, state.encoding));
    }
    bool unicode(uint16_t unit) {
        bool ok;
        if (high && unit >= 0xdc00 && unit <= 0xdfff) {
            uint32_t code = 0x10000 + ((high - 0xd800) << 10) + unit - 0xdc00;
            high = 0;
            ok = emit(code);
        } else {
            if (!flush_surrogate()) return false;
            if (unit >= 0xd800 && unit <= 0xdbff) { high = unit; ok = true; }
            else ok = emit(unit >= 0xdc00 && unit <= 0xdfff ? 0xfffd : unit);
        }
        fallback = state.fallback;
        return ok;
    }
    bool style() { return sink.style(state.bold, state.italic); }
    bool control() {
        int c = get();
        if (c < 0) { error = Invalid; return false; }
        if (c == '\\' || c == '{' || c == '}') return byte(c);
        if (c == '\'') {
            unsigned value = 0;
            for (int i = 0; i < 2; ++i) {
                c = get();
                if (c < 0 || !isxdigit((unsigned char)c)) { error = Invalid; return false; }
                value = value * 16 + (isdigit(c) ? c - '0' : tolower(c) - 'a' + 10);
            }
            return byte(value);
        }
        if (!isalpha((unsigned char)c)) {
            if (c == '*') state.hidden = true;
            if (fallback) { --fallback; return true; }
            if (c == '~') return emit(0xa0);
            if (c == '_') return emit(0x2011);
            if (c == '-') return emit(0xad);
            if ((c == '\n' || c == '\r') && !state.hidden) return sink.newline();
            return true;
        }
        std::string word;
        do {
            word += (char)c;
            if (word.size() > 32) { error = TooLarge; return false; }
            c = get();
        } while (c >= 0 && isalpha((unsigned char)c));
        bool negative = c == '-', parameter = negative || (c >= '0' && c <= '9');
        if (negative) c = get();
        int32_t number = 0;
        if (negative && (c < '0' || c > '9')) { error = Invalid; return false; }
        while (c >= '0' && c <= '9') {
            if (number > (2147483647 - (c - '0')) / 10) { error = Invalid; return false; }
            number = number * 10 + c - '0';
            c = get();
        }
        if (negative) number = -number;
        if (c >= 0 && c != ' ') held = c;
        if (word == "rtf") { header = parameter && number == 1; return header; }
        if (!header) { error = Invalid; return false; }
        if (word == "bin") {
            if (!parameter || number < 0) { error = Invalid; return false; }
            for (int32_t i = 0; i < number; ++i) if (get() < 0) { if (!error) error = Invalid; return false; }
            return true;
        }
        static const char *destinations[] = {"fonttbl","colortbl","stylesheet","info","pict","object","header","headerl","headerr","footer","footerl","footerr","fldinst","generator","filetbl","listtable","listoverridetable","xmlnstbl","datastore","themedata"};
        for (const char *name : destinations) if (word == name) { state.hidden = true; return true; }
        if (state.hidden) return true;
        if (word == "b") { state.bold = !parameter || number != 0; return style(); }
        if (word == "i") { state.italic = !parameter || number != 0; return style(); }
        if (word == "plain") { state.bold = state.italic = false; return style(); }
        if (word == "uc") {
            if (!parameter || number < 0 || number > 32) { error = Unsupported; return false; }
            state.fallback = number;
        } else if (word == "u") {
            if (!parameter || number < -32768 || number > 65535) { error = Invalid; return false; }
            return unicode((uint16_t)number);
        } else if (word == "ansicpg") {
            if (number == 1252) state.encoding = Encoding::Windows1252;
            else if (number == 1251) state.encoding = Encoding::Windows1251;
            else if (number == 28591) state.encoding = Encoding::Latin1;
            else { error = Unsupported; return false; }
        } else if (word == "mac" || word == "pc" || word == "pca") { error = Unsupported; return false; }
        else if (word == "par" || word == "line") return flush_surrogate() && sink.newline();
        else if (word == "tab") return sink.preformatted(true) && sink.text("    ", 4) && sink.preformatted(false);
        else if (word == "emdash") return emit(0x2014);
        else if (word == "endash") return emit(0x2013);
        else if (word == "bullet") return emit(0x2022);
        else if (word == "lquote") return emit(0x2018);
        else if (word == "rquote") return emit(0x2019);
        else if (word == "ldblquote") return emit(0x201c);
        else if (word == "rdblquote") return emit(0x201d);
        return true;
    }
public:
    Rtf(FILE *file, Sink &sink, Limits limits) : file(file), sink(sink), limits(limits) {}
    int run() {
        if (get() != '{') return Invalid;
        groups.push_back(state);
        if (get() != '\\' || !control() || !header) return error ? error : Invalid;
        for (int c; (c = get()) >= 0;) {
            if (closed) { if (!isspace((unsigned char)c)) return Invalid; continue; }
            if (c == '{') {
                if (groups.size() >= 128) return TooLarge;
                groups.push_back(state);
            } else if (c == '}') {
                if (groups.empty()) return Invalid;
                state = groups.back(); groups.pop_back();
                if (!style()) return LayoutError;
                if (groups.empty()) closed = true;
            } else if (c == '\\') {
                if (!control()) return error ? error : LayoutError;
            } else if (c != '\r' && c != '\n') {
                if (!byte(c)) return LayoutError;
            }
        }
        if (error) return error;
        if (!closed || !groups.empty()) return Invalid;
        return flush_surrogate() && sink.style(false, false) ? Ok : LayoutError;
    }
};
}
int read_rtf(FILE *file, Sink &sink, Limits limits) { return Rtf(file, sink, limits).run(); }
}
#endif
