"""Generate a SHA-bound arithmetic context for fully guarded native bodies.

No retained SDK files or existing FPU experiments are modified. The emitted
header uses the production SDK epoch/TLS and original FPU fallback entrypoints.
--self-test authenticates and generates in memory without writing outputs.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

SOURCE_MEMBER = 'src/runtime/fpu.cpp'
SOURCE_SHA = 'e0a2ec1ad05dcb884b69d7b30708a5b8184e09e6c2e77e55259f6ba6cc8126af'
HEADER_MEMBER = 'include/katana/runtime/fpu.hpp'
HEADER_SHA = 'be73f6dfdc85abba4696e60089cfc99b68995b35ef18afe8b02659c71787ea1d'
SCHEMA = 'sarecomp-fpu-body-v1'

PREFIX = '''// Generated from the authenticated retained SDK. Do not hand-edit.
#pragma once
#include "katana/runtime/fpu.hpp"
#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <optional>
#if defined(_MSC_VER)
#define SONIC_FPU_BODY_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define SONIC_FPU_BODY_INLINE inline __attribute__((always_inline))
#else
#error This experiment requires a forceinline-capable compiler
#endif
namespace sonic::fpu_body {
namespace detail {
using namespace katana::runtime;
'''

SUFFIX = '''
} // namespace detail

// This context replaces arithmetic-helper work, not instruction admission or
// an entire guest function. The native body owns source identity, memory and
// instruction legality before mutation. Check admitted() before its first op.
// For the context's lifetime PR/Enables/DN/RM, FD and trap/sleep state must stay
// admitted; only FPSCR Cause/Sticky/SZ/FR may change. Reads always use live FRs.
// Methods assert this admission in debug builds; release builds have no repeated
// mode check. The context is noncopyable and must end before returning to guest
// dispatch or invoking arbitrary callbacks. FP status commits after EVERY op.
class NontrappingSingleBody final {
public:
    explicit NontrappingSingleBody(katana::runtime::CpuState& cpu) noexcept
        : cpu_(cpu), rounding_(static_cast<std::uint8_t>(cpu.fpscr &
                         katana::runtime::fpscr_rounding_mode_mask)),
          admitted_((cpu.fpscr & (katana::runtime::fpscr_pr_mask |
                                 katana::runtime::fpscr_exception_enable_mask)) == 0u &&
                    (cpu.fpscr & katana::runtime::fpscr_dn_mask) != 0u && rounding_ <= 1u &&
                    (cpu.sr & katana::runtime::sr_fd_mask) == 0u &&
                    !cpu.trap_pending && !cpu.sleeping) {}
    // A synchronous caller may already own the matching SDK epoch. Borrow it
    // instead of resetting host status again at the first exceptional operand.
    // The epoch must belong to this CPU and outlive the complete body; no
    // callback, dispatch or guest RM/DN/PR/Enables mutation is allowed inside.
    NontrappingSingleBody(katana::runtime::CpuState& cpu,
                         const katana::runtime::HostFpuExecutionEpoch&) noexcept
        : NontrappingSingleBody(cpu) { borrowed_epoch_ = true; }
    NontrappingSingleBody(const NontrappingSingleBody&) = delete;
    NontrappingSingleBody& operator=(const NontrappingSingleBody&) = delete;
    NontrappingSingleBody(NontrappingSingleBody&&) = delete;
    NontrappingSingleBody& operator=(NontrappingSingleBody&&) = delete;

    [[nodiscard]] bool admitted() const noexcept { return admitted_; }

    template<katana::runtime::FpuBinaryOperation Operation, unsigned Source, unsigned Destination>
    SONIC_FPU_BODY_INLINE void binary() noexcept {
        using namespace katana::runtime;
        static_assert(Source < 16u && Destination < 16u);
        static_assert(Operation == FpuBinaryOperation::Add || Operation == FpuBinaryOperation::Subtract ||
                      Operation == FpuBinaryOperation::Multiply || Operation == FpuBinaryOperation::Divide);
        assert(admitted_);
        const auto n = cpu_.fr[Destination], m = cpu_.fr[Source];
        detail::SingleBinaryResult result;
        bool accepted;
        if constexpr (Operation == FpuBinaryOperation::Multiply)
            accepted = detail::try_normal_single_product(n, m, rounding_, result);
        else if constexpr (Operation == FpuBinaryOperation::Divide)
            accepted = detail::try_normal_single_quotient(n, m, rounding_, result);
        else
            accepted = detail::try_normal_single_sum(n, m,
                Operation == FpuBinaryOperation::Subtract, rounding_, result);
        if (accepted && (result.causes & ~fpscr_cause_inexact_mask) == 0u) {
            // Exact all-Enables=0 reduction of the retained clear/finish/signal
            // sequence. Preserve sticky flags and live SZ/FR; clear old Causes.
            cpu_.fpscr = (cpu_.fpscr & ~fpscr_cause_mask) | result.causes |
                         ((result.causes >> 10u) & fpscr_flag_mask);
            cpu_.fr[Destination] = result.bits;
            return;
        }
        ensure_epoch();
        // DN=1 plus Enables=0 rules out unmaskable denormal-operand Cause.E and
        // enabled arithmetic traps. Thus no delay-owner exception can arise in
        // an admitted context. All exceptional value/flag behavior stays SDK.
        static_cast<void>(fpu_binary(cpu_, Operation, Source, Destination, std::nullopt));
    }

    template<unsigned Source, unsigned Destination>
    SONIC_FPU_BODY_INLINE void compare_equal() noexcept {
        using namespace katana::runtime;
        static_assert(Source < 16u && Destination < 16u);
        assert(admitted_);
        const auto n = cpu_.fr[Destination], m = cpu_.fr[Source];
COMPARE_FAST_BLOCK
        ensure_epoch();
        // Retain the SDK's exact NaN/denormal comparison semantics; do not
        // replace them with a different interpretation of architectural flags.
        fpu_compare_equal(cpu_, Source, Destination);
    }

private:
    void ensure_epoch() noexcept {
        if (!borrowed_epoch_ && !epoch_) epoch_.emplace(cpu_);
    }
    katana::runtime::CpuState& cpu_;
    const std::uint8_t rounding_;
    const bool admitted_;
    bool borrowed_epoch_ = false;
    // No separate TLS. The first fallback acquires the production SDK epoch;
    // its destructor restores exact incoming MXCSR and any enclosing epoch.
    // An entirely integer body never constructs it or touches host FP state.
    std::optional<katana::runtime::HostFpuExecutionEpoch> epoch_;
};
} // namespace sonic::fpu_body
#undef SONIC_FPU_BODY_INLINE
'''


def sha(data):
    return hashlib.sha256(data).hexdigest()


def extract(source, first, after):
    if source.count(first) != 1 or source.count(after) != 1:
        raise ValueError('Retained extraction boundary changed')
    start = source.index(first)
    end = source.index(after, start)
    return source[start:end]


def generate(sdk):
    with zipfile.ZipFile(sdk) as archive:
        raw = archive.read(SOURCE_MEMBER)
        sdk_header = archive.read(HEADER_MEMBER)
    if sha(raw) != SOURCE_SHA or sha(sdk_header) != HEADER_SHA:
        raise ValueError('Retained SDK FPU source/header SHA mismatch')
    source = raw.decode('utf-8')
    arithmetic = extract(source, 'struct SingleBinaryResult {',
                         'template <typename Float> std::uint32_t truncate_to_integer_bits(')
    if arithmetic.count('[[nodiscard]] bool ') != 3:
        raise ValueError('Expected precisely product/sum/quotient integer helpers')
    predicate = extract(source, '[[nodiscard]] bool normal_or_zero_single_bits(',
                        '\n} // namespace\n\nvoid fpu_compare_equal(')
    if predicate.count('[[nodiscard]] bool ') != 1:
        raise ValueError('Compare predicate extraction changed')
    helpers = arithmetic + predicate
    emitted = helpers.replace('[[nodiscard]] bool ', '[[nodiscard]] SONIC_FPU_BODY_INLINE bool ')
    if emitted.replace('SONIC_FPU_BODY_INLINE ', '') != helpers:
        raise ValueError('Integer helper bodies changed')
    compare = extract(source, 'void fpu_compare_equal(CpuState& cpu,',
                      'void fpu_compare_greater(CpuState& cpu,')
    start = compare.index('        if (normal_or_zero_single_bits(n)')
    end = compare.index('\n        }', start) + len('\n        }')
    original_fast = compare[start:end]
    fast = original_fast.replace('normal_or_zero_single_bits(', 'detail::normal_or_zero_single_bits(')
    fast = fast.replace('clear_fpu_causes(cpu);', 'cpu.fpscr &= ~fpscr_cause_mask;').replace('cpu.', 'cpu_.')
    reverse_fast = fast.replace('cpu_.', 'cpu.').replace('cpu.fpscr &= ~fpscr_cause_mask;', 'clear_fpu_causes(cpu);')
    reverse_fast = reverse_fast.replace('detail::normal_or_zero_single_bits(', 'normal_or_zero_single_bits(')
    if reverse_fast != original_fast:
        raise ValueError('Normal comparison changed beyond the exact cause-clear reduction')
    header = (PREFIX + emitted + SUFFIX.replace('COMPARE_FAST_BLOCK', fast)).encode('utf-8')
    report = {
        'schema': SCHEMA, 'sdk_source_member': SOURCE_MEMBER, 'sdk_source_sha256': SOURCE_SHA,
        'sdk_header_member': HEADER_MEMBER, 'sdk_header_sha256': HEADER_SHA,
        'integer_helpers_sha256': sha(arithmetic.encode()), 'predicate_sha256': sha(predicate.encode()),
        'compare_normal_block_sha256': sha(original_fast.encode()),
        'reverse_helpers_sha256': sha(emitted.replace('SONIC_FPU_BODY_INLINE ', '').encode()),
        'reverse_compare_block_sha256': sha(reverse_fast.encode()),
        'header_sha256': sha(header), 'header_bytes': len(header),
        'arithmetic_changes': 'forceinline declaration decoration only; integer bodies verbatim',
        'compare_changes': 'normal predicate verbatim; exact Cause clear; special cases original helper',
        'admission': 'once: PR=0, Enables=0, DN=1, RM in {0,1}, FD=0, no pending trap/sleep',
        'lifetime': 'admitted mode stays fixed; SZ/FR/Cause/Sticky may change; live register arrays',
        'epoch': 'lazy production HostFpuExecutionEpoch on first fallback; exact RAII restore; no new TLS',
        'product_symbols': 'header-only sonic::fpu_body::NontrappingSingleBody; existing SDK fallback ABI',
        'not_performed': ['compilation', 'differential execution', 'benchmark'],
    }
    return {'sonic_fpu_body.hpp': header, 'provenance.json': (json.dumps(report, indent=2) + '\n').encode()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    sdk = args.sdk.resolve(strict=True)
    outputs = generate(sdk)
    if args.self_test:
        print('FPU_BODY_GENERATOR_TEST_OK source_header_SHA integer_reverse compare_reverse lazy_original_epoch')
        return
    if args.output_dir is None:
        parser.error('--output-dir is required unless --self-test is selected')
    destination = args.output_dir.resolve()
    root = Path(__file__).resolve().parents[1]
    relative = destination.relative_to(root)
    if (not relative.parts or not relative.parts[0].startswith('build-') or
            destination == sdk.parent or sdk.parent in destination.parents or destination in sdk.parent.parents):
        raise ValueError('Output must be a separate Sonic build-* directory')
    for name in outputs:
        target = destination / name
        if target.is_symlink() or (target.exists() and (not target.is_file() or target.stat().st_nlink > 1)):
            raise ValueError('Refusing linked/nonregular output: ' + str(target))
    provenance = destination / 'provenance.json'
    if provenance.exists() and json.loads(provenance.read_text())['schema'] != SCHEMA:
        raise ValueError('Output directory belongs to another experiment')
    destination.mkdir(parents=True, exist_ok=True)
    for name, data in outputs.items():
        target = destination / name
        if not target.exists() or target.read_bytes() != data:
            target.write_bytes(data)
    print('SONIC_FPU_BODY_PREPARED source=AUTHENTICATED integer_bodies=UNCHANGED compare_reverse=PASS')


if __name__ == '__main__':
    main()
