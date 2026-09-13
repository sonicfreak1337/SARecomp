#include "sonic_atan_math.hpp"
#include "sonic_fpu_body.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include <array>
#include <bit>
#include <cstring>
namespace sonic::atan_math {
namespace {
using namespace katana::runtime;
// Exact PAL source and read-only data words. Per-span SHA identities are public.
constexpr std::array<std::uint16_t,240> atan_words{
    0x2FE6u,0x2FD6u,0xFFFBu,0xFFEBu,0xFFDBu,0xFFCBu,0x4F22u,0x7FF4u,0xE008u,0xF54Cu,0xFF57u,0x53F2u,
    0x4311u,0x8D01u,0xE400u,0xE408u,0xF38Du,0xF534u,0xE704u,0x8F02u,0xE601u,0xA00Cu,0x247Bu,0x52F2u,
    0xD357u,0xD158u,0x2239u,0x3210u,0x8B02u,0xE002u,0xA003u,0x240Bu,0xF554u,0x8900u,0x246Bu,0x63F3u,
    0xD253u,0xFF4Au,0x6132u,0x2128u,0x8D03u,0x6543u,0xF3F8u,0xF34Du,0xFF3Au,0x2648u,0x8902u,0xD24Fu,
    0xA08Cu,0xF528u,0x62F3u,0x6122u,0xD34Au,0xE2E9u,0xE020u,0x2139u,0x412Cu,0x7181u,0x3103u,0xED08u,
    0x8F08u,0x2D49u,0xD148u,0x2DD8u,0x8F02u,0xF518u,0xA07Au,0x0009u,0xA078u,0xF54Du,0x2478u,0x8901u,
    0xA074u,0xF54Cu,0xD143u,0xF3F8u,0xF518u,0xD343u,0xF535u,0x8F04u,0xF438u,0xD242u,0xFE28u,0xA00Fu,
    0xFF3Cu,0xF534u,0x8B04u,0x2DD8u,0x8900u,0xF44Du,0xA063u,0xF04Cu,0xD23Du,0xF3F8u,0xF228u,0xF530u,
    0xFE4Cu,0xF230u,0xFF2Cu,0xFF53u,0xD33Au,0x64F3u,0xD23Au,0x7404u,0xF538u,0x420Bu,0xF4FCu,0x50F1u,
    0xD332u,0xDE38u,0x8800u,0x8D10u,0xFD38u,0x8801u,0x890Fu,0x8802u,0x890Du,0x8803u,0x891Au,0x8804u,
    0x8918u,0x8805u,0x8925u,0x8806u,0x8923u,0x8807u,0x892Eu,0xA03Au,0x0009u,0xA034u,0xF4FCu,0xD12Fu,
    0xE4FEu,0xD22Du,0xF218u,0xF328u,0xF2F0u,0xD32Du,0xFE30u,0xFC2Cu,0x430Bu,0xF4FCu,0xFD00u,0xFCD3u,
    0xA025u,0xF4CCu,0xD12Au,0xE4FFu,0xD228u,0xF218u,0xF328u,0xF2F0u,0xD325u,0xFE30u,0xFC2Cu,0x430Bu,
    0xF4FCu,0xFD00u,0xFCD3u,0xA016u,0xF4CCu,0xD125u,0xD325u,0xD223u,0xF218u,0xF038u,0xF1FCu,0xF120u,
    0xF328u,0xFDFEu,0xFE30u,0xF41Cu,0xA009u,0xF4D3u,0xD114u,0xFDF0u,0xD211u,0xF218u,0xF328u,0xF2F0u,
    0xFE30u,0xF42Cu,0xF4D3u,0x4E0Bu,0x0009u,0xF5ECu,0xF500u,0x2DD8u,0x8900u,0xF54Du,0xF05Cu,0x7F0Cu,
    0x4F26u,0xFCF9u,0xFDF9u,0xFEF9u,0xFFF9u,0x6DF6u,0x000Bu,0x6EF6u,0xFFFFu,0x7FFFu,0x0000u,0x7F80u,
    0x0000u,0x8000u,0x012Cu,0x8C16u,0x0154u,0x8C16u,0x0138u,0x8C16u,0x0158u,0x8C16u,0x0130u,0x8C16u,
    0x0168u,0x8C16u,0x02D8u,0x8C16u,0xFAF8u,0x8C10u,0xFAD4u,0x8C10u,0x02C4u,0x8C16u,0x0170u,0x8C16u,
    0xE6F8u,0x8C10u,0x02C8u,0x8C16u,0x016Cu,0x8C16u,0x02CCu,0x8C16u,0x02D4u,0x8C16u,0x02D0u,0x8C16u,
};
constexpr std::array<std::uint16_t,130> quotient_words{
    0x2FE6u,0x7FE4u,0x66F3u,0x7618u,0xD536u,0x6EF3u,0x7E14u,0xF64Au,0xFE5Au,0x67F3u,0x6362u,0x2359u,
    0x3350u,0x8D0Du,0x7710u,0x53F5u,0x2359u,0x3350u,0x8B03u,0x52F5u,0xD32Fu,0x2238u,0x8B04u,0xE014u,
    0xF38Du,0xF2F6u,0xF234u,0x8B05u,0xD32Cu,0xE200u,0x2422u,0xF338u,0xA04Cu,0xF73Au,0x61E2u,0x2159u,
    0x3150u,0x8B03u,0xE200u,0x2422u,0xA044u,0xF74Au,0x6262u,0xD326u,0x2239u,0x1F22u,0x6162u,0xD225u,
    0x2129u,0x2612u,0x60E2u,0x2029u,0x2E02u,0xE00Cu,0xF3E8u,0xF468u,0xF433u,0xFF47u,0x51F3u,0x2F12u,
    0x2139u,0x1F11u,0x61F2u,0xD31Eu,0x2519u,0x3533u,0x8905u,0xF43Du,0x005Au,0x405Au,0xE00Cu,0xF32Du,
    0xFF37u,0xE00Cu,0x52F3u,0x53F1u,0x223Bu,0x1F23u,0xF0F6u,0xC717u,0xF208u,0xF025u,0x8B04u,0xC716u,
    0xF30Cu,0xF108u,0xA001u,0xF310u,0xF30Cu,0xF33Du,0x025Au,0x6323u,0x435Au,0x4311u,0x2422u,0x8D04u,
    0xF32Du,0xD10Du,0x415Au,0xF20Du,0xF320u,0xF2E8u,0xF168u,0xF322u,0xF131u,0xF71Au,0x6272u,0x53F2u,
    0x223Bu,0x2722u,0xF078u,0x7F1Cu,0x000Bu,0x6EF6u,0x0000u,0x7F80u,0xFFFFu,0x007Fu,0x012Cu,0x8C16u,
    0x0000u,0x8000u,0xFFFFu,0x7FFFu,0x0000u,0x4F80u,0x0000u,0x4F00u,0x0000u,0xCF80u,
};
constexpr std::array<std::uint16_t,18> polynomial_words{
    0xF64Cu,0xF642u,0xD506u,0xE405u,0xF559u,0xF359u,0x74FFu,0xF06Cu,0x4415u,0xF35Eu,0x8DF9u,0xF53Cu,
    0xF04Cu,0xF65Cu,0x000Bu,0xF062u,0x02DCu,0x8C16u,
};
constexpr std::array<std::uint16_t,96> scale_words{
    0x2FE6u,0x7FFCu,0x66F3u,0xD228u,0xD325u,0xD128u,0xD525u,0xF64Au,0x6762u,0xF728u,0x6E62u,0x2739u,
    0x3730u,0x8F0Cu,0xF518u,0xD024u,0x20E8u,0x8B03u,0x25E8u,0x8B20u,0xA01Bu,0x0009u,0xD321u,0xF038u,
    0x7F04u,0x000Bu,0x6EF6u,0xD220u,0x4715u,0xD320u,0xF428u,0x8D04u,0xF638u,0x25E8u,0x8B1Bu,0xA016u,
    0x0009u,0xE3E9u,0x473Cu,0x374Cu,0x4715u,0x8B02u,0x9323u,0x3733u,0x8B15u,0x4411u,0x8B09u,0x25E8u,
    0x8B03u,0xF07Cu,0x7F04u,0x000Bu,0x6EF6u,0xF05Cu,0x7F04u,0x000Bu,0x6EF6u,0x25E8u,0x8B03u,0xF04Cu,
    0x7F04u,0x000Bu,0x6EF6u,0xF06Cu,0x7F04u,0x000Bu,0x6EF6u,0xD20Au,0xE317u,0x25E9u,0x473Cu,0x22E9u,
    0x257Bu,0x252Bu,0x2652u,0xF068u,0x7F04u,0x000Bu,0x6EF6u,0x00FFu,0x0000u,0x7F80u,0x0000u,0x8000u,
    0x0134u,0x8C16u,0x0164u,0x8C16u,0xFFFFu,0x007Fu,0x012Cu,0x8C16u,0x0130u,0x8C16u,0x0160u,0x8C16u,
};
constexpr std::array<std::uint16_t,40> constants_words{
    0x0001u,0x7F80u,0x0000u,0x0000u,0x0000u,0x7F80u,0x0000u,0x3F80u,0x0000u,0x3F00u,0x0000u,0x3E80u,
    0x0000u,0x4000u,0x0000u,0x4080u,0x0FDBu,0x4049u,0x0FDBu,0x40C9u,0x0FDBu,0x3FC9u,0x0FDBu,0x3F49u,
    0x7218u,0x3F31u,0x0000u,0x8000u,0x0000u,0xFF80u,0x0000u,0xBF80u,0x0000u,0xBF00u,0x0000u,0xBE80u,
    0x0000u,0xC000u,0x0000u,0xC080u,
};
constexpr std::array<std::uint16_t,24> coefficients_words{
    0xDBB0u,0x3E7Au,0x6338u,0x3EEDu,0xBC7Du,0x3F24u,0x0000u,0x3F40u,0x0000u,0xBF40u,0x0000u,0x3E00u,
    0x2DE9u,0xBDBAu,0x8E38u,0x3DE3u,0x4925u,0xBE12u,0xCCCDu,0x3E4Cu,0xAAABu,0xBEAAu,0x0000u,0x3F80u,
};
struct Range { std::uint32_t address, size; };
bool overlaps(Range a, Range b) noexcept {
    const auto x=a.address&0x1FFFFFFFu, y=b.address&0x1FFFFFFFu;
    return x<std::uint64_t(y)+b.size && y<std::uint64_t(x)+a.size;
}
bool admitted(const DirectLinearMemoryGuard& g, bool allow_p0, Range r, unsigned alignment=4u) noexcept {
    const auto p=r.address&0x1FFFFFFFu;
    const bool segment=(r.address&0xC0000000u)==0x80000000u ||
        (allow_p0 && r.address>=0x0C000000u && r.address<0x0D000000u);
    return segment && (r.address&(alignment-1u))==0u &&
        r.size && p>=0x0C000000u && p<0x0D000000u && r.size<=0x0D000000u-p &&
        g && g.physical_base==0x0C000000u && g.physical_span>=0x01000000u &&
        g.backing_mask==0x00FFFFFFu;
}
} // namespace

bool try_execute(katana::runtime::CpuState& cpu,
                 const katana::runtime::NativePortImmutableWriteGuard* immutable_guard) {
    using namespace katana::runtime;
    static_assert(std::endian::native==std::endian::little);
    const auto entry=cpu.pc, fpscr=cpu.read_fpscr();
    if ((entry!=atan_entry && entry!=quotient_entry && entry!=polynomial_entry && entry!=scale_entry) ||
        !immutable_guard || immutable_guard->write_detected() ||
        !cpu.privileged_mode_inline() || cpu.trap_pending || cpu.sleeping || (cpu.sr&sr_fd_mask) ||
        (fpscr&(fpscr_pr_mask|fpscr_sz_mask|fpscr_exception_enable_mask)) ||
        !(fpscr&fpscr_dn_mask) || (fpscr&fpscr_rounding_mode_mask)>1u) return false;
    auto& memory=cpu.memory;
    if (memory.watchpoint_count() || memory.has_trace_handler() || memory.has_guest_memory_access_sink() ||
        memory.has_mmio_trace_handler() || !memory.guest_write_observer_allows_prevalidated_linear_writes()) return false;
    const auto guard=memory.direct_linear_memory_guard(false);
    const bool allow_p0=(cpu.mmucr&1u)==0u &&
        (!cpu.address_space || cpu.address_space->mode()==AddressTranslationMode::NoMmu);
    const auto matches=[&](std::uint32_t address,const auto& words) {
        const Range span{address,static_cast<std::uint32_t>(sizeof(words))};
        return admitted(guard,allow_p0,span) &&
            std::memcmp(guard.read_bytes+(address&0xFFFFFFu),words.data(),span.size)==0;
    };
    if (!matches(atan_entry,atan_words) ||
        !matches(quotient_entry,quotient_words) ||
        !matches(polynomial_entry,polynomial_words) ||
        !matches(scale_entry,scale_words) ||
        !matches(constants_entry,constants_words) ||
        !matches(coefficients_entry,coefficients_words)) return false;
    const auto depth=entry==atan_entry?72u:entry==quotient_entry?32u:entry==scale_entry?8u:0u;
    const Range stack{cpu.r[15]-depth,depth}, output{cpu.r[4],4u};
    const auto writable=[&](Range span) {
        if (!admitted(guard,allow_p0,span) ||
            immutable_guard->tracks_address(span.address&0x1FFFFFFFu,span.size) ||
            !memory.is_writable_linear_range(span.address&0x1FFFFFFFu,span.size,false)) return false;
        for (const auto& source:source_spans)
            if (overlaps(span,{source.address,source.size})) return false;
        return true;
    };
    if ((depth && !writable(stack)) ||
        (entry==quotient_entry && (!writable(output) || overlaps(stack,output)))) return false;
    // Direct SDK FMAC/Float/FTRC/CompareGreater share one original epoch. This
    // also retains exact incoming MXCSR/TLS on every return, including nesting.
    // The inner arithmetic context restores its optional nested epoch first.
    HostFpuExecutionEpoch epoch(cpu);
    {
    sonic::fpu_body::NontrappingSingleBody arithmetic(cpu);
    const auto load32=[&](std::uint32_t address) {
        std::uint32_t value=0;
        (void)direct_linear_guard_read_u32(guard,(address&0x1FFFFFFFu)|0x80000000u,value);
        return value;
    };
    const auto store32=[&](std::uint32_t pc,std::uint32_t address,std::uint32_t value,CodeWriteSource source) {
        if (!memory.try_write_direct_linear_u32(address&0x1FFFFFFFu,value,source))
            guest_write_u32_at(cpu,GuestInstructionOrigin{pc,pc,true},address,value,source);
    };
    auto& r=cpu.r;auto& fr=cpu.fr;
    // Static original bodies; labels are guest addresses, not a runtime decoder.
    const auto quotient=[&] {
        store32(0x8C10FAF8u,r[15]-4u,r[14],CodeWriteSource::Cpu);r[15]-=4u; // 8C10FAF8 mov.l r14,@-r15
        r[15]+=0xFFFFFFE4u; // 8C10FAFA add #-28,r15
        r[6]=r[15]; // 8C10FAFC mov r15,r6
        r[6]+=0x00000018u; // 8C10FAFE add #24,r6
        r[5]=load32(0x8C10FBDCu); // 8C10FB00 mov.l 0x8c10fbdc,r5
        r[14]=r[15]; // 8C10FB02 mov r15,r14
        r[14]+=0x00000014u; // 8C10FB04 add #20,r14
        store32(0x8C10FB06u,r[6],fr[4],CodeWriteSource::Fpu); // 8C10FB06 fmov fr4,@r6
        store32(0x8C10FB08u,r[14],fr[5],CodeWriteSource::Fpu); // 8C10FB08 fmov fr5,@r14
        r[7]=r[15]; // 8C10FB0A mov r15,r7
        r[3]=load32(r[6]); // 8C10FB0C mov.l @r6,r3
        r[3]&=r[5]; // 8C10FB0E and r5,r3
        cpu.t=r[3]==r[5]; // 8C10FB10 cmp/eq r5,r3
        {
            const bool take=cpu.t; // 8C10FB12 bt/s 0x8c10fb30
            r[7]+=0x00000010u; // 8C10FB14 delay: add #16,r7
            if(take) goto L_8C10FB30;
        }
        r[3]=load32(r[15]+20u); // 8C10FB16 mov.l @(20,r15),r3
        r[3]&=r[5]; // 8C10FB18 and r5,r3
        cpu.t=r[3]==r[5]; // 8C10FB1A cmp/eq r5,r3
        if(!cpu.t) goto L_8C10FB26; // 8C10FB1C bf 0x8c10fb26
        r[2]=load32(r[15]+20u); // 8C10FB1E mov.l @(20,r15),r2
        r[3]=load32(0x8C10FBE0u); // 8C10FB20 mov.l 0x8c10fbe0,r3
        cpu.t=(r[2]&r[3])==0u; // 8C10FB22 tst r3,r2
        if(!cpu.t) goto L_8C10FB30; // 8C10FB24 bf 0x8c10fb30
    L_8C10FB26:;
        r[0]=0x00000014u; // 8C10FB26 mov #20,r0
        fr[3]=0u; // 8C10FB28 fldi0 fr3
        fr[2]=load32(r[0]+r[15]); // 8C10FB2A fmov @(r0,r15),fr2
        arithmetic.compare_equal<3u,2u>(); // 8C10FB2C fcmp/eq fr3,fr2
        if(!cpu.t) goto L_8C10FB3C; // 8C10FB2E bf 0x8c10fb3c
    L_8C10FB30:;
        r[3]=load32(0x8C10FBE4u); // 8C10FB30 mov.l 0x8c10fbe4,r3
        r[2]=0x00000000u; // 8C10FB32 mov #0,r2
        store32(0x8C10FB34u,r[4],r[2],CodeWriteSource::Cpu); // 8C10FB34 mov.l r2,@r4
        fr[3]=load32(r[3]); // 8C10FB36 fmov @r3,fr3
        {
            // 8C10FB38 bra 0x8c10fbd4
            store32(0x8C10FB3Au,r[7],fr[3],CodeWriteSource::Fpu); // 8C10FB3A delay: fmov fr3,@r7
            goto L_8C10FBD4;
        }
    L_8C10FB3C:;
        r[1]=load32(r[14]); // 8C10FB3C mov.l @r14,r1
        r[1]&=r[5]; // 8C10FB3E and r5,r1
        cpu.t=r[1]==r[5]; // 8C10FB40 cmp/eq r5,r1
        if(!cpu.t) goto L_8C10FB4C; // 8C10FB42 bf 0x8c10fb4c
        r[2]=0x00000000u; // 8C10FB44 mov #0,r2
        store32(0x8C10FB46u,r[4],r[2],CodeWriteSource::Cpu); // 8C10FB46 mov.l r2,@r4
        {
            // 8C10FB48 bra 0x8c10fbd4
            store32(0x8C10FB4Au,r[7],fr[4],CodeWriteSource::Fpu); // 8C10FB4A delay: fmov fr4,@r7
            goto L_8C10FBD4;
        }
    L_8C10FB4C:;
        r[2]=load32(r[6]); // 8C10FB4C mov.l @r6,r2
        r[3]=load32(0x8C10FBE8u); // 8C10FB4E mov.l 0x8c10fbe8,r3
        r[2]&=r[3]; // 8C10FB50 and r3,r2
        store32(0x8C10FB52u,r[15]+8u,r[2],CodeWriteSource::Cpu); // 8C10FB52 mov.l r2,@(8,r15)
        r[1]=load32(r[6]); // 8C10FB54 mov.l @r6,r1
        r[2]=load32(0x8C10FBECu); // 8C10FB56 mov.l 0x8c10fbec,r2
        r[1]&=r[2]; // 8C10FB58 and r2,r1
        store32(0x8C10FB5Au,r[6],r[1],CodeWriteSource::Cpu); // 8C10FB5A mov.l r1,@r6
        r[0]=load32(r[14]); // 8C10FB5C mov.l @r14,r0
        r[0]&=r[2]; // 8C10FB5E and r2,r0
        store32(0x8C10FB60u,r[14],r[0],CodeWriteSource::Cpu); // 8C10FB60 mov.l r0,@r14
        r[0]=0x0000000Cu; // 8C10FB62 mov #12,r0
        fr[3]=load32(r[14]); // 8C10FB64 fmov @r14,fr3
        fr[4]=load32(r[6]); // 8C10FB66 fmov @r6,fr4
        arithmetic.binary<FpuBinaryOperation::Divide,3u,4u>(); // 8C10FB68 fdiv fr3,fr4
        store32(0x8C10FB6Au,r[0]+r[15],fr[4],CodeWriteSource::Fpu); // 8C10FB6A fmov fr4,@(r0,r15)
        r[1]=load32(r[15]+12u); // 8C10FB6C mov.l @(12,r15),r1
        store32(0x8C10FB6Eu,r[15],r[1],CodeWriteSource::Cpu); // 8C10FB6E mov.l r1,@r15
        r[1]&=r[3]; // 8C10FB70 and r3,r1
        store32(0x8C10FB72u,r[15]+4u,r[1],CodeWriteSource::Cpu); // 8C10FB72 mov.l r1,@(4,r15)
        r[1]=load32(r[15]); // 8C10FB74 mov.l @r15,r1
        r[3]=load32(0x8C10FBF0u); // 8C10FB76 mov.l 0x8c10fbf0,r3
        r[5]&=r[1]; // 8C10FB78 and r1,r5
        cpu.t=static_cast<std::int32_t>(r[5])>=static_cast<std::int32_t>(r[3]); // 8C10FB7A cmp/ge r3,r5
        if(cpu.t) goto L_8C10FB8A; // 8C10FB7C bt 0x8c10fb8a
        fpu_truncate_to_fpul(cpu,4u); // 8C10FB7E ftrc fr4,fpul
        r[0]=cpu.fpul; // 8C10FB80 sts fpul,r0
        cpu.fpul=r[0]; // 8C10FB82 lds r0,fpul
        r[0]=0x0000000Cu; // 8C10FB84 mov #12,r0
        fpu_float_from_fpul(cpu,3u); // 8C10FB86 float fpul,fr3
        store32(0x8C10FB88u,r[0]+r[15],fr[3],CodeWriteSource::Fpu); // 8C10FB88 fmov fr3,@(r0,r15)
    L_8C10FB8A:;
        r[0]=0x0000000Cu; // 8C10FB8A mov #12,r0
        r[2]=load32(r[15]+12u); // 8C10FB8C mov.l @(12,r15),r2
        r[3]=load32(r[15]+4u); // 8C10FB8E mov.l @(4,r15),r3
        r[2]|=r[3]; // 8C10FB90 or r3,r2
        store32(0x8C10FB92u,r[15]+12u,r[2],CodeWriteSource::Cpu); // 8C10FB92 mov.l r2,@(12,r15)
        fr[0]=load32(r[0]+r[15]); // 8C10FB94 fmov @(r0,r15),fr0
        r[0]=0x8C10FBF4u; // 8C10FB96 mova 0x8c10fbf4,r0
        fr[2]=load32(r[0]); // 8C10FB98 fmov @r0,fr2
        fpu_compare_greater(cpu,2u,0u); // 8C10FB9A fcmp/gt fr2,fr0
        if(!cpu.t) goto L_8C10FBA8; // 8C10FB9C bf 0x8c10fba8
        r[0]=0x8C10FBF8u; // 8C10FB9E mova 0x8c10fbf8,r0
        fr[3]=fr[0]; // 8C10FBA0 fmov fr0,fr3
        fr[1]=load32(r[0]); // 8C10FBA2 fmov @r0,fr1
        {
            // 8C10FBA4 bra 0x8c10fbaa
            arithmetic.binary<FpuBinaryOperation::Add,1u,3u>(); // 8C10FBA6 delay: fadd fr1,fr3
            goto L_8C10FBAA;
        }
    L_8C10FBA8:;
        fr[3]=fr[0]; // 8C10FBA8 fmov fr0,fr3
    L_8C10FBAA:;
        fpu_truncate_to_fpul(cpu,3u); // 8C10FBAA ftrc fr3,fpul
        r[2]=cpu.fpul; // 8C10FBAC sts fpul,r2
        r[3]=r[2]; // 8C10FBAE mov r2,r3
        cpu.fpul=r[3]; // 8C10FBB0 lds r3,fpul
        cpu.t=static_cast<std::int32_t>(r[3])>=0; // 8C10FBB2 cmp/pz r3
        store32(0x8C10FBB4u,r[4],r[2],CodeWriteSource::Cpu); // 8C10FBB4 mov.l r2,@r4
        {
            const bool take=cpu.t; // 8C10FBB6 bt/s 0x8c10fbc2
            fpu_float_from_fpul(cpu,3u); // 8C10FBB8 delay: float fpul,fr3
            if(take) goto L_8C10FBC2;
        }
        r[1]=load32(0x8C10FBF0u); // 8C10FBBA mov.l 0x8c10fbf0,r1
        cpu.fpul=r[1]; // 8C10FBBC lds r1,fpul
        fr[2]=cpu.fpul; // 8C10FBBE fsts fpul,fr2
        arithmetic.binary<FpuBinaryOperation::Add,2u,3u>(); // 8C10FBC0 fadd fr2,fr3
    L_8C10FBC2:;
        fr[2]=load32(r[14]); // 8C10FBC2 fmov @r14,fr2
        fr[1]=load32(r[6]); // 8C10FBC4 fmov @r6,fr1
        arithmetic.binary<FpuBinaryOperation::Multiply,2u,3u>(); // 8C10FBC6 fmul fr2,fr3
        arithmetic.binary<FpuBinaryOperation::Subtract,3u,1u>(); // 8C10FBC8 fsub fr3,fr1
        store32(0x8C10FBCAu,r[7],fr[1],CodeWriteSource::Fpu); // 8C10FBCA fmov fr1,@r7
        r[2]=load32(r[7]); // 8C10FBCC mov.l @r7,r2
        r[3]=load32(r[15]+8u); // 8C10FBCE mov.l @(8,r15),r3
        r[2]|=r[3]; // 8C10FBD0 or r3,r2
        store32(0x8C10FBD2u,r[7],r[2],CodeWriteSource::Cpu); // 8C10FBD2 mov.l r2,@r7
    L_8C10FBD4:;
        fr[0]=load32(r[7]); // 8C10FBD4 fmov @r7,fr0
        r[15]+=0x0000001Cu; // 8C10FBD6 add #28,r15
        {
            const auto target=cpu.pr; // 8C10FBD8 rts 
            r[14]=load32(r[15]);r[15]+=4u; // 8C10FBDA delay: mov.l @r15+,r14
            cpu.pc=target;return;
        }
    };
    const auto polynomial=[&] {
        fr[6]=fr[4]; // 8C10FAD4 fmov fr4,fr6
        arithmetic.binary<FpuBinaryOperation::Multiply,4u,6u>(); // 8C10FAD6 fmul fr4,fr6
        r[5]=load32(0x8C10FAF4u); // 8C10FAD8 mov.l 0x8c10faf4,r5
        r[4]=0x00000005u; // 8C10FADA mov #5,r4
        fr[5]=load32(r[5]);r[5]+=4u; // 8C10FADC fmov @r5+,fr5
    L_8C10FADE:;
        fr[3]=load32(r[5]);r[5]+=4u; // 8C10FADE fmov @r5+,fr3
        r[4]+=0xFFFFFFFFu; // 8C10FAE0 add #-1,r4
        fr[0]=fr[6]; // 8C10FAE2 fmov fr6,fr0
        cpu.t=static_cast<std::int32_t>(r[4])>0; // 8C10FAE4 cmp/pl r4
        fpu_multiply_accumulate(cpu,5u,3u); // 8C10FAE6 fmac fr0,fr5,fr3
        {
            const bool take=cpu.t; // 8C10FAE8 bt/s 0x8c10fade
            fr[5]=fr[3]; // 8C10FAEA delay: fmov fr3,fr5
            if(take) goto L_8C10FADE;
        }
        fr[0]=fr[4]; // 8C10FAEC fmov fr4,fr0
        fr[6]=fr[5]; // 8C10FAEE fmov fr5,fr6
        {
            const auto target=cpu.pr; // 8C10FAF0 rts 
            arithmetic.binary<FpuBinaryOperation::Multiply,6u,0u>(); // 8C10FAF2 delay: fmul fr6,fr0
            cpu.pc=target;return;
        }
    };
    const auto scale=[&] {
        store32(0x8C10E6F8u,r[15]-4u,r[14],CodeWriteSource::Cpu);r[15]-=4u; // 8C10E6F8 mov.l r14,@-r15
        r[15]+=0xFFFFFFFCu; // 8C10E6FA add #-4,r15
        r[6]=r[15]; // 8C10E6FC mov r15,r6
        r[2]=load32(0x8C10E7A0u); // 8C10E6FE mov.l 0x8c10e7a0,r2
        r[3]=load32(0x8C10E798u); // 8C10E700 mov.l 0x8c10e798,r3
        r[1]=load32(0x8C10E7A4u); // 8C10E702 mov.l 0x8c10e7a4,r1
        r[5]=load32(0x8C10E79Cu); // 8C10E704 mov.l 0x8c10e79c,r5
        store32(0x8C10E706u,r[6],fr[4],CodeWriteSource::Fpu); // 8C10E706 fmov fr4,@r6
        r[7]=load32(r[6]); // 8C10E708 mov.l @r6,r7
        fr[7]=load32(r[2]); // 8C10E70A fmov @r2,fr7
        r[14]=load32(r[6]); // 8C10E70C mov.l @r6,r14
        r[7]&=r[3]; // 8C10E70E and r3,r7
        cpu.t=r[7]==r[3]; // 8C10E710 cmp/eq r3,r7
        {
            const bool take=!cpu.t; // 8C10E712 bf/s 0x8c10e72e
            fr[5]=load32(r[1]); // 8C10E714 delay: fmov @r1,fr5
            if(take) goto L_8C10E72E;
        }
        r[0]=load32(0x8C10E7A8u); // 8C10E716 mov.l 0x8c10e7a8,r0
        cpu.t=(r[0]&r[14])==0u; // 8C10E718 tst r14,r0
        if(!cpu.t) goto L_8C10E724; // 8C10E71A bf 0x8c10e724
        cpu.t=(r[5]&r[14])==0u; // 8C10E71C tst r14,r5
        if(!cpu.t) goto L_8C10E762; // 8C10E71E bf 0x8c10e762
        {
            // 8C10E720 bra 0x8c10e75a
            ; // 8C10E722 delay: nop 
            goto L_8C10E75A;
        }
    L_8C10E724:;
        r[3]=load32(0x8C10E7ACu); // 8C10E724 mov.l 0x8c10e7ac,r3
        fr[0]=load32(r[3]); // 8C10E726 fmov @r3,fr0
        r[15]+=0x00000004u; // 8C10E728 add #4,r15
        {
            const auto target=cpu.pr; // 8C10E72A rts 
            r[14]=load32(r[15]);r[15]+=4u; // 8C10E72C delay: mov.l @r15+,r14
            cpu.pc=target;return;
        }
    L_8C10E72E:;
        r[2]=load32(0x8C10E7B0u); // 8C10E72E mov.l 0x8c10e7b0,r2
        cpu.t=static_cast<std::int32_t>(r[7])>0; // 8C10E730 cmp/pl r7
        r[3]=load32(0x8C10E7B4u); // 8C10E732 mov.l 0x8c10e7b4,r3
        fr[4]=load32(r[2]); // 8C10E734 fmov @r2,fr4
        {
            const bool take=cpu.t; // 8C10E736 bt/s 0x8c10e742
            fr[6]=load32(r[3]); // 8C10E738 delay: fmov @r3,fr6
            if(take) goto L_8C10E742;
        }
        cpu.t=(r[5]&r[14])==0u; // 8C10E73A tst r14,r5
        if(!cpu.t) goto L_8C10E776; // 8C10E73C bf 0x8c10e776
        {
            // 8C10E73E bra 0x8c10e76e
            ; // 8C10E740 delay: nop 
            goto L_8C10E76E;
        }
    L_8C10E742:;
        r[3]=0xFFFFFFE9u; // 8C10E742 mov #-23,r3
        r[7]=(r[3]&0x80000000u)==0u?r[7]<<(r[3]&31u):static_cast<std::uint32_t>(static_cast<std::int32_t>(r[7])>>((r[3]&31u)?((0u-r[3])&31u):31u)); // 8C10E744 shad r3,r7
        r[7]+=r[4]; // 8C10E746 add r4,r7
        cpu.t=static_cast<std::int32_t>(r[7])>0; // 8C10E748 cmp/pl r7
        if(!cpu.t) goto L_8C10E752; // 8C10E74A bf 0x8c10e752
        r[3]=0x000000FFu; // 8C10E74C mov.w 0x8c10e796,r3
        cpu.t=static_cast<std::int32_t>(r[7])>=static_cast<std::int32_t>(r[3]); // 8C10E74E cmp/ge r3,r7
        if(!cpu.t) goto L_8C10E77E; // 8C10E750 bf 0x8c10e77e
    L_8C10E752:;
        cpu.t=static_cast<std::int32_t>(r[4])>=0; // 8C10E752 cmp/pz r4
        if(!cpu.t) goto L_8C10E76A; // 8C10E754 bf 0x8c10e76a
        cpu.t=(r[5]&r[14])==0u; // 8C10E756 tst r14,r5
        if(!cpu.t) goto L_8C10E762; // 8C10E758 bf 0x8c10e762
    L_8C10E75A:;
        fr[0]=fr[7]; // 8C10E75A fmov fr7,fr0
        r[15]+=0x00000004u; // 8C10E75C add #4,r15
        {
            const auto target=cpu.pr; // 8C10E75E rts 
            r[14]=load32(r[15]);r[15]+=4u; // 8C10E760 delay: mov.l @r15+,r14
            cpu.pc=target;return;
        }
    L_8C10E762:;
        fr[0]=fr[5]; // 8C10E762 fmov fr5,fr0
        r[15]+=0x00000004u; // 8C10E764 add #4,r15
        {
            const auto target=cpu.pr; // 8C10E766 rts 
            r[14]=load32(r[15]);r[15]+=4u; // 8C10E768 delay: mov.l @r15+,r14
            cpu.pc=target;return;
        }
    L_8C10E76A:;
        cpu.t=(r[5]&r[14])==0u; // 8C10E76A tst r14,r5
        if(!cpu.t) goto L_8C10E776; // 8C10E76C bf 0x8c10e776
    L_8C10E76E:;
        fr[0]=fr[4]; // 8C10E76E fmov fr4,fr0
        r[15]+=0x00000004u; // 8C10E770 add #4,r15
        {
            const auto target=cpu.pr; // 8C10E772 rts 
            r[14]=load32(r[15]);r[15]+=4u; // 8C10E774 delay: mov.l @r15+,r14
            cpu.pc=target;return;
        }
    L_8C10E776:;
        fr[0]=fr[6]; // 8C10E776 fmov fr6,fr0
        r[15]+=0x00000004u; // 8C10E778 add #4,r15
        {
            const auto target=cpu.pr; // 8C10E77A rts 
            r[14]=load32(r[15]);r[15]+=4u; // 8C10E77C delay: mov.l @r15+,r14
            cpu.pc=target;return;
        }
    L_8C10E77E:;
        r[2]=load32(0x8C10E7A8u); // 8C10E77E mov.l 0x8c10e7a8,r2
        r[3]=0x00000017u; // 8C10E780 mov #23,r3
        r[5]&=r[14]; // 8C10E782 and r14,r5
        r[7]=(r[3]&0x80000000u)==0u?r[7]<<(r[3]&31u):static_cast<std::uint32_t>(static_cast<std::int32_t>(r[7])>>((r[3]&31u)?((0u-r[3])&31u):31u)); // 8C10E784 shad r3,r7
        r[2]&=r[14]; // 8C10E786 and r14,r2
        r[5]|=r[7]; // 8C10E788 or r7,r5
        r[5]|=r[2]; // 8C10E78A or r2,r5
        store32(0x8C10E78Cu,r[6],r[5],CodeWriteSource::Cpu); // 8C10E78C mov.l r5,@r6
        fr[0]=load32(r[6]); // 8C10E78E fmov @r6,fr0
        r[15]+=0x00000004u; // 8C10E790 add #4,r15
        {
            const auto target=cpu.pr; // 8C10E792 rts 
            r[14]=load32(r[15]);r[15]+=4u; // 8C10E794 delay: mov.l @r15+,r14
            cpu.pc=target;return;
        }
    };
    const auto atan=[&] {
        store32(0x8C10EEC4u,r[15]-4u,r[14],CodeWriteSource::Cpu);r[15]-=4u; // 8C10EEC4 mov.l r14,@-r15
        store32(0x8C10EEC6u,r[15]-4u,r[13],CodeWriteSource::Cpu);r[15]-=4u; // 8C10EEC6 mov.l r13,@-r15
        store32(0x8C10EEC8u,r[15]-4u,fr[15],CodeWriteSource::Fpu);r[15]-=4u; // 8C10EEC8 fmov fr15,@-r15
        store32(0x8C10EECAu,r[15]-4u,fr[14],CodeWriteSource::Fpu);r[15]-=4u; // 8C10EECA fmov fr14,@-r15
        store32(0x8C10EECCu,r[15]-4u,fr[13],CodeWriteSource::Fpu);r[15]-=4u; // 8C10EECC fmov fr13,@-r15
        store32(0x8C10EECEu,r[15]-4u,fr[12],CodeWriteSource::Fpu);r[15]-=4u; // 8C10EECE fmov fr12,@-r15
        store32(0x8C10EED0u,r[15]-4u,cpu.pr,CodeWriteSource::Cpu);r[15]-=4u; // 8C10EED0 sts.l pr,@-r15
        r[15]+=0xFFFFFFF4u; // 8C10EED2 add #-12,r15
        r[0]=0x00000008u; // 8C10EED4 mov #8,r0
        fr[5]=fr[4]; // 8C10EED6 fmov fr4,fr5
        store32(0x8C10EED8u,r[0]+r[15],fr[5],CodeWriteSource::Fpu); // 8C10EED8 fmov fr5,@(r0,r15)
        r[3]=load32(r[15]+8u); // 8C10EEDA mov.l @(8,r15),r3
        cpu.t=static_cast<std::int32_t>(r[3])>=0; // 8C10EEDC cmp/pz r3
        {
            const bool take=cpu.t; // 8C10EEDE bt/s 0x8c10eee4
            r[4]=0x00000000u; // 8C10EEE0 delay: mov #0,r4
            if(take) goto L_8C10EEE4;
        }
        r[4]=0x00000008u; // 8C10EEE2 mov #8,r4
    L_8C10EEE4:;
        fr[3]=0u; // 8C10EEE4 fldi0 fr3
        arithmetic.compare_equal<3u,5u>(); // 8C10EEE6 fcmp/eq fr3,fr5
        r[7]=0x00000004u; // 8C10EEE8 mov #4,r7
        {
            const bool take=!cpu.t; // 8C10EEEA bf/s 0x8c10eef2
            r[6]=0x00000001u; // 8C10EEEC delay: mov #1,r6
            if(take) goto L_8C10EEF2;
        }
        {
            // 8C10EEEE bra 0x8c10ef0a
            r[4]|=r[7]; // 8C10EEF0 delay: or r7,r4
            goto L_8C10EF0A;
        }
    L_8C10EEF2:;
        r[2]=load32(r[15]+8u); // 8C10EEF2 mov.l @(8,r15),r2
        r[3]=load32(0x8C10F054u); // 8C10EEF4 mov.l 0x8c10f054,r3
        r[1]=load32(0x8C10F058u); // 8C10EEF6 mov.l 0x8c10f058,r1
        r[2]&=r[3]; // 8C10EEF8 and r3,r2
        cpu.t=r[2]==r[1]; // 8C10EEFA cmp/eq r1,r2
        if(!cpu.t) goto L_8C10EF04; // 8C10EEFC bf 0x8c10ef04
        r[0]=0x00000002u; // 8C10EEFE mov #2,r0
        {
            // 8C10EF00 bra 0x8c10ef0a
            r[4]|=r[0]; // 8C10EF02 delay: or r0,r4
            goto L_8C10EF0A;
        }
    L_8C10EF04:;
        arithmetic.compare_equal<5u,5u>(); // 8C10EF04 fcmp/eq fr5,fr5
        if(cpu.t) goto L_8C10EF0A; // 8C10EF06 bt 0x8c10ef0a
        r[4]|=r[6]; // 8C10EF08 or r6,r4
    L_8C10EF0A:;
        r[3]=r[15]; // 8C10EF0A mov r15,r3
        r[2]=load32(0x8C10F05Cu); // 8C10EF0C mov.l 0x8c10f05c,r2
        store32(0x8C10EF0Eu,r[15],fr[4],CodeWriteSource::Fpu); // 8C10EF0E fmov fr4,@r15
        r[1]=load32(r[3]); // 8C10EF10 mov.l @r3,r1
        cpu.t=(r[1]&r[2])==0u; // 8C10EF12 tst r2,r1
        {
            const bool take=cpu.t; // 8C10EF14 bt/s 0x8c10ef1e
            r[5]=r[4]; // 8C10EF16 delay: mov r4,r5
            if(take) goto L_8C10EF1E;
        }
        fr[3]=load32(r[15]); // 8C10EF18 fmov @r15,fr3
        fr[3]^=0x80000000u; // 8C10EF1A fneg fr3
        store32(0x8C10EF1Cu,r[15],fr[3],CodeWriteSource::Fpu); // 8C10EF1C fmov fr3,@r15
    L_8C10EF1E:;
        cpu.t=(r[6]&r[4])==0u; // 8C10EF1E tst r4,r6
        if(cpu.t) goto L_8C10EF28; // 8C10EF20 bt 0x8c10ef28
        r[2]=load32(0x8C10F060u); // 8C10EF22 mov.l 0x8c10f060,r2
        {
            // 8C10EF24 bra 0x8c10f040
            fr[5]=load32(r[2]); // 8C10EF26 delay: fmov @r2,fr5
            goto L_8C10F040;
        }
    L_8C10EF28:;
        r[2]=r[15]; // 8C10EF28 mov r15,r2
        r[1]=load32(r[2]); // 8C10EF2A mov.l @r2,r1
        r[3]=load32(0x8C10F058u); // 8C10EF2C mov.l 0x8c10f058,r3
        r[2]=0xFFFFFFE9u; // 8C10EF2E mov #-23,r2
        r[0]=0x00000020u; // 8C10EF30 mov #32,r0
        r[1]&=r[3]; // 8C10EF32 and r3,r1
        r[1]=(r[2]&0x80000000u)==0u?r[1]<<(r[2]&31u):static_cast<std::uint32_t>(static_cast<std::int32_t>(r[1])>>((r[2]&31u)?((0u-r[2])&31u):31u)); // 8C10EF34 shad r2,r1
        r[1]+=0xFFFFFF81u; // 8C10EF36 add #-127,r1
        cpu.t=static_cast<std::int32_t>(r[1])>=static_cast<std::int32_t>(r[0]); // 8C10EF38 cmp/ge r0,r1
        r[13]=0x00000008u; // 8C10EF3A mov #8,r13
        {
            const bool take=!cpu.t; // 8C10EF3C bf/s 0x8c10ef50
            r[13]&=r[4]; // 8C10EF3E delay: and r4,r13
            if(take) goto L_8C10EF50;
        }
        r[1]=load32(0x8C10F064u); // 8C10EF40 mov.l 0x8c10f064,r1
        cpu.t=(r[13]&r[13])==0u; // 8C10EF42 tst r13,r13
        {
            const bool take=!cpu.t; // 8C10EF44 bf/s 0x8c10ef4c
            fr[5]=load32(r[1]); // 8C10EF46 delay: fmov @r1,fr5
            if(take) goto L_8C10EF4C;
        }
        {
            // 8C10EF48 bra 0x8c10f040
            ; // 8C10EF4A delay: nop 
            goto L_8C10F040;
        }
    L_8C10EF4C:;
        {
            // 8C10EF4C bra 0x8c10f040
            fr[5]^=0x80000000u; // 8C10EF4E delay: fneg fr5
            goto L_8C10F040;
        }
    L_8C10EF50:;
        cpu.t=(r[4]&r[7])==0u; // 8C10EF50 tst r7,r4
        if(cpu.t) goto L_8C10EF58; // 8C10EF52 bt 0x8c10ef58
        {
            // 8C10EF54 bra 0x8c10f040
            fr[5]=fr[4]; // 8C10EF56 delay: fmov fr4,fr5
            goto L_8C10F040;
        }
    L_8C10EF58:;
        r[1]=load32(0x8C10F068u); // 8C10EF58 mov.l 0x8c10f068,r1
        fr[3]=load32(r[15]); // 8C10EF5A fmov @r15,fr3
        fr[5]=load32(r[1]); // 8C10EF5C fmov @r1,fr5
        r[3]=load32(0x8C10F06Cu); // 8C10EF5E mov.l 0x8c10f06c,r3
        fpu_compare_greater(cpu,3u,5u); // 8C10EF60 fcmp/gt fr3,fr5
        {
            const bool take=!cpu.t; // 8C10EF62 bf/s 0x8c10ef6e
            fr[4]=load32(r[3]); // 8C10EF64 delay: fmov @r3,fr4
            if(take) goto L_8C10EF6E;
        }
        r[2]=load32(0x8C10F070u); // 8C10EF66 mov.l 0x8c10f070,r2
        fr[14]=load32(r[2]); // 8C10EF68 fmov @r2,fr14
        {
            // 8C10EF6A bra 0x8c10ef8c
            fr[15]=fr[3]; // 8C10EF6C delay: fmov fr3,fr15
            goto L_8C10EF8C;
        }
    L_8C10EF6E:;
        arithmetic.compare_equal<3u,5u>(); // 8C10EF6E fcmp/eq fr3,fr5
        if(!cpu.t) goto L_8C10EF7C; // 8C10EF70 bf 0x8c10ef7c
        cpu.t=(r[13]&r[13])==0u; // 8C10EF72 tst r13,r13
        if(cpu.t) goto L_8C10EF78; // 8C10EF74 bt 0x8c10ef78
        fr[4]^=0x80000000u; // 8C10EF76 fneg fr4
    L_8C10EF78:;
        {
            // 8C10EF78 bra 0x8c10f042
            fr[0]=fr[4]; // 8C10EF7A delay: fmov fr4,fr0
            goto L_8C10F042;
        }
    L_8C10EF7C:;
        r[2]=load32(0x8C10F074u); // 8C10EF7C mov.l 0x8c10f074,r2
        fr[3]=load32(r[15]); // 8C10EF7E fmov @r15,fr3
        fr[2]=load32(r[2]); // 8C10EF80 fmov @r2,fr2
        arithmetic.binary<FpuBinaryOperation::Add,3u,5u>(); // 8C10EF82 fadd fr3,fr5
        fr[14]=fr[4]; // 8C10EF84 fmov fr4,fr14
        arithmetic.binary<FpuBinaryOperation::Add,3u,2u>(); // 8C10EF86 fadd fr3,fr2
        fr[15]=fr[2]; // 8C10EF88 fmov fr2,fr15
        arithmetic.binary<FpuBinaryOperation::Divide,5u,15u>(); // 8C10EF8A fdiv fr5,fr15
    L_8C10EF8C:;
        r[3]=load32(0x8C10F078u); // 8C10EF8C mov.l 0x8c10f078,r3
        r[4]=r[15]; // 8C10EF8E mov r15,r4
        r[2]=load32(0x8C10F07Cu); // 8C10EF90 mov.l 0x8c10f07c,r2
        r[4]+=0x00000004u; // 8C10EF92 add #4,r4
        fr[5]=load32(r[3]); // 8C10EF94 fmov @r3,fr5
        {
            cpu.pr=0x8C10EF9Au; // 8C10EF96 jsr @r2
            fr[4]=fr[15]; // 8C10EF98 delay: fmov fr15,fr4
            quotient();
        }
        r[0]=load32(r[15]+4u); // 8C10EF9A mov.l @(4,r15),r0
        r[3]=load32(0x8C10F068u); // 8C10EF9C mov.l 0x8c10f068,r3
        r[14]=load32(0x8C10F080u); // 8C10EF9E mov.l 0x8c10f080,r14
        cpu.t=r[0]==0x00000000u; // 8C10EFA0 cmp/eq #0,r0
        {
            const bool take=cpu.t; // 8C10EFA2 bt/s 0x8c10efc6
            fr[13]=load32(r[3]); // 8C10EFA4 delay: fmov @r3,fr13
            if(take) goto L_8C10EFC6;
        }
        cpu.t=r[0]==0x00000001u; // 8C10EFA6 cmp/eq #1,r0
        if(cpu.t) goto L_8C10EFCA; // 8C10EFA8 bt 0x8c10efca
        cpu.t=r[0]==0x00000002u; // 8C10EFAA cmp/eq #2,r0
        if(cpu.t) goto L_8C10EFCA; // 8C10EFAC bt 0x8c10efca
        cpu.t=r[0]==0x00000003u; // 8C10EFAE cmp/eq #3,r0
        if(cpu.t) goto L_8C10EFE8; // 8C10EFB0 bt 0x8c10efe8
        cpu.t=r[0]==0x00000004u; // 8C10EFB2 cmp/eq #4,r0
        if(cpu.t) goto L_8C10EFE8; // 8C10EFB4 bt 0x8c10efe8
        cpu.t=r[0]==0x00000005u; // 8C10EFB6 cmp/eq #5,r0
        if(cpu.t) goto L_8C10F006; // 8C10EFB8 bt 0x8c10f006
        cpu.t=r[0]==0x00000006u; // 8C10EFBA cmp/eq #6,r0
        if(cpu.t) goto L_8C10F006; // 8C10EFBC bt 0x8c10f006
        cpu.t=r[0]==0x00000007u; // 8C10EFBE cmp/eq #7,r0
        if(cpu.t) goto L_8C10F020; // 8C10EFC0 bt 0x8c10f020
        {
            // 8C10EFC2 bra 0x8c10f03a
            ; // 8C10EFC4 delay: nop 
            goto L_8C10F03A;
        }
    L_8C10EFC6:;
        {
            // 8C10EFC6 bra 0x8c10f032
            fr[4]=fr[15]; // 8C10EFC8 delay: fmov fr15,fr4
            goto L_8C10F032;
        }
    L_8C10EFCA:;
        r[1]=load32(0x8C10F088u); // 8C10EFCA mov.l 0x8c10f088,r1
        r[4]=0xFFFFFFFEu; // 8C10EFCC mov #-2,r4
        r[2]=load32(0x8C10F084u); // 8C10EFCE mov.l 0x8c10f084,r2
        fr[2]=load32(r[1]); // 8C10EFD0 fmov @r1,fr2
        fr[3]=load32(r[2]); // 8C10EFD2 fmov @r2,fr3
        arithmetic.binary<FpuBinaryOperation::Add,15u,2u>(); // 8C10EFD4 fadd fr15,fr2
        r[3]=load32(0x8C10F08Cu); // 8C10EFD6 mov.l 0x8c10f08c,r3
        arithmetic.binary<FpuBinaryOperation::Add,3u,14u>(); // 8C10EFD8 fadd fr3,fr14
        fr[12]=fr[2]; // 8C10EFDA fmov fr2,fr12
        {
            cpu.pr=0x8C10EFE0u; // 8C10EFDC jsr @r3
            fr[4]=fr[15]; // 8C10EFDE delay: fmov fr15,fr4
            scale();
        }
        arithmetic.binary<FpuBinaryOperation::Add,0u,13u>(); // 8C10EFE0 fadd fr0,fr13
        arithmetic.binary<FpuBinaryOperation::Divide,13u,12u>(); // 8C10EFE2 fdiv fr13,fr12
        {
            // 8C10EFE4 bra 0x8c10f032
            fr[4]=fr[12]; // 8C10EFE6 delay: fmov fr12,fr4
            goto L_8C10F032;
        }
    L_8C10EFE8:;
        r[1]=load32(0x8C10F094u); // 8C10EFE8 mov.l 0x8c10f094,r1
        r[4]=0xFFFFFFFFu; // 8C10EFEA mov #-1,r4
        r[2]=load32(0x8C10F090u); // 8C10EFEC mov.l 0x8c10f090,r2
        fr[2]=load32(r[1]); // 8C10EFEE fmov @r1,fr2
        fr[3]=load32(r[2]); // 8C10EFF0 fmov @r2,fr3
        arithmetic.binary<FpuBinaryOperation::Add,15u,2u>(); // 8C10EFF2 fadd fr15,fr2
        r[3]=load32(0x8C10F08Cu); // 8C10EFF4 mov.l 0x8c10f08c,r3
        arithmetic.binary<FpuBinaryOperation::Add,3u,14u>(); // 8C10EFF6 fadd fr3,fr14
        fr[12]=fr[2]; // 8C10EFF8 fmov fr2,fr12
        {
            cpu.pr=0x8C10EFFEu; // 8C10EFFA jsr @r3
            fr[4]=fr[15]; // 8C10EFFC delay: fmov fr15,fr4
            scale();
        }
        arithmetic.binary<FpuBinaryOperation::Add,0u,13u>(); // 8C10EFFE fadd fr0,fr13
        arithmetic.binary<FpuBinaryOperation::Divide,13u,12u>(); // 8C10F000 fdiv fr13,fr12
        {
            // 8C10F002 bra 0x8c10f032
            fr[4]=fr[12]; // 8C10F004 delay: fmov fr12,fr4
            goto L_8C10F032;
        }
    L_8C10F006:;
        r[1]=load32(0x8C10F09Cu); // 8C10F006 mov.l 0x8c10f09c,r1
        r[3]=load32(0x8C10F0A0u); // 8C10F008 mov.l 0x8c10f0a0,r3
        r[2]=load32(0x8C10F098u); // 8C10F00A mov.l 0x8c10f098,r2
        fr[2]=load32(r[1]); // 8C10F00C fmov @r1,fr2
        fr[0]=load32(r[3]); // 8C10F00E fmov @r3,fr0
        fr[1]=fr[15]; // 8C10F010 fmov fr15,fr1
        arithmetic.binary<FpuBinaryOperation::Add,2u,1u>(); // 8C10F012 fadd fr2,fr1
        fr[3]=load32(r[2]); // 8C10F014 fmov @r2,fr3
        fpu_multiply_accumulate(cpu,15u,13u); // 8C10F016 fmac fr0,fr15,fr13
        arithmetic.binary<FpuBinaryOperation::Add,3u,14u>(); // 8C10F018 fadd fr3,fr14
        fr[4]=fr[1]; // 8C10F01A fmov fr1,fr4
        {
            // 8C10F01C bra 0x8c10f032
            arithmetic.binary<FpuBinaryOperation::Divide,13u,4u>(); // 8C10F01E delay: fdiv fr13,fr4
            goto L_8C10F032;
        }
    L_8C10F020:;
        r[1]=load32(0x8C10F074u); // 8C10F020 mov.l 0x8c10f074,r1
        arithmetic.binary<FpuBinaryOperation::Add,15u,13u>(); // 8C10F022 fadd fr15,fr13
        r[2]=load32(0x8C10F06Cu); // 8C10F024 mov.l 0x8c10f06c,r2
        fr[2]=load32(r[1]); // 8C10F026 fmov @r1,fr2
        fr[3]=load32(r[2]); // 8C10F028 fmov @r2,fr3
        arithmetic.binary<FpuBinaryOperation::Add,15u,2u>(); // 8C10F02A fadd fr15,fr2
        arithmetic.binary<FpuBinaryOperation::Add,3u,14u>(); // 8C10F02C fadd fr3,fr14
        fr[4]=fr[2]; // 8C10F02E fmov fr2,fr4
        arithmetic.binary<FpuBinaryOperation::Divide,13u,4u>(); // 8C10F030 fdiv fr13,fr4
    L_8C10F032:;
        {
            cpu.pr=0x8C10F036u; // 8C10F032 jsr @r14
            ; // 8C10F034 delay: nop 
            polynomial();
        }
        fr[5]=fr[14]; // 8C10F036 fmov fr14,fr5
        arithmetic.binary<FpuBinaryOperation::Add,0u,5u>(); // 8C10F038 fadd fr0,fr5
    L_8C10F03A:;
        cpu.t=(r[13]&r[13])==0u; // 8C10F03A tst r13,r13
        if(cpu.t) goto L_8C10F040; // 8C10F03C bt 0x8c10f040
        fr[5]^=0x80000000u; // 8C10F03E fneg fr5
    L_8C10F040:;
        fr[0]=fr[5]; // 8C10F040 fmov fr5,fr0
    L_8C10F042:;
        r[15]+=0x0000000Cu; // 8C10F042 add #12,r15
        cpu.pr=load32(r[15]);r[15]+=4u; // 8C10F044 lds.l @r15+,pr
        fr[12]=load32(r[15]);r[15]+=4u; // 8C10F046 fmov @r15+,fr12
        fr[13]=load32(r[15]);r[15]+=4u; // 8C10F048 fmov @r15+,fr13
        fr[14]=load32(r[15]);r[15]+=4u; // 8C10F04A fmov @r15+,fr14
        fr[15]=load32(r[15]);r[15]+=4u; // 8C10F04C fmov @r15+,fr15
        r[13]=load32(r[15]);r[15]+=4u; // 8C10F04E mov.l @r15+,r13
        {
            const auto target=cpu.pr; // 8C10F050 rts 
            r[14]=load32(r[15]);r[15]+=4u; // 8C10F052 delay: mov.l @r15+,r14
            cpu.pc=target;return;
        }
    };
    switch(entry) {
    case atan_entry:atan();break;
    case quotient_entry:quotient();break;
    case polynomial_entry:polynomial();break;
    case scale_entry:scale();break;
    }
    }
    return true;
}
} // namespace sonic::atan_math
