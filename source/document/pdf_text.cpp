// SPDX-License-Identifier: MIT
#ifdef DSL_DOCUMENTS
#include "document.h"
#include "pdf_tokens.h"
#include "utf8.h"
#include "pdfio_glyph_names.h"
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <sys/stat.h>

namespace document {
// PDFio permits one open stream per file. Buffer each decoded content/CMap
// stream up to 128 KiB and close it before resolving fonts or nested Form objects.
static int stream_text(pdfio_stream_t *stream, std::string &data, size_t budget)
{
    if (!stream) return Invalid;
    int result = Ok;
    char chunk[4096];
    for (;;) {
        ssize_t n = pdfioStreamRead(stream, chunk, sizeof(chunk));
        if (n < 0) { result = Invalid; break; }
        if (!n) break;
        if (data.size() > std::min(budget, (size_t)128*1024)
            || (size_t)n > std::min(budget, (size_t)128*1024) - data.size()) { result = TooLarge; break; }
        data.append(chunk, n);
    }
    pdfioStreamClose(stream);
    return result;
}
static pdfio_dict_t *dictionary(pdfio_dict_t *parent, const char *key)
{
    pdfio_dict_t *dict = pdfioDictGetDict(parent, key);
    if (dict) return dict;
    pdfio_obj_t *obj = pdfioDictGetObj(parent, key);
    return obj ? pdfioObjGetDict(obj) : nullptr;
}
static pdfio_dict_t *resources(pdfio_dict_t *page)
{
    for (int depth = 0; page && depth < 16; ++depth) {
        if (auto *dict = dictionary(page, "Resources")) return dict;
        page = dictionary(page, "Parent");
    }
    return nullptr;
}
static std::string unicode(uint32_t code)
{
    char bytes[4];
    size_t length = utf8_encode(code, bytes);
    return std::string(bytes, length);
}
static std::string utf16(const std::string &bytes)
{
    std::string result;
    for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
        uint32_t code = ((unsigned char)bytes[i] << 8) | (unsigned char)bytes[i+1];
        if (code >= 0xd800 && code <= 0xdbff && i + 3 < bytes.size()) {
            uint32_t low = ((unsigned char)bytes[i+2] << 8) | (unsigned char)bytes[i+3];
            if (low >= 0xdc00 && low <= 0xdfff) { code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00; i += 2; }
        }
        if (code != 0xfeff) result += unicode(code);
    }
    return result;
}
static uint32_t number(const std::string &bytes)
{
    uint32_t code = 0;
    for (unsigned char c : bytes) code = (code << 8) | c;
    return code;
}
static uint32_t glyph_name(const char *name)
{
    if (!name) return 0xfffd;
    if (strlen(name) == 1) return (unsigned char)name[0];
    if (!strncmp(name, "uni", 3)) return strtoul(name + 3, nullptr, 16);
    if (*name == 'u' && strlen(name) >= 5) return strtoul(name + 1, nullptr, 16);
    for (const auto &entry : unicode_map) if (!strcmp(name, entry.name)) return entry.unicode;
    return 0xfffd;
}

struct Mapping { uint64_t key; std::string text; };
class PdfFont {
public:
    int load(pdfio_dict_t *resource, const std::string &name, size_t &budget);
    bool show(const std::string &bytes, Sink &sink);
private:
    uint32_t encoding[256]{};
    std::vector<Mapping> mapping;
    unsigned min_bytes = 1, max_bytes = 1;
    bool mapped = false, valid = true;
    int cmap(pdfio_obj_t *obj, size_t &budget);
    bool add(const PdfToken &source, const std::string &target);
};
bool PdfFont::add(const PdfToken &source, const std::string &target)
{
    if (source.kind != PdfToken::String || source.value.empty() || source.value.size() > 4
        || target.empty() || target.size() > 128 || mapping.size() >= 8192) return false;
    unsigned width = source.value.size();
    mapping.push_back({((uint64_t)width << 32) | number(source.value), utf16(target)});
    min_bytes = std::min(min_bytes, width);
    max_bytes = std::max(max_bytes, width);
    return true;
}
int PdfFont::cmap(pdfio_obj_t *obj, size_t &budget)
{
    std::string data;
    int read_status = stream_text(pdfioObjOpenStream(obj, true), data, budget);
    if (read_status) return read_status;
    PdfTokens tokens(data, budget);
    PdfToken token, source, last, target;
    int result = Ok;
    min_bytes = 4;
    max_bytes = 1;
    while (tokens.next(token)) {
        if (token.value == "beginbfchar") {
            while (tokens.next(source) && source.value != "endbfchar") {
                if (!tokens.next(target) || target.kind != PdfToken::String || !add(source, target.value)) { result = Unsupported; break; }
            }
            if (!result && source.value != "endbfchar") result = Invalid;
        } else if (token.value == "beginbfrange") {
            while (tokens.next(source) && source.value != "endbfrange") {
                if (!tokens.next(last) || !tokens.next(target) || source.kind != PdfToken::String
                    || last.kind != PdfToken::String || source.value.size() != last.value.size()
                    || source.value.empty() || source.value.size() > 4) { result = Invalid; break; }
                uint32_t first_code = number(source.value), last_code = number(last.value);
                if (last_code < first_code || last_code - first_code >= 8192) { result = Unsupported; break; }
                bool array = target.kind == PdfToken::Punctuation && target.value == "[";
                if (array && !tokens.next(target)) { result = Invalid; break; }
                for (uint32_t offset = 0; offset <= last_code - first_code; ++offset) {
                    PdfToken from = source;
                    uint32_t value = first_code + offset;
                    for (size_t i = from.value.size(); i--;) { from.value[i] = (char)value; value >>= 8; }
                    if (target.kind != PdfToken::String || !add(from, target.value)) { result = Unsupported; break; }
                    if (array) {
                        if (!tokens.next(target)) { result = Invalid; break; }
                    } else {
                        for (size_t i = target.value.size(); i--;) {
                            unsigned next = ((unsigned char)target.value[i] + 1) & 255;
                            target.value[i] = (char)next;
                            if (next) break;
                        }
                    }
                }
                if (result || (array && target.value != "]")) { if (!result) result = Invalid; break; }
            }
            if (!result && source.value != "endbfrange") result = Invalid;
        }
        if (result) break;
    }
    if (tokens.failed) result = Invalid;
    std::sort(mapping.begin(), mapping.end(), [](const Mapping &a, const Mapping &b) { return a.key < b.key; });
    if (mapping.empty()) return Unsupported;
    return result;
}
int PdfFont::load(pdfio_dict_t *resource, const std::string &name, size_t &budget)
{
    mapping.clear();
    mapped = false;
    valid = true;
    static const uint32_t win_ansi[32] = {
        0x20ac,0xfffd,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,0x2c6,0x2030,0x160,0x2039,0x152,0xfffd,0x17d,0xfffd,
        0xfffd,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0xfffd,0x17e,0x178
    };
    for (unsigned i = 0; i < 256; ++i) encoding[i] = i >= 128 && i < 160 ? win_ansi[i - 128] : i;
    auto *fonts = resource ? dictionary(resource, "Font") : nullptr;
    auto *font = fonts ? dictionary(fonts, name.c_str()) : nullptr;
    if (!font) return name.empty() ? Ok : Unsupported;
    if (auto *to_unicode = pdfioDictGetObj(font, "ToUnicode")) {
        int status = cmap(to_unicode, budget);
        mapped = !status;
        valid = !status;
        return status;
    }
    const char *subtype = pdfioDictGetName(font, "Subtype");
    if (subtype && !strcmp(subtype, "Type0")) { valid = false; return Unsupported; }
    auto *enc = dictionary(font, "Encoding");
    const char *base = enc ? pdfioDictGetName(enc, "BaseEncoding") : pdfioDictGetName(font, "Encoding");
    // Never present a known unsupported encoding as correctly decoded text.
    if (base && strcmp(base, "WinAnsiEncoding") && strcmp(base, "StandardEncoding")) { valid = false; return Unsupported; }
    if (base && !strcmp(base, "StandardEncoding")) {
        for (unsigned i = 128; i < 256; ++i) encoding[i] = 0xfffd;
    }
    if (enc) {
        pdfio_array_t *differences = pdfioDictGetArray(enc, "Differences");
        unsigned index = 0;
        for (size_t i = 0, count = differences ? pdfioArrayGetSize(differences) : 0; i < count; ++i) {
            if (pdfioArrayGetType(differences, i) == PDFIO_VALTYPE_NUMBER) index = (unsigned)pdfioArrayGetNumber(differences, i);
            else if (index < 256) encoding[index++] = glyph_name(pdfioArrayGetName(differences, i));
        }
    }
    return Ok;
}
bool PdfFont::show(const std::string &bytes, Sink &sink)
{
    if (!valid) return false;
    if (!mapped) {
        std::string text;
        for (unsigned char c : bytes) text += unicode(encoding[c]);
        return sink.text(text.data(), text.size());
    }
    for (size_t i = 0; i < bytes.size();) {
        bool found = false;
        for (unsigned n = min_bytes; n <= max_bytes && i + n <= bytes.size(); ++n) {
            uint64_t key = ((uint64_t)n << 32) | number(bytes.substr(i, n));
            auto entry = std::lower_bound(mapping.begin(), mapping.end(), key, [](const Mapping &a, uint64_t b) { return a.key < b; });
            if (entry != mapping.end() && entry->key == key) {
                if (!sink.text(entry->text.data(), entry->text.size())) return false;
                i += n;
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

static bool error_callback(pdfio_file_t *, const char *, void *) { return false; }

static int content(const std::string &data, pdfio_dict_t *resource, Sink &sink, size_t &budget, unsigned depth)
{
    PdfTokens tokens(data, budget);
    PdfFont font;
    font.load(nullptr, "", budget);
    PdfToken token;
    std::vector<PdfToken> operands;
    size_t operand_bytes = 0;
    bool in_text = false, have_text = false;
    while (tokens.next(token)) {
        char *end;
        strtod(token.value.c_str(), &end);
        bool numeric = token.kind == PdfToken::Word && !token.value.empty() && !*end;
        if (token.kind != PdfToken::Word || numeric) {
            if (operands.size() >= 512 || token.value.size() > 65536 - operand_bytes) return TooLarge;
            operand_bytes += token.value.size();
            operands.push_back(token);
            continue;
        }
        const auto &op = token.value;
        if (op == "BI") return Unsupported; // Binary inline images require a separate image lexer.
        if (op == "BT") in_text = true;
        else if (op == "ET") { in_text = false; if (have_text && !sink.newline()) return LayoutError; have_text = false; }
        else if (in_text && op == "Tf" && operands.size() >= 2) {
            int status = font.load(resource, operands[operands.size()-2].value, budget);
            if (status) return status;
        } else if (in_text && (op == "Tj" || op == "TJ" || op == "'" || op == "\"")) {
            if ((op == "'" || op == "\"") && have_text && !sink.newline()) return LayoutError;
            for (const auto &arg : operands) {
                if (arg.kind == PdfToken::String) {
                    if (!font.show(arg.value, sink)) return Unsupported;
                    have_text = have_text || !arg.value.empty();
                } else if (op == "TJ" && arg.kind == PdfToken::Word && atof(arg.value.c_str()) < -100) {
                    if (!sink.text(" ", 1)) return LayoutError;
                }
            }
        } else if (in_text && have_text && (op == "Td" || op == "TD" || op == "Tm" || op == "T*")) {
            if (!sink.newline()) return LayoutError;
            have_text = false;
        } else if (op == "Do" && !operands.empty() && resource) {
            auto *objects = dictionary(resource, "XObject");
            auto *obj = objects ? pdfioDictGetObj(objects, operands.back().value.c_str()) : nullptr;
            auto *dict = obj ? pdfioObjGetDict(obj) : nullptr;
            const char *subtype = dict ? pdfioDictGetName(dict, "Subtype") : nullptr;
            if (subtype && !strcmp(subtype, "Form")) {
                if (depth >= 4) return TooLarge;
                std::string nested;
                int status = stream_text(pdfioObjOpenStream(obj, true), nested, budget);
                if (status) return status;
                auto *nested_resources = dictionary(dict, "Resources");
                status = content(nested, nested_resources ? nested_resources : resource, sink, budget, depth + 1);
                if (status) return status;
            }
        }
        operands.clear();
        operand_bytes = 0;
    }
    if (tokens.failed) return Invalid;
    return have_text && !sink.newline() ? LayoutError : Ok;
}

int read_pdf(const char *path, Sink &sink, Limits limits)
{
    struct stat info;
    if (stat(path, &info) || info.st_size <= 0) return ReadError;
    if ((uint64_t)info.st_size > limits.input_bytes) return TooLarge;
    pdfio_file_t *file = pdfioFileOpen(path, nullptr, nullptr, error_callback, nullptr);
    if (!file) return Invalid;
    int status = pdfioFileGetPermissions(file, nullptr) == PDFIO_PERMISSION_NONE ? Unsupported : Ok;
    // Password-protected files are not opened interactively by this reader.
    pdfio_encryption_t encryption;
    pdfioFileGetPermissions(file, &encryption);
    if (encryption != PDFIO_ENCRYPTION_NONE) status = Unsupported;
    size_t budget = limits.input_bytes;
    size_t count = pdfioFileGetNumPages(file);
    if (count > limits.pages) status = TooLarge;
    for (size_t i = 0; !status && i < count; ++i) {
        auto *page = pdfioFileGetPage(file, i);
        if (!page) { status = Invalid; break; }
        auto *resource = resources(pdfioObjGetDict(page));
        std::string text;
        for (size_t j = 0, streams = pdfioPageGetNumStreams(page); !status && j < streams; ++j)
            status = stream_text(pdfioPageOpenStream(page, j, true), text, budget);
        if (!status) status = content(text, resource, sink, budget, 0);
        if (!status && !sink.newline()) status = LayoutError;
    }
    pdfioFileClose(file);
    return status;
}
}
#endif
