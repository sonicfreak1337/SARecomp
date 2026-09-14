#include "../sonic_startup.hpp"
#include "../sonic_user_paths.hpp"
#include "sonic_file_lock.hpp"
#include "startup_channel.hpp"
#include "katana/io/input_provenance.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <elf.h>
extern char** environ;

namespace sonic::startup {
namespace {
bool enabled(const char* name){const auto* value=std::getenv(name);return value&&std::string_view(value)=="1";}
struct Progress {
    std::mutex mutex;bool active=false;int channel=-1;pid_t child=-1;
    ProgressMessage value;std::chrono::steady_clock::time_point started,last_sent;
} progress;
void send_progress(bool force=false) noexcept {
    const auto now=std::chrono::steady_clock::now();
    if(progress.channel<0||(!force&&now-progress.last_sent<std::chrono::milliseconds(40)))return;
    progress.last_sent=now;
    ::send(progress.channel,&progress.value,sizeof(progress.value),MSG_DONTWAIT|MSG_NOSIGNAL);
}
std::filesystem::path cache_path(std::string_view domain,std::string_view key){
    if(key.size()!=64||!std::ranges::all_of(key,[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');})||
       domain.empty()||!std::ranges::all_of(domain,[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-';})||enabled("SARECOMP_DISABLE_STARTUP_CACHE"))return {};
    return paths::cache_root(paths::executable())/"startup-v1"/domain/(std::string(key)+".bin");
}
constexpr std::array<char,8> cache_magic{'S','A','R','C','C','0','0','1'};
}
Session::Session(){
    std::lock_guard guard(progress.mutex);
    if(progress.active)throw std::logic_error("startup-session-already-active");
    progress.active=true;progress.started=std::chrono::steady_clock::now();
    constexpr std::string_view label="Starting Sonic Adventure...";
    progress.value={};progress.value.label_size=label.size();std::copy(label.begin(),label.end(),progress.value.label.begin());
    if(enabled("KATANA_PORT_BACKGROUND_TEST"))return;
    int channel[2]{-1,-1};
    if(::socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0,channel)!=0)return;
    try{
        const auto helper=(paths::executable().parent_path()/"sonic-startup-ui").string();
        posix_spawn_file_actions_t actions;
        if(posix_spawn_file_actions_init(&actions)!=0){::close(channel[0]);::close(channel[1]);return;}
        posix_spawn_file_actions_adddup2(&actions,channel[1],3);
        if(channel[0]!=3)posix_spawn_file_actions_addclose(&actions,channel[0]);
        if(channel[1]!=3)posix_spawn_file_actions_addclose(&actions,channel[1]);
        char* argv[]{const_cast<char*>(helper.c_str()),nullptr};
        const auto result=::posix_spawn(&progress.child,helper.c_str(),&actions,nullptr,argv,environ);
        posix_spawn_file_actions_destroy(&actions);::close(channel[1]);channel[1]=-1;
        if(result){::close(channel[0]);progress.child=-1;std::fprintf(stderr,"SONIC_STARTUP progress_window_unavailable=%d\n",result);return;}
        progress.channel=channel[0];send_progress(true);
    }catch(...){if(channel[0]>=0)::close(channel[0]);if(channel[1]>=0)::close(channel[1]);}
}
Session::~Session(){
    finish();pid_t child;
    {std::lock_guard guard(progress.mutex);child=progress.child;progress.child=-1;}
    if(child>0){int status=0;while(::waitpid(child,&status,0)<0&&errno==EINTR){}}
}
void phase(std::string_view label,std::uint64_t completed,std::uint64_t total) noexcept {
    try{
        std::lock_guard guard(progress.mutex);if(!progress.active)return;
        const auto previous=std::string_view(progress.value.label.data(),progress.value.label_size);
        const bool changed=previous!=label;
        if(changed)std::fprintf(stderr,"SONIC_STARTUP phase=%.*s\n",int(std::min<std::size_t>(label.size(),256)),label.data());
        progress.value.label_size=std::min(label.size(),progress.value.label.size());
        std::copy_n(label.begin(),progress.value.label_size,progress.value.label.begin());
        progress.value.completed=completed;progress.value.total=total;send_progress(changed||completed==total);
    }catch(...){}
}
void advance() noexcept {std::lock_guard guard(progress.mutex);if(progress.active&&progress.value.completed<progress.value.total){++progress.value.completed;send_progress(progress.value.completed==progress.value.total);}}
void finish() noexcept {
    std::lock_guard guard(progress.mutex);if(!progress.active)return;progress.active=false;
    if(progress.channel>=0){::shutdown(progress.channel,SHUT_RDWR);::close(progress.channel);progress.channel=-1;}
    std::fprintf(stderr,"SONIC_STARTUP ready=1 elapsed_ms=%.3f\n",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-progress.started).count());
}
std::string digest(std::span<const std::byte> bytes){return katana::io::sha256_bytes({reinterpret_cast<const char*>(bytes.data()),bytes.size()});}
std::string file_digest(const std::filesystem::path& path) noexcept {
    try{
        const auto size=std::filesystem::file_size(path);if(size>32*1024*1024)return {};
        std::ifstream file(path,std::ios::binary);std::vector<std::byte> bytes(size);
        if(!file.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))return {};return digest(bytes);
    }catch(...){return {};}
}
std::vector<std::byte> cache_load(std::string_view domain,std::string_view key,std::size_t maximum) noexcept {
    try{
        const auto path=cache_path(domain,key);if(path.empty())return {};
        std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)return {};
        const auto size=file.tellg();if(size<80||std::uint64_t(size)-80>maximum)return {};file.seekg(0);
        std::array<char,8> magic{};std::uint64_t count=0;std::array<char,64> hash{};
        file.read(magic.data(),8);file.read(reinterpret_cast<char*>(&count),8);file.read(hash.data(),64);
        if(!file||magic!=cache_magic||count!=std::uint64_t(size)-80||count>maximum)return {};
        std::vector<std::byte> data(count);if(!file.read(reinterpret_cast<char*>(data.data()),data.size())||digest(data)!=std::string_view(hash.data(),64))return {};
        return data;
    }catch(...){return {};}
}
bool cache_save(std::string_view domain,std::string_view key,std::span<const std::byte> data) noexcept {
    std::filesystem::path temporary;
    try{
        const auto path=cache_path(domain,key);if(path.empty())return false;const auto hash=digest(data);if(hash.size()!=64)return false;
        std::filesystem::create_directories(path.parent_path());static std::atomic<unsigned> sequence=0;
        temporary=path;temporary+="."+std::to_string(::getpid())+"."+std::to_string(sequence++)+".tmp";
        const int fd=::open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return false;
        const auto write=[&](const void* bytes,std::size_t size){
            const auto* p=static_cast<const char*>(bytes);
            while(size){const auto n=::write(fd,p,size);if(n<0&&errno==EINTR)continue;if(n<=0)return false;p+=n;size-=n;}return true;
        };
        const std::uint64_t size=data.size();const bool ok=write(cache_magic.data(),8)&&write(&size,8)&&write(hash.data(),64)&&write(data.data(),data.size());
        ::close(fd);if(!ok)throw std::runtime_error("cache-write");
        linux_host::publish_file(temporary,path);return true;
    }catch(...){if(!temporary.empty()){std::error_code error;std::filesystem::remove(temporary,error);}return false;}
}
void prefetch_program() noexcept {
    if(enabled("SARECOMP_DISABLE_CODE_PREFETCH"))return;
    try{
        const auto path=paths::executable();const int fd=::open(path.c_str(),O_RDONLY|O_CLOEXEC);if(fd<0)return;
        struct Close{int fd;~Close(){::close(fd);}} close{fd};
        Elf64_Ehdr header{};if(::pread(fd,&header,sizeof(header),0)!=sizeof(header)||std::memcmp(header.e_ident,ELFMAG,SELFMAG)||header.e_ident[EI_CLASS]!=ELFCLASS64||header.e_phentsize!=sizeof(Elf64_Phdr)||header.e_phnum>256)return;
        const auto free_pages=::sysconf(_SC_AVPHYS_PAGES),page_size=::sysconf(_SC_PAGESIZE);if(free_pages<=0||page_size<=0)return;
        std::uint64_t budget=std::min<std::uint64_t>(2ull*1024*1024*1024,std::uint64_t(free_pages)*page_size/3),requested=0;
        phase("Loading game code...");
        for(unsigned i=0;i<header.e_phnum&&budget;++i){
            Elf64_Phdr segment{};if(::pread(fd,&segment,sizeof(segment),header.e_phoff+std::uint64_t(i)*sizeof(segment))!=sizeof(segment))break;
            if(segment.p_type!=PT_LOAD||!(segment.p_flags&PF_R)||(segment.p_flags&PF_W))continue;
            const auto bytes=std::min(segment.p_filesz,budget);if(bytes&&::posix_fadvise(fd,off_t(segment.p_offset),off_t(bytes),POSIX_FADV_WILLNEED)==0){requested+=bytes;budget-=bytes;}
        }
        std::fprintf(stderr,"SONIC_STARTUP_PREFETCH backend=linux advised_bytes=%llu mode=asynchronous\n",static_cast<unsigned long long>(requested));
    }catch(...){}
}
void frame(std::uint64_t index) noexcept {
    if(index>=2)finish();
    if(!enabled("SARECOMP_STARTUP_TRACE")||index>600)return;
    static auto previous=std::chrono::steady_clock::now();static long previous_faults=0;
    const auto now=std::chrono::steady_clock::now();const auto ms=std::chrono::duration<double,std::milli>(now-previous).count();previous=now;
    rusage usage{};::getrusage(RUSAGE_SELF,&usage);const auto faults=usage.ru_minflt+usage.ru_majflt;
    if(index%30==0||ms>80)std::fprintf(stderr,"SONIC_STARTUP_FRAME frame=%llu interval_ms=%.3f page_faults=%ld\n",static_cast<unsigned long long>(index),ms,faults-previous_faults);
    previous_faults=faults;
}
}
