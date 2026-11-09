#pragma once
#include <vector>
#include <string>
#include "book.h"
class App {
public:
    std::string bookdir = "book";
    std::vector<Book*> books;
    unsigned bookcount = 0;
    ~App() { for (auto *book : books) delete book; }
};
