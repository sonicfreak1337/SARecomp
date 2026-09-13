#include "sonic_matrix_stack.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <utility>

using namespace katana::runtime;
namespace {
constexpr auto entry=sonic::matrix_stack::push_entry,returned=0x8CF80000u;
constexpr auto capacity=0x8C88F5D8u,depth=0x8C88F5DCu,pointer=0x8C88F538u;
constexpr auto matrix_stack=0x8CE00000u,input=0x8CE10000u;
// NativePortImmutableWriteGuard requires ascending, nonoverlapping physical
// ranges. Pop precedes Push in the retail image regardless of test order.
constexpr std::array code_range{
    NativePortImmutableRange{sonic::matrix_stack::pop_entry&0x1FFFFFFFu,sonic::matrix_stack::pop_size,
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
    NativePortImmutableRange{entry&0x1FFFFFFFu,sonic::matrix_stack::push_size,
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
};
void require(bool value,const char* why) { if (!value) throw std::runtime_error(why); }
struct Services final : PlatformServices {
    std::string_view name() const noexcept override { return "matrix-stack-byte-reference"; }
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
    std::array<unsigned char,32> result{};
    require(bytes.size()<=ULONG_MAX && BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
        const_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),result.data(),ULONG(result.size()))>=0,"SHA-256 failed");
    constexpr char hex[]="0123456789abcdef"; std::string text(64,'0');
    for (std::size_t i=0;i<result.size();++i) { text[i*2]=hex[result[i]>>4]; text[i*2+1]=hex[result[i]&15]; }
    return text;
}
// Native-hook instruction/cycle/provenance accounting is intentionally omitted.
auto architecture(const CpuState& c) {
    return std::tuple(c.r,c.r_bank,c.fr,c.xf,c.pc,c.pr,c.gbr,c.vbr,c.ssr,c.spc,c.sgr,c.dbr,
        c.tra,c.tea,c.expevt,c.intevt,c.pteh,c.ptel,c.ptea,c.ttb,c.mmucr,c.mach,c.macl,c.fpul,
        c.read_fpscr(),c.sr,c.t,c.s,c.q,c.m,c.trap_pending,c.exception_generation,
        c.last_exception_cause,c.sleeping,c.prefetch_count,c.tlb_load_count);
}
// Include each intermediate written value and observer-visible R5/FPSCR. In
// particular MOVCA's StoreQueue writes are not hidden by later FMOV overwrites.
using Write=std::tuple<std::uint32_t,std::size_t,CodeWriteSource,bool,std::uint32_t,
    std::uint32_t,std::uint32_t,std::uint32_t>;
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    NativePortImmutableWriteGuard immutable{code_range};
    std::vector<Write> events;
    std::string label;
    Fixture(std::span<const std::uint8_t> boot,bool explicit_matrix,std::uint32_t fpscr=0u) {
        cpu.memory.map_region("main-ram",0x0C000000u,ram,MemoryRegionAccess::ReadWrite);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        auto bytes=ram->writable_bytes(); std::fill(bytes.begin(),bytes.end(),0xCDu);
        std::copy(boot.begin(),boot.end(),bytes.begin()+0x10000u);
        cpu.pc=entry; cpu.pr=returned; cpu.gbr=0x8CE80000u;
        cpu.write_sr(sr_md_mask); cpu.write_fpscr(fpscr);
        cpu.t=true; cpu.s=true; cpu.q=true; cpu.m=true;
        cpu.fpul=0xDEADBEEFu; cpu.mach=0x12345678u; cpu.macl=0x87654321u;
        constexpr std::array special{0u,0x80000000u,1u,0x80000001u,0x7F800000u,
            0xFF800000u,0x7FC00001u,0x7FA00001u};
        for (unsigned i=0;i<16u;++i) {
            cpu.r[i]=0xABCD1200u+i; cpu.fr[i]=0xCAFE0000u+i;
            cpu.xf[i]=special[i%special.size()]^(i<<8u);
            put(input+i*4u,0xC0000000u+i*0x123456u);
        }
        for (unsigned i=0;i<8u;++i) cpu.r_bank[i]=0x11220000u+i;
        cpu.r[4]=explicit_matrix?input:0u; cpu.r[15]=0x8CF00000u;
        put(capacity,32u); put(depth,3u); put(pointer,matrix_stack);
        label="explicit="+std::to_string(explicit_matrix)+" fpscr="+std::to_string(fpscr);
        events.reserve(80);
    }
    void put(std::uint32_t a,std::uint32_t v) {
        auto b=ram->writable_bytes(); std::memcpy(b.data()+(a&0xFFFFFFu),&v,4u);
    }
    std::uint32_t peek(std::uint32_t a) const {
        std::uint32_t v; std::memcpy(&v,ram->bytes().data()+(a&0xFFFFFFu),4u); return v;
    }
    void observe(bool stable=true) {
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e) noexcept {
            events.emplace_back(e.address,e.size,e.source,e.bytes_changed,peek(e.address),cpu.r[5],cpu.read_fpscr(),cpu.r[4]);
            immutable.observe_write(e);
        },stable?GuestWriteObserverContract::StableForPrevalidatedLinearWrites:GuestWriteObserverContract::General);
    }
    void bind_mmu(bool active) {
        cpu.address_space=std::make_shared<RuntimeAddressSpace>();
        cpu.address_space->write_mmucr(cpu.mmucr);
        cpu.address_space->set_mode(active?AddressTranslationMode::Mmu:AddressTranslationMode::NoMmu);
    }
    void pop(std::uint32_t count,std::uint32_t level,std::uint32_t matrix=matrix_stack) {
        cpu.pc=sonic::matrix_stack::pop_entry; cpu.r[4]=count;
        put(depth,level); put(pointer,matrix+(count<<6u));
        for (unsigned i=0;i<16u;++i) put(matrix_stack+i*4u,0x7FC01000u+i);
        label="pop count="+std::to_string(count)+" depth="+std::to_string(level)+" "+label;
    }
};
void execute_reference(CpuState& cpu) {
    // Every instruction, including MOVCA.L, both FSCHG transitions, all paired
    // FMOVs and BF/S's store delay slot uses the untouched original executor.
    // There is no FTRC/arithmetic reference correction in this leaf.
    const auto begin=cpu.pc;
    const auto size=begin==entry?sonic::matrix_stack::push_size:sonic::matrix_stack::pop_size;
    require(begin==entry || begin==sonic::matrix_stack::pop_entry,"unexpected reference entry");
    for (unsigned n=0;cpu.pc!=returned && n<200u;++n) {
        require(cpu.pc>=begin && cpu.pc<begin+size,"reference left SHA-bound matrix leaf");
        (void)execute_dynamic_sh4_block(cpu,services,1u);
        require(cpu.exception_generation==0u,"reference exception");
    }
    require(cpu.pc==returned,"reference did not return");
}
void compare(Fixture& native,Fixture& reference) {
    native.observe(); reference.observe();
    require(sonic::matrix_stack::try_execute(native.cpu,&native.immutable),"eligible leaf declined");
    execute_reference(reference.cpu);
    if (architecture(native.cpu)!=architecture(reference.cpu)) {
        std::cerr<<native.label<<'\n'<<std::hex;
        for (unsigned i=0;i<16u;++i) {
            if (native.cpu.r[i]!=reference.cpu.r[i]) std::cerr<<"r"<<i<<' '<<native.cpu.r[i]<<'/'<<reference.cpu.r[i]<<'\n';
            if (native.cpu.fr[i]!=reference.cpu.fr[i]) std::cerr<<"fr"<<i<<' '<<native.cpu.fr[i]<<'/'<<reference.cpu.fr[i]<<'\n';
            if (native.cpu.xf[i]!=reference.cpu.xf[i]) std::cerr<<"xf"<<i<<' '<<native.cpu.xf[i]<<'/'<<reference.cpu.xf[i]<<'\n';
        }
        std::cerr<<"fpscr "<<native.cpu.read_fpscr()<<'/'<<reference.cpu.read_fpscr()<<std::dec<<'\n';
        throw std::runtime_error("architectural state differs");
    }
    require(std::equal(native.ram->bytes().begin(),native.ram->bytes().end(),reference.ram->bytes().begin()),"RAM/CPU-stack differs");
    require(native.events==reference.events,"ordered stores/value/source/changed/R4/R5/FPSCR differ");
    require(!native.immutable.write_detected(),"protected code written");
    if (native.cpu.address_space && reference.cpu.address_space)
        require(native.cpu.address_space->snapshot()==reference.cpu.address_space->snapshot(),"MMU state differs");
}
void decline(Fixture& f,const NativePortImmutableWriteGuard* guard,bool batch_stores=false) {
    const auto before=architecture(f.cpu);
    const auto ram=std::vector<std::uint8_t>(f.ram->bytes().begin(),f.ram->bytes().end());
    const auto events=f.events; const auto metrics=f.cpu.memory.performance_counters();
    const auto translation=f.cpu.address_space?std::optional{f.cpu.address_space->snapshot()}:std::nullopt;
    std::uint64_t staged_groups=73u;
    require(!sonic::matrix_stack::try_execute(f.cpu,guard,batch_stores,&staged_groups),"unsafe leaf admitted");
    require(staged_groups==73u,"decline mutated staging counter");
    require(before==architecture(f.cpu) && events==f.events &&
        std::equal(ram.begin(),ram.end(),f.ram->bytes().begin()),"decline mutated guest");
    const auto after=f.cpu.memory.performance_counters();
    require(metrics.indexed_region_hits==after.indexed_region_hits && metrics.reference_region_probes==after.reference_region_probes &&
        metrics.observed_accesses==after.observed_accesses && metrics.unobserved_accesses==after.unobserved_accesses,"decline changed metrics");
    require(!translation || *translation==f.cpu.address_space->snapshot(),"decline changed MMU");
}

// Unlike observe(), this observer obeys StableForPrevalidatedLinearWrites in
// full: it only copies event fields and updates its own invalidation bookkeeping.
// It never reads CPU, backing bytes, memory counters or scheduler state. Keep
// the original intermediate-value/R4/R5/FPSCR comparisons on the scalar path.
using EventOnlyWrite=std::tuple<std::uint32_t,std::size_t,CodeWriteSource,bool>;
struct EventOnlyObserver {
    NativePortImmutableWriteGuard& immutable;
    std::vector<EventOnlyWrite> events;
    unsigned scalar_calls=0u,span_commits=0u;
    bool admit=true;
    explicit EventOnlyObserver(NativePortImmutableWriteGuard& guard):immutable(guard) { events.reserve(80u); }
    void record(const GuestWriteEvent& e) noexcept {
        events.emplace_back(e.address,e.size,e.source,e.bytes_changed);
        immutable.observe_write(e);
    }
    void install(Memory& memory,bool batch_interface) {
        memory.set_guest_write_observer([this](const GuestWriteEvent& e) noexcept {
            ++scalar_calls; record(e);
        },GuestWriteObserverContract::StableForPrevalidatedLinearWrites);
        if (batch_interface) memory.set_guest_write_batch_observer(GuestWriteBatchObserver{
            this,
            [](void* context,std::span<const GuestWriteEvent>) noexcept {
                return static_cast<EventOnlyObserver*>(context)->admit;
            },
            [](void* context,std::span<const GuestWriteEvent> events) noexcept {
                auto& self=*static_cast<EventOnlyObserver*>(context); ++self.span_commits;
                for (const auto& e:events) self.record(e);
            },
        });
    }
};
// Observer mode 0: no batch interface (begin must decline); 1: span accepted;
// 2: span rejected (flush must replay every store through the scalar observer).
void compare_batch(Fixture& n,Fixture& r,unsigned observer_mode,bool unchanged_expected=false,bool batch_stores=true) {
    const auto initial_entry=n.cpu.pc;
    EventOnlyObserver native_observer(n.immutable),reference_observer(r.immutable);
    native_observer.admit=observer_mode!=2u;
    native_observer.install(n.cpu.memory,observer_mode!=0u);
    reference_observer.install(r.cpu.memory,false);
    // Start nonzero to verify accumulation, never assignment/reset. A rejected
    // span still stages the same groups before the SDK's scalar replay.
    std::uint64_t staged_groups=91u;
    require(sonic::matrix_stack::try_execute(n.cpu,&n.immutable,batch_stores,&staged_groups),"eligible batch leaf declined");
    execute_reference(r.cpu);
    if (architecture(n.cpu)!=architecture(r.cpu)) {
        std::cerr<<n.label<<" batch observer="<<observer_mode<<'\n';
        throw std::runtime_error("batch architectural state differs");
    }
    require(std::equal(n.ram->bytes().begin(),n.ram->bytes().end(),r.ram->bytes().begin()),"batch RAM/CPU-stack differs");
    require(native_observer.events==reference_observer.events,"batch ordered address/size/source/changed events differ");
    require(!n.immutable.write_detected() && !r.immutable.write_detected(),"batch protected code written");
    if (n.cpu.address_space && r.cpu.address_space)
        require(n.cpu.address_space->snapshot()==r.cpu.address_space->snapshot(),"batch MMU state differs");
    const auto stores=native_observer.events.size();
    const auto groups=initial_entry==entry && stores>=20u?(stores-2u)/18u:0u;
    const bool span_commit=batch_stores && observer_mode==1u;
    require(native_observer.span_commits==(span_commit?groups:0u),"unexpected matrix batch commit count");
    require(native_observer.scalar_calls==(span_commit?stores-groups*18u:stores),"unexpected scalar replay count");
    require(staged_groups==91u+(batch_stores && observer_mode!=0u?groups:0u),"incorrect staged-group usage witness");
    if (unchanged_expected)
        require(std::any_of(native_observer.events.begin(),native_observer.events.end(),[](const EventOnlyWrite& e) {
            return !std::get<3>(e);
        }),"batch dropped unchanged store events");
    // Contexts are local: remove callbacks before their owners leave scope.
    n.cpu.memory.clear_guest_write_batch_observer(); n.cpu.memory.clear_guest_write_observer();
    r.cpu.memory.clear_guest_write_observer();
}
} // namespace
int main(int argc,char** argv) {
    try {
        require(argc==2,"usage: test_matrix_stack <installed-content-root>");
        const auto boot=read(std::filesystem::path(argv[1])/"boot.bin");
        require(boot.size()==6735296u && digest(boot)=="b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af","boot SHA differs");
        require(digest(std::span<const std::uint8_t>(boot).subspan(0x629BB0u,0x80u))==sonic::matrix_stack::push_source_sha256,"push SHA differs");
        require(digest(std::span<const std::uint8_t>(boot).subspan(0x629AD8u,0x40u))==sonic::matrix_stack::pop_source_sha256,"pop SHA differs");
        unsigned cases=0;
        constexpr std::array modes{0u,1u,2u,3u,fpscr_dn_mask,fpscr_fr_mask,
            fpscr_fr_mask|fpscr_flag_mask|fpscr_cause_mask|fpscr_exception_enable_mask};
        for (bool explicit_matrix:{false,true})
            for (auto mode:modes)
                for (auto stack_segment:{0u,0x80000000u,0xA0000000u})
                    for (auto input_segment:{0u,0x80000000u,0xA0000000u}) {
                        Fixture n(boot,explicit_matrix,mode),r(boot,explicit_matrix,mode);
                        for (auto* f:{&n,&r}) {
                            f->put(pointer,(matrix_stack&0x1FFFFFFFu)|stack_segment);
                            if (explicit_matrix) f->cpu.r[4]=(input&0x1FFFFFFFu)|input_segment;
                            f->bind_mmu(false);
                        }
                        compare(n,r); ++cases;
                    }
        // Signed capacity comparison, including wrap-edge inputs. Full-stack
        // returns must not touch the deliberately invalid matrix addresses.
        for (auto pair:std::array{std::pair{0u,0u},std::pair{32u,32u},std::pair{32u,33u},
                std::pair{0x80000000u,0u},std::pair{0u,0xFFFFFFFFu}}) {
            Fixture n(boot,true),r(boot,true);
            for (auto* f:{&n,&r}) {
                f->put(capacity,pair.first); f->put(depth,pair.second);
                if (std::bit_cast<std::int32_t>(pair.second)>=std::bit_cast<std::int32_t>(pair.first)) {
                    f->put(pointer,0xFFFFFFFFu); f->cpu.r[4]=0xFFFFFFFFu;
                }
            }
            compare(n,r); ++cases;
        }
        // Read/read aliasing is accepted: code may be used as raw input matrix.
        {
            Fixture n(boot,true),r(boot,true); n.cpu.r[4]=entry; r.cpu.r[4]=entry;
            compare(n,r); ++cases;
        }
        // P1/P2 work under a bound MMU, independently of physical P0 admission.
        for (bool explicit_matrix:{false,true}) {
            Fixture n(boot,explicit_matrix),r(boot,explicit_matrix);
            for (auto* f:{&n,&r}) { f->cpu.mmucr=1u; f->bind_mmu(true); }
            compare(n,r); ++cases;
        }
        // Repeated byte-identical FMOV stores still emit changed=false; MOVCA
        // writes remain separately visible even when immediately overwritten.
        {
            Fixture n(boot,false),r(boot,false); compare(n,r); ++cases;
            for (auto* f:{&n,&r}) {
                f->put(depth,3u); f->put(pointer,matrix_stack); f->cpu.pc=entry; f->cpu.r[0]=0xABCD1200u;
            }
            const auto start=n.events.size(); compare(n,r); ++cases;
            require(std::any_of(n.events.begin()+start,n.events.end(),[](const Write& w) {
                return std::get<2>(w)==CodeWriteSource::Fpu && !std::get<3>(w);
            }),"unchanged FPU store events absent");
        }
        for (unsigned scenario=0;scenario<17u;++scenario) {
            Fixture f(boot,true);
            switch (scenario) {
            case 0:f.cpu.sr|=sr_fd_mask;break;
            case 1:f.cpu.write_fpscr(fpscr_sz_mask);break;
            case 2:f.cpu.write_fpscr(fpscr_pr_mask);break;
            case 3:f.observe(false);break;
            case 4:f.put(entry,0u);break;
            case 5:f.put(pointer,0x0CFFFFE0u);break;
            case 6:f.cpu.r[4]=0x0CFFFFE0u;break;
            case 7:f.put(pointer,input);break;
            case 8:f.put(pointer,depth);break;
            case 9:f.put(pointer,pointer);break;
            case 10:f.cpu.r[4]=pointer;break;
            case 11:f.cpu.r[4]=depth;break;
            case 12:f.cpu.memory.clear_direct_linear_alias_window();break;
            case 13:f.put(pointer,0x0D000000u);break;
            case 14:f.cpu.r[4]=input+2u;break;
            case 15:f.cpu.sleeping=true;break;
            case 16:(void)f.cpu.memory.add_watchpoint(matrix_stack&0x1FFFFFFFu,128u,MemoryWatchpointAccess::Write,
                [](const MemoryAccessEvent&) { throw std::runtime_error("decline invoked watchpoint"); });break;
            }
            decline(f,&f.immutable); ++cases;
        }
        for (bool p0_input:{false,true})
            for (unsigned binding=0;binding<3u;++binding) {
                Fixture f(boot,true); f.observe();
                if (p0_input) f.cpu.r[4]&=0x1FFFFFFFu; else f.put(pointer,matrix_stack&0x1FFFFFFFu);
                f.cpu.mmucr=binding==2u?0u:1u;
                if (binding) f.bind_mmu(true);
                decline(f,&f.immutable); ++cases;
            }
        for (auto kind:{NativePortImmutableRangeKind::Executable,NativePortImmutableRangeKind::ReadOnlyImage})
            for (auto address:{depth,pointer,matrix_stack,matrix_stack+64u}) {
                Fixture f(boot,false); f.observe();
                const std::array ranges{NativePortImmutableRange{address&0x1FFFFFFFu,4u,native_port_immutable_range_mask(kind)}};
                NativePortImmutableWriteGuard protected_output{ranges}; decline(f,&protected_output); ++cases;
            }
        { Fixture f(boot,true); decline(f,nullptr); ++cases; }
        const auto push_cases=cases;
        // Wrapped subtraction and count*64 are architectural, including zero
        // count, underflow and counts whose address displacement wraps to zero.
        constexpr std::array pop_pairs{std::pair{1u,3u},std::pair{0u,3u},std::pair{2u,3u},
            std::pair{3u,3u},std::pair{0u,0u},std::pair{4u,3u},std::pair{0x04000000u,3u},
            std::pair{0xFFFFFFFFu,3u},std::pair{0x80000000u,0u}};
        for (auto mode:modes)
            for (auto segment:{0u,0x80000000u,0xA0000000u})
                for (auto pair:pop_pairs) {
                    Fixture n(boot,false,mode),r(boot,false,mode);
                    for (auto* f:{&n,&r}) {
                        f->pop(pair.first,pair.second,(matrix_stack&0x1FFFFFFFu)|segment);
                        f->bind_mmu(false);
                        if (pair.first==pair.second) f->put(pointer,0xFFFFFFFFu);
                    }
                    compare(n,r); ++cases;
                }
        for (auto segment:{0x80000000u,0xA0000000u}) {
            Fixture n(boot,false),r(boot,false);
            for (auto* f:{&n,&r}) {
                f->pop(1u,3u,(matrix_stack&0x1FFFFFFFu)|segment);
                f->cpu.mmucr=1u; f->bind_mmu(true);
            }
            compare(n,r); ++cases;
        }
        // Only read/read aliasing: the matrix can be the immutable pop code.
        {
            Fixture n(boot,false),r(boot,false);
            for (auto* f:{&n,&r}) f->pop(1u,3u,sonic::matrix_stack::pop_entry);
            compare(n,r); ++cases;
        }
        for (unsigned scenario=0;scenario<14u;++scenario) {
            Fixture f(boot,false); f.pop(1u,3u); f.observe();
            switch (scenario) {
            case 0:f.cpu.sr|=sr_fd_mask;break;
            case 1:f.cpu.write_fpscr(fpscr_sz_mask);break;
            case 2:f.cpu.write_fpscr(fpscr_pr_mask);break;
            case 3:f.observe(false);break;
            case 4:f.put(sonic::matrix_stack::pop_entry,0u);break;
            case 5:f.put(pointer,0x0D000020u);break; // read spans first-window end
            case 6:f.put(pointer,depth+64u);break;
            case 7:f.put(pointer,pointer+64u);break;
            case 8:f.put(pointer,0x0D000040u);break; // entirely next mirror
            case 9:f.put(pointer,matrix_stack+66u);break;
            case 10:f.cpu.memory.clear_direct_linear_alias_window();break;
            case 11:f.cpu.sleeping=true;break;
            case 12:f.cpu.pc=entry+2u;break;
            case 13:(void)f.cpu.memory.add_watchpoint(pointer&0x1FFFFFFFu,4u,MemoryWatchpointAccess::Write,
                [](const MemoryAccessEvent&) { throw std::runtime_error("pop decline invoked watchpoint"); });break;
            }
            decline(f,&f.immutable); ++cases;
        }
        for (unsigned binding=0;binding<3u;++binding) {
            Fixture f(boot,false); f.pop(1u,3u,matrix_stack&0x1FFFFFFFu); f.observe();
            f.cpu.mmucr=binding==2u?0u:1u; if (binding) f.bind_mmu(true);
            decline(f,&f.immutable); ++cases;
        }
        for (auto kind:{NativePortImmutableRangeKind::Executable,NativePortImmutableRangeKind::ReadOnlyImage})
            for (auto address:{depth,pointer}) {
                Fixture f(boot,false); f.pop(1u,3u); f.observe();
                const std::array ranges{NativePortImmutableRange{address&0x1FFFFFFFu,4u,native_port_immutable_range_mask(kind)}};
                NativePortImmutableWriteGuard protected_output{ranges}; decline(f,&protected_output); ++cases;
            }
        { Fixture f(boot,false); f.pop(1u,3u); decline(f,nullptr); ++cases; }
        const auto scalar_cases=cases;
        // Opt-in batch variants add coverage; all scalar cases and their
        // callback-visible intermediate-value assertions above stay intact.
        for (unsigned observer_mode=0u;observer_mode<3u;++observer_mode) {
            for (bool explicit_matrix:{false,true})
                for (auto mode:modes)
                    for (auto stack_segment:{0u,0x80000000u,0xA0000000u})
                        for (auto input_segment:{0u,0x80000000u,0xA0000000u}) {
                            Fixture n(boot,explicit_matrix,mode),r(boot,explicit_matrix,mode);
                            for (auto* f:{&n,&r}) {
                                f->put(pointer,(matrix_stack&0x1FFFFFFFu)|stack_segment);
                                if (explicit_matrix) f->cpu.r[4]=(input&0x1FFFFFFFu)|input_segment;
                                f->bind_mmu(false);
                            }
                            compare_batch(n,r,observer_mode); ++cases;
                        }
            // Pop has no consecutive store run: enabling the option must not
            // delay its depth/pointer stores across the following guest reads.
            for (auto mode:modes)
                for (auto segment:{0u,0x80000000u,0xA0000000u})
                    for (auto count:{0u,1u,3u,0xFFFFFFFFu}) {
                        Fixture n(boot,false,mode),r(boot,false,mode);
                        for (auto* f:{&n,&r}) {
                            f->pop(count,3u,(matrix_stack&0x1FFFFFFFu)|segment); f->bind_mmu(false);
                        }
                        compare_batch(n,r,observer_mode); ++cases;
                    }
            for (bool explicit_matrix:{false,true}) {
                Fixture n(boot,explicit_matrix),r(boot,explicit_matrix);
                for (auto* f:{&n,&r}) { f->cpu.mmucr=1u; f->bind_mmu(true); }
                compare_batch(n,r,observer_mode); ++cases;
            }
            // Full-stack return never constructs a save batch or reads matrix.
            { Fixture n(boot,true),r(boot,true);
              for (auto* f:{&n,&r}) { f->put(depth,32u); f->cpu.r[4]=0xFFFFFFFFu; f->put(pointer,0xFFFFFFFFu); }
              compare_batch(n,r,observer_mode); ++cases; }
            // Repeated payload preserves false changed flags despite the two
            // MOVCA stores overlapping later FMOV stores within each batch.
            { Fixture n(boot,false),r(boot,false);
              compare_batch(n,r,observer_mode); ++cases;
              for (auto* f:{&n,&r}) {
                  f->put(depth,3u); f->put(pointer,matrix_stack); f->cpu.pc=entry; f->cpu.r[0]=0xABCD1200u;
              }
              compare_batch(n,r,observer_mode,true); ++cases; }
        }
        // Even with a usable batch interface, explicit opt-out must leave its
        // usage witness untouched and preserve the original scalar execution.
        for (bool explicit_matrix:{false,true}) {
            Fixture n(boot,explicit_matrix),r(boot,explicit_matrix);
            compare_batch(n,r,1u,false,false); ++cases;
        }
        { Fixture n(boot,false),r(boot,false);
          n.pop(1u,3u); r.pop(1u,3u); compare_batch(n,r,1u,false,false); ++cases; }
        // Batch selection must not move admission past a guest mutation.
        for (bool pop:{false,true}) for (unsigned scenario=0u;scenario<5u;++scenario) {
            Fixture f(boot,true); if (pop) f.pop(1u,3u);
            EventOnlyObserver observer(f.immutable); observer.install(f.cpu.memory,true);
            switch (scenario) {
            case 0:f.cpu.sr|=sr_fd_mask;break;
            case 1:f.cpu.memory.clear_direct_linear_alias_window();break;
            case 2:f.put(pointer,pop?depth+64u:depth);break;
            case 3:f.put(pop?sonic::matrix_stack::pop_entry:entry,0u);break;
            case 4:f.cpu.r[4]=pop?0xFFFFFFFEu:0x0CFFFFE0u;
                if (pop) f.put(pointer,0x0D000000u-128u);break;
            }
            decline(f,&f.immutable,true); require(observer.events.empty(),"batch decline emitted observer events"); ++cases;
            f.cpu.memory.clear_guest_write_batch_observer(); f.cpu.memory.clear_guest_write_observer();
        }
        std::cout<<"SONIC_MATRIX_STACK_PASS cases="<<cases<<" push_cases="<<push_cases<<" pop_cases="<<(scalar_cases-push_cases)
            <<" scalar_cases="<<scalar_cases<<" batch_cases="<<(cases-scalar_cases)
            <<" entries=8C639BB0,8C639AD8 reference=sha_bound_retail_sh4 architectural_registers=exact RAM=exact"
            <<" stores=ordered_values_sources_changed_r4_r5_fpscr batch_events=ordered_address_size_source_changed"
            <<" accounting=native_hook_policy\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
