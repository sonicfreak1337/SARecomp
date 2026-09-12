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
    constexpr float deadzone=8689.0f/32767.0f;
    const float fx=std::clamp(float(x)/32767.0f,-1.0f,1.0f);
    const float fy=std::clamp(float(y)/32767.0f,-1.0f,1.0f);
    const float magnitude=std::hypot(fx,fy);
    if (magnitude<=deadzone) return {};
    const float weight=(std::min(magnitude,1.0f)-deadzone)/((1.0f-deadzone)*magnitude);
    return {fx*weight,fy*weight};
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
        radius_=std::clamp(distance,24.0f,65.0f);
        yaw_=std::atan2(offset.x,offset.z);
        pitch_=std::clamp(std::atan2(-offset.y,std::hypot(offset.x,offset.z)),minimum_pitch,maximum_pitch);
        active_=true;
        return true;
    }
    Pose update(Vec3 target,Stick stick,float seconds) noexcept {
        const float dt=std::isfinite(seconds)?std::clamp(seconds,0.0f,0.25f):0.0f;
        // Positive right stick turns the view right around the fixed target.
        yaw_=std::remainder(yaw_-stick.x*(150.0f*pi/180.0f)*dt,2*pi);
        pitch_=std::clamp(pitch_+stick.y*(90.0f*pi/180.0f)*dt,minimum_pitch,maximum_pitch);
        const float horizontal=radius_*std::cos(pitch_);
        return {target+Vec3{horizontal*std::sin(yaw_),-radius_*std::sin(pitch_),horizontal*std::cos(yaw_)},target,yaw_,pitch_};
    }
    bool active() const noexcept { return active_; }
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
