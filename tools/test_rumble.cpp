#include "sonic_rumble.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace sonic::rumble;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Output {
    std::mutex mutex;std::condition_variable changed;
    struct Event {unsigned endpoint;std::uint16_t low,high;};std::vector<Event> events;
    static bool send(void* opaque,unsigned endpoint,std::uint16_t low,std::uint16_t high)noexcept {
        auto& self=*static_cast<Output*>(opaque);std::lock_guard lock(self.mutex);
        self.events.push_back({endpoint,low,high});self.changed.notify_all();return true;
    }
    Event last(){std::lock_guard lock(mutex);check(!events.empty(),"no output");return events.back();}
    bool wait_stopped(){std::unique_lock lock(mutex);return changed.wait_for(lock,std::chrono::seconds(1),[&]{return events.size()>=2 && !events.back().low && !events.back().high;});}
};
int main(){try{
    std::array<std::uint8_t,8> record{1,0,3,20,0,0,0,0};
    auto e=decode(record,19);check(e.duration_ms==50 && e.power==3/7.f && e.inclination==0,"one cycle");
    record[2]=255;e=decode(record,19);check(e.power==2/7.f,"signed one normalization");
    record[1]=1;e=decode(record,1);check(e.duration_ms==500,"timeout unit");
    check(decode(record,255).duration_ms==64000,"timeout upper bound");
    record[1]=0;record[3]=0;check(decode(record,19).duration_ms==5000,"zero frequency auto stop");
    record[1]=0x88;record[2]=2;record[3]=20;record[4]=3;e=decode(record,19);
    check(e.duration_ms==300 && e.inclination<0,"INH priority");
    record[1]=8;e=decode(record,19);check(e.duration_ms==300 && e.inclination>0,"EXH slope");
    check(controller(1)==-1 && controller(2)==0 && controller(8)==1 && controller(20)==3 && controller(26)==-1,"VMU accessory isolation");
    Output output;Engine engine;engine.attach(&output,Output::send);engine.bind(0,123,2);engine.policy(false,50);
    check(engine.capable(2) && !engine.capable(1) && !engine.capable(8),"backend capability");
    check(engine.configure(1,20)==-2,"absent device return");
    record={1,0,3,20,0,0,0,0};check(engine.request(2,record)==0,"request");
    auto sent=output.last();check(sent.endpoint==2 && sent.low==sent.high && sent.low==std::lround((3/7.f)*.5f*65535.f),"single strength scale");
    // No frame tick, guest callback or polling occurs while waiting. A slow
    // game must not extend the motor request, including the final zero.
    check(output.wait_stopped(),"independent deadline did not stop");
    engine.configure(2,255);record[1]=1;engine.request(2,record);engine.policy(true,100);
    check(output.last().low==0,"modal/focus stop");
    engine.policy(false,100);check(output.last().low==0,"stale request resumed");
    engine.request(2,record);engine.bind(0,456,3);
    check(output.last().endpoint==2 && !output.last().low,"rebind did not stop old endpoint");
    engine.request(2,record);engine.bind(0,0,-1);check(!engine.capable(2) && !output.last().low,"disconnect stop");
    engine.bind(0,789,1);engine.request(2,record);engine.detach(&output);
    check(!output.last().low && !engine.capable(2),"shutdown stop/capability");
    std::cout<<"PASS PuruPuru packing, timing, backend identity, suppression and independent stop; no hardware\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
