#pragma once
#include "katana/runtime/runtime.hpp"
#include <array>
#include <cstdint>

namespace sonic::motion {
struct Owner {
    std::uint32_t task=0,work=0,callback=0;
    friend bool operator==(const Owner&,const Owner&)=default;
};
inline thread_local Owner current_owner;
// These are the three resident scheduler calls, including child display
// traversal. R4 is the actual task argument AFTER the SH-4 delay slot.
// 8C1C1C80 instead stores a callback, not a task or instance identity.
class CallScope {
    Owner previous_{};
    bool changed_=false;
    static constexpr std::uint32_t physical(std::uint32_t address) noexcept {return address&0x1fffffff;}
public:
    static constexpr bool observes(std::uint32_t return_pc) noexcept {
        const auto pc=physical(return_pc);
        return pc==0x0c0986fa||pc==0x0c098768||pc==0x0c09879c;
    }
    CallScope(katana::runtime::CpuState& cpu,std::uint32_t target,bool enabled) noexcept {
        if(!enabled||!observes(cpu.pr))return;
        previous_=current_owner;changed_=true;current_owner={};
        try {
            const auto pc=physical(cpu.pr),task=physical(cpu.r[4]);
            if(task<0x0c000000||task>0x0cffffd0||(task&3))return;
            constexpr std::array<std::uint16_t,8> update{0xeb00,0x5e44,0x6c42,0x2ee8,0x8d02,0x2de2,0x4e0b,0x0009};
            constexpr std::array<std::uint16_t,8> display{0x2ee8,0x890d,0x5de5,0x2dd8,0x8d02,0x6ce2,0x4d0b,0x64e3};
            const auto& expected=pc==0x0c0986fa?update:display;
            for(unsigned i=0;i<expected.size();++i)
                if(cpu.memory.read_u16(pc-16+2*i)!=expected[i])return;
            const auto callback=cpu.memory.read_u32(task+(pc==0x0c0986fa?16:20));
            if(!target||physical(callback)!=physical(target))return;
            current_owner={task,physical(cpu.memory.read_u32(task+44)),physical(target)};
        }catch(...){current_owner={};} // Only annotation fails; guest execution is unchanged.
    }
    ~CallScope(){if(changed_)current_owner=previous_;}
    CallScope(const CallScope&)=delete;
};
}
