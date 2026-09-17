"""Wrap the authenticated effective dispatcher with executable-epoch transfer plans."""
import argparse
import hashlib
import json
from pathlib import Path

# This is the already adapted, manifest-authenticated motion/minicart dispatcher.
# A change to that producer must be reviewed before updating this identity.
EFFECTIVE_SHA = 'cb263e6c03f13e8ae31a09314787c8ffb2bff2ef1646b079f5e4da936da19cc4'
SELECTION = '''thread_local NativeBringupCoverageDispatchSelection
    native_bringup_coverage_dispatch_selection;'''
STATE = '''
constinit thread_local sonic::dispatch::PreparedTransfers<NativeBringupCoverageDispatchSelection>
    sonic_prepared_transfers;
constinit thread_local unsigned sonic_transfer_mode = 0;
constinit thread_local std::uint64_t sonic_transfer_hits = 0, sonic_transfer_misses = 0;
unsigned sonic_transfer_configuration() noexcept {
    const auto* enabled = std::getenv("SARECOMP_PREPARED_TRANSFERS");
    const auto* verify = std::getenv("SARECOMP_PREPARED_TRANSFERS_VERIFY");
    if (enabled && std::string_view(enabled) == "0") return 0;
    return verify && std::string_view(verify) == "1" ? 2 : 1;
}
'''
WRAPPER = '''
sonic::dispatch::TransferEpoch sonic_transfer_epoch() noexcept {
    const auto& table = *active_native_bringup_table;
    const auto* images = active_native_context->runtime_images;
    const auto* binder = runtime_dispatch_detail::active_loaded_aot_binder;
    const auto image_stamp = images->dispatch_stamp();
    const auto binder_stamp = binder->dispatch_stamp();
    return {{&table, active_native_bringup_context, active_native_bringup_coverage_context,
             active_native_context, images, binder, runtime_dispatch_detail::active_services,
             binder_stamp.module_universe_identity.data(), binder_stamp.aot_pack_identity.data()},
            {table.dispatch_lifetime(), table.dispatch_generation(),
             active_native_bringup_context->runtime_generation,
             active_native_bringup_coverage_context->runtime_generation,
             image_stamp.lifecycle_generation, image_stamp.immutable_generation,
             binder_stamp.lifecycle_generation, binder_stamp.immutable_generation,
             binder_stamp.module_universe_identity.size(), binder_stamp.aot_pack_identity.size()},
            table.static_aot_dispatch_ready()};
}
void preflight_native_bringup_indirect_dispatch_impl(
        const std::uint32_t source_block, const std::uint32_t callsite,
        const std::uint32_t target, const std::uint32_t continuation, const bool call) {
    if (!native_bringup_dispatch_pending) return;
    // Diagnostics preserve their full observations and the original resolver.
    // A pending selection or incomplete context also retains original errors.
    const bool reusable = sonic_transfer_mode != 0 &&
        !sonic::diagnostics::runtime_checks_enabled() &&
        !native_bringup_coverage_dispatch_selection.valid &&
        active_native_bringup_table && active_native_bringup_context &&
        active_native_bringup_coverage_context && active_native_context &&
        active_native_context->runtime_images &&
        runtime_dispatch_detail::active_loaded_aot_binder &&
        !active_native_bringup_coverage_context->observations.recording_enabled();
    const sonic::dispatch::TransferKey key{source_block, callsite, target, continuation, call};
    if (reusable) {
        if (const auto* cached = sonic_prepared_transfers.find(sonic_transfer_epoch(), key)) {
            const auto expected = *cached;
            if (sonic_transfer_mode == 2) {
                ++sonic_transfer_hits;
                preflight_native_bringup_indirect_dispatch_original(
                    source_block, callsite, target, continuation, call);
                const auto& actual = native_bringup_coverage_dispatch_selection;
                if (expected.valid != actual.valid || (expected.valid &&
                    (expected.requested_target != actual.requested_target ||
                     expected.dispatch_source != actual.dispatch_source ||
                     expected.owner_kind != actual.owner_kind || expected.entry != actual.entry)))
                    throw std::runtime_error("sonic-prepared-transfer-verification");
            } else {
                native_bringup_coverage_dispatch_selection = expected;
            }
            return;
        }
        if (sonic_transfer_mode == 2) ++sonic_transfer_misses;
    }
    preflight_native_bringup_indirect_dispatch_original(
        source_block, callsite, target, continuation, call);
    // Resolving an owner can bind a new module. Capture the resulting epoch,
    // never a stamp from before the original resolver completed successfully.
    if (reusable)
        sonic_prepared_transfers.remember(sonic_transfer_epoch(), key,
                                         native_bringup_coverage_dispatch_selection);
}
'''

def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)

def transform(data):
    if hashlib.sha256(data).hexdigest() != EFFECTIVE_SHA:
        raise ValueError('Effective dispatcher identity changed; review adaptation first')
    source = data.decode()
    if source.count(SELECTION) != 1:
        raise ValueError('Unknown pending selection')
    source = '#include "sonic_prepared_transfers.hpp"\n' + source.replace(SELECTION, SELECTION + STATE)
    definition = 'void preflight_native_bringup_indirect_dispatch_impl(\n'
    if source.count(definition) != 1:
        raise ValueError('Unknown indirect preflight definition')
    source = source.replace(definition, 'void preflight_native_bringup_indirect_dispatch_original(\n')
    boundary = '\nvoid dispatch_call(katana::runtime::CpuState& cpu,\n'
    position = source.index(boundary, source.index('void preflight_native_bringup_indirect_dispatch_original('))
    source = source[:position] + '\n' + WRAPPER + source[position:]
    start = source.index('class NativeDispatchScope final {')
    end = source.index('\n};', start) + 3
    scope = source[start:end]
    reset = '        runtime_dispatch_detail::reset_native_dispatch_cache();'
    if scope.count(reset) != 2:
        raise ValueError('Unknown dispatch scope lifetime')
    scope = scope.replace(reset, reset + '\n        sonic_prepared_transfers.reset();')
    scope = scope.replace('        active_native_context = &context;', '''        sonic_transfer_mode = sonic_transfer_configuration();
        sonic_transfer_hits = sonic_transfer_misses = 0;
        active_native_context = &context;''')
    scope = scope.replace('    ~NativeDispatchScope() noexcept {', '''    ~NativeDispatchScope() noexcept {
        if (sonic_transfer_mode == 2)
            std::fprintf(stderr, "SONIC_PREPARED_TRANSFERS verified=%llu misses=%llu\\n",
                static_cast<unsigned long long>(sonic_transfer_hits),
                static_cast<unsigned long long>(sonic_transfer_misses));''')
    source = source[:start] + scope + source[end:]
    return source.encode()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--destination', type=Path, required=True)
    a = p.parse_args()
    source, dest = a.source.resolve(), a.destination.resolve()
    if source == dest or dest.name != 'native-port-dispatch.cpp':
        raise ValueError('Use a separate generated dispatcher output')
    output = transform(source.read_bytes())
    write(dest, output)
    report = {'schema':'sarecomp-prepared-transfers-v1', 'source_sha256':EFFECTIVE_SHA,
              'output_sha256':hashlib.sha256(output).hexdigest(), 'scope_resets':2,
              'capacity':4096, 'aot_units_changed':0}
    write(dest.parent/'preparation.json', (json.dumps(report, indent=2)+'\n').encode())
    print('SONIC_PREPARED_TRANSFERS_READY all_indirect_transfers=1 aot_units_changed=0')

if __name__ == '__main__':
    main()
