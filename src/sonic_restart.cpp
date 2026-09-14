#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include "sonic_restart.hpp"
#include "sonic_configuration_lock.hpp"
#include "sonic_menu.hpp"
#include "sonic_startup.hpp"
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <chrono>
#include <iostream>
#include <thread>
namespace sonic::restart {
namespace {
using namespace katana::runtime;
struct Handle {HANDLE value=nullptr;~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}Handle(const Handle&)=delete;explicit Handle(HANDLE v=nullptr):value(v){}};
std::wstring environment(const wchar_t* key){const auto size=GetEnvironmentVariableW(key,nullptr,0);if(!size)return {};std::wstring s(size,L'\0');GetEnvironmentVariableW(key,s.data(),size);s.resize(size-1);return s;}
std::wstring token(){auto value=environment(L"SARECOMP_DISPLAY_TRIAL");if(value.size()>80||value.find_first_not_of(L"0123456789-")!=value.npos)return {};return value;}
std::wstring event_name(std::wstring_view id,std::wstring_view suffix){return L"Local\\SARecomp-display-"+std::wstring(id)+L"-"+std::wstring(suffix);}
std::filesystem::path rollback_path(const std::filesystem::path& path){return std::filesystem::path(path.wstring()+L".display-rollback.ini");}
bool display_changed(const presentation::Settings& a,const presentation::Settings& b){auto same_display=b;same_display.active_profile=a.active_profile;same_display.gameplay_timing=a.gameplay_timing;return presentation::needs_restart(a,same_display);}
bool child_trial(){const auto id=token();if(id.empty())return false;Handle ready(OpenEventW(EVENT_MODIFY_STATE,FALSE,event_name(id,L"ready").c_str()));return ready.value!=nullptr;}
void start(const std::filesystem::path& executable,PROCESS_INFORMATION& process){
    std::wstring command=GetCommandLineW();STARTUPINFOW startup{sizeof(startup)};
    // The child inherits the existing hidden-test/window policy. No shell or
    // additional console; production game windows follow the selected mode.
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,executable.parent_path().c_str(),&startup,&process))
        throw std::runtime_error("settings-restart-process");
}
}
void recover(const std::filesystem::path& executable){
    if(child_trial())return;
    const auto path=presentation::configuration_path(executable);presentation::ConfigurationLock guard(path);
    const auto rollback=rollback_path(path);if(!std::filesystem::exists(rollback))return;
    const auto saved=presentation::read_settings(rollback);presentation::save_settings(path,saved);std::filesystem::remove(rollback);
    std::cerr<<"SONIC_DISPLAY recovered_interrupted_trial\n";
}
int launch(const std::filesystem::path& executable,const presentation::Settings& requested,int language,
           const presentation::Settings* edited_from){
    const auto path=presentation::configuration_path(executable);presentation::ConfigurationLock guard(path);
    const auto running=presentation::settings();
    const auto disk=presentation::read_settings(path);
    const auto next=presentation::merge_settings(edited_from?*edited_from:running,requested,disk);
    auto previous=disk;
    // Roll back to the display that actually worked, retaining unrelated edits
    // from the config tool and the already persisted profile/audio/input state.
    previous.width=running.width;previous.height=running.height;previous.render_percent=running.render_percent;
    previous.widescreen=running.widescreen;previous.renderer=running.renderer;
    previous.window_mode=running.window_mode;previous.vsync=running.vsync;
    const bool trial=display_changed(running,next);
    if(!trial){presentation::save_settings(path,next);PROCESS_INFORMATION process{};start(executable,process);CloseHandle(process.hThread);CloseHandle(process.hProcess);return 0;}
    const auto rollback=rollback_path(path);presentation::save_settings(rollback,previous);
    const auto id=std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    Handle ready(CreateEventW(nullptr,TRUE,FALSE,event_name(id,L"ready").c_str()));
    Handle accepted(CreateEventW(nullptr,TRUE,FALSE,event_name(id,L"accepted").c_str()));
    Handle rejected(CreateEventW(nullptr,TRUE,FALSE,event_name(id,L"rejected").c_str()));
    if(!ready.value||!accepted.value||!rejected.value)throw std::runtime_error("settings-trial-events");
    presentation::save_settings(path,next);PROCESS_INFORMATION process{};
    try {
        SetEnvironmentVariableW(L"SARECOMP_DISPLAY_TRIAL",id.c_str());SetEnvironmentVariableW(L"SARECOMP_DISPLAY_TRIAL_LANGUAGE",std::to_wstring(language).c_str());
        start(executable,process);
    }catch(...){SetEnvironmentVariableW(L"SARECOMP_DISPLAY_TRIAL",nullptr);SetEnvironmentVariableW(L"SARECOMP_DISPLAY_TRIAL_LANGUAGE",nullptr);presentation::save_settings(path,previous);std::filesystem::remove(rollback);throw;}
    SetEnvironmentVariableW(L"SARECOMP_DISPLAY_TRIAL",nullptr);SetEnvironmentVariableW(L"SARECOMP_DISPLAY_TRIAL_LANGUAGE",nullptr);
    Handle child(process.hProcess),thread(process.hThread);const HANDLE first[]{ready.value,child.value};
    auto result=WaitForMultipleObjects(2,first,FALSE,120000);bool keep=false;
    if(result==WAIT_OBJECT_0){const HANDLE second[]{accepted.value,rejected.value,child.value};result=WaitForMultipleObjects(3,second,FALSE,17000);keep=result==WAIT_OBJECT_0;}
    if(keep){std::filesystem::remove(rollback);std::cerr<<"SONIC_DISPLAY confirmed\n";return 0;}
    // This exact handle is our pre-game child. A renderer hang cannot strand
    // the user on an unusable mode, and no personal playtest is terminated.
    if(WaitForSingleObject(child.value,0)==WAIT_TIMEOUT){TerminateProcess(child.value,0x53415242);WaitForSingleObject(child.value,10000);}
    presentation::save_settings(path,previous);std::filesystem::remove(rollback);
    std::cerr<<"SONIC_DISPLAY automatic_rollback\n";
    PROCESS_INFORMATION recovered{};start(executable,recovered);CloseHandle(recovered.hThread);CloseHandle(recovered.hProcess);return 0;
}
bool confirm_display(NativePortDesktopHost& host,NativePortPlatformServices& platform){
    const auto id=token();if(id.empty())return true;
    Handle ready(OpenEventW(EVENT_MODIFY_STATE,FALSE,event_name(id,L"ready").c_str()));
    Handle accepted(OpenEventW(EVENT_MODIFY_STATE,FALSE,event_name(id,L"accepted").c_str()));
    Handle rejected(OpenEventW(EVENT_MODIFY_STATE,FALSE,event_name(id,L"rejected").c_str()));
    if(!ready.value||!accepted.value||!rejected.value)return false;
    int language=1;const auto value=environment(L"SARECOMP_DISPLAY_TRIAL_LANGUAGE");if(value.size()==1&&value[0]>=L'0'&&value[0]<=L'4')language=int(value[0]-L'0');
    SetEnvironmentVariableW(L"SARECOMP_DISPLAY_TRIAL",nullptr);SetEnvironmentVariableW(L"SARECOMP_DISPLAY_TRIAL_LANGUAGE",nullptr);
    menu::Model model(presentation::settings(),language,false);model.ask("keep_display",menu::Command::Save);
    input::set_modal(true);struct Release{~Release(){input::set_modal(false);}}release;
    sonic::startup::finish();const auto started=host.monotonic_time_nanoseconds();unsigned shown=999;bool signaled=false,selected=false;
    while(true){
        const auto elapsed=(host.monotonic_time_nanoseconds()-started)*1e-9;
        if(elapsed>=15||host.poll_lifecycle()==NativePortLifecycleState::Shutdown){SetEvent(rejected.value);return false;}
        const auto result=model.update(input::sample(input::poll_host(platform),true),elapsed);
        if(result.command==menu::Command::Save){SetEvent(accepted.value);return true;}
        if(!model.modal()){SetEvent(rejected.value);return false;}
        const unsigned remaining=unsigned(std::ceil(15-elapsed));
        if(shown!=remaining || selected!=model.confirm_selected()){
            model.countdown(remaining);const auto extent=host.graphics().layout().output_extent;
            auto pixels=menu::rasterize(model,extent.width,extent.height);NativePortImageView view;
            view.extent={pixels.width,pixels.height};view.format=NativePortTextureFormat::Rgba8Unorm;view.stride_bytes=pixels.width*4;view.pixels=pixels.pixels;
            host.graphics().present_image(view,NativePortViewportTarget::Ui,NativePortImageFit::Stretch);shown=remaining;selected=model.confirm_selected();
            if(!signaled){SetEvent(ready.value);signaled=true;}
        }else host.present_frame(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}
}
#else
#include "linux/sonic_restart_posix.inc"
#endif
