#include "install_engine.hpp"
#include "supported_disc.hpp"
#include "install_identity.hpp"
#include "../sonic_configuration_lock.hpp"
#include "katana/io/input_provenance.hpp"
#include "katana/runtime/gdi.hpp"
#include "katana/runtime/iso9660.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <fstream>
#include <random>
#include <set>
#include <sstream>
#include <span>
#include <vector>
#include <stb_image.h>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

namespace sonic::setup {
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
void require(bool condition,const std::string& message){if(!condition)throw InstallError(message);}
void checkpoint(const std::atomic<bool>& cancel){require(!cancel.load(),"Installation was cancelled.");}
std::string digest(std::span<const std::uint8_t> bytes){return katana::io::sha256_bytes({reinterpret_cast<const char*>(bytes.data()),bytes.size()});}
Bytes read_bytes(const fs::path& path,std::uint64_t limit){
    std::ifstream in(path,std::ios::binary|std::ios::ate);require(bool(in),"A required setup resource is missing.");
    const auto size=in.tellg();require(size>=0&&std::uint64_t(size)<=limit,"A setup resource has an invalid size.");
    Bytes data(std::size_t(size),0);in.seekg(0);in.read(reinterpret_cast<char*>(data.data()),size);
    require(bool(in),"A setup resource could not be read.");return data;
}
bool safe_path(std::string_view value){
    if(value.empty()||value.size()>240||value.front()=='/'||value.back()=='/')return false;
    for(unsigned char c:value)if(!(c>='a'&&c<='z')&&!(c>='A'&&c<='Z')&&!(c>='0'&&c<='9')&&c!='/'&&c!='.'&&c!='_'&&c!='-')return false;
    for(const auto& part:fs::path(value))if(part=="."||part=="..")return false;
    return true;
}
struct File {std::string path,source,sha;std::uint64_t bytes;};
std::vector<File> recipe(const fs::path& resources){
    const auto bytes=read_bytes(resources/"install-files.tsv",2*1024*1024);
    require(digest(bytes)==install_recipe_sha256,"Setup resources are damaged. Extract the complete setup package again.");
    std::istringstream input(std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size()));
    std::string line;std::getline(input,line);require(line=="SARECOMP-INSTALL-1","This installation recipe is not supported.");
    std::vector<File> result;std::set<std::string> seen;
    while(std::getline(input,line)){
        std::istringstream row(line);File file;std::string size;
        require(bool(std::getline(row,file.path,'\t')&&std::getline(row,size,'\t')&&std::getline(row,file.sha,'\t')&&std::getline(row,file.source)),"The installation recipe is invalid.");
        const auto number=std::from_chars(size.data(),size.data()+size.size(),file.bytes);
        require(number.ec==std::errc{}&&number.ptr==size.data()+size.size()&&file.bytes<=256*1024*1024,"The installation recipe has an invalid file size.");
        require(safe_path(file.path)&&seen.insert(file.path).second&&file.sha.size()==64,"The installation recipe has an invalid filename.");
        require(safe_path(file.source)||file.source=="@IP"||file.source=="@RAM","The installation recipe has an invalid source.");
        result.push_back(std::move(file));require(result.size()<=8192,"The installation recipe is too large.");
    }
    require(result.size()>=3,"The installation recipe is incomplete.");return result;
}
void sync_file(const fs::path& path){
#ifndef _WIN32
    const int fd=::open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    require(fd>=0,"An installed file could not be committed to disk.");
    const int status=::fsync(fd);::close(fd);require(status==0,"An installed file could not be committed to disk.");
#endif
}
void write_bytes(const fs::path& path,std::span<const std::uint8_t> bytes){
    fs::create_directories(path.parent_path());
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));out.flush();
    require(bool(out),"Game files could not be written. Check free space and folder permissions.");out.close();
    require(bool(out),"Game files could not be closed successfully.");sync_file(path);
}
void publish(const fs::path& temporary,const fs::path& destination){
#ifdef _WIN32
    require(MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"The installation could not be activated.");
#else
    linux_host::publish_file(temporary,destination);
#endif
}
class VerifiedTrack final:public katana::runtime::DiscSource {
    mutable std::ifstream input_;
    std::uint64_t raw_size_;
    std::string identity_=std::string(required_tracks.back().sha256);
public:
    explicit VerifiedTrack(const fs::path& path):input_(path,std::ios::binary),raw_size_(required_tracks.back().bytes){require(bool(input_),"The game data track could not be opened.");}
    std::uint64_t size()const noexcept override{return (45000+raw_size_/2352)*2048;}
    const std::string& identity()const noexcept override{return identity_;}
    void read(std::uint64_t offset,std::span<std::uint8_t> output)const override{
        constexpr std::uint64_t origin=45000ull*2048;
        require(offset>=origin&&offset<=size()&&output.size()<=size()-offset,"A disc file points outside the game data track.");
        std::array<std::uint8_t,128*2352> raw{};
        while(!output.empty()){
            const auto sector=(offset-origin)/2048;auto skip=unsigned((offset-origin)%2048);
            const auto count=std::min<std::uint64_t>(128,(output.size()+skip+2047)/2048);
            input_.seekg(std::streamoff(sector*2352));input_.read(reinterpret_cast<char*>(raw.data()),std::streamsize(count*2352));
            require(bool(input_),"The game data track could not be read completely.");
            for(unsigned i=0;i<count;++i){
                require(raw[i*2352+15]==1,"The game data track uses an unsupported sector format.");
                const auto length=std::min<std::size_t>(2048-skip,output.size());
                std::copy_n(raw.begin()+i*2352+16+skip,length,output.begin());
                output=output.subspan(length);offset+=length;skip=0;
            }
        }
    }
};
struct Stage {
    fs::path root,path;
    bool keep=false;
    explicit Stage(const fs::path& directory):root(directory){
        std::random_device random;
        for(unsigned attempt=0;attempt<20;++attempt){
            path=root/(".content-install-"+std::to_string(random())+"-"+std::to_string(random()));
            if(fs::create_directory(path)){
                fs::permissions(path,fs::perms::owner_all,fs::perm_options::replace);return;
            }
        }
        throw InstallError("A temporary installation folder could not be created.");
    }
    ~Stage(){
        // Cleanup is restricted to this exact freshly created child. An active
        // installation or a user's save directory can never be the target.
        if(!keep&&path.parent_path()==root&&path.filename().string().starts_with(".content-install-")){
            std::error_code error;fs::remove_all(path,error);
        }
    }
};
}

InstallResult install(const fs::path& gdi,const fs::path& resources,const fs::path& user_root,
                      const InstallCallback& progress,const std::atomic<bool>& cancel){
    auto report=[&](unsigned percent,const std::string& message){checkpoint(cancel);if(progress)progress({percent,message});};
    report(0,"Checking setup resources...");const auto files=recipe(resources);
    const auto packed=read_bytes(resources/"bootstrap.xor.z",16*1024*1024);
    require(digest(packed)==bootstrap_delta_sha256,"The setup bootstrap resource is damaged. Extract the complete package again.");
    fs::create_directories(user_root);const auto root=fs::canonical(user_root);
    require(root.string().find_first_of("\r\n")==std::string::npos,"The installation folder name cannot contain line breaks.");
    presentation::ConfigurationLock guard(root/"katana-content-root.txt");
    std::uint64_t total=0;for(const auto& file:files)total+=file.bytes;
    require(fs::space(root).available>=total+64*1024*1024,"There is not enough free space to install the game. At least 1.1 GB is required.");
    katana::runtime::GdiDescriptor descriptor;
    try {descriptor=katana::runtime::parse_gdi_descriptor(gdi);}
    catch(const std::exception&){throw InstallError("The GDI or one of its track files could not be read. Keep the GDI and all tracks together in the same folder.");}
    require(descriptor.tracks.size()==required_tracks.size(),"This disc release is not supported. Select Sonic Adventure PAL v1.003 (1999).");
    std::uint64_t checked=0,track_total=0;for(const auto& t:required_tracks)track_total+=t.bytes;
    for(unsigned i=0;i<required_tracks.size();++i){
        const auto& expected=required_tracks[i];const auto& track=descriptor.tracks[i];
        require(track.number==expected.number&&track.lba==expected.lba&&unsigned(track.type)==expected.type&&track.sector_size==expected.sector_size&&track.file_offset==0&&track.sector_count*track.sector_size==expected.bytes,
                "This disc layout is not supported. Select Sonic Adventure PAL v1.003 (1999).");
        auto identity=katana::io::capture_input_provenance("setup-disc-track",track.resolved_path,[&]{checkpoint(cancel);},{},
            [&](std::uint64_t offset,std::string_view bytes){report(unsigned(30*(checked+offset+bytes.size())/track_total),"Verifying track "+std::to_string(i+1)+" of 3...");});
        require(identity.sha256==expected.sha256,"Track "+std::to_string(i+1)+" does not match the supported PAL v1.003 release. Check the required SHA-256 on the game files page.");
        checked+=expected.bytes;
    }
    report(30,"Installing game files...");
    auto disc=std::make_shared<VerifiedTrack>(descriptor.tracks.back().resolved_path);
    katana::runtime::Iso9660Filesystem iso(disc,2048,45000,0);
    Stage stage(root);Bytes boot,ip;InstallResult result;
    for(const auto& file:files){
        checkpoint(cancel);Bytes bytes;
        if(file.source=="@IP")bytes=static_cast<katana::runtime::DiscSource&>(*disc).read(45000ull*2048,32768);
        else if(file.source=="@RAM"){
            require(boot.size()==6735296&&ip.size()==32768,"The setup bootstrap inputs are incomplete.");
            bytes.resize(16*1024*1024,0);std::copy(boot.begin(),boot.end(),bytes.begin()+0x10000);std::copy(ip.begin(),ip.end(),bytes.begin()+0x8000);
            Bytes delta(bytes.size());
            const int decoded=stbi_zlib_decode_buffer(reinterpret_cast<char*>(delta.data()),int(delta.size()),reinterpret_cast<const char*>(packed.data()),int(packed.size()));
            require(decoded==int(delta.size()),"The setup bootstrap patch could not be read.");
            for(std::size_t i=0;i<bytes.size();++i)bytes[i]^=delta[i];
        }else bytes=iso.read_file(file.source);
        require(bytes.size()==file.bytes&&digest(bytes)==file.sha,"The extracted file failed verification: "+file.path+". No existing installation has been replaced.");
        write_bytes(stage.path/file.path,bytes);
        if(file.path=="boot.bin")boot=std::move(bytes);else if(file.path=="ip.bin")ip=std::move(bytes);
        ++result.files;result.bytes+=file.bytes;
        report(30+unsigned(68*result.bytes/total),"Installing game files ("+std::to_string(result.files)+" / "+std::to_string(files.size())+")...");
    }
    report(99,"Finishing installation...");
    for(const auto& entry:fs::recursive_directory_iterator(stage.path))if(entry.is_directory())sync_file(entry.path());
    sync_file(stage.path);
    const auto installed=root/("content-"+stage.path.filename().string().substr(17));
    require(!fs::exists(installed),"An installation folder already exists. Please try again.");
    checkpoint(cancel);fs::rename(stage.path,installed);stage.keep=true;sync_file(root);
    const auto pointer=root/"katana-content-root.txt",temporary=root/"katana-content-root.pending";
    const auto value=installed.string()+"\n";
    write_bytes(temporary,{reinterpret_cast<const std::uint8_t*>(value.data()),value.size()});
    publish(temporary,pointer);result.content=installed;
    if(progress)progress({100,"Installation complete."});return result;
}
}
