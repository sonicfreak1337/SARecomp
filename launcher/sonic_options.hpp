#pragma once
#include "sonic_menu_runtime.hpp"
#include "sonic_menu_text.hpp"
#include "renderer/window_fullscreen.hpp"
#include <windows.h>
#include <filesystem>
#include <iostream>

namespace sonic::options {
inline constexpr UINT open_command=0x7350,install_message=WM_APP+0x351;
struct State {HWND window=nullptr;HMENU settings=nullptr;WNDPROC previous=nullptr;HHOOK hook=nullptr;};
inline State state;
inline std::wstring label(std::string_view key){return std::wstring(menu::text(key,menu::effective_text_language()));}
inline void refresh(){
    if(!state.settings)return;
    ModifyMenuW(state.settings,open_command,MF_BYCOMMAND|MF_STRING|(menu::available()?MF_ENABLED:MF_GRAYED),open_command,label("open_options").c_str());
    ModifyMenuW(rendering::window_menu(state.window),0,MF_BYPOSITION|MF_POPUP|MF_STRING,
        reinterpret_cast<UINT_PTR>(state.settings),label("title").c_str());
}
inline LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM word,LPARAM data){
    if(message==WM_INITMENU)refresh();
    if(message==WM_COMMAND&&HIWORD(word)==0&&LOWORD(word)==open_command){menu::request_open();return 0;}
    return CallWindowProcW(state.previous,window,message,word,data);
}
inline LRESULT CALLBACK install_hook(int code,WPARAM word,LPARAM data){
    if(code>=0){const auto& event=*reinterpret_cast<const CWPSTRUCT*>(data);
        if(event.hwnd==state.window&&event.message==install_message&&!state.settings){
            const auto root=rendering::window_menu(state.window);const auto settings=CreatePopupMenu();
            if(root&&settings){
                // Keep the existing SDK's developer tools available, while all
                // player settings use the one transactional in-game menu.
                ModifyMenuW(root,0,MF_BYPOSITION|MF_POPUP|MF_STRING,reinterpret_cast<UINT_PTR>(GetSubMenu(root,0)),L"Developer tools");
                AppendMenuW(settings,MF_STRING,open_command,label("open_options").c_str());
                if(InsertMenuW(root,0,MF_BYPOSITION|MF_POPUP|MF_STRING,reinterpret_cast<UINT_PTR>(settings),label("title").c_str())){
                    state.settings=settings;state.previous=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(state.window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&window_proc)));refresh();
                }else DestroyMenu(settings);
            }else if(settings)DestroyMenu(settings);
        }
    }
    return CallNextHookEx(state.hook,code,word,data);
}
inline BOOL CALLBACK own_window(HWND window,LPARAM){
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);wchar_t name[128]{};
    if(pid==GetCurrentProcessId()&&GetClassNameW(window,name,128)&&std::wstring_view(name)==L"KatanaRecompNativeGraphicsV1"){state.window=window;return FALSE;}return TRUE;
}
inline void install(const std::filesystem::path&){
    EnumWindows(&own_window,0);if(!state.window)throw std::runtime_error("Sonic Options window unavailable");
    state.hook=SetWindowsHookExW(WH_CALLWNDPROC,&install_hook,nullptr,GetWindowThreadProcessId(state.window,nullptr));
    if(!state.hook)throw std::runtime_error("Sonic Options attachment failed");
    SendMessageW(state.window,install_message,0,0);UnhookWindowsHookEx(state.hook);state.hook=nullptr;
    if(!state.previous)throw std::runtime_error("Sonic Options menu unavailable");
    std::cerr<<"SONIC_OPTIONS_READY shared_ingame_settings=1\n";
}
}
