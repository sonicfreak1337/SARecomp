#pragma once
#include "katana/runtime/native_port_graphics.hpp"
#include "sonic_motion_view.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <limits>
#include <unordered_map>
#include <vector>

namespace sonic::motion {
using namespace katana::runtime;

// Port-local metadata travels beside the immutable SDK command stream. It is
// deliberately separate from diagnostics and never changes draw admission.
struct FrameTag {
    std::array<std::uint64_t,8> identity{};
    std::uint64_t sequence=0, time_ns=0;
    std::array<float,3> eye{},player{};
    std::array<std::uint16_t,3> angles{};
    CameraFrame camera;
    bool enabled=false;
};
struct DrawTag {
    std::array<std::uint64_t,4> identity{};
    bool enabled=false;
    bool world=false;
};
inline thread_local FrameTag submitted_frame;
inline thread_local DrawTag submitted_draw;
inline std::atomic<std::uint64_t> timeline{1};
// Linked component tests only; the game never sets this. GPU readback can
// exceed a simulation interval, so image assertions need an explicit clock.
inline std::atomic<std::uint64_t> test_time_ns{0};
inline bool tracing() noexcept {
    static const bool enabled=[] {const auto* value=std::getenv("SARECOMP_MOTION_TRACE");return value&&std::string_view(value)=="1";}();
    return enabled;
}
template<class T> class Submission {
    T& target_; T previous_;
public:
    Submission(T& target,const T& value):target_(target),previous_(target){target_=value;}
    ~Submission(){target_=previous_;}
    Submission(const Submission&)=delete;
};
struct Annotation {
    std::uint64_t ordinal=0;
    FrameTag frame;
    DrawTag draw;
};

inline bool close_positions(const std::array<float,3>& a,const std::array<float,3>& b,
                            float maximum) noexcept {
    double distance=0;
    for(unsigned i=0;i<3;++i){
        if(!std::isfinite(a[i])||!std::isfinite(b[i]))return false;
        const double d=double(a[i])-b[i];distance+=d*d;
    }
    return distance<=double(maximum)*maximum;
}
inline bool continuous(const FrameTag& a,const FrameTag& b) noexcept {
    if(!a.enabled||!b.enabled||a.identity!=b.identity||b.sequence<=a.sequence||
       b.time_ns<=a.time_ns||b.time_ns-a.time_ns>100'000'000||
       !close_positions(a.eye,b.eye,150)||!close_positions(a.player,b.player,150))return false;
    for(unsigned i=0;i<3;++i){
        const auto delta=std::uint16_t(b.angles[i]-a.angles[i]);
        if(std::min<unsigned>(delta,65536u-delta)>8192u)return false;
    }
    return true;
}

struct Draw {
    NativePortDrawPacket packet;
    std::vector<NativePortVertex> vertices;
    std::vector<std::uint32_t> indices;
    DrawTag tag;
    NativePortMatrix4x4 previous_transform,previous_normal;
    bool flush=false,matched=false;
    const char* rejection="no-history";
    NativePortDrawPacket view() const noexcept {
        auto result=packet;
        result.vertices=vertices;result.indices=indices;
        return result;
    }
};
struct Frame {
    NativePortFrameConfig config;
    FrameTag tag;
    std::vector<Draw> draws;
    std::size_t bytes=0;
    CameraFrame previous_camera;
    bool camera_motion=false;
};
struct IdentityHash {
    std::size_t operator()(const std::array<std::uint64_t,4>& key)const noexcept{
        std::uint64_t hash=1469598103934665603ull;
        for(auto word:key){hash^=word;hash*=1099511628211ull;}
        return std::size_t(hash);
    }
};

inline const char* match_problem(const Draw& a,const Draw& b) noexcept {
    const auto& p=a.packet;const auto& q=b.packet;
    if(p.mesh!=q.mesh)return "mesh-handle";
    if(p.vertex_space!=NativePortVertexSpace::ObjectHomogeneous||p.vertex_space!=q.vertex_space)return "vertex-space";
    if(p.topology!=q.topology||a.indices!=b.indices||a.vertices.size()!=b.vertices.size())return "topology";
    if(p.texture!=q.texture||p.texture_stage!=q.texture_stage||p.depth_mapping.mode!=q.depth_mapping.mode||
       p.viewport!=q.viewport||p.draw_class!=q.draw_class||p.translucency!=q.translucency||
       p.rasterizer!=q.rasterizer||p.sampler!=q.sampler||p.blend!=q.blend||p.depth!=q.depth||
       p.type2_autosort!=q.type2_autosort||
       p.material.diffuse!=q.material.diffuse||p.material.texture_combine!=q.material.texture_combine||
       p.material.use_primary_alpha!=q.material.use_primary_alpha||
       p.material.use_texture_alpha!=q.material.use_texture_alpha||
       p.material.use_secondary_color!=q.material.use_secondary_color)return "state";
    for(std::size_t i=0;i<a.vertices.size();++i){
        const auto& v=a.vertices[i];const auto& w=b.vertices[i];
        // Unproven topology/deformation is never vertex-index matched. Only
        // immutable local geometry gets a changing object/view transform.
        if(v.position!=w.position||v.position_w!=w.position_w)return "deformation";
        if(v.normal!=w.normal)return "normal";
        if(v.texture_coordinate!=w.texture_coordinate)return "uv";
    }
    for(unsigned i=0;i<16;++i)
        if(!std::isfinite(p.transform.values[i])||!std::isfinite(q.transform.values[i])||
           !std::isfinite(p.normal_transform.values[i])||!std::isfinite(q.normal_transform.values[i]))return "nonfinite";
    // Bound per-object cuts as well as the frame's camera/player cuts. The
    // affine model/view translation is in title units, before projection.
    const auto& x=p.normal_transform.values;const auto& y=q.normal_transform.values;
    if(!close_positions({x[12],x[13],x[14]},{y[12],y[13],y[14]},150))return "translation-cut";
    for(unsigned row=0;row<3;++row){
        double dot=0,aa=0,bb=0;
        for(unsigned col=0;col<3;++col){const auto k=row*4+col;dot+=double(x[k])*y[k];aa+=double(x[k])*x[k];bb+=double(y[k])*y[k];}
        if(aa<1e-12||bb<1e-12||aa>bb*4||bb>aa*4||dot<0.70710678*std::sqrt(aa*bb))return "rotation-cut";
    }
    return nullptr;
}
inline bool same_geometry(const Draw& a,const Draw& b) noexcept {return !match_problem(a,b);}

// Own every transient span until the last output using it. GPU handles remain
// generation-bound; the caller must invalidate before update/destroy commands.
// No resource, guest state, clock or input is created/advanced by this class.
class History {
    Frame building_,current_;
    std::uint64_t started_=0,period_=0;
    std::size_t matched_=0;
    std::array<std::uint64_t,8> suspended_identity_{};
    bool suspended_=false;
    std::size_t world_draws_=0,rejected_world_draws_=0;
public:
    static constexpr std::size_t maximum_frame_bytes=32u*1024u*1024u;
    static constexpr std::size_t maximum_draws=32768;
    bool accepts(const FrameTag& tag) noexcept {
        if(suspended_&&suspended_identity_!=tag.identity)suspended_=false;
        return tag.enabled&&!suspended_;
    }
    void begin(const NativePortFrameConfig& config,const FrameTag& tag){
        building_={};building_.config=config;building_.tag=tag;
    }
    bool add(const NativePortDrawPacket& packet,const DrawTag& tag){
        const auto bytes=sizeof(Draw)+packet.vertices.size_bytes()+packet.indices.size_bytes();
        if(building_.draws.size()>=maximum_draws||bytes>maximum_frame_bytes||
           building_.bytes>maximum_frame_bytes-bytes)return false;
        Draw draw;draw.packet=packet;draw.packet.vertices={};draw.packet.indices={};draw.tag=tag;
        draw.vertices.assign(packet.vertices.begin(),packet.vertices.end());
        draw.indices.assign(packet.indices.begin(),packet.indices.end());
        building_.draws.push_back(std::move(draw));building_.bytes+=bytes;return true;
    }
    bool flush(){
        if(building_.draws.size()>=maximum_draws||building_.bytes>maximum_frame_bytes-sizeof(Draw))return false;
        Draw draw;draw.flush=true;building_.draws.push_back(std::move(draw));building_.bytes+=sizeof(Draw);return true;
    }
    const Frame& building()const noexcept{return building_;}
    const Frame& current()const noexcept{return current_;}
    std::size_t matched()const noexcept{return matched_;}
    std::size_t rejected_world_draws()const noexcept{return rejected_world_draws_;}
    std::size_t world_draws()const noexcept{return world_draws_;}
    bool animated()const noexcept{return matched_||current_.camera_motion;}
    void invalidate()noexcept{current_={};started_=period_=0;matched_=0;}
    void abort()noexcept{invalidate();building_={};}
    void commit(std::uint64_t now){
        matched_=0;
        world_draws_=rejected_world_draws_=0;
        const bool connect=accepts(building_.tag)&&continuous(current_.tag,building_.tag)&&
            current_.config.clear_color==building_.config.clear_color&&
            current_.config.clear_depth==building_.config.clear_depth&&
            current_.config.depth_buffer==building_.config.depth_buffer&&
            (!started_||(now>=started_&&now-started_<=100'000'000));
        period_=connect?building_.tag.time_ns-current_.tag.time_ns:0;
        if(connect){
            constexpr auto duplicate=std::numeric_limits<std::size_t>::max();
            using Index=std::unordered_map<std::array<std::uint64_t,4>,std::size_t,IdentityHash>;
            const auto index=[](const Frame& frame){
                Index result;result.reserve(frame.draws.size());
                for(std::size_t i=0;i<frame.draws.size();++i)if(frame.draws[i].tag.enabled&&!frame.draws[i].flush){
                    auto [it,inserted]=result.emplace(frame.draws[i].tag.identity,i);
                    if(!inserted)it->second=std::numeric_limits<std::size_t>::max();
                }
                return result;
            };
            const auto previous=index(current_),next=index(building_);
            for(auto& draw:building_.draws){
                if(draw.flush)continue;
                if(!draw.tag.enabled){draw.rejection="no-owner";continue;}
                const auto a=previous.find(draw.tag.identity),b=next.find(draw.tag.identity);
                if(a==previous.end()||b==next.end()){draw.rejection="new-owner";continue;}
                if(a->second==duplicate||b->second==duplicate){draw.rejection="duplicate-owner";continue;}
                const auto& old=current_.draws[a->second];
                if(const auto problem=match_problem(old,draw)){draw.rejection=problem;continue;}
                draw.previous_transform=old.packet.transform;draw.previous_normal=old.packet.normal_transform;
                draw.matched=true;++matched_;
            }
        }
        building_.camera_motion=connect&&camera_continuous(current_.tag.camera,building_.tag.camera);
        if(building_.camera_motion){
            for(auto& draw:building_.draws)if(!draw.flush&&(draw.tag.enabled||draw.tag.world)){
                if(const auto problem=camera_draw_problem(draw.view(),current_.tag.camera,building_.tag.camera)){
                    draw.rejection=problem;building_.camera_motion=false;
                }
            }
            if(building_.camera_motion)building_.previous_camera=current_.tag.camera;
        }
        if(building_.camera_motion){
            // Interpolate an owner's whole pose or none of it. A shadow,
            // deformation, unmatched part or changing skeletal basis must
            // never leave only the other meshes one simulation frame behind.
            struct OwnerPose {bool valid=true,has_delta=false;Point delta{};};
            std::unordered_map<std::array<std::uint64_t,4>,OwnerPose,IdentityHash> poses;
            const auto owner=[](const Draw& d){return std::array<std::uint64_t,4>{d.tag.identity[0],d.tag.identity[3],0,0};};
            for(const auto& d:building_.draws)if(!d.flush&&(d.tag.enabled||d.tag.world)&&d.tag.identity[0]){
                auto& pose=poses[owner(d)];
                if(!d.matched){pose.valid=false;continue;}
                const auto old=product(d.previous_normal,current_.tag.camera.inverse_view);
                const auto now=product(d.packet.normal_transform,building_.tag.camera.inverse_view);
                // Only a common rigid translation is proven here. Rotation
                // interpolation needs a complete hierarchy/pose representation.
                for(unsigned i=0;i<12;++i)if(std::abs(old.values[i]-now.values[i])>
                    .00005f*(1+std::max(std::abs(old.values[i]),std::abs(now.values[i]))))pose.valid=false;
                Point delta{};for(unsigned i=0;i<3;++i)delta[i]=old.values[12+i]-now.values[12+i];
                if(pose.has_delta&&!close_positions(pose.delta,delta,.005f))pose.valid=false;
                pose.delta=delta;pose.has_delta=true;
            }
            for(auto& d:building_.draws)if(d.matched){
                const auto found=poses.find(owner(d));
                if(found==poses.end()||!found->second.valid){d.matched=false;d.rejection="owner-pose";--matched_;}
            }
        }
        for(const auto& draw:building_.draws)if(!draw.flush&&(draw.tag.enabled||draw.tag.world)){
            ++world_draws_;if(!draw.matched&&!building_.camera_motion)++rejected_world_draws_;
        }
        if(connect&&rejected_world_draws_&&tracing()){
            struct Bucket {const Draw* draw;std::size_t count;};
            std::vector<Bucket> groups;
            for(const auto& draw:building_.draws)if(!draw.flush&&!draw.matched&&(draw.tag.enabled||draw.tag.world)){
                auto found=std::find_if(groups.begin(),groups.end(),[&](const auto& group){
                    const auto& other=*group.draw;
                    return std::string_view(draw.rejection)==other.rejection&&
                        draw.tag.identity[1]==other.tag.identity[1]&&draw.tag.identity[2]==other.tag.identity[2]&&
                        draw.packet.diagnostics.origin_identity==other.packet.diagnostics.origin_identity;
                });
                if(found!=groups.end())++found->count;
                else groups.push_back({&draw,1});
            }
            std::sort(groups.begin(),groups.end(),[](const auto& a,const auto& b){return a.count>b.count;});
            for(std::size_t i=0;i<std::min<std::size_t>(groups.size(),48);++i){
                const auto& group=groups[i];const auto& draw=*group.draw;const auto& p=draw.packet;
                std::fprintf(stderr,"SONIC_MOTION_REJECT frame=%llu reason=%s count=%zu task=%llx model=%llx mesh=%llx origin=%llx vertices=%zu persistent=%llu space=%u\n",
                    static_cast<unsigned long long>(building_.tag.sequence),draw.rejection,group.count,
                    static_cast<unsigned long long>(draw.tag.identity[0]),static_cast<unsigned long long>(draw.tag.identity[1]),
                    static_cast<unsigned long long>(draw.tag.identity[2]),static_cast<unsigned long long>(p.diagnostics.origin_identity),
                    draw.vertices.size(),static_cast<unsigned long long>(p.mesh.value),unsigned(p.vertex_space));
            }
        }
        // Camera transforms are shared scene state. Mixing old/new transforms
        // only for recognized meshes tears a moving scene apart (including
        // terrain seams and shadows). An incomplete world has ONE current pose.
        // Latch the fallback for this camera/scene epoch to avoid alternating
        // one-frame delay as objects enter/leave the source's visibility set.
        if(rejected_world_draws_){
            for(auto& draw:building_.draws)draw.matched=false;
            matched_=0;period_=0;
            if(connect){suspended_=true;suspended_identity_=building_.tag.identity;}
        }
        current_=std::move(building_);building_={};started_=now;
    }
    float alpha(std::uint64_t now)const noexcept{
        if(!period_||now<started_)return 1;
        return std::clamp(float(double(now-started_)/double(period_)),0.0f,1.0f);
    }
    bool can_render()const noexcept{return !current_.draws.empty();}
    template<class DrawFn,class FlushFn>
    static void render(const Frame& frame,float alpha,DrawFn&& draw,FlushFn&& flush){
        alpha=std::isfinite(alpha)?std::clamp(alpha,0.0f,1.0f):1.0f;
        Matrix view,delta;
        if(frame.camera_motion){
            view=camera_pose(frame.previous_camera,frame.tag.camera,alpha);
            delta=product(frame.tag.camera.inverse_view,view);
        }
        std::vector<NativePortVertex> projected;
        for(const auto& entry:frame.draws){
            if(entry.flush){flush();continue;}
            auto packet=entry.view();
            if(frame.camera_motion&&alpha<1&&(entry.tag.enabled||entry.tag.world)){
                if(packet.vertex_space==NativePortVertexSpace::ObjectHomogeneous){
                    auto world=product(packet.normal_transform,frame.tag.camera.inverse_view);
                    if(entry.matched){
                        const auto previous_world=product(entry.previous_normal,frame.previous_camera.inverse_view);
                        for(unsigned i=0;i<16;++i)world.values[i]=std::lerp(previous_world.values[i],world.values[i],alpha);
                    }
                    packet.normal_transform=product(world,view);
                    packet.transform=product(packet.normal_transform,frame.tag.camera.projection);
                }else{
                    projected.assign(packet.vertices.begin(),packet.vertices.end());
                    for(auto& vertex:projected)reproject(vertex,frame.tag.camera,delta);
                    packet.vertices=projected;
                }
            }else if(entry.matched&&alpha<1){
                for(unsigned i=0;i<16;++i){
                    packet.transform.values[i]=std::lerp(entry.previous_transform.values[i],packet.transform.values[i],alpha);
                    packet.normal_transform.values[i]=std::lerp(entry.previous_normal.values[i],packet.normal_transform.values[i],alpha);
                }
            }
            draw(packet);
        }
    }
};
}
