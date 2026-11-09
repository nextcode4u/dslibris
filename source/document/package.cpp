// SPDX-License-Identifier: MIT
#ifdef DSL_DOCUMENTS
#include "document.h"
#include "html.h"
#include "text_input.h"
#include <expat.h>
#include <unzip.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <sys/stat.h>
#include <vector>

namespace document {
namespace {
const char *local_name(const char *name)
{
    const char *separator = strrchr(name, '|');
    return separator ? separator + 1 : name;
}
const char *attribute(const char **attributes, const char *name)
{
    for (size_t i = 0; attributes && attributes[i]; i += 2)
        if (!strcmp(local_name(attributes[i]), name)) return attributes[i+1];
    return "";
}
bool suffix(const std::string &name, const char *ending)
{
    size_t length = strlen(ending);
    return name.size() >= length && !strcasecmp(name.c_str()+name.size()-length, ending);
}
bool html_name(const std::string &name)
{
    return suffix(name, ".html") || suffix(name, ".htm") || suffix(name, ".xhtml");
}
std::string parent(const std::string &name)
{
    size_t slash = name.find_last_of('/');
    return slash == std::string::npos ? "" : name.substr(0, slash+1);
}
int hex(unsigned char c)
{
    if (c >= '0' && c <= '9') return c-'0';
    if (c >= 'a' && c <= 'f') return c-'a'+10;
    if (c >= 'A' && c <= 'F') return c-'A'+10;
    return -1;
}
// Resolve a package URL within its containing directory, without fetching URLs.
bool chapter_path(const std::string &href, std::string &result)
{
    std::string decoded;
    for (size_t i = 0; i < href.size() && href[i] != '#'; ++i) {
        unsigned char c = href[i];
        if (c == '%') {
            if (i+2 >= href.size() || hex(href[i+1]) < 0 || hex(href[i+2]) < 0) return false;
            c = hex(href[i+1])*16 + hex(href[i+2]);
            i += 2;
        }
        if (c < 32 || c == 127 || c == ':' || c == '\\' || c == '?') return false;
        decoded += char(c);
    }
    if (decoded.empty() || decoded.front() == '/' || decoded.size() > 511) return false;
    std::vector<std::string> parts;
    for (size_t start = 0; start <= decoded.size();) {
        size_t end = decoded.find('/', start);
        if (end == std::string::npos) end = decoded.size();
        std::string part = decoded.substr(start, end-start);
        if (part == "..") { if (parts.empty()) return false; parts.pop_back(); }
        else if (!part.empty() && part != ".") parts.push_back(part);
        start = end+1;
    }
    result.clear();
    for (const auto &part : parts) { if (!result.empty()) result += '/'; result += part; }
    return !result.empty();
}

// ZIP entries are read directly and CRC checked; nothing is extracted to the SD.
class BookFiles {
    unzFile zip = nullptr;
    std::string directory;
    size_t remaining;
    std::vector<std::string> names;
public:
    int status = Ok;
    BookFiles(const char *path, bool archived, size_t budget) : remaining(budget) {
        if (!archived) { directory = parent(path); return; }
        struct stat info;
        if (stat(path, &info) || info.st_size < 0) { status = ReadError; return; }
        if (uint64_t(info.st_size) > budget) { status = TooLarge; return; }
        zip = unzOpen(path);
        if (!zip) { status = Invalid; return; }
        unz_global_info64 global{};
        if (unzGetGlobalInfo64(zip, &global) != UNZ_OK) { status = Invalid; return; }
        if (global.number_entry > 4096) { status = TooLarge; return; }
        size_t name_bytes = 0;
        size_t name_limit = std::min(budget, size_t(128*1024));
        int entry = unzGoToFirstFile(zip);
        for (; entry == UNZ_OK; entry = unzGoToNextFile(zip)) {
            unz_file_info64 info{};
            char name[512];
            if (unzGetCurrentFileInfo64(zip, &info, name, sizeof(name), nullptr, 0, nullptr, 0) != UNZ_OK) { status = Invalid; return; }
            if (info.size_filename >= sizeof(name)) { status = TooLarge; return; }
            if (strlen(name) != info.size_filename) { status = Invalid; return; }
            if (info.size_filename+1 > name_limit-name_bytes) { status = TooLarge; return; }
            name_bytes += info.size_filename+1;
            names.emplace_back(name);
        }
        if (entry != UNZ_END_OF_LIST_OF_FILE) status = Invalid;
    }
    ~BookFiles() { if (zip) unzClose(zip); }
    const std::vector<std::string> &entries() const { return names; }
    int read(const std::string &name, std::string &data, size_t maximum) {
        maximum = std::min(maximum, remaining);
        if (zip) {
            if (std::count(names.begin(), names.end(), name) != 1) return Invalid;
            if (unzLocateFile(zip, name.c_str(), 1) != UNZ_OK) return Invalid;
            unz_file_info64 info{};
            if (unzGetCurrentFileInfo64(zip, &info, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK) return Invalid;
            if (info.flag & 1) return Encrypted;
            if (info.uncompressed_size > maximum) return TooLarge;
            if (unzOpenCurrentFile(zip) != UNZ_OK) return Invalid;
            int status = Ok;
            data.clear();
            char chunk[4096];
            for (;;) {
                int n = unzReadCurrentFile(zip, chunk, sizeof(chunk));
                if (n < 0) { status = Invalid; break; }
                if (!n) break;
                if (size_t(n) > maximum-data.size()) { status = TooLarge; break; }
                data.append(chunk, n);
            }
            if (unzCloseCurrentFile(zip) != UNZ_OK && !status) status = Invalid;
            if (status) return status;
        } else {
            std::string path = directory + name;
            struct stat info;
            if (stat(path.c_str(), &info) || !S_ISREG(info.st_mode) || info.st_size < 0) return ReadError;
            if (uint64_t(info.st_size) > maximum) return TooLarge;
            FILE *file = fopen(path.c_str(), "rb");
            if (!file) return ReadError;
            data.resize(size_t(info.st_size));
            bool ok = fread(data.data(), 1, data.size(), file) == data.size() && !ferror(file);
            fclose(file);
            if (!ok) return ReadError;
        }
        remaining -= data.size();
        return Ok;
    }
};
struct Item { std::string href, type; };
class Package {
    XML_Parser parser;
    std::vector<std::string> stack;
    bool root = false, manifest = false, reading_order = false;
    void fail(int code) { status = code; XML_StopParser(parser, XML_FALSE); }
    static void start(void *user, const char *tag, const char **attributes) {
        auto &p = *static_cast<Package*>(user);
        const char *name = local_name(tag);
        if (p.stack.size() >= 64) { p.fail(TooLarge); return; }
        if (*attribute(attributes, "base")) { p.fail(Unsupported); return; }
        if (p.stack.empty()) {
            if (strcmp(name, "package")) { p.fail(Invalid); return; }
            p.root = true;
        } else if (p.stack.size() == 1) {
            if (!strcmp(name, "manifest")) {
                if (p.manifest) { p.fail(Invalid); return; }
                p.manifest = true;
            } else if (!strcmp(name, "spine")) {
                if (p.reading_order) { p.fail(Invalid); return; }
                p.reading_order = true;
            }
        } else if (p.stack.size() == 2 && p.stack.back() == "manifest" && !strcmp(name, "item")) {
            std::string id = attribute(attributes, "id"), href = attribute(attributes, "href"), type = attribute(attributes, "media-type");
            if (p.items.size() >= 1024) { p.fail(TooLarge); return; }
            if (id.empty() || id.size() > 511 || href.empty() || href.size() > 511 || type.size() > 127 || p.items.count(id)) { p.fail(Invalid); return; }
            p.items.emplace(id, Item{href, type});
        } else if (p.stack.size() == 2 && p.stack.back() == "spine" && !strcmp(name, "itemref")) {
            if (p.spine.size() >= 1024) { p.fail(TooLarge); return; }
            std::string id = attribute(attributes, "idref");
            if (id.empty() || id.size() > 511) { p.fail(Invalid); return; }
            if (strcmp(attribute(attributes, "linear"), "no")) p.spine.push_back(id);
        }
        p.stack.emplace_back(name);
    }
    static void end(void *user, const char *) { auto &p = *static_cast<Package*>(user); if (!p.stack.empty()) p.stack.pop_back(); }
    static void doctype(void *user, const char *name, const char *, const char *, int internal) {
        // Older OEB packages name a public DTD. Accept that declaration without
        // loading it; custom internal subsets/entity definitions are unsupported.
        if (strcmp(name, "package") || internal) static_cast<Package*>(user)->fail(Unsupported);
    }
public:
    int status = Ok;
    std::map<std::string, Item> items;
    std::vector<std::string> spine;
    Package() : parser(XML_ParserCreateNS(nullptr, '|')) {}
    ~Package() { if (parser) XML_ParserFree(parser); }
    int parse(const std::string &data) {
        if (!parser) return TooLarge;
        XML_SetUserData(parser, this);
        XML_SetElementHandler(parser, start, end);
        XML_SetStartDoctypeDeclHandler(parser, doctype);
        XML_SetParamEntityParsing(parser, XML_PARAM_ENTITY_PARSING_NEVER);
        if (XML_Parse(parser, data.data(), data.size(), true) != XML_STATUS_OK) return status ? status : Invalid;
        if (!root) return Invalid;
        return spine.empty() ? NoText : Ok;
    }
};
}
int read_package(const char *path, format_t format, Sink &sink, Limits limits)
{
    if (format != FORMAT_OPF && format != FORMAT_OEB_ZIP && format != FORMAT_HTML_ZIP
        && format != FORMAT_TEXT_ZIP && format != FORMAT_PML_ZIP) return Unsupported;
    bool archived = format != FORMAT_OPF;
    BookFiles files(path, archived, limits.input_bytes);
    if (files.status) return files.status;
    std::string selected;
    if (format == FORMAT_OPF) {
        selected = std::string(path).substr(parent(path).size());
    } else {
        std::vector<std::string> matches;
        for (const auto &name : files.entries()) {
            bool match = format == FORMAT_OEB_ZIP ? suffix(name, ".opf")
                : format == FORMAT_HTML_ZIP ? html_name(name) && parent(name).empty()
                : format == FORMAT_PML_ZIP ? suffix(name, ".pml")
                : suffix(name, ".txt") || suffix(name, ".text") || suffix(name, ".md") || suffix(name, ".markdown");
            if (match) matches.push_back(name);
        }
        if (matches.empty()) return NoText;
        if (matches.size() != 1) return Invalid; // Avoid silently losing chapters.
        selected = matches.front();
    }
    std::string data;
    size_t entry_limit = std::min(limits.input_bytes, limits.output_bytes);
    int status = files.read(selected, data, format == FORMAT_OPF || format == FORMAT_OEB_ZIP
                            ? std::min(size_t(256*1024), limits.input_bytes) : entry_limit);
    if (status) return status;
    if (format == FORMAT_PML_ZIP) return read_pml_data(data.data(), data.size(), sink, limits);
    if (format == FORMAT_TEXT_ZIP && document_format(selected.c_str()) == FORMAT_MARKDOWN) {
        Limits markdown = limits;
        size_t budget = limits.output_bytes <= 384*1024 ? 256*1024 : 1024*1024;
        markdown.input_bytes = std::min(markdown.input_bytes, budget);
        return read_markdown_data(data.data(), data.size(), sink, markdown);
    }
    if (format == FORMAT_TEXT_ZIP) return read_text_data(data.data(), data.size(), sink, limits, Encoding::Utf8);
    if (format == FORMAT_HTML_ZIP) return read_html_data(data.data(), data.size(), sink, limits, Encoding::Utf8, true);
    Package package;
    status = package.parse(data);
    if (status) return status;
    for (const auto &id : package.spine) {
        auto item = package.items.find(id);
        if (item == package.items.end()) return Invalid;
        const Item &chapter = item->second;
        bool plain = chapter.type == "text/plain";
        if (!plain && chapter.type != "application/xhtml+xml" && chapter.type != "text/html"
            && chapter.type != "text/x-oeb1-document") return Unsupported;
        std::string name;
        if (!chapter_path(chapter.href, name)) return Unsupported;
        status = files.read(parent(selected)+name, data, entry_limit);
        if (status) return status;
        if (!sink.style(false, false) || !sink.preformatted(false)) return LayoutError;
        status = plain ? read_text_data(data.data(), data.size(), sink, limits, Encoding::Utf8)
                       : read_html_data(data.data(), data.size(), sink, limits, Encoding::Utf8, true);
        if (status) return status;
        if (!sink.newline()) return LayoutError;
    }
    return Ok;
}
}
#endif
