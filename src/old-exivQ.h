#pragma once
#define NOMINMAX

#include <string>
#include <fstream>
#include <oaidl.h>   // for IDispatch
#include <comdef.h>
#include <atlbase.h>
#include <atlcom.h>

#ifdef _WIN32
#define SHIM_API __declspec(dllexport)
#else
#define SHIM_API
#endif

extern "C" {
    SHIM_API const wchar_t* Exiv2VersionW();
    SHIM_API int ReadXmpFile(const char* path, const char** jsonPtr, size_t* jsonSize);
    SHIM_API void FreeBuffer(char* ptr);
    SHIM_API HRESULT __stdcall ReadXmpToDispatch(const char* path, IDispatch* pDict);
    SHIM_API HRESULT __stdcall AddStaticToMap(IDispatch* pMap);
}

