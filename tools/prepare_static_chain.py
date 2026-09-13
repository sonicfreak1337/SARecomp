"""Move immutable native-hook exclusions into the existing static chain bitmap.

The manifest-authenticated caller supplies the full dispatch source. Keep both
original constructor validation passes, then erase only the exact P1/P2
exclusions. No binder result, mutable generation or negative lookup is cached.
"""
import re


def optimize(source):
    head = '        auto source = normalized_source_address(address);\n'
    fast = '        if (static_chainable_source_address(source)) return true;\n'
    start = source.index('bool native_chainable_entry(')
    a = source.index(head, start) + len(head)
    b = source.index(fast, a)
    exclusions = source[a:b]
    atom = r'\(source \| 0x20000000u\) == 0x[0-9A-F]{8}u'
    condition = r'\s*if \(' + atom + r'(?:\s*\|\|\s*' + atom + r')*\) return false;'
    if re.sub(condition, '', exclusions).strip():
        raise ValueError('Unknown static chain exclusion policy')
    addresses = sorted(set(int(x, 16) for x in re.findall(r'== 0x([0-9A-F]{8})u', exclusions)))
    if not addresses or any((x & 0x20000001) != 0x20000000 for x in addresses):
        raise ValueError('Invalid static chain alias/exclusion policy')
    helper = ('bool sonic_static_chain_excluded_source(const std::uint32_t source) noexcept {\n' +
              exclusions.replace('return false;', 'return true;') +
              '    return false;\n}\n')
    cls = 'class NativePortStaticChainIndex final {'
    if source.count(cls) != 1:
        raise ValueError('Static chain index class changed')
    constructor_tail = '            word |= mask;\n        }\n    }\n'
    index_start = source.index(cls)
    tail = source.index(constructor_tail, index_start)
    clear = '''        // Validation above is unchanged, including excluded duplicate entries.
        // These bits can never authorize chaining. The cold miss path still
        // checks the exact exclusions before consulting native hooks/binders.
        const auto erase = [this](std::uint32_t address) {
            const auto page = address >> page_shift;
            if (page < first_page_ || std::uint64_t(page - first_page_) >= pages_.size()) return;
            const auto encoded = pages_[page - first_page_];
            if (!encoded) return;
            const auto halfword = (address & page_mask) >> 1u;
            bitmaps_[encoded - 1u][halfword >> word_shift] &=
                ~(std::uint64_t{1} << (halfword & word_mask));
        };
'''
    clear += '        for (const auto source : std::array<std::uint32_t, ' + str(len(addresses)) + '>{\n'
    clear += ''.join('            0x%08Xu,\n' % x for x in addresses)
    clear += '        }) { erase(source); erase(source & ~0x20000000u); }\n'
    tail_after = constructor_tail[:-6] + clear + '    }\n'
    # Explicit reversible substitutions; any unrelated generated text is kept.
    edits = [(exclusions + fast, fast + '        if (sonic_static_chain_excluded_source(source)) return false;\n'),
             (cls, helper + cls), (constructor_tail, tail_after)]
    result = source
    for old, new in edits:
        if result.count(old) != 1:
            raise ValueError('Ambiguous static chain substitution')
        result = result.replace(old, new)
    check = result
    for old, new in reversed(edits):
        if check.count(new) != 1:
            raise ValueError('Ambiguous reverse static chain substitution')
        check = check.replace(new, old)
    if check != source:
        raise ValueError('Static chain rewrite changed unrelated source')
    return result, addresses
