#pragma once
#include <string>

#ifdef _WIN32
    #define SHIM_API extern "C" __declspec(dllexport)
#else
    #define SHIM_API extern "C"
#endif


//extern "C"
SHIM_API const wchar_t* Exiv2VersionW();
// SHIM_API int ReadXmpFile(const wchar_t* path, wchar_t** outBuffer);
SHIM_API int ReadXmpFile(const wchar_t* path, wchar_t** outBuffer);

// Free a buffer allocated by ReadXmpFile
SHIM_API void FreeXmpFile(wchar_t* buffer);
SHIM_API int ImportJsonToXmp(const wchar_t* path, const wchar_t* jsonStr);
SHIM_API int ExportXmpSingleKey(const wchar_t* path, wchar_t** outBuffer);
SHIM_API const char* __cdecl ReadXmpProperties(const char* filePath);