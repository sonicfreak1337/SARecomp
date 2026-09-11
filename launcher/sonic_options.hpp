#pragma once
// Port-owned extension of the pinned SDK's native Options menu. No SDK ABI
// changes or guest state writes. Attach on the window's own thread.
#include "sonic_presentation.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <windows.h>

namespace sonic::options {
inline constexpr UINT first_command=0x7350, last_command=first_command+3;
inline constexpr UINT install_message=WM_APP+0x351;
inline constexpr UINT restore_message=WM_APP+0x352;
struct MenuState {
    HWND window=nullptr;
    HMENU format=nullptr;
    WNDPROC previous=nullptr;
    HHOOK hook=nullptr;
    std::filesystem::path config;
    presentation::Settings selected;
    bool ready=false;
};
inline MenuState state;
inline std::wstring dimensions(const presentation::Settings& value) {
    return std::to_wstring(value.width)+L" x "+std::to_wstring(value.height);
}
inline unsigned choice(const presentation::Settings& value) {
    if (!value.widescreen) return 0;
    return value.width*9u==value.height*16u ? 1 : 2;
}
inline void save(const presentation::Settings& value) {
    auto temporary=state.config;
    temporary+=L".pending";
    {
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
        output<<"# Experimental Sonic presentation. Applied on next launch.\nmode="
              <<(value.widescreen?"widescreen":"original")<<"\nwidth="<<value.width
              <<"\nheight="<<value.height<<"\nrender_percent="<<value.render_percent<<'\n';
        output.flush();
        if (!output) throw std::runtime_error("Display settings could not be written");
    }
    if (!MoveFileExW(temporary.c_str(),state.config.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Display settings could not be published");
}
inline void select(const UINT command) {
    auto value=state.selected;
    if (command==first_command) value.widescreen=false;
    else if (command==first_command+1) {
        value.widescreen=true;
        value.height=1080; value.width=1920;
    } else if (command==first_command+2) {
        value.widescreen=true;
        value.height=1080; value.width=2560;
    } else if (command==first_command+3) {
        MONITORINFO info{sizeof(info)};
        if (!GetMonitorInfoW(MonitorFromWindow(state.window,MONITOR_DEFAULTTONEAREST),&info))
            throw std::runtime_error("Monitor size unavailable");
        const auto width=static_cast<unsigned>(info.rcMonitor.right-info.rcMonitor.left);
        const auto height=static_cast<unsigned>(info.rcMonitor.bottom-info.rcMonitor.top);
        const auto divisor=std::gcd(width,height);
        RECT outer{},client{};
        GetWindowRect(state.window,&outer); GetClientRect(state.window,&client);
        const auto available_width=static_cast<unsigned>(info.rcWork.right-info.rcWork.left-
            ((outer.right-outer.left)-(client.right-client.left)));
        const auto available_height=static_cast<unsigned>(info.rcWork.bottom-info.rcWork.top-
            ((outer.bottom-outer.top)-(client.bottom-client.top)));
        const auto multiple=std::min(available_width/(width/divisor),available_height/(height/divisor));
        value.width=(width/divisor)*multiple;
        value.height=(height/divisor)*multiple;
        value.widescreen=value.width*3u>=value.height*4u;
    } else return;
    if (value.width<640 || value.width>7680 || value.height<480 || value.height>4320)
        throw std::runtime_error("Monitor size outside supported display range");
    save(value);
    state.selected=value;
    CheckMenuRadioItem(state.format,first_command,last_command,command,MF_BYCOMMAND);
    const auto label=L"Nach Neustart: "+dimensions(value);
    ModifyMenuW(state.format,last_command+2,MF_BYCOMMAND|MF_STRING|MF_GRAYED,last_command+2,label.c_str());
    std::cerr<<"SONIC_OPTIONS_SAVED width="<<value.width<<" height="<<value.height
             <<" widescreen="<<value.widescreen<<" applies=restart\n";
}
inline LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM word,LPARAM data) {
    if (message==restore_message) {
        try {
            state.selected=presentation::settings();
            save(state.selected);
            CheckMenuRadioItem(state.format,first_command,last_command,first_command+choice(state.selected),MF_BYCOMMAND);
            ModifyMenuW(state.format,last_command+2,MF_BYCOMMAND|MF_STRING|MF_GRAYED,last_command+2,
                        L"Aenderungen gelten nach Neustart");
        } catch (...) {return 0;}
        return 1;
    }
    if (message==WM_COMMAND && HIWORD(word)==0 && LOWORD(word)>=first_command && LOWORD(word)<=last_command) {
        try {select(LOWORD(word));}
        catch (const std::exception& error) {
            std::cerr<<"SONIC_OPTIONS_ERROR "<<error.what()<<'\n';
            ModifyMenuW(state.format,last_command+2,MF_BYCOMMAND|MF_STRING|MF_GRAYED,last_command+2,
                        L"Einstellung konnte nicht gespeichert werden");
        }
        return 0;
    }
    return CallWindowProcW(state.previous,window,message,word,data);
}
inline LRESULT CALLBACK install_hook(int code,WPARAM word,LPARAM data) {
    if (code>=0) {
        const auto& message=*reinterpret_cast<const CWPSTRUCT*>(data);
        if (message.hwnd==state.window && message.message==install_message && !state.ready) {
            auto menu=GetSubMenu(GetMenu(state.window),0);
            auto format=CreatePopupMenu();
            if (menu && format) {
                AppendMenuW(format,MF_STRING,first_command,L"Original (4:3)");
                AppendMenuW(format,MF_STRING,first_command+1,L"16:9 - 1920 x 1080");
                AppendMenuW(format,MF_STRING,first_command+2,L"21:9 - 2560 x 1080");
                AppendMenuW(format,MF_STRING,first_command+3,L"An Monitor anpassen");
                AppendMenuW(format,MF_SEPARATOR,0,nullptr);
                const auto active=L"Aktiv: "+dimensions(state.selected);
                AppendMenuW(format,MF_STRING|MF_GRAYED,last_command+1,active.c_str());
                AppendMenuW(format,MF_STRING|MF_GRAYED,last_command+2,L"Aenderungen gelten nach Neustart");
                AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
                if (AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(format),L"Bildformat (Neustart)")) {
                    state.format=format;
                    state.previous=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(state.window,GWLP_WNDPROC,
                        reinterpret_cast<LONG_PTR>(&window_proc)));
                    state.ready=state.previous!=nullptr;
                    CheckMenuRadioItem(format,first_command,last_command,first_command+choice(state.selected),MF_BYCOMMAND);
                } else DestroyMenu(format);
            } else if (format) DestroyMenu(format);
        }
    }
    return CallNextHookEx(state.hook,code,word,data);
}
inline BOOL CALLBACK own_window(HWND window,LPARAM) {
    DWORD pid=0;
    GetWindowThreadProcessId(window,&pid);
    wchar_t name[128]{};
    if (pid==GetCurrentProcessId() && GetClassNameW(window,name,128) &&
        std::wstring_view(name)==L"KatanaRecompNativeGraphicsV1") {
        state.window=window; return FALSE;
    }
    return TRUE;
}
inline void install(const std::filesystem::path& executable) {
    state.config=executable.parent_path()/"sonic-display.ini";
    if (const auto* configured=std::getenv("SARECOMP_DISPLAY_CONFIG"); configured && *configured)
        state.config=configured;
    state.selected=presentation::settings();
    EnumWindows(&own_window,0);
    if (!state.window) throw std::runtime_error("Sonic Options window unavailable");
    state.hook=SetWindowsHookExW(WH_CALLWNDPROC,&install_hook,nullptr,GetWindowThreadProcessId(state.window,nullptr));
    if (!state.hook) throw std::runtime_error("Sonic Options attachment failed");
    SendMessageW(state.window,install_message,0,0);
    UnhookWindowsHookEx(state.hook);
    state.hook=nullptr;
    if (!state.ready) throw std::runtime_error("Sonic Options menu unavailable");
    std::cerr<<"SONIC_OPTIONS_READY choices=original,16:9,21:9,monitor applies=restart\n";
    // A bounded in-process menu integration check, only in hidden captures.
    const auto* test=std::getenv("SARECOMP_OPTIONS_SELF_TEST");
    const auto* hidden=std::getenv("KATANA_PORT_BACKGROUND_TEST");
    if (test && std::string_view(test)=="1" && hidden && std::string_view(hidden)=="1") {
        for (unsigned index : {3u,2u,1u,0u}) {
            SendMessageW(state.window,WM_COMMAND,first_command+index,0);
            if ((GetMenuState(state.format,first_command+index,MF_BYCOMMAND)&MF_CHECKED)==0)
                throw std::runtime_error("Sonic Options command failed");
        }
        // Return the pending selection to the active configuration through
        // the owning thread, preserving the exact custom capture resolution.
        if (SendMessageW(state.window,restore_message,0,0)!=1)
            throw std::runtime_error("Sonic Options settings restore failed");
        std::cerr<<"SONIC_OPTIONS_SELF_TEST_OK settings_restored=1\n";
    }
}
}
