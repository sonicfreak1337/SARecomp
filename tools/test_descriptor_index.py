"""Audit descriptor-index invalidation; optionally run the separately built C++ test.

The mutation inventory is a review tripwire, not a C++ alias-analysis proof.
New descriptor writers must be reviewed and registered here with invalidation.
No compiler or game is launched by this script.
"""
import argparse
from collections import Counter
import hashlib
from pathlib import Path
import re
import subprocess


def audit(source):
    expected = Counter({
        ('sonic_native_title_state.texlist_catalog_bindings', 'clear'): 1,
        ('sonic_native_title_state.texlist_catalog_bindings', 'push_back'): 3,
        ('sonic_native_title_state.texlist_catalog_bindings', 'pop_back'): 1,
        ('sonic_native_title_state.texlist_catalog_bindings', 'erase'): 3,
        ('sonic_native_title_state.texlist_catalog_bindings', 'swap'): 1,
        ('sonic_native_title_state.pvm_allocations', 'swap'): 1,
        ('sonic_native_title_state.pvm_allocations', 'push_back'): 1,
        ('existing->guest_descriptors', 'swap'): 1,
    })
    staging = Counter({
        ('binding.guest_descriptors', '='): 1,  # Local copy, committed by outer swap.
        ('target.guest_descriptors', 'push_back'): 1,  # Local restore object.
        ('result.pvm_allocations', '='): 1,  # Snapshot export, not live state.
        ('state.pvm_allocations', 'push_back'): 1,  # Snapshot decode, not live state.
    })
    actual = Counter()
    # Reserve needs no invalidation: candidate positions contain no host pointers.
    reads = {'begin', 'end', 'size', 'empty', 'data', 'reserve', 'front'}
    pattern = r'(\w+(?:\.|->)(?:texlist_catalog_bindings|pvm_allocations|guest_descriptors))\s*(\.\s*(\w+)\s*\(|=(?!=))'
    for match in re.finditer(pattern, source):
        method = match[3] or '='
        if method in reads:
            continue
        key = (match[1], method)
        actual[key] += 1
        if key in expected:
            preceding = source[:match.start()].rstrip().splitlines()[-1]
            assert 'descriptor_candidate_index.invalidate();' in preceding, ('missing invalidation', key)
    assert actual == expected + staging, ('unreviewed candidate mutation', actual - expected - staging)
    erasures = list(re.finditer(r'std::erase_if\(state\.(texlist_catalog_bindings|pvm_allocations),', source))
    assert len(erasures) == 2
    for match in erasures:
        preceding = source[:match.start()].rstrip().splitlines()[-1]
        assert 'descriptor_candidate_index.invalidate();' in preceding
    assert 'if (!released_pins.empty()) state.descriptor_candidate_index.invalidate();' in source
    assert 'if (displaced) ++released_pins[allocation.catalog_index];' in source

    # Existing descriptor-address writes are all local preparation/serialization;
    # the sole live inner-list replacement is the invalidated swap above.
    writes = Counter(re.sub(r'\s+', '', m[0]) for m in re.finditer(
        r'(?:\.|->)descriptor_address\s*=(?!=)[^;]*;', source))
    assert writes == Counter({
        '.descriptor_address=recyclable.descriptor_address;': 1,
        '.descriptor_address=candidate;': 1,
        '.descriptor_address=descriptor.descriptor_address;': 1,
        '.descriptor_address=surface.descriptor_address;': 1,
        '.descriptor_address=reader.u32();': 3,
        '.descriptor_address=item.descriptor_address;': 1,
    }), 'descriptor key writer inventory changed'
    assert 'SonicNativeTexlistCatalogBinding binding = has_previous' in source
    assert source.count('sonic_native_title_state = {};') == 2
    assert source.count('sonic::texture::DescriptorCandidateIndex descriptor_candidate_index;') == 1

    # Preserve the existing validating accept lambda byte-for-byte modulo space.
    function = source[source.index('find_native_pvm_descriptor('):]
    accept = function[function.index('    const auto accept = '):function.index('    };') + 6]
    digest = hashlib.sha256(re.sub(r'\s+', '', accept).encode()).hexdigest()
    assert digest == 'd9e6b4e612a7720489c71669a85957ca89bdeb6a2d89d14c485d416b9be96816', 'live acceptance contract changed'
    print('DESCRIPTOR_INDEX_SOURCE_AUDIT_OK live_mutations=14 key_writes=8 state_resets=2 retained_accept')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test-exe', type=Path, help='SDK-independent test_descriptor_index.cpp executable, already built by the coordinator')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    audit((root / 'src/native_title_adapter.cpp').read_text(encoding='utf-8'))
    if args.test_exe:
        subprocess.run([str(args.test_exe.resolve(strict=True))], check=True)


if __name__ == '__main__':
    main()
