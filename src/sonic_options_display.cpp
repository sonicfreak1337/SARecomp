#include "katana/runtime/native_port.hpp"
#include "katana/runtime/runtime.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>

extern "C" katana::runtime::NativePortHookResult sonic_options_legacy_display(
    katana::runtime::NativePortContext& context) noexcept {
    using namespace katana::runtime;
    // ADVERTISE 808929BE / 8C9079BE is the Options task's display callback,
    // separately registered from update 8C9078E0 at 8C907A72/74. Suppress only
    // this rendering subtree for the whole task lifetime, including both fades.
    // Waiting for controller-ready 8 leaves the old screen visible in states
    // 1 and 3. Audio, transition, cleanup and the original Sound Test run on.
    // The manifest authenticates the exact source span and active module.
    if(!context.cpu)return {};
    try {
        auto& cpu=*context.cpu;
        const auto word=[&](std::uint32_t address){return cpu.memory.read_u32(canonical_physical_address(address));};
        const auto ram=[](std::uint32_t address,std::uint32_t bytes){
            const auto physical=canonical_physical_address(address);
            return !(address&3u)&&physical>=0x0C010000u&&physical<=0x0D000000u-bytes;
        };
        const auto task=cpu.r[4];
        if(!ram(task,48)||task!=word(0x8C9645D0u)||
            word(task+16)!=0x8C9078E0u||word(task+20)!=0x8C9079BEu)return {};
        const auto work=word(task+44);if(!ram(work,80))return {};
        static const bool trace=[](){const auto value=std::getenv("SARECOMP_OPTIONS_TRANSITION_TRACE");return value&&std::string_view(value)=="1";}();
        if(trace){
            static std::uint32_t previous=std::numeric_limits<std::uint32_t>::max();
            static unsigned reports=0;
            const auto state=word(work);
            if(state!=previous&&reports<32){previous=state;++reports;
                std::fprintf(stderr,"SONIC_OPTIONS legacy_display_suppressed=1 phase=%u frame=%llu\n",state,
                    static_cast<unsigned long long>(context.frame_index));}
        }
        return {NativePortHookAction::Return,0u,0u};
    }catch(...){return {};}
}
