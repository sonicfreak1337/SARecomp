#define NOMINMAX
#include <windows.h>
#include "sonic_presentation.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace katana::runtime;
int main(int argc,char** argv){
    try{
        if(argc<3||argc>5)throw std::runtime_error("renderer, cache directory, optional vsync and game timing required");
        _wputenv_s(L"KATANA_PORT_BACKGROUND_TEST",L"1");
        const auto cache=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(cache);
        _wputenv_s(L"SARECOMP_CACHE_ROOT",cache.c_str());
        sonic::rendering::selected_renderer=std::string_view(argv[1])=="vulkan"?sonic::rendering::Renderer::Vulkan:sonic::rendering::Renderer::D3D11;
        sonic::presentation::Settings settings;settings.renderer=sonic::rendering::selected_renderer;
        if(argc>=4)settings.vsync=std::stoul(argv[3]);
        if(argc==5)settings.gameplay_timing=std::stoul(argv[4]);
        const auto ini=cache/L"sonic-display.ini";sonic::presentation::save_settings(ini,settings);
        _wputenv_s(L"SARECOMP_DISPLAY_CONFIG",ini.c_str());sonic::presentation::initialize(cache/L"game.exe");
        NativePortGraphicsConfig graphics;graphics.output_extent={640,480};graphics.render_extent={320,240};graphics.initially_visible=false;graphics.synchronize_present=settings.vsync==1;
        NativePortFramePacingConfig pacing;pacing.simulation_rate_hz=30;pacing.presentation_rate_hz=settings.presentation_fps;pacing.maximum_presentation_rate_hz=144;
        NativePortDesktopHost host(graphics,pacing);
        if(!host.title_cadence_available())throw std::runtime_error("independent presentation disabled");
        for(unsigned rate: settings.gameplay_timing==0 ? std::initializer_list<unsigned>{25u,60u,30u,50u} : std::initializer_list<unsigned>{30u}){
            // Original output must follow changing scene cadence with the same
            // live render owner, while Recompiled repeats this 30-Hz fixture.
            std::uint64_t start=0,presented=0,repeated=0;
            const auto epoch=host.monotonic_time_nanoseconds();
            for(unsigned frame=0;frame<=78;++frame){
                host.wait_until_title_deadline(epoch+std::uint64_t(frame)*1'000'000'000/rate);
                NativePortFrameConfig config;config.clear_color={float(frame%2),0,0,1};
                // Match the game: publish to the independent render owner.
                // A finish/GPU fence on every frame would serialize both clocks.
                host.graphics().begin_frame(config);host.present_frame_after_title_cadence(frame);
                if(frame==6){const auto s=host.frame_pacing_snapshot();start=host.monotonic_time_nanoseconds();presented=s.presentation_frames;repeated=s.repeated_presentations;}
            }
            host.graphics().finish();
            const double elapsed=(host.monotonic_time_nanoseconds()-start)*1e-9;
            const auto s=host.frame_pacing_snapshot();const auto output=s.presentation_frames-presented;
            const double output_fps=output/elapsed,simulation_fps=72/elapsed;
            const auto repeat_count=s.repeated_presentations-repeated;
            std::cout<<"SONIC_PACING_SAMPLE renderer="<<argv[1]<<" source_hz="<<rate<<" output_fps="<<output_fps
                <<" synthetic_sim_fps="<<simulation_fps<<" repeats="<<s.repeated_presentations-repeated<<" monitor_hz=unchanged\n";
            // Hidden FIFO/DWM throughput is not physical scanout and may be
            // throttled. On-mode clock selection is reported by the render
            // owner (SONIC_PRESENT_CLOCK); do not assert monitor throughput.
            const unsigned target=settings.gameplay_timing==0?rate:60u;
            if(simulation_fps<rate*.85||simulation_fps>rate*1.15||output_fps<20||
                ((settings.vsync!=1||settings.gameplay_timing==0)&&(output_fps<target*.85||output_fps>target*1.15))||
                (settings.gameplay_timing==0 ? repeat_count!=0 : (output_fps>rate*1.15&&repeat_count==0)))
                throw std::runtime_error("presentation rate changed title cadence or failed to repeat completed frames");
        }
        std::cout<<"SONIC_PACING_OK renderer="<<argv[1]<<" vsync="<<settings.vsync<<" timing="<<settings.gameplay_timing<<" simulation=unchanged\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"SONIC_PACING_FAIL "<<e.what()<<'\n';return 1;}
}
