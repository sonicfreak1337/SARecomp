#define NOMINMAX
#include <windows.h>
#include "sonic_startup.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){
    try {
        require(argc==2,"usage: sonic_startup_tests <new-fixture-directory>");
        const auto root=std::filesystem::absolute(argv[1]);require(!std::filesystem::exists(root),"fixture must be new");std::filesystem::create_directories(root);
        _putenv_s("SARECOMP_CACHE_ROOT",root.string().c_str());_putenv_s("KATANA_PORT_BACKGROUND_TEST","1");_putenv_s("SARECOMP_STARTUP_UI_TEST","1");
        using namespace sonic::startup;
        require(digest("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA-256 differs");
        const auto key=digest("source:compiler:profile:flags");const std::array data{std::byte{1},std::byte{2},std::byte{3}};
        require(cache_load("test",key,100).empty(),"unexpected hit");require(cache_save("test",key,data),"cache save failed");
        require(cache_load("test",key,100)==std::vector<std::byte>(data.begin(),data.end()),"cache roundtrip differs");
        require(cache_save("d3d-shaders",key,data) && cache_load("d3d-shaders",key,100)==std::vector<std::byte>(data.begin(),data.end()),"D3D shader cache domain rejected");
        require(cache_load("test",digest("changed-source"),100).empty(),"identity change did not miss");
        require(cache_load("test",key,2).empty(),"oversized cache accepted");
        const auto file=root/"startup-v1"/"test"/(key+".bin");{std::fstream f(file,std::ios::binary|std::ios::in|std::ios::out);f.seekp(81);f.put('x');}
        require(cache_load("test",key,100).empty(),"corrupt cache accepted");
        require(cache_save("test",key,data),"bad cache could not be replaced");std::filesystem::resize_file(file,79);
        require(cache_load("test",key,100).empty(),"truncated header accepted");
        require(!cache_save("../escape",key,data),"cache path traversal accepted");
        require(!cache_save("test","../escape",data),"invalid cache key accepted");
        const auto blocked=root/"blocked";{std::ofstream f(blocked);f<<"file";}_putenv_s("SARECOMP_CACHE_ROOT",blocked.string().c_str());
        require(!cache_save("test",key,data) && cache_load("test",key,100).empty(),"unwritable cache must be optional");
        _putenv_s("SARECOMP_CACHE_ROOT",root.string().c_str());
        {
            Session session;phase("Loading game code (MB)",720,1800);
            HWND window=nullptr;
            for(unsigned i=0;i<100 && !window;++i){
                EnumWindows([](HWND candidate,LPARAM result)->BOOL {DWORD pid=0;GetWindowThreadProcessId(candidate,&pid);if(pid!=GetCurrentProcessId())return TRUE;
                    std::array<wchar_t,64> cls{};GetClassNameW(candidate,cls.data(),int(cls.size()));if(std::wstring_view(cls.data())!=L"SARecomp.Startup")return TRUE;
                    *reinterpret_cast<HWND*>(result)=candidate;return FALSE;},reinterpret_cast<LPARAM>(&window));
                if(!window)Sleep(20);
            }
            require(window && !IsWindowVisible(window),"startup UI fixture must exist and remain hidden");
            RECT rect{};GetClientRect(window,&rect);auto dc=GetDC(window);auto memory=CreateCompatibleDC(dc);
            BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=rect.right;info.bmiHeader.biHeight=rect.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
            void* pixels=nullptr;auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);require(bitmap && pixels,"capture bitmap failed");auto previous=SelectObject(memory,bitmap);
            SendMessageW(window,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(memory),PRF_CLIENT);
            BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+rect.right*rect.bottom*4;
            std::ofstream output(root/"startup-progress.bmp",std::ios::binary);output.write(reinterpret_cast<char*>(&header),sizeof(header));output.write(reinterpret_cast<char*>(&info.bmiHeader),sizeof(info.bmiHeader));output.write(static_cast<char*>(pixels),rect.right*rect.bottom*4);output.close();
            SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(window,dc);
            phase("Preparing graphics...",2,7);advance();finish();
        }
        std::cout<<"SONIC_STARTUP_TEST_OK cache_integrity=1 invalidation=1 optional_io=1 hidden_progress=1\n";return 0;
    }catch(const std::exception& error){std::cerr<<"SONIC_STARTUP_TEST_FAILED "<<error.what()<<'\n';return 1;}
}
