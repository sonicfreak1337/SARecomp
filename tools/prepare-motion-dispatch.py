"""Retain native hooks and memoize immutable source lookup after admission."""
import hashlib
from pathlib import Path
import sys

source_path, destination = map(Path, sys.argv[1:3])
manifest = source_path.parent.parent / ".katana-generated-artifacts"
lines = manifest.read_text().splitlines()
if lines[0] != "katana-codegen-artifacts-v2":
    raise RuntimeError("Unrecognized dispatch manifest")
if lines[1] != "generation\tsha256:" + hashlib.sha256(("\n".join(lines[2:])+"\n").encode()).hexdigest():
    raise RuntimeError("Invalid dispatch manifest generation")
relative = source_path.relative_to(manifest.parent).as_posix()
records = [line.split("\t") for line in lines[2:] if line.split("\t")[0] == relative]
data = source_path.read_bytes()
if len(records) != 1 or int(records[0][1]) != len(data) or records[0][2] != "sha256:"+hashlib.sha256(data).hexdigest():
    raise RuntimeError("Dispatch source differs from its SDK-authored generation")
source = data.decode()
for header in ("katana_port.hpp", "native-port-dispatch-internal.hpp"):
    original = '#include "../include/'+header+'"'
    if source.count(original) != 1:
        raise RuntimeError("Unexpected dispatch include boundary")
    source = source.replace(original, '#include "'+header+'"')
# Interpolation withdrawn: the original prototype injection is retained below
# for reference only. The generated hook bridge remains necessary for Options.
# source = '#include "sonic_motion_owner.hpp"\n#include "sonic_presentation.hpp"\n'+source
anchor = """    require_active_context(cpu);
    const auto result = dispatch_native(
        *active_native_context, target, cpu.pr, std::nullopt, false,
        initial_source);"""
if source.count(anchor) != 1:
    raise RuntimeError("Unexpected native call-dispatch boundary")
# source = source.replace(anchor, """    require_active_context(cpu);
#     const sonic::motion::CallScope motion_owner(
#         cpu,target,sonic::motion::CallScope::observes(cpu.pr)&&sonic::presentation::settings().interpolation);
#     const auto result = dispatch_native(
#         *active_native_context, target, cpu.pr, std::nullopt, false,
#         initial_source);""")
# This memo stores no binder, runtime address, epoch, admission or executable
# owner. Preserve the complete preflight and subsequent original owner checks.
lookup = """const NativePortDispatchEntry* find_exact_entry(
    const std::uint32_t address) {
    const auto& entries = dispatch_entries();
    return dispatch_index().find(entries, address);
}"""
admitted = """        selected_entry = runtime_dispatch_detail::
            find_exact_entry(admission.dispatch_source);"""
reset = """void reset_native_dispatch_cache() noexcept {
    native_dispatch_cache.fill({});
}"""
for boundary in (lookup, admitted, reset):
    if source.count(boundary) != 1:
        raise RuntimeError("Unexpected immutable-dispatch memo boundary")
source = '#include "sonic_dispatch_memo.hpp"\n#include <cstdio>\n#include <cstdlib>\n'+source
source = source.replace(lookup, lookup+'''
thread_local sonic::dispatch::ImmutableSourceMemo<NativePortDispatchEntry> sonic_source_memo;
thread_local bool sonic_source_memo_enabled = true;
thread_local bool sonic_source_memo_diagnostics = false;
const NativePortDispatchEntry* find_admitted_source_entry(std::uint32_t source) {
    if (!sonic_source_memo_enabled) return find_exact_entry(source);
    return sonic_source_memo_diagnostics
        ? sonic_source_memo.find<true>(source, find_exact_entry)
        : sonic_source_memo.find<false>(source, find_exact_entry);
}
''')
source = source.replace(admitted, admitted.replace('find_exact_entry(', 'find_admitted_source_entry('))
source = source.replace(reset, '''void reset_native_dispatch_cache() noexcept {
    native_dispatch_cache.fill({});
    const auto& stats=sonic_source_memo.statistics;
    if (sonic_source_memo_diagnostics && (stats.hits || stats.misses))
        std::fprintf(stderr, "SONIC_DISPATCH_MEMO capacity=64 hits=%llu misses=%llu conflicts=%llu missing=%llu\\n",
            static_cast<unsigned long long>(stats.hits), static_cast<unsigned long long>(stats.misses),
            static_cast<unsigned long long>(stats.conflicts), static_cast<unsigned long long>(stats.missing));
    sonic_source_memo.reset();
    const auto is_one=[](const char* name) {
        const auto* value=std::getenv(name);
        return value && value[0]=='1' && value[1]=='\\0';
    };
    sonic_source_memo_enabled=!is_one("SARECOMP_DISPATCH_MEMO_DISABLE");
    sonic_source_memo_diagnostics=is_one("SARECOMP_DISPATCH_MEMO_STATS");
}''')
destination.parent.mkdir(parents=True, exist_ok=True)
encoded = source.encode()
if not destination.exists() or destination.read_bytes() != encoded:
    destination.write_bytes(encoded)
print("SONIC_PORT_DISPATCH_READY source_generation_verified=1 retained_aot_unchanged=1 interpolation_instrumentation=0 admitted_source_memo=64")
