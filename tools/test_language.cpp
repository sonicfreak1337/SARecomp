#include "sonic_language.hpp"
#include "sonic_native_save_contract.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/native_port_platform.hpp"
#include "katana/runtime/native_port_texture_asset.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <tuple>
#include <stdexcept>
using namespace katana::runtime;
using namespace sonic::language;
namespace {
void require(bool value,const char* reason) {if(!value) throw std::runtime_error(reason);}
struct Services final:PlatformServices {
    std::string_view name() const noexcept override{return "language-reference";}
    std::uint32_t abi_version() const noexcept override{return 128u;}
    std::uint32_t guest_cycle_contract() const noexcept override{return 0u;}
    PlatformCapabilities capabilities() const noexcept override{return {};}
    void read_memory(std::uint32_t,std::span<std::uint8_t>) override{throw std::runtime_error("unexpected device read");}
    void write_memory(std::uint32_t,std::span<const std::uint8_t>) override{throw std::runtime_error("unexpected device write");}
    std::uint64_t scheduler_cycle() const noexcept override{return 0u;}
    std::optional<std::uint64_t> next_scheduler_event_cycle() const noexcept override{return {};}
    PlatformSchedulerResult consume_guest_cycles(std::uint64_t,std::size_t) override{return {};}
    std::optional<PlatformInterruptRequest> poll_interrupt() override{return {};}
    PlatformDmaResult start_dma(const PlatformDmaRequest&) override{throw std::runtime_error("unexpected DMA");}
    PlatformFallbackResult controlled_fallback(CpuState&,const PlatformFallbackRequest&) override{throw std::runtime_error("unexpected fallback");}
    bool prefetch(CpuState&,GuestInstructionOrigin,std::uint32_t) override{throw std::runtime_error("unexpected PREF");}
} services;
void reset(CpuState& cpu) {
    for(unsigned i=0;i<16;++i){cpu.r[i]=0x10203000u+i;cpu.fr[i]=0x3f800000u+i;cpu.xf[i]=i;}
    cpu.r[15]=0x8CF00000u;cpu.pr=0x8CF80000u;cpu.sr=sr_md_mask;
    cpu.t=true;cpu.macl=0xABCDu;cpu.mach=0x8765u;cpu.write_fpscr(0u);
    cpu.exception_generation=0u;cpu.trap_pending=false;
}
auto registers(const CpuState& cpu) {
    return std::tuple(cpu.r,cpu.fr,cpu.xf,cpu.pr,cpu.sr,cpu.macl,cpu.mach,cpu.t,cpu.read_fpscr());
}
void execute(CpuState& cpu,std::uint32_t entry) {
    cpu.pc=entry;
    const auto end=cpu.pr;
    for(unsigned steps=0;cpu.pc!=end && steps<1000000u;++steps) {
        (void)execute_dynamic_sh4_block(cpu,services,1u);
        require(cpu.exception_generation==0u,"retail language/CRC exception");
    }
    require(cpu.pc==end,"retail language/CRC did not return");
}
NativePortHookResult bridge(NativePortContext& context,std::uint32_t entry) noexcept {
    try {execute(*context.cpu,entry);return {NativePortHookAction::Return,0,0};}
    catch(...) {return {NativePortHookAction::Abort,0,1};}
}
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    require(bool(file),"test source missing");
    return {std::istreambuf_iterator<char>(file),{}};
}
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"content root and empty test directory required");
        const auto content=std::filesystem::absolute(argv[1]),folder=std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(folder);
        _putenv_s("SARECOMP_DISPLAY_CONFIG",(folder/"sonic-display.ini").string().c_str());
        const auto boot=read(content/"boot.bin");
        const auto advertise=decompress_native_port_prs(read(content/"SONICAD/ADVERTISE.PRS"));
        require(boot.size()==6735296u && advertise.size()<0x600000u,"unexpected retail sources");
        CpuState cpu{.memory=Memory{0u}};
        auto ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
        cpu.memory.map_region("ram",0x0C000000u,ram);
        std::copy(boot.begin(),boot.end(),ram->writable_bytes().begin()+0x10000u);
        std::copy(advertise.begin(),advertise.end(),ram->writable_bytes().begin()+0x900000u);
        const auto put=[&](std::uint32_t a,std::uint32_t v){cpu.memory.write_u32(a&0x1FFFFFFFu,v);};
        const auto byte=[&](std::uint32_t a){return cpu.memory.read_u8(a&0x1FFFFFFFu);};
        NativePortContext context;context.cpu=&cpu;context.aot.invoke_original=bridge;
        sonic::presentation::Settings settings;
        unsigned bit_cases=0,oracle_cases=0;
        // All option-byte combinations, including unrelated high/low flags.
        for(unsigned original=0;original<256;++original)
        for(int text=-1;text<5;++text) for(int voice=-1;voice<2;++voice) for(int subtitle=-1;subtitle<2;++subtitle) {
            settings.text_language=text;settings.voice_language=voice;settings.subtitles=subtitle;
            const auto next=save_options(std::uint8_t(original),settings);
            const unsigned mask=(text<0?0:0x70)|(voice<0?0:0x0c)|(subtitle<0?0:2);
            require((next&~mask)==(original&~mask),"unrequested option changed");
            ++bit_cases;
        }
        // Compare encoding against the original three option setters.
        for(unsigned original:{0u,255u,0x95u,0x2au})
        for(int kind=0;kind<3;++kind) for(int value=0;value<(kind==0?5:2);++value) {
            settings={};
            if(kind==0) settings.text_language=value;
            if(kind==1) settings.voice_language=value;
            if(kind==2) settings.subtitles=value;
            reset(cpu);put(selected_record,0u);
            cpu.memory.write_u8((records+options_offset)&0x1FFFFFFFu,std::uint8_t(original));
            cpu.r[4]=kind==2?1-value:value;
            execute(cpu,kind==0?0x8C901C04u:kind==1?0x8C901B48u:0x8C901ABCu);
            require(byte(records+options_offset)==save_options(std::uint8_t(original),settings),"retail setter mismatch");
            ++oracle_cases;
        }
        settings={};settings.text_language=4;settings.voice_language=1;settings.subtitles=1;
        sonic::presentation::save_settings(folder/"sonic-display.ini",settings);
        sonic::presentation::initialize(folder/"game.exe");
        put(selected_record,1u);
        for(unsigned i=0;i<3*record_bytes;++i) cpu.memory.write_u8((records+i)&0x1FFFFFFFu,std::uint8_t(i*37u));
        const auto before=std::vector<std::uint8_t>(ram->bytes().begin()+0x7988E0u,ram->bytes().begin()+0x7988E0u+3*record_bytes);
        reset(cpu);cpu.pr=0x8C000000u;
        require(sonic_language_loaded(context).action==NativePortHookAction::ContinueOriginal &&
            byte(records+record_bytes+options_offset)==before[record_bytes+options_offset],"unconfirmed load changed options");
        reset(cpu);cpu.pr=0x8C011750u;
        const auto state=registers(cpu);
        require(sonic_language_loaded(context).action==NativePortHookAction::ContinueOriginal,"load hook failed");
        require(state==registers(cpu),"load hook changed CPU ABI");
        for(unsigned i=0;i<before.size();++i)
            require(byte(records+i)==(i==record_bytes+options_offset?save_options(before[i],settings):before[i]),"load changed another record/progress byte");
        put(selected_record,3u);
        const auto invalid=byte(records+3*record_bytes+options_offset);
        require(sonic_language_save(context).action==NativePortHookAction::ContinueOriginal &&
            byte(records+3*record_bytes+options_offset)==invalid,"invalid save slot was modified");
        put(selected_record,1u);
        reset(cpu);cpu.pr=0x8C054872u;
        const auto initial=registers(cpu);
        require(sonic_language_initial(context).action==NativePortHookAction::ContinueOriginal &&
            registers(cpu)==initial && cpu.memory.read_u32(text_global&0x1FFFFFFFu)==4 &&
            cpu.memory.read_u32(voice_global&0x1FFFFFFFu)==1 && byte(subtitles_global)==1,"initial language globals/ABI failed");
        // Production wrappers preserve every original getter side effect.
        for(auto [entry,hook,value]:{std::tuple{0x8C901BC0u,&sonic_language_text,4u},
                std::tuple{0x8C901AF8u,&sonic_language_voice,1u},std::tuple{0x8C901A94u,&sonic_language_subtitles,0u}}) {
            reset(cpu);execute(cpu,entry);cpu.r[0]=value;const auto expected=registers(cpu);
            reset(cpu);cpu.pc=entry;
            require(hook(context).action==NativePortHookAction::Return,"getter bridge failed");
            require(registers(cpu)==expected,"getter changed scratch/stack/FP ABI");
        }
        // Use retail CRC, the real file-backed VMU provider, then retail load
        // validation and getters with all host language overrides disabled.
        cpu.memory.write_u8((records+record_bytes+options_offset)&0x1FFFFFFFu,before[record_bytes+options_offset]);
        require(sonic_language_save(context).action==NativePortHookAction::ContinueOriginal &&
            byte(records+record_bytes+options_offset)==save_options(before[record_bytes+options_offset],settings),"save serializer did not merge options");
        for(unsigned i=0;i<3;++i) {
            reset(cpu);cpu.r[4]=records+i*record_bytes;execute(cpu,0x8C088634u);
            put(records+i*record_bytes,cpu.r[0]&0xffffu);
        }
        std::vector<std::byte> payload(4096);
        std::copy_n(reinterpret_cast<const std::byte*>(ram->bytes().data()+0x7988E0u),3*record_bytes,payload.begin());
        NativePortPlatformConfig platform_config;
        platform_config.content_root=content;platform_config.user_data_root=folder/"user-data";
        platform_config.project_id="sonic-language-test";platform_config.require_gamepad_backend=false;
        const std::array units{NativePortSaveUnitConfig{{0,0},"language-test-a1",true,true}};
        NativePortSaveProviderConfig save_config;save_config.provider_id="language-test";
        save_config.profile_identity="sonic-language-test-v1";save_config.units=units;
        {
            NativePortPlatformServices platform(platform_config);NativePortSaveProvider saves(platform,save_config);
            const auto written=sonic::native_save_sdk::write(saves,{0,0},"LANGTEST",payload,{},0,false);
            require(written.completion.error==NativePortSaveError::None,"VMU write failed");
        }
        {
            NativePortPlatformServices platform(platform_config);NativePortSaveProvider saves(platform,save_config);
            const auto loaded=sonic::native_save_sdk::read(saves,{0,0},"LANGTEST",0,0);
            require(loaded.completion.error==NativePortSaveError::None && loaded.payload==payload,"VMU reopen differs");
            std::copy_n(reinterpret_cast<const std::uint8_t*>(loaded.payload.data()),3*record_bytes,ram->writable_bytes().begin()+0x7988E0u);
        }
        sonic::presentation::save_settings(folder/"sonic-display.ini",{});
        sonic::presentation::initialize(folder/"game.exe");
        reset(cpu);cpu.r[4]=records;execute(cpu,0x8C088708u);require(cpu.r[0]==1,"retail rejected saved checksums");
        for(auto [entry,value]:{std::pair{0x8C901BC0u,4u},std::pair{0x8C901AF8u,1u},std::pair{0x8C901A94u,0u}}) {
            reset(cpu);execute(cpu,entry);
            if(cpu.r[0]!=value) std::cerr<<"getter="<<std::hex<<entry<<" actual="<<cpu.r[0]<<" expected="<<value<<" options="<<unsigned(byte(records+record_bytes+options_offset))<<std::dec<<'\n';
            require(cpu.r[0]==value,"retail save load reverted language");
        }
        // Disabling overrides must not read RAM, even in an uninitialized context.
        NativePortContext empty;
        require(sonic_language_save(empty).action==NativePortHookAction::ContinueOriginal &&
            sonic_language_initial(empty).action==NativePortHookAction::ContinueOriginal,"game setting changed baseline");
        std::cout<<"SONIC_LANGUAGE_TESTS_OK bit_cases="<<bit_cases<<" retail_setters="<<oracle_cases
            <<" getter_abi=ok native_vmu_reopen=ok retail_crc=ok host_override_disabled=ok\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
