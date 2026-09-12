#define NOMINMAX
#include <windows.h>
#include "sonic_presentation.hpp"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace katana::runtime;
namespace fs=std::filesystem;
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
int main(int argc,char** argv){
    try{
        require(argc==3,"renderer and fresh output directory required");
        const auto root=fs::absolute(argv[2]);require(!fs::exists(root),"output must be fresh");fs::create_directories(root);
        SetEnvironmentVariableW(L"KATANA_PORT_BACKGROUND_TEST",L"1");
        SetEnvironmentVariableW(L"SARECOMP_CACHE_ROOT",(root/L"cache").c_str());
        SetEnvironmentVariableW(L"KATANA_NATIVE_GRAPHICS_CAPTURE_DIRECTORY",root.c_str());
        SetEnvironmentVariableW(L"KATANA_NATIVE_GRAPHICS_CAPTURE_START_FRAME",L"1");
        SetEnvironmentVariableW(L"KATANA_NATIVE_GRAPHICS_CAPTURE_END_FRAME",L"2");
        SetEnvironmentVariableW(L"KATANA_NATIVE_GRAPHICS_CAPTURE_INTERVAL",L"1");
        SetEnvironmentVariableW(L"SARECOMP_DISPLAY_CONFIG",(root/L"sonic-display.ini").c_str());
        sonic::presentation::Settings settings;settings.setup_complete=true;settings.widescreen=true;
        settings.width=1260;settings.height=540;settings.renderer=std::string_view(argv[1])=="vulkan"?
            sonic::rendering::Renderer::Vulkan:sonic::rendering::Renderer::D3D11;
        sonic::presentation::save_settings(root/L"sonic-display.ini",settings);
        sonic::presentation::initialize(root/L"game.exe");
        NativePortGraphicsConfig config;sonic::presentation::configure(config);
        config.initially_visible=false;config.synchronize_present=false;config.maximum_type2_fragment_nodes=2'000'000;
        NativePortGraphicsDevice device(config);
        std::array<NativePortVertex,4> vertices{};
        vertices[0].position={0,0,1};vertices[1].position={0,480,1};
        vertices[2].position={640,0,1};vertices[3].position={640,480,1};
        for(auto& v:vertices){v.color={0,0,0,.5f};v.depth_coordinate=1;}
        NativePortDrawPacket source;source.vertices=vertices;source.topology=NativePortPrimitiveTopology::TriangleStrip;
        source.vertex_space=NativePortVertexSpace::PvrScreenReciprocal;source.interpolation=NativePortInterpolationMode::PvrScreenGouraud;
        source.transform.values={2.0f/640,0,0,0,0,-2.0f/480,0,0,0,0,1,0,-1,1,0,1};
        source.viewport=NativePortViewportTarget::Ui;source.depth.test_enabled=true;source.depth.write_enabled=false;
        source.depth.compare=NativePortCompareOperation::GreaterEqual;
        source.depth_mapping.mode=NativePortDepthCoordinateMode::ReciprocalPositive;
        source.rasterizer.cull=NativePortCullMode::None;
        source.rasterizer.depth_clip_enabled=false;
        source.rasterizer.small_triangle_area_space=NativePortTriangleAreaSpace::Submitted;
        source.draw_class=NativePortDrawClass::Translucent;source.translucency=NativePortTranslucencyPolicy::Type2AutoSorted;
        source.blend.enabled=true;source.blend.source_color=source.blend.source_alpha=NativePortBlendFactor::SourceAlpha;
        source.blend.destination_color=source.blend.destination_alpha=NativePortBlendFactor::InverseSourceAlpha;
        source.batch.identity=1;
        NativePortFrameConfig frame;frame.clear_color={1,1,1,1};
        frame.depth_buffer=NativePortDepthBufferConvention::ReciprocalPositive;frame.clear_depth=0;
        for(unsigned i=0;i<2;++i){
            auto packet=source;sonic::presentation::apply(packet,i?sonic::presentation::Role::World:sonic::presentation::Role::Interface);
            device.begin_frame(frame);device.draw(packet);device.present();
        }
        device.finish();
        for(unsigned frame_id=1;frame_id<=2;++frame_id){
            std::ifstream file(root/("frame-"+std::to_string(frame_id)+".bmp"),std::ios::binary);
            const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file),{}};
            require(bytes.size()>54,"capture missing");
            const auto word=[&](unsigned at){return unsigned(bytes[at])|(unsigned(bytes[at+1])<<8)|(unsigned(bytes[at+2])<<16)|(unsigned(bytes[at+3])<<24);};
            const auto width=word(18),height=unsigned(std::abs(int(word(22)))),bpp=unsigned(bytes[28])|(unsigned(bytes[29])<<8);
            require(width==1260&&height==540&&(bpp==24||bpp==32),"unexpected capture layout");
            const auto stride=((width*bpp+31)/32)*4;
            for(auto x:{1u,width/2,width-2})for(auto y:{1u,unsigned(height/2),unsigned(height-2)}){
                const auto at=word(10)+y*stride+x*(bpp/8);require(at+2<bytes.size(),"capture range");
                const bool dim=frame_id==1||x==width/2;
                for(unsigned rgb=0;rgb<3;++rgb)
                    require(dim?(bytes[at+rgb]>=126&&bytes[at+rgb]<=129):bytes[at+rgb]==255,
                            "fullscreen effect edge or preserved world aspect failed");
            }
        }
        std::cout<<"SONIC_FULLSCREEN_EFFECT_TEST_OK backend="<<argv[1]<<" dimmed_edges=all world_aspect=preserved\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"SONIC_FULLSCREEN_EFFECT_TEST_FAIL "<<e.what()<<'\n';return 1;}
}
