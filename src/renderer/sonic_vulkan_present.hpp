#pragma once
#include <vulkan/vulkan_core.h>
#include <cstddef>

namespace sonic::rendering::presentation_policy {
// The current prefix may contain real game commands. Check the successor
// before submitting it, so a busy optional repeat cannot consume a slot.
template<class Ready,class Submit,class Start>
bool prepare(bool nonblocking,std::size_t index,bool current,std::size_t slots,
             Ready ready,Submit submit,Start start) {
    const auto next=(index+(current?1u:0u))%slots;
    if(nonblocking && !ready(next))return false;
    submit();return start(nonblocking);
}
template<class Rebuild>
bool swapchain(bool dirty,bool nonblocking,Rebuild rebuild) {
    if(!dirty)return true;
    if(nonblocking)return false;
    rebuild();return true;
}
enum class Outcome { Presented,PresentedSuboptimal,OutOfDate,ExclusiveLost,Error };
inline Outcome outcome(VkResult result) noexcept {
    switch(result) {
    case VK_SUCCESS:return Outcome::Presented;
    case VK_SUBOPTIMAL_KHR:return Outcome::PresentedSuboptimal;
    case VK_ERROR_OUT_OF_DATE_KHR:return Outcome::OutOfDate;
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:return Outcome::ExclusiveLost;
    default:return Outcome::Error;
    }
}
}
