"""Require a single FPU runtime/epoch owner in the actual native link map."""
from pathlib import Path
import re
import sys

entries={}
owner='sonic_fpu_runtime:fpu.cpp.obj'
with Path(sys.argv[1]).open() as stream:
    for line in stream:
        lower=line.lower()
        if ('katana_aot_runtime:fpu.cpp.obj' in lower or
                '.lto.katana_aot_runtime.fpu.cpp.obj' in lower):
            raise SystemExit('Original FPU runtime member still linked')
        match=re.match(r'^\s*[0-9a-fA-F]+:[0-9a-fA-F]+\s+(\S+)',line)
        if not match:
            continue
        symbol=match[1]
        if (symbol.startswith('?fpu_binary@runtime@katana@@') or
                symbol.startswith('??0HostFpuExecutionEpoch@runtime@katana@@') or
                symbol.startswith('??1HostFpuExecutionEpoch@runtime@katana@@')):
            entries.setdefault(symbol,[]).append(line.split()[-1].lower())
if len(entries)!=4 or any(owners!=[owner] for owners in entries.values()):
    raise SystemExit('FPU binary/epoch symbols do not have the sole intended owner: '+repr(entries))
print('SONIC_FPU_RUNTIME_LINK_PASS binary_overloads=2 epoch_symbols=2 original_member=0')
