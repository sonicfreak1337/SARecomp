"""Prepare a reversible, SHA-bound runtime FPU experiment without changing SDK.

Product: compile fpu.cpp in place of the retained fpu.cpp archive member.
Differential test: additionally compile fpu-reference.cpp, include this output
directory, and define SONIC_FPU_RUNTIME_TEST_PROBE for the candidate object.
The reference has renamed public symbols and its own retained epoch TLS.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile

SOURCE_MEMBER = 'src/runtime/fpu.cpp'
SOURCE_SHA = 'e0a2ec1ad05dcb884b69d7b30708a5b8184e09e6c2e77e55259f6ba6cc8126af'
HEADER_MEMBER = 'include/katana/runtime/fpu.hpp'
HEADER_SHA = 'be73f6dfdc85abba4696e60089cfc99b68995b35ef18afe8b02659c71787ea1d'
PUBLIC_NAMES = (
    'HostFpuExecutionEpoch', 'FpuBinaryOperation',
    'read_fr_single', 'write_fr_single', 'read_dr_double', 'write_dr_double',
    'read_fpu_pair_bits', 'write_fpu_pair_bits', 'fpu_binary', 'fpu_absolute',
    'fpu_negate', 'fpu_square_root', 'fpu_reciprocal_square_root', 'fpu_sine_cosine',
    'fpu_inner_product', 'fpu_transform_vector', 'try_fpu_transform_vector_simd',
    'fpu_multiply_accumulate', 'fpu_compare_equal', 'fpu_compare_greater',
    'fpu_float_from_fpul', 'fpu_truncate_to_fpul', 'fpu_convert_double_to_single',
    'fpu_convert_single_to_double',
)
# These scalar implementations have external linkage for the retained SIMD
# dispatcher even though fpu.hpp does not publish them. The test reference
# must not define a second copy under the production names.
DETAIL_NAMES = ('fpu_inner_product_scalar', 'fpu_transform_vector_scalar',
                'fpu_multiply_accumulate_scalar')
RENAMES = {name: 'SonicFpuReference_' + name for name in PUBLIC_NAMES + DETAIL_NAMES}
MACRO = '''// Sonic runtime experiment: arithmetic bodies below remain verbatim.
#if defined(_MSC_VER)
#define SONIC_FPU_RUNTIME_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define SONIC_FPU_RUNTIME_INLINE inline __attribute__((always_inline))
#else
#error This bounded experiment requires a forceinline-capable compiler
#endif

'''
FAST = '''namespace {
// This replaces helper work only, never instruction admission. The retained
// overload has no FD/register-legality precheck; AOT owns those checks. Reject
// FD, out-of-range encoded indices and pending traps conservatively, then let
// the untouched original helper apply its existing behavior (including index
// masking). Old Cause/Sticky/exception-provenance fields are not new traps.
[[nodiscard]] SONIC_FPU_RUNTIME_INLINE bool sonic_try_nontrapping_fpu_binary(
    CpuState& cpu, const FpuBinaryOperation operation,
    const std::uint8_t source, const std::uint8_t destination) noexcept {
    if ((cpu.fpscr & (fpscr_pr_mask | fpscr_exception_enable_mask)) != 0u ||
        (cpu.sr & sr_fd_mask) != 0u || source >= 16u || destination >= 16u ||
        cpu.trap_pending) return false;
    const auto n = cpu.fr[destination];
    const auto m = cpu.fr[source];
    const auto rounding = guest_rounding_mode(cpu);
    SingleBinaryResult result;
    bool accepted = false;
    switch (operation) {
    case FpuBinaryOperation::Multiply:
        accepted = try_normal_single_product(n, m, rounding, result); break;
    case FpuBinaryOperation::Add:
    case FpuBinaryOperation::Subtract:
        accepted = try_normal_single_sum(n, m,
            operation == FpuBinaryOperation::Subtract, rounding, result); break;
    case FpuBinaryOperation::Divide:
        accepted = try_normal_single_quotient(n, m, rounding, result); break;
    default: return false;
    }
    if (!accepted || (result.causes & ~fpscr_cause_inexact_mask) != 0u)
        return false;
    // Exact all-Enables=0 reduction of clear_fpu_causes, finish_arithmetic and
    // signal_fpu_exception. No callback or host FP operation on this path.
    cpu.fpscr = (cpu.fpscr & ~fpscr_cause_mask) | result.causes |
                ((result.causes >> 10u) & fpscr_flag_mask);
    cpu.fr[destination] = result.bits;
    return true;
}
} // namespace

#if defined(SONIC_FPU_RUNTIME_TEST_PROBE)
bool sonic_fpu_runtime_try_fast_for_test(CpuState& cpu,
    const FpuBinaryOperation operation, const std::uint8_t source,
    const std::uint8_t destination) noexcept {
    return sonic_try_nontrapping_fpu_binary(cpu, operation, source, destination);
}
#endif

'''
VOID_HEAD = '''void fpu_binary(CpuState& cpu,
                const FpuBinaryOperation operation,
                const std::uint8_t source,
                const std::uint8_t destination) noexcept {
'''
BOOL_HEAD = '''bool fpu_binary(CpuState& cpu, const FpuBinaryOperation operation,
                const std::uint8_t source, const std::uint8_t destination,
                const std::optional<std::uint32_t> delay_owner) noexcept {
'''
PROBE_HEADER = '''#pragma once
#include "katana/runtime/fpu.hpp"
namespace katana::runtime {
bool sonic_fpu_runtime_try_fast_for_test(CpuState&, FpuBinaryOperation,
    std::uint8_t source, std::uint8_t destination) noexcept;
}
'''


def sha(data):
    return hashlib.sha256(data).hexdigest()


def renamed(text, mapping):
    pattern = re.compile(r'\b(' + '|'.join(map(re.escape, mapping)) + r')\b')
    return pattern.sub(lambda match: mapping[match[0]], text)


def generate(sdk):
    with zipfile.ZipFile(sdk) as archive:
        source_raw = archive.read(SOURCE_MEMBER)
        header_raw = archive.read(HEADER_MEMBER)
    if sha(source_raw) != SOURCE_SHA or sha(header_raw) != HEADER_SHA:
        raise ValueError('Retained FPU source/header SHA mismatch')
    original, header = source_raw.decode('utf-8'), header_raw.decode('utf-8')
    candidate, edits = original, []

    def change(before, after):
        nonlocal candidate
        if candidate.count(before) != 1:
            raise ValueError('FPU substitution boundary changed: ' + before[:100])
        candidate = candidate.replace(before, after, 1)
        edits.append((before, after))

    change('namespace katana::runtime {', MACRO + 'namespace katana::runtime {')
    for helper in ('product', 'sum', 'quotient'):
        before = '[[nodiscard]] bool try_normal_single_' + helper + '('
        change(before, before.replace('bool ', 'SONIC_FPU_RUNTIME_INLINE bool ', 1))
    change(VOID_HEAD, FAST + VOID_HEAD +
           '    if (sonic_try_nontrapping_fpu_binary(cpu, operation, source, destination)) return;\n')
    change(BOOL_HEAD, BOOL_HEAD +
           '    if (sonic_try_nontrapping_fpu_binary(cpu, operation, source, destination)) return false;\n')
    change('} // namespace katana::runtime',
           '} // namespace katana::runtime\n#undef SONIC_FPU_RUNTIME_INLINE')
    reverse = candidate
    for before, after in reversed(edits):
        if reverse.count(after) != 1:
            raise ValueError('Ambiguous reverse substitution')
        reverse = reverse.replace(after, before, 1)
    if reverse.encode('utf-8') != source_raw:
        raise ValueError('Candidate changes escaped the exact substitutions')
    # The entire arithmetic region must remain byte-identical after removing
    # only the three inline declaration decorations.
    first, after = 'struct SingleBinaryResult {', 'template <typename Float> std::uint32_t truncate_to_integer_bits('
    body = original[original.index(first):original.index(after)]
    candidate_body = candidate[candidate.index(first):candidate.index(after)]
    if candidate_body.replace('SONIC_FPU_RUNTIME_INLINE ', '') != body:
        raise ValueError('Retained integer arithmetic changed')
    if any(name not in header for name in PUBLIC_NAMES):
        raise ValueError('Reference public symbol list changed')
    reference_header = renamed(header, RENAMES)
    reference = renamed(original, RENAMES)
    original_include = '#include "katana/runtime/fpu.hpp"'
    reference_include = '#include "sonic_fpu_reference.hpp"'
    if reference.count(original_include) != 1:
        raise ValueError('Reference include boundary changed')
    reference = reference.replace(original_include, reference_include, 1)
    inverse_names = {value: key for key, value in RENAMES.items()}
    if (renamed(reference.replace(reference_include, original_include, 1), inverse_names) != original or
            renamed(reference_header, inverse_names) != header):
        raise ValueError('Reference changed beyond reversible public-symbol renames')
    outputs = {'fpu.cpp': candidate.encode(), 'fpu-reference.cpp': reference.encode(),
               'sonic_fpu_reference.hpp': reference_header.encode(),
               'sonic_fpu_runtime_probe.hpp': PROBE_HEADER.encode()}
    report = {
        'schema': 'sarecomp-fpu-runtime-v1', 'sdk_source_member': SOURCE_MEMBER,
        'sdk_source_sha256': SOURCE_SHA, 'sdk_header_member': HEADER_MEMBER,
        'sdk_header_sha256': HEADER_SHA, 'reverse_source_sha256': sha(reverse.encode()),
        'integer_helpers_sha256': sha(body.encode()), 'arithmetic_changes': 'inline decoration only',
        'substitutions': [{'before': before, 'after': after} for before, after in edits],
        'reference_public_renames': RENAMES,
        'outputs': {name: {'bytes': len(data), 'sha256': sha(data)} for name, data in outputs.items()},
        'product_symbols': 'original FPU ABI; exactly one production fpu.cpp member and epoch TLS',
        'test_probe_define': 'SONIC_FPU_RUNTIME_TEST_PROBE',
        'fast_admission': 'PR=0, all Enables=0, FD=0, indices<16, no pending trap; causes subset of I',
        'fallback': 'unaltered retained bodies; original indices, delay_owner and instruction contract',
        'not_performed': ['compilation', 'differential execution', 'link audit', 'benchmark'],
    }
    outputs['provenance.json'] = (json.dumps(report, indent=2) + '\n').encode()
    return outputs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    sdk, destination = args.sdk.resolve(strict=True), args.output_dir.resolve()
    project = Path(__file__).resolve().parents[1]
    relative = destination.relative_to(project)
    if (not relative.parts or not relative.parts[0].startswith('build-') or
            destination == sdk.parent or sdk.parent in destination.parents or
            destination in sdk.parent.parents):
        raise ValueError('Output must be a separate Sonic build-* directory')
    outputs = generate(sdk)
    for name in outputs:
        target = destination / name
        if target.is_symlink() or (target.exists() and
                (not target.is_file() or target.stat().st_nlink > 1)):
            raise ValueError('Refusing linked/nonregular output: ' + str(target))
    destination.mkdir(parents=True, exist_ok=True)
    for name, data in outputs.items():
        target = destination / name
        if not target.exists() or target.read_bytes() != data:
            target.write_bytes(data)
    print('SONIC_FPU_RUNTIME_PREPARED source_reverse=PASS integer_bodies=UNCHANGED reference_renames=PASS')


if __name__ == '__main__':
    main()
