#include "sonic_startup.hpp"
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <psapi.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <thread>

namespace sonic::startup {
namespace {
bool enabled(const char* name) {const auto* v=std::getenv(name);return v && std::string_view(v)=="1";}
struct Progress {
    std::mutex mutex;
    std::string label="Starting Sonic Adventure...";
    std::uint64_t completed=0,total=0;
    std::atomic<bool> active=false,stop=false;
    std::thread thread;
    HFONT title_font=nullptr,text_font=nullptr;
    std::chrono::steady_clock::time_point started;
};
Progress progress;
LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_TIMER){if(progress.stop){DestroyWindow(window);return 0;}InvalidateRect(window,nullptr,FALSE);return 0;}
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    if(message!=WM_PAINT && message!=WM_PRINTCLIENT)return DefWindowProcW(window,message,wp,lp);
    std::string label;std::uint64_t done,total;
    {std::lock_guard lock(progress.mutex);label=progress.label;done=progress.completed;total=progress.total;}
    PAINTSTRUCT paint{};auto dc=message==WM_PRINTCLIENT?reinterpret_cast<HDC>(wp):BeginPaint(window,&paint);RECT client{};GetClientRect(window,&client);
    auto memory=CreateCompatibleDC(dc);auto bitmap=CreateCompatibleBitmap(dc,client.right,client.bottom);auto previous=SelectObject(memory,bitmap);
    auto background=CreateSolidBrush(RGB(16,28,48));FillRect(memory,&client,background);DeleteObject(background);
    SetBkMode(memory,TRANSPARENT);SetTextColor(memory,RGB(242,247,255));SelectObject(memory,progress.title_font);
    RECT title{24,22,client.right-24,58};DrawTextW(memory,L"Sonic Adventure: Recompiled",-1,&title,DT_LEFT|DT_SINGLELINE);
    SelectObject(memory,progress.text_font);SetTextColor(memory,RGB(184,204,229));
    std::wstring text(label.begin(),label.end());RECT detail{24,67,client.right-24,92};DrawTextW(memory,text.c_str(),-1,&detail,DT_LEFT|DT_SINGLELINE);
    RECT track{24,107,client.right-24,121};auto dark=CreateSolidBrush(RGB(42,60,83));FillRect(memory,&track,dark);DeleteObject(dark);
    RECT fill=track;
    if(total) fill.right=fill.left+LONG(double(track.right-track.left)*std::min(done,total)/total);
    else {const auto width=track.right-track.left;const auto position=LONG((GetTickCount64()/8)%(width+90))-90;fill.left=track.left+std::max(0L,position);fill.right=track.left+std::min(width,position+90);}
    auto blue=CreateSolidBrush(RGB(25,160,237));FillRect(memory,&fill,blue);DeleteObject(blue);
    if(total) {const auto counts=std::to_wstring(done)+L" / "+std::to_wstring(total);RECT count{24,133,client.right-24,157};DrawTextW(memory,counts.c_str(),-1,&count,DT_RIGHT|DT_SINGLELINE);}
    BitBlt(dc,0,0,client.right,client.bottom,memory,0,0,SRCCOPY);SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);if(message==WM_PAINT)EndPaint(window,&paint);return 0;
}
std::filesystem::path cache_path(std::string_view domain,std::string_view key) {
    if(key.size()!=64 || !std::all_of(key.begin(),key.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}) ||
       domain.empty() || !std::all_of(domain.begin(),domain.end(),[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-';}))return {};
    std::filesystem::path root;
    if(const auto* test=std::getenv("SARECOMP_CACHE_ROOT");test&&*test)root=test;
    else if(const auto* data=std::getenv("KATANA_USER_DATA_ROOT");data&&*data)root=std::filesystem::path(data)/"cache";
    else if(const auto* local=std::getenv("LOCALAPPDATA");local&&*local)root=std::filesystem::path(local)/"SARecomp"/"experimental"/"cache";
    if(root.empty() || enabled("SARECOMP_DISABLE_STARTUP_CACHE"))return {};
    return root/"startup-v1"/domain/(std::string(key)+".bin");
}
constexpr std::array<char,8> cache_magic{'S','A','R','C','C','0','0','1'};
}

Session::Session() {
    progress.started=std::chrono::steady_clock::now();progress.active=true;progress.stop=false;
    if(enabled("KATANA_PORT_BACKGROUND_TEST") && !enabled("SARECOMP_STARTUP_UI_TEST"))return;
    try {progress.thread=std::thread([] {
        WNDCLASSW cls{};cls.lpfnWndProc=window_proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"SARecomp.Startup";cls.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
        if(!RegisterClassW(&cls))return;
        progress.title_font=CreateFontW(-23,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        progress.text_font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        RECT area{};SystemParametersInfoW(SPI_GETWORKAREA,0,&area,0);constexpr int width=520,height=200;
        auto window=CreateWindowExW(0,cls.lpszClassName,L"Sonic Adventure: Recompiled",WS_POPUP|WS_CAPTION,
            area.left+(area.right-area.left-width)/2,area.top+(area.bottom-area.top-height)/2,width,height,nullptr,nullptr,cls.hInstance,nullptr);
        if(window){
            if(SetTimer(window,1,40,nullptr)) {
                if(!progress.stop && !enabled("KATANA_PORT_BACKGROUND_TEST"))ShowWindow(window,SW_SHOWNOACTIVATE);
                MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
            }else DestroyWindow(window);
        }
        DeleteObject(progress.title_font);DeleteObject(progress.text_font);UnregisterClassW(cls.lpszClassName,cls.hInstance);
    });}catch(...){std::fprintf(stderr,"SONIC_STARTUP progress_window_unavailable=1\n");}
}
Session::~Session(){finish();if(progress.thread.joinable())progress.thread.join();}
void phase(std::string_view label,std::uint64_t done,std::uint64_t total) noexcept {
    if(!progress.active)return;
    try {std::lock_guard lock(progress.mutex);if(progress.label!=label)std::fprintf(stderr,"SONIC_STARTUP phase=%.*s\n",int(label.size()),label.data());progress.label=label;progress.completed=done;progress.total=total;}catch(...){}
}
void advance() noexcept {if(progress.active){std::lock_guard lock(progress.mutex);if(progress.completed<progress.total)++progress.completed;}}
void finish() noexcept {
    if(!progress.active.exchange(false))return;
    progress.stop=true;
    const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-progress.started).count();
    std::fprintf(stderr,"SONIC_STARTUP ready=1 elapsed_ms=%.3f\n",ms);
}
std::string digest(std::span<const std::byte> bytes) {
    std::array<unsigned char,32> result{};
    if(bytes.size()>ULONG_MAX || BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<std::byte*>(bytes.data())),ULONG(bytes.size()),result.data(),ULONG(result.size()))<0)return {};
    constexpr char hex[]="0123456789abcdef";std::string text(64,'0');for(std::size_t i=0;i<result.size();++i){text[i*2]=hex[result[i]>>4];text[i*2+1]=hex[result[i]&15];}return text;
}
std::string file_digest(const std::filesystem::path& path) noexcept {
    try {const auto size=std::filesystem::file_size(path);if(size>32*1024*1024)return {};std::vector<std::byte> bytes(size);std::ifstream file(path,std::ios::binary);if(!file.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))return {};return digest(bytes);}catch(...){return {};}
}
std::vector<std::byte> cache_load(std::string_view domain,std::string_view key,std::size_t maximum) noexcept {
    try {
        const auto path=cache_path(domain,key);if(path.empty())return {};std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)return {};
        const auto size=file.tellg();if(size<80 || std::uint64_t(size)>maximum+80)return {};file.seekg(0);
        std::array<char,8> magic{};std::uint64_t count=0;std::array<char,64> hash{};
        file.read(magic.data(),magic.size());file.read(reinterpret_cast<char*>(&count),8);file.read(hash.data(),hash.size());
        if(!file || magic!=cache_magic || count!=std::uint64_t(size)-80 || count>maximum)return {};
        std::vector<std::byte> data(count);if(!file.read(reinterpret_cast<char*>(data.data()),data.size()) || digest(data)!=std::string_view(hash.data(),hash.size()))return {};return data;
    }catch(...){return {};}
}
bool cache_save(std::string_view domain,std::string_view key,std::span<const std::byte> data) noexcept {
    try {
        const auto path=cache_path(domain,key);if(path.empty())return false;const auto hash=digest(data);if(hash.empty())return false;
        std::filesystem::create_directories(path.parent_path());static std::atomic<unsigned> sequence=0;
        auto temporary=path;temporary+=L"."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(sequence++)+L".tmp";
        const auto handle=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(handle==INVALID_HANDLE_VALUE)return false;
        const auto write=[&](const void* p,std::size_t count){DWORD written=0;return count<=MAXDWORD && WriteFile(handle,p,DWORD(count),&written,nullptr) && written==count;};
        const auto size=std::uint64_t(data.size());const bool ok=write(cache_magic.data(),8)&&write(&size,8)&&write(hash.data(),64)&&write(data.data(),data.size());CloseHandle(handle);
        const bool moved=ok&&MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING);if(!moved)DeleteFileW(temporary.c_str());return moved;
    }catch(...){return false;}
}
void prefetch_program() noexcept {
    if(enabled("SARECOMP_DISABLE_CODE_PREFETCH"))return;
    const auto started=std::chrono::steady_clock::now();std::uint64_t total=0,done=0;DWORD error=0;
    try {
        using Prefetch=BOOL(WINAPI*)(HANDLE,ULONG_PTR,PWIN32_MEMORY_RANGE_ENTRY,ULONG);
        const auto prefetch=reinterpret_cast<Prefetch>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"PrefetchVirtualMemory"));if(!prefetch)return;
        MEMORYSTATUSEX memory{sizeof(memory)};if(!GlobalMemoryStatusEx(&memory))return;
        // Warm mapped read-only program pages, never pin memory or preload all
        // game assets. Leave at least two thirds of currently free RAM alone.
        std::uint64_t budget=std::min<std::uint64_t>(2ull*1024*1024*1024,memory.ullAvailPhys/3);
        const auto* base=reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);const auto* sections=IMAGE_FIRST_SECTION(nt);
        std::vector<WIN32_MEMORY_RANGE_ENTRY> ranges;
        for(unsigned pass=0;pass<2;++pass)for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){const auto& section=sections[i];
            const auto flags=section.Characteristics;
            if(!(flags&IMAGE_SCN_MEM_READ)||(flags&(IMAGE_SCN_MEM_WRITE|IMAGE_SCN_MEM_DISCARDABLE)) || bool(flags&IMAGE_SCN_MEM_EXECUTE)!=!bool(pass))continue;
            auto offset=std::uint64_t(section.VirtualAddress),end=std::min<std::uint64_t>(nt->OptionalHeader.SizeOfImage,offset+section.Misc.VirtualSize);
            while(offset<end && budget){MEMORY_BASIC_INFORMATION info{};if(!VirtualQuery(base+offset,&info,sizeof(info)))break;
                const auto extent=std::min(end,std::uint64_t(reinterpret_cast<const std::byte*>(info.BaseAddress)-base)+info.RegionSize);
                if(extent<=offset)break;
                if(info.State==MEM_COMMIT && info.Type==MEM_IMAGE && !(info.Protect&(PAGE_NOACCESS|PAGE_GUARD))){
                    while(offset<extent && budget){const auto size=std::min({extent-offset,budget,8ull*1024*1024});ranges.push_back({const_cast<std::byte*>(base+offset),SIZE_T(size)});offset+=size;budget-=size;total+=size;}
                }else offset=extent;
            }
        }
        phase("Loading game code (MB)",0,(total+1048575)/1048576);
        for(auto& range:ranges){if(!prefetch(GetCurrentProcess(),1,&range,0)){error=GetLastError();break;}done+=range.NumberOfBytes;phase("Loading game code (MB)",(done+1048575)/1048576,(total+1048575)/1048576);}
    }catch(...){error=ERROR_NOT_ENOUGH_MEMORY;}
    const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    std::fprintf(stderr,"SONIC_STARTUP_PREFETCH requested_bytes=%llu completed_bytes=%llu elapsed_ms=%.3f error=%lu\n",total,done,ms,error);
}
void frame(std::uint64_t index) noexcept {
    if(index>=2)finish();
    static const bool trace=enabled("SARECOMP_STARTUP_TRACE");
    if(!trace || index>600)return;
    static auto previous=std::chrono::steady_clock::now();static DWORD previous_faults=0;static std::uint64_t previous_reads=0;
    const auto now=std::chrono::steady_clock::now();const auto ms=std::chrono::duration<double,std::milli>(now-previous).count();previous=now;
    PROCESS_MEMORY_COUNTERS memory{sizeof(memory)};IO_COUNTERS io{};GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory));GetProcessIoCounters(GetCurrentProcess(),&io);
    const auto faults=memory.PageFaultCount-previous_faults;const auto reads=io.ReadTransferCount-previous_reads;previous_faults=memory.PageFaultCount;previous_reads=io.ReadTransferCount;
    if(index%30==0 || ms>80)std::fprintf(stderr,"SONIC_STARTUP_FRAME frame=%llu interval_ms=%.3f page_faults=%lu read_bytes=%llu working_set=%zu\n",index,ms,faults,reads,memory.WorkingSetSize);
}
}
