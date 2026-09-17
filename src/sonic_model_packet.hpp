#pragma once
#include "katana/runtime/native_port_graphics.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <xmmintrin.h>

namespace sonic::model_packet {
// A port-local semantic submission. No guest address or producer scratch
// survives capture. Queued draws retain their exact immutable generation.
inline bool enabled() noexcept {
    static const bool value=[] { const auto* p=std::getenv("SARECOMP_NATIVE_MODEL_PACKETS");
        return p && std::string_view(p)=="1"; }();
    return value;
}
using Vec3=std::array<float,3>;
struct Corner { std::uint32_t point; std::array<float,2> uv; };
class Geometry final {
public:
    const std::uint32_t point_count;
    const std::vector<Corner> corners;
    const std::vector<std::uint32_t> indices;
    static std::shared_ptr<const Geometry> capture(std::uint32_t count,
        std::span<const Corner> vertices,std::span<const std::uint32_t> elements) {
        if(!count || count>65536u || vertices.empty() || vertices.size()>65536u ||
           elements.empty() || elements.size()>65536u || elements.size()%3u)
            throw std::invalid_argument("model-geometry-size");
        for(const auto& v:vertices)
            if(v.point>=count || !std::isfinite(v.uv[0]) || !std::isfinite(v.uv[1]))
                throw std::invalid_argument("model-geometry-corner");
        for(auto i:elements)if(i>=vertices.size())throw std::invalid_argument("model-geometry-index");
        return std::shared_ptr<const Geometry>(new Geometry(count,vertices,elements));
    }
    std::size_t bytes() const noexcept {
        return sizeof(*this)+corners.capacity()*sizeof(Corner)+indices.capacity()*sizeof(std::uint32_t);
    }
private:
    Geometry(std::uint32_t n,std::span<const Corner> v,std::span<const std::uint32_t> i)
        :point_count(n),corners(v.begin(),v.end()),indices(i.begin(),i.end()){}
};
class Attributes final {
public:
    const std::vector<Vec3> points,normals;
    const std::vector<std::uint32_t> primary,secondary;
    static std::shared_ptr<const Attributes> capture(std::span<const Vec3> p,
        std::span<const Vec3> n,std::span<const std::uint32_t> a,std::span<const std::uint32_t> b) {
        if(p.empty() || p.size()>65536u || (!n.empty() && n.size()!=p.size()) ||
           a.size()!=p.size() || b.size()!=p.size())throw std::invalid_argument("model-attributes-size");
        const auto finite=[](const Vec3& v){return std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);};
        if(!std::ranges::all_of(p,finite)||!std::ranges::all_of(n,finite))
            throw std::invalid_argument("model-attributes-finite");
        return std::shared_ptr<const Attributes>(new Attributes(p,n,a,b));
    }
    std::size_t bytes() const noexcept {
        return sizeof(*this)+(points.capacity()+normals.capacity())*sizeof(Vec3)+
            (primary.capacity()+secondary.capacity())*sizeof(std::uint32_t);
    }
private:
    Attributes(std::span<const Vec3> p,std::span<const Vec3> n,
        std::span<const std::uint32_t> a,std::span<const std::uint32_t> b)
        :points(p.begin(),p.end()),normals(n.begin(),n.end()),primary(a.begin(),a.end()),secondary(b.begin(),b.end()){}
};
struct Draw {
    std::shared_ptr<const Geometry> geometry;
    std::shared_ptr<const Attributes> attributes;
    bool ignore_light=false,use_secondary=false,vertex_fog=false;
    std::uint32_t host_mode=0;
    explicit operator bool() const noexcept {return bool(geometry)&&bool(attributes);}
    bool valid() const noexcept {
        return bool(*this) && geometry->point_count==attributes->points.size() && !(host_mode&~0xE040u);
    }
    std::size_t expanded_bytes() const noexcept {
        return geometry->corners.size()*sizeof(katana::runtime::NativePortVertex)+geometry->indices.size()*4u;
    }
    std::size_t retained_bytes() const noexcept {return sizeof(*this)+geometry->bytes()+attributes->bytes();}
    // Charge sharing conservatively so neither title nor render queue limits
    // can be bypassed by an empty serialized vertex span.
    std::size_t budget_bytes() const noexcept {return std::max(expanded_bytes(),retained_bytes());}
};
inline void expand(const Draw& draw,std::vector<katana::runtime::NativePortVertex>& output) {
    if(!draw.valid())throw std::invalid_argument("model-packet-invalid");
    output.resize(draw.geometry->corners.size());
    struct Rounding {
        unsigned saved=_mm_getcsr();
        explicit Rounding(unsigned mode){_mm_setcsr(0x1F80u|mode);}
        ~Rounding(){_mm_setcsr(saved);}
    } rounding(draw.host_mode);
    const auto color=[](std::uint32_t v)->std::array<float,4> {
        constexpr float scale=1.0f/255.0f;
        return {float((v>>16u)&255u)*scale,float((v>>8u)&255u)*scale,
                float(v&255u)*scale,float(v>>24u)*scale};
    };
    const auto& a=*draw.attributes;
    for(std::size_t i=0;i<output.size();++i){
        const auto& c=draw.geometry->corners[i];
        auto& v=output[i];v=katana::runtime::NativePortVertex{};
        v.position=a.points[c.point];v.texture_coordinate=c.uv;
        v.color=color(draw.ignore_light?0xFFFFFFFFu:a.primary[c.point]|0xFF000000u);
        if(!draw.ignore_light&&!a.normals.empty())v.normal=a.normals[c.point];
        if(draw.use_secondary){
            v.secondary_color=color(draw.ignore_light?0u:a.secondary[c.point]);
            if(draw.vertex_fog)v.fog_coordinate=v.secondary_color[3];
        }
    }
}
// Only the immediate producer call borrows this pointer. The facade retains
// Draw's owning references before returning, including in serial mode.
inline thread_local const Draw* submitted=nullptr;
class Submission final {
    const Draw* previous;
public:
    explicit Submission(const Draw* draw) noexcept :previous(submitted){submitted=draw;}
    ~Submission(){submitted=previous;}
    Submission(const Submission&)=delete;
    Submission& operator=(const Submission&)=delete;
};
struct Annotation { std::uint64_t ordinal; Draw draw; };
}
