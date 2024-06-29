#include "ExecutablePath.h"

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
    #include <limits.h>
#elif defined(__linux__)
    #include <unistd.h>
    #include <limits.h>
#endif

std::string getAppPath() 
{
    #if defined(_WIN32) || defined(_WIN64)

    char path[PATH_MAX];
    HMODULE hModule = GetModuleHandle(NULL);
    GetModuleFileName(hModule, path, PATH_MAX);
    return std::string(path);

    #elif defined(__APPLE__)

    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) 
    {
        return std::string(path);
    } 
    else 
    {
            return std::string();
    }

    #elif defined(__linux__)

    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count != -1) 
    {
        return std::string(path);
    } 
    else 
    {
        return std::string();
    }

    #endif
}