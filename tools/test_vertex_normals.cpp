#include "sonic_vertex_normals.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include "katana/sh4/decoder.hpp"
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <cfenv>
#include <xmmintrin.h>

using namespace katana::runtime;
namespace {
constexpr auto entry=0x8C0563ACu, returned=0x8CF80000u;
constexpr auto model=0x8CE01000u, meshes=0x8CE02000u;
constexpr auto indices=0x8CE10000u, faces=0x8CE20000u, normals=0x8CE30000u;
constexpr auto stack_top=0x8CF00000u;
constexpr std::array code_range{
    NativePortImmutableRange{0x0C0563ACu,0x2A6u,
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
};
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
struct Services final : PlatformServices {
    std::string_view name() const noexcept override { return "vertex-normals-byte-reference"; }
    std::uint32_t abi_version() const noexcept override { return 128u; }
    std::uint32_t guest_cycle_contract() const noexcept override { return 0u; }
    PlatformCapabilities capabilities() const noexcept override { return {}; }
    void read_memory(std::uint32_t, std::span<std::uint8_t>) override { throw std::runtime_error("device read"); }
    void write_memory(std::uint32_t, std::span<const std::uint8_t>) override { throw std::runtime_error("device write"); }
    std::uint64_t scheduler_cycle() const noexcept override { return 0u; }
    std::optional<std::uint64_t> next_scheduler_event_cycle() const noexcept override { return {}; }
    PlatformSchedulerResult consume_guest_cycles(std::uint64_t, std::size_t) override { return {}; }
    std::optional<PlatformInterruptRequest> poll_interrupt() override { return {}; }
    PlatformDmaResult start_dma(const PlatformDmaRequest&) override { throw std::runtime_error("DMA"); }
    PlatformFallbackResult controlled_fallback(CpuState&, const PlatformFallbackRequest&) override {
        throw std::runtime_error("executor fallback");
    }
    bool prefetch(CpuState&, GuestInstructionOrigin, std::uint32_t) override { throw std::runtime_error("PREF"); }
} services;

std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary); require(bool(file), "boot.bin missing");
    return {std::istreambuf_iterator<char>(file), {}};
}
std::string digest(std::span<const std::uint8_t> bytes) {
    std::array<unsigned char, 32> result{};
    require(bytes.size() <= ULONG_MAX && BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
        const_cast<PUCHAR>(bytes.data()), ULONG(bytes.size()), result.data(), ULONG(result.size())) >= 0,
        "SHA-256 failed");
    constexpr char hex[] = "0123456789abcdef"; std::string text(64, '0');
    for (std::size_t i = 0; i < result.size(); ++i) {
        text[i * 2] = hex[result[i] >> 4]; text[i * 2 + 1] = hex[result[i] & 15];
    }
    return text;
}

// Architectural state only. Executor instruction/cycle/provenance bookkeeping
// is intentionally outside the native hook's architectural leaf contract.
auto architecture(const CpuState& c) {
    return std::tuple(c.r, c.r_bank, c.fr, c.xf, c.pc, c.pr, c.gbr, c.vbr, c.ssr,
        c.spc, c.sgr, c.dbr, c.tra, c.tea, c.expevt, c.intevt, c.pteh, c.ptel,
        c.ptea, c.ttb, c.mmucr, c.mach, c.macl, c.fpul, c.read_fpscr(), c.sr,
        c.t, c.s, c.q, c.m, c.trap_pending, c.exception_generation,
        c.last_exception_cause, c.sleeping, c.prefetch_count, c.tlb_load_count);
}
using Write = std::tuple<std::uint32_t, std::size_t, CodeWriteSource, bool>;
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    NativePortImmutableWriteGuard immutable{code_range};
    std::vector<Write> events;
    std::string label;
    Fixture(std::span<const std::uint8_t> boot,unsigned count,unsigned shape,unsigned mode,
            unsigned variant=0u,bool read_only=false) {
        label="count="+std::to_string(count)+" shape="+std::to_string(shape)+
            " mode="+std::to_string(mode)+" variant="+std::to_string(variant);
        cpu.memory.map_region("main-ram",0x0C000000u,ram,
            read_only?MemoryRegionAccess::ReadOnly:MemoryRegionAccess::ReadWrite);
        if (!read_only) cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        auto bytes=ram->writable_bytes();std::fill(bytes.begin(),bytes.end(),0xCDu);
        std::copy(boot.begin(),boot.end(),bytes.begin()+0x10000u);
        cpu.pc=entry;cpu.pr=returned;cpu.gbr=0x8CE00000u;
        cpu.write_sr(sr_md_mask);cpu.write_fpscr(fpscr_dn_mask|mode);
        cpu.t=true;cpu.s=true;cpu.q=true;cpu.m=true;
        cpu.fpul=0xDEADBEEFu;cpu.mach=0x12345678u;cpu.macl=0x87654321u;
        for(unsigned i=0;i<16;++i) {
            cpu.r[i]=0xABCDEF00u+i;cpu.fr[i]=0x3F000000u+i*0x123u;
            cpu.xf[i]=0xBF000000u+i*0x321u;
        }
        for(unsigned i=0;i<8;++i) cpu.r_bank[i]=0x11220000u+i;
        cpu.r[4]=model;cpu.r[15]=stack_top;
        put(model+4u,normals);put(model+8u,count);put(model+12u,meshes);
        const unsigned sets=shape==3u?3u:shape==4u?0u:1u;
        half(model+20u,sets);
        constexpr std::array special{0u,0x80000000u,1u,0x80000001u,0x00800000u,
            0x7F800000u,0xFF800000u,0x7FC00001u,0x7FA00001u,0x7F7FFFFFu};
        for(unsigned mesh=0;mesh<sets;++mesh) {
            const auto kind=shape==3u?mesh:shape==6u?2u:shape==5u?0u:shape;
            const auto at=meshes+mesh*24u, idx=indices+mesh*0x1000u, norm=faces+mesh*0x1000u;
            half(at,kind==0u?0x007Fu:kind==1u?0x407Fu:0xC07Fu);
            half(at+2u,shape==5u?0u:kind==2u?3u:5u);
            put(at+4u,idx);put(at+12u,norm);
            if(kind!=2u) {
                const auto width=kind==0u?3u:4u;
                for(unsigned f=0;f<5u;++f) for(unsigned j=0;j<width;++j)
                    half(idx+(f*width+j)*2u, f==0u?0u:(f+j)%(count+2u));
            } else {
                std::uint32_t cursor=idx;
                for(unsigned strip=0;strip<3u;++strip) {
                    const auto length=shape==6u?strip:strip==0u?3u:strip==1u?5u:8u;
                    half(cursor,length|((strip&1u)?0xC000u:0u));
                    // Extra look-ahead word is initialized even after the final
                    // strip. Short strips overlap that read with the next header.
                    for(unsigned j=0;j<std::max(3u,length+1u);++j)
                        half(cursor+2u+j*2u,j<3u?0u:(strip+j)%(count+2u));
                    cursor+=length>2u?2u+length*2u:6u;
                }
            }
            for(unsigned i=0;i<24u;++i) {
                const auto bits=variant?special[(i+variant+mesh)%special.size()]:
                    std::bit_cast<std::uint32_t>(float(int((i+mesh*3u)%17u)-8)*0.1875f);
                put(norm+i*4u,bits);
            }
        }
        events.reserve(8192);
    }
    void put(std::uint32_t address,std::uint32_t value) {
        auto bytes=ram->writable_bytes();const auto offset=address&0xFFFFFFu;
        for(unsigned i=0;i<4u;++i) bytes[offset+i]=std::uint8_t(value>>(i*8u));
    }
    void half(std::uint32_t address,std::uint32_t value) {
        auto bytes=ram->writable_bytes();const auto offset=address&0xFFFFFFu;
        bytes[offset]=std::uint8_t(value);bytes[offset+1u]=std::uint8_t(value>>8u);
    }
    void aliases(unsigned p0_mask,bool p2) {
        const auto alias=[&](std::uint32_t address,unsigned bit) {
            return (address&0x1FFFFFFFu)|((p0_mask&(1u<<bit))?0u:p2?0xA0000000u:0x80000000u);
        };
        cpu.r[4]=alias(model,0u);cpu.r[15]=alias(stack_top,1u);
        put(model+4u,alias(normals,2u));put(model+12u,alias(meshes,3u));
        for(unsigned mesh=0;mesh<3u;++mesh) {
            put(meshes+mesh*24u+4u,alias(indices+mesh*0x1000u,4u));
            put(meshes+mesh*24u+12u,alias(faces+mesh*0x1000u,5u));
        }
    }
    void bind_address_space(bool mmu) {
        cpu.address_space=std::make_shared<RuntimeAddressSpace>();
        cpu.address_space->write_mmucr(cpu.mmucr);
        cpu.address_space->set_mode(mmu?AddressTranslationMode::Mmu:AddressTranslationMode::NoMmu);
    }
    void observe(bool stable=true) {
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e) noexcept {
            events.emplace_back(e.address,e.size,e.source,e.bytes_changed);immutable.observe_write(e);
        },stable?GuestWriteObserverContract::StableForPrevalidatedLinearWrites:GuestWriteObserverContract::General);
    }
};

void execute_reference(CpuState& cpu) {
    for(unsigned n=0;cpu.pc!=returned && n<2000000u;++n) {
        require(cpu.pc>=entry && cpu.pc<entry+0x2A6u,"reference left byte-bound leaf");
        // This leaf contains no FTRC. Do not import the palette fixture's FTRC
        // repair or silently compensate any other interpreter discrepancy.
        (void)execute_dynamic_sh4_block(cpu,services,1u);
        require(cpu.exception_generation==0u,"reference FPU/memory exception");
    }
    require(cpu.pc==returned,"reference did not return");
}
void compare(Fixture& native, Fixture& reference) {
    native.observe(); reference.observe();
    const auto host_before=_mm_getcsr();
    require(sonic::vertex_normals::try_execute(native.cpu, &native.immutable), "eligible leaf declined");
    require(_mm_getcsr()==host_before,"native changed ambient MXCSR");
    execute_reference(reference.cpu);
    require(_mm_getcsr()==host_before,"reference changed ambient MXCSR");
    if (architecture(native.cpu) != architecture(reference.cpu)) {
        std::cerr << native.label << '\n' << std::hex;
        for (unsigned i = 0; i < 16; ++i) {
            if (native.cpu.r[i] != reference.cpu.r[i]) std::cerr << "r" << i << " native=" << native.cpu.r[i] << " reference=" << reference.cpu.r[i] << '\n';
            if (native.cpu.fr[i] != reference.cpu.fr[i]) std::cerr << "fr" << i << " native=" << native.cpu.fr[i] << " reference=" << reference.cpu.fr[i] << '\n';
            if (native.cpu.xf[i] != reference.cpu.xf[i]) std::cerr << "xf" << i << " native=" << native.cpu.xf[i] << " reference=" << reference.cpu.xf[i] << '\n';
        }
        std::cerr << "T=" << native.cpu.t << '/' << reference.cpu.t << " FPSCR=" << native.cpu.read_fpscr() << '/' << reference.cpu.read_fpscr()
            << " FPUL=" << native.cpu.fpul << '/' << reference.cpu.fpul << std::dec << '\n';
        throw std::runtime_error("architectural register/status mismatch");
    }
    require(std::equal(native.ram->bytes().begin(), native.ram->bytes().end(), reference.ram->bytes().begin()),
        "RAM/stack mismatch");
    if (native.events != reference.events) {
        std::cerr<<native.label<<" native_events="<<native.events.size()
                 <<" reference_events="<<reference.events.size()<<'\n';
        const auto shared=std::min(native.events.size(),reference.events.size());
        std::size_t first=0;
        while(first<shared && native.events[first]==reference.events[first]) ++first;
        const auto show=[](const char* name,const std::vector<Write>& events,std::size_t index) {
            std::cerr<<name<<"["<<index<<"]=";
            if(index>=events.size()) {std::cerr<<"<missing>\n";return;}
            const auto& [address,size,source,changed]=events[index];
            std::cerr<<"address=0x"<<std::hex<<address<<std::dec<<" size="<<size
                     <<" source="<<static_cast<unsigned>(source)<<" changed="<<changed<<'\n';
        };
        for(std::size_t i=first?first-1u:0u;i<std::min(first+3u,std::max(native.events.size(),reference.events.size()));++i) {
            show("native",native.events,i);show("reference",reference.events,i);
        }
        throw std::runtime_error("store order/source/changed flags differ");
    }
    require(!native.immutable.write_detected(), "native wrote protected code");
}
void decline(Fixture& fixture, const NativePortImmutableWriteGuard* guard) {
    const auto before = architecture(fixture.cpu);
    const auto memory = std::vector<std::uint8_t>(fixture.ram->bytes().begin(), fixture.ram->bytes().end());
    const auto events = fixture.events;
    const auto metrics = fixture.cpu.memory.performance_counters();
    const auto translation = fixture.cpu.address_space ?
        std::optional{fixture.cpu.address_space->snapshot()} : std::nullopt;
    require(!sonic::vertex_normals::try_execute(fixture.cpu, guard), "unsafe admission accepted");
    require(before == architecture(fixture.cpu) && events == fixture.events &&
        std::equal(memory.begin(), memory.end(), fixture.ram->bytes().begin()), "decline mutated guest state");
    const auto after = fixture.cpu.memory.performance_counters();
    require(metrics.indexed_region_hits == after.indexed_region_hits &&
        metrics.reference_region_probes == after.reference_region_probes &&
        metrics.observed_accesses == after.observed_accesses && metrics.unobserved_accesses == after.unobserved_accesses,
        "decline changed memory metrics");
    require(!translation || *translation == fixture.cpu.address_space->snapshot(),
        "decline changed translation state");
}
} // namespace

int main(int argc,char** argv) {
    try {
        require(argc==2,"usage: test_vertex_normals <installed-content-root>");
        const auto boot=read(std::filesystem::path(argv[1])/"boot.bin");
        require(boot.size()==6735296u && digest(boot)==
            "b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af","retail boot SHA differs");
        require(digest(std::span<const std::uint8_t>(boot).subspan(0x463ACu,0x2A6u))==
            sonic::vertex_normals::source_sha256,"retail leaf SHA differs");
        const auto original_mxcsr=_mm_getcsr();
        unsigned cases=0;
        for(unsigned count:{0u,1u,2u,7u,16u,33u}) for(unsigned shape=0;shape<7u;++shape)
            for(unsigned mode:{0u,1u,fpscr_fr_mask,fpscr_fr_mask|1u}) {
                _mm_setcsr(0x1F80u|((cases&3u)<<13u)|(cases&0x3Fu)|((cases&4u)?0x8040u:0u));
                Fixture native(boot,count,shape,mode),reference(boot,count,shape,mode);
                compare(native,reference);++cases;
            }
        // Read/read overlap is legal: identical index lists, identical face
        // normals, both shared, and adjacent strips with look-ahead overlap.
        // The fourth case also overlaps face-normal windows by two components.
        for(unsigned shared=0;shared<4u;++shared) {
            Fixture native(boot,7u,3u,0u),reference(boot,7u,3u,0u);
            for(auto* f:{&native,&reference}) {
                f->label+=" shared_case="+std::to_string(shared);
                for(unsigned mesh=0;mesh<3u;++mesh) {
                    const auto idx=indices+(shared==0u || shared==2u?0u:shared==3u?mesh*8u:mesh*0x1000u);
                    const auto normal=faces+(shared==1u || shared==2u?0u:shared==3u?mesh*4u:mesh*0x1000u);
                    const auto at=meshes+mesh*24u;
                    f->half(at,0xC000u|mesh);f->half(at+2u,1u);
                    f->put(at+4u,idx);f->put(at+12u,normal);
                    f->half(idx,3u);f->half(idx+2u,0u);f->half(idx+4u,1u);
                    f->half(idx+6u,1u);f->half(idx+8u,0x1234u);
                }
            }
            compare(native,reference);++cases;
        }
        for(unsigned variant=1u;variant<=4u;++variant) for(unsigned shape:{0u,1u,2u,3u}) {
            Fixture native(boot,9u,shape,variant&1u,variant),reference(boot,9u,shape,variant&1u,variant);
            compare(native,reference);++cases;
        }
        for(unsigned mode:{fpscr_cause_mask|fpscr_flag_mask,fpscr_fr_mask|fpscr_flag_mask|1u}) {
            Fixture native(boot,7u,3u,mode),reference(boot,7u,3u,mode);
            compare(native,reference);++cases;
        }
        {
            Fixture native(boot,7u,3u,0u),reference(boot,7u,3u,0u);
            for(auto* f:{&native,&reference}) {
                f->cpu.r[4]|=0x20000000u;f->cpu.r[15]|=0x20000000u;
                f->put(model+4u,normals|0x20000000u);f->put(model+12u,meshes|0x20000000u);
                for(unsigned mesh=0;mesh<3u;++mesh) {
                    f->put(meshes+mesh*24u+4u,(indices+mesh*0x1000u)|0x20000000u);
                    f->put(meshes+mesh*24u+12u,(faces+mesh*0x1000u)|0x20000000u);
                }
            }
            compare(native,reference);++cases;
            native.cpu.pc=entry;reference.cpu.pc=entry;compare(native,reference);++cases;
        }
        for(bool p2:{false,true}) for(unsigned mask:{1u,2u,4u,8u,16u,32u,63u}) {
            Fixture native(boot,7u,3u,0u),reference(boot,7u,3u,0u);
            for(auto* f:{&native,&reference}) {
                f->aliases(mask,p2);if(p2) f->bind_address_space(false);
            }
            compare(native,reference);++cases;
        }
        for(bool p2:{false,true}) {
            Fixture native(boot,7u,3u,0u),reference(boot,7u,3u,0u);
            for(auto* f:{&native,&reference}) {
                f->aliases(0u,p2);f->cpu.mmucr=1u;f->bind_address_space(true);
            }
            compare(native,reference);++cases;
        }
        for(unsigned operand=0;operand<6u;++operand) for(unsigned binding=0;binding<3u;++binding) {
            Fixture f(boot,7u,3u,0u);f.aliases(1u<<operand,true);
            f.cpu.mmucr=binding==2u?0u:1u;if(binding) f.bind_address_space(true);
            f.observe();decline(f,&f.immutable);++cases;
        }
        // Last strip reads one word beyond its final logical index. Admit it
        // precisely at RAM's end; reject the same records without that word.
        for(bool fit:{true,false}) {
            Fixture native(boot,7u,2u,0u),reference(boot,7u,2u,0u);
            for(auto* f:{&native,&reference}) {
                const auto where=fit?0x8CFFFFF6u:0x8CFFFFF8u;
                f->half(meshes+2u,1u);f->put(meshes+4u,where);f->half(where,3u);
                for(unsigned i=0;i<3u;++i) f->half(where+2u+i*2u,i);
                if(fit) f->half(where+8u,0x1234u);
            }
            if(fit) compare(native,reference);else decline(native,&native.immutable);
            ++cases;
        }
        for(unsigned scenario=0;scenario<26u;++scenario) {
            Fixture f(boot,7u,3u,0u,0u,scenario==15u);
            switch(scenario) {
            case 0:f.cpu.write_fpscr(0u);break;
            case 1:f.cpu.write_fpscr(fpscr_dn_mask|fpscr_pr_mask);break;
            case 2:f.cpu.write_fpscr(fpscr_dn_mask|fpscr_sz_mask);break;
            case 3:f.cpu.write_fpscr(fpscr_dn_mask|fpscr_enable_invalid_mask);break;
            case 4:f.cpu.sr|=sr_fd_mask;break;
            case 5:f.cpu.write_fpscr(fpscr_dn_mask|2u);break;
            case 6:f.half(meshes+24u,0x8000u);break;
            case 7:f.put(model+4u,faces);break;
            case 8:f.put(model+4u,stack_top-60u);break;
            case 9:f.cpu.r[15]=model+60u;break;
            case 10:f.put(model+4u,0x8CFFFFFCu);break;
            case 11:f.put(meshes+4u,0x8CFFFFFFu);break;
            case 12:f.put(meshes+12u,0x8CFFFFFCu);break;
            case 13:f.observe(false);break;
            case 14:f.put(entry,0u);break;
            case 15:break;
            case 16:f.cpu.memory.clear_direct_linear_alias_window();break;
            case 17:f.put(model+8u,0xFFFFFFFFu);break;
            case 18:f.cpu.r[15]=0x8C000020u;break;
            case 19:f.cpu.trap_pending=true;break;
            case 20:f.cpu.sleeping=true;break;
            case 21:f.cpu.write_sr(0u);break;
            case 22:f.put(meshes+24u+4u,normals);break; // input/output overlap still declines
            case 23:f.half(model+20u,4097u);break;
            case 24:
                (void)f.cpu.memory.add_watchpoint(normals&0x1FFFFFFFu,64u,MemoryWatchpointAccess::Write,
                    [](const MemoryAccessEvent&){throw std::runtime_error("decline invoked watchpoint");});break;
            case 25:f.put(meshes+24u+4u,stack_top-32u);break; // input/stack overlap still declines
            }
            decline(f,&f.immutable);++cases;
        }
        for(const auto kind:{NativePortImmutableRangeKind::Executable,NativePortImmutableRangeKind::ReadOnlyImage})
            for(auto address:{normals,stack_top-4u}) {
                Fixture f(boot,7u,3u,0u);f.observe();
                const std::array ranges{NativePortImmutableRange{address&0x1FFFFFFFu,4u,
                    native_port_immutable_range_mask(kind)}};
                NativePortImmutableWriteGuard protected_output{ranges};decline(f,&protected_output);++cases;
            }
        {
            Fixture f(boot,7u,3u,0u);decline(f,nullptr);++cases;
            f.immutable.observe_write(GuestWriteEvent{0x0C0563ACu,4u,CodeWriteSource::Cpu,true});
            decline(f,&f.immutable);++cases;
        }
        _mm_setcsr(original_mxcsr);
        std::cout<<"SONIC_VERTEX_NORMALS_PASS cases="<<cases
            <<" reference=sha_bound_retail_sh4 architectural_registers=exact RAM_stack=exact stores=ordered"
            <<" host_MXCSR=preserved accounting=native_hook_policy algorithm=original_V_times_F\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
