#pragma once
#include <string>
class App;
#include "document_format.h"
class Book {
    std::string filename, folder, title;
public:
    int format;
    explicit Book(App*) {}
    const char *GetFileName() { return filename.c_str(); }
    const char *GetFolderName() { return folder.c_str(); }
    void SetFileName(const char *name) { filename = name; }
    void SetFolderName(const std::string &name) { folder = name; }
    void SetTitle(const char *name) { title = name; }
};
