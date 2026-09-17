#include "sonic_collision_math.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include "test_collision_memory_support.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <tuple>

using namespace katana::runtime;
namespace cm = sonic::collision_math;
namespace {
constexpr auto returned = 0x8CF80000u;
constexpr std::array entries{cm::cross_entry,cm::length_entry,cm::normalize_entry};
constexpr std::array sizes{cm::cross_size,cm::length_size,cm::normalize_size};
constexpr std::array hashes{cm::cross_source_sha256,cm::length_source_sha256,cm::normalize_source_sha256};
constexpr std::array code_ranges{
    NativePortImmutableRange{cm::cross_entry&0x1FFFFFFFu,cm::cross_size,
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
    NativePortImmutableRange{cm::length_entry&0x1FFFFFFFu,cm::length_size,
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
    NativePortImmutableRange{cm::normalize_entry&0x1FFFFFFFu,cm::normalize_size,
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
};
void require(bool value,const char* why) { if (!value) throw std::runtime_error(why); }
struct Services final : PlatformServices {
    std::string_view name() const noexcept override { return "collision-math-byte-reference"; }
    std::uint32_t abi_version() const noexcept override { return 128u; }
    std::uint32_t guest_cycle_contract() const noexcept override { return 0u; }
    PlatformCapabilities capabilities() const noexcept override { return {}; }
    void read_memory(std::uint32_t,std::span<std::uint8_t>) override { throw std::runtime_error("device read"); }
    void write_memory(std::uint32_t,std::span<const std::uint8_t>) override { throw std::runtime_error("device write"); }
    std::uint64_t scheduler_cycle() const noexcept override { return 0u; }
    std::optional<std::uint64_t> next_scheduler_event_cycle() const noexcept override { return {}; }
    PlatformSchedulerResult consume_guest_cycles(std::uint64_t,std::size_t) override { return {}; }
    std::optional<PlatformInterruptRequest> poll_interrupt() override { return {}; }
    PlatformDmaResult start_dma(const PlatformDmaRequest&) override { throw std::runtime_error("DMA"); }
    PlatformFallbackResult controlled_fallback(CpuState&,const PlatformFallbackRequest&) override {
        throw std::runtime_error("executor fallback");
    }
    bool prefetch(CpuState&,GuestInstructionOrigin,std::uint32_t) override { throw std::runtime_error("PREF"); }
} services;
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream f(path,std::ios::binary); require(bool(f),"boot.bin missing");
    return {std::istreambuf_iterator<char>(f),{}};
}
std::string digest(std::span<const std::uint8_t> bytes) {
#ifndef _WIN32
    return native_port_content_sha256(bytes);
#else
    std::array<unsigned char,32> result{};
    require(bytes.size()<=ULONG_MAX && BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
        const_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),result.data(),ULONG(result.size()))>=0,"SHA-256 failed");
    constexpr char hex[]="0123456789abcdef"; std::string text(64,'0');
    for (std::size_t i=0;i<result.size();++i) { text[i*2]=hex[result[i]>>4]; text[i*2+1]=hex[result[i]&15]; }
    return text;
#endif
}
// Instruction/cycle/provenance bookkeeping belongs to native-hook integration.
auto architecture(const CpuState& c) {
    return std::tuple(c.r,c.r_bank,c.fr,c.xf,c.pc,c.pr,c.gbr,c.vbr,c.ssr,c.spc,c.sgr,c.dbr,
        c.tra,c.tea,c.expevt,c.intevt,c.pteh,c.ptel,c.ptea,c.ttb,c.mmucr,c.mach,c.macl,c.fpul,
        c.read_fpscr(),c.sr,c.t,c.s,c.q,c.m,c.trap_pending,c.exception_generation,
        c.last_exception_cause,c.sleeping,c.prefetch_count,c.tlb_load_count);
}
// Store order and callback-visible pointers matter, particularly normalize's
// reverse stores, which commit R4 only AFTER each notification.
using Write = std::tuple<std::uint32_t,std::size_t,CodeWriteSource,bool,std::uint32_t,
    std::array<std::uint32_t,5>,std::uint32_t>;
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    NativePortImmutableWriteGuard immutable{code_ranges};
    std::vector<Write> events;
    std::string label;
    Fixture(std::span<const std::uint8_t> boot,unsigned leaf,std::uint32_t mode=fpscr_dn_mask,
            bool read_only=false) {
        cpu.memory.map_region("main-ram",0x0C000000u,ram,
            read_only?MemoryRegionAccess::ReadOnly:MemoryRegionAccess::ReadWrite);
        // The SDK only permits a direct alias window over fully writable RAM.
        // A read-only fixture must remain a normal mapped region; admission
        // then declines without touching it. Binding first would throw before
        // this negative test can exercise the leaf at all.
        if (!read_only)
            cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        auto bytes=ram->writable_bytes(); std::fill(bytes.begin(),bytes.end(),0xCDu);
        std::copy(boot.begin(),boot.end(),bytes.begin()+0x10000u);
        cpu.pc=entries[leaf]; cpu.pr=returned; cpu.gbr=0x8CE80000u;
        cpu.write_sr(sr_md_mask); cpu.write_fpscr(mode);
        cpu.t=true; cpu.s=true; cpu.q=true; cpu.m=true;
        cpu.fpul=0xDEADBEEFu; cpu.mach=0x12345678u; cpu.macl=0x87654321u;
        for (unsigned i=0;i<16u;++i) {
            cpu.r[i]=0xABCD1200u+i; cpu.fr[i]=0xCAFE0000u+i; cpu.xf[i]=0x7FA00000u+i;
        }
        for (unsigned i=0;i<8u;++i) cpu.r_bank[i]=0x11220000u+i;
        for (unsigned i=4u;i<=7u;++i) cpu.r[i]=0x8CE00000u+(i-4u)*0x100u;
        cpu.r[15]=0x8CF00000u;
        vector(cpu.r[4],{0x3F800000u,0x40000000u,0x40400000u});
        vector(cpu.r[5],{0xC0800000u,0x40A00000u,0x40C00000u});
        vector(cpu.r[6],{0x40E00000u,0xC1000000u,0x41100000u});
        label="leaf="+std::to_string(leaf)+" fpscr="+std::to_string(mode);
        events.reserve(16u);
    }
    void put(std::uint32_t a,std::uint32_t v) {
        auto b=ram->writable_bytes(); std::memcpy(b.data()+(a&0xFFFFFFu),&v,4u);
    }
    std::uint32_t peek(std::uint32_t a) const {
        std::uint32_t v; std::memcpy(&v,ram->bytes().data()+(a&0xFFFFFFu),4u); return v;
    }
    void vector(std::uint32_t a,std::array<std::uint32_t,3> values) {
        for (unsigned i=0;i<3u;++i) put(a+i*4u,values[i]);
    }
    void observe(bool stable=true) {
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e) noexcept {
            events.emplace_back(e.address,e.size,e.source,e.bytes_changed,peek(e.address),
                std::array{cpu.r[0],cpu.r[4],cpu.r[5],cpu.r[6],cpu.r[7]},cpu.read_fpscr());
            immutable.observe_write(e);
        },stable?GuestWriteObserverContract::StableForPrevalidatedLinearWrites:GuestWriteObserverContract::General);
    }
    void bind_mmu(bool active) {
        cpu.address_space=std::make_shared<RuntimeAddressSpace>();
        cpu.address_space->write_mmucr(cpu.mmucr);
        cpu.address_space->set_mode(active?AddressTranslationMode::Mmu:AddressTranslationMode::NoMmu);
    }
};
void execute_reference(CpuState& cpu,unsigned leaf) {
    // Untouched original executor, fetching the installed SHA-bound SH4 words.
    // No synthetic instruction substitute or FTRC reference repair is involved.
    for (unsigned n=0;cpu.pc!=returned && n<100u;++n) {
        require(cpu.pc>=entries[leaf] && cpu.pc<entries[leaf]+sizes[leaf],"reference left SHA-bound leaf");
        (void)execute_dynamic_sh4_block(cpu,services,1u);
        require(cpu.exception_generation==0u,"reference exception");
    }
    require(cpu.pc==returned,"reference did not return");
}
void compare(Fixture& n,Fixture& r,unsigned leaf) {
    collision_test::Comparison observers(n,r);
    const bool closed=observers.product() && sonic::collision_memory::closure_enabled();
    bool completed=false;
    if(closed){
        sonic::collision_memory::Access access;
        access.capture(n.cpu,n.immutable,n.cpu.memory.direct_linear_memory_guard(false),true);
        const std::array writes{cm::ClosedWriteRange{n.cpu.r[leaf==0u?7u:4u],12u}};
        const bool p0=!(n.cpu.mmucr&1u) && (!n.cpu.address_space ||
            n.cpu.address_space->mode()==AddressTranslationMode::NoMmu);
        const HostFpuExecutionEpoch epoch(n.cpu);
        completed=cm::try_execute_closed(n.cpu,n.cpu.pc,access,p0,writes);
    }else completed=cm::try_execute(n.cpu,&n.immutable);
    if (!completed) {
        std::cerr<<n.label<<'\n'; throw std::runtime_error("eligible leaf declined");
    }
    execute_reference(r.cpu,leaf);
    if (architecture(n.cpu)!=architecture(r.cpu)) {
        std::cerr<<n.label<<'\n'<<std::hex;
        for (unsigned i=0;i<16u;++i) {
            if (n.cpu.r[i]!=r.cpu.r[i]) std::cerr<<"r"<<i<<' '<<n.cpu.r[i]<<'/'<<r.cpu.r[i]<<'\n';
            if (n.cpu.fr[i]!=r.cpu.fr[i]) std::cerr<<"fr"<<i<<' '<<n.cpu.fr[i]<<'/'<<r.cpu.fr[i]<<'\n';
            if (n.cpu.xf[i]!=r.cpu.xf[i]) std::cerr<<"xf"<<i<<' '<<n.cpu.xf[i]<<'/'<<r.cpu.xf[i]<<'\n';
        }
        std::cerr<<"fpscr "<<n.cpu.read_fpscr()<<'/'<<r.cpu.read_fpscr()<<std::dec<<'\n';
        throw std::runtime_error("architectural state differs");
    }
    require(std::equal(n.ram->bytes().begin(),n.ram->bytes().end(),r.ram->bytes().begin()),"RAM/CPU-stack differs");
    if (!observers.product() && n.events!=r.events) {
        std::cerr<<n.label<<" native stores="<<n.events.size()<<" reference stores="<<r.events.size()<<'\n';
        throw std::runtime_error("ordered stores/value/source/changed/pointers/FPSCR differ");
    }
    observers.verify(leaf!=1u,closed);
    require(!n.immutable.write_detected() && !r.immutable.write_detected(),"protected code written");
    if (n.cpu.address_space && r.cpu.address_space)
        require(n.cpu.address_space->snapshot()==r.cpu.address_space->snapshot(),"MMU state differs");
}
void decline(Fixture& f,const NativePortImmutableWriteGuard* guard) {
    const auto before=architecture(f.cpu);
    const auto ram=std::vector<std::uint8_t>(f.ram->bytes().begin(),f.ram->bytes().end());
    const auto events=f.events; const auto metrics=f.cpu.memory.performance_counters();
    const auto translation=f.cpu.address_space?std::optional{f.cpu.address_space->snapshot()}:std::nullopt;
    require(!cm::try_execute(f.cpu,guard),"unsafe leaf admitted");
    require(before==architecture(f.cpu) && events==f.events &&
        std::equal(ram.begin(),ram.end(),f.ram->bytes().begin()),"decline mutated guest");
    const auto after=f.cpu.memory.performance_counters();
    require(metrics.indexed_region_hits==after.indexed_region_hits && metrics.reference_region_probes==after.reference_region_probes &&
        metrics.observed_accesses==after.observed_accesses && metrics.unobserved_accesses==after.unobserved_accesses,"decline changed metrics");
    require(!translation || *translation==f.cpu.address_space->snapshot(),"decline changed MMU");
}
unsigned closed_declines(std::span<const std::uint8_t> boot) {
    if(collision_test::mode()!=1u || !sonic::collision_memory::closure_enabled())return 0;
    for(unsigned kind=0;kind<12u;++kind){
        Fixture n(boot,0u),r(boot,0u);collision_test::Comparison observers(n,r);
        sonic::collision_memory::Access access;
        access.capture(n.cpu,n.immutable,n.cpu.memory.direct_linear_memory_guard(false),true);
        require(access.direct(),"closed negative fixture not authenticated");
        std::array writes{cm::ClosedWriteRange{n.cpu.r[7],12u}};
        auto target=cm::cross_entry;bool p0=true;
        switch(kind){
        case 0:n.cpu.r[4]+=2u;break;
        case 1:n.cpu.r[4]=0x8CFFFFF8u;break;
        case 2:n.cpu.r[5]=0x0D000000u;break;
        case 3:n.cpu.r[6]=0xCC000000u;break;
        case 4:n.cpu.r[7]+=4u;break;
        case 5:writes[0].size=8u;break;
        case 6:p0=false;n.cpu.r[4]&=0x1FFFFFFFu;break;
        case 7:target+=2u;break;
        case 8:access.reset();break;
        case 9:writes[0].size=0u;break;
        case 10:n.cpu.r[7]=0x8CFFFFF8u;break;
        case 11:target=cm::length_entry;n.cpu.r[4]=0xEC000000u;break;
        }
        const auto before=architecture(n.cpu);
        const auto bytes=std::vector<std::uint8_t>(n.ram->bytes().begin(),n.ram->bytes().end());
        const auto metrics=n.cpu.memory.performance_counters();
        const HostFpuExecutionEpoch epoch(n.cpu);
        require(!cm::try_execute_closed(n.cpu,target,access,p0,writes),"unsafe closed child admitted");
        access.reset();
        require(before==architecture(n.cpu) &&
            std::equal(bytes.begin(),bytes.end(),n.ram->bytes().begin()),"closed decline changed guest");
        const auto after=n.cpu.memory.performance_counters();
        require(metrics.indexed_region_hits==after.indexed_region_hits &&
            metrics.unobserved_accesses==after.unobserved_accesses,"closed decline accessed RAM");
        collision_test::bridge_boundary();
    }
    return 12u;
}
} // namespace
int main(int argc,char** argv) {
    try {
        require(argc==2,"usage: test_collision_math <installed-content-root>");
        const auto boot=read(std::filesystem::path(argv[1])/"boot.bin");
        require(boot.size()==6735296u && digest(boot)=="b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af","boot SHA differs");
        for (unsigned i=0;i<3u;++i)
            require(digest(std::span<const std::uint8_t>(boot).subspan(entries[i]-0x8C010000u,sizes[i]))==hashes[i],"leaf SHA differs");
        unsigned cases=0; std::array<unsigned,3> counts{};
        constexpr std::array modes{fpscr_dn_mask,fpscr_dn_mask|1u,
            fpscr_dn_mask|fpscr_fr_mask,fpscr_dn_mask|fpscr_fr_mask|1u|fpscr_flag_mask|fpscr_cause_mask};
        constexpr std::array special{0u,0x80000000u,1u,0x80000001u,0x007FFFFFu,
            0x00800000u,0x3F800000u,0xBF800000u,0x7F7FFFFFu,0xFF7FFFFFu,
            0x7F800000u,0xFF800000u,0x7FC00001u,0x7FA00001u};
        for (unsigned leaf=0;leaf<3u;++leaf) {
            const auto start=cases;
            for (auto mode:modes) {
                for (unsigned sample=0;sample<special.size();++sample) {
                    Fixture n(boot,leaf,mode),r(boot,leaf,mode);
                    for (auto* f:{&n,&r}) {
                        for (unsigned reg=4u;reg<=6u;++reg)
                            f->vector(f->cpu.r[reg],{special[sample],special[(sample+reg)%special.size()],special[(sample+reg+3u)%special.size()]});
                        f->label+=" special="+std::to_string(sample);
                    }
                    compare(n,r,leaf); ++cases;
                }
                // All P0/P1/P2 input/output mixtures (81 for cross, 3 otherwise).
                const unsigned combinations=leaf==0u?81u:3u;
                for (unsigned combination=0;combination<combinations;++combination) {
                    Fixture n(boot,leaf,mode),r(boot,leaf,mode);
                    for (auto* f:{&n,&r}) {
                        auto code=combination;
                        for (unsigned reg=4u;reg<=(leaf==0u?7u:4u);++reg) {
                            constexpr std::array segments{0u,0x80000000u,0xA0000000u};
                            f->cpu.r[reg]=(f->cpu.r[reg]&0x1FFFFFFFu)|segments[code%3u]; code/=3u;
                        }
                        f->bind_mmu(false); f->label+=" aliases="+std::to_string(combination);
                    }
                    compare(n,r,leaf); ++cases;
                }
            }
            // Read/read and output/read overlap are legal: all reads precede
            // the first store. Exercise both full and partial physical aliases.
            if (leaf==0u) {
                for (unsigned input_reg=4u;input_reg<=6u;++input_reg)
                    for (auto offset:{0u,4u,8u}) {
                        Fixture n(boot,leaf),r(boot,leaf);
                        for (auto* f:{&n,&r}) f->cpu.r[7]=((f->cpu.r[input_reg]+offset)&0x1FFFFFFFu)|0xA0000000u;
                        compare(n,r,leaf); ++cases;
                    }
                { Fixture n(boot,leaf),r(boot,leaf);
                  for (auto* f:{&n,&r}) { f->cpu.r[5]=f->cpu.r[4]; f->cpu.r[6]=f->cpu.r[4]+4u; }
                  compare(n,r,leaf); ++cases; }
            }
            // A zero vector, and finite independent vectors, cover the original
            // FIPR/FSRRA zero handling and ordinary geometry independently.
            for (bool zero:{false,true}) {
                Fixture n(boot,leaf),r(boot,leaf);
                if (zero) for (auto* f:{&n,&r}) f->vector(f->cpu.r[4],{0u,0u,0u});
                compare(n,r,leaf); ++cases;
            }
            for (auto segment:{0x80000000u,0xA0000000u}) {
                Fixture n(boot,leaf),r(boot,leaf);
                for (auto* f:{&n,&r}) {
                    f->cpu.mmucr=1u; f->bind_mmu(true);
                    for (unsigned reg=4u;reg<=7u;++reg) f->cpu.r[reg]=(f->cpu.r[reg]&0x1FFFFFFFu)|segment;
                }
                compare(n,r,leaf); ++cases;
            }
            // Last valid vector in the first 16 MiB window, with no mirror wrap.
            {
                Fixture n(boot,leaf),r(boot,leaf);
                for (auto* f:{&n,&r}) {
                    f->cpu.r[4]=0x0CFFFFF4u; f->vector(f->cpu.r[4],{0x3F800000u,0x40000000u,0x40400000u});
                    if (leaf==0u) f->cpu.r[7]=f->cpu.r[4];
                }
                compare(n,r,leaf); ++cases;
            }
            // Code is a legal read-only operand for cross and length.
            if (leaf!=2u) {
                Fixture n(boot,leaf),r(boot,leaf); n.cpu.r[4]=cm::cross_entry; r.cpu.r[4]=cm::cross_entry;
                compare(n,r,leaf); ++cases;
            }
            if (leaf!=1u) {
                Fixture n(boot,leaf),r(boot,leaf);
                if (leaf==2u) for (auto* f:{&n,&r}) f->vector(f->cpu.r[4],{0x3F800000u,0u,0u});
                const auto initial=n.cpu.r;
                compare(n,r,leaf); ++cases;
                for (auto* f:{&n,&r}) { f->cpu.pc=entries[leaf]; f->cpu.r=initial; }
                const auto& observed=collision_test::mode()==1u?r.events:n.events;
                const auto begin=observed.size(); compare(n,r,leaf); ++cases;
                require(std::any_of(observed.begin()+begin,observed.end(),[](const Write& w) { return !std::get<3>(w); }),"unchanged store events absent");
            }
            for (unsigned scenario=0;scenario<19u;++scenario) {
                Fixture f(boot,leaf); f.observe();
                switch (scenario) {
                case 0:f.cpu.sr|=sr_fd_mask;break;
                case 1:f.cpu.write_fpscr(fpscr_dn_mask|fpscr_sz_mask);break;
                case 2:f.cpu.write_fpscr(fpscr_dn_mask|fpscr_pr_mask);break;
                case 3:f.cpu.write_fpscr(fpscr_dn_mask|fpscr_exception_enable_mask);break;
                case 4:f.cpu.write_fpscr(0u);break;
                case 5:f.cpu.write_fpscr(fpscr_dn_mask|2u);break;
                case 6:f.observe(false);break;
                case 7:f.put(entries[leaf],0u);break;
                case 8:f.cpu.r[4]=0x0CFFFFF8u;break;
                case 9:f.cpu.r[4]=0x0D000000u;break;
                case 10:f.cpu.r[4]+=2u;break;
                case 11:f.cpu.memory.clear_direct_linear_alias_window();break;
                case 12:f.cpu.sleeping=true;break;
                case 13:f.cpu.trap_pending=true;break;
                case 14:f.cpu.pc+=2u;break;
                case 15:f.cpu.write_sr(0u);break;
                case 16:f.cpu.r[4]=0x2CE00000u;break;
                case 17:f.cpu.r[4]=0xFFFF0000u;break;
                case 18:(void)f.cpu.memory.add_watchpoint(f.cpu.r[4]&0x1FFFFFFFu,12u,MemoryWatchpointAccess::Read,
                    [](const MemoryAccessEvent&) { throw std::runtime_error("decline invoked watchpoint"); });break;
                }
                decline(f,&f.immutable); ++cases;
            }
            for (unsigned reg=4u;reg<=(leaf==0u?7u:4u);++reg)
                for (unsigned binding=0;binding<3u;++binding) {
                    Fixture f(boot,leaf); f.observe(); f.cpu.r[reg]&=0x1FFFFFFFu;
                    f.cpu.mmucr=binding==2u?0u:1u; if (binding) f.bind_mmu(true);
                    decline(f,&f.immutable); ++cases;
                }
            if (leaf!=1u) {
                const auto output_reg=leaf==0u?7u:4u;
                { Fixture f(boot,leaf,fpscr_dn_mask,true); f.observe(); decline(f,&f.immutable); ++cases; }
                for (auto address:{0x0CFFFFF8u,0x0D000000u,0x8CE00002u}) {
                    Fixture f(boot,leaf); f.observe(); f.cpu.r[output_reg]=address;
                    decline(f,&f.immutable); ++cases;
                }
                for (auto kind:{NativePortImmutableRangeKind::Executable,NativePortImmutableRangeKind::ReadOnlyImage})
                    for (auto offset:{0u,4u,8u}) {
                        Fixture f(boot,leaf); f.observe();
                        const std::array ranges{NativePortImmutableRange{(f.cpu.r[output_reg]&0x1FFFFFFFu)+offset,4u,native_port_immutable_range_mask(kind)}};
                        NativePortImmutableWriteGuard protected_output{ranges}; decline(f,&protected_output); ++cases;
                    }
                for (auto code:entries) {
                    Fixture f(boot,leaf); f.observe(); f.cpu.r[output_reg]=code;
                    decline(f,&f.immutable); ++cases;
                }
                // A range guard which omits this leaf still cannot authorize
                // self-modifying output: explicit identity overlap rejects it.
                { Fixture f(boot,leaf); f.cpu.r[output_reg]=entries[leaf];
                  const std::array ranges{NativePortImmutableRange{0x0CC00000u,4u,
                      native_port_immutable_range_mask(NativePortImmutableRangeKind::ReadOnlyImage)}};
                  NativePortImmutableWriteGuard unrelated{ranges};
                  decline(f,&unrelated); ++cases; }
            }
            if (leaf==0u) for (unsigned reg=5u;reg<=6u;++reg)
                for (auto address:{0x0CFFFFF8u,0x0D000000u,0x8CE00002u}) {
                    Fixture f(boot,leaf); f.observe(); f.cpu.r[reg]=address;
                    decline(f,&f.immutable); ++cases;
                }
            { Fixture f(boot,leaf); decline(f,nullptr); ++cases; }
            counts[leaf]=cases-start;
        }
        collision_test::verify_fusion();
        const auto closed_rejections=closed_declines(boot);
        std::cout<<"SONIC_COLLISION_MATH_PASS cases="<<cases<<" cross_cases="<<counts[0]
            <<" length_cases="<<counts[1]<<" normalize_cases="<<counts[2]
            <<" closed_rejections="<<closed_rejections
            <<" entries=8C027360,8C63A69C,8C63A88C reference=sha_bound_retail_sh4"
            <<" architectural_registers=exact RAM=exact stores=ordered_values_sources_changed_pointers_fpscr"
            <<" accounting=native_hook_policy\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
