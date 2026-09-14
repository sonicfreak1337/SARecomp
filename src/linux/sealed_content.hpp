#pragma once
#include "katana/io/input_provenance.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace sonic::linux_content {
enum class Failure { Boundary, Read, Identity };
struct Error : std::runtime_error {
    Failure failure;int code;
    Error(Failure value,const char* operation):std::runtime_error(operation),failure(value),code(errno){}
};
class File final {
    int descriptor_=-1;
public:
    explicit File(int descriptor=-1):descriptor_(descriptor){}
    ~File(){if(descriptor_>=0)::close(descriptor_);}
    File(const File&)=delete;
    File& operator=(const File&)=delete;
    File(File&& other)noexcept:descriptor_(std::exchange(other.descriptor_,-1)){}
    File& operator=(File&& other)noexcept{
        if(this!=&other){if(descriptor_>=0)::close(descriptor_);descriptor_=std::exchange(other.descriptor_,-1);}return *this;
    }
    int get()const noexcept{return descriptor_;}
};
inline void require(bool okay,Failure failure,const char* operation){if(!okay)throw Error(failure,operation);}

// Advisory locks cannot prevent another Linux process writing a source file.
// Decode only a SHA-verified, kernel-sealed snapshot of that file. Component
// handles prevent a symlink replacement between path validation and open.
class SealedContent final {
    File immutable_;
    const std::byte* data_=nullptr;
    std::uint64_t size_=0;
public:
    SealedContent(const std::filesystem::path& root,const std::filesystem::path& relative,std::string_view identity){
        require(root.is_absolute()&&!relative.empty()&&!relative.is_absolute()&&!relative.has_root_path(),Failure::Boundary,"movie-content-path");
        File directory(::open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC));
        require(directory.get()>=0,Failure::Boundary,"movie-content-root");
        const auto descend=[&](const std::filesystem::path& components){
            for(const auto& part:components){
                require(!part.empty()&&part!="."&&part!="..",Failure::Boundary,"movie-content-component");
                File next(::openat(directory.get(),part.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
                require(next.get()>=0,Failure::Boundary,"movie-content-directory");directory=std::move(next);
            }
        };
        descend(root.relative_path());descend(relative.parent_path());
        require(relative.filename()!="."&&relative.filename()!="..",Failure::Boundary,"movie-content-name");
        File source(::openat(directory.get(),relative.filename().c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK));
        require(source.get()>=0,Failure::Boundary,"movie-content-open");
        struct stat info{};
        require(::fstat(source.get(),&info)==0&&S_ISREG(info.st_mode)&&info.st_size>0&&std::uint64_t(info.st_size)<=16ull*1024*1024*1024,Failure::Boundary,"movie-content-size");
        size_=std::uint64_t(info.st_size);
        immutable_=File(::memfd_create("sarecomp-movie",MFD_CLOEXEC|MFD_ALLOW_SEALING));
        require(immutable_.get()>=0,Failure::Read,"movie-content-snapshot");
        std::array<char,64*1024> buffer{};katana::io::Sha256Accumulator digest;
        for(std::uint64_t offset=0;offset<size_;){
            const auto wanted=std::size_t(std::min<std::uint64_t>(buffer.size(),size_-offset));
            const auto count=::pread(source.get(),buffer.data(),wanted,off_t(offset));
            if(count<0&&errno==EINTR)continue;
            require(count>0,Failure::Read,"movie-content-read");
            digest.update({buffer.data(),std::size_t(count)});
            for(std::size_t done=0;done<std::size_t(count);){
                const auto written=::pwrite(immutable_.get(),buffer.data()+done,std::size_t(count)-done,off_t(offset+done));
                if(written<0&&errno==EINTR)continue;
                require(written>0,Failure::Read,"movie-content-copy");done+=std::size_t(written);
            }
            offset+=std::uint64_t(count);
        }
        require("sha256:"+digest.finish()==identity,Failure::Identity,"movie-content-identity");
        require(::fcntl(immutable_.get(),F_ADD_SEALS,F_SEAL_WRITE|F_SEAL_SHRINK|F_SEAL_GROW|F_SEAL_SEAL)==0,Failure::Read,"movie-content-seal");
        const auto mapped=::mmap(nullptr,std::size_t(size_),PROT_READ,MAP_SHARED,immutable_.get(),0);
        require(mapped!=MAP_FAILED,Failure::Read,"movie-content-map");data_=static_cast<const std::byte*>(mapped);
    }
    ~SealedContent(){if(data_)::munmap(const_cast<std::byte*>(data_),std::size_t(size_));}
    SealedContent(const SealedContent&)=delete;
    SealedContent& operator=(const SealedContent&)=delete;
    std::span<const std::byte> bytes()const noexcept{return {data_,std::size_t(size_)};}
};
}
