#pragma once
#include "sonic_menu.hpp"
#include "sonic_menu_text.hpp"
#include "sonic_profiles.hpp"
#include <algorithm>
#include <cmath>
#include <utility>
namespace sonic::menu {
// Shared by the real modal teardown/observe path and its input regression test.
class InputReleaseGate {
    bool waiting_=false;
public:
    void reset() noexcept {waiting_=false;}
    void arm() noexcept {waiting_=true;}
    bool consume(const input::Snapshot& s) noexcept {
        const bool neutral=!s.pad && std::abs(s.move_x)<.2f && std::abs(s.move_y)<.2f &&
            std::none_of(s.mouse.begin()+1,s.mouse.end(),[](bool held){return held;}) &&
            std::none_of(s.keys.begin(),s.keys.end(),[](bool held){return held;});
        if(waiting_&&neutral)waiting_=false;
        return waiting_;
    }
};
inline std::vector<Choice> profile_choices(int language,std::string_view active) {
    std::vector<Choice> choices;
    for(const auto& entry:profiles::catalog()) {
        auto detail=std::wstring(text(entry.available?"empty_profile":"profile_unavailable",language));
        if(entry.preview) {
            const auto& p=*entry.preview;
            detail=std::to_wstring(p.bytes/1024)+L" KiB · #"+std::to_wstring(p.generation)+L"\n";
            for(const auto& f:p.files) {
                if(detail.back()!=L'\n')detail+=L" + ";
                detail+=f=="SONICADV_ALF"?L"Chao":f=="SONICADV_INT"?std::wstring(text("story_data",language)):std::wstring(f.begin(),f.end());
            }
        }
        if(entry.id==active)detail=std::wstring(text("active",language))+L" · "+detail;
        choices.push_back({entry.id,entry.id=="default"?std::wstring(text("default_profile",language)):
            std::wstring(entry.id.begin(),entry.id.end()),std::move(detail),entry.available});
    }
    return choices;
}
}
