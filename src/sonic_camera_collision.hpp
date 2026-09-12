#pragma once
#include "sonic_camera_orbit.hpp"
#include <array>
#include <span>
#include <vector>

namespace sonic::camera {
inline float dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 scaled(Vec3 v,float s) { return {v.x*s,v.y*s,v.z*s}; }
inline Vec3 cross(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
struct Triangle { Vec3 a,b,c; };
struct Sweep {
    float fraction=1;
    unsigned triangles=0;
};
// Two-sided swept volume, including face, edge and vertex contacts. A center
// ray alone lets the near plane cut through corners and narrow door frames.
class CollisionMesh {
    struct Bounds {
        Vec3 low{INFINITY,INFINITY,INFINITY},high{-INFINITY,-INFINITY,-INFINITY};
        void include(Vec3 p) {
            low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};
            high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};
        }
        bool intersects(Vec3 origin,Vec3 delta,float radius,float end) const {
            float begin=0;
            const std::array o{origin.x,origin.y,origin.z},d{delta.x,delta.y,delta.z};
            const std::array lo{low.x-radius,low.y-radius,low.z-radius},hi{high.x+radius,high.y+radius,high.z+radius};
            for (unsigned i=0;i<3;++i) {
                if (std::abs(d[i])<1e-8f) { if (o[i]<lo[i] || o[i]>hi[i]) return false; }
                else {
                    float a=(lo[i]-o[i])/d[i],b=(hi[i]-o[i])/d[i];
                    if (a>b) std::swap(a,b);
                    begin=std::max(begin,a);end=std::min(end,b);
                    if (begin>end) return false;
                }
            }
            return true;
        }
    };
    struct Node { Bounds box; unsigned start=0,count=0,left=0,right=0; };
    std::vector<Triangle> triangles_;
    std::vector<Node> nodes_;
    unsigned build(unsigned start,unsigned count) {
        const auto index=unsigned(nodes_.size());nodes_.push_back({});
        Bounds bounds,centers;
        for (unsigned i=start;i<start+count;++i) {
            const auto& t=triangles_[i];bounds.include(t.a);bounds.include(t.b);bounds.include(t.c);
            centers.include(scaled(t.a+t.b+t.c,1.0f/3));
        }
        nodes_[index].box=bounds;
        if (count<=8) {nodes_[index].start=start;nodes_[index].count=count;return index;}
        const auto extent=centers.high-centers.low;
        const unsigned axis=extent.x>extent.y && extent.x>extent.z?0:extent.y>extent.z?1:2;
        const auto component=[axis](const Triangle& t) {
            const auto v=t.a+t.b+t.c;return axis==0?v.x:axis==1?v.y:v.z;
        };
        const auto middle=start+count/2;
        std::nth_element(triangles_.begin()+start,triangles_.begin()+middle,triangles_.begin()+start+count,
            [&](const Triangle& a,const Triangle& b){return component(a)<component(b);});
        const auto left=build(start,count/2),right=build(middle,count-count/2);
        nodes_[index].left=left;nodes_[index].right=right;return index;
    }
    static bool inside(Vec3 p,const Triangle& t,Vec3 n) {
        return dot(cross(t.b-t.a,p-t.a),n)>=-1e-5f &&
               dot(cross(t.c-t.b,p-t.b),n)>=-1e-5f &&
               dot(cross(t.a-t.c,p-t.c),n)>=-1e-5f;
    }
    static void root(Vec3 origin,Vec3 velocity,float radius,float& end,
                     Vec3 edge={},float along=0,float along_velocity=0) {
        const double a=dot(velocity,velocity),b=dot(origin,velocity),c=dot(origin,origin)-double(radius)*radius;
        if (a<1e-12) return;
        const double discriminant=b*b-a*c;
        if (discriminant<0) return;
        const auto t=float((-b-std::sqrt(discriminant))/a);
        if (t<0 || t>end) return;
        const float edge_length=dot(edge,edge);
        if (edge_length && (along+along_velocity*t<0 || along+along_velocity*t>edge_length)) return;
        end=t;
    }
    static float contact(Vec3 origin,Vec3 delta,float radius,const Triangle& tri,float end) {
        const auto normal=cross(tri.b-tri.a,tri.c-tri.a);
        const auto n=scaled(normal,1.0f/length(normal));
        const float distance=dot(origin-tri.a,n),speed=dot(delta,n);
        if (std::abs(distance)<=radius && inside(origin-scaled(n,distance),tri,n)) return 0;
        if (std::abs(speed)>1e-8f) {
            for (const float side:{-radius,radius}) {
                const float t=(side-distance)/speed;
                if (t>=0 && t<=end && inside(origin+scaled(delta,t)-scaled(n,side),tri,n)) end=t;
            }
        }
        const std::array points{tri.a,tri.b,tri.c};
        for (unsigned i=0;i<3;++i) {
            const auto relative=origin-points[i],edge=points[(i+1)%3]-points[i];
            const float size=dot(edge,edge),along=dot(relative,edge),along_velocity=dot(delta,edge);
            if (size>1e-10f) {
                const auto nearest=relative-scaled(edge,std::clamp(along/size,0.0f,1.0f));
                if (dot(nearest,nearest)<=radius*radius) return 0;
                root(relative-scaled(edge,along/size),delta-scaled(edge,along_velocity/size),radius,end,edge,along,along_velocity);
            }
            root(relative,delta,radius,end);
        }
        return end;
    }
public:
    void assign(std::vector<Triangle> triangles) {
        triangles_=std::move(triangles);nodes_.clear();
        std::erase_if(triangles_,[](const Triangle& t) {
            return !finite(t.a)||!finite(t.b)||!finite(t.c)||length(cross(t.b-t.a,t.c-t.a))<1e-6f;
        });
        if (!triangles_.empty()) {nodes_.reserve(triangles_.size()*2);build(0,unsigned(triangles_.size()));}
    }
    Sweep sweep(Vec3 origin,Vec3 end,float radius,float limit=1) const {
        Sweep result{limit};
        if (nodes_.empty()) return result;
        const auto delta=end-origin;
        std::array<unsigned,64> stack{};unsigned count=1; // Balanced tree depth is <32 even at the parser cap.
        while (count) {
            const auto& node=nodes_[stack[--count]];
            if (!node.box.intersects(origin,delta,radius,result.fraction)) continue;
            if (node.count) for (unsigned i=node.start;i<node.start+node.count;++i) {
                ++result.triangles;result.fraction=contact(origin,delta,radius,triangles_[i],result.fraction);
                if (result.fraction==0) return result;
            }
            else {stack[count++]=node.left;stack[count++]=node.right;}
        }
        return result;
    }
    std::size_t triangle_count() const {return triangles_.size();}
};
inline bool sphere_near_segment(Vec3 center,float radius,Vec3 a,Vec3 b) {
    const auto delta=b-a;const float size=dot(delta,delta);
    const auto nearest=a+scaled(delta,size>0?std::clamp(dot(center-a,delta)/size,0.0f,1.0f):0);
    return dot(nearest-center,nearest-center)<=radius*radius;
}
class CollisionBoom {
    float distance_=0;
    bool active_=false;
public:
    void reset() { active_=false; }
    float update(float desired,float allowed,float dt) {
        allowed=std::clamp(allowed,0.0f,desired);
        if (!active_ || allowed<distance_) distance_=allowed; // Never smooth into a wall.
        else distance_+= (allowed-distance_)*(1-std::exp(-10*std::max(0.0f,dt)));
        active_=true;return distance_;
    }
};
}
