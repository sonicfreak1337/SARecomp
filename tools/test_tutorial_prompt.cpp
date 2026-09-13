#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include "sonic_tutorial_prompt.hpp"
#include "sonic_tutorial_art.hpp"
#include "katana/runtime/native_port_texture_asset.hpp"
#include <filesystem>
#include <fstream>
#include <map>
#include <iostream>
#include <stdexcept>

void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){
    try{
        check(argc==2 || argc==3,"preview directory and optional installed content directory required");
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
        using namespace sonic::tutorial;
        Controls controls;controls.bindings=bindings;controls.style=sonic::input::GlyphStyle::PlayStation;
        check(control_label(ControlToken::A,controls)==L"△","page must show gameplay A binding");
        controls.recompiled=true;
        check(control_label(ControlToken::Camera,controls)==L"RS","Recompiled camera stick");
        check(control_label(ControlToken::ShoulderPair,controls).find(L"L1")!=std::wstring::npos,
            "fishing exit is a trigger instruction, not free camera");
        check(control_label(ControlToken::ShoulderPair,controls).find(L"+")==std::wstring::npos,
            "independent shoulder inputs must not imply a simultaneous chord");
        controls.swap_sticks=true;
        check(control_label(ControlToken::Camera,controls)==L"LS" && control_label(ControlToken::Move,controls)==L"RS","swapped physical sticks");
        const std::array synthetic_span{ControlSpan{ControlToken::A,16,2,28,27}};
        const Artwork synthetic{"","","",0,64,32,synthetic_span};
        std::vector<std::uint8_t> pixels(64*32*4);
        for(unsigned i=0;i<pixels.size();i+=4){pixels[i]=117;pixels[i+1]=unsigned(i%233);pixels[i+2]=81;pixels[i+3]=255;}
        const auto retained=rasterize_artwork(synthetic,controls,pixels);
        for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x)if(x<16 || x>=44)
            for(unsigned c=0;c<4;++c)check(retained.pixels[(std::size_t(y)*64+x)*4+c]==
                std::byte(pixels[(std::size_t(y)*64+x)*4+c]),"non-control artwork changed");
        unsigned rendered=0;
        if(argc==3){
            std::map<std::string,std::vector<katana::runtime::NativePortDecodedTextureAsset>> decoded;
            for(const auto& art:artwork_catalog()){
                const auto name=std::string(art.archive);
                if(!decoded.contains(name)){
                    const auto path=std::filesystem::path(argv[2])/name;
                    std::ifstream file(path,std::ios::binary);check(bool(file),"installed tutorial archive unavailable");
                    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
                    decoded.emplace(name,katana::runtime::decode_native_port_prs_pvm_texture_archive(bytes));
                }
                check(art.ordinal<decoded.at(name).size(),"source ordinal outside decoded archive");
                const auto& original=decoded.at(name)[art.ordinal];
                check(original.extent.width==art.width && original.extent.height==art.height,"metadata/source extent");
                check(find_artwork(art.archive,art.archive_sha256,art.ordinal,art.pvrt_sha256)==&art,"exact page artwork lookup");
                check(!find_artwork(art.archive,art.archive_sha256,art.ordinal,"sha256:wrong"),"changed payload accepted");
                controls.style=sonic::input::GlyphStyle(1+rendered%3);controls.recompiled=rendered%2==0;
                controls.swap_sticks=false;controls.language=name.find("_E.PRS")!=std::string::npos?1:
                    name.find("_F.PRS")!=std::string::npos?2:name.find("_S.PRS")!=std::string::npos?3:
                    name.find("_G.PRS")!=std::string::npos?4:0;
                auto image=rasterize_artwork(art,controls,original.rgba8);
                const unsigned raster_scale=art.spans[0].token==ControlToken::Diagram?4:1;
                check(image.width==art.width*raster_scale && image.height==art.height*raster_scale,"page geometry changed");
                if(name=="SONICAD/TUTOMSG_SONIC_E.PRS" || art.ordinal<5 ||
                    name=="SONICAD/TUTOMSG_BIG.PRS"){
                    for(std::size_t p=0;p<image.pixels.size();p+=4)std::swap(image.pixels[p],image.pixels[p+2]);
                    Gdiplus::Bitmap bitmap(image.width,image.height,image.width*4,PixelFormat32bppARGB,
                        reinterpret_cast<BYTE*>(image.pixels.data()));
                    const auto path=output/(std::filesystem::path(name).stem().string()+"-"+std::to_string(art.ordinal)+".png");
                    check(bitmap.Save(path.c_str(),&png,nullptr)==Gdiplus::Ok,"page PNG export");
                }
                ++rendered;
            }
            check(decoded.size()==30 && rendered==272,"incomplete tutorial control inventory");
        }
        std::cout<<"SONIC_TUTORIAL_PAGE_TESTS PASS rendered="<<rendered<<" non_control_pixels=unchanged original_archive_owner=1\n";
        std::cout<<"SONIC_TUTORIAL_PROMPT_TESTS PASS languages=5 styles=3 actual_bindings=1 bounded_crop=1\n";
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
