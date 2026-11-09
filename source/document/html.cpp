// SPDX-License-Identifier: MIT
#ifdef DSL_DOCUMENTS
#include "html.h"
#include "entities.h"
#include "text_input.h"
#include "utf8.h"
#include <algorithm>
#include <cctype>
#include <vector>

namespace document {
namespace {
std::string lower(std::string value)
{
    for (char &c : value) if ((unsigned char)c < 128) c = (char)tolower((unsigned char)c);
    return value;
}
std::string attribute(const std::string &tag, const std::string &wanted)
{
    size_t i = tag.find_first_of(" \t\r\n");
    if (i == std::string::npos) return {};
    while (i < tag.size()) {
        while (i < tag.size() && isspace((unsigned char)tag[i])) ++i;
        size_t start = i;
        while (i < tag.size() && !isspace((unsigned char)tag[i]) && tag[i] != '=') ++i;
        std::string name = lower(tag.substr(start, i - start));
        while (i < tag.size() && isspace((unsigned char)tag[i])) ++i;
        if (i == tag.size()) break;
        if (tag[i] != '=') continue;
        ++i;
        while (i < tag.size() && isspace((unsigned char)tag[i])) ++i;
        char quote = i < tag.size() && (tag[i] == '\'' || tag[i] == '"') ? tag[i++] : 0;
        start = i;
        while (i < tag.size() && (quote ? tag[i] != quote : !isspace((unsigned char)tag[i]) && tag[i] != '>')) ++i;
        std::string value = tag.substr(start, i - start);
        if (quote && i < tag.size()) ++i;
        if (name == wanted) return value;
    }
    return {};
}
bool is_block(const std::string &name)
{
    return name == "p" || name == "div" || name == "li" || name == "section" || name == "article"
        || name == "blockquote" || name == "pre" || name == "tr" || name == "dt" || name == "dd"
        || (name.size() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6');
}
struct Frame { std::string name; bool bold, italic, literal, hidden; };
class Html {
    TextInput &input;
    Sink &sink;
    std::vector<Frame> stack;
    bool bold = false, italic = false, literal = false, hidden = false;
    bool content = false;
    int32_t held = -1;
    int error = Ok;
    std::string raw_name;
    size_t raw_match = 0;
    std::string text;
    int32_t next() { if (held < 0) return input.next(); int32_t c = held; held = -1; return c; }
    bool flush() {
        if (!hidden && !text.empty()) {
            if (!sink.text(text.data(), text.size())) return false;
            content = true;
        }
        text.clear();
        return true;
    }
    bool restyle() { return sink.style(bold, italic) && sink.preformatted(literal); }
    bool close(const std::string &name) {
        for (size_t i = stack.size(); i--;) if (stack[i].name == name) {
            const Frame previous = stack[i];
            stack.resize(i);
            bold = previous.bold; italic = previous.italic; literal = previous.literal; hidden = previous.hidden;
            return restyle();
        }
        return true;
    }
    bool charset(const std::string &name) {
        std::string value = lower(name);
        if (value.empty()) return true;
        if (value == "utf-8" || value == "utf8" || value == "us-ascii") return input.set_encoding(Encoding::Utf8);
        if (value == "utf-16" || value == "utf-16le") return input.set_encoding(Encoding::Utf16LE);
        if (value == "utf-16be") return input.set_encoding(Encoding::Utf16BE);
        if (value == "windows-1252" || value == "cp1252") return input.set_encoding(Encoding::Windows1252);
        if (value == "windows-1251" || value == "cp1251") return input.set_encoding(Encoding::Windows1251);
        if (value == "iso-8859-1" || value == "latin1") return input.set_encoding(Encoding::Latin1);
        error = Unsupported;
        return false;
    }
    bool tag(std::string value) {
        if (value.empty() || value[0] == '!') return true;
        bool ending = value[0] == '/';
        size_t begin = ending ? 1 : 0, end = begin;
        while (end < value.size() && !isspace((unsigned char)value[end]) && value[end] != '/') ++end;
        std::string name = lower(value.substr(begin, end - begin));
        if (!flush()) return false;
        if (name == "meta" || name == "?xml") {
            std::string encoding = attribute(value, name == "meta" ? "charset" : "encoding");
            if (encoding.empty() && name == "meta") {
                std::string raw = lower(attribute(value, "content"));
                size_t pos = raw.find("charset=");
                if (pos != std::string::npos) {
                    encoding = raw.substr(pos + 8);
                    encoding = encoding.substr(0, encoding.find_first_of("; \t\r\n"));
                }
            }
            return charset(encoding);
        }
        if (ending) {
            if (!hidden && (is_block(name) || name == "br") && content && !sink.newline()) return false;
            return close(name);
        }
        if (name == "body" && !close("head")) return false;
        if ((name == "p" || name == "li") && !close(name)) return false;
        if (!hidden && is_block(name) && content && !sink.newline()) return false;
        if (name == "br" || name == "hr") return hidden || sink.newline();
        if (name == "img") {
            std::string alt = attribute(value, "alt");
            return hidden || sink.text(alt.data(), alt.size());
        }
        if (name == "meta" || name == "link" || name == "input" || name == "wbr" || name.empty()) return true;
        if (stack.size() >= 128) { error = TooLarge; return false; }
        stack.push_back({name, bold, italic, literal, hidden});
        hidden = hidden || name == "head" || name == "script" || name == "style" || name == "title" || name == "noscript";
        bold = bold || name == "b" || name == "strong" || (name.size() == 2 && name[0] == 'h' && isdigit(name[1]));
        italic = italic || name == "i" || name == "em";
        literal = literal || name == "pre";
        if (!hidden && name == "li" && !sink.text("- ", 2)) return false;
        if (!hidden && name == "td" && content && !sink.text(" | ", 3)) return false;
        if (name == "script" || name == "style") { raw_name = name; raw_match = 0; }
        if (!restyle()) return false;
        if (!value.empty() && value.back() == '/') return close(name);
        return true;
    }
public:
    Html(TextInput &input, Sink &sink) : input(input), sink(sink) {}
    int run() {
        for (int32_t c; (c = next()) >= 0;) {
            if (!raw_name.empty()) {
                const std::string pattern = "</" + raw_name;
                int32_t folded = c < 128 ? tolower((unsigned char)c) : c;
                raw_match = folded == pattern[raw_match] ? raw_match + 1 : folded == '<' ? 1 : 0;
                if (raw_match == pattern.size()) {
                    c = next();
                    if (c != '>' && (c < 0 || c > 127 || !isspace((unsigned char)c))) { raw_match = 0; continue; }
                    while (c >= 0 && c != '>') c = next();
                    if (c < 0) return input.status() ? input.status() : Invalid;
                    std::string name = raw_name;
                    raw_name.clear(); raw_match = 0;
                    if (!tag("/" + name)) return LayoutError;
                }
                continue;
            }
            if (c == '<') {
                std::string value;
                char quote = 0;
                bool complete = false;
                while ((c = next()) >= 0) {
                    if (value == "![CDATA[") {
                        int32_t a = -1, b = -1;
                        do {
                            if (a == ']' && b == ']' && c == '>') { complete = true; break; }
                            if (a >= 0 && !hidden) {
                                char bytes[4]; text.append(bytes, utf8_encode(a, bytes));
                                if (text.size() >= 512 && !flush()) return LayoutError;
                            }
                            a = b; b = c;
                        } while ((c = next()) >= 0);
                        value.clear();
                        break;
                    }
                    if (value == "!--") {
                        int32_t a = 0, b = c;
                        while ((c = next()) >= 0) { if (a == '-' && b == '-' && c == '>') { complete = true; break; } a = b; b = c; }
                        value.clear();
                        break;
                    }
                    if (!quote && c == '>') { complete = true; break; }
                    if (c == '\'' || c == '"') { if (!quote) quote = c; else if (quote == c) quote = 0; }
                    char bytes[4]; value.append(bytes, utf8_encode(c, bytes));
                    if (value.size() > 4096) return TooLarge;
                }
                if (!complete) return input.status() ? input.status() : Invalid;
                if (!tag(value)) return error ? error : LayoutError;
            } else if (c == '&' && !hidden) {
                std::string entity = "&";
                while ((c = next()) >= 0) {
                    if (c > 127 || c == '<' || isspace((unsigned char)c)) { held = c; break; }
                    entity += (char)c;
                    if (c == ';' || entity.size() >= 64) break;
                }
                text += decode_entity(entity);
            } else if (!hidden) {
                if (!literal && c < 128 && isspace((unsigned char)c)) c = ' ';
                char bytes[4]; text.append(bytes, utf8_encode(c, bytes));
            }
            if (text.size() >= 512 && !flush()) return LayoutError;
        }
        if (input.status()) return input.status();
        if (!flush() || !sink.style(false, false) || !sink.preformatted(false)) return LayoutError;
        return Ok;
    }
};
}
int read_html(FILE *file, Sink &sink, Limits limits)
{
    TextInput input(file, limits.input_bytes);
    return Html(input, sink).run();
}
int read_html_data(const char *data, size_t size, Sink &sink, Limits limits, Encoding encoding)
{
    TextInput input(data, size, limits.input_bytes, encoding);
    return Html(input, sink).run();
}
}
#endif
