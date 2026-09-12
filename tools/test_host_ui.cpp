#define NOMINMAX
#include <windows.h>
#include "sonic_presentation.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace katana::runtime;
namespace fs=std::filesystem;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Capture {
    std::vector<unsigned char> bytes;
    unsigned word(unsigned p)const{return bytes[p]|unsigned(bytes[p+1])<<8|unsigned(bytes[p+2])<<16|unsigned(bytes[p+3])<<24;}
    unsigned width()const{return word(18);}unsigned height()const{return word(22);}
    std::array<unsigned char,3> rgb(unsigned x,unsigned y)const{
        const auto p=word(10)+((height()-1-y)*width()+x)*4;
        return {bytes[p+2],bytes[p+1],bytes[p]};
    }
    Capture(const fs::path& path){std::ifstream file(path,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(file),{});
        check(bytes.size()>=54&&bytes[0]=='B'&&bytes[1]=='M'&&bytes[28]==32,"capture format");
        check(bytes.size()>=word(10)+std::size_t(width())*height()*4,"capture size");}
};
int main(int argc,char** argv){
    try{
        check(argc==3,"renderer and fresh capture directory required");
        const auto root=fs::absolute(argv[2]);check(!fs::exists(root),"capture directory must be fresh");fs::create_directories(root);
        SetEnvironmentVariableW(L"KATANA_PORT_BACKGROUND_TEST",L"1");
        SetEnvironmentVariableW(L"KATANA_NATIVE_GRAPHICS_CAPTURE_DIRECTORY",root.c_str());
        SetEnvironmentVariableW(L"KATANA_NATIVE_GRAPHICS_CAPTURE_START_FRAME",L"1");
        SetEnvironmentVariableW(L"KATANA_NATIVE_GRAPHICS_CAPTURE_END_FRAME",L"6");
        SetEnvironmentVariableW(L"KATANA_NATIVE_GRAPHICS_CAPTURE_INTERVAL",L"1");
        SetEnvironmentVariableW(L"SARECOMP_CACHE_ROOT",(root/L"cache").c_str());
        sonic::rendering::selected_renderer=std::string_view(argv[1])=="vulkan"?sonic::rendering::Renderer::Vulkan:sonic::rendering::Renderer::D3D11;
        NativePortGraphicsConfig config;config.output_extent={960,540};config.render_extent={480,270};
        config.game_viewport=config.ui_viewport={NativePortViewportPolicy::FitAspect,{4,3}};config.initially_visible=false;config.synchronize_present=false;
        NativePortGraphicsDevice device(config);
        const auto scene=[&](std::array<float,4> color){NativePortFrameConfig frame;frame.clear_color=color;device.begin_frame(frame);device.present();device.finish();};
        scene({0,1,0,1});
        const unsigned stride=960*4+16;
        std::vector<std::byte> pixels(std::size_t(stride)*540);
        NativePortImageView image;image.extent={960,540};image.format=NativePortTextureFormat::Rgba8Unorm;
        image.stride_bytes=stride;image.bottom_up=true;image.pixels=pixels;
        const auto pattern=[&](bool invert){
            for(unsigned y=0;y<540;++y)for(unsigned x=0;x<960;++x){
                const auto p=std::size_t(539-y)*stride+x*4;const auto v=(((x^y)&1)^unsigned(invert))?255:0;
                pixels[p]=std::byte(v);pixels[p+1]=std::byte(255-v);pixels[p+2]=std::byte(y%251);pixels[p+3]=std::byte(255);
            }
        };
        pattern(false);device.present_image(image,NativePortViewportTarget::Ui,NativePortImageFit::Stretch);device.finish();
        std::fill(pixels.begin(),pixels.end(),std::byte{});device.repeat_present();device.finish();
        pattern(true);device.present_image(image,NativePortViewportTarget::Ui,NativePortImageFit::Stretch);device.finish();
        std::vector<std::byte> movie(80*60*4,std::byte{});for(unsigned p=0;p<movie.size();p+=4){movie[p]=movie[p+3]=std::byte(255);}
        image.extent={80,60};image.stride_bytes=80*4;image.bottom_up=false;image.pixels=movie;
        device.present_image(image,NativePortViewportTarget::Game,NativePortImageFit::Contain);device.finish();
        scene({0,0,1,1});
        const auto layout=device.layout();check(layout.render_extent==config.render_extent&&layout.game_viewport.x==60&&layout.game_viewport.y==0&&layout.game_viewport.width==360&&layout.game_viewport.height==270,"menu changed game resolution or viewport");
        for(unsigned frame=1;frame<=6;++frame){
            Capture c(root/("frame-"+std::to_string(frame)+".bmp"));const bool ui=frame>=2&&frame<=4;
            check(c.width()==(ui?960u:480u)&&c.height()==(ui?540u:270u),"host menu passed through the low-resolution game framebuffer");
            if(ui)for(unsigned y=0;y<c.height();++y)for(unsigned x=0;x<c.width();++x){
                const auto v=(((x^y)&1)^unsigned(frame==4))?255:0;
                check(c.rgb(x,y)==std::array<unsigned char,3>{static_cast<unsigned char>(v),static_cast<unsigned char>(255-v),static_cast<unsigned char>(y%251)},"host image lost pixel detail, orientation, or repeat lifetime");
            }
            if(frame==1)check(c.rgb(240,135)==std::array<unsigned char,3>{0,255,0},"initial game frame");
            if(frame==5)check(c.rgb(240,135)==std::array<unsigned char,3>{255,0,0}&&c.rgb(5,135)==std::array<unsigned char,3>{0,0,0},"movie fitting changed");
            if(frame==6)check(c.rgb(240,135)==std::array<unsigned char,3>{0,0,255},"return to game did not restore original framebuffer");
        }
        std::cout<<"SONIC_HOST_UI_OK renderer="<<argv[1]<<" output=960x540 game=480x270 exact_pixels=1 padded_bottom_up=1 repeat_lifetime=1 movie_fit=preserved game_return=1\n";return 0;
    }catch(const std::exception& e){std::cerr<<"SONIC_HOST_UI_FAIL "<<e.what()<<'\n';return 1;}
}
