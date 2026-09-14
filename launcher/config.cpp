#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include "sonic_presentation.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using sonic::presentation::Settings;
enum Control { Renderer=101,WindowMode,Resolution,Aspect,RenderScale,GameTiming,TextLanguage,VoiceLanguage,Subtitles,CameraStyle,ErrorText,VSync };
struct Dialog {
    std::filesystem::path path;
    Settings settings;
    HWND window=nullptr;
    HFONT font=nullptr,title_font=nullptr;
    int dpi=96;
    bool first_run=false,self_test=false,saved=false;
    std::wstring error;
    int px(int value) const {return MulDiv(value,dpi,96);}
    ~Dialog() {if(font) DeleteObject(font);if(title_font) DeleteObject(title_font);}
    HWND control(const wchar_t* type,const wchar_t* text,DWORD style,int id,int x,int y,int w,int h) {
        auto child=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),
            window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        if(!child) throw std::runtime_error("Could not create a configuration control");
        SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        return child;
    }
    HWND combo(int id,const wchar_t* label,int y,const std::vector<std::wstring>& values,int selected,bool editable=false) {
        control(L"STATIC",label,0,0,28,y+5,178,24);
        auto box=control(L"COMBOBOX",L"",WS_TABSTOP|WS_VSCROLL|(editable?CBS_DROPDOWN:CBS_DROPDOWNLIST),id,210,y,350,250);
        SendMessageW(box,CB_SETMINVISIBLE,8,0);
        for(const auto& value:values) SendMessageW(box,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value.c_str()));
        SendMessageW(box,CB_SETCURSEL,selected,0);
        return box;
    }
    int selected(int id) const {return int(SendDlgItemMessageW(window,id,CB_GETCURSEL,0,0));}
    std::wstring text(int id) const {
        const auto child=GetDlgItem(window,id);
        std::wstring value(GetWindowTextLengthW(child)+1,L'\0');
        value.resize(GetWindowTextW(child,value.data(),int(value.size())));
        return value;
    }
    void populate() {
        font=CreateFontW(-px(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        title_font=CreateFontW(-px(22),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        auto title=control(L"STATIC",L"Sonic Adventure: Recompiled",0,0,28,22,535,32);
        SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(title_font),TRUE);
        control(L"STATIC",L"Graphics, camera and language settings",0,0,28,58,535,25);
        combo(Renderer,L"Renderer",102,{L"Direct3D 11",L"Vulkan (experimental)"},int(settings.renderer));
        combo(WindowMode,L"Display mode",145,{L"Windowed",L"Borderless fullscreen",L"Fullscreen (exclusive)"},int(settings.window_mode));
        std::set<std::pair<unsigned,unsigned>> sizes{{640,480},{1280,720},{1920,1080},{2560,1080},{2560,1440},{3440,1440},{3840,2160}};
        DEVMODEW mode{}; mode.dmSize=sizeof(mode);
        for(DWORD index=0;EnumDisplaySettingsW(nullptr,index,&mode);++index)
            if(mode.dmPelsWidth>=640 && mode.dmPelsWidth<=7680 && mode.dmPelsHeight>=480 && mode.dmPelsHeight<=4320)
                sizes.emplace(mode.dmPelsWidth,mode.dmPelsHeight);
        sizes.emplace(settings.width,settings.height);
        std::vector<std::wstring> resolutions;
        int current=0;
        for(auto [width,height]:sizes) {
            if(width==settings.width && height==settings.height) current=int(resolutions.size());
            resolutions.push_back(std::to_wstring(width)+L" x "+std::to_wstring(height));
        }
        combo(Resolution,L"Resolution",188,resolutions,current,true);
        combo(Aspect,L"Picture format",231,{L"Original (4:3)",L"Widescreen (match resolution)"},settings.widescreen?1:0);
        std::vector<std::wstring> scales{L"25",L"50",L"75",L"100"};
        const auto scale=std::to_wstring(settings.render_percent);
        if(std::find(scales.begin(),scales.end(),scale)==scales.end()) scales.push_back(scale);
        combo(RenderScale,L"Render scale (%)",274,scales,int(std::find(scales.begin(),scales.end(),scale)-scales.begin()));
        combo(GameTiming,L"Game timing",317,{L"Original",L"Recompiled"},int(settings.gameplay_timing));
        combo(VSync,L"VSync",360,{L"Off",L"On"},settings.vsync==2?0:1);
        combo(TextLanguage,L"Text language",402,{L"Use game setting",L"Japanese",L"English",L"French",L"Spanish",L"German"},settings.text_language+1);
        combo(VoiceLanguage,L"Voice language",445,{L"Use game setting",L"Japanese",L"English"},settings.voice_language+1);
        combo(Subtitles,L"Subtitles",488,{L"Use game setting",L"Off",L"On"},settings.subtitles+1);
        combo(CameraStyle,L"Camera style",531,{L"Original",L"Recompiled"},int(settings.camera_style));
        control(L"STATIC",L"Recompiled: orbit with the right stick. Scripted camera sequences stay original.",0,0,28,571,535,40);
        control(L"STATIC",L"Recompiled: 60 FPS, or display-synced output with VSync.\nOriginal: original game and output rates. Changes require a restart.",0,0,28,620,535,42);
        control(L"STATIC",L"",0,ErrorText,28,666,535,40);
        control(L"BUTTON",L"Cancel",WS_TABSTOP|BS_PUSHBUTTON,IDCANCEL,306,714,110,34);
        control(L"BUTTON",first_run?L"Save && start":L"Save",WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK,430,714,130,34);
    }
    Settings read_controls() const {
        auto result=settings;
        std::wistringstream resolution(text(Resolution));
        wchar_t separator=0;
        if(!(resolution>>result.width>>separator>>result.height) || (separator!=L'x' && separator!=L'X'))
            throw std::runtime_error("Enter a resolution such as 1920 x 1080.");
        resolution>>std::ws;
        if(!resolution.eof()) throw std::runtime_error("Enter a resolution such as 1920 x 1080.");
        result.renderer=sonic::rendering::Renderer(selected(Renderer));
        result.window_mode=sonic::rendering::WindowMode(selected(WindowMode));
        result.widescreen=selected(Aspect)==1;
        result.render_percent=std::stoul(text(RenderScale));
        result.gameplay_timing=unsigned(selected(GameTiming));
        result.vsync=selected(VSync)?1u:2u;
        result.text_language=selected(TextLanguage)-1;
        result.voice_language=selected(VoiceLanguage)-1;
        result.subtitles=selected(Subtitles)-1;
        result.camera_style=sonic::camera::Style(selected(CameraStyle));
        result.setup_complete=true;
        return result;
    }
    void save() {
        settings=sonic::presentation::save_settings_changes(path,settings,read_controls());
        saved=true;
    }
    static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM word,LPARAM data) {
        auto* self=reinterpret_cast<Dialog*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(message==WM_NCCREATE) {
            self=static_cast<Dialog*>(reinterpret_cast<CREATESTRUCTW*>(data)->lpCreateParams);
            self->window=window; SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        if(!self) return DefWindowProcW(window,message,word,data);
        try {
            if(message==WM_CREATE) {self->populate();return 0;}
            if(message==WM_COMMAND && LOWORD(word)==Aspect && HIWORD(word)==CBN_SELCHANGE) {
                if(self->selected(Aspect)==1)
                    MessageBoxW(window,L"Some cutscenes may be glitchy.",L"Widescreen",MB_OK|MB_ICONWARNING);
                return 0;
            }
            if(message==WM_COMMAND && LOWORD(word)==IDOK) {
                self->save(); if(!self->self_test) DestroyWindow(window); return 0;
            }
            if(message==WM_CLOSE || (message==WM_COMMAND && LOWORD(word)==IDCANCEL)) {DestroyWindow(window);return 0;}
            if(message==WM_DESTROY) {PostQuitMessage(self->saved?0:1);return 0;}
        } catch(const std::exception& error) {
            const std::string reason=error.what(); self->error.assign(reason.begin(),reason.end());
            SetDlgItemTextW(window,ErrorText,self->error.c_str());
            if(message==WM_CREATE) return -1;
            return 0;
        }
        return DefWindowProcW(window,message,word,data);
    }
    void snapshot(const std::filesystem::path& path) {
        // Common controls skip text painting in an invisible ancestor. Render
        // this test-owned tool window outside the entire virtual desktop,
        // without activation or a taskbar entry, then hide it immediately.
        SetWindowLongPtrW(window,GWL_EXSTYLE,GetWindowLongPtrW(window,GWL_EXSTYLE)|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE);
        RECT frame{};GetWindowRect(window,&frame);
        SetWindowPos(window,nullptr,GetSystemMetrics(SM_XVIRTUALSCREEN)-(frame.right-frame.left)-100,
            GetSystemMetrics(SM_YVIRTUALSCREEN)-(frame.bottom-frame.top)-100,0,0,
            SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
        ShowWindow(window,SW_SHOWNOACTIVATE);UpdateWindow(window);
        RECT client{}; GetClientRect(window,&client);
        const int width=client.right,height=client.bottom;
        BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=height;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        void* pixels=nullptr;
        HDC dc=CreateCompatibleDC(nullptr);
        auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
        auto previous=SelectObject(dc,bitmap);
        const bool painted=PrintWindow(window,dc,PW_CLIENTONLY)!=FALSE;
        ShowWindow(window,SW_HIDE);
        if(!painted) {SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);
            throw std::runtime_error("Configuration capture failed");}
        BITMAPFILEHEADER header{};header.bfType=0x4D42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
        header.bfSize=header.bfOffBits+width*height*4;
        std::ofstream out(path,std::ios::binary);
        out.write(reinterpret_cast<const char*>(&header),sizeof(header));
        out.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));
        out.write(static_cast<const char*>(pixels),width*height*4);
        SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);
    }
    int run() {
        dpi=int(GetDpiForSystem());
        WNDCLASSEXW type{sizeof(type)};type.hInstance=GetModuleHandleW(nullptr);type.lpfnWndProc=procedure;
        type.lpszClassName=L"SARecompConfiguration";type.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
        type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);
        if(!RegisterClassExW(&type)) throw std::runtime_error("Could not register the configuration window");
        constexpr DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
        RECT bounds{0,0,px(590),px(774)};AdjustWindowRectEx(&bounds,style,FALSE,WS_EX_CONTROLPARENT);
        auto handle=CreateWindowExW(WS_EX_CONTROLPARENT,type.lpszClassName,L"Sonic Adventure: Recompiled - Configuration",style,
            CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,type.hInstance,this);
        if(!handle) throw std::runtime_error("Could not open the configuration window");
        if(self_test) {
            // Native controls and serialization are exercised without showing/focusing a window.
            if(!GetDlgItem(window,GameTiming)||!GetDlgItem(window,VSync))
                throw std::runtime_error("Timing or VSync control missing");
            SetDlgItemTextW(window,Resolution,L"2560 x 1080");
            SendDlgItemMessageW(window,Renderer,CB_SETCURSEL,1,0);
            SendDlgItemMessageW(window,WindowMode,CB_SETCURSEL,1,0);
            SendDlgItemMessageW(window,Aspect,CB_SETCURSEL,1,0);
            SendDlgItemMessageW(window,TextLanguage,CB_SETCURSEL,5,0);
            SendDlgItemMessageW(window,VoiceLanguage,CB_SETCURSEL,2,0);
            SendDlgItemMessageW(window,Subtitles,CB_SETCURSEL,2,0);
            SendDlgItemMessageW(window,CameraStyle,CB_SETCURSEL,1,0);
            SendDlgItemMessageW(window,GameTiming,CB_SETCURSEL,0,0);
            SendDlgItemMessageW(window,VSync,CB_SETCURSEL,1,0);
            SendMessageW(window,WM_COMMAND,IDOK,0);
            auto roundtrip=sonic::presentation::read_settings(path);
            if(!saved || !roundtrip.setup_complete || roundtrip.width!=2560 || roundtrip.height!=1080 ||
                roundtrip.renderer!=sonic::rendering::Renderer::Vulkan || roundtrip.window_mode!=sonic::rendering::WindowMode::Borderless ||
                roundtrip.text_language!=4 || roundtrip.voice_language!=1 || roundtrip.subtitles!=1 ||
                roundtrip.camera_style!=sonic::camera::Style::Recompiled ||
                roundtrip.presentation_fps!=60 || roundtrip.gameplay_timing!=0 || roundtrip.vsync!=1 || IsWindowVisible(window))
                throw std::runtime_error("Configuration control/save roundtrip failed");
            if(!GetEnvironmentVariableW(L"KATANA_PORT_BACKGROUND_TEST",nullptr,0))
                snapshot(path.parent_path()/"configuration.bmp");
            saved=false; SetDlgItemTextW(window,Resolution,L"-1 x 0");
            SendMessageW(window,WM_COMMAND,IDOK,0);
            if(saved || error.empty() || sonic::presentation::read_settings(path).width!=2560)
                throw std::runtime_error("Invalid configuration replaced a valid file");
            saved=true;DestroyWindow(window);return 0;
        }
        ShowWindow(window,SW_SHOWNORMAL);UpdateWindow(window);
        MSG message{};
        while(GetMessageW(&message,nullptr,0,0)>0)
            if(!IsDialogMessageW(window,&message)) {TranslateMessage(&message);DispatchMessageW(&message);}
        return saved?0:1;
    }
};
}

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    Dialog dialog;
    try {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
        int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        if(!argv) throw std::runtime_error("Could not read configuration arguments");
        for(int i=1;i<argc;++i) {
            const std::wstring_view arg(argv[i]);
            if(arg==L"--first-run") dialog.first_run=true;
            else if(arg==L"--self-test") dialog.self_test=true;
            else if(arg==L"--config" && i+1<argc) dialog.path=argv[++i];
            else {LocalFree(argv);throw std::runtime_error("Unknown configuration argument");}
        }
        LocalFree(argv);
        if(dialog.path.empty())dialog.path=sonic::presentation::configuration_path(executable);
        dialog.settings=sonic::presentation::read_settings(dialog.path);
        return dialog.run();
    } catch(const std::exception& error) {
        if(!dialog.self_test) MessageBoxA(nullptr,error.what(),"Sonic Adventure: Recompiled - Configuration",MB_OK|MB_ICONERROR);
        else {std::ofstream out(dialog.path.parent_path()/"configuration-error.txt");out<<error.what();}
        return 2;
    }
}
