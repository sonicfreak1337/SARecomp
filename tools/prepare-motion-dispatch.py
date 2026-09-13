"""Retain native hooks and memoize exact immutable source-table lookups."""
import hashlib
from pathlib import Path
import sys
import json
from prepare_static_chain import optimize as optimize_static_chain

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
supplement=Path(sys.argv[2]).parent.parent/'minicart/minicart-identities.json'
for before,after in json.loads(supplement.read_text()).items():
    if source.count(before)<1:raise RuntimeError('MINICART pack identity boundary changed')
    source=source.replace(before,after)
# Add the reviewed results closure after the original table validations. The
# same byte identities still gate module binding and every dynamic admission.
for before, after in (
    ('throw std::runtime_error("native-dispatch-table");\n        return result;',
     'throw std::runtime_error("native-dispatch-table");\n        append_sonic_minicart_entries(result);\n        return result;'),
    ('"native-loaded-aot-module-table");\n        return result;',
     '"native-loaded-aot-module-table");\n        extend_sonic_minicart_identities(result);\n        return result;')):
    if source.count(before)!=1:raise RuntimeError('MINICART dispatch integration boundary changed')
    source=source.replace(before,after)
source=source.replace('#include "../include/native-port-dispatch-internal.hpp"',
    '#include "../include/native-port-dispatch-internal.hpp"\n#include "minicart-bindings.hpp"')
if "--filtered-static-chain" in sys.argv[3:]:
    source, _ = optimize_static_chain(source)
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
# This memo stores only positive pointers into the immutable generated table.
# Apply it to every exact lookup, including the repeated post-admission identity
# check. It stores no binder, runtime address, epoch, admission or executable
# owner; all original preflight, owner and generation checks still execute.
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
source = source.replace(lookup, lookup.replace('find_exact_entry(', 'find_uncached_entry(')+'''
thread_local sonic::dispatch::ImmutableSourceMemo<NativePortDispatchEntry> sonic_source_memo;
thread_local bool sonic_source_memo_enabled = true;
thread_local bool sonic_source_memo_diagnostics = false;
const NativePortDispatchEntry* find_exact_entry(std::uint32_t source) {
    if (!sonic_source_memo_enabled) return find_uncached_entry(source);
    return sonic_source_memo_diagnostics
        ? sonic_source_memo.find<true>(source, find_uncached_entry)
        : sonic_source_memo.find<false>(source, find_uncached_entry);
}
''')
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
# Keep the inner cause and register frontier before outer host services restore
# their interrupted CPU state. Failure codes/dispatch semantics remain unchanged.
callback_start = source.index('katana_native_invoke_callback(\n    katana::runtime::NativePortContext& context,')
callback_end = source.index('\n}\n', callback_start) + 3
callback = source[callback_start:callback_end]
callback = callback.replace('    try {', '''
    const auto report_failure = [&](const char* message, unsigned code) noexcept {
        const auto* cpu = context.cpu;
        std::fprintf(stderr, "SONIC_NATIVE_CALLBACK_FAILURE entry=%08x pc=%08x pr=%08x r4=%08x instruction=%08x error=%u cause=%.768s\\n",
            guest_address, cpu ? cpu->pc : 0u, cpu ? cpu->pr : 0u,
            cpu ? cpu->r[4] : 0u, cpu ? cpu->active_instruction_pc : 0u, code, message);
        std::fflush(stderr);
        try {
            if (context.crash_capsule && cpu) {
                katana::runtime::CrashCapsuleProviderTranscript record;
                record.sequence=context.frame_index;
                record.provider=0x53414342u;
                record.source=guest_address;
                record.target=cpu->pc;
                record.callsite=cpu->pr;
                record.address=cpu->r[4];
                record.result=cpu->active_instruction_pc;
                record.value=code;
                record.provider_identity.assign("sonic-native-callback-failure");
                record.target_identity.assign(std::string_view(message).substr(0,768));
                context.crash_capsule->note_v5_provider_transcript(record);
            }
        } catch (...) {}
    };
    try {''', 1)
needle = '''        return katana_port_generated::bridge_failure(
            context, katana::runtime::NativePortStopReason::HookAbort, 6u);'''
if callback.count(needle) != 2:
    raise RuntimeError("Unexpected callback failure boundary")
callback = callback.replace(needle, '        report_failure("nested-hook-abort", abort.error_code);\n'+needle, 1)
callback = callback.replace('    } catch (...) {\n'+needle, '''    } catch (const std::exception& error) {
        report_failure(error.what(), 6u);
'''+needle+'''
    } catch (...) {
        report_failure("unknown-native-exception", 6u);
'''+needle, 1)
source = source[:callback_start]+callback+source[callback_end:]
source = '#include "katana/runtime/crash_capsule.hpp"\n#include <exception>\n'+source

destination.parent.mkdir(parents=True, exist_ok=True)
encoded = source.encode()
if not destination.exists() or destination.read_bytes() != encoded:
    destination.write_bytes(encoded)
print("SONIC_PORT_DISPATCH_READY source_generation_verified=1 retained_aot_unchanged=1 interpolation_instrumentation=0 exact_source_memo=64")
