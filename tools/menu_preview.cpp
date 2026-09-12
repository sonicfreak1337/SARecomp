#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include "sonic_menu.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){
    try{
        if(argc!=6&&argc!=7)throw std::invalid_argument("menu-preview output.png page language width height [recovery-status]");
        sonic::presentation::Settings settings;settings.text_language=std::stoi(argv[3]);settings.widescreen=true;
        settings.width=3440;settings.height=1440;settings.renderer=sonic::rendering::Renderer::Vulkan;
        settings.camera_style=sonic::camera::Style::Recompiled;
        if(argc==7&&std::string_view(argv[6])=="vsync")settings.vsync=1;
        sonic::menu::load_background("assets/ui/options-background.png");
        sonic::menu::Model model(settings,settings.text_language,true);model.choose(argv[2],{});
        if(argc==7){
            const std::string_view state=argv[6];
            if(state=="failure")model.status(sonic::recovery::BackupState::Failed,sonic::recovery::AudioState::RestartRequired);
            else if(state=="retry")model.status(sonic::recovery::BackupState::CleanupPending,sonic::recovery::AudioState::Partial);
            else if(state!="vsync")throw std::invalid_argument("unknown preview fixture");
        }
        auto image=sonic::menu::rasterize(model,std::stoul(argv[4]),std::stoul(argv[5]));
        for(std::size_t i=0;i<image.pixels.size();i+=4)std::swap(image.pixels[i],image.pixels[i+2]);
        Gdiplus::GdiplusStartupInput startup;ULONG_PTR token=0;
        if(Gdiplus::GdiplusStartup(&token,&startup,nullptr)!=Gdiplus::Ok)throw std::runtime_error("png-startup");
        {
            Gdiplus::Bitmap bitmap(image.width,image.height,image.width*4,PixelFormat32bppARGB,reinterpret_cast<BYTE*>(image.pixels.data()));
            UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);std::vector<std::byte> encoders(size);
            auto* list=reinterpret_cast<Gdiplus::ImageCodecInfo*>(encoders.data());Gdiplus::GetImageEncoders(count,size,list);bool saved=false;
            const auto output=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(output.parent_path());
            for(unsigned i=0;i<count;++i)if(std::wstring_view(list[i].MimeType)==L"image/png")saved=bitmap.Save(output.c_str(),&list[i].Clsid,nullptr)==Gdiplus::Ok;
            if(!saved)throw std::runtime_error("png-write");std::cout<<output.string()<<'\n';
        }
        Gdiplus::GdiplusShutdown(token);return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
