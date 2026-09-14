"""Port-local CRLF adaptation of the pinned GDI reader; no SDK mutation."""
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
text = source.read_text(encoding='utf-8')
needle = '    std::vector<std::string> result;\n    std::size_t position = 0u;'
if text.count(needle) != 1:
    raise RuntimeError('Pinned GDI tokenizer no longer matches the reviewed adaptation')
signature = 'const std::string_view line) {'
if text.count(signature) != 1:
    raise RuntimeError('Pinned GDI tokenizer signature changed')
text = text.replace(signature, 'std::string_view line) {')
replacement = "    // Windows text streams strip CR; POSIX streams retain it. The original\n"
replacement += "    // descriptor bytes and their SHA-256 identity are never changed.\n"
replacement += "    if (!line.empty() && line.back() == '\\r') line.remove_suffix(1);\n" + needle
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(text.replace(needle, replacement), encoding='utf-8')
