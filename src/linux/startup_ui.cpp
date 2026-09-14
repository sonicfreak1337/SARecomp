#include "startup_channel.hpp"
#include "../ui/sonic_raster.hpp"
#include "../sonic_user_paths.hpp"
#include <SDL3/SDL.h>
#include <sys/socket.h>
#include <algorithm>
#include <iostream>

int main(){
    try{
        // This helper has its own SDL event owner; it cannot consume input or
        // device events intended for the game's graphics thread.
        if(!SDL_Init(SDL_INIT_VIDEO))throw std::runtime_error(SDL_GetError());
        const auto resources=sonic::paths::executable().parent_path()/"resources";
        sonic::ui::Font regular(resources/"NotoSans-Regular.ttf"),bold(resources/"NotoSans-Bold.ttf");
        auto flags=SDL_WINDOW_HIDDEN|SDL_WINDOW_HIGH_PIXEL_DENSITY;
        auto* window=SDL_CreateWindow("Sonic Adventure: Recompiled",640,190,flags);
        if(!window)throw std::runtime_error(SDL_GetError());
        auto* renderer=SDL_CreateRenderer(window,nullptr);if(!renderer)throw std::runtime_error(SDL_GetError());
        auto* texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,640,190);
        if(!texture)throw std::runtime_error(SDL_GetError());
        const auto* hidden=std::getenv("KATANA_PORT_BACKGROUND_TEST");
        if(!hidden||std::string_view(hidden)!="1")SDL_ShowWindow(window);
        sonic::startup::ProgressMessage value;bool dirty=true,quit=false;
        const auto render=[&]{
            sonic::ui::Canvas c(640,190);c.gradient({0,0,640,190},{7,42,76},{13,73,104});
            c.text(bold,L"SONIC ADVENTURE: RECOMPILED",{24,20,592,36},27,{245,250,255});
            // Progress labels are fixed English host strings, bounded by IPC.
            std::wstring label;for(unsigned i=0;i<value.label_size;++i)label+=wchar_t(static_cast<unsigned char>(value.label[i]));
            c.text(regular,label,{24,66,592,30},21,{177,207,226});
            c.rounded({24,112,592,11},5,{31,72,96});
            if(value.total)c.rounded({24,112,int(592*std::min(value.completed,value.total)/value.total),11},5,{255,198,58});
            else {const int position=int((SDL_GetTicks()/8)%682)-90;c.rounded({24+std::max(0,position),112,std::max(0,std::min(592,position+90)-std::max(0,position)),11},5,{255,198,58});}
            c.text(regular,L"Please wait...",{24,142,450,28},17,{177,207,226});
            SDL_UpdateTexture(texture,nullptr,c.image.pixels.data(),640*4);SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer,texture,nullptr,nullptr);SDL_RenderPresent(renderer);
        };
        while(!quit){
            sonic::startup::ProgressMessage next;
            for(;;){
                const auto count=::recv(3,&next,sizeof(next),MSG_DONTWAIT);
                if(count==0){quit=true;break;}
                if(count<0){if(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)quit=true;break;}
                if(count==sizeof(next)&&next.version==1&&next.label_size<=next.label.size()){value=next;dirty=true;}
            }
            if(quit)break;
            if(dirty||!value.total){render();dirty=false;}
            SDL_Event event;
            if(SDL_WaitEventTimeout(&event,40)&&event.type==SDL_EVENT_QUIT)SDL_HideWindow(window);
        }
        SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    }catch(const std::exception& error){std::cerr<<"SONIC_STARTUP_UI "<<error.what()<<'\n';return 1;}
}
