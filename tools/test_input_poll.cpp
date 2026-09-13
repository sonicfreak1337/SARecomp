#define NOMINMAX
#include <windows.h>
#include "sonic_input.hpp"
#include "sonic_joystick_query.hpp"
#include "sonic_input_probe.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace fs=std::filesystem;
using namespace katana::runtime;
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
template<class T>void put(std::ofstream& f,T value){for(unsigned i=0;i<sizeof(T);++i)f.put(char(value>>(i*8)));}
void fixture(const fs::path& path,std::string_view identity){
    std::ofstream f(path,std::ios::binary);f.write("KATANAIR",8);
    for(auto value:{2u,1u,4u,unsigned(identity.size())})put(f,value);
    put(f,std::uint64_t(2));put(f,std::uint64_t(2));put(f,224u);put(f,0u);f<<identity;
    for(unsigned frame=1;frame<=2;++frame){
        put(f,std::uint64_t(frame));put(f,std::uint64_t(77));
        for(unsigned pad=0;pad<4;++pad){
            put(f,unsigned(pad==0));put(f,frame);put(f,pad==0?1u<<(9+frame):0u);
            for(unsigned i=0;i<4;++i)put(f,std::uint16_t(0));
            for(unsigned i=0;i<8;++i)put(f,0u);
        }
    }check(bool(f),"trace fixture write");
}
std::uint64_t recorded_frames(const fs::path& path){
    std::ifstream f(path,std::ios::binary);f.seekg(24);std::uint64_t value=0;
    for(unsigned i=0;i<8;++i){const auto byte=f.get();check(byte!=EOF,"trace header read");value|=std::uint64_t(byte)<<(8*i);}return value;
}
int main(int argc,char** argv){
    try{
        for(bool position_ok:{false,true})for(bool caps_ok:{false,true})for(bool control:{false,true}){
            std::string calls;
            const bool admitted=sonic::input::query_connected_joystick(control,
                [&]{calls+='P';return position_ok;},[&]{calls+='C';return caps_ok;});
            check(admitted==(position_ok&&caps_ok),"joystick error path admitted or rejected a device");
            check(calls==(control?(caps_ok?"CP":"C"):(position_ok?"PC":"P")),"joystick query order or early rejection");
        }
        check(argc==2,"fresh test directory required");const auto root=fs::absolute(argv[1]);check(!fs::exists(root),"test directory exists");
        fs::create_directories(root/"content");fs::create_directories(root/"data");
        _putenv_s("KATANA_PORT_BACKGROUND_TEST","1");
        _putenv_s("SARECOMP_BENCHMARK_ISOLATED_INPUT","0");
        NativePortPlatformConfig config;config.content_root=root/"content";config.user_data_root=root/"data";
        config.project_id="sonic-input-poll-tests";config.input_identity="sonic-input-poll-tests-v1";
        config.maximum_input_record_frames=8;config.require_gamepad_backend=false;
        config.input_replay_path=root/"authored.kat1";fixture(config.input_replay_path,config.input_identity);
        {
            NativePortPlatformServices platform(config);
            const auto first=platform.poll_gamepads();check(first.poll_sequence==1&&first.gamepads[0].buttons==(1u<<10),"first authored input");
            for(unsigned i=0;i<24;++i)(void)sonic::input::poll_host(platform);
            const auto second=platform.poll_gamepads();check(second.poll_sequence==2&&second.connection_generation==77&&second.gamepads[0].buttons==(1u<<11),"host menu consumed guest replay");
            check(platform.snapshot().input_polls==2,"host menu changed guest poll count");
            bool exhausted=false;try{(void)platform.poll_gamepads();}catch(const NativePortPlatformError& e){exhausted=e.platform_error_code()==ERROR_HANDLE_EOF;}
            check(exhausted,"guest replay must still end at its authored boundary");
            bool restored=false;
            std::thread wrong_owner([&]{try{(void)sonic::input::poll_host(platform);}catch(const NativePortPlatformError& e){restored=e.failure()==NativePortPlatformFailure::ThreadViolation&&!sonic::input::host_poll_active();}});
            wrong_owner.join();check(restored&&!sonic::input::host_poll_active(),"host poll scope leaked after error");
        }
        config.input_replay_path.clear();config.input_record_path=root/"recorded.kat1";
        {
            NativePortPlatformServices platform(config);
            check(platform.poll_gamepads().poll_sequence==1,"first recording sequence");
            for(unsigned i=0;i<24;++i)(void)sonic::input::poll_host(platform);
            check(platform.poll_gamepads().poll_sequence==2,"host menu advanced recording sequence");
            platform.finalize_clean_shutdown();
        }
        check(recorded_frames(config.input_record_path)==2,"host menu input was recorded");
        _putenv_s("SARECOMP_BENCHMARK_ISOLATED_INPUT","1");
        _putenv_s("KATANA_SONIC_GAMEPLAY_PROBE","1");
        _putenv_s("KATANA_SONIC_GAMEPLAY_INPUT_PROFILE","2");
        check(!sonic::input::isolated_gameplay_input(),"isolation admitted another input profile");
        _putenv_s("KATANA_SONIC_GAMEPLAY_INPUT_PROFILE","3");
        _putenv_s("KATANA_PORT_BACKGROUND_TEST","0");
        check(!sonic::input::isolated_gameplay_input(),"isolation admitted a visible session");
        _putenv_s("KATANA_PORT_BACKGROUND_TEST","1");
        check(sonic::input::isolated_gameplay_input(),"isolated fixture was not admitted");
        bool rejected=false;try{NativePortPlatformServices conflicting(config);}
        catch(const NativePortPlatformError& e){rejected=e.failure()==NativePortPlatformFailure::InvalidConfig;}
        check(rejected,"isolated fixture admitted recording");
        config.input_record_path.clear();
        {
            NativePortPlatformServices platform(config);
            const auto first=platform.poll_gamepads();
            check(first.poll_sequence==1 && first.connection_generation==0,"isolated first sequence");
            for(const auto& pad:first.gamepads)check(!pad.connected && pad.buttons==0 && pad.left_stick_y_raw==0,"isolated snapshot is not neutral");
            // Policy is captured per platform. A later environment change
            // cannot reintroduce hardware queries halfway through a sample.
            _putenv_s("SARECOMP_BENCHMARK_ISOLATED_INPUT","0");
            for(unsigned i=0;i<24;++i)check(sonic::input::poll_host(platform).poll_sequence==1,"isolated host poll advanced sequence");
            check(platform.poll_gamepads().poll_sequence==2 && platform.snapshot().input_polls==2,"isolated guest counter");
        }
        std::cout<<"SONIC_INPUT_POLL_TEST_OK actual_platform joystick_query_orders=both success_and_failures replay_cursor recording_count sequence scope_cleanup isolated_probe\n";return 0;
    }catch(const std::exception& error){std::cerr<<"SONIC_INPUT_POLL_TEST_FAIL "<<error.what()<<'\n';return 1;}
}
