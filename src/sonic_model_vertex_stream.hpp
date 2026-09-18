#pragma once
#include "sonic_model_packet.hpp"
#include <cstddef>
#include <atomic>
#include <type_traits>

namespace sonic::model_vertex_stream {
inline std::atomic<std::uint64_t> gpu_draws{},point_uploads{},corner_uploads{};
// Port-owned model ABI. Corners retain authored point indices and UV seams;
// these four float4 records are uploaded once per live model/rounding mode.
// The GPU only gathers already computed attributes: no guest projection,
// palette publication, material decision or arithmetic epoch is discarded.
struct alignas(16) Point final {
    std::array<float,4> position{},normal{},primary{},secondary{};
};
static_assert(sizeof(Point)==model_packet::stream_point_bytes && std::is_trivially_copyable_v<Point>);
static_assert(offsetof(Point,normal)==16 && offsetof(Point,primary)==32 &&
              offsetof(Point,secondary)==48);
struct alignas(16) Constants final {
    // ignore lighting, use secondary, vertex fog, float4 base (D3D arena).
    std::array<std::uint32_t,4> flags{};
    std::array<float,4> white{};
};
static_assert(sizeof(Constants)==model_packet::stream_constants_bytes && offsetof(Constants,white)==16);
inline bool enabled() noexcept {
    return model_packet::vertex_stream_enabled();
}
namespace detail {
struct Rounding final {
    unsigned saved=_mm_getcsr();
    explicit Rounding(unsigned mode){
        if(mode&~0xE040u)throw std::invalid_argument("model-stream-rounding");
        _mm_setcsr(0x1F80u|mode);
    }
    ~Rounding(){_mm_setcsr(saved);}
};
constexpr std::array<float,4> color(std::uint32_t value) noexcept {
    constexpr float scale=1.0f/255.0f;
    return {float((value>>16u)&255u)*scale,float((value>>8u)&255u)*scale,
            float(value&255u)*scale,float(value>>24u)*scale};
}
}
inline void prepare_points(const model_packet::Attributes& attributes,unsigned mode,
                           std::vector<Point>& output) {
    detail::Rounding rounding(mode);
    output.resize(attributes.points.size());
    for(std::size_t i=0;i<output.size();++i){
        auto& point=output[i];const auto& position=attributes.points[i];
        point.position={position[0],position[1],position[2],1.0f};
        point.normal={0.0f,0.0f,1.0f,0.0f};
        if(!attributes.normals.empty()){
            const auto& normal=attributes.normals[i];
            point.normal={normal[0],normal[1],normal[2],0.0f};
        }
        point.primary=detail::color(attributes.primary[i]|0xFF000000u);
        point.secondary=detail::color(attributes.secondary[i]);
    }
}
inline Constants constants(const model_packet::Draw& draw,std::uint32_t base=0) {
    if(!draw.valid())throw std::invalid_argument("model-stream-draw");
    detail::Rounding rounding(draw.host_mode);
    // Keep the retained runtime select before channel conversion. Folding an
    // unconditional all-white literal changes RGB by one ULP under Linux's
    // upward host mode; opaque alpha is still the authored constant one.
    const auto selected=draw.ignore_light?0xFFFFFFFFu:draw.attributes->primary.front()|0xFF000000u;
    return {{std::uint32_t(draw.ignore_light),std::uint32_t(draw.use_secondary),
             std::uint32_t(draw.vertex_fog),base},detail::color(selected)};
}
// CPU image/vertex oracle for the vertex-shader gather. This is deliberately
// separate from model_packet::expand, which remains the retained reference.
inline katana::runtime::NativePortVertex gather(const Point& point,
        const model_packet::Corner& corner,const Constants& constants) noexcept {
    katana::runtime::NativePortVertex vertex;
    vertex.position={point.position[0],point.position[1],point.position[2]};
    vertex.texture_coordinate=corner.uv;
    const bool ignore=constants.flags[0]!=0;
    vertex.color=ignore?constants.white:point.primary;
    if(!ignore)vertex.normal={point.normal[0],point.normal[1],point.normal[2]};
    if(constants.flags[1]){
        if(!ignore)vertex.secondary_color=point.secondary;
        if(constants.flags[2])vertex.fog_coordinate=vertex.secondary_color[3];
    }
    return vertex;
}
}
