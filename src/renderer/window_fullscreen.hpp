#pragma once
#include <windows.h>

namespace sonic::rendering {
inline constexpr wchar_t detached_menu_property[]=L"SonicRecompiled.DetachedMenu";
inline HMENU window_menu(HWND window) noexcept {
    const auto attached=GetMenu(window);
    return attached?attached:static_cast<HMENU>(GetPropW(window,detached_menu_property));
}
// Window-thread-owned borderless fullscreen. No display-mode change, focus
// request or visibility change; hidden renderer checks stay hidden.
class WindowFullscreen {
public:
    bool active() const noexcept {return active_;}
    bool toggle(HWND window) noexcept {
        constexpr UINT flags=SWP_FRAMECHANGED|SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER;
        if (!active_) {
            MONITORINFO monitor{sizeof(monitor)};
            if (!GetWindowRect(window,&bounds_) ||
                !GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor)) return false;
            style_=GetWindowLongPtrW(window,GWL_STYLE);
            extended_style_=GetWindowLongPtrW(window,GWL_EXSTYLE);
            menu_=GetMenu(window);
            // The Options extension attaches after renderer construction.
            // Keep the owned menu discoverable while fullscreen detaches it.
            if(menu_ && !SetPropW(window,detached_menu_property,menu_)) return false;
            const auto& r=monitor.rcMonitor;
            if (!set_style(window,GWL_STYLE,style_&~LONG_PTR(WS_OVERLAPPEDWINDOW)) ||
                !set_style(window,GWL_EXSTYLE,extended_style_&~LONG_PTR(WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE|WS_EX_DLGMODALFRAME|WS_EX_STATICEDGE)) ||
                !SetMenu(window,nullptr) ||
                !SetWindowPos(window,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,flags)) {
                restore(window,flags);
                return false;
            }
            active_=true;
        } else {
            if (!restore(window,flags)) return false;
            active_=false;
        }
        return true;
    }
    void attach_menu_before_destroy(HWND window) noexcept {
        // DestroyWindow owns attached menus; a hidden fullscreen menu otherwise leaks.
        if (active_ && menu_) SetMenu(window,menu_);
        RemovePropW(window,detached_menu_property);
    }
private:
    static bool set_style(HWND window,int index,LONG_PTR value) noexcept {
        SetLastError(0);
        return SetWindowLongPtrW(window,index,value)!=0 || GetLastError()==0;
    }
    bool restore(HWND window,UINT flags) noexcept {
        // Execute every restoration even when one fails.
        const bool style=set_style(window,GWL_STYLE,style_);
        const bool extended=set_style(window,GWL_EXSTYLE,extended_style_);
        const bool menu=SetMenu(window,menu_)!=FALSE;
        if(menu) RemovePropW(window,detached_menu_property);
        const bool bounds=SetWindowPos(window,nullptr,bounds_.left,bounds_.top,
            bounds_.right-bounds_.left,bounds_.bottom-bounds_.top,flags)!=FALSE;
        return style && extended && menu && bounds;
    }
    RECT bounds_{};
    LONG_PTR style_=0,extended_style_=0;
    HMENU menu_=nullptr;
    bool active_=false;
};
}
