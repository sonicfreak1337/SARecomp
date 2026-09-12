#define NOMINMAX
#include <windows.h>
#include "sonic_restart.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace fs=std::filesystem;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void mark(const fs::path& path){std::ofstream file(path);file<<"ok\n";require(bool(file),"test marker");}
void wait_marker(const fs::path& path){for(unsigned i=0;i<100&&!fs::exists(path);++i)Sleep(50);require(fs::exists(path),"recovered process did not finish");}
int wmain(int argc,wchar_t** argv){
    try{
        require(argc==3,"fresh test directory and case required");
        const auto root=fs::absolute(argv[1]);const std::wstring which=argv[2];
        const auto path=root/"sonic-display.ini",marker=root/"parent-started";
        _wputenv_s(L"KATANA_PORT_BACKGROUND_TEST",L"1");
        _wputenv_s(L"SARECOMP_DISPLAY_CONFIG",path.c_str());
        wchar_t module[32768]{};require(GetModuleFileNameW(nullptr,module,32768)>0,"module path");
        wchar_t token[100]{};const auto trial=GetEnvironmentVariableW(L"SARECOMP_DISPLAY_TRIAL",token,100);
        if(trial){
            // Stand in only for the pre-game process. The real watchdog, INI
            // transaction, child launch, event protocol and timeout are used.
            sonic::restart::recover(module);
            require(sonic::presentation::read_settings(path).width==800,"trial settings not installed");
            if(which==L"crash")return 27;
            const auto signal=[&](const wchar_t* suffix){
                const auto name=L"Local\\SARecomp-display-"+std::wstring(token)+L"-"+suffix;
                const auto event=OpenEventW(EVENT_MODIFY_STATE,FALSE,name.c_str());require(event!=nullptr,"trial event missing");
                require(SetEvent(event)!=FALSE,"trial signal");CloseHandle(event);
            };
            signal(L"ready");
            if(which==L"hang"){Sleep(60000);return 28;}
            signal(which==L"accept"?L"accepted":L"rejected");return 0;
        }
        if(fs::exists(marker)){
            sonic::restart::recover(module);
            require(sonic::presentation::read_settings(path).width==640,"rollback child has wrong settings");
            mark(root/"recovered");return 0;
        }
        require(!fs::exists(root),"test directory must be fresh");fs::create_directories(root);
        sonic::presentation::Settings original;original.width=640;original.height=480;original.setup_complete=true;
        sonic::presentation::save_settings(path,original);sonic::presentation::initialize(module);mark(marker);
        auto candidate=original;candidate.width=800;candidate.height=600;
        const auto rollback=fs::path(path.wstring()+L".display-rollback.ini");
        if(which==L"orphan"){
            sonic::presentation::save_settings(rollback,original);sonic::presentation::save_settings(path,candidate);sonic::restart::recover(module);
        }else if(which==L"start-failure"){
            bool failed=false;try{sonic::restart::launch(fs::path(module).parent_path()/"missing-test-child.exe",candidate,4);}catch(...){failed=true;}
            require(failed,"missing child succeeded");
        }else {
            require(which==L"accept"||which==L"reject"||which==L"crash"||which==L"hang","unknown case");
            require(sonic::restart::launch(module,candidate,4)==0,"watchdog failed");
            if(which!=L"accept")wait_marker(root/"recovered");
        }
        require(sonic::presentation::read_settings(path)==(which==L"accept"?candidate:original),"settings outcome");
        require(!fs::exists(rollback),"rollback transaction left behind");
        std::wcout<<L"SONIC_DISPLAY_RESTART_TEST_OK "<<which<<L"\n";return 0;
    }catch(const std::exception& e){std::cerr<<"SONIC_DISPLAY_RESTART_TEST_FAIL "<<e.what()<<'\n';return 1;}
}
