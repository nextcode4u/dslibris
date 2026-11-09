// SPDX-License-Identifier: MIT
#include "document.h"
#include <cassert>
#include <iostream>
#include <string>

struct Capture : document::Sink {
    std::string value;
    bool bold = false;
    bool text(const char *s, size_t n) override { value.append(s, n); return true; }
    bool newline() override { value += '\n'; return true; }
    bool style(bool b, bool) override { bold |= b; return true; }
    bool preformatted(bool) override { return true; }
};
int main(int argc, char **argv)
{
    assert(argc == 2);
    std::string root = std::string(argv[1]) + "/";
    document::Limits limits{1024*1024, 1024*1024, 4096};
    for (const char *name : {"Palm.pdb", "Compressed.prc", "Book.mobi", "Book.azw", "Book.azw3", "Legacy.mobi", "Legacy.azw3"}) {
        Capture out;
        int status = document::read_mobi((root+name).c_str(), out, limits);
        if (status) std::cerr << name << ": " << status << '\n';
        assert(!status);
        assert(out.value.find("HIDDEN") == std::string::npos);
        if (std::string(name).find("Book.") == 0) {
            assert(out.value.find("MOBI START") != std::string::npos);
            assert(out.value.find("Ω") != std::string::npos && out.bold);
        } else assert(out.value.find("Café") != std::string::npos);
    }
    Capture palm;
    assert(!document::read_mobi((root+"Palm.pdb").c_str(), palm, limits));
    assert(palm.value == "PALMDOC START\nCafé text.\nPALMDOC END");
    Capture legacy;
    assert(!document::read_mobi((root+"Legacy.mobi").c_str(), legacy, limits));
    assert(legacy.value.find("“quotes”") != std::string::npos);
    for (const char *name : {"Broken.mobi", "BadOffset.mobi"}) {
        Capture out;
        assert(document::read_mobi((root+name).c_str(), out, limits) == document::Invalid);
    }
    Capture encrypted;
    assert(document::read_mobi((root+"Encrypted.azw").c_str(), encrypted, limits) == document::Encrypted);
    Capture tiny;
    assert(document::read_mobi((root+"Book.mobi").c_str(), tiny, {16,1024,10}) == document::TooLarge);
    assert(document::read_mobi((root+"Book.mobi").c_str(), tiny, {1024*1024,16,10}) == document::TooLarge);
    for (const char *ext : {"mobi", "PRC", "PDB", "AZW", "AZW3"})
        assert(document_format((std::string("Book.")+ext).c_str()) == FORMAT_MOBI);
    std::cout << "PalmDOC/MOBI text, compression, encoding and rejection tests passed\n";
}
