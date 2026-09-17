#include "sonic_model_packet.hpp"
#include "renderer/renderer_selection.hpp"
#include "sonic_internal_diagnostics.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
static void _putenv_s(const char* k,const char* v){setenv(k,v,1);}
#endif
using namespace katana::runtime;
namespace mp=sonic::model_packet;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
// Retained title argb_color expression, including its constexpr scale and
// each platform compiler's treatment of the known opaque primary alpha.
constexpr std::array<float,4> reference_color(std::uint32_t color) noexcept {
    constexpr float scale=1.0f/255.0f;
    return {static_cast<float>((color>>16u)&0xFFu)*scale,
        static_cast<float>((color>>8u)&0xFFu)*scale,
        static_cast<float>(color&0xFFu)*scale,
        static_cast<float>((color>>24u)&0xFFu)*scale};
}
std::vector<char> read(const std::filesystem::path& p){
    std::ifstream f(p,std::ios::binary);require(bool(f),"capture missing");
    return {std::istreambuf_iterator<char>(f),{}};
}
int main(int argc,char** argv){try{
    require(argc==4,"backend, output directory and serial/parallel required");
    _putenv_s("KATANA_PORT_BACKGROUND_TEST","1");
    _putenv_s("SARECOMP_VULKAN_OFFSCREEN_TEST","1");
    _putenv_s("KATANA_PORT_DISABLE_RENDER_THREAD",std::string_view(argv[3])=="serial"?"1":"0");
    _putenv_s("KATANA_NATIVE_GRAPHICS_CAPTURE_DIRECTORY",argv[2]);
    _putenv_s("KATANA_NATIVE_GRAPHICS_CAPTURE_START_FRAME","1");
    _putenv_s("KATANA_NATIVE_GRAPHICS_CAPTURE_END_FRAME","4");
    _putenv_s("KATANA_NATIVE_GRAPHICS_CAPTURE_INTERVAL","1");
    sonic::diagnostics::internal_runtime_enabled=true;
    std::filesystem::create_directories(argv[2]);
    sonic::rendering::selected_renderer=std::string_view(argv[1])=="vulkan"?
        sonic::rendering::Renderer::Vulkan:sonic::rendering::Renderer::D3D11;
    std::vector<mp::Vec3> points{{-.8f,.8f,.5f},{.8f,.8f,.5f},{-.8f,-.8f,.5f},{.8f,-.8f,.5f}};
    std::vector<mp::Vec3> normals{{.2f,.4f,1},{-.2f,.3f,1},{.4f,-.2f,1},{.1f,.2f,1}};
    std::vector<unsigned> primary{0xFFA57423u,0xFFFF008Bu,0xFF618421u,0xFF345678u};
    std::vector<unsigned> secondary{0x27161821u,0x46100123u,0x922A2123u,0xAF345612u};
    std::vector<mp::Corner> corners{{0,{0,0}},{1,{1,0}},{2,{0,1}},{3,{1,1}},{0,{.5f,0}}};
    std::vector<unsigned> indices{0,1,2,2,1,3,4,1,2};
    mp::Draw draw{mp::Geometry::capture(4,corners,indices),mp::Attributes::capture(points,normals,primary,secondary)};
    const auto original_csr=_mm_getcsr();unsigned cases=0;
    for(unsigned rounding=0;rounding<4;++rounding)for(unsigned flags=0;flags<8;++flags){
        draw.ignore_light=flags&1;draw.use_secondary=flags&2;draw.vertex_fog=flags&4;
        draw.host_mode=rounding<<13u;
        std::vector<NativePortVertex> expanded;mp::expand(draw,expanded);
        require(_mm_getcsr()==original_csr,"consumer FPU mode leaked");
        _mm_setcsr(0x1F80u|draw.host_mode);
        for(std::size_t i=0;i<corners.size();++i){
            const auto p=corners[i].point;NativePortVertex v;
            v.position=points[p];v.texture_coordinate=corners[i].uv;
            const auto packed_rgb=draw.ignore_light?0x00FFFFFFu:primary[p]&0x00FFFFFFu;
            v.color=reference_color(packed_rgb|0xFF000000u);
            if(!draw.ignore_light)v.normal=normals[p];
            if(draw.use_secondary){
                v.secondary_color=draw.ignore_light?reference_color(0u):reference_color(secondary[p]);
                if(draw.vertex_fog)v.fog_coordinate=v.secondary_color[3];
            }
            if(std::memcmp(&v,&expanded[i],sizeof(v))){
                const auto* expected=reinterpret_cast<const unsigned char*>(&v);
                const auto* actual=reinterpret_cast<const unsigned char*>(&expanded[i]);
                for(std::size_t b=0;b<sizeof(v);++b)if(expected[b]!=actual[b])
                    std::cerr<<"round="<<rounding<<" flags="<<flags<<" corner="<<i<<" byte="<<b<<" expected="<<unsigned(expected[b])<<" actual="<<unsigned(actual[b])<<'\n';
                throw std::runtime_error("vertex bytes differ");
            }
        }
        _mm_setcsr(original_csr);++cases;
    }
    draw.ignore_light=false;draw.use_secondary=true;draw.vertex_fog=false;draw.host_mode=0;
    std::vector<NativePortVertex> vertices;mp::expand(draw,vertices);
    NativePortGraphicsConfig config;config.output_extent=config.render_extent={128,128};
    config.initially_visible=false;config.synchronize_present=false;config.maximum_type2_fragment_nodes=131072;
    NativePortGraphicsDevice device(config);
    NativePortDrawPacket packet;packet.vertices=vertices;packet.indices=indices;
    packet.vertex_space=NativePortVertexSpace::ObjectHomogeneous;
    packet.rasterizer.cull=NativePortCullMode::None;packet.batch.identity=1;
    packet.material.use_secondary_color=true;
    device.begin_frame();device.draw(packet);device.present();device.finish();
    packet.vertices={};packet.indices={};
    device.begin_frame();
    {mp::Submission scope(&draw);device.draw(packet);}
    // Mutation and owner release before publication must not alter the draw.
    points.assign(4,mp::Vec3{100,100,100});primary.assign(4,0);corners.clear();indices.clear();
    std::weak_ptr<const mp::Attributes> lifetime=draw.attributes;draw={};
    require(!lifetime.expired(),"queue did not own snapshot");
    // This resource transaction publishes and consumes a draw prefix.
    NativePortTextureConfig texture;texture.extent={2,2};
    auto handle=device.create_texture(texture);
    device.present();device.destroy_texture(handle);device.finish();
    require(lifetime.expired(),"completed snapshot retained");
    require(read(std::filesystem::path(argv[2])/"frame-1.bmp")==read(std::filesystem::path(argv[2])/"frame-2.bmp"),"compact image differs");
    // Two distinct snapshots and states, including a synchronous transaction
    // between them: the second draw has an ordinal in a different lease.
    const std::array<mp::Corner,3> triangle{{{0,{0,0}},{1,{1,0}},{2,{0,1}}}};
    const std::array<unsigned,3> triangle_indices{0,1,2};
    const std::array<mp::Vec3,3> triangle_points{{{-.6f,.6f,.5f},{.6f,.6f,.5f},{0,-.6f,.5f}}};
    const std::array<unsigned,3> red{0xFFAA1144u,0xFFAA1144u,0xFFAA1144u},blue{0xFF2288CCu,0xFF2288CCu,0xFF2288CCu},zero{};
    const auto geometry=mp::Geometry::capture(3,triangle,triangle_indices);
    mp::Draw a{geometry,mp::Attributes::capture(triangle_points,{},red,zero)};
    mp::Draw b{geometry,mp::Attributes::capture(triangle_points,{},blue,zero)};
    std::vector<NativePortVertex> av,bv;mp::expand(a,av);mp::expand(b,bv);
    packet.material.use_secondary_color=false;packet.depth.test_enabled=packet.depth.write_enabled=false;
    for(unsigned frame=3;frame<=4;++frame){
        device.begin_frame();packet.batch.identity=frame;
        for(unsigned which=0;which<2;++which){
            packet.batch.submission_order=which;packet.transform.values[12]=which?.2f:-.2f;
            packet.vertices=frame==3?std::span<const NativePortVertex>(which?bv:av):std::span<const NativePortVertex>{};
            packet.indices=frame==3?std::span<const unsigned>(triangle_indices):std::span<const unsigned>{};
            {mp::Submission scope(frame==4?(which?&b:&a):nullptr);device.draw(packet);}
            if(frame==4&&!which)handle=device.create_texture(texture);
        }
        device.present();device.finish();
        if(frame==4)device.destroy_texture(handle);
    }
    require(read(std::filesystem::path(argv[2])/"frame-3.bmp")==read(std::filesystem::path(argv[2])/"frame-4.bmp"),"split-lease state or order differs");
    // Empty/invalid shape is rejected before either thread consumes it.
    bool rejected=false;try{mp::Geometry::capture(4,{},{});}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"invalid geometry accepted");
    mp::Draw bad;
    device.begin_frame();rejected=false;
    {mp::Submission scope(&bad);try{device.draw(packet);}catch(const NativePortGraphicsError&){rejected=true;}}
    require(rejected,"invalid queued model accepted");
    // Failed producer encoding leaves no sidecar for the next frame.
    device.begin_frame();packet.vertices=av;packet.indices=triangle_indices;
    packet.batch.identity=5;packet.batch.submission_order=0;
    device.draw(packet);device.present();device.finish();
    require(!mp::submitted,"submission scope leaked");
    std::cout<<"MODEL_PACKETS_OK backend="<<argv[1]<<" queue="<<argv[3]<<" vertex_cases="<<cases
        <<" exact_pixels=4 ownership=ok split_prefix=ok abort_recovery=ok rounding=ok\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"MODEL_PACKETS_FAIL "<<e.what()<<'\n';return 1;}}
