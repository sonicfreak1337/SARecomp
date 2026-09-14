#include "install_runtime.hpp"
#include "../sonic_user_paths.hpp"
#include "../sonic_configuration_lock.hpp"
#include "katana/io/input_provenance.hpp"
#include <charconv>
#include <fstream>
#include <random>
#include <set>
#include <sstream>
#include <algorithm>
#include <vector>
#ifdef _WIN32
#include <shlobj.h>
#include <shobjidl.h>
#else
#include <unistd.h>
#endif

namespace sonic::setup {
namespace {
namespace fs=std::filesystem;
void check(bool ok,const char* message){if(!ok)throw InstallError(message);}
void cancel_check(const std::atomic<bool>& cancel){check(!cancel.load(),"Installation was cancelled.");}
std::string text(const fs::path& file){std::ifstream input(file,std::ios::binary);check(bool(input),"The setup payload is missing.");std::ostringstream data;data<<input.rdbuf();check(bool(input),"The setup payload could not be read.");return data.str();}
void write(const fs::path& file,std::string_view value){fs::create_directories(file.parent_path());std::ofstream out(file,std::ios::binary|std::ios::trunc);out.write(value.data(),value.size());out.flush();check(bool(out),"An application shortcut could not be written.");}
bool safe(std::string_view name){
    if(name.empty()||name.size()>240||name.front()=='/'||name.back()=='/')return false;
    for(const char c:name)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c=='-'||c=='_'||c=='.'))return false;
    for(const auto& part:fs::path(name))if(part==".."||part==".")return false;
    return true;
}
void verify(const fs::path& file,std::uint64_t size,std::string_view sha,const std::atomic<bool>& cancel){
    check(fs::is_regular_file(fs::symlink_status(file))&&fs::file_size(file)==size,"An application file is missing or damaged.");
    const auto identity=katana::io::capture_input_provenance("setup-payload",file,[&]{cancel_check(cancel);});
    check(identity.sha256==sha,"An application file failed verification. Extract the complete setup package again.");
}
#ifndef _WIN32
std::string exec_quote(const fs::path& path){std::string out="\"";for(const char c:path.string()){if(c=='"'||c=='\\'||c=='`'||c=='$')out+='\\';check(c!='\n'&&c!='\r',"The installation folder name is unsupported.");if(c=='%')out+='%';out+=c;}return out+'"';}
fs::path desktop_directory(){
    const fs::path home=paths::environment(L"HOME");
    fs::path config=paths::environment(L"XDG_CONFIG_HOME");if(!config.is_absolute())config=home/".config";
    std::ifstream input(config/"user-dirs.dirs");std::string row;
    while(std::getline(input,row))if(row.starts_with("XDG_DESKTOP_DIR=\"")&&row.ends_with('"')){
        auto value=row.substr(17,row.size()-18);if(value.starts_with("$HOME/"))value=home.string()+value.substr(5);
        const fs::path path=value;if(path.is_absolute()&&path!=home&&fs::is_directory(path))return path;return {};
    }
    return fs::is_directory(home/"Desktop")?home/"Desktop":fs::path{};
}
#endif
void shortcut(const fs::path& game){
#ifdef _WIN32
    PWSTR menu=nullptr;check(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs,0,nullptr,&menu)),"The Start menu could not be located.");
    const fs::path link=fs::path(menu)/L"Sonic Adventure Recompiled.lnk";CoTaskMemFree(menu);
    PWSTR desktop=nullptr;const auto desktop_result=SHGetKnownFolderPath(FOLDERID_Desktop,0,nullptr,&desktop);
    const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    IShellLinkW* shell=nullptr;auto result=CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&shell));
    if(SUCCEEDED(result)){
        shell->SetPath(game.c_str());shell->SetWorkingDirectory(game.parent_path().c_str());shell->SetDescription(L"Sonic Adventure Recompiled");
        shell->SetIconLocation((game.parent_path()/"resources/sonic-adventure-recompiled.ico").c_str(),0);
        IPersistFile* file=nullptr;result=shell->QueryInterface(IID_PPV_ARGS(&file));if(SUCCEEDED(result)){
            result=file->Save(link.c_str(),TRUE);
            if(SUCCEEDED(result)&&SUCCEEDED(desktop_result))result=file->Save((fs::path(desktop)/L"Sonic Adventure Recompiled.lnk").c_str(),TRUE);
            file->Release();
        }shell->Release();
    }
    CoTaskMemFree(desktop);
    if(SUCCEEDED(initialized))CoUninitialize();check(SUCCEEDED(result),"The Start menu shortcut could not be saved.");
#else
    fs::path applications;
    if(const auto xdg=paths::environment(L"XDG_DATA_HOME");!xdg.empty()&&fs::path(xdg).is_absolute())applications=fs::path(xdg)/"applications";
    else applications=fs::path(paths::environment(L"HOME"))/".local/share/applications";
    const auto file=applications/"sonic-adventure-recompiled.desktop";
    const std::string entry="[Desktop Entry]\nType=Application\nName=Sonic Adventure Recompiled\nComment=Port by SoNiCFReaK; powered by KatanaRecomp\nExec="+exec_quote(game)+"\nPath="+game.parent_path().string()+"\nIcon="+(game.parent_path()/"resources/sonic-adventure-recompiled.png").string()+"\nTerminal=false\nCategories=Game;\nStartupNotify=true\n";
    auto temporary=file;temporary+=".pending";write(temporary,entry);linux_host::publish_file(temporary,file);
    if(const auto desktop=desktop_directory();!desktop.empty()){
        const auto link=desktop/file.filename();auto pending=link;pending+=".pending";write(pending,entry);
        fs::permissions(pending,fs::perms::owner_all|fs::perms::group_read|fs::perms::others_read);
        linux_host::publish_file(pending,link);
    }
#endif
}
}
fs::path install_runtime(const fs::path& source,const fs::path& content,const InstallCallback& progress,const std::atomic<bool>& cancel){
    const auto manifest=text(source/"resources/payload-files.tsv");
    const auto identity=katana::io::sha256_bytes(manifest);
    const auto root=paths::data_root(paths::executable()).parent_path()/"SARecomp-app";
    fs::create_directories(root);presentation::ConfigurationLock lock(root/"installation");
    std::istringstream lines(manifest);std::string row;std::getline(lines,row);
    if(row.ends_with('\r'))row.pop_back();
    check(row=="SARECOMP-PAYLOAD-1","The application payload is not supported.");
    struct File{std::string name,sha;std::uint64_t size;};std::vector<File> files;std::set<std::string> names;std::uint64_t total=0;
    while(std::getline(lines,row)){
        if(row.ends_with('\r'))row.pop_back();
        std::istringstream fields(row);File file;std::string size;
        check(bool(std::getline(fields,file.name,'\t')&&std::getline(fields,size,'\t')&&std::getline(fields,file.sha)),"The application manifest is invalid.");
        const auto number=std::from_chars(size.data(),size.data()+size.size(),file.size);
        check(number.ec==std::errc{}&&number.ptr==size.data()+size.size()&&safe(file.name)&&file.sha.size()==64&&names.insert(file.name).second&&file.size<=4ull*1024*1024*1024,"The application manifest contains an invalid file.");
        total+=file.size;files.push_back(std::move(file));check(files.size()<1024&&total<8ull*1024*1024*1024,"The application manifest is too large.");
    }
#ifdef _WIN32
    constexpr auto executable="game.exe";
#else
    constexpr auto executable="game";
#endif
    check(names.contains(executable)&&names.contains("resources/NotoSans-Regular.ttf"),"The application package is incomplete.");
    const auto destination=root/("1.0-candidate-"+identity.substr(0,16));
    if(fs::exists(destination)){
        for(const auto& file:files)verify(destination/file.name,file.size,file.sha,cancel);
        if(progress)progress({99,"Restoring application shortcut..."});
        // The unchanged application can reuse newly verified disc content.
    }else{
        check(fs::space(root).available>total+64*1024*1024,"There is not enough free space for the application.");
        std::random_device random;const auto pending=root/(".install-"+std::to_string(random())+"-"+std::to_string(random()));
        check(fs::create_directory(pending),"The application staging folder could not be created.");
        try{
            std::uint64_t copied=0;
            for(const auto& file:files){
                cancel_check(cancel);const auto original=source/file.name,output=pending/file.name;
                check(fs::is_regular_file(fs::symlink_status(original)),"An application payload file is not regular.");
                fs::create_directories(output.parent_path());fs::copy_file(original,output,fs::copy_options::none);
                verify(output,file.size,file.sha,cancel);copied+=file.size;
#ifndef _WIN32
                if(file.name=="game"||file.name=="sonic-setup"||file.name=="sonic-startup-ui")fs::permissions(output,fs::perms::owner_all|fs::perms::group_read|fs::perms::group_exec|fs::perms::others_read|fs::perms::others_exec);
#endif
                if(progress)progress({90+unsigned(9*copied/std::max(std::uint64_t{1},total)),"Installing application files..."});
            }
            write(pending/"resources/payload-files.tsv",manifest);cancel_check(cancel);fs::rename(pending,destination);
        }catch(...){if(pending.parent_path()==root&&pending.filename().string().starts_with(".install-")){std::error_code error;fs::remove_all(pending,error);}throw;}
    }
#ifdef _WIN32
    auto pointer=destination/"katana-content-root.txt",temporary=destination/"katana-content-root.pending";write(temporary,content.string()+"\n");
    check(MoveFileExW(temporary.c_str(),pointer.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"The installed content could not be selected.");
#endif
    // Isolated installer tests must never edit the user's launcher menu.
    if(fs::path(paths::environment(L"KATANA_PORT_BACKGROUND_TEST"))!="1")shortcut(destination/executable);
    if(progress)progress({100,"Installation complete."});return destination/executable;
}
void start_game(const fs::path& program){
#ifdef _WIN32
    std::wstring command=L"\""+program.wstring()+L"\"";STARTUPINFOW start{sizeof(start)};PROCESS_INFORMATION process{};
    check(CreateProcessW(program.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,program.parent_path().c_str(),&start,&process)!=0,"The installed game could not be started.");CloseHandle(process.hThread);CloseHandle(process.hProcess);
#else
    ::execl(program.c_str(),program.c_str(),static_cast<char*>(nullptr));throw InstallError("The installed game could not be started.");
#endif
}
}
