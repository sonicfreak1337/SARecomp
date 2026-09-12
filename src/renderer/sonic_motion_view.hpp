#pragma once
#include "katana/runtime/native_port_graphics.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace sonic::motion {
using Matrix=katana::runtime::NativePortMatrix4x4;
using Point=std::array<float,3>;
inline Matrix product(const Matrix& a,const Matrix& b) noexcept {
    Matrix out;out.values.fill(0);
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
        for(unsigned k=0;k<4;++k)out.values[r*4+c]+=a.values[r*4+k]*b.values[k*4+c];
    return out;
}
inline Point point(const Point& p,const Matrix& m) noexcept {
    Point result{};
    for(unsigned c=0;c<3;++c)result[c]=p[0]*m.values[c]+p[1]*m.values[4+c]+p[2]*m.values[8+c]+m.values[12+c];
    return result;
}
inline float distance(const Point& a,const Point& b={}) noexcept {
    return std::hypot(a[0]-b[0],a[1]-b[1],a[2]-b[2]);
}
inline Matrix rigid_inverse(const Matrix& m) noexcept {
    Matrix result;
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)result.values[4*r+c]=m.values[4*c+r];
    for(unsigned c=0;c<3;++c)
        result.values[12+c]=-(m.values[12]*result.values[c]+m.values[13]*result.values[4+c]+m.values[14]*result.values[8+c]);
    return result;
}
struct CameraFrame {
    Matrix view,inverse_view,projection,screen;
    Point eye{};
    // Original logical-raster center X/Y, horizontal/vertical projection, near.
    std::array<float,5> raster{};
    bool valid=false;
};
inline CameraFrame camera_frame(Matrix view,const std::array<float,5>& raster,
                                float horizontal_scale,const Point& expected_eye) noexcept {
    CameraFrame result;
    for(unsigned r=0;r<4;++r)view.values[4*r+3]=r==3?1.0f:0.0f;
    for(auto v:view.values)if(!std::isfinite(v))return result;
    for(auto v:raster)if(!std::isfinite(v))return result;
    if(!std::isfinite(horizontal_scale)||horizontal_scale<=0||
       std::abs(raster[2])<1||std::abs(raster[3])<1||raster[4]<=0)return result;
    // A camera is rigid (either fixed handedness), never a model scale.
    for(unsigned r=0;r<3;++r)for(unsigned s=0;s<3;++s){
        float dot=0;for(unsigned c=0;c<3;++c)dot+=view.values[4*r+c]*view.values[4*s+c];
        if(std::abs(dot-(r==s?1.0f:0.0f))>.001f)return result;
    }
    result.view=view;result.inverse_view=rigid_inverse(view);
    result.eye={result.inverse_view.values[12],result.inverse_view.values[13],result.inverse_view.values[14]};
    if(!std::isfinite(distance(result.eye,expected_eye))||distance(result.eye,expected_eye)>.1f)return {};
    result.raster=raster;
    auto& p=result.projection.values;p.fill(0);
    p[0]=2*raster[2]/640*horizontal_scale;p[5]=-2*raster[3]/480;
    p[8]=(2*raster[0]/640-1)*horizontal_scale;p[9]=1-2*raster[1]/480;
    p[10]=p[11]=1;p[14]=-raster[4];
    result.screen.values={2.0f/640*horizontal_scale,0,0,0,0,-2.0f/480,0,0,0,0,1,0,-horizontal_scale,1,0,1};
    result.valid=true;return result;
}
inline bool near_matrix(const Matrix& a,const Matrix& b) noexcept {
    for(unsigned i=0;i<16;++i)if(!std::isfinite(a.values[i])||!std::isfinite(b.values[i])||
        std::abs(a.values[i]-b.values[i])>0.00005f*(1+std::max(std::abs(a.values[i]),std::abs(b.values[i]))))return false;
    return true;
}
inline float camera_angle(const CameraFrame& a,const CameraFrame& b) noexcept {
    float trace=0;
    for(unsigned r=0;r<3;++r)for(unsigned k=0;k<3;++k)trace+=a.view.values[k*4+r]*b.view.values[k*4+r];
    return std::acos(std::clamp((trace-1)*.5f,-1.0f,1.0f));
}
inline bool camera_continuous(const CameraFrame& a,const CameraFrame& b) noexcept {
    return a.valid&&b.valid&&a.raster==b.raster&&a.projection.values==b.projection.values&&
           camera_angle(a,b)<.7853981634f&&distance(a.eye,b.eye)<150;
}
inline Matrix camera_pose(const CameraFrame& a,const CameraFrame& b,float alpha) noexcept {
    if(alpha<=0)return a.view;if(alpha>=1)return b.view;
    // Interpolate the relative proper rotation; this preserves even a
    // left-handed source basis without linearly shrinking its axes.
    auto relative=product(a.inverse_view,b.view);
    const float theta=camera_angle(a,b),s=std::sin(theta);
    Matrix delta;
    if(theta>0.0001f&&std::abs(s)>0.00001f){
        const float x=(relative.values[6]-relative.values[9])/(2*s);
        const float y=(relative.values[8]-relative.values[2])/(2*s);
        const float z=(relative.values[1]-relative.values[4])/(2*s);
        const float c=std::cos(theta*alpha),q=1-c,t=std::sin(theta*alpha);
        delta.values={c+x*x*q,x*y*q+z*t,x*z*q-y*t,0,
                      y*x*q-z*t,c+y*y*q,y*z*q+x*t,0,
                      z*x*q+y*t,z*y*q-x*t,c+z*z*q,0,0,0,0,1};
    }
    auto result=product(a.view,delta);
    Point eye{};for(unsigned i=0;i<3;++i)eye[i]=std::lerp(a.eye[i],b.eye[i],alpha);
    for(unsigned c=0;c<3;++c)result.values[12+c]=-(eye[0]*result.values[c]+eye[1]*result.values[4+c]+eye[2]*result.values[8+c]);
    return result;
}
inline Point raster_point(const katana::runtime::NativePortVertex& vertex,const CameraFrame& camera) noexcept {
    const float z=1/vertex.depth_coordinate;
    return {(vertex.position[0]-camera.raster[0])*z/camera.raster[2],
            (vertex.position[1]-camera.raster[1])*z/camera.raster[3],z};
}
inline const char* camera_draw_problem(const katana::runtime::NativePortDrawPacket& packet,
                                       const CameraFrame& old,const CameraFrame& now) noexcept {
    using namespace katana::runtime;
    if(packet.vertex_space==NativePortVertexSpace::ObjectHomogeneous){
        if(packet.depth_mapping.mode!=NativePortDepthCoordinateMode::ReciprocalPositiveHomogeneousClip||
           !near_matrix(product(packet.normal_transform,now.projection),packet.transform))return "camera-projection";
        return nullptr;
    }
    if(packet.vertex_space!=NativePortVertexSpace::PvrScreenReciprocal||packet.mesh||
       !near_matrix(packet.transform,now.screen))return "camera-vertex-space";
    const float turn=2*std::sin(camera_angle(old,now)*.5f),move=distance(old.eye,now.eye);
    for(const auto& v:packet.vertices){
        if(!std::isfinite(v.depth_coordinate)||v.depth_coordinate<=0||
           std::abs(v.position[2]-v.depth_coordinate)>.00001f*(1+v.depth_coordinate))return "camera-depth";
        const auto p=raster_point(v,now);const float radius=distance(p);
        if(!std::isfinite(radius))return "camera-nonfinite";
        // Bound the full continuous camera arc, not just sampled endpoints.
        // Already clipped source geometry is never extrapolated through near.
        if(p[2]-move-radius*turn<=now.raster[4])return "camera-near";
    }
    return nullptr;
}
inline void reproject(katana::runtime::NativePortVertex& vertex,const CameraFrame& camera,const Matrix& delta) noexcept {
    auto view=point(raster_point(vertex,camera),delta);
    const float previous_depth=vertex.depth_coordinate,depth=1/view[2];
    vertex.position={camera.raster[0]+camera.raster[2]*view[0]*depth,
                     camera.raster[1]+camera.raster[3]*view[1]*depth,depth};
    vertex.depth_coordinate=depth;
    if(vertex.fog_coordinate==previous_depth)vertex.fog_coordinate=depth;
}
}
