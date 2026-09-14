#pragma once
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace sonic::linux_host {
inline int open_directory(const std::filesystem::path& path){
    const auto absolute=std::filesystem::absolute(path).lexically_normal();int fd=::open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    for(const auto& part:absolute.relative_path()){
        if(fd<0)return -1;
        const int next=::openat(fd,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);::close(fd);fd=next;
    }
    return fd;
}
inline int open_regular(const std::filesystem::path& path,int flags,unsigned mode=0){
    const int directory=open_directory(path.parent_path());if(directory<0)return -1;
    const int fd=::openat(directory,path.filename().c_str(),flags|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK,mode);::close(directory);return fd;
}
// Recursion is shared per normalized path, including across editor helpers.
// The fd remains locked until the outermost guard releases it.
class FileLock {
    struct State {std::recursive_timed_mutex mutex;int fd=-1;unsigned depth=0;
        ~State(){if(fd>=0)::close(fd);}};
    std::shared_ptr<State> state_;
    std::unique_lock<std::recursive_timed_mutex> lock_;
public:
    explicit FileLock(const std::filesystem::path& path,unsigned timeout=2000) {
        auto name=std::filesystem::absolute(path).lexically_normal();name+=".lock";
        static std::mutex index_mutex;
        static std::unordered_map<std::string,std::weak_ptr<State>> index;
        {std::lock_guard guard(index_mutex);auto& weak=index[name.string()];
            state_=weak.lock();if(!state_){state_=std::make_shared<State>();weak=state_;}}
        lock_=std::unique_lock(state_->mutex,std::defer_lock);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout);
        if(!lock_.try_lock_until(deadline))throw std::runtime_error("configuration-in-use");
        if(state_->depth){++state_->depth;return;}
        std::filesystem::create_directories(name.parent_path());
        if(state_->fd<0)state_->fd=::open(name.c_str(),O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);
        if(state_->fd<0)throw std::runtime_error("configuration-lock-open");
        while(::flock(state_->fd,LOCK_EX|LOCK_NB)!=0) {
            if(errno!=EWOULDBLOCK && errno!=EAGAIN && errno!=EINTR)
                throw std::runtime_error("configuration-lock-failed");
            if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("configuration-in-use");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        ++state_->depth;
    }
    ~FileLock(){if(state_ && lock_.owns_lock() && state_->depth && !--state_->depth)::flock(state_->fd,LOCK_UN);}
    FileLock(const FileLock&)=delete;FileLock& operator=(const FileLock&)=delete;
};
inline void publish_file(const std::filesystem::path& temporary,const std::filesystem::path& final) {
    const int fd=::open(temporary.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)throw std::runtime_error("settings-publish-open");
    const auto synced=::fsync(fd);::close(fd);
    if(synced!=0)throw std::runtime_error("settings-publish-sync");
    std::filesystem::rename(temporary,final);
    const auto parent=final.parent_path().empty()?std::filesystem::path("."):final.parent_path();
    const int directory=::open(parent.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(directory<0)throw std::runtime_error("settings-directory-open");
    const auto durable=::fsync(directory);::close(directory);
    if(durable!=0)throw std::runtime_error("settings-directory-sync");
}
}
