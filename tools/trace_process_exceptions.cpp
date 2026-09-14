#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <dbghelp.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

// Private diagnostic helper. The benchmark supplies ONLY its newly owned game
// PID. Capture first-chance host faults, then let the original handler run.
// No dump of RAM/saves, process termination or desktop input is performed.
void capture(HANDLE process,DWORD tid,const EXCEPTION_DEBUG_INFO& exception){
    const auto& record=exception.ExceptionRecord;
    std::cout<<"SONIC_EXCEPTION_TRACE code="<<record.ExceptionCode<<" first="<<exception.dwFirstChance
        <<" tid="<<tid<<" ip="<<reinterpret_cast<std::uintptr_t>(record.ExceptionAddress);
    for(DWORD i=0;i<record.NumberParameters && i<2;++i)std::cout<<" info"<<i<<'='<<record.ExceptionInformation[i];
    std::cout<<'\n';
    SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_FAIL_CRITICAL_ERRORS|SYMOPT_NO_PROMPTS|SYMOPT_IGNORE_CVREC);
    const bool symbols=SymInitialize(process,"",FALSE)!=FALSE;
    std::vector<HMODULE> modules(256);DWORD needed=0;
    if(EnumProcessModulesEx(process,modules.data(),DWORD(modules.size()*sizeof(HMODULE)),&needed,LIST_MODULES_64BIT)){
        const auto count=(std::min)(modules.size(),std::size_t(needed/sizeof(HMODULE)));
        for(std::size_t i=0;i<count;++i){
            MODULEINFO info{};wchar_t path[32768]{};
            if(!GetModuleInformation(process,modules[i],&info,sizeof(info)) ||
               !GetModuleFileNameExW(process,modules[i],path,DWORD(std::size(path))))continue;
            const auto base=reinterpret_cast<DWORD64>(info.lpBaseOfDll);
            std::cout<<"SONIC_EXCEPTION_MODULE base="<<base<<" size="<<info.SizeOfImage
                <<" name="<<std::filesystem::path(path).filename().string()<<'\n';
            if(symbols)SymLoadModuleExW(process,nullptr,path,nullptr,base,info.SizeOfImage,nullptr,0);
        }
    }
    HANDLE thread=OpenThread(THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,tid);
    CONTEXT context{};context.ContextFlags=CONTEXT_FULL;
    if(thread && GetThreadContext(thread,&context)){
        STACKFRAME64 frame{};frame.AddrPC={context.Rip,0,AddrModeFlat};
        frame.AddrStack={context.Rsp,0,AddrModeFlat};frame.AddrFrame={context.Rbp,0,AddrModeFlat};
        DWORD64 last_pc=0,last_sp=0;
        for(unsigned i=0;i<40;++i){
            if(i>1u && frame.AddrPC.Offset==last_pc && frame.AddrStack.Offset==last_sp)break;
            last_pc=frame.AddrPC.Offset;last_sp=frame.AddrStack.Offset;
            std::cout<<"SONIC_EXCEPTION_FRAME depth="<<i<<" ip="<<last_pc<<" sp="<<last_sp;
            alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+512]{};
            auto* symbol=reinterpret_cast<SYMBOL_INFO*>(storage);symbol->SizeOfStruct=sizeof(*symbol);symbol->MaxNameLen=511;
            DWORD64 displacement=0;
            if(symbols && SymFromAddr(process,last_pc,&displacement,symbol))std::cout<<" symbol="<<symbol->Name<<" displacement="<<displacement;
            std::cout<<'\n';
            if(!symbols || !StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,thread,&frame,&context,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr))break;
        }
    }
    if(thread)CloseHandle(thread);
    if(symbols)SymCleanup(process);
    std::cout<<std::flush;
}
int wmain(int argc,wchar_t** argv){
    if(argc!=3)return 2;
    const auto pid=DWORD(std::stoul(argv[1]));
    HANDLE process=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|SYNCHRONIZE,FALSE,pid);
    if(!process)return 3;
    wchar_t path[32768]{};DWORD length=DWORD(std::size(path));
    if(!QueryFullProcessImageNameW(process,0,path,&length) || _wcsicmp(path,argv[2])!=0){CloseHandle(process);return 4;}
    if(!DebugActiveProcess(pid)){CloseHandle(process);return 5;}
    if(!DebugSetProcessKillOnExit(FALSE)){DebugActiveProcessStop(pid);CloseHandle(process);return 6;}
    std::cout<<"SONIC_EXCEPTION_TRACE_ATTACHED pid="<<pid<<'\n'<<std::flush;
    bool first_break=true,captured=false,exited=false;const auto started=GetTickCount64();
    while(GetTickCount64()-started<150000){
        DEBUG_EVENT event{};if(!WaitForDebugEvent(&event,500))continue;
        DWORD status=DBG_CONTINUE;
        if(event.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT){
            if(event.u.CreateProcessInfo.hFile)CloseHandle(event.u.CreateProcessInfo.hFile);
            if(event.u.CreateProcessInfo.hThread)CloseHandle(event.u.CreateProcessInfo.hThread);
            if(event.u.CreateProcessInfo.hProcess)CloseHandle(event.u.CreateProcessInfo.hProcess);
        }else if(event.dwDebugEventCode==CREATE_THREAD_DEBUG_EVENT){CloseHandle(event.u.CreateThread.hThread);
        }else if(event.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT){if(event.u.LoadDll.hFile)CloseHandle(event.u.LoadDll.hFile);
        }else if(event.dwDebugEventCode==EXCEPTION_DEBUG_EVENT){
            const auto code=event.u.Exception.ExceptionRecord.ExceptionCode;
            if(first_break && code==EXCEPTION_BREAKPOINT)first_break=false;
            else status=DBG_EXCEPTION_NOT_HANDLED;
            if(!captured && (code==EXCEPTION_ACCESS_VIOLATION || code==0xC0000374u || !event.u.Exception.dwFirstChance)){
                capture(process,event.dwThreadId,event.u.Exception);captured=true;
            }
        }else if(event.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT){
            std::cout<<"SONIC_EXCEPTION_TRACE_EXIT code="<<event.u.ExitProcess.dwExitCode<<'\n'<<std::flush;exited=true;
        }
        ContinueDebugEvent(event.dwProcessId,event.dwThreadId,status);
        if(exited)break;
    }
    if(!exited)DebugActiveProcessStop(pid);
    CloseHandle(process);return exited?0:7;
}
