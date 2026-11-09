/* SPDX-License-Identifier: MIT */
#include "argv_path.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    assert(argv_path_select_root(NULL));
    assert(argv_path_select_root("relative.epub"));
    assert(!argv_path_select_root("unknown:/book.epub"));
    char directory[256], filename[256], tiny[3];
    char temporary[] = "/tmp/argv-launch-XXXXXX";
    char path[512];
    FILE *file;
    assert(argv_path_split("sd:/games/My File.MX2", directory, sizeof(directory), filename, sizeof(filename)));
    assert(!strcmp(directory, "sd:/games") && !strcmp(filename, "My File.MX2"));
    assert(argv_path_split("fat:/game.PRG", directory, sizeof(directory), filename, sizeof(filename)));
    assert(!strcmp(directory, "fat:/"));
    assert(argv_path_split("/game.epub", directory, sizeof(directory), filename, sizeof(filename)));
    assert(!strcmp(directory, "/"));
    assert(argv_path_split("relative.crt", directory, sizeof(directory), filename, sizeof(filename)));
    assert(!strcmp(directory, "."));
    assert(argv_path_extension(filename, ".CRT"));
    assert(!argv_path_split(NULL, directory, sizeof(directory), filename, sizeof(filename)));
    assert(!argv_path_split("", directory, sizeof(directory), filename, sizeof(filename)));
    assert(!argv_path_split("/directory/", directory, sizeof(directory), filename, sizeof(filename)));
    assert(!argv_path_split("/directory/..", directory, sizeof(directory), filename, sizeof(filename)));
    assert(!argv_path_split("/directory/game", tiny, sizeof(tiny), filename, sizeof(filename)));
    assert(!argv_path_split("game.epub", directory, sizeof(directory), tiny, sizeof(tiny)));
    assert(mkdtemp(temporary));
    snprintf(path, sizeof(path), "%s/Book With Spaces.EPUB", temporary);
    file = fopen(path, "wb"); assert(file); fputs("Synthetic file fixture", file); fclose(file);
    assert(argv_path_prepare("Book With Spaces.EPUB", temporary, directory, sizeof(directory), filename, sizeof(filename)));
    assert(!strcmp(directory, temporary) && !strcmp(filename, "Book With Spaces.EPUB"));
    assert(!argv_path_prepare("missing.epub", temporary, directory, sizeof(directory), filename, sizeof(filename)));
    assert(!argv_path_prepare(temporary, NULL, directory, sizeof(directory), filename, sizeof(filename)));
    unlink(path); rmdir(temporary);
    return 0;
}
