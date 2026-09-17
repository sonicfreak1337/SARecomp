#include "sonic_palette_batch.hpp"
#include "katana/runtime/fpu.hpp"
#include <array>
#include <bit>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace katana::runtime;
void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main()try{
    CpuState cpu{.memory=Memory{0u}};
    sonic::palette_batch::Result out;
    std::mt19937 random(0x37350);
    std::uint64_t vertices=0,cases=0;
    std::array<std::uint32_t,387> normals{};
    const std::array special{0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x00800000u,
        0x80800000u,0x3f800000u,0xbf800000u,0x41800000u,0xc1800000u};
    for(unsigned mode=0;mode<4;++mode)for(unsigned sample=0;sample<500;++sample){
        const unsigned count=std::array{2u,3u,4u,5u,7u,16u,127u,129u}[sample%8];
        const auto flags=fpscr_dn_mask|fpscr_flag_inexact_mask|(mode&1)|
            ((mode&2)?fpscr_fr_mask:0u)|fpscr_flag_invalid_mask|fpscr_cause_mask;
        cpu.write_fpscr(flags);
        std::array<std::uint32_t,4> light{};
        for(unsigned i=0;i<3;++i)light[i]=sample<100?special[(sample+i)%special.size()]:
            std::bit_cast<std::uint32_t>(float(int(random()%8193)-4096)/512.f);
        for(unsigned i=0;i<count*3;++i){
            if(sample<100)normals[i]=special[(sample+i*3)%special.size()];
            else {auto bits=random();bits=(bits&0x807fffffu)|((1u+random()%130u)<<23);
                normals[i]=bits;}
        }
        auto scale=std::bit_cast<std::uint32_t>(std::array{1.f,127.5f,256.f,-1.f,-63.25f,-256.f}[sample%6]);
        const auto before=cpu.read_fpscr();
        require(sonic::palette_batch::prepare(cpu,reinterpret_cast<const std::uint8_t*>(normals.data()),
            count,light.data(),scale,out),"ordinary batch declined");
        require(cpu.read_fpscr()==before,"prepare changed guest FPSCR");
        const HostFpuExecutionEpoch epoch(cpu);
        for(unsigned i=0;i<count;++i){
            for(unsigned j=0;j<4;++j)cpu.fr[12+j]=light[j];
            for(unsigned j=0;j<3;++j)cpu.fr[j]=normals[i*3+j];
            cpu.fr[3]=0x3f800000u;cpu.fr[7]=scale;
            fpu_inner_product(cpu,12,0);
            fpu_binary(cpu,FpuBinaryOperation::Multiply,7,3);
            fpu_binary(cpu,FpuBinaryOperation::Add,7,3);
            fpu_truncate_to_fpul(cpu,3);
            if(cpu.fr[3]!=out.scaled[i] || cpu.fpul!=out.integers[i]){
                std::cerr<<"mode="<<mode<<" sample="<<sample<<" vertex="<<i<<" ref="
                    <<std::hex<<cpu.fr[3]<<" got="<<out.scaled[i]<<'\n';
                throw std::runtime_error("palette vector or integer differs");
            }
            require(cpu.read_fpscr()==(before&~fpscr_cause_mask),"unexpected sticky flag change");
            ++vertices;
        }
        ++cases;
    }
    unsigned rejections=0;
    for(unsigned test=0;test<12;++test){
        cpu.write_fpscr(fpscr_dn_mask|fpscr_flag_inexact_mask);
        std::array<std::uint32_t,4> light{0x3f800000,0,0,0};
        normals.fill(0);std::uint32_t scale=0x42ff0000u;std::size_t count=7;
        switch(test){
            case 0:cpu.fpscr&=~fpscr_flag_inexact_mask;break;
            case 1:cpu.fpscr&=~fpscr_dn_mask;break;
            case 2:cpu.fpscr|=fpscr_pr_mask;break;
            case 3:cpu.fpscr|=fpscr_enable_inexact_mask;break;
            case 4:cpu.fpscr|=2;break;
            case 5:scale=0x3f000000;break;
            case 6:scale=0x43800001;break;
            case 7:light[1]=0x7f800000;break;
            case 8:normals[9]=0x7fc00000;break;
            case 9:count=1;break;
            case 10:count=65537;break;
            case 11:light[3]=0x80000000;break;
        }
        const auto before=cpu.read_fpscr();const auto previous=out.scaled;
        require(!sonic::palette_batch::prepare(cpu,reinterpret_cast<const std::uint8_t*>(normals.data()),
            count,light.data(),scale,out),"exceptional batch accepted");
        require(cpu.read_fpscr()==before && out.scaled==previous,"decline mutated state");
        ++rejections;
    }
    std::cout<<"PALETTE_BATCH_OK cases="<<cases<<" vertices="<<vertices<<" rejections="
        <<rejections<<" floating_bits=exact integer_bits=exact fpscr=exact\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"PALETTE_BATCH_FAIL "<<e.what()<<'\n';return 1;}
