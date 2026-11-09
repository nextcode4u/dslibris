// SPDX-License-Identifier: MIT
// Isolated launcher integration; existing Book indexing/opening handles EPUB data.
#include "argv_launch.h"
#include "argv_path.h"
#include "app.h"
#include "book.h"

std::string argv_prepare_book(App *app, const char *path)
{
    char directory[512], filename[256];
    if (!argv_path_prepare(path, NULL, directory, sizeof(directory), filename, sizeof(filename))
        || !argv_path_extension(filename, ".epub"))
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
    // The normal scanner only matches lowercase .epub. Explicit .EPUB works too.
    Book *book = new Book(app);
    book->SetFolderName(app->bookdir);
    book->SetFileName(filename.c_str());
    book->SetTitle(filename.c_str());
    book->format = FORMAT_EPUB;
    app->books.push_back(book);
    ++app->bookcount;
    return book;
}
