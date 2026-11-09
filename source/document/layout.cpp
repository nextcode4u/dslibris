// SPDX-License-Identifier: MIT
#ifdef DSL_DOCUMENTS
#include "document.h"
#include "utf8.h"
#include <algorithm>

namespace document {
Layout::Layout(Output &out, unsigned line_width, unsigned lines, Limits bounds)
    : output(out), limits(bounds), width(line_width), lines_per_page(lines * 2)
{
    if (!width || !lines || lines > 256) error = LayoutError;
}

bool Layout::append(const char *data, size_t length)
{
    if (error) return false;
    if (length > limits.output_bytes - total || length + buffer.size() > 16384) { error = TooLarge; return false; }
    buffer.append(data, length);
    total += length;
    return true;
}

bool Layout::save_page()
{
    if (!page_visible) { buffer.clear(); return true; }
    if (pages >= limits.pages) { error = TooLarge; return false; }
    if (!output.page(buffer.data(), buffer.size())) { error = LayoutError; return false; }
    ++pages;
    buffer.clear();
    page_visible = false;
    return true;
}

bool Layout::glyph(uint32_t code)
{
    unsigned advance = std::min(width, output.advance(code, bold, italic));
    if (used && used + advance > width && !newline()) return false;
    char bytes[4];
    if (!append(bytes, utf8_encode(code, bytes))) return false;
    used += advance;
    if (code != ' ') visible = page_visible = true;
    return true;
}

bool Layout::flush_word()
{
    if (word.empty()) return !error;
    unsigned length = 0;
    for (size_t i = 0; i < word.size();) {
        uint32_t code;
        i += utf8_decode(word.data() + i, word.size() - i, &code);
        length += std::min(width, output.advance(code, bold, italic));
    }
    if (used && used + length + (space ? output.advance(' ', bold, italic) : 0) > width) {
        // Clear before newline(), which also flushes any pending word.
        std::string pending;
        pending.swap(word);
        if (!newline()) return false;
        pending.swap(word);
    }
    if (space && used && !glyph(' ')) return false;
    space = false;
    std::string pending;
    pending.swap(word);
    for (size_t i = 0; i < pending.size();) {
        uint32_t code;
        i += utf8_decode(pending.data() + i, pending.size() - i, &code);
        if (!glyph(code)) return false;
    }
    return true;
}

bool Layout::newline()
{
    if (!flush_word() || !append("\n", 1)) return false;
    used = 0;
    space = false;
    if (++line == lines_per_page) {
        if (!save_page()) return false;
        line = 0;
        if (bold && !append("\2", 1)) return false;
        if (italic && !append("\4", 1)) return false;
    }
    return true;
}

bool Layout::text(const char *data, size_t size)
{
    for (size_t i = 0; i < size;) {
        uint32_t code;
        i += utf8_decode(data + i, size - i, &code);
        if (code == 0xfeff || code == '\r') continue;
        if (code == '\n') { if (!newline()) return false; continue; }
        if (code == ' ' || code == '\t') {
            if (!flush_word()) return false;
            if (literal) {
                unsigned count = code == '\t' ? 4 : 1;
                while (count--) if (!glyph(' ')) return false;
            } else space = true;
            continue;
        }
        if (code < 32 || code == 127) continue; // Keep source bytes away from style controls.
        char bytes[4];
        word.append(bytes, utf8_encode(code, bytes));
        if (word.size() >= 256 && !flush_word()) return false;
    }
    return !error;
}

bool Layout::style(bool next_bold, bool next_italic)
{
    if (!flush_word()) return false;
    if (bold != next_bold && !append(next_bold ? "\2" : "\3", 1)) return false;
    if (italic != next_italic && !append(next_italic ? "\4" : "\5", 1)) return false;
    bold = next_bold;
    italic = next_italic;
    return true;
}

bool Layout::preformatted(bool enabled)
{
    if (!flush_word()) return false;
    literal = enabled;
    return true;
}

int Layout::finish()
{
    if (!flush_word()) return error;
    if (!visible) return NoText;
    if (!save_page()) return error;
    return error;
}

const char *error_message(int code)
{
    switch (code) {
    case ReadError: return "Cannot read document";
    case TooLarge: return "Document exceeds memory limit";
    case Invalid: return "Invalid document";
    case Unsupported: return "Unsupported format or encoding";
    case Encrypted: return "Encrypted book is not supported";
    case NoText: return "No readable text layer";
    case LayoutError: return "Cannot lay out document";
    default: return "Cannot open document";
    }
}
}
#endif
