// SPDX-License-Identifier: MIT
#include "runtime_paths.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <unistd.h>
int main()
{
    namespace fs = std::filesystem;
    char temporary[] = "/tmp/dslibris-paths-XXXXXX";
    assert(mkdtemp(temporary));
    auto previous = fs::current_path();
    fs::current_path(temporary);
    assert(!strcmp(reader_font_directory(),"font"));
    assert(!strcmp(reader_book_directory(),"book"));
    assert(!strcmp(reader_preferences_path(),"dslibris.xml"));
    fs::create_directories("data/dslibris/font");
    fs::create_directories("media/books");
    assert(!strcmp(reader_font_directory(),"data/dslibris/font"));
    assert(!strcmp(reader_book_directory(),"media/books"));
    assert(!strcmp(reader_preferences_path(),"data/dslibris/dslibris.xml"));
    fs::create_directories("font");
    std::ofstream("dslibris.xml") << "Existing user settings";
    assert(!strcmp(reader_preferences_path(),"dslibris.xml"));
    assert(!strcmp(reader_font_directory(),"font"));
    std::ofstream("data/dslibris/dslibris.xml") << "Managed user settings";
    assert(!strcmp(reader_preferences_path(),"data/dslibris/dslibris.xml"));
    assert(!strcmp(reader_font_directory(),"data/dslibris/font"));
    fs::current_path(previous);
    fs::remove_all(temporary);
}
