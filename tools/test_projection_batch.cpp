#include "sonic_projection_batch.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <tuple>
#include <xmmintrin.h>
using namespace katana::runtime;
using Op=FpuBinaryOperation;
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
// Retained owner's instruction/helper order: independent scalar oracle, with
// read-ahead and padded pair. Do not reuse candidate algebra or result state.
void reference(CpuState& cpu,unsigned count,std::span<const std::uint8_t> points,
    std::span<std::uint8_t> output,std::span<std::uint8_t> clips){
    const HostFpuExecutionEpoch epoch(cpu);
    auto* source=points.data();
    const auto load=[&](unsigned base){
        for(unsigned axis=0;axis<3;++axis)std::memcpy(&cpu.fr[base+axis],source+axis*4,4);
        cpu.fr[base+3]=0x3f800000;
        if(!try_fpu_transform_vector_simd(cpu,std::uint8_t(base)))fpu_transform_vector(cpu,std::uint8_t(base));
        cpu.fr[base+3]=0x3f800000;fpu_binary(cpu,Op::Divide,std::uint8_t(base+2),std::uint8_t(base+3));
        source+=12;
    };
    unsigned observed=0;
    const auto clip=[&](unsigned z){
        fpu_compare_greater(cpu,14,std::uint8_t(z));
        if(observed<count)clips[observed]=cpu.t?0:1;
        ++observed;if(!cpu.t)++cpu.r[13];
    };
    const auto store=[&](std::size_t offset,std::uint32_t word){std::memcpy(output.data()+offset,&word,4);};
    load(0);unsigned remaining=count;std::size_t offset=0;
    do{
        clip(2);load(8);clip(10);const auto first_depth=cpu.fr[3];
        fpu_binary(cpu,Op::Multiply,6,0);fpu_binary(cpu,Op::Multiply,7,1);
        fpu_binary(cpu,Op::Multiply,3,0);fpu_binary(cpu,Op::Multiply,3,1);
        fpu_binary(cpu,Op::Add,4,0);cpu.fr[3]=0x3f800000;fpu_binary(cpu,Op::Add,5,1);
        fpu_binary(cpu,Op::Multiply,6,8);fpu_binary(cpu,Op::Multiply,7,9);
        const auto x=cpu.fr[0],y=cpu.fr[1];load(0);
        fpu_binary(cpu,Op::Multiply,11,9);fpu_binary(cpu,Op::Multiply,11,8);
        fpu_binary(cpu,Op::Add,5,9);const auto depth=cpu.fr[11];fpu_binary(cpu,Op::Add,4,8);
        store(offset,x);store(offset+4,y);store(offset+8,first_depth);
        store(offset+16,cpu.fr[8]);store(offset+20,cpu.fr[9]);store(offset+24,depth);
        offset+=32;remaining-=2;
    }while(std::bit_cast<std::int32_t>(remaining)>0);
}
auto state(const CpuState& c){return std::tuple(c.r,c.r_bank,c.fr,c.xf,c.fpscr,c.t,c.sr,c.fpul,
    c.pc,c.pr,c.gbr,c.trap_pending,c.sleeping,c.exception_generation,
    c.attempted_guest_instructions,c.retired_guest_instructions,c.total_guest_cycles);}
int main()try{
    std::mt19937 random(0x3729426);sonic::projection_batch::Result result;
    std::uint64_t accepted=0,declined=0,vertices=0;
    for(unsigned test=0;test<4096;++test){
        const unsigned count=std::array{1u,2u,3u,4u,5u,6u,7u,8u,9u,16u,31u,128u,129u}[test%13];
        const auto even=(count+1)&~1u;
        CpuState a{.memory=Memory{0u}},b{.memory=Memory{0u}};
        a.sr=b.sr=sr_md_mask;a.fpscr=b.fpscr=fpscr_dn_mask|fpscr_flag_inexact_mask|
            (test&1u)|((test&2u)?fpscr_fr_mask:0u)|(test&fpscr_flag_mask)|fpscr_cause_mask;
        const auto number=[&](float scale){return std::bit_cast<std::uint32_t>(float(int(random()%8193)-4096)*scale);};
        for(unsigned reg=0;reg<16;++reg){a.r[reg]=b.r[reg]=random();a.fr[reg]=b.fr[reg]=number(1.f);
            a.xf[reg]=b.xf[reg]=number(1.f/4096.f);}
        for(unsigned reg=4;reg<8;++reg)a.fr[reg]=b.fr[reg]=number(1.f/16.f);
        a.fr[14]=b.fr[14]=0xbf800000;
        std::vector<std::uint8_t> input((even+1)*12),oa(even*16,0xa5),ob=oa,ca(count,0xcd),cb=ca;
        for(std::size_t at=0;at<input.size();at+=4){auto word=number(1.f/32.f);
            if(test<128 && at%16==0)word=std::array{0u,0x80000000u,1u,0x807fffffu,0x00800000u}[test%5];
            std::memcpy(input.data()+at,&word,4);}
        const auto before=state(a);
        const auto mxcsr=_mm_getcsr();
        bool ok=false;
        {const HostFpuExecutionEpoch epoch(a);ok=sonic::projection_batch::prepare(a,count,input,result,epoch);
            require(state(a)==before,"prepare mutated guest state");
            if(ok)sonic::projection_batch::publish(a,result,oa,ca);}
        require(_mm_getcsr()==mxcsr,"host FP state changed");
        if(!ok){++declined;require(oa==ob && ca==cb,"decline mutated outputs");continue;}
        reference(b,count,input,ob,cb);
        if(oa!=ob || ca!=cb || state(a)!=state(b)){
            std::cerr<<"case="<<test<<" count="<<count<<std::hex<<" fpscr="<<a.fpscr<<'/'<<b.fpscr
                <<" clip="<<a.r[13]<<'/'<<b.r[13]<<" t="<<a.t<<'/'<<b.t<<'\n';
            for(unsigned i=0;i<16;++i)if(a.fr[i]!=b.fr[i])std::cerr<<"fr"<<i<<'='<<a.fr[i]<<'/'<<b.fr[i]<<'\n';
            for(std::size_t i=0;i<oa.size();i+=4){std::uint32_t x,y;std::memcpy(&x,oa.data()+i,4);std::memcpy(&y,ob.data()+i,4);
                if(x!=y){std::cerr<<"output+"<<i<<'='<<x<<'/'<<y<<'\n';break;}}
            throw std::runtime_error("whole projection differs from retained owner");}
        ++accepted;vertices+=count;
    }
    require(accepted>4000,"batch admission unexpectedly low");
    unsigned rejects=0;
    for(unsigned test=0;test<14;++test){
        CpuState c{.memory=Memory{0u}};c.sr=sr_md_mask;c.fpscr=fpscr_dn_mask|fpscr_flag_inexact_mask;
        c.xf[0]=c.xf[5]=c.xf[10]=c.xf[15]=0x3f800000;c.fr[6]=c.fr[7]=0x43800000;
        std::array<std::uint32_t,9> words{0,0,0xbf800000,0,0,0xbf800000,0,0,0xbf800000};
        switch(test){
        case 0:c.fpscr&=~fpscr_flag_inexact_mask;break;case 1:c.fpscr&=~fpscr_dn_mask;break;
        case 2:c.fpscr|=fpscr_pr_mask;break;case 3:c.fpscr|=fpscr_sz_mask;break;
        case 4:c.fpscr|=fpscr_enable_inexact_mask;break;case 5:c.fpscr|=2;break;
        case 6:c.fr[4]=1;break;case 7:c.fr[14]=0x7f800000;break;case 8:c.xf[15]=0x7fc00000;break;
        case 9:words[2]=0;break;case 10:words[2]=0x35000000;break;case 11:words[0]=0x7fc00000;break;
        case 12:c.sr|=sr_fd_mask;break;case 13:c.trap_pending=true;break;}
        const auto before=state(c);const HostFpuExecutionEpoch epoch(c);
        require(!sonic::projection_batch::prepare(c,1,{reinterpret_cast<const std::uint8_t*>(words.data()),36},result,epoch),
            "exceptional input admitted");require(state(c)==before,"rejection changed guest");++rejects;
    }
    std::cout<<"PROJECTION_BATCH_OK cases="<<accepted<<" declined="<<declined<<" vertices="<<vertices
        <<" rejections="<<rejects<<" output=exact state=exact padding=preserved host=restored\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"PROJECTION_BATCH_FAIL "<<e.what()<<'\n';return 1;}
