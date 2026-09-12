#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include "sonic_tutorial_prompt.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>

void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){
    try{
        check(argc==2,"preview directory argument required");
        const auto output=std::filesystem::path(argv[1]);std::filesystem::create_directories(output);
        Gdiplus::GdiplusStartupInput startup;ULONG_PTR token=0;
        check(Gdiplus::GdiplusStartup(&token,&startup,nullptr)==Gdiplus::Ok,"GDI+ startup");
        struct End {ULONG_PTR token;~End(){Gdiplus::GdiplusShutdown(token);}} end{token};
        UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);
        std::vector<std::byte> codecs(size);auto* entries=reinterpret_cast<Gdiplus::ImageCodecInfo*>(codecs.data());
        Gdiplus::GetImageEncoders(count,size,entries);CLSID png{};
        for(unsigned i=0;i<count;++i)if(std::wstring_view(entries[i].MimeType)==L"image/png")png=entries[i].Clsid;
        auto bindings=sonic::input::default_bindings;
        bindings[unsigned(sonic::input::Action::A)].pad=1u<<13; // gameplay A -> Triangle / Y
        bindings[unsigned(sonic::input::Action::B)].pad=1u<<12; // gameplay B -> Square / X
        bindings[unsigned(sonic::input::Action::Confirm)].pad=1u<<10;
        for(int language=0;language<5;++language)for(unsigned style=1;style<=3;++style){
            const auto labels=sonic::tutorial::labels(language,bindings,sonic::input::GlyphStyle(style));
            if(style==2){check(labels.next.find(L"△")!=std::wstring::npos,"must show gameplay A binding, not Confirm");
                check(labels.back.find(L"□")!=std::wstring::npos,"must show gameplay Back binding");}
            auto image=sonic::tutorial::rasterize(labels,352);
            check(image.width==2048 && image.height==128 && image.pixels.size()==2048*128*4,"raster dimensions");
            unsigned glyph_pixels=0;
            for(unsigned y=0;y<image.height;++y)for(unsigned x=0;x<image.width;++x){
                const auto pixel=(std::size_t(y)*image.width+x)*4;
                check(image.pixels[pixel+3]==std::byte{255},"unexpected transparent gaps");
                if(std::to_integer<unsigned>(image.pixels[pixel+2])<150){++glyph_pixels;
                    check(x<352*4,"text escaped the visible original crop");}
            }
            check(glyph_pixels>100,"missing glyphs");
            for(std::size_t p=0;p<image.pixels.size();p+=4)std::swap(image.pixels[p],image.pixels[p+2]);
            Gdiplus::Bitmap bitmap(image.width,image.height,image.width*4,PixelFormat32bppARGB,
                reinterpret_cast<BYTE*>(image.pixels.data()));
            const auto path=output/(std::to_string(language)+"-"+std::to_string(style)+".png");
            check(bitmap.Save(path.c_str(),&png,nullptr)==Gdiplus::Ok,"PNG preview export");
        }
        bindings[unsigned(sonic::input::Action::B)]={};
        const auto fallback=sonic::tutorial::labels(1,bindings,sonic::input::GlyphStyle::PlayStation);
        check(fallback.back.find(L"□")!=std::wstring::npos,"unbound B lost X alternative");
        auto long_labels=sonic::tutorial::Labels{1,L"Mouse side button 1 : Next",L"Mouse side button 2 : Back"};
        check(!sonic::tutorial::rasterize(long_labels,352).pixels.empty(),"bounded two-row layout");
        std::cout<<"SONIC_TUTORIAL_PROMPT_TESTS PASS languages=5 styles=3 actual_bindings=1 bounded_crop=1\n";
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
