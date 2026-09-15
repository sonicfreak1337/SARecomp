"""Authoring-only lossless component measurement; does not produce an installer."""
import argparse
import hashlib
import json
import lzma
from pathlib import Path
import time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('source', type=Path)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--length', type=int, default=0, help='Optional prefix for an explicitly partial measurement')
p.add_argument('--dictionary-mib', nargs='+', type=int, default=[4, 16, 32])
a = p.parse_args()
source = a.source.resolve(strict=True)
output = a.output.resolve()
root = Path(__file__).resolve().parents[1]
if not output.is_relative_to(root/'.local') or output == root/'.local':
    p.error('Use a fresh diagnostic directory below this project/.local')
if output.exists() or a.length < 0 or a.length > source.stat().st_size:
    p.error('Invalid source length or output already exists')
if len(set(a.dictionary_mib)) != len(a.dictionary_mib) or any(n not in (4, 8, 16, 32, 64) for n in a.dictionary_mib):
    p.error('Choose distinct 4, 8, 16, 32 or 64 MiB dictionaries')
output.mkdir(parents=True)
length = a.length or source.stat().st_size
identity = (source.stat().st_size, source.stat().st_mtime_ns)
report = {'source':str(source), 'source_bytes':identity[0], 'measured_bytes':length,
          'complete_file':length==identity[0], 'preset':4, 'filter':'x86+lzma2', 'variants':[]}
reference = None
for dictionary in a.dictionary_mib:
    target = output/f'dictionary-{dictionary}MiB.xz'
    compressor = lzma.LZMACompressor(format=lzma.FORMAT_XZ, filters=[
        {'id':lzma.FILTER_X86}, {'id':lzma.FILTER_LZMA2, 'preset':4, 'dict_size':dictionary*1024*1024}])
    digest = hashlib.sha256()
    consumed = 0
    started = time.monotonic()
    cpu_start = time.process_time()
    with source.open('rb') as inp, target.open('xb') as out:
        while consumed < length:
            block = inp.read(min(1024*1024, length-consumed))
            if not block: raise RuntimeError('Source shortened during compression')
            digest.update(block)
            out.write(compressor.compress(block))
            consumed += len(block)
            if consumed % (64*1024*1024) == 0:
                print(f'SONIC_COMPRESSION_PROGRESS dictionary_mib={dictionary} input_bytes={consumed}', flush=True)
        out.write(compressor.flush())
    compress_seconds = time.monotonic()-started
    compress_cpu_seconds = time.process_time()-cpu_start
    del compressor
    compressed_bytes = target.stat().st_size
    actual = hashlib.sha256()
    restored = 0
    started = time.monotonic()
    cpu_start = time.process_time()
    with lzma.open(target,'rb') as inp:
        while block := inp.read(1024*1024):
            actual.update(block)
            restored += len(block)
    elapsed = time.monotonic()-started
    cpu = time.process_time()-cpu_start
    if restored != length or actual.digest() != digest.digest():
        raise RuntimeError('Lossless roundtrip failed')
    if reference is not None and digest.hexdigest() != reference:
        raise RuntimeError('Measured source changed between variants')
    reference = digest.hexdigest()
    if (source.stat().st_size, source.stat().st_mtime_ns) != identity:
        raise RuntimeError('Source changed during measurement')
    result = {'dictionary_mib':dictionary, 'compressed_bytes':compressed_bytes,
              'compress_seconds':compress_seconds, 'compress_cpu_seconds':compress_cpu_seconds,
              'decompress_seconds':elapsed, 'decompress_cpu_seconds':cpu, 'roundtrip_exact':True}
    report['source_span_sha256'] = reference
    report['variants'].append(result)
    (output/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print('SONIC_COMPRESSION_RESULT '+json.dumps(result),flush=True)
