// SPDX-License-Identifier: MIT
#include "app.h"
#include "argv_launch.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
int main() {
    char root[] = "/tmp/dslibris-argv-XXXXXX", cwd[1024], after[1024], path[1024];
    assert(getcwd(cwd, sizeof(cwd)) && mkdtemp(root));
    snprintf(path, sizeof(path), "%s/External Book.EPUB", root);
    FILE *file = fopen(path, "wb"); assert(file); fputs("Synthetic fixture", file); fclose(file);
    App app;
    std::string name = argv_prepare_book(&app, path);
    assert(name == "External Book.EPUB" && app.bookdir == root);
    Book *selected = argv_select_book(&app, name);
    assert(selected && app.bookcount == 1 && app.books.size() == 1);
    assert(!strcmp(selected->GetFolderName(), root));
    assert(argv_select_book(&app, name) == selected && app.books.size() == 1);
    assert(getcwd(after, sizeof(after)) && !strcmp(after, cwd));
    assert(argv_prepare_book(&app, nullptr).empty());
    assert(argv_prepare_book(&app, "/does-not-exist.epub").empty());
    assert(app.bookdir == root);
    unlink(path);
    const char *extra[] = {"Notes.TXT", "Book.md", "Book.Markdown", "Paper.PDF", "Page.HTML", "Page.xhtml", "Book.FB2", "Book.FB2.ZIP",
                           "Book.fbz", "Book.RTF", "Book.DOCX", "Book.ODT"};
    for (const char *filename : extra) {
        snprintf(path, sizeof(path), "%s/%s", root, filename);
        file = fopen(path, "wb"); assert(file); fputs("Synthetic fixture", file); fclose(file);
        name = argv_prepare_book(&app, path);
        assert(name == filename);
        selected = argv_select_book(&app, name);
        assert(selected && selected->format == document_format(filename));
        assert(argv_select_book(&app, name) == selected);
        unlink(path);
    }
    assert(app.books.size() == 1 + sizeof(extra)/sizeof(extra[0]));
    assert(document_format("file.doc") == FORMAT_UNDEF);
    rmdir(root);
}
