#include "sonic_contour.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

struct Point { double x,y; };
double cross(Point a,Point b,Point c) {
    return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
}
void require(bool value,const char* why) { if(!value) throw std::runtime_error(why); }
void verify(std::vector<Point> contour) {
    double polygon=0,triangles=0;
    for(std::size_t i=0;i<contour.size();++i) {
        const auto a=contour[i],b=contour[(i+1)%contour.size()];
        polygon+=a.x*b.y-a.y*b.x;
    }
    std::vector<std::size_t> strip;
    for(std::size_t i=0;i<contour.size();++i) {
        const auto index=sonic::contour::strip_index(i,contour.size());
        require(index<contour.size(),"corner outside contour");
        require(std::find(strip.begin(),strip.end(),index)==strip.end(),"duplicate corner");
        strip.push_back(index);
    }
    for(std::size_t i=2;i<strip.size();++i) {
        const auto a=strip[(i&1)?i-1:i-2],b=strip[(i&1)?i-2:i-1],c=strip[i];
        const auto area=cross(contour[a],contour[b],contour[c]);
        // Retail front/back strips wind opposite to the incoming contour.
        require(area*polygon<=0,"crossed or reversed strip triangle");
        triangles+=std::abs(area);
    }
    require(std::abs(triangles-std::abs(polygon))<1e-8,"missing or overlapping polygon area");
}
int main() try {
    // Chao's four-corner billboard; the old sequential-strip code loses one
    // quadrant and draws another twice. Test a near-plane-clipped variant too.
    const std::vector<Point> quad{{-2,-1},{2,-1},{2,1},{-2,1}};
    verify(quad);
    verify({{-1,-1},{2,-1},{2,1},{-2,1},{-2,0}});
    for(unsigned count=3;count<=12;++count) {
        std::vector<Point> shape;
        for(unsigned i=0;i<count;++i) {
            const auto angle=6.283185307179586*double(i)/count;
            shape.push_back({3*std::cos(angle),2*std::sin(angle)});
        }
        verify(shape);std::reverse(shape.begin(),shape.end());verify(shape);
    }
    std::cout<<"SONIC_CONTOUR_OK cases=22 full_area=1 consistent_winding=1 clipped_contour=1\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
