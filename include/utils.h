#pragma once

// utils.h
// Helper functions not owned by a single class or file

bool findFileOnSdRecursive(const char *basePath, const char *targetPath, bool &isFile);
bool isValidSdPath(const char *path);