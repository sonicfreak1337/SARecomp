#include "sonic_vertex_normals.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <vector>

namespace sonic::vertex_normals {
namespace {
using namespace katana::runtime;
constexpr std::uint32_t entry = 0x8C0563ACu, code_size = 0x2A6u;
// Exact retail little-endian words, including the unreachable literal island.
constexpr std::array<std::uint16_t, 339> original_words{
    0x2FE6u,0x2FD6u,0x2FC6u,0x2FB6u,0x2FA6u,0x2F96u,0x2F86u,0x7FE0u,
    0x5242u,0xE700u,0xF68Du,0x6673u,0x1F27u,0x6273u,0x5341u,0x1F36u,
    0x53F7u,0x3232u,0x8F02u,0xF59Du,0xA134u,0x0009u,0x6E63u,0x4E00u,
    0x6363u,0x52F6u,0x3E3Cu,0x4E08u,0xE008u,0x3E2Cu,0xFE67u,0xE004u,
    0xFE67u,0xE300u,0xFE6Au,0x5A43u,0x1F75u,0x854Au,0x600Du,0x3036u,
    0x8D02u,0xF46Cu,0xA106u,0x0009u,0xD310u,0x51A3u,0x2F12u,0x85A1u,
    0x6B03u,0x60A1u,0x600Du,0x2039u,0x8800u,0x8D23u,0x55A1u,0xE140u,
    0x4118u,0x3010u,0x895Eu,0xD109u,0x3010u,0x8B01u,0xA0A3u,0x0009u,
    0xA108u,0x0009u,0x0900u,0x0900u,0x0900u,0x0900u,0x0900u,0x0900u,
    0x0900u,0x0900u,0x8000u,0x0000u,0xA88Cu,0x8C63u,0xC000u,0x0000u,
    0x0900u,0x0900u,0x0900u,0x0900u,0x0900u,0x0900u,0x0900u,0x0900u,
    0x0900u,0x0900u,0xE200u,0x6DF2u,0x61BDu,0x6973u,0xE000u,0x325Cu,
    0x3012u,0x1F22u,0x8F02u,0x6C73u,0xA0C1u,0x0009u,0x50F2u,0x7006u,
    0x1F02u,0x70FAu,0x6001u,0x81F2u,0x60C3u,0x7001u,0x4000u,0x035Du,
    0x60C3u,0x7002u,0x4000u,0x2F31u,0x085Du,0x85F2u,0x600Du,0x3600u,
    0x8905u,0x633Du,0x3630u,0x8902u,0x688Du,0x3680u,0x8B12u,0xF2E8u,
    0xE004u,0xF3D8u,0x6103u,0x31DCu,0xF450u,0xF230u,0xFE2Au,0xF2E6u,
    0xF318u,0xF230u,0xFE27u,0xE008u,0x6103u,0x31DCu,0xF2E6u,0xF318u,
    0xF230u,0xFE27u,0x63BDu,0x7901u,0x3932u,0x7D0Cu,0x8FCEu,0x7C03u,
    0xA08Du,0x0009u,0xE200u,0x6DF2u,0x61BDu,0x6973u,0xE000u,0x325Cu,
    0x3012u,0x1F23u,0x8F02u,0x6C73u,0xA081u,0x0009u,0x50F3u,0x7008u,
    0x1F03u,0x70F8u,0x6001u,0x81F2u,0x60C3u,0x7001u,0x4000u,0x005Du,
    0x81F4u,0x60C3u,0x7002u,0x4000u,0x035Du,0x60C3u,0x7003u,0x4000u,
    0x2F31u,0x085Du,0x85F2u,0x600Du,0x3600u,0x8909u,0x85F4u,0x600Du,
    0x3600u,0x8905u,0x633Du,0x3630u,0x8902u,0x688Du,0x3680u,0x8B12u,
    0xF2E8u,0xE004u,0xF3D8u,0x6103u,0x31DCu,0xF450u,0xF230u,0xFE2Au,
    0xF2E6u,0xF318u,0xF230u,0xFE27u,0xE008u,0x6103u,0x31DCu,0xF2E6u,
    0xF318u,0xF230u,0xFE27u,0x63BDu,0x7901u,0x3932u,0x7D0Cu,0x8FC5u,
    0x7C04u,0xA044u,0x0009u,0x6BBDu,0x1F73u,0xE100u,0x62B3u,0x3122u,
    0x8D3Du,0x1FB4u,0x6055u,0xE340u,0x4318u,0x6873u,0x81F4u,0x73FFu,
    0x85F4u,0x2039u,0x81F4u,0x6955u,0x85F4u,0x6C55u,0x6DF2u,0x70FEu,
    0x6B55u,0x4015u,0x8F24u,0x1F01u,0x699Du,0x3690u,0x8905u,0x62CDu,
    0x3620u,0x8902u,0x62BDu,0x3620u,0x8B12u,0xF2E8u,0xE004u,0xF3D8u,
    0x6103u,0x31DCu,0xF450u,0xF230u,0xFE2Au,0xF2E6u,0xF318u,0xF230u,
    0xFE27u,0xE008u,0x6103u,0x31DCu,0xF2E6u,0xF318u,0xF230u,0xFE27u,
    0x69C3u,0x53F1u,0x6CB3u,0x7801u,0x6B55u,0x3833u,0x8FDCu,0x7D0Cu,
    0x53F3u,0x7301u,0x1F33u,0x52F4u,0x3322u,0x8FC3u,0x75FEu,0x52F5u,
    0x7201u,0x6323u,0x1F25u,0x854Au,0x600Du,0x3036u,0x8F02u,0x7A18u,
    0xAEFAu,0x0009u,0x6563u,0x4500u,0x6363u,0x52F6u,0x353Cu,0x4508u,
    0xE004u,0x352Cu,0xF358u,0x7601u,0xF343u,0xF53Au,0xF256u,0xF243u,
    0xF527u,0xE008u,0xF356u,0xF343u,0xF537u,0x53F7u,0x3632u,0x8901u,
    0xAECCu,0x0009u,0x7F20u,0x68F6u,0x69F6u,0x6AF6u,0x6BF6u,0x6CF6u,
    0x6DF6u,0x000Bu,0x6EF6u,
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
std::uint32_t peek32(const DirectLinearMemoryGuard& g,std::uint32_t address) noexcept {
    std::uint32_t v;std::memcpy(&v,g.read_bytes+(address&0xFFFFFFu),4u);return v;
}
std::uint16_t peek16(const DirectLinearMemoryGuard& g,std::uint32_t address) noexcept {
    std::uint16_t v;std::memcpy(&v,g.read_bytes+(address&0xFFFFFFu),2u);return v;
}
} // namespace

bool try_execute(katana::runtime::CpuState& cpu,
                 const katana::runtime::NativePortImmutableWriteGuard* immutable_guard) {
    using namespace katana::runtime;
    static_assert(std::endian::native==std::endian::little);
    const auto fpscr=cpu.read_fpscr();
    if (!immutable_guard || immutable_guard->write_detected() || cpu.pc!=entry ||
        !cpu.privileged_mode_inline() || cpu.trap_pending || cpu.sleeping || (cpu.sr&sr_fd_mask) ||
        (fpscr&(fpscr_pr_mask|fpscr_sz_mask|fpscr_exception_enable_mask)) ||
        !(fpscr&fpscr_dn_mask) || (fpscr&fpscr_rounding_mode_mask)>1u) return false;
    auto& memory=cpu.memory;
    if (memory.watchpoint_count() || memory.has_trace_handler() || memory.has_guest_memory_access_sink() ||
        memory.has_mmio_trace_handler() || !memory.guest_write_observer_allows_prevalidated_linear_writes())
        return false;
    const auto guard=memory.direct_linear_memory_guard(false);
    // P0 only when both MMU views agree it is untranslated. P1/P2 bypass MMU.
    const bool allow_p0=(cpu.mmucr&1u)==0u &&
        (!cpu.address_space || cpu.address_space->mode()==AddressTranslationMode::NoMmu);
    const Range code{entry,code_size}, model{cpu.r[4],24u}, stack{cpu.r[15]-60u,60u};
    if (!admitted(guard,allow_p0,code) || !admitted(guard,allow_p0,model) || !admitted(guard,allow_p0,stack) ||
        std::memcmp(guard.read_bytes+0x563ACu,original_words.data(),code_size)) return false;
    const auto count=peek32(guard,model.address+8u), output=peek32(guard,model.address+4u);
    if (count>65536u) return false;
    std::vector<Range> reads{code,model};
    const auto input=[&](Range range,unsigned alignment=4u) {
        if (!admitted(guard,allow_p0,range,alignment)) return false;
        // Inputs are never mutated. Meshsets may share arrays, and a strip's
        // required look-ahead can read the next adjacent index list's header.
        // Keep every range: writable() must still reject output/stack overlap
        // with any of these inputs, including partially shared ranges.
        reads.push_back(range);return true;
    };
    // Preflight is read-only. Bound all conditional loads, including the strip
    // loop's final look-ahead index and its per-strip reset of face normals.
    std::uint64_t work=0u;
    if (count) {
        const auto mesh_count=peek16(guard,model.address+20u);
        const auto meshes=peek32(guard,model.address+12u);
        if (mesh_count>4096u || (mesh_count && !input({meshes,std::uint32_t(mesh_count)*24u}))) return false;
        for (unsigned mesh=0;mesh<mesh_count;++mesh) {
            const auto at=meshes+mesh*24u;
            const auto type=peek16(guard,at)&0xC000u;
            const auto faces=peek16(guard,at+2u);
            const auto indices=peek32(guard,at+4u), face_normals=peek32(guard,at+12u);
            if (type==0x8000u) return false; // original early-exit path owns unsupported types
            if ((++work)*count>8388608u) return false;
            if (!faces) continue;
            if (type!=0xC000u) {
                const auto width=type==0u?3u:4u;
                if (!input({indices,std::uint32_t(faces)*width*2u},2u) ||
                    !input({face_normals,std::uint32_t(faces)*12u})) return false;
                work+=faces;
            } else {
                std::uint32_t cursor=indices, end=indices, max_faces=0u;
                for (unsigned strip=0;strip<faces;++strip) {
                    if (!admitted(guard,allow_p0,{cursor,2u},2u)) return false;
                    const auto length=peek16(guard,cursor)&0x3FFFu;
                    const auto nfaces=length>2u?length-2u:0u;
                    // Even lengths 0..2 read three initial indices. Every
                    // executed triangle additionally reads one index ahead.
                    const auto bytes=2u+(3u+nfaces)*2u;
                    if (!admitted(guard,allow_p0,{cursor,bytes},2u)) return false;
                    end=std::max(end,cursor+bytes);max_faces=std::max(max_faces,nfaces);
                    cursor+=length>2u?2u+length*2u:6u;
                    work+=nfaces+1u;
                    if (work>8388608u) return false;
                }
                if (!input({indices,end-indices},2u) ||
                    (max_faces && !input({face_normals,max_faces*12u}))) return false;
            }
            if (work*count>8388608u) return false;
        }
    }
    const auto writable=[&](Range range) {
        if (!admitted(guard,allow_p0,range) || immutable_guard->tracks_address(range.address&0x1FFFFFFFu,range.size) ||
            !memory.is_writable_linear_range(range.address&0x1FFFFFFFu,range.size,false)) return false;
        for (const auto source:reads) if (overlaps(range,source)) return false;
        return true;
    };
    const Range normals{output,count*12u};
    if (!writable(stack) || (count && (!writable(normals) || overlaps(stack,normals)))) return false;

    // No fallback after this point. This is a static translation of the whole
    // leaf, not a runtime decoder. Keep O(V*F): a scatter rewrite changes the
    // observable store sequence and would need a separate proven contract.
    const auto load32=[&](std::uint32_t a) {std::uint32_t v=0;(void)direct_linear_guard_read_u32(guard,(a&0x1FFFFFFFu)|0x80000000u,v);return v;};
    const auto load16=[&](std::uint32_t a) {std::uint16_t v=0;(void)direct_linear_guard_read_u16(guard,(a&0x1FFFFFFFu)|0x80000000u,v);return v;};
    const auto store32=[&](std::uint32_t pc,std::uint32_t a,std::uint32_t v) {
        if (!memory.try_write_direct_linear_u32(a&0x1FFFFFFFu,v,CodeWriteSource::Cpu))
            guest_write_u32_at(cpu,GuestInstructionOrigin{pc,pc,true},a,v);
    };
    const auto store_fpu32=[&](std::uint32_t pc,std::uint32_t a,std::uint32_t v) {
        if (!memory.try_write_direct_linear_u32(a&0x1FFFFFFFu,v,CodeWriteSource::Fpu))
            guest_write_u32_at(cpu,GuestInstructionOrigin{pc,pc,true},a,v,CodeWriteSource::Fpu);
    };
    const auto store16=[&](std::uint32_t pc,std::uint32_t a,std::uint32_t v) {
        if (!memory.try_write_direct_linear_u16(a&0x1FFFFFFFu,static_cast<std::uint16_t>(v),CodeWriteSource::Cpu))
            guest_write_u16_at(cpu,GuestInstructionOrigin{pc,pc,true},a,static_cast<std::uint16_t>(v));
    };
    auto& r=cpu.r;auto& fr=cpu.fr;
    const HostFpuExecutionEpoch epoch(cpu);
    store32(0x8C0563ACu,r[15]-4u,r[14]);r[15]-=4u; // 8C0563AC mov.l r14,@-r15
    store32(0x8C0563AEu,r[15]-4u,r[13]);r[15]-=4u; // 8C0563AE mov.l r13,@-r15
    store32(0x8C0563B0u,r[15]-4u,r[12]);r[15]-=4u; // 8C0563B0 mov.l r12,@-r15
    store32(0x8C0563B2u,r[15]-4u,r[11]);r[15]-=4u; // 8C0563B2 mov.l r11,@-r15
    store32(0x8C0563B4u,r[15]-4u,r[10]);r[15]-=4u; // 8C0563B4 mov.l r10,@-r15
    store32(0x8C0563B6u,r[15]-4u,r[9]);r[15]-=4u; // 8C0563B6 mov.l r9,@-r15
    store32(0x8C0563B8u,r[15]-4u,r[8]);r[15]-=4u; // 8C0563B8 mov.l r8,@-r15
    r[15]+=0xFFFFFFE0u; // 8C0563BA add #-32,r15
    r[2]=load32(8u+r[4]); // 8C0563BC mov.l @(8,r4),r2
    r[7]=0x00000000u; // 8C0563BE mov #0,r7
    fr[6]=0u; // 8C0563C0 fldi0 fr6
    r[6]=r[7]; // 8C0563C2 mov r7,r6
    store32(0x8C0563C4u,28u+r[15],r[2]); // 8C0563C4 mov.l r2,@(28,r15)
    r[2]=r[7]; // 8C0563C6 mov r7,r2
    r[3]=load32(4u+r[4]); // 8C0563C8 mov.l @(4,r4),r3
    store32(0x8C0563CAu,24u+r[15],r[3]); // 8C0563CA mov.l r3,@(24,r15)
    r[3]=load32(28u+r[15]); // 8C0563CC mov.l @(28,r15),r3
    cpu.t=r[2]>=r[3]; // 8C0563CE cmp/hs r3,r2
    { const bool taken=!cpu.t; // 8C0563D0 bf/s 0x8c0563d8
      fr[5]=0x3F800000u; // 8C0563D2 delay slot
      if (taken) goto L_8C0563D8; }
     // 8C0563D4 bra 0x8c056640
    goto L_8C056640;
L_8C0563D8:
    r[14]=r[6]; // 8C0563D8 mov r6,r14
    cpu.t=(r[14]&0x80000000u)!=0u;r[14]<<=1u; // 8C0563DA shll r14
    r[3]=r[6]; // 8C0563DC mov r6,r3
    r[2]=load32(24u+r[15]); // 8C0563DE mov.l @(24,r15),r2
    r[14]+=r[3]; // 8C0563E0 add r3,r14
    r[14]<<=2u; // 8C0563E2 shll2 r14
    r[0]=0x00000008u; // 8C0563E4 mov #8,r0
    r[14]+=r[2]; // 8C0563E6 add r2,r14
    store_fpu32(0x8C0563E8u,r[0]+r[14],fr[6]); // 8C0563E8 fmov fr6,@(r0,r14)
    r[0]=0x00000004u; // 8C0563EA mov #4,r0
    store_fpu32(0x8C0563ECu,r[0]+r[14],fr[6]); // 8C0563EC fmov fr6,@(r0,r14)
    r[3]=0x00000000u; // 8C0563EE mov #0,r3
    store_fpu32(0x8C0563F0u,r[14],fr[6]); // 8C0563F0 fmov fr6,@r14
    r[10]=load32(12u+r[4]); // 8C0563F2 mov.l @(12,r4),r10
    store32(0x8C0563F4u,20u+r[15],r[7]); // 8C0563F4 mov.l r7,@(20,r15)
    r[0]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(20u+r[4]))); // 8C0563F6 mov.w @(20,r4),r0
    r[0]=r[0]&0xFFFFu; // 8C0563F8 extu.w r0,r0
    cpu.t=r[0]>r[3]; // 8C0563FA cmp/hi r3,r0
    { const bool taken=cpu.t; // 8C0563FC bt/s 0x8c056404
      fr[4]=fr[6]; // 8C0563FE delay slot
      if (taken) goto L_8C056404; }
     // 8C056400 bra 0x8c056610
    goto L_8C056610;
L_8C056404:
    r[3]=load32(0x8c056448u); // 8C056404 mov.l 0x8c056448,r3
    r[1]=load32(12u+r[10]); // 8C056406 mov.l @(12,r10),r1
    store32(0x8C056408u,r[15],r[1]); // 8C056408 mov.l r1,@r15
    r[0]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(2u+r[10]))); // 8C05640A mov.w @(2,r10),r0
    r[11]=r[0]; // 8C05640C mov r0,r11
    r[0]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(r[10]))); // 8C05640E mov.w @r10,r0
    r[0]=r[0]&0xFFFFu; // 8C056410 extu.w r0,r0
    r[0]&=r[3]; // 8C056412 and r3,r0
    cpu.t=r[0]==0x00000000u; // 8C056414 cmp/eq #0,r0
    { const bool taken=cpu.t; // 8C056416 bt/s 0x8c056460
      r[5]=load32(4u+r[10]); // 8C056418 delay slot
      if (taken) goto L_8C056460; }
    r[1]=0x00000040u; // 8C05641A mov #64,r1
    r[1]<<=8u; // 8C05641C shll8 r1
    cpu.t=r[0]==r[1]; // 8C05641E cmp/eq r1,r0
    if (cpu.t) goto L_8C0564E0; // 8C056420 bt 0x8c0564e0
    r[1]=load32(0x8c056448u); // 8C056422 mov.l 0x8c056448,r1
    cpu.t=r[0]==r[1]; // 8C056424 cmp/eq r1,r0
    if (!cpu.t) goto L_8C05642C; // 8C056426 bf 0x8c05642c
     // 8C056428 bra 0x8c056572
    goto L_8C056572;
L_8C05642C:
     // 8C05642C bra 0x8c056640
    goto L_8C056640;
L_8C056460:
    r[2]=0x00000000u; // 8C056460 mov #0,r2
    r[13]=load32(r[15]); // 8C056462 mov.l @r15,r13
    r[1]=r[11]&0xFFFFu; // 8C056464 extu.w r11,r1
    r[9]=r[7]; // 8C056466 mov r7,r9
    r[0]=0x00000000u; // 8C056468 mov #0,r0
    r[2]+=r[5]; // 8C05646A add r5,r2
    cpu.t=r[0]>=r[1]; // 8C05646C cmp/hs r1,r0
    store32(0x8C05646Eu,8u+r[15],r[2]); // 8C05646E mov.l r2,@(8,r15)
    { const bool taken=!cpu.t; // 8C056470 bf/s 0x8c056478
      r[12]=r[7]; // 8C056472 delay slot
      if (taken) goto L_8C056478; }
     // 8C056474 bra 0x8c0565fa
    goto L_8C0565FA;
L_8C056478:
    r[0]=load32(8u+r[15]); // 8C056478 mov.l @(8,r15),r0
    r[0]+=0x00000006u; // 8C05647A add #6,r0
    store32(0x8C05647Cu,8u+r[15],r[0]); // 8C05647C mov.l r0,@(8,r15)
    r[0]+=0xFFFFFFFAu; // 8C05647E add #-6,r0
    r[0]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(r[0]))); // 8C056480 mov.w @r0,r0
    store16(0x8C056482u,4u+r[15],r[0]); // 8C056482 mov.w r0,@(4,r15)
    r[0]=r[12]; // 8C056484 mov r12,r0
    r[0]+=0x00000001u; // 8C056486 add #1,r0
    cpu.t=(r[0]&0x80000000u)!=0u;r[0]<<=1u; // 8C056488 shll r0
    r[3]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(r[0]+r[5]))); // 8C05648A mov.w @(r0,r5),r3
    r[0]=r[12]; // 8C05648C mov r12,r0
    r[0]+=0x00000002u; // 8C05648E add #2,r0
    cpu.t=(r[0]&0x80000000u)!=0u;r[0]<<=1u; // 8C056490 shll r0
    store16(0x8C056492u,r[15],r[3]); // 8C056492 mov.w r3,@r15
    r[8]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(r[0]+r[5]))); // 8C056494 mov.w @(r0,r5),r8
    r[0]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(4u+r[15]))); // 8C056496 mov.w @(4,r15),r0
    r[0]=r[0]&0xFFFFu; // 8C056498 extu.w r0,r0
    cpu.t=r[6]==r[0]; // 8C05649A cmp/eq r0,r6
    if (cpu.t) goto L_8C0564AA; // 8C05649C bt 0x8c0564aa
    r[3]=r[3]&0xFFFFu; // 8C05649E extu.w r3,r3
    cpu.t=r[6]==r[3]; // 8C0564A0 cmp/eq r3,r6
    if (cpu.t) goto L_8C0564AA; // 8C0564A2 bt 0x8c0564aa
    r[8]=r[8]&0xFFFFu; // 8C0564A4 extu.w r8,r8
    cpu.t=r[6]==r[8]; // 8C0564A6 cmp/eq r8,r6
    if (!cpu.t) goto L_8C0564D0; // 8C0564A8 bf 0x8c0564d0
L_8C0564AA:
    fr[2]=load32(r[14]); // 8C0564AA fmov @r14,fr2
    r[0]=0x00000004u; // 8C0564AC mov #4,r0
    fr[3]=load32(r[13]); // 8C0564AE fmov @r13,fr3
    r[1]=r[0]; // 8C0564B0 mov r0,r1
    r[1]+=r[13]; // 8C0564B2 add r13,r1
    fpu_binary(cpu,FpuBinaryOperation::Add,5u,4u); // 8C0564B4 fadd fr5,fr4
    fpu_binary(cpu,FpuBinaryOperation::Add,3u,2u); // 8C0564B6 fadd fr3,fr2
    store_fpu32(0x8C0564B8u,r[14],fr[2]); // 8C0564B8 fmov fr2,@r14
    fr[2]=load32(r[0]+r[14]); // 8C0564BA fmov @(r0,r14),fr2
    fr[3]=load32(r[1]); // 8C0564BC fmov @r1,fr3
    fpu_binary(cpu,FpuBinaryOperation::Add,3u,2u); // 8C0564BE fadd fr3,fr2
    store_fpu32(0x8C0564C0u,r[0]+r[14],fr[2]); // 8C0564C0 fmov fr2,@(r0,r14)
    r[0]=0x00000008u; // 8C0564C2 mov #8,r0
    r[1]=r[0]; // 8C0564C4 mov r0,r1
    r[1]+=r[13]; // 8C0564C6 add r13,r1
    fr[2]=load32(r[0]+r[14]); // 8C0564C8 fmov @(r0,r14),fr2
    fr[3]=load32(r[1]); // 8C0564CA fmov @r1,fr3
    fpu_binary(cpu,FpuBinaryOperation::Add,3u,2u); // 8C0564CC fadd fr3,fr2
    store_fpu32(0x8C0564CEu,r[0]+r[14],fr[2]); // 8C0564CE fmov fr2,@(r0,r14)
L_8C0564D0:
    r[3]=r[11]&0xFFFFu; // 8C0564D0 extu.w r11,r3
    r[9]+=0x00000001u; // 8C0564D2 add #1,r9
    cpu.t=r[9]>=r[3]; // 8C0564D4 cmp/hs r3,r9
    r[13]+=0x0000000Cu; // 8C0564D6 add #12,r13
    { const bool taken=!cpu.t; // 8C0564D8 bf/s 0x8c056478
      r[12]+=0x00000003u; // 8C0564DA delay slot
      if (taken) goto L_8C056478; }
     // 8C0564DC bra 0x8c0565fa
    goto L_8C0565FA;
L_8C0564E0:
    r[2]=0x00000000u; // 8C0564E0 mov #0,r2
    r[13]=load32(r[15]); // 8C0564E2 mov.l @r15,r13
    r[1]=r[11]&0xFFFFu; // 8C0564E4 extu.w r11,r1
    r[9]=r[7]; // 8C0564E6 mov r7,r9
    r[0]=0x00000000u; // 8C0564E8 mov #0,r0
    r[2]+=r[5]; // 8C0564EA add r5,r2
    cpu.t=r[0]>=r[1]; // 8C0564EC cmp/hs r1,r0
    store32(0x8C0564EEu,12u+r[15],r[2]); // 8C0564EE mov.l r2,@(12,r15)
    { const bool taken=!cpu.t; // 8C0564F0 bf/s 0x8c0564f8
      r[12]=r[7]; // 8C0564F2 delay slot
      if (taken) goto L_8C0564F8; }
     // 8C0564F4 bra 0x8c0565fa
    goto L_8C0565FA;
L_8C0564F8:
    r[0]=load32(12u+r[15]); // 8C0564F8 mov.l @(12,r15),r0
    r[0]+=0x00000008u; // 8C0564FA add #8,r0
    store32(0x8C0564FCu,12u+r[15],r[0]); // 8C0564FC mov.l r0,@(12,r15)
    r[0]+=0xFFFFFFF8u; // 8C0564FE add #-8,r0
    r[0]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(r[0]))); // 8C056500 mov.w @r0,r0
    store16(0x8C056502u,4u+r[15],r[0]); // 8C056502 mov.w r0,@(4,r15)
    r[0]=r[12]; // 8C056504 mov r12,r0
    r[0]+=0x00000001u; // 8C056506 add #1,r0
    cpu.t=(r[0]&0x80000000u)!=0u;r[0]<<=1u; // 8C056508 shll r0
    r[0]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(r[0]+r[5]))); // 8C05650A mov.w @(r0,r5),r0
    store16(0x8C05650Cu,8u+r[15],r[0]); // 8C05650C mov.w r0,@(8,r15)
    r[0]=r[12]; // 8C05650E mov r12,r0
    r[0]+=0x00000002u; // 8C056510 add #2,r0
    cpu.t=(r[0]&0x80000000u)!=0u;r[0]<<=1u; // 8C056512 shll r0
    r[3]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(r[0]+r[5]))); // 8C056514 mov.w @(r0,r5),r3
    r[0]=r[12]; // 8C056516 mov r12,r0
    r[0]+=0x00000003u; // 8C056518 add #3,r0
    cpu.t=(r[0]&0x80000000u)!=0u;r[0]<<=1u; // 8C05651A shll r0
    store16(0x8C05651Cu,r[15],r[3]); // 8C05651C mov.w r3,@r15
    r[8]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(r[0]+r[5]))); // 8C05651E mov.w @(r0,r5),r8
    r[0]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(4u+r[15]))); // 8C056520 mov.w @(4,r15),r0
    r[0]=r[0]&0xFFFFu; // 8C056522 extu.w r0,r0
    cpu.t=r[6]==r[0]; // 8C056524 cmp/eq r0,r6
    if (cpu.t) goto L_8C05653C; // 8C056526 bt 0x8c05653c
    r[0]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(8u+r[15]))); // 8C056528 mov.w @(8,r15),r0
    r[0]=r[0]&0xFFFFu; // 8C05652A extu.w r0,r0
    cpu.t=r[6]==r[0]; // 8C05652C cmp/eq r0,r6
    if (cpu.t) goto L_8C05653C; // 8C05652E bt 0x8c05653c
    r[3]=r[3]&0xFFFFu; // 8C056530 extu.w r3,r3
    cpu.t=r[6]==r[3]; // 8C056532 cmp/eq r3,r6
    if (cpu.t) goto L_8C05653C; // 8C056534 bt 0x8c05653c
    r[8]=r[8]&0xFFFFu; // 8C056536 extu.w r8,r8
    cpu.t=r[6]==r[8]; // 8C056538 cmp/eq r8,r6
    if (!cpu.t) goto L_8C056562; // 8C05653A bf 0x8c056562
L_8C05653C:
    fr[2]=load32(r[14]); // 8C05653C fmov @r14,fr2
    r[0]=0x00000004u; // 8C05653E mov #4,r0
    fr[3]=load32(r[13]); // 8C056540 fmov @r13,fr3
    r[1]=r[0]; // 8C056542 mov r0,r1
    r[1]+=r[13]; // 8C056544 add r13,r1
    fpu_binary(cpu,FpuBinaryOperation::Add,5u,4u); // 8C056546 fadd fr5,fr4
    fpu_binary(cpu,FpuBinaryOperation::Add,3u,2u); // 8C056548 fadd fr3,fr2
    store_fpu32(0x8C05654Au,r[14],fr[2]); // 8C05654A fmov fr2,@r14
    fr[2]=load32(r[0]+r[14]); // 8C05654C fmov @(r0,r14),fr2
    fr[3]=load32(r[1]); // 8C05654E fmov @r1,fr3
    fpu_binary(cpu,FpuBinaryOperation::Add,3u,2u); // 8C056550 fadd fr3,fr2
    store_fpu32(0x8C056552u,r[0]+r[14],fr[2]); // 8C056552 fmov fr2,@(r0,r14)
    r[0]=0x00000008u; // 8C056554 mov #8,r0
    r[1]=r[0]; // 8C056556 mov r0,r1
    r[1]+=r[13]; // 8C056558 add r13,r1
    fr[2]=load32(r[0]+r[14]); // 8C05655A fmov @(r0,r14),fr2
    fr[3]=load32(r[1]); // 8C05655C fmov @r1,fr3
    fpu_binary(cpu,FpuBinaryOperation::Add,3u,2u); // 8C05655E fadd fr3,fr2
    store_fpu32(0x8C056560u,r[0]+r[14],fr[2]); // 8C056560 fmov fr2,@(r0,r14)
L_8C056562:
    r[3]=r[11]&0xFFFFu; // 8C056562 extu.w r11,r3
    r[9]+=0x00000001u; // 8C056564 add #1,r9
    cpu.t=r[9]>=r[3]; // 8C056566 cmp/hs r3,r9
    r[13]+=0x0000000Cu; // 8C056568 add #12,r13
    { const bool taken=!cpu.t; // 8C05656A bf/s 0x8c0564f8
      r[12]+=0x00000004u; // 8C05656C delay slot
      if (taken) goto L_8C0564F8; }
     // 8C05656E bra 0x8c0565fa
    goto L_8C0565FA;
L_8C056572:
    r[11]=r[11]&0xFFFFu; // 8C056572 extu.w r11,r11
    store32(0x8C056574u,12u+r[15],r[7]); // 8C056574 mov.l r7,@(12,r15)
    r[1]=0x00000000u; // 8C056576 mov #0,r1
    r[2]=r[11]; // 8C056578 mov r11,r2
    cpu.t=r[1]>=r[2]; // 8C05657A cmp/hs r2,r1
    { const bool taken=cpu.t; // 8C05657C bt/s 0x8c0565fa
      store32(0x8C05657Eu,16u+r[15],r[11]); // 8C05657E delay slot
      if (taken) goto L_8C0565FA; }
L_8C056580:
    r[0]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(r[5])));r[5]+=2u; // 8C056580 mov.w @r5+,r0
    r[3]=0x00000040u; // 8C056582 mov #64,r3
    r[3]<<=8u; // 8C056584 shll8 r3
    r[8]=r[7]; // 8C056586 mov r7,r8
    store16(0x8C056588u,8u+r[15],r[0]); // 8C056588 mov.w r0,@(8,r15)
    r[3]+=0xFFFFFFFFu; // 8C05658A add #-1,r3
    r[0]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(8u+r[15]))); // 8C05658C mov.w @(8,r15),r0
    r[0]&=r[3]; // 8C05658E and r3,r0
    store16(0x8C056590u,8u+r[15],r[0]); // 8C056590 mov.w r0,@(8,r15)
    r[9]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(r[5])));r[5]+=2u; // 8C056592 mov.w @r5+,r9
    r[0]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(8u+r[15]))); // 8C056594 mov.w @(8,r15),r0
    r[12]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(r[5])));r[5]+=2u; // 8C056596 mov.w @r5+,r12
    r[13]=load32(r[15]); // 8C056598 mov.l @r15,r13
    r[0]+=0xFFFFFFFEu; // 8C05659A add #-2,r0
    r[11]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(r[5])));r[5]+=2u; // 8C05659C mov.w @r5+,r11
    cpu.t=std::bit_cast<std::int32_t>(r[0])>0; // 8C05659E cmp/pl r0
    { const bool taken=!cpu.t; // 8C0565A0 bf/s 0x8c0565ec
      store32(0x8C0565A2u,4u+r[15],r[0]); // 8C0565A2 delay slot
      if (taken) goto L_8C0565EC; }
L_8C0565A4:
    r[9]=r[9]&0xFFFFu; // 8C0565A4 extu.w r9,r9
    cpu.t=r[6]==r[9]; // 8C0565A6 cmp/eq r9,r6
    if (cpu.t) goto L_8C0565B6; // 8C0565A8 bt 0x8c0565b6
    r[2]=r[12]&0xFFFFu; // 8C0565AA extu.w r12,r2
    cpu.t=r[6]==r[2]; // 8C0565AC cmp/eq r2,r6
    if (cpu.t) goto L_8C0565B6; // 8C0565AE bt 0x8c0565b6
    r[2]=r[11]&0xFFFFu; // 8C0565B0 extu.w r11,r2
    cpu.t=r[6]==r[2]; // 8C0565B2 cmp/eq r2,r6
    if (!cpu.t) goto L_8C0565DC; // 8C0565B4 bf 0x8c0565dc
L_8C0565B6:
    fr[2]=load32(r[14]); // 8C0565B6 fmov @r14,fr2
    r[0]=0x00000004u; // 8C0565B8 mov #4,r0
    fr[3]=load32(r[13]); // 8C0565BA fmov @r13,fr3
    r[1]=r[0]; // 8C0565BC mov r0,r1
    r[1]+=r[13]; // 8C0565BE add r13,r1
    fpu_binary(cpu,FpuBinaryOperation::Add,5u,4u); // 8C0565C0 fadd fr5,fr4
    fpu_binary(cpu,FpuBinaryOperation::Add,3u,2u); // 8C0565C2 fadd fr3,fr2
    store_fpu32(0x8C0565C4u,r[14],fr[2]); // 8C0565C4 fmov fr2,@r14
    fr[2]=load32(r[0]+r[14]); // 8C0565C6 fmov @(r0,r14),fr2
    fr[3]=load32(r[1]); // 8C0565C8 fmov @r1,fr3
    fpu_binary(cpu,FpuBinaryOperation::Add,3u,2u); // 8C0565CA fadd fr3,fr2
    store_fpu32(0x8C0565CCu,r[0]+r[14],fr[2]); // 8C0565CC fmov fr2,@(r0,r14)
    r[0]=0x00000008u; // 8C0565CE mov #8,r0
    r[1]=r[0]; // 8C0565D0 mov r0,r1
    r[1]+=r[13]; // 8C0565D2 add r13,r1
    fr[2]=load32(r[0]+r[14]); // 8C0565D4 fmov @(r0,r14),fr2
    fr[3]=load32(r[1]); // 8C0565D6 fmov @r1,fr3
    fpu_binary(cpu,FpuBinaryOperation::Add,3u,2u); // 8C0565D8 fadd fr3,fr2
    store_fpu32(0x8C0565DAu,r[0]+r[14],fr[2]); // 8C0565DA fmov fr2,@(r0,r14)
L_8C0565DC:
    r[9]=r[12]; // 8C0565DC mov r12,r9
    r[3]=load32(4u+r[15]); // 8C0565DE mov.l @(4,r15),r3
    r[12]=r[11]; // 8C0565E0 mov r11,r12
    r[8]+=0x00000001u; // 8C0565E2 add #1,r8
    r[11]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(r[5])));r[5]+=2u; // 8C0565E4 mov.w @r5+,r11
    cpu.t=std::bit_cast<std::int32_t>(r[8])>=std::bit_cast<std::int32_t>(r[3]); // 8C0565E6 cmp/ge r3,r8
    { const bool taken=!cpu.t; // 8C0565E8 bf/s 0x8c0565a4
      r[13]+=0x0000000Cu; // 8C0565EA delay slot
      if (taken) goto L_8C0565A4; }
L_8C0565EC:
    r[3]=load32(12u+r[15]); // 8C0565EC mov.l @(12,r15),r3
    r[3]+=0x00000001u; // 8C0565EE add #1,r3
    store32(0x8C0565F0u,12u+r[15],r[3]); // 8C0565F0 mov.l r3,@(12,r15)
    r[2]=load32(16u+r[15]); // 8C0565F2 mov.l @(16,r15),r2
    cpu.t=r[3]>=r[2]; // 8C0565F4 cmp/hs r2,r3
    { const bool taken=!cpu.t; // 8C0565F6 bf/s 0x8c056580
      r[5]+=0xFFFFFFFEu; // 8C0565F8 delay slot
      if (taken) goto L_8C056580; }
L_8C0565FA:
    r[2]=load32(20u+r[15]); // 8C0565FA mov.l @(20,r15),r2
    r[2]+=0x00000001u; // 8C0565FC add #1,r2
    r[3]=r[2]; // 8C0565FE mov r2,r3
    store32(0x8C056600u,20u+r[15],r[2]); // 8C056600 mov.l r2,@(20,r15)
    r[0]=static_cast<std::uint32_t>(static_cast<std::int16_t>(load16(20u+r[4]))); // 8C056602 mov.w @(20,r4),r0
    r[0]=r[0]&0xFFFFu; // 8C056604 extu.w r0,r0
    cpu.t=r[0]>r[3]; // 8C056606 cmp/hi r3,r0
    { const bool taken=!cpu.t; // 8C056608 bf/s 0x8c056610
      r[10]+=0x00000018u; // 8C05660A delay slot
      if (taken) goto L_8C056610; }
     // 8C05660C bra 0x8c056404
    goto L_8C056404;
L_8C056610:
    r[5]=r[6]; // 8C056610 mov r6,r5
    cpu.t=(r[5]&0x80000000u)!=0u;r[5]<<=1u; // 8C056612 shll r5
    r[3]=r[6]; // 8C056614 mov r6,r3
    r[2]=load32(24u+r[15]); // 8C056616 mov.l @(24,r15),r2
    r[5]+=r[3]; // 8C056618 add r3,r5
    r[5]<<=2u; // 8C05661A shll2 r5
    r[0]=0x00000004u; // 8C05661C mov #4,r0
    r[5]+=r[2]; // 8C05661E add r2,r5
    fr[3]=load32(r[5]); // 8C056620 fmov @r5,fr3
    r[6]+=0x00000001u; // 8C056622 add #1,r6
    fpu_binary(cpu,FpuBinaryOperation::Divide,4u,3u); // 8C056624 fdiv fr4,fr3
    store_fpu32(0x8C056626u,r[5],fr[3]); // 8C056626 fmov fr3,@r5
    fr[2]=load32(r[0]+r[5]); // 8C056628 fmov @(r0,r5),fr2
    fpu_binary(cpu,FpuBinaryOperation::Divide,4u,2u); // 8C05662A fdiv fr4,fr2
    store_fpu32(0x8C05662Cu,r[0]+r[5],fr[2]); // 8C05662C fmov fr2,@(r0,r5)
    r[0]=0x00000008u; // 8C05662E mov #8,r0
    fr[3]=load32(r[0]+r[5]); // 8C056630 fmov @(r0,r5),fr3
    fpu_binary(cpu,FpuBinaryOperation::Divide,4u,3u); // 8C056632 fdiv fr4,fr3
    store_fpu32(0x8C056634u,r[0]+r[5],fr[3]); // 8C056634 fmov fr3,@(r0,r5)
    r[3]=load32(28u+r[15]); // 8C056636 mov.l @(28,r15),r3
    cpu.t=r[6]>=r[3]; // 8C056638 cmp/hs r3,r6
    if (cpu.t) goto L_8C056640; // 8C05663A bt 0x8c056640
     // 8C05663C bra 0x8c0563d8
    goto L_8C0563D8;
L_8C056640:
    r[15]+=0x00000020u; // 8C056640 add #32,r15
    r[8]=load32(r[15]);r[15]+=4u; // 8C056642 mov.l @r15+,r8
    r[9]=load32(r[15]);r[15]+=4u; // 8C056644 mov.l @r15+,r9
    r[10]=load32(r[15]);r[15]+=4u; // 8C056646 mov.l @r15+,r10
    r[11]=load32(r[15]);r[15]+=4u; // 8C056648 mov.l @r15+,r11
    r[12]=load32(r[15]);r[15]+=4u; // 8C05664A mov.l @r15+,r12
    r[13]=load32(r[15]);r[15]+=4u; // 8C05664C mov.l @r15+,r13
    r[14]=load32(r[15]);r[15]+=4u; // 8C056650 RTS delay slot
    cpu.pc=cpu.pr;return true;
}
} // namespace sonic::vertex_normals
