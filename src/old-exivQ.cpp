#include "exivQ.h"
#include <exiv2/exiv2.hpp>
#include <string>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/encodings.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <windows.h>
#include <comdef.h>
#include <nlohmann/json.hpp>   // For JSON representation
#include <string>
#include <vector>

extern "C" {

SHIM_API const wchar_t* Exiv2VersionW() {
    static std::wstring versionW;
    std::string version = Exiv2::version(); // returns std::string
    versionW.assign(version.begin(), version.end());
    return versionW.c_str();
}
int ReadXmpFile(const char* path, const char** jsonPtr, size_t* jsonSize)
{
    if (!jsonPtr || !jsonSize) return -1;
    *jsonPtr = nullptr;
    *jsonSize = 0;

    // // 1. Read XMP file into a local string
    // std::ifstream file(path, std::ios::in | std::ios::binary);
    // if (!file.is_open()) return -2;

    // std::ostringstream ss;
    // ss << file.rdbuf();
    // std::string xmpStr = ss.str();

    // 1. Open XMP file
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return -2;

    // 2. Determine file size
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // 3. Read into mutable string buffer
    std::string xmpStr(size, '\0');
    if (!file.read(&xmpStr[0], size)) {
        return -3; // read error
    }

    // // xmpStr now contains the full XMP contents

    // 2. Parse XMP
    Exiv2::XmpData xmp;
    try {
        Exiv2::XmpParser::decode(xmp, xmpStr);
    } catch (...) {
        return -3;
    }

    // 3. Convert to near-flat JSON
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    for (auto& kv : xmp) {
        std::string key = kv.key();
        std::string val = kv.toString();
        doc.AddMember(rapidjson::Value(key.c_str(), alloc),
                      rapidjson::Value(val.c_str(), alloc),
                      alloc);
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    // 4. Allocate heap buffer and set outputs
    char* ret = new char[buffer.GetSize()];
    memcpy(ret, buffer.GetString(), buffer.GetSize());
    *jsonPtr = ret;
    *jsonSize = buffer.GetSize();

    return 0;
}
void FreeBuffer(char* ptr)
{
    delete[] ptr;
}

// HRESULT __stdcall ReadXmpToDispatch(const char* path, IDispatch* pDict)
// {
//     if (!path || !pDict)
//         return E_INVALIDARG;

//     HRESULT hr = S_OK;

//     try {
//         // Read file into memory
//         std::ifstream file(path, std::ios::in | std::ios::binary);
//         if (!file.is_open())
//             return E_FAIL;

//         std::ostringstream ss;
//         ss << file.rdbuf();
//         std::string xmpString = ss.str();

//         // Parse XMP from memory
//         Exiv2::XmpData xmpData;
//         Exiv2::XmpParser::decode(xmpData, xmpString);

//         // Get DISPID for .Add method
//         OLECHAR* methodName = L"Add";
//         DISPID dispidAdd;
//         hr = pDict->GetIDsOfNames(IID_NULL, &methodName, 1, LOCALE_USER_DEFAULT, &dispidAdd);
//         if (FAILED(hr))
//             return hr;

//         // Fill dictionary
//         for (const auto& kv : xmpData) {
//             std::string key = kv.key();
//             std::string val = kv.toString();

//             VARIANTARG args[2];
//             VariantInit(&args[0]);
//             VariantInit(&args[1]);

//             args[0].vt = VT_BSTR;
//             args[0].bstrVal = _com_util::ConvertStringToBSTR(val.c_str());
//             args[1].vt = VT_BSTR;
//             args[1].bstrVal = _com_util::ConvertStringToBSTR(key.c_str());

//             DISPPARAMS params = { args, nullptr, 2, 0 };
//             hr = pDict->Invoke(dispidAdd, IID_NULL, LOCALE_USER_DEFAULT,
//                                DISPATCH_METHOD, &params, nullptr, nullptr, nullptr);

//             VariantClear(&args[0]);
//             VariantClear(&args[1]);
//             if (FAILED(hr))
//                 break;
//         }
//     }
//     catch (...) {
//         hr = E_FAIL;
//     }

//     return hr;
// }


SHIM_API HRESULT __stdcall ReadXmpToDispatch(const char* path, IDispatch* pDict)
{
    if (!pDict) return E_POINTER;

    try {
        std::unique_ptr<Exiv2::Image> image = Exiv2::ImageFactory::open(path);
        if (!image) return E_FAIL;

        image->readMetadata();
        Exiv2::XmpData& xmpData = image->xmpData();

        if (xmpData.empty()) return S_OK;

        for (auto& kv : xmpData) {
            std::string key = kv.key();
            std::string value = kv.toString(); // single-value representation

            // TODO: handle arrays manually (Bags / Seqs) if needed

            CComBSTR bstrKey(key.c_str());
            CComVariant varVal(value.c_str());

            DISPID dispid;
            HRESULT hr = pDict->GetIDsOfNames(IID_NULL, reinterpret_cast<LPOLESTR*>(&bstrKey), 1, LOCALE_USER_DEFAULT, &dispid);
            if (FAILED(hr)) continue;

            DISPPARAMS dp = { &varVal, nullptr, 1, 0 };
            hr = pDict->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT, &dp, nullptr, nullptr, nullptr);
            if (FAILED(hr)) continue;
        }

        return S_OK;
    }
    catch (const Exiv2::Error& e) {
        return E_FAIL;
    }
}
SHIM_API HRESULT __stdcall AddStaticToMap(IDispatch* pMap)
{
    if (!pMap) return E_POINTER;

    try {
        // Example static key/value pairs
        const char* keys[] = { "Xmp.digiKam.ColorLabel", "Xmp.exif.DateTimeOriginal" };
        const char* values[] = { "0", "2021-01-05T20:55:26" };

        for (int i = 0; i < 2; ++i) {
            CComBSTR bstrKey(keys[i]);
            CComVariant varVal(values[i]);

            DISPID dispid;
            HRESULT hr = pMap->GetIDsOfNames(IID_NULL, reinterpret_cast<LPOLESTR*>(&bstrKey), 1,
                                             LOCALE_USER_DEFAULT, &dispid);
            if (FAILED(hr)) continue; // skip if key doesn't exist (AHK Map auto-creates)

            DISPPARAMS dp = { &varVal, nullptr, 1, 0 };
            hr = pMap->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT, &dp, nullptr, nullptr, nullptr);
            if (FAILED(hr)) continue;
        }

        return S_OK;
    }
    catch (...) {
        return E_FAIL;
    }
}
}

