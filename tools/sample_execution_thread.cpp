// Private diagnostic tool, never linked into the game. Only the benchmark's
// owned process/thread pair is sampled; there is no system-wide trace.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <dbghelp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using Clock=std::chrono::steady_clock;
struct Handle {
    HANDLE value=nullptr;
    ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
};
void require(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
struct Suspension {
    HANDLE thread;DWORD previous;
    bool active=false;
    explicit Suspension(HANDLE handle):thread(handle),previous(SuspendThread(thread)),active(previous!=DWORD(-1)){}
    DWORD resume() noexcept {if(!active)return DWORD(-1);const auto result=ResumeThread(thread);if(result!=DWORD(-1))active=false;return result;}
    ~Suspension(){if(active)ResumeThread(thread);}
};
struct Module {std::string name;std::uint64_t base,size;};
struct StackFrame {std::uint64_t ip;std::string symbol;};
struct Stack {double elapsed_ms;std::vector<StackFrame> frames;};
struct Report {
    DWORD pid=0,tid=0,error=0;
    std::uint64_t samples=0,skipped=0,errors=0;
    double elapsed_ms=0,suspension_ms=0,max_suspension_ms=0;
    std::vector<Module> modules;
    std::map<std::uint64_t,std::uint64_t> ips;
    std::vector<Stack> stacks;
};
std::string narrow(const wchar_t* text){
    const auto size=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);
    require(size>0,"module name conversion");std::string result(size,'\0');
    WideCharToMultiByte(CP_UTF8,0,text,-1,result.data(),size,nullptr,nullptr);result.pop_back();return result;
}
struct Symbols {
    HANDLE process;bool active=false;
    ~Symbols(){if(active)SymCleanup(process);}
};
Report collect(DWORD pid,DWORD tid,unsigned milliseconds,bool with_stacks=false){
    Handle thread{OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,tid)};
    require(thread.value&&GetProcessIdOfThread(thread.value)==pid,"thread does not belong to the requested process");
    require(tid!=GetCurrentThreadId(),"cannot sample the calling thread");
    Handle process{OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|SYNCHRONIZE,FALSE,pid)};
    require(process.value!=nullptr,"cannot inspect target process");
    Report result;result.pid=pid;result.tid=tid;
    std::vector<HMODULE> modules(1024);DWORD bytes=0;
    require(EnumProcessModules(process.value,modules.data(),DWORD(modules.size()*sizeof(HMODULE)),&bytes)&&bytes<=modules.size()*sizeof(HMODULE),"module inventory failed");
    for(unsigned i=0;i<bytes/sizeof(HMODULE);++i){
        MODULEINFO info{};wchar_t name[512]{};
        if(GetModuleInformation(process.value,modules[i],&info,sizeof(info))&&GetModuleBaseNameW(process.value,modules[i],name,512))
            result.modules.push_back({narrow(name),reinterpret_cast<std::uint64_t>(info.lpBaseOfDll),info.SizeOfImage});
    }
    Symbols symbols{process.value};
    if(with_stacks){
        SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_FAIL_CRITICAL_ERRORS|SYMOPT_NO_PROMPTS|SYMOPT_IGNORE_CVREC|
            SYMOPT_IGNORE_IMAGEDIR|SYMOPT_IGNORE_NT_SYMPATH|SYMOPT_DISABLE_SYMSRV_AUTODETECT);
        // Unwind image data and local exports only. No PDB server or UI.
        symbols.active=SymInitialize(process.value,"",FALSE)!=FALSE;require(symbols.active,"symbol handler initialization");
        for(unsigned i=0;i<bytes/sizeof(HMODULE);++i){
            MODULEINFO info{};wchar_t path[32768]{};
            if(GetModuleInformation(process.value,modules[i],&info,sizeof(info))&&GetModuleFileNameExW(process.value,modules[i],path,32768)){
                const auto base=reinterpret_cast<DWORD64>(info.lpBaseOfDll);
                SymLoadModuleExW(process.value,nullptr,path,nullptr,base,info.SizeOfImage,nullptr,0);
                (void)SymFunctionTableAccess64(process.value,base+0x1000); // Prepare outside suspension.
            }
        }
    }
    // Inventory/symbol initialization and output happen while the thread runs.
    // The optional bounded unwind runs in this tool's own process; it never
    // calls target code. Use the SDK's actual aligned x64 CONTEXT layout.
    const auto start=Clock::now();const auto end=start+std::chrono::milliseconds(milliseconds);
    std::uint32_t jitter=0x53415245;auto next_stack=start;
    while(Clock::now()<end&&WaitForSingleObject(thread.value,0)==WAIT_TIMEOUT){
        CONTEXT context{};context.ContextFlags=with_stacks?CONTEXT_FULL:CONTEXT_CONTROL;
        std::array<std::uint64_t,16> stack{};unsigned depth=0;
        bool valid=false;DWORD previous=DWORD(-1),resumed=DWORD(-1),error=0;
        const auto before=Clock::now();
        {
            Suspension pause(thread.value);previous=pause.previous;
            if(pause.active){
                if(previous==0){
                    valid=GetThreadContext(thread.value,&context)!=FALSE;if(!valid)error=GetLastError();
                    const bool in_game=std::any_of(result.modules.begin(),result.modules.end(),[&](const Module& m){return m.name=="game.exe"&&context.Rip>=m.base&&context.Rip<m.base+m.size;});
                    if(valid&&with_stacks&&!in_game&&before>=next_stack&&result.stacks.size()<32){
                        auto unwind=context;STACKFRAME64 frame{};
                        frame.AddrPC.Offset=context.Rip;frame.AddrPC.Mode=AddrModeFlat;
                        frame.AddrStack.Offset=context.Rsp;frame.AddrStack.Mode=AddrModeFlat;
                        frame.AddrFrame.Offset=context.Rbp;frame.AddrFrame.Mode=AddrModeFlat;
                        stack[depth++]=context.Rip;DWORD64 last_sp=context.Rsp;bool first=true;
                        while(depth<stack.size()&&StackWalk64(IMAGE_FILE_MACHINE_AMD64,process.value,thread.value,&frame,&unwind,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr)){
                            if(!frame.AddrPC.Offset)break;
                            if(frame.AddrPC.Offset==stack[depth-1]&&frame.AddrStack.Offset==last_sp){if(first){first=false;continue;}break;}
                            first=false;last_sp=frame.AddrStack.Offset;
                            stack[depth++]=frame.AddrPC.Offset;
                        }
                    }
                }
                resumed=pause.resume();if(resumed==DWORD(-1))error=GetLastError();
            }else error=GetLastError();
        }
        const auto duration=std::chrono::duration<double,std::milli>(Clock::now()-before).count();
        if(previous!=DWORD(-1)){
            result.suspension_ms+=duration;result.max_suspension_ms=std::max(result.max_suspension_ms,duration);
        }
        if(error){++result.errors;result.error=error;break;}
        if(previous!=0)++result.skipped; // Leave someone else's suspension intact.
        else if(valid&&resumed!=DWORD(-1)){++result.samples;++result.ips[context.Rip];}
        if(depth){
            Stack captured{std::chrono::duration<double,std::milli>(before-start).count()};
            for(unsigned i=0;i<depth;++i){
                alignas(SYMBOL_INFO) std::array<char,sizeof(SYMBOL_INFO)+1024> storage{};
                auto* symbol=reinterpret_cast<SYMBOL_INFO*>(storage.data());symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=1024;
                DWORD64 displacement=0;std::string name;
                if(SymFromAddr(process.value,stack[i],&displacement,symbol))name.assign(symbol->Name,symbol->NameLen);
                captured.frames.push_back({stack[i],std::move(name)});
            }
            result.stacks.push_back(std::move(captured));next_stack=Clock::now()+std::chrono::milliseconds(700);
        }
        jitter^=jitter<<13;jitter^=jitter>>17;jitter^=jitter<<5;
        Sleep(7+jitter%7); // Bounded jitter avoids sampling one fixed frame phase.
    }
    result.elapsed_ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();return result;
}
std::string quoted(const std::string& text){
    std::string out="\"";constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:text){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}
    return out+'"';
}
void write(const Report& r,const std::filesystem::path& path){
    require(!std::filesystem::exists(path),"profile output must be new");std::ofstream out(path);require(bool(out),"profile output open");
    out<<"{\n  \"schema\": \"sarecomp-execution-ip-v1\",\n  \"pid\": "<<r.pid<<", \"tid\": "<<r.tid
       <<",\n  \"samples\": "<<r.samples<<", \"skipped_suspended\": "<<r.skipped<<", \"errors\": "<<r.errors<<", \"last_error\": "<<r.error
       <<",\n  \"elapsed_ms\": "<<r.elapsed_ms<<", \"suspension_ms\": "<<r.suspension_ms<<", \"max_suspension_ms\": "<<r.max_suspension_ms<<",\n  \"modules\": [";
    bool comma=false;for(const auto& m:r.modules){if(comma)out<<',';comma=true;out<<"\n    {\"name\": "<<quoted(m.name)<<", \"base\": "<<m.base<<", \"size\": "<<m.size<<'}';}
    out<<"\n  ],\n  \"ips\": [";comma=false;
    for(const auto& [ip,count]:r.ips){if(comma)out<<',';comma=true;out<<"\n    {\"ip\": "<<ip<<", \"count\": "<<count<<'}';}
    out<<"\n  ],\n  \"stacks\": [";comma=false;
    for(const auto& stack:r.stacks){
        if(comma)out<<',';comma=true;out<<"\n    {\"elapsed_ms\": "<<stack.elapsed_ms<<", \"frames\": [";bool next=false;
        for(const auto& frame:stack.frames){if(next)out<<',';next=true;out<<"{\"ip\": "<<frame.ip<<", \"symbol\": "<<quoted(frame.symbol)<<'}';}out<<"]}";
    }
    out<<"\n  ]\n}\n";require(bool(out),"profile output write");
}
struct Worker {
    std::atomic<bool> stop{false};std::atomic<std::uint64_t> progress{0};DWORD id=0;Handle thread;
    Worker(){thread.value=CreateThread(nullptr,0,[](void* p)->DWORD{auto& w=*static_cast<Worker*>(p);while(!w.stop.load())++w.progress;return 0;},this,0,&id);require(thread.value!=nullptr,"self-test worker");}
    ~Worker(){stop=true;WaitForSingleObject(thread.value,2000);}
};
void self_test(){
    Worker worker;bool rejected=false;
    try{collect(GetCurrentProcessId()+1,worker.id,50);}catch(const std::exception&){rejected=true;}
    require(rejected,"foreign process/thread pair accepted");
    const auto normal=collect(GetCurrentProcessId(),worker.id,150);
    require(normal.samples>0&&!normal.errors&&worker.progress>0,"worker sample or resume failed");
    const auto stack=collect(GetCurrentProcessId(),worker.id,100,true);
    require(!stack.stacks.empty()&&stack.stacks[0].frames.size()>1&&!stack.errors,"unwind self-test failed");
    {
        Suspension external(worker.thread.value);require(external.previous==0,"self-test external suspension");
        const auto held=collect(GetCurrentProcessId(),worker.id,50);
        require(!held.samples&&held.skipped>0&&!held.errors,"external suspension not preserved");
        require(external.resume()==1,"sampler changed external suspension count");
    }
    const auto before=worker.progress.load();Sleep(30);require(worker.progress>before,"worker left suspended");
    std::cout<<"EXECUTION_SAMPLER_TEST_OK samples="<<normal.samples<<" foreign_pair=rejected external_suspend=preserved resume=verified\n";
}
int wmain(int argc,wchar_t** argv){
    try{
        if(argc==2&&std::wstring(argv[1])==L"--self-test"){self_test();return 0;}
        require(argc==5||(argc==6&&std::wstring(argv[5])==L"--stacks"),"usage: sampler PID TID DURATION_MS NEW_OUTPUT.json [--stacks]");
        const auto pid=std::stoul(argv[1]),tid=std::stoul(argv[2]),duration=std::stoul(argv[3]);
        require(pid>0&&tid>0&&duration>=1000&&duration<=30000,"invalid bounded sample request");
        const auto report=collect(pid,tid,duration,argc==6);write(report,argv[4]);return report.samples&&!report.errors?0:1;
    }catch(const std::exception& e){std::cerr<<"EXECUTION_SAMPLER_ERROR "<<e.what()<<'\n';return 1;}
}
