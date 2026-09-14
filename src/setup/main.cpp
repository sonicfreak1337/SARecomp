#include "setup_ui.hpp"
#include "install_engine.hpp"
#include "install_runtime.hpp"
#include "../sonic_user_paths.hpp"
#include "../sonic_presentation.hpp"
#include "../sonic_configuration_lock.hpp"
#include "../linux/deck_detection.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <fstream>

namespace {
namespace fs=std::filesystem;
using namespace sonic::setup;
std::wstring wide(std::string_view input){
    std::wstring output;
    for(std::size_t i=0;i<input.size();){
        unsigned cp=static_cast<unsigned char>(input[i++]),count=0;
        if(cp>=0xf0){cp&=7;count=3;}else if(cp>=0xe0){cp&=15;count=2;}else if(cp>=0xc0){cp&=31;count=1;}
        for(unsigned n=0;n<count;++n){if(i==input.size()||(static_cast<unsigned char>(input[i])&0xc0)!=0x80){cp=0xfffd;break;}cp=(cp<<6)|(static_cast<unsigned char>(input[i++])&63);}
        output.push_back(wchar_t(cp>0x10ffff?0xfffd:cp));
    }
    return output;
}
bool test_allowed(){const auto* value=std::getenv("KATANA_PORT_BACKGROUND_TEST");return value&&std::string_view(value)=="1";}
void sdl_check(bool success){if(!success)throw std::runtime_error(SDL_GetError());}
struct Session {
    std::mutex mutex;
    View view;
    fs::path source;
    bool dirty=true,dialog_pending=false,quit=false;
    std::atomic<bool> cancel{false};
};
void selected_file(void* userdata,const char* const* files,int){
    std::unique_ptr<std::weak_ptr<Session>> weak(static_cast<std::weak_ptr<Session>*>(userdata));
    if(auto session=weak->lock()){
        std::lock_guard guard(session->mutex);session->dialog_pending=false;session->dirty=true;
        if(files&&files[0]){session->source=fs::u8path(files[0]);session->view.source_selected=true;session->view.filename=wide(session->source.filename().string());session->view.page=Page::Files;}
        else if(!files){session->view.page=Page::Error;session->view.error=L"The file picker could not be opened. Run setup in Desktop Mode. You can also drag your GDI file onto the game files page.";}
    }
}
void save_defaults(const fs::path& program,bool deck){
    const auto file=sonic::paths::data_root(program)/"sonic-display.ini";
    sonic::presentation::ConfigurationLock lock(file);
    const bool exists=fs::exists(file);
    auto settings=exists?sonic::presentation::read_settings(file):sonic::presentation::Settings{};
    if(!exists){
#ifndef _WIN32
    settings.renderer=sonic::rendering::Renderer::Vulkan;
#endif
    settings.width=1280;settings.height=deck?800:720;settings.widescreen=true;
    settings.window_mode=deck?sonic::rendering::WindowMode::Fullscreen:sonic::rendering::WindowMode::Windowed;
    settings.vsync=deck?1:2;
    // Temporary Deck default while Recompiled timing cannot sustain 60 SIM.
    // Reinstallation preserves the user's existing timing choice.
    if(deck)settings.gameplay_timing=0;
    }
    settings.setup_complete=true;
    sonic::presentation::save_settings(file,settings);
}
class Application {
    fs::path program_,resources_path_,installed_game_;
    Resources resources_;
    std::shared_ptr<Session> session_=std::make_shared<Session>();
    std::jthread worker_;
    SDL_Window* window_=nullptr;
    SDL_Renderer* renderer_=nullptr;
    SDL_Texture* texture_=nullptr;
    std::vector<SDL_Gamepad*> gamepads_;
    unsigned texture_w_=0,texture_h_=0;
    bool hidden_=false;
public:
    Application(fs::path program,fs::path resources,bool hidden):program_(std::move(program)),resources_path_(std::move(resources)),resources_(resources_path_),hidden_(hidden){
        sdl_check(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD));
        session_->view.steam_deck=sonic::linux_host::steam_deck();
        std::string edition;std::getline(std::ifstream(resources_path_/"edition.txt"),edition);
        if(edition.ends_with('\r'))edition.pop_back();
        if(edition=="steam-deck")session_->view.steam_deck=true;
        SDL_Rect area{0,0,1280,800};SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(),&area);
        const double scale=hidden?1.0:std::min({1.0,(area.w-48)/1280.0,(area.h-72)/800.0});
        const int w=std::max(800,int(1280*scale)),h=std::max(500,int(800*scale));
        // Setup runs in Desktop Mode. Only the game enters fullscreen.
        window_=SDL_CreateWindow("Sonic Adventure: Recompiled Setup",w,h,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY|(hidden?SDL_WINDOW_HIDDEN:0));
        sdl_check(window_!=nullptr);SDL_SetWindowMinimumSize(window_,800,500);
        if(fs::is_regular_file(resources_path_/"sonic-adventure-recompiled.png")){
            auto icon=sonic::ui::Image::load(resources_path_/"sonic-adventure-recompiled.png");
            if(auto* surface=SDL_CreateSurfaceFrom(int(icon.width),int(icon.height),SDL_PIXELFORMAT_RGBA32,icon.pixels.data(),int(icon.width)*4)){
                SDL_SetWindowIcon(window_,surface);SDL_DestroySurface(surface);
            }
        }
        renderer_=SDL_CreateRenderer(window_,hidden?"software":nullptr);sdl_check(renderer_!=nullptr);
        SDL_SetRenderVSync(renderer_,0);
        int count=0;auto* ids=SDL_GetGamepads(&count);for(int i=0;i<count;++i)if(auto* pad=SDL_OpenGamepad(ids[i]))gamepads_.push_back(pad);SDL_free(ids);
    }
    ~Application(){
        session_->cancel=true;if(worker_.joinable())worker_.join();session_.reset();
        for(auto* pad:gamepads_)SDL_CloseGamepad(pad);
        if(texture_)SDL_DestroyTexture(texture_);if(renderer_)SDL_DestroyRenderer(renderer_);if(window_)SDL_DestroyWindow(window_);SDL_Quit();
    }
    void picker(){
        {std::lock_guard lock(session_->mutex);if(session_->dialog_pending)return;session_->dialog_pending=true;}
        static const SDL_DialogFileFilter filter{"Dreamcast GDI","gdi"};
        SDL_ShowOpenFileDialog(selected_file,new std::weak_ptr<Session>(session_),window_,&filter,1,nullptr,false);
    }
    void back(){
        std::lock_guard lock(session_->mutex);auto& v=session_->view;if(session_->dialog_pending)return;
        if(v.page==Page::Installing){session_->cancel=true;v.detail=L"Stopping installation...";}
        else if(v.page==Page::Legal||v.page==Page::Welcome||v.page==Page::Ready)session_->quit=true;
        else v.page=v.page==Page::Error?Page::Files:Page::Welcome;
        v.selected=0;session_->dirty=true;
    }
    void primary(){
        Page page;bool selected;fs::path source;bool deck;
        {std::lock_guard lock(session_->mutex);if(session_->dialog_pending)return;page=session_->view.page;selected=session_->view.source_selected;source=session_->source;deck=session_->view.steam_deck;}
        if(page==Page::Installing)return;
        if(page==Page::Files&&selected){
            if(worker_.joinable())worker_.join();session_->cancel=false;
            {std::lock_guard lock(session_->mutex);session_->view.page=Page::Installing;session_->view.progress=0;session_->view.detail=L"Checking game files...";session_->dirty=true;}
            worker_=std::jthread([this,source,deck]{
                try{
                    const auto result=install(source,resources_path_,sonic::paths::content_store_root(program_),[this](const InstallProgress& p){
                        std::lock_guard lock(session_->mutex);auto& v=session_->view;v.progress=p.percent*90/100;v.detail=wide(p.message);session_->dirty=true;
                    },session_->cancel);
                    installed_game_=install_runtime(program_.parent_path(),result.content,[this](const InstallProgress& p){
                        std::lock_guard lock(session_->mutex);session_->view.progress=p.percent;session_->view.detail=wide(p.message);session_->dirty=true;
                    },session_->cancel);
                    save_defaults(program_,deck);
                    std::cout<<"SONIC_INSTALL_COMPLETE files="<<result.files<<" bytes="<<result.bytes<<'\n';
                    std::lock_guard lock(session_->mutex);session_->view.page=Page::Ready;session_->dirty=true;
                }catch(const std::exception& e){
                    std::cerr<<"SONIC_SETUP_ERROR "<<e.what()<<'\n';std::lock_guard lock(session_->mutex);session_->view.page=Page::Error;session_->view.error=wide(e.what());session_->dirty=true;
                }
            });return;
        }
        if(page==Page::Files){picker();return;}
        if(page==Page::Ready){
            if(worker_.joinable())worker_.join();
            if(fs::is_regular_file(installed_game_)){start_game(installed_game_);std::lock_guard lock(session_->mutex);session_->quit=true;return;}
            std::lock_guard lock(session_->mutex);session_->view.page=Page::Error;session_->view.error=L"The game executable could not be started. Extract the complete package and try again.";session_->dirty=true;return;
        }
        std::lock_guard lock(session_->mutex);session_->view.page=page==Page::Legal?Page::Welcome:Page::Files;session_->view.selected=0;session_->dirty=true;
    }
    void event(const SDL_Event& e){
        if(e.type==SDL_EVENT_QUIT){
            std::lock_guard lock(session_->mutex);if(session_->view.page==Page::Installing){session_->cancel=true;session_->view.detail=L"Stopping installation...";session_->dirty=true;}else if(!session_->dialog_pending)session_->quit=true;return;
        }
        if(e.type==SDL_EVENT_GAMEPAD_ADDED){if(auto* pad=SDL_OpenGamepad(e.gdevice.which))gamepads_.push_back(pad);return;}
        if(e.type==SDL_EVENT_GAMEPAD_REMOVED){std::erase_if(gamepads_,[&](SDL_Gamepad* pad){if(SDL_GetGamepadID(pad)!=e.gdevice.which)return false;SDL_CloseGamepad(pad);return true;});return;}
        if(e.type==SDL_EVENT_DROP_FILE&&e.drop.data){std::lock_guard lock(session_->mutex);if(session_->view.page==Page::Files){session_->source=fs::u8path(e.drop.data);session_->view.filename=wide(session_->source.filename().string());session_->view.source_selected=true;session_->dirty=true;}return;}
        if(e.type==SDL_EVENT_KEY_DOWN&&!e.key.repeat){
            if(e.key.key==SDLK_ESCAPE){back();return;}
            if(e.key.key==SDLK_RETURN||e.key.key==SDLK_SPACE){unsigned selected;{std::lock_guard lock(session_->mutex);selected=session_->view.selected;}if(selected==1)back();else if(selected==2)picker();else primary();return;}
            if(e.key.key==SDLK_TAB||e.key.key==SDLK_LEFT||e.key.key==SDLK_RIGHT){std::lock_guard lock(session_->mutex);auto& v=session_->view;v.selected=(v.page==Page::Welcome||v.page==Page::Ready)?0:(v.selected==1?0:1);session_->dirty=true;return;}
            if(e.key.key==SDLK_UP||e.key.key==SDLK_DOWN){std::lock_guard lock(session_->mutex);if(session_->view.page==Page::Files)session_->view.selected=e.key.key==SDLK_UP?2:0;session_->dirty=true;return;}
        }
        if(e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN){
            SDL_Event key{};key.type=SDL_EVENT_KEY_DOWN;
            switch(e.gbutton.button){case SDL_GAMEPAD_BUTTON_SOUTH:key.key.key=SDLK_RETURN;break;case SDL_GAMEPAD_BUTTON_EAST:key.key.key=SDLK_ESCAPE;break;case SDL_GAMEPAD_BUTTON_DPAD_LEFT:key.key.key=SDLK_LEFT;break;case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:key.key.key=SDLK_RIGHT;break;case SDL_GAMEPAD_BUTTON_DPAD_UP:key.key.key=SDLK_UP;break;case SDL_GAMEPAD_BUTTON_DPAD_DOWN:key.key.key=SDLK_DOWN;break;default:return;}event(key);return;
        }
        if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&e.button.button==SDL_BUTTON_LEFT){
            int logical_w,logical_h,pixel_w,pixel_h;SDL_GetWindowSize(window_,&logical_w,&logical_h);SDL_GetWindowSizeInPixels(window_,&pixel_w,&pixel_h);
            const int x=int(e.button.x*pixel_w/std::max(1,logical_w)),y=int(e.button.y*pixel_h/std::max(1,logical_h));
            Page page;{std::lock_guard lock(session_->mutex);page=session_->view.page;}
            if(primary_button(pixel_w,pixel_h).contains(x,y)&&page!=Page::Installing){primary();return;}
            if(back_button(pixel_w,pixel_h).contains(x,y)&&page!=Page::Welcome&&page!=Page::Ready){back();return;}
            const float scale=std::min(pixel_w/1280.f,pixel_h/800.f);
            const int bx=int((x-(pixel_w-1280*scale)/2)/scale),by=int((y-(pixel_h-800*scale)/2)/scale);
            if(page==Page::Files&&sonic::ui::Rect{699,509,493,77}.contains(bx,by)){picker();return;}
            if(sonic::ui::Rect{699,726,493,23}.contains(bx,by))SDL_OpenURL(("file://"+(resources_path_/"THIRD-PARTY-NOTICES.txt").string()).c_str());
        }
        if(e.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED||e.type==SDL_EVENT_WINDOW_EXPOSED){std::lock_guard lock(session_->mutex);session_->dirty=true;}
    }
    void draw(const fs::path& capture={}){
        View view;{std::lock_guard lock(session_->mutex);view=session_->view;session_->dirty=false;}
        int w,h;SDL_GetWindowSizeInPixels(window_,&w,&h);if(w<1||h<1)return;
        auto frame=render(resources_,view,unsigned(w),unsigned(h));
        if(texture_w_!=unsigned(w)||texture_h_!=unsigned(h)){
            if(texture_)SDL_DestroyTexture(texture_);texture_=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,w,h);sdl_check(texture_!=nullptr);texture_w_=w;texture_h_=h;
        }
        sdl_check(SDL_UpdateTexture(texture_,nullptr,frame.pixels.data(),w*4));SDL_RenderClear(renderer_);sdl_check(SDL_RenderTexture(renderer_,texture_,nullptr,nullptr));sdl_check(SDL_RenderPresent(renderer_));
        if(!capture.empty())frame.save(capture);
    }
    int run(){
        for(;;){bool quit,dirty;{std::lock_guard lock(session_->mutex);quit=session_->quit;dirty=session_->dirty;}if(quit)return 0;if(dirty)draw();
            SDL_Event e;if(SDL_WaitEventTimeout(&e,50)){event(e);while(SDL_PollEvent(&e))event(e);}
        }
    }
    void smoke(const fs::path& output){
        fs::create_directories(output);
        if(session_->view.page!=Page::Legal)throw std::runtime_error("Setup did not begin with the notice");
        draw(output/"00-notice.png");
        SDL_Event key{};key.type=SDL_EVENT_KEY_DOWN;key.key.key=SDLK_RETURN;event(key);
        if(session_->view.page!=Page::Welcome)throw std::runtime_error("Notice confirmation failed");draw(output/"01-welcome.png");
        event(key);if(session_->view.page!=Page::Files)throw std::runtime_error("File selection navigation failed");draw(output/"02-game-files.png");
        SDL_Event back_event{};back_event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;back_event.gbutton.button=SDL_GAMEPAD_BUTTON_EAST;event(back_event);
        if(session_->view.page!=Page::Welcome)throw std::runtime_error("Controller Back failed");
        if(SDL_GetWindowFlags(window_)&SDL_WINDOW_FULLSCREEN)throw std::runtime_error("Setup must use a Desktop Mode window");
        std::cout<<"SONIC_SETUP_LINUX_UI_OK first=notice keyboard=ok controller=ok mode=desktop-window\n";
    }
};
}
int main(int argc,char** argv){
    try{
        const auto program=sonic::paths::executable();const auto resources=program.parent_path()/"resources";
        if(argc==3&&std::string_view(argv[1])=="--defaults-check"){
            if(!test_allowed()||sonic::paths::environment(L"KATANA_USER_DATA_ROOT").empty())throw std::runtime_error("Defaults checks require isolated background data");
            const auto file=sonic::paths::data_root(program)/"sonic-display.ini";
            if(fs::exists(file))throw std::runtime_error("Defaults check requires a fresh directory");
            const bool deck=std::string_view(argv[2])=="steam-deck";
            save_defaults(program,deck);auto value=sonic::presentation::read_settings(file);
            if(value.gameplay_timing!=unsigned(!deck)||!value.setup_complete||value.width!=1280||value.height!=(deck?800:720))throw std::runtime_error("Incorrect edition defaults");
            value.gameplay_timing=unsigned(deck);value.music_volume=37;
            sonic::presentation::save_settings(file,value);save_defaults(program,deck);
            const auto kept=sonic::presentation::read_settings(file);
            if(kept.gameplay_timing!=value.gameplay_timing||kept.music_volume!=37)throw std::runtime_error("Reinstall changed existing settings");
            std::cout<<"SONIC_SETUP_DEFAULTS_OK edition="<<argv[2]<<" default_timing="<<(deck?"original":"recompiled")<<" reinstall=preserved\n";return 0;
        }
        if(argc==3&&std::string_view(argv[1])=="--full-install-check"){
            if(!test_allowed()||sonic::paths::environment(L"KATANA_USER_DATA_ROOT").empty())throw std::runtime_error("Full installation checks require an isolated background test");
            const std::atomic<bool> cancel{false};
            const auto report=[](const InstallProgress& p){std::cout<<p.percent<<"% "<<p.message<<std::endl;};
            const auto result=install(fs::u8path(argv[2]),resources,sonic::paths::content_store_root(program),report,cancel);
            const auto game=install_runtime(program.parent_path(),result.content,report,cancel);
            std::string edition;std::getline(std::ifstream(resources/"edition.txt"),edition);
            if(edition.ends_with('\r'))edition.pop_back();
            save_defaults(program,edition=="steam-deck"||sonic::linux_host::steam_deck());
            std::cout<<"SONIC_FULL_INSTALL_OK files="<<result.files<<" game="<<game.string()<<'\n';return 0;
        }
        if(argc==5&&std::string_view(argv[1])=="--install-check"){
            if(!test_allowed())throw std::runtime_error("Install checks require an isolated background test");
            const std::atomic<bool> cancel{false};unsigned previous=101;
            const auto result=install(argv[2],argv[3],argv[4],[&](const InstallProgress& p){if(p.percent!=previous){std::cout<<p.percent<<"% "<<p.message<<std::endl;previous=p.percent;}},cancel);
            std::cout<<"SONIC_INSTALL_CHECK_OK files="<<result.files<<" bytes="<<result.bytes<<'\n';return 0;
        }
        const bool smoke=argc==3&&std::string_view(argv[1])=="--smoke";
        if(smoke&&!test_allowed())throw std::runtime_error("UI checks require an isolated background test");
        Application app(program,resources,smoke);if(smoke){app.smoke(argv[2]);return 0;}return app.run();
    }catch(const std::exception& e){std::cerr<<"SONIC_SETUP_ERROR "<<e.what()<<'\n';if(!test_allowed())SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Sonic Adventure Setup",e.what(),nullptr);return 1;}
}
