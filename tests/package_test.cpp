// SPDX-License-Identifier: MIT
#include "document.h"
#include <cassert>
#include <iostream>
#include <string>
struct Capture : document::Sink {
    std::string value;
    bool bold = false;
    bool text(const char *p, size_t n) override { value.append(p,n); return true; }
    bool newline() override { value += '\n'; return true; }
    bool style(bool b, bool) override { bold |= b; return true; }
    bool preformatted(bool) override { return true; }
};
int main(int argc, char **argv)
{
    assert(argc == 2);
    const std::string root = std::string(argv[1])+'/';
    document::Limits limits{1024*1024, 512*1024, 1024};
    auto read = [&](const char *name, Capture &out, document::Limits bounds) {
        std::string path = root+name;
        int status = document::read_package(path.c_str(), document_format(name), out, bounds);
        return status;
    };
    for (const char *name : {"Book.opf", "Book.oeb", "Book.oebzip", "DTD.opf"}) {
        Capture out;
        int status = read(name,out,limits);
        if (status) std::cerr << name << ": " << status << '\n';
        assert(!status);
        size_t first = out.value.find("FIRST CHAPTER"), second = out.value.find("SECOND CHAPTER"), third = out.value.find("THIRD CHAPTER");
        assert(first < second && second < third && third != std::string::npos);
        assert(out.bold && out.value.find("Café") != std::string::npos && out.value.find("Ω") != std::string::npos);
        assert(out.value.find("HIDDEN") == std::string::npos && out.value.find("METADATA") == std::string::npos);
    }
    Capture html; assert(!read("Book.htmlz",html,limits));
    assert(html.value.find("HTMLZ START") != std::string::npos && html.value.find("© Ω") != std::string::npos);
    Capture legacy; assert(!read("Legacy.htmlz",legacy,limits)); assert(legacy.value.find("Café") != std::string::npos);
    Capture text; assert(!read("Book.txtz",text,limits)); assert(text.value == "TXTZ START\nCafé Ω.\nTXTZ END");
    Capture markdown; assert(!read("Markdown.txtz",markdown,limits));
    assert(markdown.bold && markdown.value.find("MARKDOWN ZIP") != std::string::npos);
    Capture utf16; assert(!read("Utf16.txtz",utf16,limits)); assert(utf16.value == "UTF16 Ω");
    for (const char *name : {"Ambiguous.htmlz", "Ambiguous.txtz", "Multiple.oebzip", "Duplicate.opf", "MissingRef.opf", "WrongRoot.opf", "BadCRC.txtz"}) {
        Capture out; assert(read(name,out,limits) == document::Invalid);
    }
    for (const char *name : {"Remote.opf", "Escape.opf", "EncodedEscape.opf", "Absolute.opf", "Backslash.opf", "Query.opf", "WrongMedia.opf", "Entity.opf", "Base.opf"}) {
        Capture out; assert(read(name,out,limits) == document::Unsupported);
    }
    Capture empty; assert(read("Empty.htmlz",empty,limits) == document::NoText);
    assert(read("Metadata.opf",empty,limits) == document::NoText);
    Capture missing; assert(read("MissingFile.opf",missing,limits) == document::ReadError);
    Capture encrypted; assert(read("Encrypted.txtz",encrypted,limits) == document::Encrypted);
    Capture bounded; assert(read("Book.opf",bounded,{64,512,100}) == document::TooLarge);
    assert(read("Book.opf",bounded,{1024*1024,8,100}) == document::TooLarge);
    assert(read("LargeMarkdown.txtz",bounded,{2*1024*1024,384*1024,1024}) == document::TooLarge);
    assert(read("BigDirectory.htmlz",bounded,limits) == document::TooLarge);
    assert(document_format("Book.KEPUB") == FORMAT_EPUB);
    assert(document_format("Book.kepub.epub") == FORMAT_EPUB);
    assert(document_format("Notes.TEXT") == FORMAT_TEXT);
    assert(document_format("Book.zip") == FORMAT_UNDEF);
    std::cout << "OPF/OEB reading order, HTMLZ/TXTZ, encoding and bounded container tests passed\n";
}
