#pragma once
#include "sonic_presentation.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace sonic::configuration {
inline bool first_start(const std::filesystem::path& executable) {
    const auto* hidden=std::getenv("KATANA_PORT_BACKGROUND_TEST");
    if(hidden && std::string_view(hidden)=="1") return true;
    const auto path=presentation::configuration_path(executable);
    if(presentation::read_settings(path).setup_complete) return true;
#ifdef _WIN32
    const auto program=executable.parent_path()/"sonic-config.exe";
    auto command=L"\""+program.wstring()+L"\" --first-run --config \""+path.wstring()+L"\"";
    STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
    if(!CreateProcessW(program.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,
        executable.parent_path().c_str(),&startup,&process))
        throw std::runtime_error("First-start configuration is missing: keep sonic-config.exe beside game.exe");
    CloseHandle(process.hThread);
    const auto waited=WaitForSingleObject(process.hProcess,INFINITE);
    DWORD result=2;GetExitCodeProcess(process.hProcess,&result);CloseHandle(process.hProcess);
    if(waited!=WAIT_OBJECT_0 || result>1) throw std::runtime_error("First-start configuration failed");
    return result==0 && presentation::read_settings(path).setup_complete;
#else
    const auto program=executable.parent_path()/"sonic-setup";
    ::execl(program.c_str(),program.c_str(),static_cast<char*>(nullptr));
    throw std::runtime_error("First-start setup is missing: extract the complete Linux package");
#endif
}
}
