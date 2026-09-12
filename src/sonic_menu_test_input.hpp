#pragma once
#include "sonic_input.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace sonic::menu {
// Read-only logical input at the same boundary as live menu input. This is
// admitted only for explicit hidden tests and never sends OS input or changes
// guest state. Times restart at each menu opening, independent of guest FPS.
class TestInput {
    struct Event {double time,duration;unsigned key;};
    std::vector<Event> events_;
    bool active_=false;
    mutable std::size_t next_=0;
    mutable std::optional<double> pressed_at_;
public:
    TestInput() {
        const auto hidden=std::getenv("KATANA_PORT_BACKGROUND_TEST");
        const auto path=std::getenv("SARECOMP_MENU_TEST_INPUT");
        if(!hidden||std::string_view(hidden)!="1"||!path||!*path)return;
        const auto file=std::filesystem::path(path);
        if(std::filesystem::file_size(file)>16384)throw std::runtime_error("menu-test-input-size");
        std::ifstream stream(file);Event value{};double previous=-1;
        while(stream>>value.time>>value.duration>>value.key){
            if(events_.size()>=128||value.time<previous||value.time<0||value.time>120||
               value.duration<=0||value.duration>.25||!value.key||value.key>=256)
                throw std::runtime_error("menu-test-input-record");
            previous=value.time;events_.push_back(value);
        }
        if(!stream.eof())throw std::runtime_error("menu-test-input-format");
        active_=true;
    }
    std::optional<input::Snapshot> sample(double seconds,unsigned width,unsigned height) const {
        if(!active_)return {};
        input::Snapshot result;result.focused=true;result.client_width=width;result.client_height=height;
        // Deliver every edge even if a cold raster/upload takes longer than
        // one scheduled pulse. A neutral sample always separates events.
        if(next_<events_.size()){
            const auto& event=events_[next_];
            if(!pressed_at_&&seconds>=event.time)pressed_at_=seconds;
            if(pressed_at_){
                if(seconds-*pressed_at_<event.duration)result.keys[event.key]=true;
                else {++next_;pressed_at_.reset();}
            }
        }
        return result;
    }
};
}
