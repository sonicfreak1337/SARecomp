#include "sonic_camera.hpp"
#include "sonic_camera_orbit.hpp"
#include "sonic_camera_policy.hpp"
#include "sonic_presentation.hpp"
#include "katana/runtime/runtime.hpp"
#include <algorithm>
#include <bit>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <vector>
using namespace sonic::camera;
using namespace katana::runtime;
namespace {
void require(bool value,const char* why) { if(!value) throw std::runtime_error(why); }
bool near(float a,float b,float tolerance=0.001f) {return std::abs(a-b)<tolerance;}
bool near(Vec3 a,Vec3 b) {return length(a-b)<0.002f;}
struct Host final:NativePortHostServices {
    std::uint64_t now=1'000'000'000;
    std::uint64_t monotonic_time_nanoseconds() const noexcept override {return now;}
    NativePortLifecycleState poll_lifecycle() override {return {};}
    void synchronize_simulation_boundary() override {}
    void begin_frame(std::uint64_t) override {}
    void present_frame(std::uint64_t) override {}
    std::uint64_t presented_frames() const noexcept override {return 0;}
};
auto registers(const CpuState& c) {return std::tuple(c.r,c.fr,c.xf,c.pr,c.sr,c.macl,c.mach,c.t,c.read_fpscr());}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"provide an isolated test configuration directory");
        const auto folder=std::filesystem::absolute(argv[1]);
        std::filesystem::create_directories(folder);
        require(right_stick(8000,0,true).x==0 && right_stick(32767,0,false).x==0,"deadzone/disconnect");
        const auto diagonal=right_stick(32767,-32768,true);
        require(near(std::hypot(diagonal.x,diagonal.y),1) && diagonal.y<0,"radial stick normalization");
        Orbit orbit;
        require(orbit.enter({0,12,40},{0,7,0}),"valid orbit entry");
        auto first=orbit.update({0,7,0},{},0);
        Pose pose;
        // 150 degrees/s for 2.4 seconds must return to the same point.
        for(unsigned i=0;i<144;++i) pose=orbit.update({0,7,0},{1,0},1.0f/60);
        require(near(first.eye,pose.eye),"complete 360 degree orbit");
        for(unsigned i=0;i<100;++i) pose=orbit.update({0,7,0},{0,1},1.0f/60);
        require(near(pose.pitch,Orbit::maximum_pitch),"upward pitch limit");
        for(unsigned i=0;i<100;++i) pose=orbit.update({0,7,0},{0,-1},1.0f/60);
        require(near(pose.pitch,Orbit::minimum_pitch),"downward pitch limit");
        const auto moved=orbit.update({23,8,-12},{},0);
        require(near(moved.eye-pose.eye,{23,1,-12}) && near(length(moved.eye-moved.target),orbit.radius()),"character-locked follow");
        Orbit slow,fast;slow.enter({0,12,40},{0,7,0});fast.enter({0,12,40},{0,7,0});
        Pose a,b;
        for(unsigned i=0;i<30;++i) a=slow.update({0,7,0},{0.6f,0.1f},1.0f/30);
        for(unsigned i=0;i<60;++i) b=fast.update({0,7,0},{0.6f,0.1f},1.0f/60);
        require(near(a.eye,b.eye),"wall-time response changes with frame rate");

        CpuState cpu{.memory=Memory{0u}};
        NativePortContext context;context.cpu=&cpu;context.title_state=&cpu;
        Host host;context.host=&host;
        sonic::presentation::Settings settings;
        const auto configure=[&](Style style) {
            settings.camera_style=style;
            sonic::presentation::save_settings(folder/"sonic-display.ini",settings);
            sonic::presentation::initialize(folder/"game.exe");
            reset_timeline();
        };
        configure(Style::Original);
        // Unmapped memory proves Original requires no guest reads or writes.
        require(sonic_recompiled_camera_publish(context).action==NativePortHookAction::ContinueOriginal,"original passthrough");
        auto ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
        cpu.memory.map_region("ram",0x0C000000u,ram);
        constexpr std::uint32_t cam=0x8CF10000u,player=0x8CF20000u,control=0x8CF30000u;
        const auto put=[&](std::uint32_t p,std::uint32_t v){cpu.memory.write_u32(p&0x1fffffffu,v);};
        const auto half=[&](std::uint32_t p,std::uint16_t v){cpu.memory.write_u16(p&0x1fffffffu,v);};
        const auto byte=[&](std::uint32_t p,std::uint8_t v){cpu.memory.write_u8(p&0x1fffffffu,v);};
        const auto xyz=[&](std::uint32_t p,Vec3 v){put(p+0x20u,std::bit_cast<std::uint32_t>(v.x));put(p+0x24u,std::bit_cast<std::uint32_t>(v.y));put(p+0x28u,std::bit_cast<std::uint32_t>(v.z));};
        const auto get=[&](std::uint32_t p){return cpu.memory.read_u32(p&0x1fffffffu);};
        const auto eye=[&](){return Vec3{std::bit_cast<float>(get(cam+0x20)),std::bit_cast<float>(get(cam+0x24)),std::bit_cast<float>(get(cam+0x28))};};
        const auto setup=[&]() {
            reset_timeline();cpu.pr=0x8C01991Cu;cpu.r[4]=cam;
            half(0x8C7492F4,15);half(0x8C19DD64,0);half(0x8C19DD66,1);put(0x8C7491EC,0);
            put(0x8C18B72C,cam);put(0x8C111F88,control);put(0x8C78C548,player);put(0x8C6B6FAC,0);
            byte(cam,2);byte(control+8,1);byte(control+6,1);byte(control+7,2);put(control+12,0x8C01E6A6);byte(0x8C78B39D,0);
            half(0x8C7492FA,1);half(0x8C7492FC,0);
            xyz(player,{0,0,0});xyz(cam,{0,12,40});put(cam+0x1c,1234);
            ++context.frame_index;host.now+=33'333'333;
        };
        configure(Style::Recompiled);
        unsigned guard_cases=0;
        const auto unchanged=[&]() {
            const auto before=std::vector(ram->bytes().begin(),ram->bytes().end());
            const auto regs=registers(cpu);
            require(sonic_recompiled_camera_publish(context).action==NativePortHookAction::ContinueOriginal,"guard action");
            require(std::equal(before.begin(),before.end(),ram->bytes().begin()) && regs==registers(cpu),"script/pause guard mutated guest");
            ++guard_cases;
        };
        for(unsigned level:{2,3,4,5,255}) {setup();byte(control+8,std::uint8_t(level));unchanged();}
        for(unsigned mode:{0,14,16,17}) {setup();half(0x8C7492F4,std::uint16_t(mode));unchanged();}
        setup();half(0x8C19DD64,1);unchanged();
        setup();half(0x8C19DD66,0);unchanged();
        setup();put(0x8C7491EC,1);unchanged();
        setup();put(0x8C6B6FAC,1);unchanged();
        setup();cpu.pr=0x8C01991Eu;unchanged();
        setup();put(0x8C78C548,0);unchanged();
        setup();byte(cam,1);unchanged();
        for(unsigned type:{26,27,28,29,48,49,51,52,53,54,55,56,57,62,63,65,66,67,68,69,70,65535}) {
            setup();half(control+6,std::uint16_t(type));unchanged();
        }
        setup();put(control+12,0x8C024538);unchanged();
        for(unsigned level:{0,1}) {
            setup();byte(control+8,std::uint8_t(level));
            const auto before=std::vector(ram->bytes().begin(),ram->bytes().end());
            const auto regs=registers(cpu);
            sonic_recompiled_camera_publish(context);
            NativePortInputSnapshot input;input.gamepads[0].connected=true;input.gamepads[0].right_stick_x_raw=32767;
            ++context.frame_index;host.now+=33'333'333;sample_input(context,input,false);
            sonic_recompiled_camera_publish(context);
            require(eye().x<0,"right-stick orbit was not applied");
            const auto once=eye();host.now+=1'000'000;
            sonic_recompiled_camera_publish(context);
            require(near(once,eye()),"duplicate frame integrated twice");
            ++context.frame_index;host.now+=32'333'333;sample_input(context,input,false);
            sonic_recompiled_camera_publish(context);
            require(near(std::atan2(eye().x,eye().z),-2*(150*pi/180)/30,0.0001f),"duplicate publisher lost elapsed time");
            require(std::equal(before.begin(),before.begin()+0xF10014,ram->bytes().begin()) &&
                std::equal(before.begin()+0xF1002c,before.end(),ram->bytes().begin()+0xF1002c) &&
                get(cam+0x1c)==1234 && regs==registers(cpu),"camera changed unrelated memory/CPU/roll");
            reset_timeline();xyz(cam,{40,12,0});++context.frame_index;host.now+=33'333'333;
            sonic_recompiled_camera_publish(context);
            require(near(eye(),{40,12,0}),"quicksave reset did not reacquire original camera");
            byte(0x8C78B39D,1);xyz(cam,{-40,12,0});++context.frame_index;host.now+=33'333'333;
            sonic_recompiled_camera_publish(context);
            require(near(eye(),{-40,12,0}),"character change retained stale orbit");
        }
        std::cout<<"SONIC_CAMERA_TEST_OK orbit_360=1 vertical=1 follow=1 raw_stick=1 guard_cases="<<guard_cases<<" restore=1 abi=1\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"SONIC_CAMERA_TEST_FAILED "<<error.what()<<'\n';return 1;}
}
