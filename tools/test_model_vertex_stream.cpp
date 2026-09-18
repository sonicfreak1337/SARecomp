#include "sonic_model_vertex_stream.hpp"
#include <bit>
#include <cstring>
#include <iostream>
namespace mp=sonic::model_packet;
namespace stream=sonic::model_vertex_stream;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    const unsigned original=_mm_getcsr();
    // Repeated point indices with UV seams, signed zero, small values, no
    // normals and different palettes exercise the real retained expansion.
    const std::array<mp::Vec3,5> positions{{{1,-2,3},{-0.0f,0,1},{.001f,6,-7},
        {1.0e-20f,-1.0e-20f,.5f},{-8,9,10}}};
    const std::array<mp::Vec3,5> normals{{{0,0,1},{.25f,-.5f,.75f},
        {-0.0f,1,0},{-.333f,.666f,1},{1,1,1}}};
    const std::array<unsigned,5> primary{0x00418217u,0xABFFFFFFu,0,0xF10A2B3Cu,0xAA7F80FEu};
    const std::array<unsigned,5> secondary{0x31425364u,0,0xFFFFFFFFu,0xFF010203u,0x87459312u};
    const std::array<mp::Corner,6> corners{{{4,{0,0}},{1,{1,0}},{2,{0,1}},
        {0,{1,1}},{4,{.25f,.75f}},{3,{.5f,-.5f}}}};
    const std::array<unsigned,9> indices{0,1,2,2,1,3,4,3,5};
    const auto geometry=mp::Geometry::capture(positions.size(),corners,indices);
    unsigned cases=0;
    for(unsigned has_normals=0;has_normals<2;++has_normals)
    for(unsigned rounding=0;rounding<4;++rounding)
    for(unsigned denormals=0;denormals<4;++denormals)
    for(unsigned flags=0;flags<8;++flags){
        const auto mode=(rounding<<13u)|((denormals&1u)?0x40u:0u)|((denormals&2u)?0x8000u:0u);
        mp::Draw draw{geometry,mp::Attributes::capture(positions,has_normals?std::span<const mp::Vec3>{normals}:std::span<const mp::Vec3>{},primary,secondary),
            bool(flags&1),bool(flags&2),bool(flags&4),mode};
        std::vector<katana::runtime::NativePortVertex> expected;
        mp::expand(draw,expected);
        std::vector<stream::Point> points;stream::prepare_points(*draw.attributes,mode,points);
        const auto constants=stream::constants(draw,16);
        require(_mm_getcsr()==original,"host mode leaked");
        require(constants.flags[3]==16 && points.size()==positions.size(),"point addressing differs");
        for(std::size_t i=0;i<corners.size();++i){
            const auto actual=stream::gather(points[corners[i].point],corners[i],constants);
            if(std::memcmp(&actual,&expected[i],sizeof(actual))){
                std::cerr<<"case="<<cases<<" corner="<<i<<" mode="<<mode<<" flags="<<flags<<'\n';
                const auto words=sizeof(actual)/sizeof(std::uint32_t);
                const auto old_bits=std::bit_cast<std::array<std::uint32_t,sizeof(actual)/4>>(expected[i]);
                const auto new_bits=std::bit_cast<std::array<std::uint32_t,sizeof(actual)/4>>(actual);
                for(std::size_t k=0;k<words;++k)if(old_bits[k]!=new_bits[k])
                    std::cerr<<"word="<<k<<" expected="<<std::hex<<old_bits[k]<<" actual="<<new_bits[k]<<std::dec<<'\n';
                throw std::runtime_error("retained vertex bytes differ");
            }
        }
        ++cases;
    }
    bool rejected=false;
    try{const stream::detail::Rounding invalid(1);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected && _mm_getcsr()==original,"invalid mode changed host state");
    std::cout<<"MODEL_VERTEX_STREAM_OK exact_vertex_cases="<<cases<<" bytes_per_point="<<sizeof(stream::Point)
             <<" corner_bytes="<<sizeof(mp::Corner)<<" host_mode_preserved=1\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"MODEL_VERTEX_STREAM_FAIL "<<error.what()<<'\n';return 1;}}
