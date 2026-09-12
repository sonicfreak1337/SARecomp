#pragma once
#include "sonic_presentation.hpp"
#include <atomic>
#include <charconv>
namespace sonic::audio {
enum class Bus {Master,Music,Voice,Effects};
inline std::atomic<bool> focused{true};
inline float factor(Bus bus) noexcept {
    const auto& s=presentation::settings();if(s.mute_background&&!focused.load(std::memory_order_relaxed))return 0;
    const auto bus_volume=bus==Bus::Music?s.music_volume:bus==Bus::Voice?s.voice_volume:bus==Bus::Effects?s.effects_volume:100u;
    return s.master_volume*.01f*bus_volume*.01f;
}
// Call only with a catalog-authenticated binding. The retail file-ADX lane
// contains the BGM/jingle catalog; spoken event lines use the two AFS owners.
inline Bus adx_bus(std::string_view path) noexcept {
    if(path.ends_with(".ADX"))return Bus::Music;
    if(path=="EVENT_ADX.AFS"||path=="EVENT_ADX_US.AFS")return Bus::Voice;
    return Bus::Master;
}
// MLT is a container, not a bus. Retail P_/V_ banks are kept separate inside
// the Chao containers; ALT relocates the voice bank from 6 to 1. Sky Deck's
// two localized announcements share bank 1 with its stage effects.
// These are authenticated asset/program identities, never guest addresses.
inline Bus program_bus(std::string_view id, unsigned bank, unsigned program) noexcept {
    constexpr std::string_view prefix="sa-pal-v1003-mlt-";
    if(!id.starts_with(prefix)||id.size()!=prefix.size()+3)return Bus::Master;
    unsigned n=999;const auto value=id.substr(prefix.size());const auto converted=std::from_chars(value.data(),value.data()+value.size(),n);
    if(converted.ec!=std::errc{}||converted.ptr!=value.data()+value.size()||n>121)return Bus::Master;
    if(bank==6)return Bus::Voice;
    if(bank==1&&((n>=13&&n<=14)||(n>=16&&n<=17)||(n>=19&&n<=20)))return Bus::Voice;
    if((n==107||n==108)&&bank==1&&(program==29||program==30))return Bus::Voice;
    return Bus::Effects;
}
}
