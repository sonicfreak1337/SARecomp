#pragma once
#include "katana/runtime/fpu.hpp"
#include <algorithm>
#include <array>

namespace sonic::model_math {
using namespace katana::runtime;
// Closed SDK SRT operations shared by semantic native model owners. The
// arithmetic and per-axis rounding scope match the retained SDK. This is
// not a standalone SDK call ABI: each semantic owner publishes its own
// integer registers, T flag and other boundary effects.
struct Transform {
    CpuState& cpu;
    void binary(FpuBinaryOperation op,unsigned src,unsigned dst){fpu_binary(cpu,op,std::uint8_t(src),std::uint8_t(dst));}
    void translate(){
        cpu.fr[7]=0x3F800000u;fpu_transform_vector(cpu,4u);
        std::copy_n(cpu.fr.begin()+4,4,cpu.xf.begin()+12);
    }
    void scale(){
        std::copy_n(cpu.xf.begin(),4,cpu.fr.begin());std::copy_n(cpu.xf.begin()+4,4,cpu.fr.begin()+8);
        for(unsigned i=0;i<4u;++i)binary(FpuBinaryOperation::Multiply,4u,i);
        for(unsigned i=8;i<12u;++i)binary(FpuBinaryOperation::Multiply,5u,i);
        std::copy_n(cpu.fr.begin(),4,cpu.xf.begin());std::copy_n(cpu.fr.begin()+8,4,cpu.xf.begin()+4);
        std::copy_n(cpu.xf.begin()+8,4,cpu.fr.begin());
        for(unsigned i=0;i<4u;++i)binary(FpuBinaryOperation::Multiply,6u,i);
        std::copy_n(cpu.fr.begin(),4,cpu.xf.begin()+8);
    }
    void axis(char which,std::uint32_t angle){
        cpu.fpul=angle;if(!angle)return;
        // Same closed FSCA/FTRV epoch as the retained SDK axis basic block.
        // Keep it inside the axis: no new owner-wide rounding boundary.
        const HostFpuExecutionEpoch epoch(cpu);
        cpu.fr[3]=0u;fpu_sine_cosine(cpu,0u);cpu.fr[7]=0u;
        if(which=='x'){
            cpu.fr[4]=0u;cpu.fr[2]=cpu.fr[0];cpu.fr[0]=0u;
            cpu.fr[5]=cpu.fr[2];cpu.fr[6]=cpu.fr[1];fpu_negate(cpu,5u);
        }else if(which=='y'){
            cpu.fr[5]=0u;cpu.fr[2]=cpu.fr[0];cpu.fr[0]=cpu.fr[1];cpu.fr[1]=0u;
            cpu.fr[6]=cpu.fr[0];cpu.fr[4]=cpu.fr[2];fpu_negate(cpu,2u);
        }else{
            cpu.fr[6]=0u;cpu.fr[2]=cpu.fr[0];cpu.fr[0]=cpu.fr[1];cpu.fr[1]=cpu.fr[2];
            cpu.fr[2]=0u;cpu.fr[5]=cpu.fr[0];cpu.fr[4]=cpu.fr[1];fpu_negate(cpu,4u);
        }
        fpu_transform_vector(cpu,0u);fpu_transform_vector(cpu,4u);
        const unsigned first=which=='x'?4u:0u,second=which=='z'?4u:8u;
        std::copy_n(cpu.fr.begin(),4,cpu.xf.begin()+first);std::copy_n(cpu.fr.begin()+4,4,cpu.xf.begin()+second);
    }
    void rotate(const std::array<std::uint32_t,3>& angles){
        cpu.r[6]=angles[1];cpu.r[7]=angles[2];
        axis('z',angles[2]);axis('y',angles[1]);axis('x',angles[0]);
    }
    void rotate_yxz(const std::array<std::uint32_t,3>& angles){
        cpu.r[6]=angles[1];cpu.r[7]=angles[2];
        axis('y',angles[1]);axis('x',angles[0]);axis('z',angles[2]);
    }
};
}
