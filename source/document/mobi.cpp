// SPDX-License-Identifier: MIT
#ifdef DSL_DOCUMENTS
#include "document.h"
#include "html.h"
#include "text_input.h"
#include <mobi.h>
#include <algorithm>
#include <memory>
#include <vector>
#include <sys/stat.h>

namespace document {
namespace {
int result(MOBI_RET status)
{
    if (status == MOBI_SUCCESS) return Ok;
    if (status == MOBI_MALLOC_FAILED) return TooLarge;
    if (status == MOBI_FILE_ENCRYPTED || status == MOBI_DRM_UNSUPPORTED) return Encrypted;
    if (status == MOBI_FILE_UNSUPPORTED) return Unsupported;
    return Invalid;
}
struct FileClose { void operator()(FILE *file) const { fclose(file); } };
unsigned big16(const unsigned char *p) { return (unsigned(p[0]) << 8) | p[1]; }
size_t big32(const unsigned char *p) { return (size_t(p[0]) << 24) | (size_t(p[1]) << 16) | (size_t(p[2]) << 8) | p[3]; }
// Validate the record table before libmobi allocates linked records. No extraction.
int validate(FILE *file, size_t size)
{
    unsigned char header[78];
    if (fread(header, 1, sizeof(header), file) != sizeof(header)) return Invalid;
    unsigned count = big16(header + 76);
    if (!count) return Invalid;
    if (count > 4096) return TooLarge;
    size_t previous = sizeof(header) + count * 8;
    if (previous > size) return Invalid;
    for (unsigned i = 0; i < count; ++i) {
        unsigned char record[8];
        if (fread(record, 1, sizeof(record), file) != sizeof(record)) return Invalid;
        size_t offset = big32(record);
        if (offset < previous || offset >= size) return Invalid;
        previous = offset;
    }
    rewind(file);
    return Ok;
}
}
int read_mobi(const char *path, Sink &sink, Limits limits)
{
    struct stat info;
    if (stat(path, &info) != 0) return ReadError;
    if (info.st_size < 78) return Invalid;
    if (uint64_t(info.st_size) > limits.input_bytes) return TooLarge;
    std::unique_ptr<FILE, FileClose> file(fopen(path, "rb"));
    if (!file) return ReadError;
    int status = validate(file.get(), size_t(info.st_size));
    if (status) return status;
    std::unique_ptr<MOBIData, decltype(&mobi_free)> book(mobi_init(), mobi_free);
    if (!book) return TooLarge;
    status = result(mobi_load_file(book.get(), file.get()));
    if (status) return status;
    if (!book->rh) return Invalid;
    if (mobi_is_encrypted(book.get())) return Encrypted;
    if (mobi_is_replica(book.get()) || mobi_is_dictionary(book.get())) return Unsupported;
    if (mobi_is_mobipocket(book.get()) && (!book->mh || !book->mh->text_encoding)) return Unsupported;
    size_t maximum = mobi_get_text_maxsize(book.get());
    // This allocation is additional to compressed records, markup copies and pages.
    size_t text_limit = std::min(size_t(1024*1024), limits.output_bytes) * 2;
    if (maximum == MOBI_NOTSET || maximum > text_limit || book->rh->text_length > text_limit) return TooLarge;
    if (!maximum) return NoText;
    Limits markup_limits = limits;
    markup_limits.input_bytes = text_limit;
    if (mobi_is_kf8(book.get())) {
        std::unique_ptr<MOBIRawml, decltype(&mobi_free_rawml)> raw(mobi_init_rawml(book.get()), mobi_free_rawml);
        if (!raw) return TooLarge;
        status = result(mobi_parse_rawml_opt(raw.get(), book.get(), false, false, false));
        if (status) return status;
        size_t total = 0;
        bool found = false;
        for (MOBIPart *part = raw->markup; part; part = part->next) {
            if (part->type != T_HTML) continue;
            if (part->size > text_limit - total) return TooLarge;
            total += part->size;
            // libmobi normalizes reconstructed markup to UTF-8; ignore stale meta charset.
            status = read_html_data(reinterpret_cast<const char*>(part->data), part->size,
                                    sink, markup_limits, Encoding::Utf8);
            if (status) return status;
            if (!sink.newline()) return LayoutError;
            found = true;
        }
        return found ? Ok : NoText;
    }
    std::vector<char> text(maximum + 1);
    size_t length = maximum;
    status = result(mobi_get_rawml(book.get(), text.data(), &length));
    if (status) return status;
    if (!mobi_is_mobipocket(book.get()))
        return read_text_data(text.data(), length, sink, markup_limits, Encoding::Windows1252);
    Encoding encoding = *book->mh->text_encoding == MOBI_CP1252 ? Encoding::Windows1252 : Encoding::Utf8;
    return read_html_data(text.data(), length, sink, markup_limits, encoding);
}
}
#endif
