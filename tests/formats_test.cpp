// SPDX-License-Identifier: MIT
#include "document.h"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <cctype>

struct Capture : document::Sink {
    std::string value;
    int bold = 0, italic = 0;
    bool text(const char *s, size_t n) override { value.append(s, n); return true; }
    bool newline() override { value += '\n'; return true; }
    bool style(bool b, bool i) override { bold += b; italic += i; return true; }
    bool preformatted(bool) override { return true; }
};
static std::string words(const std::string &s)
{
    std::string out;
    bool space = false;
    for (unsigned char c : s) {
        if (isspace(c)) { space = true; continue; }
        if (space && !out.empty()) out += ' ';
        space = false;
        out += (char)c;
    }
    return out;
}
static int read(const std::string &path, document::Sink &sink, document::Limits limits)
{
    format_t format = document_format(path.c_str());
    assert(format != FORMAT_UNDEF);
    if (format == FORMAT_FB2 || format == FORMAT_FB2_ZIP || format == FORMAT_DOCX || format == FORMAT_ODT)
        return document::read_xml_book(path.c_str(), format, sink, limits);
    FILE *f = fopen(path.c_str(), "rb"); assert(f);
    int status = format == FORMAT_RTF ? document::read_rtf(f, sink, limits) : document::read_html(f, sink, limits);
    fclose(f);
    return status;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const std::string root = std::string(argv[1]) + "/";
    document::Limits limits{1024*1024, 512*1024, 4096};
    for (const char *name : {"Page.html", "Page.xhtml"}) {
        Capture out; int result = read(root+name, out, limits);
        if (result) std::cerr << name << " error=" << result << '\n';
        assert(result == 0);
        assert(out.value.find("HIDDEN") == std::string::npos);
        assert(words(out.value).find("Hello bold italic © Ω …." ) != std::string::npos);
        assert(out.value.find("  code\n  next") != std::string::npos);
        assert(out.value.find("picture label") != std::string::npos);
        assert(out.value.find("<escaped>&ready Ω") != std::string::npos);
        assert(out.bold && out.italic);
    }
    Capture loose; assert(!read(root+"Page.htm", loose, limits));
    assert(words(loose.value) == "First Second Third & &notAnEntity;");
    Capture comment; assert(!read(root+"Comment.html", comment, limits));
    assert(words(comment.value) == "After comment");
    for (const char *name : {"Legacy.html", "Legacy.fb2"}) {
        Capture out; assert(!read(root+name, out, limits));
        assert(out.value.find("Привет") != std::string::npos);
    }
    Capture utf16; assert(!read(root+"Utf16.html", utf16, limits));
    assert(utf16.value.find("UTF16 Ω") != std::string::npos);
    for (const char *name : {"Book.fb2", "Book.fb2.zip", "Book.fbz"}) {
        Capture out; assert(!read(root+name, out, limits));
        assert(out.value.find("HIDDEN") == std::string::npos);
        assert(words(out.value).find("Hello bold italic Ω.") != std::string::npos);
        assert(out.value.find("Verse one\nVerse two") != std::string::npos);
        assert(out.bold && out.italic);
    }
    Capture docx; assert(!read(root+"Book.docx", docx, limits));
    assert(words(docx.value).find("DOCX START bold end.") != std::string::npos);
    assert(docx.value.find("HIDDEN") == std::string::npos && docx.bold);
    Capture odt; assert(!read(root+"Book.odt", odt, limits));
    assert(words(odt.value).find("Hello world Ω.") != std::string::npos);
    assert(odt.value.find("   Indented") != std::string::npos);
    Capture rtf; int rtf_result = read(root+"Book.rtf", rtf, limits);
    if (rtf_result) std::cerr << "RTF error " << rtf_result << " text " << rtf.value << '\n';
    assert(rtf_result == 0);
    std::string text = words(rtf.value);
    assert(text.find("HIDDEN") == std::string::npos);
    assert(text.find("Plain bold both normal.") != std::string::npos && rtf.bold && rtf.italic);
    assert(text.find("Café Ω 🌙") != std::string::npos);
    assert(text.find("link label") != std::string::npos);
    assert(text.find("Escaped \\ { }") != std::string::npos);
    assert(text.find("Привет é") != std::string::npos);
    for (const char *name : {"Broken.rtf", "BadHex.rtf", "BadBin.rtf", "Plain.rtf", "Wrong.fb2", "Ambiguous.fbz", "BadCRC.fbz", "Missing.docx"}) {
        Capture out; assert(read(root+name, out, limits) == document::Invalid);
    }
    for (const char *name : {"Entity.fb2", "BadEncoding.rtf", "Unknown.html"}) {
        Capture out; assert(read(root+name, out, limits) == document::Unsupported);
    }
    Capture encrypted;
    assert(read(root+"Encrypted.fbz", encrypted, limits) == document::Encrypted);
    Capture deep; assert(read(root+"Deep.html", deep, limits) == document::TooLarge);
    for (const char *name : {"Page.html", "Book.fb2", "Book.fbz", "Book.docx", "Book.odt", "Book.rtf"}) {
        Capture out; assert(read(root+name, out, {16,100,10}) == document::TooLarge);
    }
    assert(document_format("FILE.FB2.ZIP") == FORMAT_FB2_ZIP);
    assert(document_format("FILE.ZIP") == FORMAT_UNDEF && document_format("FILE.DOC") == FORMAT_UNDEF);
    std::cout << "HTML/XHTML, FB2/ZIP, RTF, DOCX and ODT tests passed\n";
}
