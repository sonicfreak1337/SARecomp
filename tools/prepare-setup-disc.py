"""Use only the pinned read-only disc sources in the installer, no scheduler."""
from pathlib import Path
import sys
source, output = map(Path, sys.argv[1:])
text = source.read_text(encoding='utf-8')
marker = '\nGdRomDrive::GdRomDrive('
if text.count(marker) != 1:
    raise RuntimeError('Pinned read-only disc boundary changed')
value = text[:text.index(marker)] + '\n} // namespace katana::runtime\n'
output.parent.mkdir(parents=True, exist_ok=True)
if not output.exists() or output.read_text(encoding='utf-8') != value:
    output.write_text(value, encoding='utf-8')
