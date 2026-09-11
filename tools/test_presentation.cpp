#include "sonic_presentation.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace katana::runtime;
using namespace sonic::presentation;
void require(bool value, const char* reason) {
    if (!value) throw std::runtime_error(reason);
}
void close(float a,float b,const char* reason) { require(std::abs(a-b)<0.002f,reason); }
int main(int argc,char** argv) {
    try {
        require(argc==2,"test directory argument");
        const std::filesystem::path folder(argv[1]);
        std::filesystem::create_directories(folder);
        for (auto extent : {NativePortExtent{1280,720},NativePortExtent{2560,1080},NativePortExtent{3440,1440}}) {
            { std::ofstream f(folder/"sonic-display.ini");
              f<<"mode=widescreen\nwidth="<<extent.width<<"\nheight="<<extent.height<<"\nrender_percent=50\n"; }
            initialize(folder/"game.exe");
            NativePortGraphicsConfig config;
            configure(config);
            require(std::uint64_t(config.output_extent.width)*config.render_extent.height ==
                    std::uint64_t(config.render_extent.width)*config.output_extent.height,"aspect drift");
            NativePortDrawPacket original;
            original.transform.values = {2.0f/640,0,0,0,0,-2.0f/480,0,0,0,0,1,0,-1,1,0,1};
            const auto project=[&](Role role,float x) {
                auto packet=original; apply(packet,role);
                return (packet.transform.values[0]*x+packet.transform.values[12]+1)*extent.width/2;
            };
            const auto pixels_per_unit=extent.height/480.0f;
            close(project(Role::HudLeft,16),16*pixels_per_unit,"left HUD margin");
            close(extent.width-project(Role::HudRight,624),16*pixels_per_unit,"right HUD margin");
            close(project(Role::Interface,320),extent.width/2.0f,"UI center");
            close(project(Role::World,352)-project(Role::World,320),32*pixels_per_unit,"world square distortion");
            close(project(Role::World,-extra_horizontal_pixels()),0,"left frustum extent");
            close(project(Role::Fullscreen,640),float(extent.width),"incomplete fullscreen fade");
            auto packet=original; apply(packet,Role::World);
            for(unsigned row=0;row<4;++row) for(unsigned col=1;col<4;++col)
                require(packet.transform.values[row*4+col]==original.transform.values[row*4+col],"Y/Z/W changed");
        }
        { std::ofstream f(folder/"sonic-display.ini"); f<<"mode=original\n"; }
        initialize(folder/"game.exe");
        NativePortDrawPacket packet;
        const auto original=packet;
        for(auto role:{Role::World,Role::Interface,Role::Fullscreen,Role::HudLeft,Role::HudRight}) {
            apply(packet,role);
            require(packet.transform.values==original.transform.values && packet.viewport==original.viewport,"original changed");
        }
        std::cout<<"SONIC_PRESENTATION_TESTS_OK 16:9 64:27 43:18 original\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
