#include "sonic_camera.hpp"
#include "sonic_camera_orbit.hpp"
#include "sonic_camera_policy.hpp"
#include "sonic_camera_world.hpp"
#include "sonic_camera_input.hpp"
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
        require(right_stick(3000,0,true).x==0 && right_stick(32767,0,false).x==0,"deadzone/disconnect");
        require(right_stick(8000,0,true).x>0.1f,"small deliberate stick movement is sluggish");
        require(right_stick(32767,3000,true).y==0 && right_stick(-32768,-3000,true).y==0,"horizontal turning admits vertical center noise");
        const auto sony=dualsense_right_stick(32511,32511,0,65535,0,65535);
        require(right_stick(sony.x,sony.y,true).x==0 && right_stick(sony.x,sony.y,true).y==0,"observed DualSense rest must not move the camera");
        require(dualsense_right_stick(65535,32768,0,65535,0,65535).x==32767 &&
            dualsense_right_stick(32768,0,0,65535,0,65535).y==32767 &&
            dualsense_right_stick(32768,65535,0,65535,0,65535).y==-32767,"DualSense independent stick axes/signs");
        const auto diagonal=right_stick(32767,-32768,true);
        require(near(std::hypot(diagonal.x,diagonal.y),1) && diagonal.y<0,"radial stick normalization");
        Orbit orbit;
        require(orbit.enter({0,12,40},{0,7,0}),"valid orbit entry");
        auto first=orbit.update({0,7,0},{},0);
        Pose pose;
        // 240 degrees/s for 1.5 seconds must return to the same point.
        for(unsigned i=0;i<90;++i) pose=orbit.update({0,7,0},{1,0},1.0f/60);
        require(near(first.eye,pose.eye),"complete 360 degree orbit");
        for(unsigned i=0;i<100;++i) pose=orbit.update({0,7,0},{0,1},1.0f/60);
        require(near(pose.pitch,Orbit::minimum_pitch),"up stick must raise the camera");
        for(unsigned i=0;i<100;++i) pose=orbit.update({0,7,0},{0,-1},1.0f/60);
        require(near(pose.pitch,Orbit::maximum_pitch),"down stick must lower the camera");
        const auto held=pose;
        for(unsigned i=0;i<600;++i) pose=orbit.update({0,7,0},right_stick(1000,2000,true),1.0f/60);
        require(near(pose.eye,held.eye),"released stick must hold elevation without sinking");
        Orbit noise;noise.enter({0,18,40},{0,7,0});
        const auto quiet=noise.update({0,7,0},{},0);
        for(unsigned i=0;i<600;++i) pose=noise.update({0,7,0},right_stick(32767,3000,true),1.0f/60);
        require(near(pose.eye.y,quiet.eye.y),"ten seconds of horizontal stick must preserve height");
        pose=held;
        const auto moved=orbit.update({23,8,-12},{},0);
        require(near(moved.eye-pose.eye,{23,1,-12}) && near(length(moved.eye-moved.target),orbit.radius()),"character-locked follow");
        Orbit slow,fast;slow.enter({0,12,40},{0,7,0});fast.enter({0,12,40},{0,7,0});
        Pose a,b;
        for(unsigned i=0;i<30;++i) a=slow.update({0,7,0},{0.6f,0.1f},1.0f/30);
        for(unsigned i=0;i<60;++i) b=fast.update({0,7,0},{0.6f,0.1f},1.0f/60);
        require(near(a.eye,b.eye),"wall-time response changes with frame rate");

        CollisionMesh wall;
        wall.assign({{{-100,-100,10},{100,-100,10},{100,100,10}},{{-100,-100,10},{100,100,10},{-100,100,10}}});
        require(near(wall.sweep({0,0,0},{0,0,100},2).fraction,0.08f),"swept volume tunnels through a thin wall");
        require(near(wall.sweep({0,0,20},{0,0,0},2).fraction,0.4f),"camera wall must be two-sided");
        require(wall.sweep({0,0,9},{0,0,0},2).fraction==0,"initial overlap");
        require(wall.sweep({0,0,0},{10,0,0},2).fraction==1,"parallel free path");
        CollisionMesh edge;
        edge.assign({{{0,-10,10},{10,-10,10},{0,10,10}}});
        const auto corner=edge.sweep({-1,0,0},{-1,0,20},2).fraction;
        require(corner>0.4f && corner<0.45f,"near-plane edge collision missed by center ray");
        CollisionBoom boom;
        require(boom.update(40,40,0)==40 && boom.update(40,8,0.016f)==8,"wall retraction must be immediate");
        const auto recovery=boom.update(40,40,0.033f);
        require(recovery>8 && recovery<40 && boom.update(40,10,0.033f)==10,"outward recovery crosses a new obstacle");

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
        require(sonic_recompiled_camera_original_step(context).action==NativePortHookAction::ContinueOriginal,"original preupdate passthrough");
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
        setup();unchanged(); // No stick: retain the original camera completely.
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
            require(near(std::atan2(eye().x,eye().z),-2*(240*pi/180)/30,0.0001f),"duplicate publisher lost elapsed time");
            const auto stable=eye();
            input.gamepads[0].right_stick_x_raw=0;input.gamepads[0].right_stick_y_raw=2000;
            for(unsigned tick=0;tick<90;++tick) {
                ++context.frame_index;host.now+=33'333'333;sample_input(context,input,false);
                sonic_recompiled_camera_publish(context);
            }
            require(near(eye(),stable),"neutral runtime camera drifted after manual control");
            cpu.pr=0x8C01989Eu;sonic_recompiled_camera_publish(context);cpu.pr=0x8C01991Cu;
            xyz(cam,{0,1,40});++context.frame_index;host.now+=33'333'333;sample_input(context,input,false);
            sonic_recompiled_camera_publish(context);
            require(near(eye(),stable),"foreign publisher reset the chosen camera height");
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
        {
            setup();sonic_recompiled_camera_publish(context);
            NativePortInputSnapshot input;input.gamepads[0].connected=true;
            const auto tick=[&](Vec3 p,std::int16_t stick_x) {
                ++context.frame_index;host.now+=33'333'333;
                cpu.pr=0x8C019918;sonic_recompiled_camera_original_step(context);
                xyz(player,p);xyz(cam,p+Vec3{0,12,40});
                input.gamepads[0].right_stick_x_raw=stick_x;
                sample_input(context,input,false);cpu.pr=0x8C01991C;
                sonic_recompiled_camera_publish(context);
            };
            tick({},32767);
            const auto manual=eye();const auto regs=registers(cpu);
            cpu.pr=0x8C019918;
            sonic_recompiled_camera_original_step(context);
            cpu.pr=0x8C01991C;
            require(near(eye(),{0,12,40}) && regs==registers(cpu),"manual pose contaminated Original camera history/CPU");
            for(unsigned i=0;i<105;++i) tick({},0);
            require(near(eye(),manual),"idle standing must retain manual view beyond three seconds");
            // Movement after the three-second timeout starts a return; it
            // must not snap directly to the original view on the first tick.
            tick({0.1f,0,0},0);
            require(!near(eye(),{0.1f,12,40}),"return to Original snapped instead of blending");
            for(unsigned i=2;i<65;++i) tick({float(i)*0.1f,0,0},0);
            require(near(eye(),{6.4f,12,40}),"walking after three seconds did not restore Original");
            tick({6.4f,0,0},32767);
            require(!near(eye(),{6.4f,12,40}),"stick did not immediately interrupt Original follow");
            // Scene-authored edits between frames must never be overwritten
            // by the host's saved Original pose.
            xyz(cam,{88,55,22});cpu.pr=0x8C019918;
            sonic_recompiled_camera_original_step(context);cpu.pr=0x8C01991C;
            require(near(eye(),{88,55,22}),"preupdate clobbered a newly authored event camera");
        }
        {
            // PAL-shaped RAM fixture exercises real reader layout, registry
            // changes and in-place geometry mutation through the production path.
            constexpr std::uint32_t land=0x8CF40000,col=0x8CF40100,obj=0x8CF40200,model=0x8CF40300;
            constexpr std::uint32_t points=0x8CF40400,meshes=0x8CF40500,stream=0x8CF40600;
            const auto vec=[&](std::uint32_t at,Vec3 v){put(at,std::bit_cast<std::uint32_t>(v.x));put(at+4,std::bit_cast<std::uint32_t>(v.y));put(at+8,std::bit_cast<std::uint32_t>(v.z));};
            put(0x8C759634,land);put(0x8C19E8B8,1);half(land,1);put(land+12,col);
            vec(col,{0,0,10});put(col+12,std::bit_cast<std::uint32_t>(30.0f));put(col+24,obj);put(col+32,1);
            put(obj,7);put(obj+4,model);put(model,points);put(model+8,4);put(model+12,meshes);half(model+20,1);
            vec(model+24,{0,0,10});put(model+36,std::bit_cast<std::uint32_t>(30.0f));
            vec(points,{-10,-10,10});vec(points+12,{10,-10,10});vec(points+24,{-10,10,10});vec(points+36,{10,10,10});
            half(meshes,0x4000);half(meshes+2,1);put(meshes+4,stream);
            for(unsigned i=0;i<4;++i) half(stream+i*2,std::uint16_t(i));
            CollisionWorld world;
            auto hit=world.sweep(cpu.memory,{0,0,0},{0,0,20},2);
            require(hit.valid && near(hit.fraction,0.4f) && hit.hit_object==obj,"static PAL world collision");
            hit=world.sweep(cpu.memory,{0,0,0},{0,0,20},2);
            require(hit.valid && hit.rebuilt==0,"unchanged geometry rebuilt its acceleration structure");
            put(col+32,0x201);hit=world.sweep(cpu.memory,{0,0,0},{0,0,20},2);
            require(hit.valid && hit.fraction==1,"NoCamCollision surface blocks the camera");
            put(col+32,2);hit=world.sweep(cpu.memory,{0,0,0},{0,0,20},2);
            require(hit.valid && hit.fraction==1,"water-only surface blocks the camera");
            put(col+32,1);put(0x8C19E8B8,0);
            require(world.sweep(cpu.memory,{0,0,0},{0,0,20},2).fraction==1,"disabled static land remained cached");
            half(0x8C759644,1);put(0x8C759648,1);put(0x8C75964C,obj);put(0x8C759650,player);
            put(obj,6);vec(obj+8,{0,0,5});
            hit=world.sweep(cpu.memory,{0,0,0},{0,0,20},2);
            require(hit.valid && near(hit.fraction,0.65f),"registered moving solid transform");
            put(0x8C19E8B8,1);put(0x8C759634,0);
            hit=world.sweep(cpu.memory,{0,0,0},{0,0,20},2);
            require(hit.valid && near(hit.fraction,0.65f),"null static table discarded registered dynamic solids");
            put(0x8C759634,land);put(0x8C19E8B8,0);
            put(obj,4);put(obj+0x18,0x40000000); // FSCA uses only the low 16 angle bits.
            hit=world.sweep(cpu.memory,{0,0,0},{0,0,20},2);
            require(hit.valid && near(hit.fraction,0.65f),"high angle bits rotated the collision mesh");
            for(unsigned i=0;i<4;++i) put(points+i*12+8,std::bit_cast<std::uint32_t>(20.0f));
            hit=world.sweep(cpu.memory,{0,0,0},{0,0,40},2);
            require(hit.valid && near(hit.fraction,0.575f) && hit.rebuilt==1,"same-address mutable geometry remained stale");
            half(0x8C759644,0);
            require(world.sweep(cpu.memory,{0,0,0},{0,0,40},2).fraction==1,"unregistered platform remained cached");
            put(0x8C19E8B8,1);half(land,0xffff);
            require(!world.sweep(cpu.memory,{0,0,0},{0,0,40},2).valid,"invalid table must fall back to Original");
        }
        std::cout<<"SONIC_CAMERA_TEST_OK orbit_360=1 vertical=1 follow=1 raw_stick=1 guard_cases="<<guard_cases<<" restore=1 abi=1 no_height_drift=1 swept_collision=1 world_registry=1 original_shadow=1 idle_walk_return=1\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"SONIC_CAMERA_TEST_FAILED "<<error.what()<<'\n';return 1;}
}
