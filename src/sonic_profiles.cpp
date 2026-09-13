#define NOMINMAX
#include <windows.h>
#include "sonic_profiles.hpp"
#include "sonic_presentation.hpp"
#include "sonic_startup.hpp"
#include "sonic_diagnostics.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
namespace sonic::profiles {
namespace {
namespace fs=std::filesystem;
constexpr std::string_view project="sonic-adventure-pal-v1003",slot="sonic-adventure-pal-v1003-vmu.save-c0-s0";
constexpr std::size_t limit=1024*1024;
fs::path base_path,library;
std::optional<std::vector<std::byte>> pending;
std::string pending_profile;
std::uint64_t next_poll=0;
fs::file_time_type previous_write{};
std::optional<std::pair<fs::file_time_type,std::uintmax_t>> verified_write;
std::string last_digest;
BackupStatus backup_state;
void backup_failed() noexcept {
    backup_state.state=recovery::BackupState::Failed;
    ++backup_state.failures;
}
struct Handle {HANDLE value=INVALID_HANDLE_VALUE;~Handle(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
void require(bool b){if(!b)throw std::runtime_error("save-profile-validation");}
bool identifier(std::string_view s){return !s.empty()&&s.size()<=64&&s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")==s.npos;}
bool save_id(std::string_view s){return !s.empty()&&s.size()<=64&&s.front()!='.'&&s.back()!='.'&&s.find("..")==s.npos&&s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-")==s.npos;}
bool save_text(std::string_view s,std::size_t maximum){return s.size()<=maximum&&std::all_of(s.begin(),s.end(),[](unsigned char c){return c==9||(c>=32&&c!=127);});}
struct IncompatibleSchema : std::runtime_error {IncompatibleSchema():std::runtime_error("save-incompatible-schema") {}};
std::uint64_t envelope(std::span<const std::byte> bytes);
fs::path checked(fs::path p){
    p=fs::absolute(p).lexically_normal();
    auto relative=p.lexically_relative(base_path);require(!relative.empty()&&*relative.begin()!=L"..");
    for(auto walk=p;!walk.empty();walk=walk.parent_path()){
        const DWORD a=GetFileAttributesW(walk.c_str());if(a!=INVALID_FILE_ATTRIBUTES)require(!(a&FILE_ATTRIBUTE_REPARSE_POINT));
        if(walk==base_path)break;
    }
    return p;
}
std::vector<std::byte> read(const fs::path& path,bool external=false){
    if(!external)checked(path);
    Handle h{CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr)};
    require(h.value!=INVALID_HANDLE_VALUE);FILE_ATTRIBUTE_TAG_INFO attributes{};
    require(GetFileInformationByHandleEx(h.value,FileAttributeTagInfo,&attributes,sizeof(attributes))&&!(attributes.FileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)));
    LARGE_INTEGER size{};require(GetFileSizeEx(h.value,&size)&&size.QuadPart>=0&&size.QuadPart<=limit);
    std::vector<std::byte> result(std::size_t(size.QuadPart));DWORD n=0;
    require(ReadFile(h.value,result.data(),DWORD(result.size()),&n,nullptr)&&n==result.size());return result;
}
std::string unique_name(std::string_view prefix){
    SYSTEMTIME t{};GetSystemTime(&t);char value[96]{};
    std::snprintf(value,sizeof(value),"%.*s%04u%02u%02u-%02u%02u%02u-%03u-%lu",int(prefix.size()),prefix.data(),t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,GetCurrentProcessId());
    static unsigned sequence=0;return std::string(value)+"-"+std::to_string(++sequence);
}
void atomic_write(const fs::path& path,std::span<const std::byte> bytes,bool replace=false){
    checked(path);fs::create_directories(path.parent_path());checked(path);
    const auto temporary=path.parent_path()/(unique_name("pending-")+".tmp");
    try {
        {Handle h{CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH|FILE_FLAG_OPEN_REPARSE_POINT,nullptr)};
         require(h.value!=INVALID_HANDLE_VALUE);DWORD n=0;require(WriteFile(h.value,bytes.data(),DWORD(bytes.size()),&n,nullptr)&&n==bytes.size());require(FlushFileBuffers(h.value));}
        require(MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH|(replace?MOVEFILE_REPLACE_EXISTING:0)));
    }catch(...){std::error_code e;fs::remove(checked(temporary),e);throw;}
}
fs::path primary(std::string_view profile){return data_root(profile)/project/"saves"/(std::string(slot)+".ksave");}
fs::path backup_root(){return library/"backups"/presentation::settings().active_profile;}
std::vector<std::byte> current_bytes(std::string_view profile){
    const auto path=primary(profile);std::vector<std::byte> bytes;
    try {bytes=read(path);envelope(bytes);}
    catch(const IncompatibleSchema&){throw;}
    catch(...){bytes=read(fs::path(path.wstring()+L".bak"));envelope(bytes);}
    // A valid envelope with an invalid volume must not revive an older save.
    inspect(bytes);return bytes;
}
std::vector<std::byte> current_bytes(){return current_bytes(presentation::settings().active_profile);}
template<class T> T little(std::span<const std::byte> s,std::size_t off){require(off<=s.size()&&sizeof(T)<=s.size()-off);T result=0;for(unsigned i=0;i<sizeof(T);++i)result|=T(std::to_integer<unsigned char>(s[off+i]))<<(8*i);return result;}
template<class T>void append(std::vector<std::byte>& b,T value){for(unsigned i=0;i<sizeof(T);++i)b.push_back(std::byte(value>>(8*i)));}
void string32(std::vector<std::byte>& b,std::string_view v){append(b,std::uint32_t(v.size()));for(char c:v)b.push_back(std::byte(c));}
struct Reader {
    std::span<const std::byte> bytes;std::size_t offset=0;
    template<class T>T next(){const auto value=little<T>(bytes,offset);offset+=sizeof(T);return value;}
    std::string string(){const auto count=next<std::uint16_t>();require(count<=1024 && offset<=bytes.size() && count<=bytes.size()-offset);std::string s(reinterpret_cast<const char*>(bytes.data()+offset),count);offset+=count;return s;}
    void skip(std::uint64_t n){require(offset<=bytes.size()&&n<=bytes.size()-offset);offset+=std::size_t(n);}
};
std::uint64_t envelope(std::span<const std::byte> bytes){
    constexpr unsigned char magic[]{'K','A','T','A','N','A','S','A','V','E',13,10,26,10,0,1};
    require(bytes.size()>=80&&bytes.size()<=limit);
    for(unsigned i=0;i<16;++i)require(std::to_integer<unsigned char>(bytes[i])==magic[i]);
    require(little<std::uint32_t>(bytes,16)==2&&little<std::uint32_t>(bytes,20)==80&&little<std::uint32_t>(bytes,28)==0);
    const auto schema=little<std::uint32_t>(bytes,24);const auto generation=little<std::uint64_t>(bytes,32);
    require(schema&&generation&&little<std::uint64_t>(bytes,40)==bytes.size()-80);
    std::vector<std::byte> material;string32(material,"katana-native-save-v2");string32(material,project);string32(material,slot);
    append(material,schema);append(material,generation);append(material,std::uint64_t(bytes.size()-80));material.insert(material.end(),bytes.begin()+80,bytes.end());
    constexpr char hex[]="0123456789abcdef";std::string stored;for(unsigned i=48;i<80;++i){auto v=std::to_integer<unsigned char>(bytes[i]);stored+=hex[v>>4];stored+=hex[v&15];}
    require(stored==startup::digest(material));if(schema!=1)throw IncompatibleSchema();return generation;
}
}
void initialize(const fs::path& base){
    base_path=fs::absolute(base).lexically_normal();fs::create_directories(base_path);checked(base_path);
    library=base_path/"profile-library";for(const auto& child:{"backups","imports","exports","diagnostics"})fs::create_directories(checked(library/child));
    fs::create_directories(checked(base_path/"profiles"));
    next_poll=0;previous_write={};verified_write.reset();last_digest.clear();backup_state={};
}
BackupStatus backup_status() noexcept {return backup_state;}
const fs::path& library_root(){return library;}
fs::path data_root(std::string_view id){require(identifier(id));return checked(id=="default"?base_path:base_path/"profiles"/id);}
std::vector<std::string> list(){
    std::vector<std::string> result{"default"};const auto root=checked(base_path/"profiles");
    if(fs::exists(root))for(const auto& item:fs::directory_iterator(root))if(item.is_directory()&&identifier(item.path().filename().string())){checked(item.path());result.push_back(item.path().filename().string());}
    std::sort(result.begin()+1,result.end());return result;
}
std::string create(){for(unsigned i=1;i<10000;++i){const auto id="profile-"+std::to_string(i);const auto p=data_root(id);if(fs::create_directory(p))return id;}throw std::runtime_error("profile-capacity");}
Preview inspect(std::span<const std::byte> bytes){
    const auto generation=envelope(bytes);require(generation!=UINT64_MAX);
    auto payload=bytes.subspan(80);require(payload.size()>=20);
    constexpr unsigned char volume[]{'K','N','S','V','O','L',1,0};for(unsigned i=0;i<8;++i)require(std::to_integer<unsigned char>(payload[i])==volume[i]);
    Reader reader{payload,8};require(reader.next<std::uint32_t>()==1&&reader.next<std::uint32_t>()==512&&reader.next<std::uint32_t>()==200);
    require(reader.string()=="sonic-adventure-pal-v1003:73dd6546704fb1ac491fc44672442610d40ddf9bf34876a596c8c4f0739410e2");
    require(reader.string()=="sonic-adventure-pal-v1003-vmu-a1");const auto count=reader.next<std::uint32_t>();require(count<=128);
    Preview result;result.bytes=bytes.size();result.generation=generation;result.digest=startup::digest(bytes);std::string previous;unsigned total=0;
    for(unsigned i=0;i<count;++i){
        const auto id=reader.string();require(save_id(id)&&(previous.empty()||previous<id));previous=id;result.files.push_back(id);
        require(save_id(reader.string()));require(save_text(reader.string(),128));require(save_text(reader.string(),256));
        reader.next<std::uint32_t>();const auto blocks=reader.next<std::uint32_t>();
        const auto length=reader.next<std::uint64_t>();require(blocks<=200&&length<=blocks*512ull&&(length+511)/512==blocks);total+=blocks;require(total<=200);reader.skip(length);
    }
    require(reader.offset==payload.size());return result;
}
Preview active_preview(){return inspect(current_bytes());}
std::optional<Preview> profile_preview(std::string_view id){
    const auto file=primary(id);if(!fs::exists(file)&&!fs::exists(fs::path(file.wstring()+L".bak")))return {};
    return inspect(current_bytes(id));
}
std::vector<Profile> catalog(){
    std::vector<Profile> result;
    for(const auto& id:list()){
        Profile entry{id};
        try{entry.preview=profile_preview(id);}catch(const std::exception&){entry.available=false;}
        result.push_back(std::move(entry));
    }
    return result;
}
Preview import_candidate(const fs::path& file){
    const auto bytes=read(fs::absolute(file),true);auto info=inspect(bytes);
    info.id=unique_name("import-")+".sasave";atomic_write(checked(library/"imports"/info.id),bytes);return info;
}
std::vector<Preview> snapshots(bool imports){
    const auto root=checked(imports?library/"imports":backup_root());std::vector<Preview> result;
    if(fs::exists(root))for(const auto& item:fs::directory_iterator(root)){
        if(!item.is_regular_file()||item.path().extension()!=L".sasave")continue;
        try{auto preview=inspect(read(item.path()));preview.id=item.path().filename().string();result.push_back(std::move(preview));}catch(...){}
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.id>b.id;});return result;
}
namespace {
void prune_backups() noexcept {
    try {
        const auto entries=snapshots();unsigned retained=0;
        for(const auto& e:entries)if(e.id.starts_with("auto-")&&++retained>presentation::settings().backup_limit)
            fs::remove(checked(backup_root()/e.id));
        backup_state.state=recovery::BackupState::Saved;
    }catch(...){
        // Publication already succeeded. Retain that fact even if an older
        // version is locked; retry cleanup without creating duplicate copies.
        backup_state.state=recovery::BackupState::CleanupPending;
        ++backup_state.cleanup_failures;
    }
}
}
fs::path backup(bool automatic){
    fs::path path;
    try {
        const auto bytes=current_bytes();auto preview=inspect(bytes);
        path=checked(backup_root()/(unique_name(automatic?"auto-":"manual-")+".sasave"));
        atomic_write(path,bytes);last_digest=std::move(preview.digest);
        backup_state.state=recovery::BackupState::Saved;++backup_state.saved;
    }catch(...){backup_failed();throw;}
    if(automatic)prune_backups();
    return path;
}
fs::path export_save(std::string_view expected){const auto bytes=current_bytes();const auto info=inspect(bytes);require(expected.empty()||info.digest==expected);const auto path=checked(library/"exports"/(unique_name("sonic-")+".sasave"));atomic_write(path,bytes);return path;}
void poll(std::uint64_t now) noexcept {
    if(!presentation::settings().automatic_backups||now<next_poll)return;next_poll=now+2'000'000'000ull;
    bool attempted_backup=false;
    try {
        if(backup_state.state==recovery::BackupState::CleanupPending)prune_backups();
        const auto path=primary(presentation::settings().active_profile);if(!fs::exists(path))return;
        const auto stamp=fs::last_write_time(path);if(stamp!=previous_write){previous_write=stamp;return;}
        const auto fingerprint=std::pair{stamp,fs::file_size(path)};if(verified_write==fingerprint)return;
        const auto bytes=current_bytes();const auto hash=startup::digest(bytes);
        if(hash!=last_digest){attempted_backup=true;backup(true);}
        if(fs::last_write_time(path)==fingerprint.first&&fs::file_size(path)==fingerprint.second)verified_write=fingerprint;
    }catch(...){if(!attempted_backup)backup_failed();} // Surface failure without crashing gameplay.
}
void stage_restore(std::string_view id,bool imported,std::string_view expected){
    require(id.size()<160&&id.ends_with(".sasave")&&id.find_first_of("/\\:")==id.npos);
    auto bytes=read(checked((imported?library/"imports":backup_root())/id));const auto info=inspect(bytes);require(expected.empty()||info.digest==expected);
    pending=std::move(bytes);pending_profile=presentation::settings().active_profile;
}
void apply_pending_restore(){
    if(!pending)return;inspect(*pending);const auto path=primary(pending_profile);
    if(fs::exists(path)||fs::exists(fs::path(path.wstring()+L".bak"))) {
        const auto before=current_bytes(pending_profile);
        atomic_write(checked(library/"backups"/pending_profile/(unique_name("before-restore-")+".sasave")),before);
    }
    // Replace exactly one complete semantic VMU; story and Chao cannot split.
    atomic_write(path,*pending,true);auto committed=std::move(*pending);pending.reset();last_digest.clear();previous_write={};verified_write.reset();
    backup_state.state=recovery::BackupState::Waiting;next_poll=0;
    // Primary publication is the commit point. Never retry that transaction
    // because a redundant recovery copy could not be refreshed.
    try{atomic_write(fs::path(path.wstring()+L".bak"),committed,true);}catch(...){OutputDebugStringW(L"SARecomp: restored primary; redundant VMU backup repair deferred.\n");}
}
fs::path export_diagnostics(){
    const auto& s=presentation::settings();std::ostringstream out;
    out<<"Sonic Adventure Recompiled diagnostic report\nFormat: 1\nRenderer: "<<(s.renderer==rendering::Renderer::Vulkan?"Vulkan":"D3D11")
       <<"\nResolution: "<<s.width<<'x'<<s.height<<"\nRender scale: "<<s.render_percent<<"\nOutput FPS: "<<s.presentation_fps<<"\nText language: "<<s.text_language<<'\n';
    out<<"Version: 0.49.9 experimental\nAdapter: "<<diagnostics::source_identity<<"\nBuild profile: "<<diagnostics::build_profile
       <<"\nFailure: "<<diagnostics::kind()<<"\nFailure code: "<<diagnostics::code.load()<<"\nLast completed input frame: "<<diagnostics::frame.load()<<'\n';
    const auto audio=recovery::audio();const auto backups=backup_status();
    out<<"Audio output: "<<recovery::name(audio.state)<<"\nAudio streams connected: "<<audio.connected
       <<"\nAudio streams retrying: "<<audio.silent<<"\nAudio streams requiring restart: "<<audio.restart_required
       <<"\nAudio endpoint faults: "<<audio.faults<<"\nAudio endpoint reconnections: "<<audio.reconnections
       <<"\nAudio last error code: "<<audio.last_error
       <<"\nBackup state: "<<recovery::name(backups.state)<<"\nBackups saved this session: "<<backups.saved
       <<"\nBackup failures: "<<backups.failures<<"\nBackup cleanup failures: "<<backups.cleanup_failures<<'\n';
#define SONIC_SETTING(name,initial,minimum,maximum) out<<#name<<": "<<s.name<<'\n';
#include "sonic_settings_fields.inc"
#undef SONIC_SETTING
    out<<"No save files, memory dump, user names, machine identifiers or personal paths included.\n";
    const auto content=out.str();const auto path=checked(library/"diagnostics"/(unique_name("diagnostic-")+".txt"));
    atomic_write(path,std::as_bytes(std::span(content.data(),content.size())));return path;
}
}
