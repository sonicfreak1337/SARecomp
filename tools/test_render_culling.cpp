#include "sonic_render_culling.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

using namespace katana::runtime;
namespace {
struct Services final : PlatformServices {
    std::string_view name() const noexcept override { return "cull-reference"; }
    std::uint32_t abi_version() const noexcept override { return 128u; }
    std::uint32_t guest_cycle_contract() const noexcept override { return 0u; }
    PlatformCapabilities capabilities() const noexcept override { return {}; }
    void read_memory(std::uint32_t,std::span<std::uint8_t>) override { throw std::runtime_error("unexpected service read"); }
    void write_memory(std::uint32_t,std::span<const std::uint8_t>) override { throw std::runtime_error("unexpected service write"); }
    std::uint64_t scheduler_cycle() const noexcept override { return 0u; }
    std::optional<std::uint64_t> next_scheduler_event_cycle() const noexcept override { return {}; }
    PlatformSchedulerResult consume_guest_cycles(std::uint64_t,std::size_t) override { return {}; }
    std::optional<PlatformInterruptRequest> poll_interrupt() override { return {}; }
    PlatformDmaResult start_dma(const PlatformDmaRequest&) override { throw std::runtime_error("unexpected DMA"); }
    PlatformFallbackResult controlled_fallback(CpuState&,const PlatformFallbackRequest&) override { throw std::runtime_error("unexpected fallback"); }
    bool prefetch(CpuState&,GuestInstructionOrigin,std::uint32_t) override { throw std::runtime_error("unexpected PREF"); }
};
void require(bool ok,const char* why) { if (!ok) throw std::runtime_error(why); }
constexpr std::uint32_t model=0x8cf00000u, mesh=model+0x100u, material=model+0x200u, gbr=0x8cfff000u;
std::uint32_t bits(float x) { return std::bit_cast<std::uint32_t>(x); }
void put(CpuState& c,std::uint32_t a,std::uint32_t v) { c.memory.write_u32(a&0x1fffffffu,v); }
void initialize(CpuState& c,std::uint32_t entry,float x,float y,float z,float extra,std::uint32_t fpscr,unsigned variant) {
    c.r.fill(0x13579bdfu); c.fr.fill(bits(0.25f)); c.xf.fill(0u);
    for(unsigned i=0;i<4;++i) c.xf[i*5]=bits(1.0f);
    c.r[4]=model; c.r[15]=gbr-256u; c.gbr=gbr; c.pc=entry; c.pr=0x8cffeff0u;
    c.sr=sr_md_mask; c.t=true; c.fpul=0x12345678u; c.mach=0xaabbccdd; c.macl=1234u;
    c.write_fpscr(fpscr); c.exception_generation=0; c.trap_pending=false;
    c.pending_guest_cycles=0;
    c.fr[4]=bits(x); c.fr[5]=bits(y); c.fr[6]=bits(z); c.fr[7]=bits(2.0f);
    c.fr[12]=bits(-extra); c.fr[13]=bits(640.0f+extra);
    for(unsigned i=0;i<64;i+=4) put(c,gbr+i,0x5a5a0000u+i);
    put(c,gbr+8,bits(320.0f)); put(c,gbr+12,bits(1.0f)); put(c,gbr+16,bits(1.0f));
    put(c,model+12,mesh); put(c,model+16,material);
    put(c,model+24,bits(x)); put(c,model+28,bits(y)); put(c,model+32,bits(z)); put(c,model+36,bits(2.0f));
    put(c,mesh,2u); put(c,material+2*20+16,0x12345678u);
    put(c,0x8c88f524u,bits(320)); put(c,0x8c88f530u,bits(320)); put(c,0x8c88f534u,bits(240));
    put(c,0x8c88f540u,bits(0)); put(c,0x8c88f544u,bits(0)); put(c,0x8c88f548u,bits(640)); put(c,0x8c88f54cu,bits(480));
    put(c,0x8c88f550u,bits(1)); put(c,0x8c88f554u,bits(1000));
    put(c,0x8c88f558u,bits(1)); put(c,0x8c88f55cu,bits(1));
    put(c,0x8c88f56cu,(variant&1)?0x34u:0u);
    put(c,0x8c88f5a0u,0x00ffffffu); put(c,0x8c88f5a4u,0x80000000u);
    put(c,0x8c8ffe1cu,variant&1u); put(c,0x8c8ffe20u,0xffffff00u); put(c,0x8c8ffe24u,0x81u);
    put(c,0x8c754e08u,(variant>>1)&1u);
}
void equal(const CpuState& a,const CpuState& b) {
    require(a.r==b.r,"GPR mismatch"); require(a.fr==b.fr,"FR mismatch"); require(a.xf==b.xf,"XF changed");
    require(a.read_fpscr()==b.read_fpscr(),"FPSCR mismatch"); require(a.read_sr()==b.read_sr(),"SR/T mismatch");
    require(a.fpul==b.fpul && a.macl==b.macl && a.mach==b.mach,"FPUL/MAC mismatch");
    require(a.pr==b.pr && a.gbr==b.gbr,"PR/GBR changed");
    require(a.exception_generation==0 && b.exception_generation==0,"unexpected guest exception");
    for(unsigned i=0;i<64;i+=4) require(a.memory.read_u32((gbr+i)&0x1fffffffu)==b.memory.read_u32((gbr+i)&0x1fffffffu),"GBR material state mismatch");
    for(unsigned i=0;i<16;i+=4) require(a.memory.read_u32(0x0c88f540u+i)==b.memory.read_u32(0x0c88f540u+i),"shared viewport bounds changed");
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"boot.bin argument required");
        std::ifstream f(argv[1],std::ios::binary);
        std::vector<std::uint8_t> boot{std::istreambuf_iterator<char>(f),{}};
        require(boot.size()>=0x28da0u && boot.size()<=0xff0000u,"missing reference boot image");
        CpuState original{.memory=Memory{0u}}, native{.memory=Memory{0u}};
        for(auto* c:{&original,&native}) {
            auto ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
            std::copy(boot.begin(),boot.end(),ram->writable_bytes().begin()+0x10000u);
            c->memory.map_region("ram",0x0c000000u,ram);
        }
        Services services;
        unsigned cases=0,added_visible=0;
        for(auto entry:{0x8c03718cu,0x8c038d00u})
        for(float extra:{0.0f,106.66667f,253.33333f}) {
            const auto left=entry==0x8c03718cu?0x0c0371d2u:0x0c038d4cu;
            const auto right=entry==0x8c03718cu?0x0c0371e0u:0x0c038d5au;
            // Test oracle: execute the retail SH-4 bytes. Only widescreen
            // FCMP source registers change to FR12/13, preloaded with the two
            // host X bounds. FR8 and all Y instructions remain retail bytes.
            original.memory.write_u16(left,entry==0x8c03718cu?(extra?0xf0c5u:0xf085u):(extra?0xf4c5u:0xf485u));
            original.memory.write_u16(right,extra?0xfbd5u:0xfb85u);
            for(auto fpscr:{0u,fpscr_dn_mask,fpscr_dn_mask|1u})
            for(float x:{-400.0f,-180.0f,-120.0f,0.0f,120.0f,180.0f,400.0f})
            for(float y:{-300.0f,0.0f,300.0f})
            for(float z:{-10.0f,100.0f,1200.0f})
            for(unsigned variant=0;variant<4;++variant) {
                initialize(original,entry,x,y,z,extra,fpscr,variant);
                initialize(native,entry,x,y,z,extra,fpscr,variant);
                unsigned blocks=0;
                while(original.pc!=original.pr && ++blocks<32u) {
                    (void)execute_dynamic_sh4_block(original,services);
                    require(original.exception_generation==0,"reference exception");
                }
                require(original.pc==original.pr,"reference did not return");
                if(entry==0x8c03718cu) sonic::presentation::basic_model_cull(native,extra);
                else sonic::presentation::draw_sphere_cull(native,extra);
                equal(original,native); ++cases;
                if(extra && std::abs(x)==120.0f && y==0 && z==100 && !(variant&2)) {
                    require(entry==0x8c03718cu?!native.t:native.r[0]==1u,"side-band object still culled");
                    ++added_visible;
                }
            }
        }
        require(sonic::presentation::draw_sphere_caller(0x8c0366b6u),"render caller omitted");
        require(sonic::presentation::draw_sphere_caller(0x8c061400u),"hint monitor still uses 4:3 bounds");
        require(!sonic::presentation::draw_sphere_caller(0x8c046854u),"movement caller widened");
        require(sonic::presentation::draw_sphere_caller(0x8c0599e0u),"presentation-only motion omitted");
        require(!sonic::presentation::draw_sphere_caller(0x8c000000u),"unknown caller widened");
        std::cout<<"SONIC_CULL_REFERENCE_OK cases="<<cases<<" sideband_visible="<<added_visible<<"\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
