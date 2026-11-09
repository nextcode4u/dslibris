// SPDX-License-Identifier: MIT
#ifdef DSL_DOCUMENTS
#include "document.h"
#include "md4c.h"
#include "entities.h"
#include <stdlib.h>
#include <string.h>
#include <vector>

namespace document {
struct Markdown {
    Sink &sink;
    unsigned bold = 0, italic = 0;
    std::vector<unsigned> list_numbers;
    bool heading = false;
    bool restyle() { return sink.style(bold || heading, italic); }
};
static int enter_block(MD_BLOCKTYPE type, void *detail, void *data)
{
    auto &m = *(Markdown*)data;
    switch (type) {
    case MD_BLOCK_H: m.heading = true; return !m.restyle();
    case MD_BLOCK_CODE: return !m.sink.preformatted(true);
    case MD_BLOCK_UL: m.list_numbers.push_back(0); break;
    case MD_BLOCK_OL: m.list_numbers.push_back(((MD_BLOCK_OL_DETAIL*)detail)->start); break;
    case MD_BLOCK_LI:
        if (!m.list_numbers.empty() && m.list_numbers.back()) {
            std::string number = std::to_string(m.list_numbers.back()++) + ". ";
            return !m.sink.text(number.data(), number.size());
        }
        return !m.sink.text("- ", 2);
    default: break;
    }
    return 0;
}
static int leave_block(MD_BLOCKTYPE type, void *, void *data)
{
    auto &m = *(Markdown*)data;
    switch (type) {
    case MD_BLOCK_UL: case MD_BLOCK_OL:
        if (!m.list_numbers.empty()) m.list_numbers.pop_back();
        break;
    case MD_BLOCK_H:
        m.heading = false;
        if (!m.restyle()) return 1;
        [[fallthrough]];
    case MD_BLOCK_P: case MD_BLOCK_LI: case MD_BLOCK_HR: case MD_BLOCK_TR:
        return !m.sink.newline();
    case MD_BLOCK_TD: case MD_BLOCK_TH: return !m.sink.text(" | ", 3);
    case MD_BLOCK_CODE:
        if (!m.sink.preformatted(false)) return 1;
        return !m.sink.newline();
    default: break;
    }
    return 0;
}
static int enter_span(MD_SPANTYPE type, void *, void *data)
{
    auto &m = *(Markdown*)data;
    if (type == MD_SPAN_STRONG) ++m.bold;
    if (type == MD_SPAN_EM) ++m.italic;
    return !m.restyle();
}
static int leave_span(MD_SPANTYPE type, void *, void *data)
{
    auto &m = *(Markdown*)data;
    if (type == MD_SPAN_STRONG && m.bold) --m.bold;
    if (type == MD_SPAN_EM && m.italic) --m.italic;
    return !m.restyle();
}
static int text(MD_TEXTTYPE type, const MD_CHAR *s, MD_SIZE n, void *data)
{
    auto &sink = ((Markdown*)data)->sink;
    if (type == MD_TEXT_HTML) return 0;
    if (type == MD_TEXT_BR) return !sink.newline();
    if (type == MD_TEXT_SOFTBR) return !sink.text(" ", 1);
    if (type == MD_TEXT_ENTITY) {
        std::string decoded = decode_entity(std::string(s, n));
        return !sink.text(decoded.data(), decoded.size());
    }
    return !sink.text(s, n);
}
int read_markdown(FILE *file, Sink &sink, Limits limits)
{
    std::string input;
    char chunk[1024];
    for (size_t n; (n = fread(chunk, 1, sizeof(chunk), file));) {
        if (n > limits.input_bytes - input.size()) return TooLarge;
        input.append(chunk, n);
    }
    if (ferror(file)) return ReadError;
    return read_markdown_data(input.data(), input.size(), sink, limits);
}
int read_markdown_data(const char *data, size_t size, Sink &sink, Limits limits)
{
    if (size > limits.input_bytes) return TooLarge;
    if (size >= 3 && !memcmp(data, "\xef\xbb\xbf", 3)) { data += 3; size -= 3; }
    Markdown state{sink, 0, 0, {}, false};
    MD_PARSER parser{};
    parser.flags = MD_FLAG_TABLES | MD_FLAG_NOHTML;
    parser.enter_block = enter_block;
    parser.leave_block = leave_block;
    parser.enter_span = enter_span;
    parser.leave_span = leave_span;
    parser.text = text;
    return md_parse(data, (MD_SIZE)size, &parser, &state) ? Invalid : Ok;
}
}
#endif
