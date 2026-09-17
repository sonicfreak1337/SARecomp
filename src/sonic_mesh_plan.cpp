#include "sonic_mesh_plan.hpp"
#include "sonic_model_uv.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>
#include <xmmintrin.h>

namespace sonic::geometry {
namespace {
constexpr std::size_t max_plan_bytes=512u*1024u;
std::uint16_t u16(const std::uint8_t* p) noexcept {return std::uint16_t(p[0]|(unsigned(p[1])<<8));}
std::uint32_t u32(const std::uint8_t* p) noexcept {return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);}
std::span<const std::uint8_t> span(MeshPlanReader reader,std::uint32_t base,
                                 std::size_t offset,std::size_t size) noexcept {
    if(!reader.bytes || !size || std::uint64_t(base)+offset+size>0x100000000ull)return {};
    const auto bytes=reader.bytes(reader.context,base+std::uint32_t(offset),size);
    return bytes.size()==size?bytes:std::span<const std::uint8_t>{};
}
bool same_source(const MeshPlan& p,MeshPlanReader reader) noexcept {
    const auto stream=span(reader,u32(p.request.descriptor.data()+4),0,p.stream_bytes.size());
    if(stream.size()!=p.stream_bytes.size() || std::memcmp(stream.data(),p.stream_bytes.data(),stream.size()))return false;
    if(p.uv_bytes.empty())return true;
    const auto uv=span(reader,u32(p.request.descriptor.data()+20),0,p.uv_bytes.size());
    return uv.size()==p.uv_bytes.size() && !std::memcmp(uv.data(),p.uv_bytes.data(),uv.size());
}
std::optional<MeshPlan> build(const MeshPlanRequest& request,MeshPlanReader reader,
                            const katana::runtime::CpuState& cpu){
    const auto type=u16(request.descriptor.data())>>14;
    const auto count=u16(request.descriptor.data()+2);
    const auto stream=u32(request.descriptor.data()+4),uv=u32(request.descriptor.data()+20);
    if(!count || !request.point_count || !request.corner_budget ||
       request.corner_budget>65536u || count>request.corner_budget/3u)return {};
    MeshPlan plan;plan.request=request;
    plan.fpscr_mode=cpu.read_fpscr()&0x001C0003u;
    plan.host_mode=_mm_getcsr()&0xE040u;
    std::size_t offset=0;
    for(unsigned polygon=0;polygon<count;++polygon){
        unsigned corners=type==0u?3u:4u;bool reversed=false;
        if(type>=2u){
            const auto header=span(reader,stream,offset,2);
            if(header.empty())return {};
            const auto word=u16(header.data());offset+=2;corners=word&0x7FFFu;reversed=(word&0x8000u)!=0u;
        }
        if(corners<3u || corners>request.corner_budget-plan.corner_count)return {};
        const auto indices=span(reader,stream,offset,std::size_t(corners)*2);
        if(indices.empty())return {};
        const auto first=std::uint32_t(plan.triangles.size()),base=plan.corner_count;
        const auto add=[&](unsigned a,unsigned b,unsigned c){
            MeshPlanTriangle t{{u16(indices.data()+a*2),u16(indices.data()+b*2),u16(indices.data()+c*2)},
                               {base+a,base+b,base+c}};
            if(std::ranges::any_of(t.points,[&](unsigned point){return point>=request.point_count;}))return false;
            plan.triangles.push_back(t);return true;
        };
        if(type==0u){if(!add(0,1,2))return {};}
        else if(type==1u){if(!add(0,1,2) || !add(2,1,3))return {};}
        else{
            bool flip=!reversed;
            for(unsigned corner=0;corner+2<corners;++corner){
                flip=!flip;
                if(!add(flip?corner+1:corner,flip?corner:corner+1,corner+2))return {};
            }
        }
        plan.polygons.push_back({first,std::uint32_t(plan.triangles.size())-first,base,corners});
        plan.corner_count+=corners;offset+=std::size_t(corners)*2;
        if(plan.allocation_bytes()>max_plan_bytes)return {};
    }
    const auto raw=span(reader,stream,0,offset);
    if(raw.empty())return {};
    plan.stream_bytes.assign(raw.begin(),raw.end());
    if(uv){
        const auto raw_uv=span(reader,uv,0,std::size_t(plan.corner_count)*4);
        if(raw_uv.empty())return {};
        plan.uv_bytes.assign(raw_uv.begin(),raw_uv.end());
        plan.uvs.reserve(plan.corner_count);
        for(std::size_t i=0;i<plan.corner_count;++i){
            const auto* p=plan.uv_bytes.data()+i*4;
            plan.uvs.push_back(sonic::model_uv::decode(std::bit_cast<std::int16_t>(u16(p)),
                std::bit_cast<std::int16_t>(u16(p+2)),true,cpu));
        }
    }
    // Match raw UV words, not approximate positions or sampled texture
    // values. A different point or either side of a UV seam stays distinct.
    // Every authored corner belongs to at least one generated triangle.
    constexpr auto missing=std::numeric_limits<std::uint32_t>::max();
    plan.shared_corners.assign(plan.corner_count,missing);
    std::unordered_map<std::uint64_t,std::uint32_t> identities;
    identities.reserve(plan.corner_count);
    for(const auto& triangle:plan.triangles)for(unsigned i=0;i<3u;++i){
        const auto corner=triangle.corners[i];
        if(plan.shared_corners[corner]!=missing)continue;
        const auto uv_bits=uv?u32(plan.uv_bytes.data()+std::size_t(corner)*4u):0u;
        const auto key=(std::uint64_t(triangle.points[i])<<32u)|uv_bits;
        const auto [found,inserted]=identities.emplace(key,plan.shared_corner_count);
        if(inserted){
            ++plan.shared_corner_count;
            plan.shared_vertices.push_back({triangle.points[i],corner});
        }
        plan.shared_corners[corner]=found->second;
    }
    plan.shared_indices.reserve(plan.triangles.size()*3u);
    for(const auto& triangle:plan.triangles)for(const auto corner:triangle.corners)
        plan.shared_indices.push_back(plan.shared_corners[corner]);
    if(sonic::model_packet::enabled() && plan.shared_indices.size()<=65536u){
        std::vector<sonic::model_packet::Corner> corners;
        corners.reserve(plan.shared_vertices.size());
        for(const auto& c:plan.shared_vertices)
            corners.push_back({c.point,uv?plan.uvs[c.corner]:std::array<float,2>{}});
        plan.model_geometry=sonic::model_packet::Geometry::capture(request.point_count,corners,plan.shared_indices);
    }
    if(plan.allocation_bytes()>max_plan_bytes)return {};
    return plan;
}
}
std::size_t MeshPlan::allocation_bytes()const noexcept {
    return sizeof(MeshPlan)+(model_geometry?model_geometry->bytes():0u)+stream_bytes.capacity()+uv_bytes.capacity()+
        triangles.capacity()*sizeof(MeshPlanTriangle)+polygons.capacity()*sizeof(MeshPlanPolygon)+uvs.capacity()*sizeof(uvs[0])+
        shared_corners.capacity()*sizeof(shared_corners[0])+
        shared_vertices.capacity()*sizeof(shared_vertices[0])+shared_indices.capacity()*sizeof(shared_indices[0]);
}
void MeshPlanCache::erase(std::unordered_map<std::uint64_t,Entry>::iterator it) noexcept {
    bytes_-=it->second.plan.allocation_bytes();ages_.erase(it->second.age);entries_.erase(it);
}
void MeshPlanCache::clear() noexcept {entries_.clear();ages_.clear();bytes_=0;}
MeshPlanCache::MeshPlanCache(MeshPlanCache&& other) noexcept
    :MeshPlanCache(other.byte_limit_,other.entry_limit_){*this=std::move(other);}
MeshPlanCache& MeshPlanCache::operator=(MeshPlanCache&& other) noexcept {
    if(this!=&other){
        clear();entries_.swap(other.entries_);ages_.swap(other.ages_);
        byte_limit_=other.byte_limit_;entry_limit_=other.entry_limit_;
        bytes_=std::exchange(other.bytes_,0u);stats=std::exchange(other.stats,{});
    }
    return *this;
}
const MeshPlan* MeshPlanCache::get(const MeshPlanRequest& request,MeshPlanReader reader,
                                const katana::runtime::CpuState& cpu) noexcept {
    ++stats.lookups;
    // A cache hit must not suppress a host FP exception that the original UV
    // multiply could raise. Unsupported ambient masks retain the old path.
    if((_mm_getcsr()&0x1F80u)!=0x1F80u){++stats.declines;return nullptr;}
    try{
        const auto key=(std::uint64_t(request.model)<<32)|request.mesh;
        auto found=entries_.find(key);
        if(found!=entries_.end()){
            const auto& plan=found->second.plan;
            if(plan.request==request && plan.fpscr_mode==(cpu.read_fpscr()&0x001C0003u) &&
               plan.host_mode==(_mm_getcsr()&0xE040u) && same_source(plan,reader)){
                ages_.splice(ages_.begin(),ages_,found->second.age);++stats.hits;return &plan;
            }
            ++stats.changes;erase(found);
        }
        ++stats.misses;
        auto plan=build(request,reader,cpu);
        if(!plan || !entry_limit_ || plan->allocation_bytes()>byte_limit_){++stats.declines;return nullptr;}
        const auto size=plan->allocation_bytes();
        while(!ages_.empty() && (entries_.size()>=entry_limit_ || bytes_>byte_limit_-size)){
            erase(entries_.find(ages_.back()));++stats.evictions;
        }
        ages_.push_front(key);
        try{found=entries_.emplace(key,Entry{std::move(*plan),ages_.begin()}).first;}
        catch(...){ages_.pop_front();throw;}
        bytes_+=size;++stats.builds;return &found->second.plan;
    }catch(...){++stats.declines;return nullptr;}
}
}
