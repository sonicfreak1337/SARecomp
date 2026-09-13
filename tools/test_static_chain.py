"""Build the differential fixture from both actual generated index classes."""
import argparse
from pathlib import Path
import re
import struct
from prepare_static_chain import optimize

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
original = args.source.read_text()
candidate, excluded = optimize(original)


def index(text):
    start = text.index('class NativePortStaticChainIndex final {')
    return text[start:text.index('\n};', start) + 3]


start = candidate.index('bool sonic_static_chain_excluded_source(')
predicate = candidate[start:candidate.index('\n}\n', start) + 3]
entries = {}
for shard in sorted(args.source.parent.glob('native-port-dispatch-shard-*.cpp')):
    for address, static in re.findall(
            r'\{0x([0-9A-F]{8})u, &fn_[0-9A-F]+_runtime_entry, (?:true|false), (true|false)\}',
            shard.read_text()):
        key = int(address, 16)
        if key in entries:
            raise ValueError('Duplicate generated dispatch fixture entry')
        entries[key] = static == 'true'
if not entries:
    raise ValueError('No actual dispatch entries parsed')
args.output.parent.mkdir(parents=True, exist_ok=True)
data = args.output.with_suffix('.bin')
data.write_bytes(b''.join(struct.pack('<II', a, s) for a, s in entries.items()))
source = '''#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
struct NativePortDispatchEntry { std::uint32_t address; bool static_chainable; };
'''
source += 'namespace retained {\n' + index(original) + '\n}\n'
source += 'namespace candidate {\n' + predicate + index(candidate) + '\n}\n'
source += '''void require(bool v) {if(!v) throw std::runtime_error("static chain policy differs");}
int main(int argc,char** argv) {
    try {
        require(argc==2);std::ifstream file(argv[1],std::ios::binary);require(bool(file));
        std::vector<NativePortDispatchEntry> entries;
        std::uint32_t pair[2];while(file.read(reinterpret_cast<char*>(pair),sizeof(pair)))
            entries.push_back({pair[0],bool(pair[1])});
        retained::NativePortStaticChainIndex before(entries);
        candidate::NativePortStaticChainIndex after(entries);
        std::uint64_t checked=0u;
        const auto check=[&](std::uint32_t source) {
            require((!candidate::sonic_static_chain_excluded_source(source) && before.contains(source))==after.contains(source));
            ++checked;
        };
        for(const auto e:entries) for(const auto mask:{0u,1u,2u,0x20000000u,0x20000001u}) check(e.address^mask);
        std::uint32_t random=0x75AC491Du;
        for(unsigned i=0;i<100000u;++i) {random^=random<<13u;random^=random>>17u;random^=random<<5u;check(random);}
'''
source += '        const std::array blocked{' + ','.join('0x%08Xu' % x for x in excluded) + '};\n'
source += '''        for(const auto address:blocked) {check(address);check(address&~0x20000000u);}
        const auto failures=[](const std::vector<NativePortDispatchEntry>& input) {
            unsigned count=0u;
            try {retained::NativePortStaticChainIndex a(input);} catch(const std::runtime_error&) {++count;}
            try {candidate::NativePortStaticChainIndex a(input);} catch(const std::runtime_error&) {++count;}
            require(count==2u);
        };
        failures({{blocked[0],true},{blocked[0],true}});
        failures({{blocked[0]|1u,true}});
        retained::NativePortStaticChainIndex empty_before({});
        candidate::NativePortStaticChainIndex empty_after({});
        for(auto address:blocked) require(!empty_before.contains(address) && !empty_after.contains(address));
        std::cout<<"SONIC_STATIC_CHAIN_TEST_PASS entries="<<entries.size()<<" comparisons="<<checked
            <<" exclusions="<<blocked.size()<<" constructor_validation=retained\\n";
    } catch(const std::exception& e) {std::cerr<<"SONIC_STATIC_CHAIN_TEST_FAIL "<<e.what()<<'\\n';return 1;}
}
'''
args.output.write_text(source)
