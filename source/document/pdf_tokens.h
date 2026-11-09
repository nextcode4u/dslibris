// SPDX-License-Identifier: MIT
#pragma once
#include <pdfio.h>
#include <string>
#include <stddef.h>

namespace document {
struct PdfToken {
    enum Kind { Word, Name, String, Punctuation } kind = Word;
    std::string value;
};

// The public PDFio token API returns C strings. This lexer retains embedded NULs
// in CID-font strings, decodes PDF escapes and bounds decompressed content.
class PdfTokens {
public:
    PdfTokens(const std::string &bytes, size_t &remaining) : bytes(bytes), remaining(remaining) {}
    bool next(PdfToken &token);
    bool failed = false;
private:
    const std::string &bytes;
    size_t &remaining;
    size_t position = 0;
    int lookahead = -2;
    int get();
    int peek();
    bool add(std::string &value, int c);
};
}
