// SPDX-License-Identifier: MIT
#ifdef DSL_DOCUMENTS
#include "document.h"
#include "text_input.h"
#include "expat.h"
#include "unzip.h"
#include <cstring>
#include <cstdlib>
#include <strings.h>
#include <algorithm>
#include <vector>
#include <sys/stat.h>

namespace document {
namespace {
const char *local(const char *name)
{
    const char *separator = strrchr(name, '|');
    return separator ? separator + 1 : name;
}
const char *value(const char **attributes, const char *key)
{
    for (size_t i = 0; attributes && attributes[i]; i += 2)
        if (!strcmp(local(attributes[i]), key)) return attributes[i+1];
    return "";
}
struct Element { std::string name; bool visible, bold, italic; };
class XmlBook {
    XML_Parser parser;
    Sink &sink;
    format_t format;
    Limits limits;
    std::vector<Element> stack;
    size_t consumed = 0;
    int error = Ok;
    bool root = false, run_bold = false, run_italic = false;
    bool check(bool result) { if (!result && !error) stop(LayoutError); return result; }
    void stop(int status) { error = status; XML_StopParser(parser, XML_FALSE); }
    bool ancestor(const char *name) const {
        for (const auto &element : stack) if (element.name == name) return true;
        return false;
    }
    static void start(void *data, const char *tag, const char **attributes) {
        auto &book = *(XmlBook*)data;
        const char *name = local(tag);
        if (book.stack.size() >= 128) { book.stop(TooLarge); return; }
        if (book.stack.empty()) {
            const char *expected = book.format == FORMAT_FB2 ? "FictionBook" : book.format == FORMAT_DOCX ? "document" : "document-content";
            if (strcmp(name, expected)) { book.stop(Invalid); return; }
            book.root = true;
        }
        Element state = book.stack.empty() ? Element{"",false,false,false} : book.stack.back();
        state.name = name;
        if (!strcmp(name, "body") && book.stack.size() == 1) state.visible = true;
        if (!strcmp(name, "binary") || !strcmp(name, "binary-data") || !strcmp(name, "del")
            || !strcmp(name, "drawing") || !strcmp(name, "object")) state.visible = false;
        if (book.format == FORMAT_FB2) {
            state.bold = state.bold || !strcmp(name, "strong") || !strcmp(name, "title") || !strcmp(name, "subtitle");
            state.italic = state.italic || !strcmp(name, "emphasis");
        }
        if (book.format == FORMAT_ODT && !strcmp(name, "h")) state.bold = true;
        if (book.format == FORMAT_DOCX) {
            if (!strcmp(name, "r")) book.run_bold = book.run_italic = false;
            if (book.ancestor("rPr") && (!strcmp(name, "b") || !strcmp(name, "i"))) {
                const char *setting = value(attributes, "val");
                bool enabled = strcmp(setting, "0") && strcmp(setting, "false") && strcmp(setting, "off");
                if (!strcmp(name, "b")) book.run_bold = enabled; else book.run_italic = enabled;
            }
        }
        book.stack.push_back(state);
        if (!state.visible) return;
        if (!book.check(book.sink.style(state.bold || book.run_bold, state.italic || book.run_italic))) return;
        if (!strcmp(name, "empty-line") || !strcmp(name, "br") || !strcmp(name, "cr") || !strcmp(name, "line-break"))
            book.check(book.sink.newline());
        if (!strcmp(name, "list-item")) book.check(book.sink.text("- ", 2));
        if (!strcmp(name, "s") || !strcmp(name, "tab")) {
            unsigned count = !strcmp(name, "tab") ? 4 : 1;
            const char *number = value(attributes, "c");
            if (*number) {
                char *end; unsigned long parsed = strtoul(number, &end, 10);
                if (*end || parsed > 256) { book.stop(TooLarge); return; }
                count = parsed;
            }
            std::string spaces(count, ' ');
            book.check(book.sink.preformatted(true) && book.sink.text(spaces.data(), spaces.size()) && book.sink.preformatted(false));
        }
    }
    static void text(void *data, const char *s, int size) {
        auto &book = *(XmlBook*)data;
        if (book.error || book.stack.empty() || !book.stack.back().visible) return;
        if (book.format == FORMAT_DOCX && book.stack.back().name != "t") return;
        // XML indentation between elements is not a paragraph break.
        bool words = false;
        for (int i = 0; i < size; ++i) if ((unsigned char)s[i] > 32) { words = true; break; }
        if (!words && book.format != FORMAT_DOCX && !book.ancestor("p") && !book.ancestor("h") && !book.ancestor("v")) return;
        std::string normalized(s, size);
        if (book.format != FORMAT_DOCX)
            for (char &c : normalized) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        book.check(book.sink.text(normalized.data(), normalized.size()));
    }
    static void end(void *data, const char *) {
        auto &book = *(XmlBook*)data;
        if (book.stack.empty() || book.error) return;
        Element state = book.stack.back();
        const auto &name = state.name;
        if (state.visible) {
            if (name == "p" || name == "h" || name == "v" || name == "stanza" || name == "body") book.check(book.sink.newline());
            if (name == "td" || name == "tc" || name == "table-cell") book.check(book.sink.text(" | ", 3));
        }
        if (book.format == FORMAT_DOCX && name == "r") book.run_bold = book.run_italic = false;
        book.stack.pop_back();
        if (!book.error) {
            bool bold = !book.stack.empty() && book.stack.back().bold;
            bool italic = !book.stack.empty() && book.stack.back().italic;
            book.check(book.sink.style(bold || book.run_bold, italic || book.run_italic));
        }
    }
    static void doctype(void *data, const char *, const char *, const char *, int) { ((XmlBook*)data)->stop(Unsupported); }
    static int unknown(void *, const char *name, XML_Encoding *info) {
        Encoding encoding;
        if (!strcasecmp(name, "windows-1251") || !strcasecmp(name, "cp1251")) encoding = Encoding::Windows1251;
        else if (!strcasecmp(name, "windows-1252") || !strcasecmp(name, "cp1252")) encoding = Encoding::Windows1252;
        else return XML_STATUS_ERROR;
        for (unsigned i = 0; i < 256; ++i) info->map[i] = decode_byte(i, encoding);
        info->data = nullptr; info->convert = nullptr; info->release = nullptr;
        return XML_STATUS_OK;
    }
public:
    XmlBook(Sink &sink, format_t format, Limits limits) : parser(XML_ParserCreateNS(nullptr, '|')), sink(sink), format(format), limits(limits) {
        if (!parser) { error = LayoutError; return; }
        XML_SetUserData(parser, this);
        XML_SetElementHandler(parser, start, end);
        XML_SetCharacterDataHandler(parser, text);
        XML_SetStartDoctypeDeclHandler(parser, doctype);
        XML_SetUnknownEncodingHandler(parser, unknown, nullptr);
        XML_SetParamEntityParsing(parser, XML_PARAM_ENTITY_PARSING_NEVER);
    }
    ~XmlBook() { if (parser) XML_ParserFree(parser); }
    int feed(const char *bytes, size_t size, bool final) {
        if (error) return error;
        if (size > limits.input_bytes - consumed) return error = TooLarge;
        consumed += size;
        if (XML_Parse(parser, bytes, size, final) == XML_STATUS_ERROR) return error ? error : Invalid;
        if (final && !root) return Invalid;
        return error;
    }
};
int archive_book(const char *path, format_t format, Sink &sink, Limits limits)
{
    unzFile archive = unzOpen(path);
    if (!archive) return Invalid;
    unz_global_info64 info{};
    int status = Ok;
    if (unzGetGlobalInfo64(archive, &info) != UNZ_OK) status = Invalid;
    if (info.number_entry > 4096) status = TooLarge;
    char selected[512] = "";
    unsigned matches = 0;
    int entry = status ? UNZ_END_OF_LIST_OF_FILE : unzGoToFirstFile(archive);
    for (; !status && entry == UNZ_OK; entry = unzGoToNextFile(archive)) {
        unz_file_info64 file{};
        char name[512];
        if (unzGetCurrentFileInfo64(archive, &file, name, sizeof name, nullptr, 0, nullptr, 0) != UNZ_OK) { status = Invalid; break; }
        if (file.size_filename >= sizeof name) { status = TooLarge; break; }
        size_t length = strlen(name);
        bool match = format == FORMAT_DOCX ? !strcmp(name, "word/document.xml")
            : format == FORMAT_ODT ? !strcmp(name, "content.xml")
            : length >= 4 && !strcasecmp(name + length - 4, ".fb2");
        if (match) {
            if (++matches > 1) { status = Invalid; break; }
            if (file.flag & 1) { status = Encrypted; break; }
            if (file.uncompressed_size > limits.input_bytes) { status = TooLarge; break; }
            strcpy(selected, name);
        }
    }
    if (!status && entry != UNZ_END_OF_LIST_OF_FILE) status = Invalid;
    if (!status && matches != 1) status = Invalid;
    if (!status && (unzLocateFile(archive, selected, 1) != UNZ_OK || unzOpenCurrentFile(archive) != UNZ_OK)) status = Invalid;
    if (!status) {
        XmlBook reader(sink, format == FORMAT_FB2_ZIP ? FORMAT_FB2 : format, limits);
        char chunk[4096];
        for (;;) {
            int n = unzReadCurrentFile(archive, chunk, sizeof chunk);
            if (n < 0) { status = Invalid; break; }
            status = reader.feed(chunk, n, n == 0);
            if (status || !n) break;
        }
        if (unzCloseCurrentFile(archive) != UNZ_OK && !status) status = Invalid;
    }
    unzClose(archive);
    return status;
}
}

int read_xml_book(const char *path, format_t format, Sink &sink, Limits limits)
{
    struct stat info;
    if (stat(path, &info) || info.st_size < 0) return ReadError;
    if ((uint64_t)info.st_size > limits.input_bytes) return TooLarge;
    if (format != FORMAT_FB2) return archive_book(path, format, sink, limits);
    FILE *file = fopen(path, "rb");
    if (!file) return ReadError;
    XmlBook reader(sink, format, limits);
    int status;
    char chunk[4096];
    for (;;) {
        size_t n = fread(chunk, 1, sizeof chunk, file);
        if (ferror(file)) { status = ReadError; break; }
        status = reader.feed(chunk, n, n == 0);
        if (status || !n) break;
    }
    fclose(file);
    return status;
}
}
#endif
