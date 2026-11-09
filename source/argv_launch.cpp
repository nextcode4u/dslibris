// SPDX-License-Identifier: MIT
// Isolated launcher integration; existing Book indexing/opening handles EPUB data.
#include "argv_launch.h"
#include "argv_path.h"
#include "document_format.h"
#include "app.h"
#include "book.h"

std::string argv_prepare_book(App *app, const char *path)
{
    char directory[512], filename[256];
    if (!argv_path_prepare(path, NULL, directory, sizeof(directory), filename, sizeof(filename))
        || document_format(filename) == FORMAT_UNDEF)
        return std::string();
    app->bookdir = directory;
    return filename;
}

Book *argv_select_book(App *app, const std::string &filename)
{
    if (filename.empty())
        return NULL;
    for (auto *book : app->books)
        if (!strcasecmp(book->GetFileName(), filename.c_str()))
            return book;
    // Keep explicit selections available even if the scanner skipped the file.
    Book *book = new Book(app);
    book->SetFolderName(app->bookdir);
    book->SetFileName(filename.c_str());
    book->SetTitle(filename.c_str());
    book->format = document_format(filename.c_str());
    app->books.push_back(book);
    ++app->bookcount;
    return book;
}
