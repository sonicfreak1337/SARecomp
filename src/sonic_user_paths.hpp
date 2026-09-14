#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#include <unistd.h>
#endif
#include <filesystem>
#include <string>
#include <stdexcept>

namespace sonic::paths {
#ifdef _WIN32
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
#else
inline std::string environment(const wchar_t* key) {
    std::string name;
    for(;*key;++key)name.push_back(static_cast<char>(*key));
    const auto* value=std::getenv(name.c_str());return value?value:"";
}
inline std::filesystem::path executable() {
    return std::filesystem::read_symlink("/proc/self/exe");
}
#endif
inline std::filesystem::path data_root(const std::filesystem::path& program) {
    if(const auto value=environment(L"KATANA_USER_DATA_ROOT");!value.empty())
        return std::filesystem::absolute(value).lexically_normal();
    // Portable storage is explicit. An old writable installation or an INI
    // beside the EXE does not silently opt the user into portable mode.
    if(std::filesystem::path(environment(L"SARECOMP_PORTABLE"))=="1" ||
       std::filesystem::is_regular_file(program.parent_path()/"sarecomp-portable.txt"))
        return std::filesystem::absolute(program.parent_path()/"user-data").lexically_normal();
#ifdef _WIN32
    if(const auto value=environment(L"LOCALAPPDATA");!value.empty())
        return (std::filesystem::absolute(value)/"SARecomp"/"experimental").lexically_normal();
#else
    if(const auto value=environment(L"XDG_DATA_HOME");!value.empty() && std::filesystem::path(value).is_absolute())
        return std::filesystem::path(value)/"SARecomp";
    if(const auto value=environment(L"HOME");!value.empty())
        return std::filesystem::path(value)/".local/share/SARecomp";
#endif
    throw std::runtime_error("native-product-user-data-root");
}
inline std::filesystem::path cache_root(const std::filesystem::path& program) {
    if(const auto value=environment(L"SARECOMP_CACHE_ROOT");!value.empty())
        return std::filesystem::absolute(value).lexically_normal();
#ifndef _WIN32
    if(environment(L"KATANA_USER_DATA_ROOT").empty() && environment(L"SARECOMP_PORTABLE").empty() &&
       !std::filesystem::is_regular_file(program.parent_path()/"sarecomp-portable.txt")) {
        if(const auto value=environment(L"XDG_CACHE_HOME");!value.empty() && std::filesystem::path(value).is_absolute())
            return std::filesystem::path(value)/"SARecomp";
        if(const auto value=environment(L"HOME");!value.empty())
            return std::filesystem::path(value)/".cache/SARecomp";
    }
#endif
    return data_root(program)/"cache";
}
// Installed read-only content and writable profiles must remain disjoint,
// including when a test or portable installation selects a custom save root.
inline std::filesystem::path content_store_root(const std::filesystem::path& program) {
    const auto state=data_root(program);
    if(state.filename().empty())throw std::runtime_error("content-store-root");
    return state.parent_path()/(state.filename().string()+"-content");
}
}
