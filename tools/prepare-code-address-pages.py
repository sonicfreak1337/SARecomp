"""Prepare exact page-delta caches without changing the pinned SDK or guest pack."""
import argparse
import hashlib
import json
from pathlib import Path

RUNTIME_SHA = '2050b48b7cf06534a15e2de7d276c9aeba26b911f85d1fbc2e67815559fbe58d'
HEADER = '#include "sonic_code_address_pages.hpp"\n'

def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)

def once(source, before, after):
    if source.count(before) != 1:
        raise ValueError('Code-address boundary changed: ' + before[:100])
    return source.replace(before, after)

def runtime(source):
    data = source.read_bytes()
    if hashlib.sha256(data).hexdigest() != RUNTIME_SHA:
        raise ValueError('Pinned block ABI source changed')
    text = data.decode().replace('\r\n', '\n')
    text = HEADER + text
    text = once(text, 'namespace katana::runtime {', '''namespace sonic::code_address_pages {
// Identity is valid across the entire virtual address space before any scope.
constinit thread_local std::uint32_t forward[count]{};
constinit thread_local std::uint32_t reverse[count]{};
}

namespace katana::runtime {''')
    text = once(text, '#undef KATANA_BLOCK_ABI_NOINLINE', '''#undef KATANA_BLOCK_ABI_NOINLINE

template <bool Reverse>
void refresh_code_address_pages(const CodeAddressMapping& mapping) noexcept {
    using namespace sonic::code_address_pages;
    auto* pages = Reverse ? reverse : forward;
    const std::uint64_t start = Reverse ? mapping.runtime_start : mapping.source_start;
    const auto last = (start + mapping.extent - 1u) >> shift;
    for (auto page = start >> shift; page <= last; ++page) {
        const auto begin = static_cast<std::uint32_t>(page << shift);
        // The original resolver supplies an interval that respects every
        // newer overlap. Only a whole-page proof permits direct translation.
        (void)lookup_code_address_slow<Reverse>(begin);
        const auto& interval = Reverse ? unrelocated : relocated;
        const bool whole_page = interval.begin <= begin &&
            static_cast<std::uint64_t>(interval.begin) + interval.extent >=
                static_cast<std::uint64_t>(begin) + (1ull << shift);
        pages[page] = whole_page && interval.delta != fallback ? interval.delta : fallback;
    }
}

void refresh_code_address_pages(const CodeAddressMapping& mapping) noexcept {
    refresh_code_address_pages<false>(mapping);
    refresh_code_address_pages<true>(mapping);
}''')
    before = '''    code_address_mapping_active = true;
    invalidate_code_address_lookup_cache();'''
    text = once(text, before, before + '\n    refresh_code_address_pages(mapping);')
    text = once(text, '''    active_code_address_mappings.erase(std::next(found).base());''',
        '''    const auto retired_mapping = found->mapping;
    active_code_address_mappings.erase(std::next(found).base());''')
    before = '''    code_address_mapping_active = !active_code_address_mappings.empty();
    invalidate_code_address_lookup_cache();'''
    text = once(text, before, before + '\n    refresh_code_address_pages(retired_mapping);')
    return text.encode()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime', type=Path, required=True)
    p.add_argument('--destination', type=Path, required=True)
    p.add_argument('--units-file', type=Path)
    p.add_argument('--source-root', type=Path)
    a = p.parse_args()
    out = a.destination.resolve()
    result = runtime(a.runtime)
    write(out/'block_abi.cpp', result)
    report = {'runtime_sha256': RUNTIME_SHA, 'runtime_output_sha256': hashlib.sha256(result).hexdigest(), 'units': []}
    if a.units_file:
        if not a.source_root:
            raise ValueError('Retained source identity is required')
        root = a.source_root.resolve()
        lines = (root/'.katana-generated-artifacts').read_text().splitlines()
        generation = 'generation\tsha256:'+hashlib.sha256(('\n'.join(lines[2:])+'\n').encode()).hexdigest()
        if lines[:2] != ['katana-codegen-artifacts-v2', generation]:
            raise ValueError('Invalid retained source manifest')
        records = {row[0]:row for line in lines[2:] if len(row:=line.split('\t')) >= 3}
        report['generation'] = generation
        paths = [Path(line) for line in a.units_file.read_text().splitlines() if line]
        if len({p.name for p in paths}) != len(paths):
            raise ValueError('Ambiguous guest member names')
        for path in paths:
            if path.resolve().parent == out:
                raise ValueError('Prepared input/output overlap')
            source = path.read_bytes()
            original = (root/'code'/path.name).read_bytes()
            original_hash = hashlib.sha256(original).hexdigest()
            if records.get('code/'+path.name,[])[1:3] != [str(len(original)), 'sha256:'+original_hash]:
                raise ValueError('Retained guest identity changed: '+path.name)
            if path.resolve().parent == root/'code':
                if source != original:
                    raise ValueError('Original source changed')
            elif path.parent.name == 'region-writes':
                previous = json.loads((path.parent/'preparation.json').read_text())
                entry = [row for row in previous['units'] if row['unit']==path.name]
                if (previous['mode']!='region' or previous.get('guard_probe',False) or
                    previous['generation']!=generation or len(entry)!=1 or
                    entry[0]['source_sha256']!=original_hash or
                    entry[0]['output_sha256']!=hashlib.sha256(source).hexdigest()):
                    raise ValueError('RAM region provenance differs: '+path.name)
            else:
                raise ValueError('Unreviewed input preparation: '+str(path))
            text = source.decode()
            old = ('katana::runtime::relocate_code_address_inline', 'katana::runtime::unrelocate_code_address_inline')
            new = ('sonic::code_address_pages::relocate', 'sonic::code_address_pages::unrelocate')
            counts = [text.count(token) for token in old]
            if not all(counts):
                raise ValueError('Missing original lookup sites: ' + str(path))
            changed = HEADER + text.replace(old[0], new[0]).replace(old[1], new[1])
            if changed[len(HEADER):].replace(new[0], old[0]).replace(new[1], old[1]) != text:
                raise ValueError('Unrelated guest change')
            result = changed.encode()
            write(out/path.name, result)
            report['units'].append({'unit':path.name,'original_sha256':original_hash,'input_sha256':hashlib.sha256(source).hexdigest(),
                                    'output_sha256':hashlib.sha256(result).hexdigest(),'sites':counts})
    write(out/'preparation.json', (json.dumps(report,indent=2)+'\n').encode())
    print('SONIC_CODE_ADDRESS_PAGES_READY units='+str(len(report['units'])))

if __name__ == '__main__':
    main()
