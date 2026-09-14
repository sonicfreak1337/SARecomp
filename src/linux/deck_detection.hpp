#pragma once
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <filesystem>
#include <chrono>

namespace sonic::linux_host {
inline bool steam_deck() {
    if(const auto* value=std::getenv("SteamDeck");value&&std::string_view(value)=="1")return true;
    std::string vendor,product;
    std::getline(std::ifstream("/sys/class/dmi/id/sys_vendor"),vendor);
    std::getline(std::ifstream("/sys/class/dmi/id/product_name"),product);
    return vendor=="Valve"&&(product=="Jupiter"||product=="Galileo");
}
inline bool external_display_connected() {
    std::error_code error;
    for(const auto& connector:std::filesystem::directory_iterator("/sys/class/drm",error)){
        const auto name=connector.path().filename().string();
        if(name.find("-eDP-")!=name.npos || name.find("-DSI-")!=name.npos)continue;
        std::ifstream input(connector.path()/"status");std::string status;
        if(std::getline(input,status)&&status=="connected")return true;
    }
    return false;
}
}
