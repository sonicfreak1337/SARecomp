#define NOMINMAX
#include <windows.h>
#include "renderer/sonic_motion.hpp"
#include "sonic_presentation.hpp"
#include "sonic_motion_owner.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>
using namespace katana::runtime;
namespace m=sonic::motion;
namespace fs=std::filesystem;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void ownership(){
    CpuState cpu{.memory=Memory{0u}};
    auto ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    cpu.memory.map_region("task-owner-test",0x0c000000u,ram);
    constexpr std::array<std::uint16_t,8> update{0xeb00,0x5e44,0x6c42,0x2ee8,0x8d02,0x2de2,0x4e0b,0x0009};
    constexpr std::array<std::uint16_t,8> display{0x2ee8,0x890d,0x5de5,0x2dd8,0x8d02,0x6ce2,0x4d0b,0x64e3};
    constexpr std::uint32_t task=0x0c300000,child=0x0c300100,callback=0x8c100000;
    for(auto pc:{0x0c0986fau,0x0c098768u,0x0c09879cu}){
        const auto& bytes=pc==0x0c0986fa?update:display;
        for(unsigned i=0;i<bytes.size();++i)cpu.memory.write_u16(pc-16+2*i,bytes[i]);
    }
    for(auto address:{task,child}){
        cpu.memory.write_u32(address+16,callback);
        cpu.memory.write_u32(address+20,callback);
        cpu.memory.write_u32(address+44,address+0x1000);
    }
    for(auto pc:{0x8c0986fau,0x8c098768u,0x8c09879cu}){
        cpu.pr=pc;cpu.r[4]=task|0x80000000;const auto regs=cpu.r;
        {
            m::CallScope parent(cpu,callback,true);
            check(m::current_owner==m::Owner{task,task+0x1000,callback&0x1fffffff},"true task argument, not shared callback");
            cpu.r[4]=child;
            try{
                m::CallScope nested(cpu,callback,true);
                check(m::current_owner.task==child,"identical callbacks retain distinct object instances");
                throw 1;
            }catch(int){}
            check(m::current_owner.task==task,"nested/exception task ownership restored");
            {
                m::CallScope wrong_callback(cpu,callback+2,true);
                check(m::current_owner==m::Owner{},"unproven task boundary must not borrow parent identity");
            }
            cpu.pr=0x8c100004;
            {m::CallScope ordinary(cpu,callback,true);check(m::current_owner.task==task,"ordinary helper inherits caller");}
            cpu.pr=pc;cpu.r[4]=regs[4];
            {m::CallScope disabled(cpu,callback,false);check(m::current_owner.task==task,"disabled annotation has no effect");}
            check(cpu.r==regs&&cpu.pr==pc,"annotation must not change guest registers");
        }
        check(m::current_owner==m::Owner{},"task annotation ends with original callback");
    }
    cpu.memory.write_u16(0x0c09878c,0x0009);
    cpu.pr=0x8c09879c;
    {m::CallScope mismatch(cpu,callback,true);check(!m::current_owner.task,"modified scheduler fails closed");}
    cpu.pr=0x8c098768;cpu.r[4]=0x0cfffff0;
    {m::CallScope bounds(cpu,callback,true);check(!m::current_owner.task,"task extent checked");}
    check(cpu.memory.read_u32(task+20)==callback&&cpu.memory.read_u32(child+20)==callback,"annotation does not mutate task callbacks");
    std::cout<<"SONIC_MOTION_OWNER_OK instances=distinct nesting=restored guest_state=unchanged scheduler=verified\n";
}
m::FrameTag tag(std::uint64_t sequence){
    m::FrameTag result;result.enabled=true;result.identity={1,2,3,4,5,6,7,8};
    result.sequence=sequence;result.time_ns=sequence*33'333'333;return result;
}
m::DrawTag key(std::uint64_t object){return {{object,2,3},true};}
std::array<NativePortVertex,3> triangle(){
    std::array<NativePortVertex,3> v{};
    v[0].position={-.05f,-.05f,.5f};v[1].position={0,.05f,.5f};v[2].position={.05f,-.05f,.5f};
    for(auto& p:v)p.color={1,0,0,1};return v;
}
NativePortDrawPacket packet(std::span<const NativePortVertex> v,float x){
    NativePortDrawPacket p;p.vertices=v;p.batch.identity=1;p.rasterizer.cull=NativePortCullMode::None;
    p.transform.values[12]=x;p.normal_transform.values[12]=x;return p;
}
void unit(){
    auto vertices=triangle();auto a=packet(vertices,-.5f),b=packet(vertices,.5f);
    m::History h;NativePortFrameConfig config;
    auto first=tag(1),second=tag(2);
    h.begin(config,first);check(h.add(a,key(1)),"record first");h.commit(100'000'000);
    check(h.matched()==0&&h.alpha(100'000'000)==1,"first image is original");
    h.begin(config,second);check(h.add(b,key(1)),"record second");check(h.add(a,{}),"record UI");
    check(h.flush(),"record type2 boundary");h.commit(133'333'333);
    check(h.matched()==1,"stable model match");
    std::size_t draws=0,flushes=0;
    m::History::render(h.current(),.25f,[&](const auto& p){
        check(p.transform.values[12]==(draws?-.5f:-.25f),"analytic model/camera interpolation and unmodified UI");
        check(p.vertices[0].position==vertices[0].position,"geometry lifetime");++draws;
    },[&]{++flushes;});
    check(draws==2&&flushes==1,"draw/flush order retained");
    check(h.alpha(150'000'000)>.49f&&h.alpha(150'000'000)<.51f,"clock uses source interval");
    check(h.alpha(1'000'000'000)==1&&h.alpha(1)==1,"no extrapolation or reversed clock");
    h.begin(config,tag(3));h.add(a,key(1));h.add(b,key(1));h.commit(166'666'666);
    check(h.matched()==0,"duplicate owner in new frame");
    h.begin(config,tag(4));h.add(a,key(1));h.commit(199'999'999);
    check(h.matched()==0,"duplicate owner in previous frame");
    h.begin(config,tag(5));h.add(a,key(2));h.add(b,key(1));h.commit(233'333'332);
    check(h.matched()==0,"a partially matched scene is never interpolated");
    check(!h.accepts(tag(6)),"incomplete epoch stays on original pose");
    auto changed_scene=tag(6);++changed_scene.identity[1];check(h.accepts(changed_scene),"new scene can establish complete history");
    h.invalidate();check(!h.can_render()&&h.matched()==0,"resource retirement invalidates cached draw handles");
    h.begin(config,tag(6));h.add(a,key(1));h.commit(266'666'665);
    vertices[0].position[0]=-.06f;b.vertices=vertices;
    h.begin(config,tag(7));h.add(b,key(1));h.commit(299'999'998);
    check(h.matched()==0,"deformed geometry is not blindly index-matched");
    auto reset=second;reset.enabled=false;check(!m::continuous(first,reset),"cutscene/menu/pause gate");
    reset=second;reset.identity[0]++;check(!m::continuous(first,reset),"quicksave epoch");
    reset=second;reset.identity[1]++;check(!m::continuous(first,reset),"scene switch");
    reset=second;reset.player[0]=151;check(!m::continuous(first,reset),"player teleport");
    reset=second;reset.eye[2]=151;check(!m::continuous(first,reset),"camera cut translation");
    reset=second;reset.angles[1]=8193;check(!m::continuous(first,reset),"camera cut rotation");
    reset=second;reset.time_ns=first.time_ns+100'000'001;check(!m::continuous(first,reset),"suspend gap");
    reset=second;reset.sequence=first.sequence;check(!m::continuous(first,reset),"repeated source frame");
    first.angles[1]=65530;second.angles[1]=6;check(m::continuous(first,second),"angle wrap is continuous");
    m::History budget;budget.begin(config,tag(1));
    auto big=triangle();auto p=packet(big,0);std::size_t accepted=0;
    while(budget.add(p,key(++accepted))){}
    check(budget.building().bytes<=m::History::maximum_frame_bytes&&accepted<=m::History::maximum_draws+1,"bounded frame storage");
    check(!m::submitted_draw.enabled,"default submission bypass");
    try{m::Submission scope(m::submitted_draw,key(4));throw 1;}catch(int){}
    check(!m::submitted_draw.enabled,"submission restores after exception");
    m::History coherent;auto raw=key(99);raw.enabled=false;raw.world=true;
    coherent.begin(config,tag(1));coherent.add(a,key(1));coherent.add(a,raw);coherent.commit(100'000'000);
    coherent.begin(config,tag(2));coherent.add(b,key(1));coherent.add(a,raw);coherent.commit(133'333'333);
    check(coherent.matched()==0&&coherent.rejected_world_draws()==1,"pretransformed/unproven world forces complete-scene fallback");
    unsigned world=0;m::History::render(coherent.current(),0,[&](const auto& p){
        check(p.transform.values[12]==(world++?-.5f:.5f),"fallback must render every world draw at current pose");},[]{});
    std::cout<<"SONIC_MOTION_UNIT_OK transforms=analytic ownership=stable duplicates=reject topology=guarded cuts=guarded resource_lifetime=owned bounds=checked\n";
}
void shared_camera(){
    const std::array<float,5> lens{320,240,320,-320,1};
    auto previous=tag(1),current=tag(2);
    m::Matrix view;previous.camera=m::camera_frame(view,lens,1,{0,0,0});
    view.values[12]=-4;current.eye={4,0,0};current.camera=m::camera_frame(view,lens,1,current.eye);
    check(previous.camera.valid&&current.camera.valid,"original camera matrix and raster binding");
    auto make=[&](const m::FrameTag& frame,float world_x,float z){
        NativePortDrawPacket p;
        m::Matrix model;model.values[12]=world_x;model.values[14]=z;
        p.normal_transform=m::product(model,frame.camera.view);
        p.transform=m::product(p.normal_transform,frame.camera.projection);
        p.depth_mapping.mode=NativePortDepthCoordinateMode::ReciprocalPositiveHomogeneousClip;
        return p;
    };
    auto shadow=[&](const m::FrameTag& frame){
        NativePortDrawPacket p;p.vertex_space=NativePortVertexSpace::PvrScreenReciprocal;
        p.depth_mapping.mode=NativePortDepthCoordinateMode::ReciprocalPositive;
        p.transform=frame.camera.screen;p.interpolation=NativePortInterpolationMode::PvrScreenGouraud;
        return p;
    };
    auto vertices=triangle();
    for(auto& v:vertices){v.position[2]=0;v.color={1,0,0,1};}
    auto raw_vertices=triangle();
    for(auto& v:raw_vertices){v.position={320,240,.05f};v.depth_coordinate=v.fog_coordinate=.05f;v.texture_coordinate={.25f,.75f};}
    m::DrawTag world;world.world=true;
    m::History h;NativePortFrameConfig config;
    auto a=make(previous,0,10);a.vertices=vertices;
    auto b=shadow(previous);b.vertices=raw_vertices;
    auto moving=make(previous,2,10);moving.vertices=vertices;
    auto hud=packet(vertices,.6f);
    h.begin(config,previous);h.add(a,world);h.add(b,world);h.add(moving,key(77));h.add(hud,{});h.commit(100'000'000);
    a=make(current,0,10);a.vertices=vertices;
    for(auto& v:raw_vertices)v.position[0]=256;
    b=shadow(current);b.vertices=raw_vertices;
    moving=make(current,6,10);moving.vertices=vertices;
    h.begin(config,current);h.add(a,world);h.add(b,world);h.add(moving,key(77));h.add(hud,{});h.commit(133'333'333);
    check(h.current().camera_motion&&h.animated()&&h.matched()==1,"unknown world geometry shares camera; known movement interpolates independently");
    for(float alpha:{0.0f,.25f,.5f,.75f,1.0f}){
        unsigned index=0;
        m::History::render(h.current(),alpha,[&](const auto& p){
            if(index==0){
                const float screen_x=(p.transform.values[12]/p.transform.values[15]+1)*320;
                check(std::abs(screen_x-(320-128*alpha))<.001f,"static near model shares exact camera time");
            }else if(index==1){
                check(std::abs(p.vertices[0].position[0]-(320-64*alpha))<.001f,"far projected shadow has correct parallax");
                check(p.vertices[0].depth_coordinate==.05f&&p.vertices[0].texture_coordinate==raw_vertices[0].texture_coordinate,"reciprocal depth and UV retained");
            }else if(index==2){
                check(std::abs((p.transform.values[12]/p.transform.values[15]+1)*320-384)<.001f,"object and camera movement do not double count");
            }else check(p.transform.values==hud.transform.values,"screen interface never receives camera reprojection");
            ++index;
        },[]{});
        check(index==4,"all draw families preserved");
    }
    auto invalid=view;invalid.values[0]=2;
    check(!m::camera_frame(invalid,lens,1,current.eye).valid,"model scale cannot masquerade as camera");
    check(!m::camera_frame(view,lens,1,{99,0,0}).valid,"stale camera matrix rejected");
    // Two camera-space representations of the same depth plane stay equal
    // under a rotating, translating camera, not just horizontal translation.
    const float theta=.3490658504f,c=std::cos(theta),s=std::sin(theta);
    m::Matrix rotated;rotated.values[0]=rotated.values[10]=c;rotated.values[2]=-s;rotated.values[8]=s;
    auto rotation=m::camera_frame(rotated,lens,1,{});
    const auto half=m::camera_pose(previous.camera,rotation,.5f);
    check(std::abs(half.values[0]-std::cos(theta*.5f))<.00001f&&std::abs(half.values[2]+std::sin(theta*.5f))<.00001f,"camera uses a rigid half rotation");
    check(std::abs(half.values[0]*half.values[0]+half.values[2]*half.values[2]-1)<.00001f,"camera interpolation does not shrink axes");
    auto reflected=rotated;reflected.values[5]=-1;
    auto reflected_start=m::Matrix{};reflected_start.values[5]=-1;
    check(m::camera_continuous(m::camera_frame(reflected_start,lens,1,{}),m::camera_frame(reflected,lens,1,{})),"fixed source handedness supported");
    for(auto& v:raw_vertices)v.position={256,240,1.0f/3},v.depth_coordinate=1.0f/3;
    b.vertices=raw_vertices;
    check(m::camera_draw_problem(b,previous.camera,current.camera)!=nullptr,"entire camera arc checked against near plane");
    // A multipart actor must not interpolate its unchanged body while a
    // deformed part or its already projected shadow uses the current pose.
    m::History actor;
    auto body_key=key(42),part_key=key(42),shadow_key=key(42);
    part_key.identity[2]=4;shadow_key.enabled=false;shadow_key.world=true;
    auto part_vertices=vertices;
    auto record_actor=[&](const m::FrameTag& frame,float x,bool deformed){
        auto body=make(frame,x,10);body.vertices=vertices;
        auto part=make(frame,x+1,10);
        if(deformed)part_vertices[0].position[1]+=.01f;
        part.vertices=part_vertices;
        for(auto& v:raw_vertices){v.position={320+320*(x-frame.eye[0])/10,240,.1f};v.depth_coordinate=v.fog_coordinate=.1f;}
        auto shade=shadow(frame);shade.vertices=raw_vertices;
        actor.begin(config,frame);actor.add(body,body_key);actor.add(part,part_key);actor.add(shade,shadow_key);
        actor.commit(frame.sequence==1?100'000'000:133'333'333);
    };
    record_actor(previous,2,false);record_actor(current,6,true);
    check(actor.current().camera_motion,"mixed actor can still use a coherent shared camera");
    check(actor.matched()==0,"mixed actor pose must not interpolate isolated body parts");
    unsigned actor_draw=0;
    m::History::render(actor.current(),.5f,[&](const auto& p){
        const float x=p.vertex_space==NativePortVertexSpace::PvrScreenReciprocal?p.vertices[0].position[0]
            :(p.transform.values[12]/p.transform.values[15]+1)*320;
        check(std::abs(x-(actor_draw==1?480.0f:448.0f))<.001f,"body, deformed part and shadow use one object pose");
        ++actor_draw;
    },[]{});
    check(actor_draw==3,"complete actor retained");
    std::cout<<"SONIC_MOTION_CAMERA_OK world=shared depth=parallax raw_vertices=reprojected movement=atomic-pose hud=unchanged rotation=rigid cuts=guarded\n";
}
struct Image {unsigned w=0,h=0,stride=0,bpp=0,offset=0;std::vector<unsigned char> data;};
Image read(const fs::path& path){
    Image i;std::ifstream f(path,std::ios::binary);i.data={std::istreambuf_iterator<char>(f),{}};
    check(i.data.size()>54,"capture missing");
    const auto word=[&](unsigned at){return unsigned(i.data[at])|(unsigned(i.data[at+1])<<8)|(unsigned(i.data[at+2])<<16)|(unsigned(i.data[at+3])<<24);};
    i.w=word(18);i.h=unsigned(std::abs(int(word(22))));i.bpp=unsigned(i.data[28])|(unsigned(i.data[29])<<8);
    check(i.w==640&&i.h==480&&(i.bpp==24||i.bpp==32),"capture layout");
    i.stride=((i.w*i.bpp+31)/32)*4;i.offset=word(10);
    check(i.data.size()>=i.offset+i.stride*i.h,"capture extent");return i;
}
double centroid(const Image& i){double sum=0;unsigned n=0;
    for(unsigned y=0;y<i.h;++y)for(unsigned x=0;x<i.w;++x){const auto at=i.offset+y*i.stride+x*(i.bpp/8);
        if(i.data[at+2]>200&&i.data[at]<10&&i.data[at+1]<10){sum+=x;++n;}}
    check(n>50,"moving model not rendered");return sum/n;
}
void gpu_camera(NativePortGraphicsDevice& device,const fs::path& root,const char* renderer){
    const std::array<float,5> lens{320,240,320,-320,1};
    auto near_vertices=triangle(),far_vertices=triangle(),hud=triangle();
    near_vertices[0].position={-2,-2,10};near_vertices[1].position={0,2,10};near_vertices[2].position={2,-2,10};
    far_vertices[0].position={-6,-5,20};far_vertices[1].position={0,5,20};far_vertices[2].position={6,-5,20};
    for(auto& v:far_vertices)v.color={0,1,0,1};
    for(auto& v:hud)v.color={0,0,1,1};
    NativePortFrameConfig config;config.clear_depth=0;config.depth_buffer=NativePortDepthBufferConvention::ReciprocalPositive;
    auto submit=[&](float eye,std::uint64_t sequence,bool interpolate){
        m::Matrix view;view.values[12]=-eye;
        auto frame=tag(sequence);frame.eye={eye,0,0};frame.camera=m::camera_frame(view,lens,1,frame.eye);
        frame.enabled=interpolate;
        {m::Submission scope(m::submitted_frame,frame);device.begin_frame(config);}
        m::DrawTag world;world.world=true;
        auto p=packet(near_vertices,0);p.normal_transform=view;p.transform=m::product(view,frame.camera.projection);
        p.depth_mapping.mode=NativePortDepthCoordinateMode::ReciprocalPositiveHomogeneousClip;
        p.depth.compare=NativePortCompareOperation::GreaterEqual;
        {m::Submission scope(m::submitted_draw,world);device.draw(p);}
        auto projected=far_vertices;
        for(auto& v:projected){
            const auto position=m::point(v.position,view);const float depth=1/position[2];
            v.position={320+320*position[0]*depth,240-320*position[1]*depth,depth};
            v.depth_coordinate=v.fog_coordinate=depth;
        }
        p.vertices=projected;p.vertex_space=NativePortVertexSpace::PvrScreenReciprocal;p.batch.submission_order=1;
        p.transform=frame.camera.screen;p.normal_transform={};
        p.depth_mapping.mode=NativePortDepthCoordinateMode::ReciprocalPositive;
        p.interpolation=NativePortInterpolationMode::PvrScreenGouraud;p.rasterizer.depth_clip_enabled=false;
        {m::Submission scope(m::submitted_draw,world);device.draw(p);}
        auto ui=packet(hud,.8f);ui.transform.values[13]=-.8f;ui.depth.test_enabled=ui.depth.write_enabled=false;ui.batch.submission_order=2;
        device.draw(ui);device.present();static_cast<void>(device.snapshot());
    };
    m::test_time_ns.store(100'000'000);submit(0,1,true);
    m::test_time_ns.store(133'333'333);submit(4,2,true);
    m::test_time_ns.store(149'999'999);device.repeat_present();static_cast<void>(device.snapshot());
    submit(2,3,false);device.finish();m::test_time_ns.store(0);
    const auto intermediate=read(root/"frame-3.bmp"),reference=read(root/"frame-4.bmp");
    unsigned changed=0,red=0,green=0,blue=0;
    for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x){
        const auto a=intermediate.offset+y*intermediate.stride+x*(intermediate.bpp/8);
        const auto b=reference.offset+y*reference.stride+x*(reference.bpp/8);
        bool different=false;for(unsigned c=0;c<3;++c)different|=std::abs(int(intermediate.data[a+c])-int(reference.data[b+c]))>2;
        changed+=different;
        red+=reference.data[b+2]>200&&reference.data[b+1]<10;
        green+=reference.data[b+1]>200&&reference.data[b+2]<10;
        blue+=reference.data[b]>200&&reference.data[b+2]<10;
    }
    check(red>500&&green>500&&blue>50,"mixed world/depth/HUD reference rendered");
    check(changed<160,"shared camera intermediate differs from independent current-pose reference");
    check(device.completed_drawn_frames_nonblocking()==3,"camera smoothing must not tick simulation");
    std::cout<<"SONIC_MOTION_CAMERA_GPU_OK backend="<<renderer<<" intermediate_vs_reference_changed_pixels="<<changed
             <<" object=homogeneous shadow=screen-reciprocal depth=occluded hud=unchanged native_frames=3\n";
}
void gpu(const char* renderer,const fs::path& root,bool camera_only=false){
    check(!fs::exists(root),"test directory must be new");fs::create_directories(root);
    SetEnvironmentVariableW(L"KATANA_PORT_BACKGROUND_TEST",L"1");
    _putenv_s("KATANA_PORT_BACKGROUND_TEST","1");
    SetEnvironmentVariableW(L"SARECOMP_DISPLAY_CONFIG",(root/L"sonic-display.ini").c_str());
    SetEnvironmentVariableW(L"SARECOMP_CACHE_ROOT",(root/L"cache").c_str());
    SetEnvironmentVariableW(L"KATANA_NATIVE_GRAPHICS_CAPTURE_DIRECTORY",root.c_str());
    SetEnvironmentVariableW(L"KATANA_NATIVE_GRAPHICS_CAPTURE_START_FRAME",L"1");
    SetEnvironmentVariableW(L"KATANA_NATIVE_GRAPHICS_CAPTURE_END_FRAME",L"100");
    SetEnvironmentVariableW(L"KATANA_NATIVE_GRAPHICS_CAPTURE_INTERVAL",L"1");
    SetEnvironmentVariableW(L"SARECOMP_MOTION_TRACE",L"1");
    _putenv_s("SARECOMP_MOTION_TRACE","1");
    sonic::presentation::Settings settings;settings.width=640;settings.height=480;settings.setup_complete=true;settings.interpolation=1;
    settings.renderer=std::string_view(renderer)=="vulkan"?sonic::rendering::Renderer::Vulkan:sonic::rendering::Renderer::D3D11;
    sonic::presentation::save_settings(root/L"sonic-display.ini",settings);sonic::presentation::initialize(root/L"game.exe");
    NativePortGraphicsConfig cfg;sonic::presentation::configure(cfg);cfg.initially_visible=false;cfg.synchronize_present=false;
    cfg.maximum_type2_fragment_nodes=2'000'000;NativePortGraphicsDevice device(cfg);
    if(camera_only){gpu_camera(device,root,renderer);return;}
    std::array<unsigned,6> begins{};unsigned output=0;
    auto vertices=triangle(),hud=triangle();for(auto& v:hud)v.color={0,0,1,1};
    for(unsigned f=0;f<6;++f){
        const auto source_time=100'000'000ull+33'333'333ull*f;
        m::test_time_ns.store(source_time);
        const auto stamp=tag(f+1);m::Submission frame_scope(m::submitted_frame,stamp);
        device.begin_frame();
        {m::Submission draw_scope(m::submitted_draw,key(1));device.draw(packet(vertices,-.6f+.2f*f));}
        auto ui=packet(hud,.8f);ui.transform.values[13]=.8f;ui.batch.submission_order=1;device.draw(ui);
        device.present();static_cast<void>(device.snapshot());begins[f]=++output;
        for(unsigned extra=0;extra<3;++extra){m::test_time_ns.store(source_time+8'333'333ull*(extra+1));device.repeat_present();++output;}
    }
    // Explicitly disabling source metadata retains current-pose rendering.
    device.begin_frame();device.draw(packet(vertices,.6f));device.present();static_cast<void>(device.snapshot());const auto bypass=++output;
    // An update inside a deferred frame must first render old texture users.
    // Destruction after Present must prevent any subsequent replay of handles.
    std::array<std::byte,4> pixels{std::byte{255},std::byte{0},std::byte{0},std::byte{255}};
    NativePortTextureConfig texture;texture.extent={1,1};texture.dynamic=true;
    NativePortImageView image;image.extent={1,1};image.stride_bytes=4;image.pixels=pixels;image.format=NativePortTextureFormat::Rgba8Unorm;
    const auto handle=device.create_texture(texture,&image);
    m::test_time_ns.store(400'000'000);
    {m::Submission frame_scope(m::submitted_frame,tag(8));device.begin_frame();}
    auto white=triangle();for(auto& v:white)v.color={1,1,1,1};
    auto textured=packet(white,-.4f);textured.texture=handle; texture.dynamic=true;
    textured.texture_stage=NativePortTextureStage::RequiredResolved;
    {m::Submission draw_scope(m::submitted_draw,key(9));device.draw(textured);}
    pixels={std::byte{0},std::byte{255},std::byte{0},std::byte{255}};device.update_texture(handle,image);
    textured.transform.values[12]=.4f;textured.batch.submission_order=1;device.draw(textured);
    device.present();static_cast<void>(device.snapshot());const auto mutation=++output;
    device.destroy_texture(handle);device.repeat_present();++output;
    device.finish();m::test_time_ns.store(0);check(device.completed_drawn_frames_nonblocking()==8,"output interpolation advanced native simulation frame count");
    unsigned intermediate=0;std::set<int> positions;
    for(unsigned f=1;f<6;++f)for(unsigned frame=begins[f];frame<begins[f]+4;++frame){
        const auto image=read(root/("frame-"+std::to_string(frame)+".bmp"));const auto x=centroid(image);
        const double left=(1+(-.6+.2*(f-1)))*320-.5,right=left+64;
        check(x>=left-2&&x<=right+2,"interpolation overshot source poses");
        if(x>left+2&&x<right-2)++intermediate;positions.insert(int(std::lround(x)));
        const auto at=image.offset+unsigned(432)*image.stride+unsigned(576)*(image.bpp/8);
        check(image.data[at]>200&&image.data[at+2]<10,"HUD changed during motion interpolation");
    }
    check(intermediate>=3&&positions.size()>5,"output still repeats simulation images");
    check(std::abs(centroid(read(root/("frame-"+std::to_string(bypass)+".bmp")))-511.5)<2,"disabled path retained an old pose");
    const auto changed=read(root/("frame-"+std::to_string(mutation)+".bmp"));
    check(std::abs(centroid(changed)-191.5)<2,"texture update overtook an earlier deferred draw");
    const auto green=changed.offset+240u*changed.stride+448u*(changed.bpp/8);
    check(changed.data[green+1]>200&&changed.data[green+2]<10,"new texture contents were not rendered");
    std::cout<<"SONIC_MOTION_GPU_OK backend="<<renderer<<" native_frames=8 output_frames="<<output
             <<" intermediate_images="<<intermediate<<" distinct_positions="<<positions.size()<<" hud=unchanged resource_update=ordered destroy=no_stale_replay\n";
}
int main(int argc,char** argv){try{ownership();unit();shared_camera();if(argc>=3)gpu(argv[1],fs::absolute(argv[2]),argc==4);return 0;}
    catch(const std::exception& e){std::cerr<<"SONIC_MOTION_TEST_FAIL "<<e.what()<<'\n';return 1;}}
