"""Generate an isolated, source-bound arithmetic header; never modify retained AOT.

Usage: --sdk .local/baseline/r354/katana-source-178448be.zip --output-dir <build-dir>
Root owns AOT call substitution/linking. Scope inverse emits the 221 XMTRX
sites; scope unit emits all 423 four-argument add/sub/mul sites in that one
SHA-bound unit. Neither sequence is a complete matrix algorithm.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import zipfile

FPU_MEMBER = 'src/runtime/fpu.cpp'
FPU_SHA = 'e0a2ec1ad05dcb884b69d7b30708a5b8184e09e6c2e77e55259f6ba6cc8126af'
UNIT = 'unit-v8C638FF0-8C639E9C-df982d963eeb3342.cpp'
AOT_SHA = '79ecc1ebbdf3e06c517d5edcc42850c08eb50c7dfe6fe4359c59b0e626472558'
CALL = re.compile(r'\s*katana::runtime::fpu_binary\(cpu, katana::runtime::FpuBinaryOperation::(Multiply|Subtract|Add), (\d+)u, (\d+)u\);')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def extract(source, first, after):
    if source.count(first) != 1 or source.count(after) != 1:
        raise ValueError('SDK extraction boundary changed')
    start = source.index(first)
    end = source.index(after, start)
    text = source[start:end].rstrip() + '\n'
    return text, {'first_line': source.count('\n', 0, start) + 1,
                  'last_line': source.count('\n', 0, start + len(text.rstrip())) + 1,
                  'sha256': sha(text.encode())}


PREFIX = '''// Generated from SHA-bound retained SDK fpu.cpp. Do not hand-edit.
#pragma once
#include "katana/runtime/fpu.hpp"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <optional>
#if defined(_MSC_VER)
#define SONIC_INVERSE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define SONIC_INVERSE_INLINE inline __attribute__((always_inline))
#else
#error This bounded experiment requires a forceinline-capable compiler
#endif
namespace sonic::inverse_arithmetic {
using katana::runtime::CpuState;
using katana::runtime::FpuBinaryOperation;
namespace detail {
using namespace katana::runtime;
'''

SUFFIX = '''
} // namespace detail

// Contract: this is a replacement for the FOUR-argument arithmetic helper,
// not a whole SH-4 instruction. Existing AOT FD/legality checks, epochs,
// attempted/retired accounting, safepoints and five-argument paths stay intact.
template<FpuBinaryOperation Operation, unsigned Source, unsigned Destination>
[[nodiscard]] SONIC_INVERSE_INLINE bool try_fast(CpuState& cpu) noexcept {
    using namespace katana::runtime;
    static_assert(Source < 16 && Destination < 16);
    static_assert(Operation == FpuBinaryOperation::Multiply ||
                  Operation == FpuBinaryOperation::Add ||
                  Operation == FpuBinaryOperation::Subtract);
    // Conservative O/U/I enables can trap even with zero Cause. Reject ALL
    // enables and PR before any mutation. DN needs no extra restriction:
    // the exact integer helpers reject subnormal inputs/results requiring it.
    // RM 2/3 intentionally use the retained helpers' RM!=1 nearest-even rule;
    // SZ/FR do not change this helper's single FR-array arithmetic contract.
    if ((cpu.fpscr & (fpscr_pr_mask | fpscr_exception_enable_mask)) != 0u)
        return false;
    detail::SingleBinaryResult result;
    const auto n = cpu.fr[Destination];
    const auto m = cpu.fr[Source];
    const auto rounding = static_cast<std::uint8_t>(cpu.fpscr & fpscr_rounding_mode_mask);
    bool accepted;
    if constexpr (Operation == FpuBinaryOperation::Multiply)
        accepted = detail::try_normal_single_product(n, m, rounding, result);
    else
        accepted = detail::try_normal_single_sum(n, m,
            Operation == FpuBinaryOperation::Subtract, rounding, result);
    if (!accepted) return false;
    // Source-bound helpers return only {0, Cause.I}, never unmaskable Cause.E.
    // Keep this runtime guard too; rejection must leave CPU and host FP intact.
    if ((result.causes & ~fpscr_cause_inexact_mask) != 0u) return false;
    // Exact nontrapping reduction of clear_fpu_causes (SDK 202..204),
    // finish_arithmetic (365..373) and signal_fpu_exception (213..214).
    // Sticky flags survive; all old causes clear; no exception callback runs.
    cpu.fpscr = (cpu.fpscr & ~fpscr_cause_mask) | result.causes |
                ((result.causes >> 10u) & fpscr_flag_mask);
    cpu.fr[Destination] = result.bits;
    return true;
}

template<FpuBinaryOperation Operation, unsigned Source, unsigned Destination>
SONIC_INVERSE_INLINE void binary(CpuState& cpu) noexcept {
    if (try_fast<Operation, Source, Destination>(cpu)) return;
    static_cast<void>(katana::runtime::fpu_binary(
        cpu, Operation, Source, Destination, std::nullopt));
}
} // namespace sonic::inverse_arithmetic
#undef SONIC_INVERSE_INLINE
'''


def generate(sdk, scope='inverse'):
    with zipfile.ZipFile(sdk) as archive:
        raw = archive.read(FPU_MEMBER)
    if sha(raw) != FPU_SHA:
        raise ValueError('Retained SDK fpu.cpp SHA mismatch')
    source = raw.decode('utf-8')
    # Both arithmetic bodies are extracted verbatim, including integer rounding.
    body, extraction = extract(source, 'struct SingleBinaryResult {',
        '[[nodiscard]] bool try_normal_single_quotient(')
    if body.count('[[nodiscard]] bool ') != 2:
        raise ValueError('Expected exactly product and sum helpers')
    emitted = body.replace('[[nodiscard]] bool ',
                           '[[nodiscard]] SONIC_INVERSE_INLINE bool ')
    if emitted.replace('[[nodiscard]] SONIC_INVERSE_INLINE bool ',
                       '[[nodiscard]] bool ') != body:
        raise ValueError('Arithmetic changed during inline decoration')
    aot_path = sdk.parent / 'product' / 'generated' / 'code' / UNIT
    aot = aot_path.read_bytes()
    if sha(aot) != AOT_SHA:
        raise ValueError('Retained AOT SHA mismatch')
    pc = 0
    calls = []
    for line_no, line in enumerate(aot.decode('utf-8').splitlines(), 1):
        marker = re.search(r'// katana-guest 0x([0-9A-F]+)u', line)
        if marker:
            pc = int(marker[1], 16)
        match = CALL.fullmatch(line)
        if match and (scope == 'unit' or 0x8C639066 <= pc < 0x8C6393DA):
            calls.append({'pc': f'0x{pc:08X}', 'operation': match[1],
                          'source': int(match[2]), 'destination': int(match[3]),
                          'source_line': line_no})
    counts = dict(Counter(call['operation'] for call in calls))
    expected = ({'Multiply': 274, 'Subtract': 82, 'Add': 67} if scope == 'unit'
                else {'Multiply': 141, 'Subtract': 47, 'Add': 33})
    expected_size = sum(expected.values())
    if counts != expected or len(calls) != expected_size:
        raise ValueError('Selected four-argument arithmetic family changed')
    if len({c['pc'] for c in calls}) != expected_size:
        raise ValueError('Repeated guest PCs in selected call list')
    # Test-only arithmetic skeleton: not the inverse (FMOV/FSCHG/etc. omitted).
    sequence = '\n// Constant operand sequence for the standalone test only.\n'
    sequence += '#define SONIC_INVERSE_ARITHMETIC_SEQUENCE(X) \\\n'
    sequence += ' \\\n'.join(
        f"    X({c['pc']}u, {c['operation']}, {c['source']}u, {c['destination']}u)"
        for c in calls) + '\n'
    header = (PREFIX + emitted + SUFFIX + sequence).encode('utf-8')
    report = {'schema': 'sarecomp-inverse-arithmetic-v1',
              'sdk_member': FPU_MEMBER, 'sdk_fpu_sha256': FPU_SHA,
              'extraction': extraction, 'arithmetic_modification': 'forceinline decoration only',
              'aot_unit': UNIT, 'aot_sha256': AOT_SHA,
              'scope': scope,
              'interval': ['0x8C638FF0', '0x8C639F32'] if scope == 'unit' else ['0x8C639066', '0x8C6393DA'],
              'four_argument_calls': len(calls), 'operations': counts,
              'calls': calls, 'header_sha256': sha(header),
              'fallback': 'original retained five-argument fpu_binary / std::nullopt',
              'status_contract': 'PR=0, all Enables=0; accepted causes subset of Inexact; no CPU mutation before rejection',
              'not_performed': ['AOT substitution', 'compilation', 'runtime equivalence', 'benchmark']}
    return header, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--scope', choices=('inverse', 'unit'), default='inverse')
    args = parser.parse_args()
    sdk = args.sdk.resolve(strict=True)
    output = args.output_dir.resolve()
    protected = sdk.parent
    if output == protected or protected in output.parents or output in protected.parents:
        raise ValueError('Output must be separate from retained baseline')
    header, report = generate(sdk, args.scope)
    output.mkdir(parents=True, exist_ok=True)
    for name, data in [('sonic_inverse_arithmetic.hpp', header),
                       ('provenance.json', (json.dumps(report, indent=2) + '\n').encode())]:
        target = output / name
        if target.is_symlink() or (target.exists() and target.stat().st_nlink > 1):
            raise ValueError('Refusing linked output file')
        if not target.exists() or target.read_bytes() != data:
            target.write_bytes(data)
    print(f"SONIC_INVERSE_ARITHMETIC_PREPARED scope={args.scope} calls={report['four_argument_calls']}; correctness/performance NOT tested")


if __name__ == '__main__':
    main()
