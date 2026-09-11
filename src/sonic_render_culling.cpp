#include "sonic_render_culling.hpp"
#include "sonic_presentation.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port.hpp"
#include <bit>
#include <cmath>
#include <cstdio>
#include <exception>
#include "katana/runtime/crash_capsule.hpp"

namespace sonic::presentation {
namespace {
using namespace katana::runtime;
// Both original leaves reuse FR8 in a later vertical comparison. Restore its
// original bits immediately, while keeping the widened FCMP's T/FPSCR result.
void compare_x(CpuState& cpu, std::uint8_t point, float extra) {
    const auto original = cpu.fr[8];
    const auto value = std::bit_cast<float>(original);
    if (extra != 0.0f && std::isfinite(value))
        cpu.fr[8] = std::bit_cast<std::uint32_t>(value + extra);
    fpu_compare_greater(cpu, 8u, point);
    cpu.fr[8] = original;
}
void add(CpuState& c, std::uint8_t m, std::uint8_t n) {
    fpu_binary(c, FpuBinaryOperation::Add, m, n);
}
void sub(CpuState& c, std::uint8_t m, std::uint8_t n) {
    fpu_binary(c, FpuBinaryOperation::Subtract, m, n);
}
void mul(CpuState& c, std::uint8_t m, std::uint8_t n) {
    fpu_binary(c, FpuBinaryOperation::Multiply, m, n);
}
void div(CpuState& c, std::uint8_t m, std::uint8_t n) {
    fpu_binary(c, FpuBinaryOperation::Divide, m, n);
}
std::uint32_t load(CpuState& c, std::uint32_t a) { return guest_read_u32(c,a); }
std::uint32_t post(CpuState& c, std::uint8_t r) {
    const auto value=load(c,c.r[r]); c.r[r]+=4u; return value;
}
void store_gbr(CpuState& c, std::uint32_t offset) {
    guest_write_u32(c,c.gbr+offset,c.r[0],CodeWriteSource::Cpu);
}
bool supported(const CpuState& c) {
    return !c.fpu_disabled() && !c.fpu_double_precision() && !c.fpu_transfer_pair()
        && (c.read_fpscr() & fpscr_rounding_mode_mask)<=1u
        && (c.read_fpscr() & fpscr_exception_enable_mask)==0u;
}
NativePortHookResult cull_failure(NativePortContext& context, const char* message) noexcept {
    constexpr std::uint32_t error=0x53415743u; // SA widescreen cull, not graphics budget.
    if (context.crash_capsule)
        context.crash_capsule->note_v3_contract_detail(message);
    std::fprintf(stderr,"SONIC_WIDESCREEN_CULL_FAILURE pc=%08X pr=%08X r4=%08X gbr=%08X detail=%s\n",
        context.cpu->pc,context.cpu->pr,context.cpu->r[4],context.cpu->gbr,message);
    return {NativePortHookAction::Abort,0u,error};
}
}

void basic_model_cull(katana::runtime::CpuState& c, float extra) {
    using namespace katana::runtime;
    HostFpuExecutionEpoch epoch(c);
    const auto reject=[&] { c.t=true; };
    // 03718C..0371B6: sphere transform and unchanged near/far gates.
    c.r[0]=c.r[4]+24u;
    c.fr[0]=post(c,0); c.fr[1]=post(c,0); c.fr[2]=post(c,0);
    c.fr[3]=0x3f800000u; fpu_transform_vector(c,0u); c.fr[3]=post(c,0);
    c.r[0]=load(c,c.gbr+16u); c.fpul=c.r[0]; c.fr[4]=c.fpul;
    sub(c,3,4); fpu_compare_greater(c,4u,2u);
    if (!c.t) return reject();
    c.r[1]=0x8c88f554u; c.fr[4]=load(c,c.r[1]); add(c,3,4);
    fpu_compare_greater(c,4u,2u); if (c.t) return reject();
    c.fr[5]=0x3f800000u; add(c,3,2); div(c,2,5);
    // 0371B8..0371E2: only these two comparisons widen.
    c.r[0]=load(c,c.gbr+8u); c.fpul=c.r[0]; c.fr[4]=c.fpul; c.fr[6]=c.fr[4];
    c.r[3]=0x8c88f540u; c.fr[8]=post(c,3);
    c.r[1]=0x8c88f530u; c.fr[4]=post(c,1); mul(c,5,6); c.fr[11]=c.fr[0];
    add(c,3,0); mul(c,6,0); add(c,4,0); compare_x(c,0,-extra);
    if (!c.t) return reject();
    c.fr[9]=post(c,3); c.fr[8]=post(c,3);
    sub(c,3,11); mul(c,6,11); add(c,4,11); compare_x(c,11,extra);
    if (c.t) return reject();
    // 0371E4..03720A: retain the original cross-register Y calculations.
    c.r[0]=load(c,c.gbr+12u); c.fpul=c.r[0]; c.fr[7]=c.fpul;
    mul(c,4,7); c.fr[4]=post(c,1); mul(c,5,7); c.fr[11]=c.fr[1];
    add(c,3,1); mul(c,7,1); add(c,4,1); fpu_compare_greater(c,9u,1u);
    if (!c.t) return reject();
    c.fr[9]=post(c,3); sub(c,3,11); mul(c,6,11); add(c,4,11);
    fpu_compare_greater(c,8u,11u); if (c.t) return reject();
    // 037218..037274: visible models must publish the complete material ABI.
    c.r[0]=0u; c.r[2]=0x8c88f56cu; c.r[6]=load(c,c.r[2]); c.r[3]=4u;
    c.t=(c.r[3]&c.r[6])==0u; if (!c.t) c.r[0]=1u; store_gbr(c,28u);
    c.r[0]=0x8c88f5a0u; c.r[0]=load(c,c.r[0]); store_gbr(c,32u);
    c.r[0]=0x8c88f5a4u; c.r[0]=load(c,c.r[0]); store_gbr(c,36u);
    c.r[0]=0u; c.r[3]=0x20u; c.t=(c.r[3]&c.r[6])==0u;
    if (!c.t) c.r[0]=0xffffffffu;
    c.r[3]=0x10u; c.t=(c.r[3]&c.r[6])==0u; if (!c.t) c.r[0]=1u;
    store_gbr(c,40u);
    c.r[2]=load(c,c.r[4]+12u); c.r[3]=load(c,c.r[4]+16u);
    c.r[0]=static_cast<std::uint32_t>(guest_read_s16(c,c.r[2]));
    c.r[6]=0x3fffu; c.r[0]&=c.r[6]; c.r[5]=20u;
    c.macl=(c.r[0]&0xffffu)*(c.r[5]&0xffffu);
    c.r[5]=c.macl; c.r[5]+=c.r[3]; c.r[0]=c.r[5]; store_gbr(c,52u);
    c.r[0]=load(c,c.r[5]+16u); c.r[1]=0x8c8ffe1cu; c.r[6]=post(c,1);
    c.t=c.r[6]==0u;
    if (!c.t) { c.r[6]=post(c,1); c.r[0]&=c.r[6]; c.r[6]=post(c,1); c.r[0]|=c.r[6]; }
    store_gbr(c,44u); c.t=false;
}

bool draw_sphere_caller(std::uint32_t pr) noexcept {
    // These source-reviewed display owners only draw/set render state or
    // advance presentation-only animation (see docs/widescreen-culling.md). The
    // shared query also serves movement/activation, so unknown callers must
    // retain the original bounds. Task registration alone is not proof.
    switch (pr) {
    case 0x8c0366b6u:
    case 0x8c048318u:
    case 0x8c048436u:
    case 0x8c0599e0u:
    case 0x8c05a7bcu:
    case 0x8c05ac82u:
    case 0x8c05ad5eu:
    case 0x8c05ae62u:
    case 0x8c05d4e6u:
    case 0x8c05dbc6u:
    case 0x8c06399eu:
    case 0x8c064500u:
    case 0x8c0650e0u:
    case 0x8c0676eau:
    case 0x8c069b1eu:
    // Hint monitor: 0613BC draws the 060E86-cloned model. Its separate
    // 061300 proximity/state update still controls whether the screen is on.
    case 0x8c061400u:
        return true;
    default:
        return false;
    }
}
void draw_sphere_cull(katana::runtime::CpuState& c, float extra) {
    using namespace katana::runtime;
    HostFpuExecutionEpoch epoch(c);
    c.r[0]=0x8c754e08u; c.r[0]=load(c,c.r[0]); c.t=c.r[0]==0u;
    const auto reject=[&] { c.r[0]=0u; };
    if (!c.t) return reject();
    c.fr[0]=c.fr[7]; c.fr[7]=0x3f800000u; fpu_transform_vector(c,4u);
    c.r[1]=0x8c88f550u; c.fr[7]=load(c,c.r[1]); sub(c,0,7);
    fpu_compare_greater(c,7u,6u); if (!c.t) return reject();
    c.r[1]=0x8c88f554u; c.fr[7]=load(c,c.r[1]); add(c,0,7);
    fpu_compare_greater(c,7u,6u); if (c.t) return reject();
    c.fr[1]=0x3f800000u; add(c,0,6); div(c,6,1);
    c.r[1]=0x8c88f524u; c.fr[7]=load(c,c.r[1]);
    c.r[2]=0x8c88f558u; c.fr[2]=load(c,c.r[2]); mul(c,7,2);
    c.r[3]=0x8c88f540u; c.fr[8]=post(c,3);
    c.r[1]+=12u; c.fr[7]=post(c,1); mul(c,1,2); c.fr[11]=c.fr[4];
    add(c,0,4); mul(c,2,4); add(c,7,4); compare_x(c,4,-extra);
    if (!c.t) return reject();
    c.fr[9]=post(c,3); c.fr[8]=post(c,3);
    sub(c,0,11); mul(c,2,11); add(c,7,11); compare_x(c,11,extra);
    if (c.t) return reject();
    c.r[2]=0x8c88f55cu; c.fr[3]=load(c,c.r[2]); mul(c,7,3);
    c.fr[7]=post(c,1); mul(c,1,3); c.fr[11]=c.fr[5];
    add(c,0,5); mul(c,3,5); add(c,7,5); fpu_compare_greater(c,9u,5u);
    if (!c.t) return reject();
    c.fr[9]=post(c,3); sub(c,0,11); mul(c,2,11); add(c,7,11);
    fpu_compare_greater(c,8u,11u); if (c.t) return reject();
    c.r[0]=1u;
}
}

extern "C" katana::runtime::NativePortHookResult
sonic_native_widescreen_model_cull(katana::runtime::NativePortContext& context) noexcept {
    using namespace katana::runtime;
    if (!sonic::presentation::settings().widescreen || !context.cpu ||
        !sonic::presentation::supported(*context.cpu))
        return {NativePortHookAction::ContinueOriginal,0u,0u};
    try {
        sonic::presentation::basic_model_cull(*context.cpu,sonic::presentation::extra_horizontal_pixels());
        return {NativePortHookAction::Return,0u,0u};
    } catch (const std::exception& error) {
        return sonic::presentation::cull_failure(context,error.what());
    } catch (...) {
        return sonic::presentation::cull_failure(context,"unknown-model-cull-exception");
    }
}
extern "C" katana::runtime::NativePortHookResult
sonic_native_widescreen_draw_sphere_cull(katana::runtime::NativePortContext& context) noexcept {
    using namespace katana::runtime;
    if (!sonic::presentation::settings().widescreen || !context.cpu ||
        !sonic::presentation::draw_sphere_caller(context.cpu->pr) ||
        !sonic::presentation::supported(*context.cpu))
        return {NativePortHookAction::ContinueOriginal,0u,0u};
    try {
        sonic::presentation::draw_sphere_cull(*context.cpu,sonic::presentation::extra_horizontal_pixels());
        return {NativePortHookAction::Return,0u,0u};
    } catch (const std::exception& error) {
        return sonic::presentation::cull_failure(context,error.what());
    } catch (...) {
        return sonic::presentation::cull_failure(context,"unknown-sphere-cull-exception");
    }
}
