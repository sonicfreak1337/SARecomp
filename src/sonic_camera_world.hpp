#pragma once
#include "sonic_camera_collision.hpp"
#include "katana/runtime/memory.hpp"
#include <bit>
#include <stdexcept>
#include <unordered_map>

namespace sonic::camera {
// Read only the PAL title's registered world collision, not draw submissions.
// This reader owns no guest pointers between calls and never installs an SDK
// memory observer (doing so would change the retained execution contract).
class CollisionWorld {
    using Memory=katana::runtime::Memory;
    struct Reader {
        Memory& memory;
        katana::runtime::DirectLinearMemoryGuard guard;
        explicit Reader(Memory& m):memory(m),guard(m.direct_linear_memory_guard(false)) {}
        static void range(std::uint32_t address,std::size_t size,unsigned alignment=4) {
            const auto p=address&0x1fffffffu;
            if ((address&(alignment-1)) || p<0x0c010000u || p>=0x0d000000u || size>0x0d000000u-p)
                throw std::runtime_error("camera collision address");
        }
        std::uint32_t word(std::uint32_t address) {
            range(address,4);std::uint32_t value;
            if (katana::runtime::direct_linear_guard_read_u32(guard,address|0x80000000u,value)) return value;
            return memory.read_u32(address&0x1fffffffu);
        }
        std::uint16_t half(std::uint32_t address) {
            range(address,2,2);std::uint16_t value;
            if (katana::runtime::direct_linear_guard_read_u16(guard,address|0x80000000u,value)) return value;
            return memory.read_u16(address&0x1fffffffu);
        }
        float number(std::uint32_t address) {
            const auto value=std::bit_cast<float>(word(address));
            if (!std::isfinite(value) || std::abs(value)>1e7f) throw std::runtime_error("camera collision coordinate");
            return value;
        }
        Vec3 vector(std::uint32_t address) {return {number(address),number(address+4),number(address+8)};}
    };
    struct Transform {
        std::array<std::uint32_t,11> words;
        Vec3 point(Vec3 p) const {
            const auto flags=words[0];
            if (!(flags&4)) p={p.x*std::bit_cast<float>(words[8]),p.y*std::bit_cast<float>(words[9]),p.z*std::bit_cast<float>(words[10])};
            if (!(flags&2)) {
                // Collision owner 02BCDC..02BD32 builds T * Rz * Ry * Rx * S.
                const float x=float(words[5]&0xffffu)/angle_units,y=float(words[6]&0xffffu)/angle_units,z=float(words[7]&0xffffu)/angle_units;
                const float sx=std::sin(x),cx=std::cos(x),sy=std::sin(y),cy=std::cos(y),sz=std::sin(z),cz=std::cos(z);
                p={p.x,cx*p.y-sx*p.z,sx*p.y+cx*p.z};
                p={cy*p.x+sy*p.z,p.y,-sy*p.x+cy*p.z};
                p={cz*p.x-sz*p.y,sz*p.x+cz*p.y,p.z};
            }
            if (!(flags&1)) p=p+Vec3{std::bit_cast<float>(words[2]),std::bit_cast<float>(words[3]),std::bit_cast<float>(words[4])};
            if (!finite(p)) throw std::runtime_error("camera collision transform");
            return p;
        }
        float scale() const {
            if (words[0]&4) return 1;
            return std::max({std::abs(std::bit_cast<float>(words[8])),std::abs(std::bit_cast<float>(words[9])),std::abs(std::bit_cast<float>(words[10]))});
        }
    };
    struct Cached { std::vector<std::uint32_t> source; CollisionMesh mesh; std::uint64_t seen=0; };
    std::unordered_map<std::uint32_t,Cached> cache_;
    std::vector<std::uint32_t> snapshot_;
    std::vector<Vec3> points_;
    std::vector<std::array<std::uint16_t,3>> indices_;
    std::vector<std::uint16_t> polygon_;
    std::uint64_t serial_=0;
    std::uint32_t root_=0;
    static constexpr unsigned max_triangles=1'000'000;
    static bool solid(std::uint32_t flags) {return (flags&1u) && !(flags&0x200u);}
    Transform transform(Reader& reader,std::uint32_t object) {
        Transform result;Reader::range(object,44);
        for (unsigned i=0;i<11;++i) result.words[i]=reader.word(object+i*4);
        return result;
    }
    const CollisionMesh& model(Reader& reader,std::uint32_t object,const Transform& transform) {
        const auto model=transform.words[1];Reader::range(model,40);
        snapshot_.assign(transform.words.begin(),transform.words.end());
        std::array<std::uint32_t,10> header;
        for (unsigned i=0;i<10;++i) {header[i]=reader.word(model+i*4);snapshot_.push_back(header[i]);}
        const auto count=header[2],mesh_count=header[5]&0xffffu;
        if (count>65536 || mesh_count>1024) throw std::runtime_error("camera collision model count");
        if (count) Reader::range(header[0],std::size_t(count)*12);
        if (mesh_count) Reader::range(header[3],std::size_t(mesh_count)*24);
        points_.clear();indices_.clear();points_.reserve(count);
        for (unsigned i=0;i<count;++i) {
            const auto a=reader.word(header[0]+i*12),b=reader.word(header[0]+i*12+4),c=reader.word(header[0]+i*12+8);
            snapshot_.insert(snapshot_.end(),{a,b,c});
            const Vec3 p{std::bit_cast<float>(a),std::bit_cast<float>(b),std::bit_cast<float>(c)};
            if (!finite(p)) throw std::runtime_error("camera collision point");
            points_.push_back(p);
        }
        unsigned corners=0;
        for (unsigned i=0;i<mesh_count;++i) {
            const auto mesh=header[3]+i*24,type_count=reader.word(mesh),stream=reader.word(mesh+4);
            snapshot_.insert(snapshot_.end(),{type_count,stream});
            const auto type=(type_count&0xffffu)>>14,count=type_count>>16;
            auto address=stream;
            for (unsigned p=0;p<count;++p) {
                unsigned n=type==0?3:type==1?4:0;
                if (!n) {const auto h=reader.half(address);snapshot_.push_back(h);address+=2;n=h&0x7fffu;}
                if (n<3 || n>32767 || corners+n>max_triangles*3) throw std::runtime_error("camera collision polygon count");
                Reader::range(address,std::size_t(n)*2,2);corners+=n;polygon_.clear();
                for (unsigned j=0;j<n;++j) {
                    const auto index=reader.half(address);address+=2;snapshot_.push_back(index);
                    if (index>=points_.size()) throw std::runtime_error("camera collision vertex index");
                    polygon_.push_back(index);
                }
                if (indices_.size()+n-2>max_triangles) throw std::runtime_error("camera collision triangle count");
                // Triangle/quad and signed strips follow the Basic decoder;
                // camera collision is two-sided, so winding is immaterial.
                for (unsigned j=0;j+2<n;++j) indices_.push_back({polygon_[j],polygon_[j+1],polygon_[j+2]});
            }
        }
        auto& entry=cache_[object];entry.seen=serial_;
        // Compare actual geometry bytes, not only an address/hash: loaded
        // modules and mutable platforms can reuse the same NJS_MODEL pointer.
        if (entry.source!=snapshot_) {
            for (auto& p:points_) p=transform.point(p);
            std::vector<Triangle> triangles;triangles.reserve(indices_.size());
            for (const auto& index:indices_) triangles.push_back({points_[index[0]],points_[index[1]],points_[index[2]]});
            entry.mesh.assign(std::move(triangles));entry.source=snapshot_;++rebuilt_;
        }
        return entry.mesh;
    }
    unsigned rebuilt_=0;
public:
    struct Result { bool valid=false;float fraction=1;unsigned tested=0,objects=0,rebuilt=0;std::uint32_t hit_object=0; };
    void reset() {cache_.clear();root_=0;}
    Result sweep(Memory& memory,Vec3 start,Vec3 end,float radius) noexcept {
        Result result;
        try {
            Reader reader(memory);++serial_;rebuilt_=0;
            const auto root=reader.word(0x8C759634u);
            if (root!=root_) {reset();root_=root;}
            const auto test=[&](std::uint32_t object,const Transform& transform) {
                const auto hit=model(reader,object,transform).sweep(start,end,radius,result.fraction);
                result.tested+=hit.triangles;++result.objects;
                if (hit.fraction<result.fraction) {result.fraction=hit.fraction;result.hit_object=object;}
            };
            if (root && reader.word(0x8C19E8B8u)==1) {
                const auto count=reader.half(root);
                const auto colliders=reader.word(root+12);
                if (count>8192) throw std::runtime_error("camera land count");
                if (count) Reader::range(colliders,std::size_t(count)*36);
                for (unsigned i=0;i<count;++i) {
                    const auto col=colliders+i*36;
                    if (!solid(reader.word(col+32))) continue;
                    const auto center=reader.vector(col);const auto bound=reader.number(col+12);
                    if (bound<0) throw std::runtime_error("camera land bound");
                    if (!sphere_near_segment(center,bound+radius,start,end)) continue;
                    const auto object=reader.word(col+24);test(object,transform(reader,object));
                }
            }
            const auto count=reader.half(0x8C759644u);
            if (count>256) throw std::runtime_error("camera dynamic count");
            for (unsigned i=0;i<count;++i) {
                const auto record=0x8C759648u+i*12;
                if (!solid(reader.word(record))) continue;
                const auto object=reader.word(record+4);
                const auto t=transform(reader,object);
                const auto center=t.point(reader.vector(t.words[1]+24));
                const float bound=reader.number(t.words[1]+36)*t.scale();
                if (!std::isfinite(bound) || bound<0) throw std::runtime_error("camera dynamic bound");
                if (sphere_near_segment(center,bound+radius,start,end)) test(object,t);
            }
            // Only retain recently queried objects. A disconnected platform
            // cannot keep colliding just because its mesh is cached.
            std::erase_if(cache_,[&](const auto& pair){return pair.second.seen+120<serial_;});
            result.valid=true;result.rebuilt=rebuilt_;return result;
        } catch (...) {reset();return {};}
    }
};
}
