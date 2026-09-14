"""Independent Windows save-format fixtures plus a native Linux contract run."""
import hashlib
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

def record(slot, schema, generation, payload):
    fields = b""
    for text in ("katana-native-save-v2", "sonic-linux-contract", slot):
        encoded = text.encode()
        fields += struct.pack("<I", len(encoded)) + encoded
    fields += struct.pack("<IQQ", schema, generation, len(payload)) + payload
    digest = hashlib.sha256(fields).digest()
    return b"KATANASAVE\r\n\x1a\n\0\1" + struct.pack("<IIIIQQ", 2, 80, schema, 0, generation, len(payload)) + digest + payload

binary = Path(sys.argv[1]).resolve()
root = Path(tempfile.mkdtemp(prefix="sonic-linux-platform-", dir=sys.argv[2]))
(root / "content").mkdir()
(root / "content/source.bin").write_bytes(b"BOUND-CONTENT-0123456789")
(root / "outside.bin").write_bytes(b"OUTSIDE_SENTINEL")
saves = root / "state/sonic-linux-contract/saves"
saves.mkdir(parents=True)
original = record("story", 7, 41, b"STORY_FULL_CLEAR_FIXTURE_41")
(saves / "story.ksave").write_bytes(original)
(saves / "future.ksave").write_bytes(record("future", 8, 9, b"FUTURE_SCHEMA"))
(saves / "future.ksave.bak").write_bytes(record("future", 7, 8, b"OLD_SCHEMA"))
corrupt = bytearray(record("broken", 8, 9, b"CORRUPT"))
corrupt[-1] ^= 1
(saves / "broken.ksave").write_bytes(corrupt)
subprocess.run([str(binary), str(root)], check=True,
               env={**os.environ, "KATANA_PORT_BACKGROUND_TEST": "1", "SDL_JOYSTICK_HIDAPI": "0"})
assert (root / "linux-output.ksave").read_bytes() == record("story", 7, 42, b"STORY_FULL_CLEAR_FIXTURE_42")
assert (root / "linux-backup.ksave").read_bytes() == original
assert (root / "outside.bin").read_bytes() == b"OUTSIDE_SENTINEL"
print("SONIC_LINUX_SAVE_INTEROP_OK byte_exact=1 outside_unchanged=1 root=" + str(root))
