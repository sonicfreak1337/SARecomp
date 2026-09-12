#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include "katana/runtime/native_port_texture_asset.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char** argv){
    if(argc!=3)return 1;try {
        std::ifstream input(argv[1],std::ios::binary);std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)),{});
        const auto textures=katana::runtime::decode_native_port_prs_pvm_texture_archive(data);
        Gdiplus::GdiplusStartupInput start;ULONG_PTR token=0;Gdiplus::GdiplusStartup(&token,&start,nullptr);
        UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);std::vector<std::byte> storage(size);
        auto enc=reinterpret_cast<Gdiplus::ImageCodecInfo*>(storage.data());Gdiplus::GetImageEncoders(count,size,enc);CLSID codec{};
        for(unsigned i=0;i<count;++i)if(std::wstring_view(enc[i].MimeType)==L"image/png")codec=enc[i].Clsid;
        std::filesystem::create_directories(argv[2]);
        for(const auto& t:textures){
            auto bytes=t.rgba8;for(std::size_t i=0;i<bytes.size();i+=4)std::swap(bytes[i],bytes[i+2]);
            Gdiplus::Bitmap bitmap(t.extent.width,t.extent.height,t.extent.width*4,PixelFormat32bppARGB,bytes.data());
            const auto path=std::filesystem::absolute(argv[2])/(std::to_string(t.archive_ordinal)+"-"+t.name+".png");
            if(bitmap.Save(path.c_str(),&codec,nullptr)!=Gdiplus::Ok)return 2;
            std::cout<<t.archive_ordinal<<' '<<t.name<<' '<<t.extent.width<<'x'<<t.extent.height<<'\n';
        }
        Gdiplus::GdiplusShutdown(token);return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 3;}
}
