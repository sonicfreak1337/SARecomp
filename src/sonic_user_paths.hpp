#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>
#include <string>
#include <stdexcept>

namespace sonic::paths {
inline std::wstring environment(const wchar_t* key) {
    const auto size=GetEnvironmentVariableW(key,nullptr,0);
    if(!size)return {};
    std::wstring value(size,L'\0');
    const auto read=GetEnvironmentVariableW(key,value.data(),size);
    if(!read || read>=size)throw std::runtime_error("user-path-environment-changed");
    value.resize(read);return value;
}
inline std::filesystem::path executable() {
    std::wstring path(32768,L'\0');
    const auto size=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));
    if(!size || size>=path.size())throw std::runtime_error("user-path-executable");
    path.resize(size);return path;
}
inline std::filesystem::path data_root(const std::filesystem::path& program) {
    if(const auto value=environment(L"KATANA_USER_DATA_ROOT");!value.empty())
        return std::filesystem::absolute(value).lexically_normal();
    // Portable storage is explicit. An old writable installation or an INI
    // beside the EXE does not silently opt the user into portable mode.
    if(environment(L"SARECOMP_PORTABLE")==L"1" ||
       std::filesystem::is_regular_file(program.parent_path()/"sarecomp-portable.txt"))
        return std::filesystem::absolute(program.parent_path()/"user-data").lexically_normal();
    if(const auto value=environment(L"LOCALAPPDATA");!value.empty())
        return (std::filesystem::absolute(value)/"SARecomp"/"experimental").lexically_normal();
    throw std::runtime_error("native-product-user-data-root");
}
inline std::filesystem::path cache_root(const std::filesystem::path& program) {
    if(const auto value=environment(L"SARECOMP_CACHE_ROOT");!value.empty())
        return std::filesystem::absolute(value).lexically_normal();
    return data_root(program)/"cache";
}
}
