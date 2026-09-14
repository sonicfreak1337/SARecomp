#include "../src/setup/setup_ui.hpp"
#include <iostream>
int main(int argc,char** argv){
    try{
        if(argc!=3)throw std::runtime_error("setup-preview <resources> <capture-directory>");
        sonic::setup::Resources resources(argv[1]);const std::filesystem::path output(argv[2]);std::filesystem::create_directories(output);
        for(unsigned i=0;i<6;++i){sonic::setup::View state;state.page=sonic::setup::Page(i);state.steam_deck=true;state.source_selected=true;state.filename=L"Sonic Adventure.gdi";state.progress=62;
            state.error=L"One of the track files is missing. Place the GDI and all of its track files together, then choose the GDI again.";
            sonic::setup::render(resources,state).save(output/(std::to_string(i)+"-setup.png"));}
        std::cout<<"SONIC_SETUP_UI_CAPTURE_OK pages=6 size=1280x800\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
