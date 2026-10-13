// Shared by the libcrtweb tests: remove a directory tree without a shell. The tests used
// system("rm -rf ...") until Windows, where a CRT program's system() runs the CRT's mksh with the
// host's PATH (drive-letter entries, ';'-separated) and finds no `rm`: the tests must not depend on
// the surrounding command environment, only on the C library.
#pragma once

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

// Returns true when `path` no longer exists (it need not have existed).
static inline bool crtwebRemoveTree(const std::string& path)
{
    struct stat info;
    if (lstat(path.c_str(), &info) != 0)
        return errno == ENOENT;
    if (!S_ISDIR(info.st_mode))
        return unlink(path.c_str()) == 0;
    if (DIR* directory = opendir(path.c_str())) {
        while (dirent* entry = readdir(directory)) {
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
                continue;
            crtwebRemoveTree(path + "/" + entry->d_name);
        }
        closedir(directory);
    }
    return rmdir(path.c_str()) == 0;
}
