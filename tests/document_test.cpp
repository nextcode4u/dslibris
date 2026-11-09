// SPDX-License-Identifier: MIT
#include "document.h"
#include "utf8.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

struct Capture : document::Sink {
    std::string value;
    bool text(const char *p, size_t n) override { value.append(p, n); return true; }
    bool newline() override { value += '\n'; return true; }
    bool style(bool, bool) override { return true; }
    bool preformatted(bool) override { return true; }
};
struct Pages : document::Output {
    std::vector<std::string> pages;
    unsigned advance(uint32_t, bool, bool) override { return 1; }
    bool page(const char *p, size_t n) override { pages.emplace_back(p, n); return true; }
};
static FILE *file(const std::string &text) {
    FILE *f = tmpfile(); assert(f);
    assert(fwrite(text.data(), 1, text.size(), f) == text.size()); rewind(f); return f;
}
static std::string joined(Pages &pages) {
    std::string result;
    for (auto &p : pages.pages) result += p;
    return result;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    document::Limits limits{1024*1024, 512*1024, 4096};
    Pages plain;
    document::Layout text(plain, 60, 4, limits);
    FILE *f = file("\xef\xbb\xbf" "Caf\xc3\xa9\r\n\r\n\tLong line\rLast line\n");
    assert(document::read_text(f, text, limits) == 0); fclose(f);
    assert(text.finish() == 0);
    assert(joined(plain) == "Caf\xc3\xa9\n\n    Long line\nLast line\n");
    Capture boundary;
    f = file(std::string(511, 'x') + "\xe2\x82\xac\r\nend");
    assert(!document::read_text(f, boundary, limits)); fclose(f);
    assert(boundary.value == std::string(511, 'x') + "\xe2\x82\xac\nend");
    Pages controls; document::Layout safe(controls, 60, 4, limits);
    assert(safe.text("safe\2text\0", 10)); assert(!safe.finish());
    assert(joined(controls) == "safetext");
    for (bool big_endian : {false, true}) {
        std::u16string units = u"\ufeffAé\n🌙";
        std::string encoded;
        for (char16_t unit : units) {
            encoded += (char)(big_endian ? unit >> 8 : unit & 255);
            encoded += (char)(big_endian ? unit & 255 : unit >> 8);
        }
        Capture decoded;
        f = file(encoded);
        assert(!document::read_text(f, decoded, limits)); fclose(f);
        assert(decoded.value == "Aé\n🌙");
    }
    Capture malformed;
    f = file(std::string("\xff\xfe" "A", 3));
    assert(document::read_text(f, malformed, limits) == document::Invalid); fclose(f);
    Pages exact;
    document::Layout exact_style(exact, 2, 1, limits);
    assert(exact_style.style(true, true));
    assert(exact_style.text("ab\ncd\n", 6));
    assert(exact_style.style(false, false));
    assert(!exact_style.finish() && exact.pages.size() == 1);
    Pages long_word;
    document::Layout wrapped(long_word, 10, 2, limits);
    std::string word(10000, 'a'); assert(wrapped.text(word.data(), word.size()));
    assert(wrapped.finish() == 0 && long_word.pages.size() > 100);
    size_t letters = 0;
    for (char c : joined(long_word)) if (c == 'a') ++letters;
    assert(letters == word.size());
    Pages style;
    document::Layout styled(style, 5, 1, limits);
    assert(styled.style(true, true)); assert(styled.text("abcdefghijklmno", 15));
    assert(styled.finish() == 0 && style.pages.size() == 2);
    assert(style.pages[1].find("\2\4") == 0); // Carry styles across page boundaries.
    Pages bounded;
    document::Layout small(bounded, 10, 2, {100, 4, 10});
    small.text("too much text", 13); assert(small.finish() == document::TooLarge);
    Pages empty;
    document::Layout blank(empty, 10, 2, limits);
    assert(blank.finish() == document::NoText);
    Pages md_pages;
    document::Layout markdown(md_pages, 80, 12, limits);
    f = file("# Heading\n\nA **bold** and *italic* [link](https://example.com) &amp; &#x3a9;.\n\n1. First\n2. Second\n\n```\n  code\n```\n");
    assert(document::read_markdown(f, markdown, limits) == 0); fclose(f);
    assert(markdown.finish() == 0);
    std::string md = joined(md_pages);
    assert(md.find("\2Heading\3") != std::string::npos);
    assert(md.find("bold\3") != std::string::npos && md.find("italic\5") != std::string::npos);
    assert(md.find("link & \xce\xa9.") != std::string::npos);
    assert(md.find("1. First") != std::string::npos && md.find("2. Second") != std::string::npos);
    assert(md.find("  code\n") != std::string::npos);
    f = file(std::string(1025, 'x')); Capture ignored;
    assert(document::read_markdown(f, ignored, {1024, 1024, 10}) == document::TooLarge); fclose(f);
    for (const char *name : {"simple.pdf", "compressed.pdf", "inherited.pdf"}) {
        Capture extracted; std::string path = std::string(argv[1]) + "/" + name;
        int result = document::read_pdf(path.c_str(), extracted, limits);
        if (result) std::cerr << name << " error " << result << " text " << extracted.value << "\n";
        assert(result == 0);
        assert(extracted.value.find("Hello (reader) \xe2\x82\xac") != std::string::npos);
        assert(extracted.value.find("spaced words") != std::string::npos);
    }
    for (const char *name : {"unicode.pdf", "unicode-literal.pdf"}) {
        Capture extracted; std::string path = std::string(argv[1]) + "/" + name;
        int result = document::read_pdf(path.c_str(), extracted, limits);
        if (result) std::cerr << name << " error " << result << " text " << extracted.value << "\n";
        assert(result == 0);
        assert(extracted.value.find("\xce\xa9" "AB\xc3\xa9" "fi") != std::string::npos);
    }
    Capture split;
    assert(!document::read_pdf((std::string(argv[1])+"/split-stream.pdf").c_str(), split, limits));
    assert(split.value.find("Split streams") != std::string::npos);
    assert(document::read_pdf((std::string(argv[1])+"/oversized-stream.pdf").c_str(), ignored, limits) == document::TooLarge);
    assert(document::read_pdf((std::string(argv[1])+"/inline-image.pdf").c_str(), ignored, limits) == document::Unsupported);
    Capture form; assert(!document::read_pdf((std::string(argv[1])+"/form.pdf").c_str(), form, limits));
    assert(form.value.find("Form text") != std::string::npos);
    Capture differences; assert(!document::read_pdf((std::string(argv[1])+"/differences.pdf").c_str(), differences, limits));
    assert(differences.value.find("\xc3\xa9") != std::string::npos);
    Capture outside; assert(!document::read_pdf((std::string(argv[1])+"/binary-outside-text.pdf").c_str(), outside, limits));
    assert(outside.value.find("not text") == std::string::npos && outside.value.find("Visible") != std::string::npos);
    assert(document::read_pdf((std::string(argv[1])+"/bad-encoding.pdf").c_str(), ignored, limits) == document::Unsupported);
    assert(document::read_pdf((std::string(argv[1])+"/invalid.pdf").c_str(), ignored, limits) == document::Invalid);
    assert(document::read_pdf((std::string(argv[1])+"/broken-string.pdf").c_str(), ignored, limits) == document::Invalid);
    assert(document::read_pdf("/no-such-book.pdf", ignored, limits) == document::ReadError);
    assert(document::read_pdf((std::string(argv[1])+"/simple.pdf").c_str(), ignored, {4, 100, 10}) == document::TooLarge);
    Pages image_only; document::Layout scanned(image_only, 10, 2, limits);
    assert(!document::read_pdf((std::string(argv[1])+"/empty.pdf").c_str(), scanned, limits));
    assert(scanned.finish() == document::NoText);
    std::cout << "TXT, Markdown, PDF and layout tests passed\n";
}
