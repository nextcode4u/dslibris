// SPDX-License-Identifier: MIT
#pragma once
#include <string>
class App;
class Book;
std::string argv_prepare_book(App *app, const char *path);
Book *argv_select_book(App *app, const std::string &filename);
