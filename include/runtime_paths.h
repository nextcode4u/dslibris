// SPDX-License-Identifier: MIT
#pragma once
#include <sys/stat.h>
#include <cstring>

inline const char *reader_preferences_path();

inline bool reader_directory(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}
inline const char *reader_font_directory()
{
#ifdef DSL_DOCUMENTS
    struct stat legacy;
    if (!strcmp(reader_preferences_path(), "dslibris.xml") && reader_directory("font")
        && stat("dslibris.xml", &legacy) == 0 && S_ISREG(legacy.st_mode)) return "font";
    if (reader_directory("data/dslibris/font")) return "data/dslibris/font";
#endif
    return "font";
}
inline const char *reader_book_directory()
{
#ifdef DSL_DOCUMENTS
    if (reader_directory("media/books")) return "media/books";
#endif
    return "book";
}
inline const char *reader_preferences_path()
{
#ifdef DSL_DOCUMENTS
    struct stat info;
    if (stat("data/dslibris/dslibris.xml", &info) == 0 && S_ISREG(info.st_mode))
        return "data/dslibris/dslibris.xml";
    // Keep an existing installation's reading positions/settings in use.
    if (stat("dslibris.xml", &info) == 0 && S_ISREG(info.st_mode)) return "dslibris.xml";
    if (reader_directory("data/dslibris")) return "data/dslibris/dslibris.xml";
#endif
    return "dslibris.xml";
}
