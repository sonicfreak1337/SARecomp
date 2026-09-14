#include "renderer/sonic_vulkan_present.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace sonic::rendering::presentation_policy;
void require(bool yes,const char* reason){if(!yes)throw std::runtime_error(reason);}
int main(){try{
    unsigned cases=0;
    for(std::size_t index=0;index<3;++index)for(bool current:{false,true})for(bool busy:{false,true}) {
        const auto expected=(index+(current?1:0))%3;
        unsigned submitted=0,started=0;bool live=current;
        const auto ready=[&](std::size_t slot){require(slot==expected,"wrong successor slot");return !busy;};
        const auto submit=[&]{++submitted;live=false;};
        const auto start=[&](bool optional){require(optional,"optional path became blocking");++started;return true;};
        const auto result=prepare(true,index,current,3,ready,submit,start);
        require(result==!busy,"busy admission");
        require(submitted==unsigned(!busy)&&started==unsigned(!busy),"busy repeat submitted or waited");
        if(busy)require(live==current,"busy repeat changed real frame prefix");
        ++cases;
    }
    unsigned rebuilt=0;
    require(!swapchain(true,true,[&]{++rebuilt;} )&&rebuilt==0,"optional repeat rebuilt swapchain");
    require(swapchain(true,false,[&]{++rebuilt;} )&&rebuilt==1,"real lifecycle failed to rebuild");
    require(swapchain(false,true,[&]{++rebuilt;} )&&rebuilt==1,"clean swapchain rebuilt");cases+=3;
    bool waiting=false;
    require(prepare(false,0,true,3,[](auto){throw std::runtime_error("required frame probed optional gate");return false;},
                    []{},[&](bool optional){waiting=!optional;return true;})&&waiting,"required work dropped");++cases;
    require(!prepare(true,0,false,3,[](auto){return true;},[]{},[](bool optional){return !optional;}),"late zero-time timeout counted as ready");++cases;
    require(outcome(VK_SUCCESS)==Outcome::Presented,"success");
    require(outcome(VK_SUBOPTIMAL_KHR)==Outcome::PresentedSuboptimal,"suboptimal");
    require(outcome(VK_ERROR_OUT_OF_DATE_KHR)==Outcome::OutOfDate,"rejected presentation counted");
    require(outcome(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)==Outcome::ExclusiveLost,"exclusive loss");
    require(outcome(VK_ERROR_DEVICE_LOST)==Outcome::Error,"device loss hidden");cases+=5;
    std::cout<<"SONIC_VULKAN_PRESENT_TESTS_OK cases="<<cases<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
