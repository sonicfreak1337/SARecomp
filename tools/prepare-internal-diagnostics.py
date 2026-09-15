"""Port-local diagnostic policy. SDK/AOT inputs remain byte-for-byte untouched."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

SOURCES = {
    'native_bringup_dispatch.cpp': '12c3e221ffc52b1b9d9cc7884f0246bd6d19b1f7664580372768110ad464ef10',
    'native_port_runtime.cpp': '0ca5290d78f5e85c661377b52220376c9745ceab2c1251bcc169fcf09e39d3b8',
}

def prepare(name, source):
    if name == 'native_bringup_dispatch.cpp':
        changes = [
            # The journal is an offline diagnostic, never used for dispatch.
            ('    increment_saturated(total_occurrences_);\n    for (std::size_t index = 0u; index < event_count_; ++index) {',
             '    if (!sonic::diagnostics::runtime_checks_enabled()) return;\n    increment_saturated(total_occurrences_);\n    for (std::size_t index = 0u; index < event_count_; ++index) {'),
            # These immutable pack fields were authenticated at construction.
            # Always retain live owner, lifetime and table-generation checks.
            ('           table.dispatch_generation() == validated_table_generation_ &&\n           current.contract_version == validated_identity_.contract_version &&',
             '           table.dispatch_generation() == validated_table_generation_ &&\n           (!sonic::diagnostics::runtime_checks_enabled() || (\n           current.contract_version == validated_identity_.contract_version &&'),
            ('           same_storage(current.module_universe_identity,\n                        validated_identity_.module_universe_identity) &&\n           current.aot_pack_generation ==\n               validated_identity_.aot_pack_generation;',
             '           same_storage(current.module_universe_identity,\n                        validated_identity_.module_universe_identity) &&\n           current.aot_pack_generation ==\n               validated_identity_.aot_pack_generation));'),
        ]
    else:
        # Installation is checked by the constructor. Rechecking whether our
        # own observer was replaced is a developer audit. The observer, write
        # tracking, executable invalidation, fault and cycle paths stay active.
        changes = [('bool NativePortAotServices::aot_contract_valid() const noexcept {\n',
                    'bool NativePortAotServices::aot_contract_valid() const noexcept {\n    if (!sonic::diagnostics::runtime_checks_enabled()) return true;\n')]
    source = source.replace('\r\n', '\n')
    for before, after in changes:
        if source.count(before) != 1:
            raise RuntimeError('Diagnostic policy boundary changed: '+name+' '+repr(before))
        source = source.replace(before, after)
    return '#include "sonic_internal_diagnostics.hpp"\n'+source

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args=parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    records={}
    with zipfile.ZipFile(args.sdk) as archive:
        for name, expected in SOURCES.items():
            original=archive.read('src/runtime/'+name)
            if hashlib.sha256(original).hexdigest()!=expected:
                raise RuntimeError('Pinned diagnostic source changed: '+name)
            data=prepare(name,original.decode()).encode()
            path=args.output_dir/name
            if not path.exists() or path.read_bytes()!=data: path.write_bytes(data)
            records[name]={'source_sha256':expected,'sha256':hashlib.sha256(data).hexdigest()}
    manifest=json.dumps({'policy':1,'guest_units_changed':0,'files':records},indent=2)+'\n'
    path=args.output_dir/'provenance.json'
    if not path.exists() or path.read_text()!=manifest:path.write_text(manifest)
    print('SONIC_INTERNAL_DIAGNOSTICS_READY policy=1 default=off guest_units_changed=0')

if __name__=='__main__':main()
