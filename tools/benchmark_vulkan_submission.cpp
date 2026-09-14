#include "katana/runtime/native_port_graphics.hpp"
#include "renderer/renderer_selection.hpp"
#include "sonic_execution_clock.hpp"
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#ifndef _WIN32
#include <time.h>
#endif
using namespace katana::runtime;
static double cpu_ms(){
    const auto clock=sonic::performance::execution_clock();
    if(!clock.thread_valid||!clock.process_valid||!clock.thread_id)throw std::runtime_error("CPU clock unavailable");
    return double(clock.thread_cpu_100ns)/10000;
}
int main(int argc,char** argv){
    try{
        if(argc!=2&&argc!=4)return 2;
#ifdef _WIN32
        _putenv_s("KATANA_PORT_BACKGROUND_TEST","1");_putenv_s("KATANA_PORT_DISABLE_RENDER_THREAD","1");
        _putenv_s("SARECOMP_VULKAN_OFFSCREEN_TEST","1");
#else
        setenv("KATANA_PORT_BACKGROUND_TEST","1",1);setenv("KATANA_PORT_DISABLE_RENDER_THREAD","1",1);
#endif
        sonic::rendering::selected_renderer=sonic::rendering::Renderer::Vulkan;
        NativePortGraphicsConfig config;config.output_extent=config.render_extent={128,128};
        config.initially_visible=false;config.synchronize_present=false;
        config.maximum_type2_fragment_nodes=131072;
        NativePortGraphicsDevice device(config);
        if(device.snapshot().active_execution_mode!=NativePortGraphicsExecutionMode::SerialReference)throw std::runtime_error("benchmark needs the calling render owner");
        std::array<NativePortVertex,3> vertices{};
        vertices[0].position={-.01f,.01f,.5f};vertices[1].position={.01f,.01f,.5f};vertices[2].position={0,-.01f,.5f};
        for(auto& v:vertices){v.color={1,1,1,1};v.depth_coordinate=1;}
        NativePortDrawPacket packet;packet.vertices=vertices;packet.rasterizer.cull=NativePortCullMode::None;
        packet.depth.test_enabled=packet.depth.write_enabled=false;
        constexpr unsigned warmup=12;
        const unsigned frames=argc==4?unsigned(std::stoul(argv[2])):80;
        const unsigned draws=argc==4?unsigned(std::stoul(argv[3])):2048;
        if(frames<2||frames>200||!draws||draws>16000)throw std::runtime_error("invalid benchmark workload");
        double start_cpu=0;std::chrono::steady_clock::time_point start;
        for(unsigned frame=0;frame<warmup+frames;++frame){
            if(frame==warmup){device.finish();start_cpu=cpu_ms();start=std::chrono::steady_clock::now();}
            device.begin_frame();
            for(unsigned draw=0;draw<draws;++draw){
                packet.batch.identity=1;packet.batch.submission_order=draw;
                packet.material.diffuse={float(draw%5)/4,1,.5f,1};
                packet.rasterizer.front_counter_clockwise=(draw&1)!=0;
                device.draw(packet);
            }
            device.present();
        }
        device.finish();const double cpu=cpu_ms()-start_cpu;
        const auto wall=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        const auto snapshot=device.snapshot();
        if(snapshot.contract_failures||snapshot.draw_calls!=(warmup+frames)*draws)throw std::runtime_error("draw workload changed");
        std::cout<<"SONIC_VULKAN_SUBMISSION_BENCH label="<<argv[1]<<" frames="<<frames<<" draws_per_frame="<<draws
            <<" monitor_presentation="
#ifdef _WIN32
            <<"offscreen-test"
#else
            <<"xvfb"
#endif
            <<" cpu_ms_per_frame="<<cpu/frames<<" wall_ms_per_frame="<<wall/frames<<" all_draws="<<snapshot.draw_calls<<'\n';
        return 0;
    }catch(const std::exception& e){std::cerr<<"SONIC_VULKAN_SUBMISSION_FAIL "<<e.what()<<'\n';return 1;}
}
