#include "sonic_presentation.hpp"
#include "sonic_big_hud.hpp"
#include "sonic_user_paths.hpp"
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
    const auto big=[](std::uint32_t pr,std::uint32_t carrier,std::uint32_t texlist,
                      std::uint32_t frames,std::optional<std::uint32_t> parent={}) {
        return sonic::big_hud::left_anchored(pr,0x8CFFE000u,carrier,texlist,frames,parent);
    };
    require(big(0x8C0E6FA0u,0x8CFFE000u,0x8C54CA8Cu,0x8C565F84u),"Big weight artwork");
    require(big(0x8C0E714Cu,0x8CFFE000u,0x8C54CA8Cu,0x8C565F84u),"Big alternate ring artwork");
    require(big(0x8C0E7176u,0x8CFFE000u,0x8C566038u,0x8C566040u),"Big life icon");
    for(auto parent:{0x8C0E7494u,0x8C0E74C2u,0x8C0E75D2u})
        require(big(0x8C08F028u,0x8C1BF420u,0x8C1BF0E4u,0x8C1BF1C8u,parent),"Big HUD digits");
    require(!big(0x8C08F028u,0x8C1BF420u,0x8C1BF0E4u,0x8C1BF1C8u,0x8C0EBABCu),"Catch popup must remain centered");
    require(!big(0x8C08F028u,0x8C1BF420u,0x8C1BF0E4u,0x8C1BF1C8u),"Unproven formatter caller");
    require(!big(0x8C0E6E88u,0x8CFFE004u,0x8C54CA8Cu,0x8C565F84u),"Fishing overlay must remain centered");
    require(!big(0x8C0E6FA0u,0x8CFFE004u,0x8C54CA8Cu,0x8C565F84u),"Mismatched fishing carrier");
        require(argc==2,"test directory argument");
        const auto folder=std::filesystem::absolute(argv[1]);
        std::filesystem::create_directories(folder);
        SetEnvironmentVariableW(L"SARECOMP_DISPLAY_CONFIG",(folder/"sonic-display.ini").c_str());
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
            std::array<NativePortVertex,4> plane{};
            plane[0].position={0,0,1};plane[1].position={0,480,1};
            plane[2].position={640,0,1};plane[3].position={640,480,1};
            const auto overlay=[&](Role role=Role::Interface,bool textured=false){
                auto p=original;p.vertex_space=NativePortVertexSpace::PvrScreenReciprocal;
                p.topology=NativePortPrimitiveTopology::TriangleStrip;p.vertices=plane;
                if(textured)p.texture_stage=NativePortTextureStage::RequiredResolved;
                apply(p,role);return p;
            };
            require(overlay().transform.values==original.transform.values,"scene color overlay stayed 4:3");
            plane[1].position[1]=plane[3].position[1]=48;
            require(overlay().transform.values==original.transform.values,"cinematic bar stayed 4:3");
            require(overlay(Role::World).transform.values!=original.transform.values,"world plane stretched");
            require(overlay(Role::Interface,true).transform.values!=original.transform.values,"textured picture stretched");
            plane[2].position[0]=plane[3].position[0]=400;
            require(overlay().transform.values!=original.transform.values,"local UI panel stretched");
            plane[2].position[0]=plane[3].position[0]=640;
            std::swap(plane[1],plane[3]);
            require(overlay().transform.values!=original.transform.values,"overlapping triangles admitted");
            auto packet=original; apply(packet,Role::World);
            for(unsigned row=0;row<4;++row) for(unsigned col=1;col<4;++col)
                require(packet.transform.values[row*4+col]==original.transform.values[row*4+col],"Y/Z/W changed");
        }
        for(auto extent:{NativePortExtent{1366,768},NativePortExtent{1919,1079},NativePortExtent{3440,1440},NativePortExtent{640,480}})
          for(unsigned scale:{25u,50u,75u,100u}) {
            Settings selected;selected.width=extent.width;selected.height=extent.height;selected.widescreen=true;selected.render_percent=scale;
            save_settings(folder/"sonic-display.ini",selected);initialize(folder/"game.exe");
            NativePortGraphicsConfig config;configure(config);
            require(std::abs(config.render_extent.width-extent.width*double(scale)/100)<=.5,"render width ignores selected scale");
            require(std::abs(config.render_extent.height-extent.height*double(scale)/100)<=.5,"render height ignores selected scale");
            require(config.output_extent.width==extent.width && config.output_extent.height==extent.height,"scaling changed output");
            require(config.explicit_camera_aspect.numerator==extent.width && config.explicit_camera_aspect.denominator==extent.height,"raster rounding changed projection");
          }
        { std::ofstream f(folder/"sonic-display.ini"); f<<"mode=original\n"; }
        initialize(folder/"game.exe");
        require(settings().camera_style==sonic::camera::Style::Original,"missing camera setting must preserve original");
        NativePortDrawPacket packet;
        const auto original=packet;
        for(auto role:{Role::World,Role::Interface,Role::Fullscreen,Role::HudLeft,Role::HudRight}) {
            apply(packet,role);
            require(packet.transform.values==original.transform.values && packet.viewport==original.viewport,"original changed");
        }
        Settings configured;
        configured.camera_style=sonic::camera::Style::Recompiled;
        configured.renderer=sonic::rendering::Renderer::Vulkan;
        configured.text_language=4;
        save_settings(folder/"sonic-display.ini",configured);
        auto loaded=read_settings(folder/"sonic-display.ini");
        require(loaded.camera_style==sonic::camera::Style::Recompiled &&
            loaded.renderer==configured.renderer && loaded.text_language==4,"camera config roundtrip");
        { std::ofstream f(folder/"sonic-display.ini");f<<"camera_style=invalid\n"; }
        bool rejected=false;
        try { (void)read_settings(folder/"sonic-display.ini"); } catch(const std::exception&) {rejected=true;}
        require(rejected,"invalid camera setting accepted");
        Settings before,external,edited;
        external=before;external.width=3440;external.height=1440;external.render_percent=50;external.music_volume=30;
        save_settings(folder/"sonic-display.ini",external);edited=before;edited.renderer=sonic::rendering::Renderer::Vulkan;
        auto merged=save_settings_changes(folder/"sonic-display.ini",before,edited);
        require(merged.width==3440 && merged.height==1440 && merged.render_percent==50 && merged.music_volume==30 && merged.renderer==edited.renderer,"renderer edit overwrote external display/audio");
        edited=before;edited.widescreen=true;edited.camera_style=sonic::camera::Style::Recompiled;
        merged=save_settings_changes(folder/"sonic-display.ini",before,edited);
        require(merged.renderer==sonic::rendering::Renderer::Vulkan && merged.width==3440 && merged.widescreen && merged.camera_style==edited.camera_style,"aspect/camera edit reverted renderer");
        external=merged;external.bindings[0].key=65;edited=before;edited.bindings[0].pad=0x2000;
        merged=merge_settings(before,edited,external);
        require(merged.bindings[0].key==65 && merged.bindings[0].pad==0x2000,"independent key/pad mapping edit lost");
        require(merge_settings(before,before,external)==external,"no-op save reverted settings");
        // Isolated installed/portable contracts. No real AppData or save is touched.
        const auto install=folder/"install",appdata=folder/"appdata",data=folder/"data";
        std::filesystem::create_directories(install);
        SetEnvironmentVariableW(L"SARECOMP_DISPLAY_CONFIG",nullptr);
        SetEnvironmentVariableW(L"KATANA_USER_DATA_ROOT",nullptr);
        SetEnvironmentVariableW(L"SARECOMP_PORTABLE",nullptr);
        SetEnvironmentVariableW(L"SARECOMP_CACHE_ROOT",nullptr);
        SetEnvironmentVariableW(L"LOCALAPPDATA",appdata.c_str());
        save_settings(install/"sonic-display.ini",external);
        const auto user_config=configuration_path(install/"game.exe");
        require(user_config==appdata/"SARecomp/experimental/sonic-display.ini" && read_settings(user_config)==external,"legacy config migration");
        const auto installed=read_settings(install/"sonic-display.ini");
        save_settings(user_config,before);
        require(configuration_path(install/"game.exe")==user_config && read_settings(user_config)==before,"migration overwrote current user config");
        require(read_settings(install/"sonic-display.ini")==installed && !std::filesystem::exists(install/"user-data"),"installed files changed");
        SetEnvironmentVariableW(L"SARECOMP_PORTABLE",L"1");
        require(sonic::paths::data_root(install/"game.exe")==install/"user-data","explicit portable root");
        SetEnvironmentVariableW(L"KATANA_USER_DATA_ROOT",data.c_str());
        require(sonic::paths::data_root(install/"game.exe")==data && sonic::paths::cache_root(install/"game.exe")==data/"cache","override did not cover user/cache roots");
        const auto explicit_config=folder/"isolated/settings.ini";
        SetEnvironmentVariableW(L"SARECOMP_DISPLAY_CONFIG",explicit_config.c_str());
        require(configuration_path(install/"game.exe")==explicit_config && !std::filesystem::exists(explicit_config),"explicit config triggered migration");
        save_settings(explicit_config,before);require(read_settings(explicit_config)==before,"new settings parent missing");
        std::cout<<"SONIC_PRESENTATION_TESTS_OK aspects scaling(16) config-merge user-paths migration\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
