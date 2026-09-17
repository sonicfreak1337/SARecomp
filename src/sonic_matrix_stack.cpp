#include "sonic_matrix_stack.hpp"
#include "sonic_native_model_memory.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include <array>
#include <bit>
#include <cstring>

namespace sonic::matrix_stack {
namespace {
using namespace katana::runtime;
constexpr std::uint32_t capacity_address=0x8C88F5D8u, depth_address=0x8C88F5DCu;
constexpr std::uint32_t pointer_address=0x8C88F538u;
// Complete little-endian retail span including both literal islands.
constexpr std::array<std::uint16_t,64> original_words{
    0xD103u,0xD604u,0x6312u,0x6262u,0x3233u,0x8B05u,0x000Bu,0xE000u,
    0xF5D8u,0x8C88u,0xF5DCu,0x8C88u,0x7201u,0xD112u,0x2622u,0x6512u,
    0x2448u,0x05C3u,0x7520u,0x05C3u,0x7520u,0xF3FDu,0xF5FBu,0xF5DBu,
    0xF5BBu,0xF59Bu,0xF57Bu,0xF55Bu,0xF53Bu,0xF51Bu,0xF3FDu,0x7540u,
    0x8F12u,0x2152u,0x05C3u,0x7520u,0x05C3u,0x7520u,0xF3FDu,0xF5FBu,0xF5DBu,
    0xF5BBu,0xF59Bu,0xF57Bu,0xF55Bu,0xF53Bu,0xF51Bu,0xF3FDu,
    0x000Bu,0xE001u,0xF538u,0x8C88u,0xF3FDu,0xF149u,0xF349u,0xF549u,
    0xF749u,0xF949u,0xFB49u,0xFD49u,0xFF49u,0xF3FDu,0x000Bu,0xE001u,
};
constexpr std::array<std::uint16_t,32> pop_words{
    0xD503u,0xE201u,0x6352u,0x3348u,0x3322u,0x8903u,0x000Bu,0xE000u,
    0xF5DCu,0x8C88u,0x4408u,0x2532u,0x4408u,0xD508u,0x4408u,0x6152u,
    0x3148u,0x2512u,0xF3FDu,0xF119u,0xF319u,0xF519u,0xF719u,0xF919u,
    0xFB19u,0xFD19u,0xFF19u,0xF3FDu,0x000Bu,0xE001u,0xF538u,0x8C88u,
};
struct Range { std::uint32_t address,size; };
bool overlaps(Range a,Range b) noexcept {
    const auto x=a.address&0x1FFFFFFFu,y=b.address&0x1FFFFFFFu;
    return x<std::uint64_t(y)+b.size && y<std::uint64_t(x)+a.size;
}
bool admitted(const DirectLinearMemoryGuard& g,bool p0,Range r) noexcept {
    const auto p=r.address&0x1FFFFFFFu;
    const bool segment=(r.address&0xC0000000u)==0x80000000u ||
        (p0 && r.address>=0x0C000000u && r.address<0x0D000000u);
    return segment && !(r.address&3u) && r.size && p>=0x0C000000u &&
        p<0x0D000000u && r.size<=0x0D000000u-p && g &&
        g.physical_base==0x0C000000u && g.physical_span>=0x01000000u &&
        g.backing_mask==0x00FFFFFFu;
}
std::uint32_t peek(const DirectLinearMemoryGuard& g,std::uint32_t a) noexcept {
    std::uint32_t v; std::memcpy(&v,g.read_bytes+(a&0xFFFFFFu),4u); return v;
}
} // namespace

bool bulk_enabled() noexcept {
    static const bool value=[] {
        const char* option=std::getenv("SARECOMP_NATIVE_MATRIX_BULK");
        return option && std::strcmp(option,"1")==0 &&
            sonic::native_cpu::enabled("SARECOMP_NATIVE_MATRIX_BULK");
    }();
    return value;
}

static bool try_push(katana::runtime::CpuState& cpu,
                 const katana::runtime::NativePortImmutableWriteGuard* immutable_guard,
                 bool batch_stores,std::uint64_t* staged_groups) {
    using namespace katana::runtime;
    static_assert(std::endian::native==std::endian::little);
    // No arithmetic is executed: payload NaNs, DN, RM and exception enables
    // are preserved as raw bits. Initial PR/SZ and FD remain conservative exits.
    if (!immutable_guard || immutable_guard->write_detected() || cpu.pc!=push_entry ||
        !cpu.privileged_mode_inline() || cpu.trap_pending || cpu.sleeping ||
        (cpu.sr&sr_fd_mask) || (cpu.read_fpscr()&(fpscr_pr_mask|fpscr_sz_mask))) return false;
    auto& memory=cpu.memory;
    if (memory.watchpoint_count() || memory.has_trace_handler() || memory.has_guest_memory_access_sink() ||
        memory.has_mmio_trace_handler() || !memory.guest_write_observer_allows_prevalidated_linear_writes())
        return false;
    const auto g=memory.direct_linear_memory_guard(false);
    const bool p0=!(cpu.mmucr&1u) &&
        (!cpu.address_space || cpu.address_space->mode()==AddressTranslationMode::NoMmu);
    const Range code{push_entry,push_size}, capacity{capacity_address,4u}, depth{depth_address,4u};
    if (!admitted(g,p0,code) || !admitted(g,p0,capacity) || !admitted(g,p0,depth) ||
        std::memcmp(g.read_bytes+0x639BB0u,original_words.data(),push_size)) return false;
    const auto limit=peek(g,capacity_address), level=peek(g,depth_address);
    const bool full=std::bit_cast<std::int32_t>(level)>=std::bit_cast<std::int32_t>(limit);
    if (!full) {
        const Range pointer{pointer_address,4u};
        if (!admitted(g,p0,pointer)) return false;
        const auto current=peek(g,pointer_address);
        const Range matrices{current,cpu.r[4] ? 64u : 128u};
        const Range input{cpu.r[4],64u};
        const std::array writes{depth,pointer,matrices};
        for (std::size_t i=0;i<writes.size();++i) {
            const auto w=writes[i];
            if (!admitted(g,p0,w) || immutable_guard->tracks_address(w.address&0x1FFFFFFFu,w.size) ||
                !memory.is_writable_linear_range(w.address&0x1FFFFFFFu,w.size,false) ||
                overlaps(w,code) || overlaps(w,capacity)) return false;
            for (std::size_t j=0;j<i;++j) if (overlaps(w,writes[j])) return false;
            if (cpu.r[4] && (!admitted(g,p0,input) || overlaps(w,input))) return false;
        }
        // The depth and stack-pointer words intentionally read/modify/write
        // themselves. Every unrelated source/destination overlap is excluded;
        // input/input or input/code aliasing is harmless and remains admitted.
    }
    // No original fallback can occur after this point.
    sonic::model_memory::ClosedLeafWrites native_writes(cpu,*immutable_guard,g);
    const auto load=[&](std::uint32_t a) {
        std::uint32_t v=0u;
        (void)direct_linear_guard_read_u32(g,(a&0x1FFFFFFFu)|0x80000000u,v);
        return v;
    };
    const auto store=[&](std::uint32_t pc,std::uint32_t a,std::uint32_t v,CodeWriteSource source) {
        if(native_writes.try_store(a,v,source))return;
        if (!memory.try_write_direct_linear_u32(a&0x1FFFFFFFu,v,source))
            guest_write_u32_at(cpu,GuestInstructionOrigin{pc,pc,true},a,v,source);
    };
    cpu.r[1]=capacity_address; cpu.r[6]=depth_address;
    cpu.r[3]=load(cpu.r[1]); cpu.r[2]=load(cpu.r[6]); cpu.t=full;
    if (cpu.t) { cpu.r[0]=0u; cpu.pc=cpu.pr; return true; }
    ++cpu.r[2]; cpu.r[1]=pointer_address;
    store(0x8C639BCCu,cpu.r[6],cpu.r[2],CodeWriteSource::Cpu);
    cpu.r[5]=load(cpu.r[1]); cpu.t=cpu.r[4]==0u;
    if(native_writes.direct() && bulk_enabled()) {
        // In scalar entry mode, each odd-indexed 64-bit FMOV addresses XF.
        // Descending pair stores produce XF[0..15] in ascending RAM order.
        // Complete preflight already excludes source/control/code aliases.
        (void)native_writes.try_store_matrix_snapshot(cpu.r[5],cpu.xf);
        ++bulk_counts.saved;
        cpu.r[5]+=64u;
        store(0x8C639BF2u,cpu.r[1],cpu.r[5],CodeWriteSource::Cpu);
        if(cpu.t) {
            (void)native_writes.try_store_matrix_snapshot(cpu.r[5],cpu.xf);
            ++bulk_counts.saved;
        } else {
            if(!direct_linear_guard_read_u32_group(g,(cpu.r[4]&0x1FFFFFFFu)|0x80000000u,cpu.xf))
                throw std::runtime_error("native matrix: admitted input changed");
            cpu.r[4]+=64u;
            ++bulk_counts.loaded;
        }
        // Two FSCHG writes restore SZ and clear reserved FPSCR bits. Raw copy
        // preserves payloads; no host floating-point arithmetic occurs.
        cpu.fpscr&=fpscr_writable_mask;
        ++bulk_counts.pushes;
        cpu.r[0]=1u;cpu.pc=cpu.pr;return true;
    }
    // MOVCA.L is a scalar StoreQueue-source write in the unchanged retained
    // executor/AOT. Each pair store emits low then high Fpu-source events,
    // before committing R5, exactly like FmovStorePreDecrement in that executor.
    const auto save_matrix_using=[&](std::uint32_t movca_pc,std::uint32_t pair_pc,auto&& write) {
        write(movca_pc,cpu.r[5],cpu.r[0],CodeWriteSource::StoreQueue); cpu.r[5]+=32u;
        write(movca_pc+4u,cpu.r[5],cpu.r[0],CodeWriteSource::StoreQueue); cpu.r[5]+=32u;
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask);
        for (unsigned i=0;i<8u;++i) {
            const auto bits=read_fpu_pair_bits(cpu,static_cast<std::uint8_t>(15u-2u*i));
            const auto address=cpu.r[5]-8u;
            write(pair_pc+2u*i,address,static_cast<std::uint32_t>(bits),CodeWriteSource::Fpu);
            write(pair_pc+2u*i,address+4u,static_cast<std::uint32_t>(bits>>32u),CodeWriteSource::Fpu);
            cpu.r[5]=address;
        }
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask);
    };
    const auto save_matrix=[&](std::uint32_t movca_pc,std::uint32_t pair_pc) {
        if (native_writes.direct() || !batch_stores || (!memory.has_guest_write_batch_observer() && memory.has_guest_write_observer())) {
            // Avoid initializing the SDK's two fixed arrays when a known
            // scalar-only observer already makes batch admission impossible.
            save_matrix_using(movca_pc,pair_pc,store);
            return;
        }
        // Exactly 18 stores fit the SDK's 32-entry batch. Keep separate saves:
        // this bounds its quadratic staged-overlap scan and never stages the
        // intervening pointer store/control transfer. All output ranges were
        // proven writable and nonexecutable before the first guest mutation.
        // There are only register transfers between these writes, no guest
        // reads or host/device calls. The SDK keeps each source/changed/order,
        // including MOVCA words overwritten by the following paired FMOVs.
        auto batch=memory.begin_direct_linear_write_batch();
        bool staged_any=false;
        const auto write=[&](std::uint32_t pc,std::uint32_t a,std::uint32_t v,CodeWriteSource source) {
            if (batch && batch.try_stage_u32(a&0x1FFFFFFFu,v,source)) {
                staged_any=true;
                return;
            }
            // A staging miss is mutation-free. Publish the accepted prefix
            // before entering the scalar fallback, never after that store.
            batch.flush();
            store(pc,a,v,source);
        };
        save_matrix_using(movca_pc,pair_pc,write);
        // flush() handles batch-observer rejection by exact scalar replay.
        // Its destructor also flushes on host unwind; no staged state escapes.
        batch.flush();
        // Count actual staging only after the Save group has published. The
        // SDK deliberately does not expose whether flush used span admission
        // or scalar replay; this witness makes no optimized-commit claim.
        if (staged_any && staged_groups) ++*staged_groups;
    };
    save_matrix(0x8C639BD2u,0x8C639BDCu);
    cpu.r[5]+=64u;
    store(0x8C639BF2u,cpu.r[1],cpu.r[5],CodeWriteSource::Cpu); // BF/S delay slot
    if (cpu.t) {
        save_matrix(0x8C639BF4u,0x8C639BFEu);
    } else {
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask);
        for (unsigned i=0;i<8u;++i) {
            const auto low=load(cpu.r[4]),high=load(cpu.r[4]+4u);
            write_fpu_pair_bits(cpu,static_cast<std::uint8_t>(1u+2u*i),std::uint64_t(low)|(std::uint64_t(high)<<32u));
            cpu.r[4]+=8u;
        }
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask);
    }
    cpu.r[0]=1u; cpu.pc=cpu.pr; return true;
}

static bool try_pop(katana::runtime::CpuState& cpu,
                    const katana::runtime::NativePortImmutableWriteGuard* immutable_guard) {
    using namespace katana::runtime;
    if (!immutable_guard || immutable_guard->write_detected() || cpu.pc!=pop_entry ||
        !cpu.privileged_mode_inline() || cpu.trap_pending || cpu.sleeping ||
        (cpu.sr&sr_fd_mask) || (cpu.read_fpscr()&(fpscr_pr_mask|fpscr_sz_mask))) return false;
    auto& memory=cpu.memory;
    if (memory.watchpoint_count() || memory.has_trace_handler() || memory.has_guest_memory_access_sink() ||
        memory.has_mmio_trace_handler() || !memory.guest_write_observer_allows_prevalidated_linear_writes())
        return false;
    const auto g=memory.direct_linear_memory_guard(false);
    const bool p0=!(cpu.mmucr&1u) &&
        (!cpu.address_space || cpu.address_space->mode()==AddressTranslationMode::NoMmu);
    const Range code{pop_entry,pop_size},depth{depth_address,4u},pointer{pointer_address,4u};
    if (!admitted(g,p0,code) || !admitted(g,p0,depth) ||
        std::memcmp(g.read_bytes+0x639AD8u,pop_words.data(),pop_size)) return false;
    // SUB wraps in 32 bits; CMP/HS with 1 is unsigned. Do not introduce a
    // positive-count/depth restriction absent from this original SDK leaf.
    const std::uint32_t remaining=peek(g,depth_address)-cpu.r[4];
    if (remaining!=0u) {
        if (!admitted(g,p0,pointer)) return false;
        const Range matrix{peek(g,pointer_address)-(cpu.r[4]<<6u),64u};
        if (!admitted(g,p0,matrix)) return false;
        const std::array writes{depth,pointer};
        for (std::size_t i=0;i<writes.size();++i) {
            const auto w=writes[i];
            if (!admitted(g,p0,w) || immutable_guard->tracks_address(w.address&0x1FFFFFFFu,w.size) ||
                !memory.is_writable_linear_range(w.address&0x1FFFFFFFu,w.size,false) ||
                overlaps(w,code) || overlaps(w,matrix)) return false;
            for (std::size_t j=0;j<i;++j) if (overlaps(w,writes[j])) return false;
        }
        // The two original global RMW words are distinct. Matrix/code read
        // aliasing remains valid, but matrix data cannot alias either writer.
    }
    sonic::model_memory::ClosedLeafWrites native_writes(cpu,*immutable_guard,g);
    const auto load=[&](std::uint32_t a) {
        std::uint32_t v=0u;
        (void)direct_linear_guard_read_u32(g,(a&0x1FFFFFFFu)|0x80000000u,v);
        return v;
    };
    const auto store=[&](std::uint32_t pc,std::uint32_t a,std::uint32_t v) {
        if(native_writes.try_store(a,v,CodeWriteSource::Cpu))return;
        if (!memory.try_write_direct_linear_u32(a&0x1FFFFFFFu,v,CodeWriteSource::Cpu))
            guest_write_u32_at(cpu,GuestInstructionOrigin{pc,pc,true},a,v,CodeWriteSource::Cpu);
    };
    cpu.r[5]=depth_address; cpu.r[2]=1u; cpu.r[3]=load(cpu.r[5]);
    cpu.r[3]-=cpu.r[4]; cpu.t=cpu.r[3]>=cpu.r[2];
    if (!cpu.t) { cpu.r[0]=0u; cpu.pc=cpu.pr; return true; }
    cpu.r[4]<<=2u;
    store(0x8C639AEEu,cpu.r[5],cpu.r[3]);
    cpu.r[4]<<=2u; cpu.r[5]=pointer_address; cpu.r[4]<<=2u;
    cpu.r[1]=load(cpu.r[5]); cpu.r[1]-=cpu.r[4];
    store(0x8C639AFAu,cpu.r[5],cpu.r[1]);
    if(native_writes.direct() && bulk_enabled()) {
        if(!direct_linear_guard_read_u32_group(g,(cpu.r[1]&0x1FFFFFFFu)|0x80000000u,cpu.xf))
            throw std::runtime_error("native matrix: admitted stack changed");
        cpu.r[1]+=64u;
        cpu.fpscr&=fpscr_writable_mask;
        ++bulk_counts.pops;++bulk_counts.loaded;
        cpu.r[0]=1u;cpu.pc=cpu.pr;return true;
    }
    cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask);
    for (unsigned i=0;i<8u;++i) {
        const auto low=load(cpu.r[1]),high=load(cpu.r[1]+4u);
        write_fpu_pair_bits(cpu,static_cast<std::uint8_t>(1u+2u*i),std::uint64_t(low)|(std::uint64_t(high)<<32u));
        cpu.r[1]+=8u;
    }
    cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask);
    cpu.r[0]=1u; cpu.pc=cpu.pr; return true;
}

bool try_execute(katana::runtime::CpuState& cpu,
                 const katana::runtime::NativePortImmutableWriteGuard* immutable_guard,
                 bool batch_stores,std::uint64_t* staged_groups) {
    if (cpu.pc==push_entry) return try_push(cpu,immutable_guard,batch_stores,staged_groups);
    if (cpu.pc==pop_entry) return try_pop(cpu,immutable_guard);
    return false;
}
} // namespace sonic::matrix_stack
