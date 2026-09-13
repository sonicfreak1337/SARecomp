#include "sonic_corner_indices.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using Vertex=std::array<std::uint32_t,19>;
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
Vertex authored(std::size_t polygon,std::size_t corner){
    Vertex v{};
    for(unsigned i=0;i<v.size();++i)v[i]=static_cast<unsigned>(polygon*100000+corner*37+i);
    v[0]=static_cast<unsigned>(corner%7); // shared points with distinct UV seams
    return v;
}
int main()try{
    unsigned cases=0;
    for(const bool reverse:{false,true})for(unsigned count:{4u,5u,16u,127u,512u})
    for(unsigned clip_mode=0;clip_mode<4;++clip_mode){
        sonic::geometry::CornerIndices stream;std::vector<Vertex> vertices,expected;
        stream.begin_mesh();
        for(unsigned polygon=0;polygon<3;++polygon){
            stream.begin_polygon(count);
            for(unsigned t=0;t+2<count;++t){
                const bool flip=reverse!=(t%2==0);
                const std::array<std::size_t,3> corners{t+(flip?1u:0u),t+(flip?0u:1u),t+2u};
                if(clip_mode && t%7==2){
                    // Clipping itself stays unchanged. Exercise interleaving
                    // its 0/3/6 expanded outputs with cached authored corners.
                    const auto first=vertices.size();
                    const unsigned output=clip_mode==1?0u:(clip_mode==2?6u:3u);
                    for(unsigned i=0;i<output;++i){
                        auto v=authored(polygon,1000+t*8+i);vertices.push_back(v);expected.push_back(v);
                    }
                    require(stream.append_expanded(first,output,65536),"clipped append");
                }else{
                    require(stream.append_shared(vertices,corners,65536,[&](std::size_t i){
                        vertices.push_back(authored(polygon,corners[i]));return true;}),"shared append");
                    for(auto corner:corners)expected.push_back(authored(polygon,corner));
                }
            }
        }
        require(stream.indices.size()==expected.size(),"triangle count/order");
        for(unsigned i=0;i<expected.size();++i)
            require(stream.indices[i]<vertices.size() && vertices[stream.indices[i]]==expected[i],"expanded vertex differs");
        require(vertices.size()<expected.size(),"no corner reuse");
        stream.begin_mesh();stream.begin_polygon(3);
        require(stream.indices.empty() && std::all_of(stream.corners.begin(),stream.corners.end(),
            [](auto x){return x==sonic::geometry::CornerIndices::missing;}),"stale polygon cache");
        ++cases;
    }
    {
        sonic::geometry::CornerIndices s;std::vector<Vertex> v;s.begin_polygon(3);
        const auto add=[&](std::size_t i){v.push_back(authored(0,i));return true;};
        require(s.append_shared(v,{0,1,2},6,add) && s.append_shared(v,{0,1,2},6,add),"six logical vertices");
        require(v.size()==3 && !s.append_shared(v,{0,1,2},6,add),"unique vertices bypassed logical budget");
        require(!s.append_expanded(3,3,6),"clip output bypassed logical budget");
        s.begin_mesh();s.begin_polygon(3);
        require(!s.append_shared(v,{0,3,2},6,add),"invalid authored corner accepted");
        ++cases;
    }
    std::cout<<"SONIC_CORNER_INDICES_PASS cases="<<cases<<" winding seams clipped_mix stream_reset budget exact_vertices\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
