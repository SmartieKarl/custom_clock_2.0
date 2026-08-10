#include <SD.h>
#include <cstring>

#include "utils.h"

// utils.h
// Helper functions not owned by a single class or file

// Attempts to find given file on mounted SD card
bool findFileOnSdRecursive(const char *basePath, const char *targetPath, bool &isFile)
{
    if (!basePath || !targetPath || basePath[0] == '\0' || targetPath[0] == '\0')
        return false;

    File dir = SD.open(basePath);
    if (!dir || !dir.isDirectory())
        return false;

    while (File entry = dir.openNextFile())
    {
        char childPath[256];
        std::snprintf(childPath, sizeof(childPath), "%s/%s", basePath, entry.name());

        if (entry.isDirectory())
        {
            if (findFileOnSdRecursive(childPath, targetPath, isFile))
            {
                entry.close();
                dir.close();
                return true;
            }
        }
        else if (std::strcmp(childPath, targetPath) == 0)
        {
            isFile = true;
            entry.close();
            dir.close();
            return true;
        }

        entry.close();
    }

    dir.close();
    return false;
}

// Validates the given path is valid and on the SD card
bool isValidSdPath(const char *path)
{
    if (path == nullptr || path[0] != '/' || path[1] == '\0')
        return false;

    bool isFile = false;
    return findFileOnSdRecursive("/", path, isFile) && isFile;
}