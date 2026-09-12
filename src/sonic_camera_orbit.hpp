#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace sonic::camera {
struct Vec3 {
    float x=0, y=0, z=0;
    friend Vec3 operator+(Vec3 a,Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
    friend Vec3 operator-(Vec3 a,Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
};
inline bool finite(Vec3 v) noexcept { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
inline float length(Vec3 v) noexcept { return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z); }
struct Stick { float x=0, y=0; };
inline Stick right_stick(std::int16_t x,std::int16_t y,bool connected) noexcept {
    if (!connected) return {};
    const auto axis=[](std::int16_t raw) {
        constexpr float deadzone=0.12f;
        const float value=std::clamp(float(raw)/32767.0f,-1.0f,1.0f);
        return std::copysign(std::max(0.0f,std::abs(value)-deadzone)/(1-deadzone),value);
    };
    // Each axis keeps its own quiet zone. A purely radial deadzone admits
    // vertical center noise as soon as the player turns horizontally.
    const float fx=axis(x),fy=axis(y),magnitude=std::max(1.0f,std::hypot(fx,fy));
    return {fx/magnitude,fy/magnitude};
}
// PAL 01A680/01A718: positive pitch looks upwards; eye is below the target.
// NINJA uses 65536 angle units per full revolution.
inline constexpr float angle_units=10430.380859375f;
inline constexpr float pi=std::numbers::pi_v<float>;
inline std::int32_t angle(float radians) noexcept {
    return static_cast<std::int32_t>(std::trunc(std::remainder(radians,2*pi)*angle_units));
}
struct Pose { Vec3 eye{},target{}; float yaw=0,pitch=0; };
class Orbit final {
public:
    // Enter from the original position, then keep an independent orbit.
    // Stage camera volumes must not continually overwrite the user's yaw.
    bool enter(Vec3 eye,Vec3 target) noexcept {
        if (!finite(eye)||!finite(target)) return false;
        const auto offset=eye-target;
        const float distance=length(offset);
        if (!std::isfinite(distance)) return false;
        if (distance<1.0f || distance>2000.0f) return false;
        radius_=distance;
        yaw_=std::atan2(offset.x,offset.z);
        pitch_=std::atan2(-offset.y,std::hypot(offset.x,offset.z));
        active_=true;
        return true;
    }
    Pose update(Vec3 target,Stick stick,float seconds) noexcept {
        const float dt=std::isfinite(seconds)?std::clamp(seconds,0.0f,0.25f):0.0f;
        // Positive right stick turns the view right around the fixed target.
        yaw_=std::remainder(yaw_-stick.x*(240.0f*pi/180.0f)*dt,2*pi);
        // Positive raw XInput Y moves the camera UP around the character.
        if (stick.y!=0) pitch_=std::clamp(pitch_-stick.y*(120.0f*pi/180.0f)*dt,minimum_pitch,maximum_pitch);
        const float horizontal=radius_*std::cos(pitch_);
        return {target+Vec3{horizontal*std::sin(yaw_),-radius_*std::sin(pitch_),horizontal*std::cos(yaw_)},target,yaw_,pitch_};
    }
    bool active() const noexcept { return active_; }
    bool return_to(Vec3 original_eye,Vec3 target,float seconds) noexcept {
        const auto offset=original_eye-target;
        const float distance=length(offset);
        if (!finite(original_eye) || distance<1.0f) return false;
        const float desired_yaw=std::atan2(offset.x,offset.z);
        const float desired_pitch=std::atan2(-offset.y,std::hypot(offset.x,offset.z));
        const float weight=1-std::exp(-8*std::clamp(seconds,0.0f,0.25f));
        yaw_=std::remainder(yaw_+std::remainder(desired_yaw-yaw_,2*pi)*weight,2*pi);
        pitch_+=(desired_pitch-pitch_)*weight;radius_+=(distance-radius_)*weight;
        return length(update(target,{},0).eye-original_eye)<0.15f;
    }
    void reset() noexcept { active_=false; }
    float radius() const noexcept { return radius_; }
    static constexpr float minimum_pitch=-75.0f*pi/180.0f;
    // Keep the lowest orbit above the character's feet at the maximum radius.
    // This is a pitch limit, not a substitute for scene/wall collision.
    static constexpr float maximum_pitch=5.0f*pi/180.0f;
private:
    bool active_=false;
    float yaw_=0,pitch_=0,radius_=40;
};
}
