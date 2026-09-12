"""Compare authenticated localized MLT programs; emit metadata, never audio."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

def extent(data, offset, size):
    if offset < 0 or size < 0 or offset + size > len(data):
        raise ValueError("Audio metadata extent outside authenticated file")
    return data[offset:offset + size]

def integer(data, offset, size=4):
    return int.from_bytes(extent(data, offset, size), "little")

def programs(data):
    if extent(data, 0, 4) != b"SMLT":
        raise ValueError("Expected SMLT")
    result = {}
    units = integer(data, 8)
    if units > 4096:
        raise ValueError("MLT unit count")
    extent(data, 32, units * 32)
    for unit in range(units):
        row = 32 + unit * 32
        kind = extent(data, row, 4)
        start, size = integer(data, row + 16), integer(data, row + 20)
        if kind not in (b"SMPB", b"SMDB") or start == 0xffffffff:
            continue
        bank = data[row + 4]
        payload = extent(data, start, size)
        if payload[:4] != kind:
            raise ValueError("Program bank kind")
        count, program_table = integer(payload, 20), integer(payload, 16)
        if count > 128:
            raise ValueError("Program count")
        extent(payload, program_table, count * 4)
        for program in range(count):
            pointer = integer(payload, program_table + program * 4)
            if not pointer:
                continue
            for layer in range(4):
                address = integer(payload, pointer + layer * 4)
                if not address:
                    continue
                splits, split_table = integer(payload, address), integer(payload, address + 4)
                if splits > 128:
                    raise ValueError("Split count")
                for index in range(splits):
                    split = extent(payload, split_table + index * 48, 48)
                    tone = integer(split, 2, 2) + ((split[0] & 127) << 16)
                    samples = integer(split, 6, 2)
                    encoding = "adpcm" if split[1] & 1 else "pcm8" if split[0] & 128 else "pcm16"
                    size = (samples + 1) // 2 if encoding == "adpcm" else samples * (1 if encoding == "pcm8" else 2)
                    key = f"{kind.decode()}:{bank}:{program}:{layer}:{index}"
                    result[key] = {"sample_sha256": hashlib.sha256(extent(payload, tone, size)).hexdigest(),
                                   "samples": samples, "format": encoding,
                                   "note_range": list(split[36:38]), "base_note": split[38],
                                   "velocity_range": list(split[43:45])}
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("content", type=Path)
    parser.add_argument("catalog", type=Path)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    entries = re.findall(r'\{"(sa-pal-v1003-mlt-\d+)", "([^"]+)", "sha256:([a-f0-9]+)", (\d+)ull\}', args.catalog.read_text())
    if len(entries) != 122:
        raise ValueError("Unexpected authenticated title catalog")
    files, paths = {}, {}
    for identity, relative, digest, size in entries:
        path = args.content / relative
        data = path.read_bytes()
        if len(data) != int(size) or hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(f"Catalog identity mismatch: {relative}")
        files[identity] = {"path": relative, "sha256": digest, "programs": programs(data)}
        paths[relative] = identity
    pairs = []
    for identity, item in files.items():
        path = item["path"]
        if not path.endswith("_E.MLT") or path[:-6] + ".MLT" not in paths:
            continue
        original = paths[path[:-6] + ".MLT"]
        left, right = files[original]["programs"], item["programs"]
        counts, changed = Counter(), []
        for key in sorted(left.keys() | right.keys()):
            a, b = left.get(key), right.get(key)
            status = "shared" if a == b else "different" if a and b else "missing"
            counts[status] += 1
            if status != "shared":
                changed.append({"program_split": key, "jp": a, "en": b})
        pairs.append({"jp": original, "en": identity, "counts": dict(counts), "changed": changed})
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps({"files": files, "localized_pairs": pairs}, indent=2) + "\n")
    print(json.dumps({"authenticated_files": len(files), "localized_pairs": len(pairs),
                      "pairs": [{"jp": p["jp"], "en": p["en"], **p["counts"],
                                 "changed_programs": (sorted({r["program_split"].rsplit(":", 2)[0] for r in p["changed"]})
                                    if p["jp"].endswith("-107") else len({r["program_split"].rsplit(":", 2)[0] for r in p["changed"]}))}
                                for p in pairs]}, separators=(",", ":")))

if __name__ == "__main__":
    main()
