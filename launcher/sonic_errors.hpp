#pragma once
#include "sonic_diagnostics.hpp"
#include "sonic_menu_runtime.hpp"
#include "sonic_menu_text.hpp"
#include "sonic_profiles.hpp"
#include <windows.h>
#include <string>

namespace sonic::errors {
struct State {int language;std::wstring body;};
inline std::wstring label(std::string_view key,int language){return std::wstring(menu::text(key,language));}
inline INT_PTR CALLBACK procedure(HWND window,UINT message,WPARAM word,LPARAM data){
    auto* state=reinterpret_cast<State*>(GetWindowLongPtrW(window,DWLP_USER));
    if(message==WM_INITDIALOG){
        state=reinterpret_cast<State*>(data);SetWindowLongPtrW(window,DWLP_USER,data);
        SetWindowTextW(window,L"Sonic Adventure: Recompiled");
        const auto child=[&](const wchar_t* type,const std::wstring& text,DWORD style,int x,int y,int w,int h,int id){
            RECT rect{x,y,x+w,y+h};MapDialogRect(window,&rect);
            const auto control=CreateWindowExW(0,type,text.c_str(),WS_CHILD|WS_VISIBLE|style,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,
                window,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
            SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return control;
        };
        child(L"STATIC",state->body,SS_LEFT,14,14,340,104,100);
        child(L"BUTTON",label("export_diagnostics",state->language),WS_TABSTOP|BS_PUSHBUTTON,14,139,204,25,IDYES);
        const auto close=child(L"BUTTON",label("close",state->language),WS_TABSTOP|BS_DEFPUSHBUTTON,228,139,126,25,IDCANCEL);
        SetFocus(close);return FALSE;
    }
    if(message==WM_CLOSE){EndDialog(window,0);return TRUE;}
    if(message==WM_COMMAND){
        if(LOWORD(word)==IDCANCEL){EndDialog(window,0);return TRUE;}
        if(LOWORD(word)==IDYES&&state){
            try {const auto report=profiles::export_diagnostics();
                SetWindowTextW(GetDlgItem(window,100),(label("report_saved",state->language)+L"\n\n"+report.wstring()).c_str());
                EnableWindow(GetDlgItem(window,IDYES),FALSE);
            }catch(...){SetWindowTextW(GetDlgItem(window,100),label("operation_error",state->language).c_str());}
            return TRUE;
        }
    }
    return FALSE;
}
inline void show() noexcept {
    if(diagnostics::failure.load()==diagnostics::Failure::None)return;
    if(const auto hidden=std::getenv("KATANA_PORT_BACKGROUND_TEST");hidden&&std::string_view(hidden)=="1")return;
    // A failed pre-game display trial belongs to its rollback parent.
    if(GetEnvironmentVariableW(L"SARECOMP_DISPLAY_TRIAL",nullptr,0))return;
    try {
        const auto language=menu::effective_text_language();
        State state{language,label(diagnostics::failure.load()==diagnostics::Failure::Graphics?"graphics_stopped":"game_stopped",language)+
            L"\n\n"+label("diagnostics_help",language)};
        struct Template {DLGTEMPLATE header;WORD menu=0,klass=0,title=0;};
        Template dialog{};dialog.header.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;
        dialog.header.cx=370;dialog.header.cy=180;
        DialogBoxIndirectParamW(GetModuleHandleW(nullptr),&dialog.header,nullptr,procedure,reinterpret_cast<LPARAM>(&state));
    }catch(...){} // Failure reporting must never replace the original failure.
}
}
