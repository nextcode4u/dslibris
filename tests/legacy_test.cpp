// SPDX-License-Identifier: MIT
#include "document.h"
#include <cassert>
#include <iostream>
#include <string>
struct Capture : document::Sink {
    std::string value;
    bool bold = false, italic = false;
    bool text(const char *p, size_t n) override { value.append(p,n); return true; }
    bool newline() override { value += '\n'; return true; }
    bool style(bool b, bool i) override { bold |= b; italic |= i; return true; }
    bool preformatted(bool) override { return true; }
};
int main(int argc, char **argv)
{
    assert(argc == 2);
    std::string root = std::string(argv[1])+'/';
    document::Limits limits{1024*1024,512*1024,1024};
    auto read = [&](const char *name, Capture &out, document::Limits bounds) {
        std::string path = root+name;
        format_t format = document_format(name);
        if (format == FORMAT_PML_ZIP) return document::read_package(path.c_str(),format,out,bounds);
        FILE *file = fopen(path.c_str(),"rb"); assert(file);
        int result = format == FORMAT_PML ? document::read_pml(file,out,bounds) : document::read_tcr(file,out,bounds);
        fclose(file); return result;
    };
    for (const char *name : {"Book.pml", "Book.pmlz"}) {
        Capture out; int status = read(name,out,limits);
        if (status) std::cerr << name << ": " << status << '\n';
        assert(!status);
        for (const char *text : {"PML START", "bold and italic", "Café Ω 🌙", "link label", "Subheading", "Super", "Footnote text", "Sidebar text", "Escaped \\ end.", "PML END"})
            assert(out.value.find(text) != std::string::npos);
        assert(out.value.find("HIDDEN") == std::string::npos && out.value.find("ignored.png") == std::string::npos);
        assert(out.bold && out.italic);
    }
    Capture legacy; assert(!read("Legacy.pml",legacy,limits)); assert(legacy.value == "Café");
    Capture utf8; assert(!read("Utf8.pml",utf8,limits)); assert(utf8.value == "UTF8 Ω");
    Capture tcr; assert(!read("Book.tcr",tcr,limits)); assert(tcr.value == "TCR START\nCafé\nTCR END");
    Capture unicode; assert(!read("Utf8.tcr",unicode,limits)); assert(unicode.value == "Unicode Ω 🌙");
    for (const char *name : {"BadUnicode.pml", "Trailing.pml", "Comment.pml", "BadParameter.pml", "Broken.tcr", "Truncated.tcr"}) {
        Capture out; assert(read(name,out,limits) == document::Invalid);
    }
    Capture unknown; assert(read("Unknown.pml",unknown,limits) == document::Unsupported);
    Capture bounded;
    assert(read("Long.tcr",bounded,{1024*1024,128,1024}) == document::TooLarge);
    assert(read("Book.pml",bounded,{8,128,1024}) == document::TooLarge);
    std::cout << "PML/PMLZ style, Unicode, TCR dictionary and expansion limit tests passed\n";
}
