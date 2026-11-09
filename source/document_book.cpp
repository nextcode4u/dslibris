// SPDX-License-Identifier: MIT
#ifdef DSL_DOCUMENTS
#include "document_book.h"
#include "document.h"
#include "book.h"
#include "app.h"
#include <algorithm>

namespace {
class BookOutput : public document::Output {
    Book *book;
    Text *text;
public:
    explicit BookOutput(Book *book) : book(book), text(book->GetApp()->ts) {}
    unsigned advance(uint32_t code, bool bold, bool italic) override {
        text->SetStyle(bold ? (italic ? TEXT_STYLE_BOLDITALIC : TEXT_STYLE_BOLD)
                            : (italic ? TEXT_STYLE_ITALIC : TEXT_STYLE_REGULAR));
        return text->GetAdvance(code);
    }
    bool page(const char *data, size_t length) override {
        if (length > 65535) return false;
        return book->AppendPage()->SetBuffer((u8*)data, length) == 0;
    }
};
}

int document_open_book(Book *book, const char *path)
{
    auto *text = book->GetApp()->ts;
    bool dsi = isDSiMode();
    document::Limits limits{dsi ? 8u*1024*1024 : 2u*1024*1024,
                            dsi ? 2u*1024*1024 : 384u*1024, dsi ? 4096u : 1024u};
    if (book->format == FORMAT_MARKDOWN) limits.input_bytes = dsi ? 1024*1024 : 256*1024;
    unsigned height = text->GetHeight();
    unsigned step = height * std::max(1, text->linespacing);
    int width = text->display.width - text->margin.left - text->margin.right;
    int remaining = text->display.height - text->margin.bottom - text->margin.top - height;
    if (!step || width <= 0 || remaining < 0) return document::LayoutError;
    BookOutput output(book);
    document::Layout layout(output, width, remaining / step + 1, limits);
    int status;
    if (book->format == FORMAT_PDF) status = document::read_pdf(path, layout, limits);
    else {
        FILE *file = fopen(path, "rb");
        if (!file) return document::ReadError;
        status = book->format == FORMAT_MARKDOWN ? document::read_markdown(file, layout, limits)
                                                : document::read_text(file, layout, limits);
        fclose(file);
    }
    int layout_status = layout.finish();
    text->SetStyle(TEXT_STYLE_REGULAR);
    if (status || layout_status) {
        book->Close();
        // A sink's own memory-limit failure is more useful than a parser abort.
        return layout_status == document::TooLarge ? layout_status : status ? status : layout_status;
    }
    return 0;
}
#endif
