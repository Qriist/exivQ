#include "exivQ.h"
#include <exiv2/exiv2.hpp>
#include <fstream>
#include <codecvt>
#include <locale>
#include <string>
#include <Windows.h>
#include <combaseapi.h> // CoTaskMemAlloc / CoTaskMemFree
#include <nlohmann/json.hpp>
#include <unordered_set>

SHIM_API const wchar_t* Exiv2VersionW() {
    static std::wstring versionW;
    std::string version = Exiv2::version(); // returns std::string
    versionW.assign(version.begin(), version.end());
    return versionW.c_str();
}

// int ReadXmpFile(const char* path, const char** jsonPtr, size_t* jsonSize)
// SHIM_API int ReadXmpF1ile(const wchar_t* filePathW, wchar_t** outBuffer)
// {
//     if (!filePathW || !outBuffer)
//         return -1; // invalid arguments

//     *outBuffer = nullptr;
//     // if (!jsonPtr || !jsonSize) return -1;
//     // *jsonPtr = nullptr;
//     // *jsonSize = 0;

//     // 1. Open XMP file
//     std::ifstream file(filePathW, std::ios::binary);
//     if (!file.is_open()) return -2;

//     // 2. Determine file size
//     file.seekg(0, std::ios::end);
//     std::streamsize size = file.tellg();
//     file.seekg(0, std::ios::beg);

//     // 3. Read into mutable string buffer
//     std::string xmpStr(size, '\0');
//     if (!file.read(&xmpStr[0], size)) {
//         return -3; // read error
//     }
// }
// ---------------------------
// Read XMP file
// ---------------------------
SHIM_API int ReadXmpFile(const wchar_t* path, wchar_t** outBuffer) {
    if (!path || !outBuffer) return -1;
    *outBuffer = nullptr;

    try {
        // UTF-16/UTF-8 conversion
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        std::string pathUtf8 = conv.to_bytes(path);

        // Open file at end to get size
        std::ifstream file(pathUtf8, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return -2;

        std::streamsize size = file.tellg();
        if (size <= 0) return -3;
        file.seekg(0, std::ios::beg);

        // Read file into string
        std::string xmpStr(size, '\0');
        if (!file.read(&xmpStr[0], size)) return -4;

        // Convert to UTF-16 wstring
        std::wstring xmpW = conv.from_bytes(xmpStr);

        // Use vector for contiguous allocation
        std::vector<wchar_t> bufferVec(xmpW.begin(), xmpW.end());
        bufferVec.push_back(L'\0'); // null terminator

        // Allocate CoTaskMem buffer and copy
        size_t byteSize = bufferVec.size() * sizeof(wchar_t);
        wchar_t* buffer = (wchar_t*)CoTaskMemAlloc(byteSize);
        if (!buffer) return -5;

        memcpy(buffer, bufferVec.data(), byteSize);
        *outBuffer = buffer;

        return 0;
    } catch (...) {
        return -6;
    }
}


// ---------------------------
// Free buffer
// ---------------------------
SHIM_API void FreeXmpFile(wchar_t* buffer) {
    if (buffer) CoTaskMemFree(buffer);
}
using json = nlohmann::json;

#include <exiv2/exiv2.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <map>
#include <regex>

using json = nlohmann::json;

// mdToJson: Processes a single Xmpdatum
json mdToJson(Exiv2::Xmpdatum& md) {
    std::string type = md.typeName();
    json result;

    // Scalar text
    if (type == "XmpText") {
        result = {
            {"type", type},
            {"value", md.toString()}
        };
        return result;
    }
    // Arrays (XmpBag, XmpSeq, XmpAlt, LangAlt)
    else if (type == "XmpBag" || type == "XmpSeq" || type == "XmpAlt" || type == "LangAlt") {
        json arr = json::array();
        const Exiv2::Value& value = md.value();
        try {
            for (size_t i = 0; i < value.count(); ++i) {
                std::string val = value.toString(i);
                if (type == "LangAlt") {
                    std::string lang = "x-default";
                    std::string text = val;
                    if (val.find("lang=\"") == 0) {
                        size_t langEnd = val.find("\" ", 5);
                        if (langEnd != std::string::npos) {
                            lang = val.substr(6, langEnd - 6);
                            text = val.substr(langEnd + 2);
                        }
                    }
                    arr.push_back({
                        {"lang", lang},
                        {"value", text}
                    });
                } else {
                    arr.push_back({
                        {"type", "XmpText"},
                        {"value", val}
                    });
                }
            }
        } catch (const Exiv2::Error& e) {
            result = {
                {"type", type},
                {"value", "Error: " + std::string(e.what())}
            };
            return result;
        }
        result = {
            {"type", type},
            {"value", arr}
        };
        return result;
    }
    // Fallback for other types
    else {
        result = {
            {"type", type},
            {"value", md.toString()}
        };
        return result;
    }
}

// xmpDataToJson: Processes all Xmpdatum entries and groups structs and arrays
json xmpDataToJson(const Exiv2::XmpData& xmpData) {
    json result = json::object();
    std::map<std::string, json> structs;
    std::map<std::string, std::map<int, json>> arrayStructs;

    // Regex to match array indices (e.g., MPRI:Regions[1]/MPReg:PersonDisplayName)
    std::regex arrayRegex(R"((.*)\[(\d+)\](.*))");

    // First pass: Handle array subfields
    for (const auto& datum : xmpData) {
        std::string key = datum.key();
        std::smatch match;

        // Check if the key is part of an array element
        if (std::regex_match(key, match, arrayRegex)) {
            std::string arrayKey = match[1].str();
            int index = std::stoi(match[2].str()) - 1;
            std::string subKey = match[3].str().empty() ? "" : match[3].str().substr(1);

            if (!subKey.empty()) {
                // Subfield of array element
                arrayStructs[arrayKey][index]["type"] = "Struct";
                arrayStructs[arrayKey][index]["value"][subKey] = mdToJson(const_cast<Exiv2::Xmpdatum&>(datum));
            }
            // Skip array index keys (e.g., MPRI:Regions[1])
        }
    }

    // Second pass: Handle arrays with subfields
    for (const auto& [arrayKey, indices] : arrayStructs) {
        json arr = json::array();

        // Create array elements
        for (size_t i = 0; i < indices.size(); ++i) {
            if (indices.find(i) != indices.end()) {
                arr.push_back(indices.at(i));
            } else {
                arr.push_back({
                    {"type", "Struct"},
                    {"value", json::object()}
                });
            }
        }

        // Force XmpBag type for arrays with subfields
        std::string arrayType = "XmpBag";

        json arrayJson = {
            {"type", arrayType},
            {"value", arr}
        };

        // Place the array in the appropriate struct or result
        size_t pos = arrayKey.rfind('/');
        std::string parentKey = (pos == std::string::npos) ? arrayKey : arrayKey.substr(0, pos);
        std::string arrayName = (pos == std::string::npos) ? arrayKey : arrayKey.substr(pos + 1);

        if (pos != std::string::npos) {
            structs[parentKey][arrayName] = arrayJson;
        } else {
            result[std::string(arrayKey)] = arrayJson; // Explicit cast to resolve operator[] ambiguity
        }
    }

    // Third pass: Handle non-array keys
    for (const auto& datum : xmpData) {
        std::string key = datum.key();
        std::smatch match;

        // Skip array subfields and array keys
        if (std::regex_match(key, match, arrayRegex) || arrayStructs.find(key) != arrayStructs.end()) {
            continue;
        }

        size_t pos = key.rfind('/');
        if (pos != std::string::npos) {
            // Struct field
            std::string parentKey = key.substr(0, pos);
            std::string fieldName = key.substr(pos + 1);
            structs[parentKey][fieldName] = mdToJson(const_cast<Exiv2::Xmpdatum&>(datum));
        } else {
            // Non-struct, non-array field
            result[key] = mdToJson(const_cast<Exiv2::Xmpdatum&>(datum));
        }
    }

    // Process structs
    for (const auto& [key, value] : structs) {
        result[std::string(key)] = json{ // Explicit cast to resolve operator[] ambiguity
            {"type", "Struct"},
            {"value", value}
        };
    }

    return result;
}

// ReadXmpPropertiesJSON: Reads XMP file and converts to JSON
extern "C" SHIM_API const char* __cdecl ReadXmpPropertiesJSON(const char* filePath) {
    try {
        Exiv2::XmpParser::initialize();
        Exiv2::DataBuf buf = Exiv2::readFile(filePath);
        std::string xmpPacket(buf.c_str(), buf.size());

        Exiv2::XmpData xmpData;
        if (0 != Exiv2::XmpParser::decode(xmpData, xmpPacket)) {
            Exiv2::XmpParser::terminate();
            return _strdup("Error: failed to parse XMP packet");
        }

        json j = xmpDataToJson(xmpData);

        Exiv2::XmpParser::terminate();
        return _strdup(j.dump().c_str()); // Caller must free
    }
    catch (const Exiv2::Error& e) {
        Exiv2::XmpParser::terminate();
        return _strdup(("Exiv2 Error: " + std::string(e.what())).c_str());
    }
    catch (const std::exception& e) {
        Exiv2::XmpParser::terminate();
        return _strdup(e.what());
    }
}

// // jsonToXmpDatum: Converts a JSON object to an Exiv2::Xmpdatum
// void jsonToXmpDatum(const json& j, const std::string& key, Exiv2::XmpData& xmpData) {
//     if (!j.contains("type") || !j.contains("value")) {
//         return; // Invalid JSON structure
//     }

//     std::string type = j["type"].get<std::string>();

//     if (type == "XmpText") {
//         xmpData[key] = Exiv2::XmpTextValue(j["value"].get<std::string>());
//     }
//     else if (type == "XmpBag" || type == "XmpSeq") {
//         std::unique_ptr<Exiv2::Value> value = type == "XmpBag" ?
//             Exiv2::Value::create(Exiv2::xmpBag) :
//             Exiv2::Value::create(Exiv2::xmpSeq);
//         for (const auto& item : j["value"]) {
//             if (item.contains("type") && item["type"] == "XmpText" && item.contains("value")) {
//                 value->read(item["value"].get<std::string>());
//             }
//         }
//         xmpData.add(Exiv2::XmpKey(key), value.get());
//     }
//     else if (type == "LangAlt") {
//         std::unique_ptr<Exiv2::Value> value = Exiv2::Value::create(Exiv2::langAlt);
//         for (const auto& item : j["value"]) {
//             if (item.contains("lang") && item.contains("value")) {
//                 std::string lang = item["lang"].get<std::string>();
//                 std::string val = item["value"].get<std::string>();
//                 value->read(val + " lang=\"" + lang + "\"");
//             }
//         }
//         xmpData.add(Exiv2::XmpKey(key), value.get());
//     }
//     else if (type == "Struct") {
//         for (const auto& [field, fieldJson] : j["value"].items()) {
//             std::string subKey = key + "/" + field;
//             jsonToXmpDatum(fieldJson, subKey, xmpData);
//         }
//     }
// }


// jsonToXmpDatum: Converts a JSON object to an Exiv2::Xmpdatum
void jsonToXmpDatum(const json& j, const std::string& key, Exiv2::XmpData& xmpData) {
    if (!j.contains("type") || !j.contains("value")) {
        return; // Invalid JSON structure
    }

    std::string type = j["type"].get<std::string>();

    if (type == "XmpText") {
        xmpData[key] = Exiv2::XmpTextValue(j["value"].get<std::string>());
    }
    else if (type == "XmpBag" || type == "XmpSeq") {
        std::unique_ptr<Exiv2::Value> value = type == "XmpBag" ?
            Exiv2::Value::create(Exiv2::xmpBag) :
            Exiv2::Value::create(Exiv2::xmpSeq);
        for (const auto& item : j["value"]) {
            if (item.contains("type") && item["type"] == "XmpText" && item.contains("value")) {
                value->read(item["value"].get<std::string>());
            }
        }
        xmpData.add(Exiv2::XmpKey(key), value.get());
    }
    else if (type == "LangAlt") {
        std::unique_ptr<Exiv2::Value> value = Exiv2::Value::create(Exiv2::langAlt);
        for (const auto& item : j["value"]) {
            if (item.contains("lang") && item.contains("value")) {
                std::string lang = item["lang"].get<std::string>();
                std::string val = item["value"].get<std::string>();
                value->read(val + " lang=\"" + lang + "\"");
            }
        }
        xmpData.add(Exiv2::XmpKey(key), value.get());
    }
    else if (type == "Struct") {
        for (const auto& [field, fieldJson] : j["value"].items()) {
            std::string subKey = key + "/" + field;
            jsonToXmpDatum(fieldJson, subKey, xmpData);
        }
    }
}


// // WriteXmpPropertiesJSON: Updates XMP file from JSON input
// extern "C" SHIM_API int __cdecl WriteXmpPropertiesJSON(const char* filePath, const char* jsonStr) {
//     try {
//         Exiv2::XmpParser::initialize();
//         Exiv2::XmpProperties::registerNs("http://sparklegleam.com/ns/", "sparklegleam");

//         // Parse JSON input
//         json j = json::parse(jsonStr);

//         // Convert JSON to XmpData
//         Exiv2::XmpData xmpData;
//         std::regex arrayRegex(R"((.*)\[(\d+)\](.*))");
//         std::map<std::string, std::map<int, json>> arrayStructs;

//         // First pass: Handle array subfields (e.g., MPRI:Regions[1]/MPReg:PersonDisplayName)
//         for (const auto& [key, value] : j.items()) {
//             std::smatch match;
//             if (std::regex_match(key, match, arrayRegex)) {
//                 std::string arrayKey = match[1].str();
//                 int index = std::stoi(match[2].str()) - 1;
//                 std::string subKey = match[3].str().empty() ? "" : match[3].str().substr(1);
//                 if (!subKey.empty()) {
//                     arrayStructs[arrayKey][index] = value;
//                 }
//             }
//         }

//         // Second pass: Process array structs
//         for (const auto& [arrayKey, indices] : arrayStructs) {
//             std::unique_ptr<Exiv2::Value> value = Exiv2::Value::create(Exiv2::xmpBag);
//             for (size_t i = 0; i < indices.size(); ++i) {
//                 if (indices.find(i) != indices.end()) {
//                     const json& structJson = indices.at(i);
//                     if (structJson.contains("type") && structJson["type"] == "Struct") {
//                         for (const auto& [field, fieldJson] : structJson["value"].items()) {
//                             std::string subKey = arrayKey + "[" + std::to_string(i + 1) + "]/" + field;
//                             jsonToXmpDatum(fieldJson, subKey, xmpData);
//                         }
//                     }
//                 }
//             }
//             xmpData[arrayKey] = value.get(); // Empty bag for array key
//         }

//         // Third pass: Process non-array keys
//         for (const auto& [key, value] : j.items()) {
//             std::smatch match;
//             if (std::regex_match(key, match, arrayRegex) || arrayStructs.find(key) != arrayStructs.end()) {
//                 continue;
//             }
//             jsonToXmpDatum(value, key, xmpData);
//         }

//         // Encode XmpData to XMP packet
//         std::string xmpPacket;
//         if (0 != Exiv2::XmpParser::encode(xmpPacket, xmpData)) {
//             Exiv2::XmpParser::terminate();
//             return 1;
//         }

//         // Write XMP packet to file
//         Exiv2::DataBuf buf(reinterpret_cast<const Exiv2::byte*>(xmpPacket.data()), xmpPacket.size());
//         if (0 != Exiv2::writeFile(buf, filePath)) {
//             Exiv2::XmpParser::terminate();
//             return 1;
//         }

//         Exiv2::XmpParser::terminate();
//         return 0;
//     }
//     catch (const std::exception& e) {
//         Exiv2::XmpParser::terminate();
//         return 1;
//     }
// }


// WriteXmpPropertiesJSON: Updates XMP file from JSON input
extern "C" SHIM_API int __cdecl WriteXmpPropertiesJSON(const char* filePath, const char* jsonStr) {
    try {
        Exiv2::XmpParser::initialize();
        Exiv2::XmpProperties::registerNs("http://sparklegleam.com/ns/", "sparklegleam");

        // Parse JSON input
        json j = json::parse(jsonStr);

        // Convert JSON to XmpData
        Exiv2::XmpData xmpData;
        std::regex arrayRegex(R"((.*)\[(\d+)\](.*))");
        std::map<std::string, std::map<int, json>> arrayStructs;

        // First pass: Handle array subfields (e.g., MPRI:Regions[1]/MPReg:PersonDisplayName)
        for (const auto& [key, value] : j.items()) {
            std::smatch match;
            if (std::regex_match(key, match, arrayRegex)) {
                std::string arrayKey = match[1].str();
                int index = std::stoi(match[2].str()) - 1;
                std::string subKey = match[3].str().empty() ? "" : match[3].str().substr(1);
                if (!subKey.empty()) {
                    arrayStructs[arrayKey][index] = value;
                }
            }
        }

        // Second pass: Process array structs
        for (const auto& [arrayKey, indices] : arrayStructs) {
            std::unique_ptr<Exiv2::Value> value = Exiv2::Value::create(Exiv2::xmpBag);
            for (size_t i = 0; i < indices.size(); ++i) {
                if (indices.find(i) != indices.end()) {
                    const json& structJson = indices.at(i);
                    if (structJson.contains("type") && structJson["type"] == "Struct") {
                        for (const auto& [field, fieldJson] : structJson["value"].items()) {
                            std::string subKey = arrayKey + "[" + std::to_string(i + 1) + "]/" + field;
                            jsonToXmpDatum(fieldJson, subKey, xmpData);
                        }
                    }
                }
            }
            xmpData[arrayKey] = value.get(); // Empty bag for array key
        }

        // Third pass: Process non-array keys
        for (const auto& [key, value] : j.items()) {
            std::smatch match;
            if (std::regex_match(key, match, arrayRegex) || arrayStructs.find(key) != arrayStructs.end()) {
                continue;
            }
            jsonToXmpDatum(value, key, xmpData);
        }

        // Encode XmpData to XMP packet
        std::string xmpPacket;
        if (0 != Exiv2::XmpParser::encode(xmpPacket, xmpData)) {
            Exiv2::XmpParser::terminate();
            return 1;
        }

        // Write XMP packet to file
        Exiv2::DataBuf buf(reinterpret_cast<const Exiv2::byte*>(xmpPacket.c_str()), xmpPacket.size());
        if (0 != Exiv2::writeFile(buf, filePath)) {
            Exiv2::XmpParser::terminate();
            return 1;
        }

        Exiv2::XmpParser::terminate();
        return 0;
    }
    catch (const std::exception& e) {
        Exiv2::XmpParser::terminate();
        return 1;
    }
}