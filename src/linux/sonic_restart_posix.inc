#include "../sonic_restart.hpp"
#include "../sonic_configuration_lock.hpp"
#include "../sonic_menu.hpp"
#include "../sonic_startup.hpp"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>
#include <spawn.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
namespace sonic::restart {
namespace {
using namespace katana::runtime;
struct Fd{int value=-1;~Fd(){if(value>=0)::close(value);}};
int trial_channel()noexcept{
    const auto* raw=std::getenv("SARECOMP_DISPLAY_TRIAL_FD");if(!raw||std::strcmp(raw,"3"))return -1;
    int kind=0;socklen_t size=sizeof(kind);
    return ::getsockopt(3,SOL_SOCKET,SO_TYPE,&kind,&size)==0&&kind==SOCK_SEQPACKET?3:-1;
}
bool child_trial(){return trial_channel()>=0;}
std::filesystem::path rollback_path(std::filesystem::path path){path+=".display-rollback.ini";return path;}
bool display_changed(const presentation::Settings& a,const presentation::Settings& b){auto value=b;value.active_profile=a.active_profile;value.gameplay_timing=a.gameplay_timing;return presentation::needs_restart(a,value);}
pid_t start(const std::filesystem::path& executable,int channel=-1,int language=1){
    std::ifstream command("/proc/self/cmdline",std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(command),{}};
    std::vector<std::string> arguments{executable.string()};
    for(auto offset=bytes.find('\0');offset!=bytes.npos&&offset+1<bytes.size();){const auto end=bytes.find('\0',offset+1);if(end==bytes.npos)break;arguments.push_back(bytes.substr(offset+1,end-offset-1));offset=end;}
    std::vector<char*> argv;for(auto& s:arguments)argv.push_back(s.data());argv.push_back(nullptr);
    std::vector<std::string> environment;for(auto e=environ;*e;++e)if(!std::string_view(*e).starts_with("SARECOMP_DISPLAY_TRIAL"))environment.emplace_back(*e);
    if(channel>=0){environment.emplace_back("SARECOMP_DISPLAY_TRIAL_FD=3");environment.emplace_back("SARECOMP_DISPLAY_TRIAL_LANGUAGE="+std::to_string(language));}
    std::vector<char*> env;for(auto& s:environment)env.push_back(s.data());env.push_back(nullptr);
    posix_spawn_file_actions_t actions;::posix_spawn_file_actions_init(&actions);
    struct Destroy{posix_spawn_file_actions_t* value;~Destroy(){::posix_spawn_file_actions_destroy(value);}}destroy{&actions};
    if(channel>=0&&::posix_spawn_file_actions_adddup2(&actions,channel,3))throw std::runtime_error("settings-trial-channel");
    if(channel>3)::posix_spawn_file_actions_addclose(&actions,channel);
    pid_t child=0;const int result=::posix_spawn(&child,executable.c_str(),&actions,nullptr,argv.data(),env.data());
    if(result)throw std::runtime_error("settings-restart-process");return child;
}
void signal(int fd,char value)noexcept{if(fd>=0)::send(fd,&value,1,MSG_NOSIGNAL|MSG_DONTWAIT);}
}
void recover(const std::filesystem::path& executable){
    if(child_trial())return;
    const auto path=presentation::configuration_path(executable);presentation::ConfigurationLock guard(path);
    const auto rollback=rollback_path(path);if(!std::filesystem::exists(rollback))return;
    presentation::save_settings(path,presentation::read_settings(rollback));std::filesystem::remove(rollback);
    std::cerr<<"SONIC_DISPLAY recovered_interrupted_trial\n";
}
int launch(const std::filesystem::path& executable,const presentation::Settings& requested,int language,const presentation::Settings* edited_from){
    const auto path=presentation::configuration_path(executable);presentation::ConfigurationLock guard(path);
    const auto running=presentation::settings(),disk=presentation::read_settings(path);
    const auto next=presentation::merge_settings(edited_from?*edited_from:running,requested,disk);auto previous=disk;
    previous.width=running.width;previous.height=running.height;previous.render_percent=running.render_percent;
    previous.widescreen=running.widescreen;previous.renderer=running.renderer;previous.window_mode=running.window_mode;previous.vsync=running.vsync;
    if(!display_changed(running,next)){presentation::save_settings(path,next);start(executable);return 0;}
    int sockets[2];if(::socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0,sockets))throw std::runtime_error("settings-trial-socket");
    Fd parent{sockets[0]},child_socket{sockets[1]};const auto rollback=rollback_path(path);presentation::save_settings(rollback,previous);
    pid_t child=0;
    try{presentation::save_settings(path,next);child=start(executable,child_socket.value,language);}
    catch(...){presentation::save_settings(path,previous);std::filesystem::remove(rollback);throw;}
    ::close(child_socket.value);child_socket.value=-1;
    bool accepted=false,exited=false;auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(120);
    while(std::chrono::steady_clock::now()<deadline){
        pollfd ready{parent.value,POLLIN,0};const auto result=::poll(&ready,1,50);
        if(result<0&&errno!=EINTR)break;
        if(ready.revents&POLLIN){char command=0;const auto n=::recv(parent.value,&command,1,0);if(n<=0)break;
            if(command=='R')deadline=std::chrono::steady_clock::now()+std::chrono::seconds(17);
            else if(command=='Y'){accepted=true;break;}else if(command=='N')break;
        }
        int status=0;if(::waitpid(child,&status,WNOHANG)==child){exited=true;break;}
        if(ready.revents&(POLLHUP|POLLERR))break;
    }
    if(accepted){std::filesystem::remove(rollback);std::cerr<<"SONIC_DISPLAY confirmed\n";return 0;}
    // Only the exact pre-game child we spawned is stopped. Never a playtest.
    if(!exited){::kill(child,SIGKILL);while(::waitpid(child,nullptr,0)<0&&errno==EINTR){}}
    presentation::save_settings(path,previous);std::filesystem::remove(rollback);start(executable);
    std::cerr<<"SONIC_DISPLAY automatic_rollback\n";return 0;
}
bool confirm_display(NativePortDesktopHost& host,NativePortPlatformServices& platform){
    const int channel=trial_channel();if(channel<0)return true;Fd owned{channel};int language=1;
    if(const auto* value=std::getenv("SARECOMP_DISPLAY_TRIAL_LANGUAGE");value&&value[0]>='0'&&value[0]<='4'&&!value[1])language=value[0]-'0';
    ::unsetenv("SARECOMP_DISPLAY_TRIAL_FD");::unsetenv("SARECOMP_DISPLAY_TRIAL_LANGUAGE");
    menu::Model model(presentation::settings(),language,false);model.ask("keep_display",menu::Command::Save);
    input::set_modal(true);struct Release{~Release(){input::set_modal(false);}}release;
    startup::finish();const auto started=host.monotonic_time_nanoseconds();unsigned shown=999;bool signaled=false,selected=false;
    while(true){
        const auto elapsed=(host.monotonic_time_nanoseconds()-started)*1e-9;
        if(elapsed>=15||host.poll_lifecycle()==NativePortLifecycleState::Shutdown){signal(channel,'N');return false;}
        const auto result=model.update(input::sample(input::poll_host(platform),true),elapsed);
        if(result.command==menu::Command::Save){signal(channel,'Y');return true;}
        if(!model.modal()){signal(channel,'N');return false;}
        const unsigned remaining=unsigned(std::ceil(15-elapsed));
        if(shown!=remaining||selected!=model.confirm_selected()){
            model.countdown(remaining);const auto extent=host.graphics().layout().output_extent;auto image=menu::rasterize(model,extent.width,extent.height);
            NativePortImageView view;view.extent={image.width,image.height};view.format=NativePortTextureFormat::Rgba8Unorm;view.stride_bytes=image.width*4;view.pixels=image.pixels;
            host.graphics().present_image(view,NativePortViewportTarget::Ui,NativePortImageFit::Stretch);shown=remaining;selected=model.confirm_selected();
            if(!signaled){signal(channel,'R');signaled=true;}
        }else host.present_frame(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}
}
