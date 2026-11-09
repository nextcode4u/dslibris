// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include "document_format.h"

namespace document {
enum Error { Ok = 0, ReadError = 240, TooLarge, Invalid, Unsupported, NoText, LayoutError };
const char *error_message(int code);

struct Limits {
    size_t input_bytes;
    size_t output_bytes;
    size_t pages;
};

class Sink {
public:
    virtual ~Sink() = default;
    virtual bool text(const char *data, size_t length) = 0;
    virtual bool newline() = 0;
    virtual bool style(bool bold, bool italic) = 0;
    virtual bool preformatted(bool enabled) = 0;
};

// Renderer-independent output lets the same layout run in native tests.
class Output {
public:
    virtual ~Output() = default;
    virtual unsigned advance(uint32_t code, bool bold, bool italic) = 0;
    virtual bool page(const char *data, size_t length) = 0;
};

class Layout : public Sink {
public:
    Layout(Output &output, unsigned width, unsigned lines_per_screen, Limits limits);
    bool text(const char *data, size_t length) override;
    bool newline() override;
    bool style(bool bold, bool italic) override;
    bool preformatted(bool enabled) override;
    int finish();
private:
    Output &output;
    Limits limits;
    unsigned width, lines_per_page, line = 0, used = 0;
    size_t total = 0, pages = 0;
    bool bold = false, italic = false, literal = false, space = false, visible = false, page_visible = false;
    int error = Ok;
    std::string buffer, word;
    bool append(const char *data, size_t length);
    bool save_page();
    bool flush_word();
    bool glyph(uint32_t code);
};

int read_text(FILE *file, Sink &sink, Limits limits);
int read_markdown(FILE *file, Sink &sink, Limits limits);
int read_pdf(const char *path, Sink &sink, Limits limits);
}
