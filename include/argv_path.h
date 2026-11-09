/* SPDX-License-Identifier: MIT
 * Original argv path handling for nextcode4u's fork. See ARGUMENT_LAUNCH_LICENSE.md.
 */
#ifndef ARGV_PATH_H
#define ARGV_PATH_H
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

/* Keep device roots (sd:/, fat:/) intact and never modify the caller's argv. */
static inline int argv_path_split(const char *path, char *directory, size_t dir_size,
                                  char *filename, size_t name_size)
{
    const char *slash;
    const char *name;
    size_t parent_length;
    if (!path || !*path || !directory || !filename || !dir_size || !name_size)
        return 0;
    slash = strrchr(path, '/');
    name = slash ? slash + 1 : path;
    if (!*name || !strcmp(name, ".") || !strcmp(name, "..") || strlen(name) >= name_size)
        return 0;
    parent_length = slash ? (size_t)(slash - path) : 1;
    if (slash && (slash == path || slash[-1] == ':'))
        ++parent_length;
    if (parent_length >= dir_size)
        return 0;
    if (slash)
        memcpy(directory, path, parent_length);
    else
        directory[0] = '.';
    directory[parent_length] = '\0';
    memcpy(filename, name, strlen(name) + 1);
    return 1;
}

static inline int argv_path_extension(const char *filename, const char *extension)
{
    const char *dot = filename ? strrchr(filename, '.') : NULL;
    return dot && !strcasecmp(dot, extension);
}

/* Relative paths are resolved against the mount-time cwd, before browser chdir. */
static inline int argv_path_prepare(const char *argument, const char *base,
                                    char *directory, size_t dir_size,
                                    char *filename, size_t name_size)
{
    char full[1024];
    struct stat info;
    int length;
    if (!argument || !*argument)
        return 0;
    if (argument[0] == '/' || strstr(argument, ":/") || !base || !*base)
        length = snprintf(full, sizeof(full), "%s", argument);
    else
        length = snprintf(full, sizeof(full), "%s/%s", base, argument);
    if (length < 0 || (size_t)length >= sizeof(full) || stat(full, &info) != 0
        || !S_ISREG(info.st_mode) || info.st_size <= 0)
        return 0;
    return argv_path_split(full, directory, dir_size, filename, name_size);
}
/* Select an explicitly requested card before relative config/BIOS lookups. */
static inline int argv_path_select_root(const char *argument)
{
    const char *root = NULL;
    struct stat info;
    if (!argument || !*argument)
        return 1;
    if (!strncmp(argument, "sd:/", 4)) root = "sd:/";
    else if (!strncmp(argument, "fat:/", 5)) root = "fat:/";
    else return strchr(argument, ':') == NULL;
    return stat(root, &info) == 0 && S_ISDIR(info.st_mode) && chdir(root) == 0;
}
#endif
