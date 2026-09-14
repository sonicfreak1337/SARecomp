"""Replace only the pinned host PCM endpoint; keep the stream/domain facade."""
from pathlib import Path
import hashlib
import sys

source, destination = map(Path, sys.argv[1:])
data = source.read_bytes()
if hashlib.sha256(data).hexdigest() != '0153b9d08fcecf5aba5ddf8872ae2e6342d1f6330c737684c0487bdebac71499':
    raise RuntimeError('Pinned audio endpoint source changed')
text = data.decode()
begin = text.index('class NativePortAudioEndpointCore final {')
end = text.index('\nnamespace {\n\nenum class NativePortAudioStreamOpcode', begin)
text = text[:begin] + '#include "native_port_audio_sdl.inc"\n' + text[end:]
text = '#include "sonic_audio_device.hpp"\n#include <deque>\n#include <cstdio>\n' + text
destination.parent.mkdir(parents=True, exist_ok=True)
output = text.encode()
if not destination.exists() or destination.read_bytes() != output:
    destination.write_bytes(output)
print('SONIC_LINUX_AUDIO_READY facade_and_domain=retained endpoint=SDL3')
