#pragma once
#include "katana/runtime/runtime.hpp"
#include <optional>
#include <utility>

namespace sonic {
// Host-only state for renderer FPU calculations. Memory{0} still constructs a
// 256-KiB address lookup table. Retain that empty table between model draws,
// but reset every CPU field exactly as a fresh aggregate would. No guest
// memory, mappings or cached floating-point results belong in this slot.
class FpuScratch {
    std::optional<katana::runtime::CpuState> cpu_;
    bool leased_=false;
public:
    class Lease {
        FpuScratch* owner_=nullptr;
        std::optional<katana::runtime::CpuState> nested_;
        katana::runtime::CpuState* cpu_=nullptr;
    public:
        Lease(FpuScratch& slot,std::uint32_t fpscr) {
            using namespace katana::runtime;
            if(slot.leased_) {
                // A reentrant caller must not reset its parent's live FPU.
                nested_.emplace(CpuState{.memory=Memory{0u,MemoryAlignmentPolicy::Permissive}});
                cpu_=&*nested_;
            } else {
                if(!slot.cpu_)
                    slot.cpu_.emplace(CpuState{.memory=Memory{0u,MemoryAlignmentPolicy::Permissive}});
                else {
                    auto memory=std::move(slot.cpu_->memory);
                    *slot.cpu_=CpuState{.memory=std::move(memory)};
                }
                slot.leased_=true;owner_=&slot;cpu_=&*slot.cpu_;
            }
            cpu_->fpscr=fpscr;
        }
        ~Lease(){if(owner_)owner_->leased_=false;}
        Lease(const Lease&)=delete;
        Lease& operator=(const Lease&)=delete;
        katana::runtime::CpuState& get() noexcept{return *cpu_;}
    };
};
}
