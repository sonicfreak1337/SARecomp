"""Pinned portable rasterizer and redistributable fonts for native product UI."""
from pathlib import Path
import concurrent.futures
import hashlib
import json
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / '.local/ui-deps'
STB = '2c980bb59875b0d32144a71867fbdebb2f77cd20'
NOTO = 'ffebf8c1ee449e544955a7e813c54f9b73848eac'
CJK = 'f8d157532fbfaeda587e826d4cd5b21a49186f7c'
FILES = {**{name: f'https://raw.githubusercontent.com/nothings/stb/{STB}/{name}'
            for name in ['stb_image.h', 'stb_image_write.h', 'stb_truetype.h', 'LICENSE']},
         **{name: f'https://raw.githubusercontent.com/notofonts/noto-fonts/{NOTO}/hinted/ttf/NotoSans/{name}'
            for name in ['NotoSans-Regular.ttf', 'NotoSans-Bold.ttf']},
         'Noto-OFL.txt': f'https://raw.githubusercontent.com/notofonts/noto-fonts/{NOTO}/LICENSE',
         'NotoSansCJKjp-Regular.otf': f'https://raw.githubusercontent.com/notofonts/noto-cjk/{CJK}/Sans/OTF/Japanese/NotoSansCJKjp-Regular.otf',
         'Noto-CJK-OFL.txt': f'https://raw.githubusercontent.com/notofonts/noto-cjk/{CJK}/Sans/LICENSE'}
HASHES = {
    'NotoSansCJKjp-Regular.otf': '68a3fc98800b2a27b371f2fb79991daf3633bd89309d4ffaa6946fd587f375b5',
    'Noto-CJK-OFL.txt': '6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2',
    'stb_image.h': '594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3',
    'stb_image_write.h': 'cbd5f0ad7a9cf4468affb36354a1d2338034f2c12473cf1a8e32053cb6914a05',
    'stb_truetype.h': 'ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab',
    'LICENSE': 'bebfe904b14301657e4e5d655c811d51fd31b97c455b9cc2d8600d6bac6cff63',
    'NotoSans-Regular.ttf': 'b85c38ecea8a7cfb39c24e395a4007474fa5a4fc864f6ee33309eb4948d232d5',
    'NotoSans-Bold.ttf': 'c976e4b1b99edc88775377fcc21692ca4bfa46b6d6ca6522bfda505b28ff9d6a',
    'Noto-OFL.txt': '0dab92d0544f7b233403f14b84a663bdbfa746982eda629e7f4f9ffe1b036feb',
}

def obtain(item):
    name, url = item
    path = DEST / name
    if not path.exists():
        temporary = path.with_suffix(path.suffix + '.download')
        urllib.request.urlretrieve(url, temporary)
        if hashlib.sha256(temporary.read_bytes()).hexdigest() != HASHES[name]:
            raise RuntimeError('UI dependency checksum mismatch: ' + name)
        temporary.replace(path)
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != HASHES[name]:
        raise RuntimeError('Cached UI dependency checksum mismatch: ' + name)
    return {'file': name, 'source': url, 'sha256': digest}

if __name__ == '__main__':
    DEST.mkdir(parents=True, exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as workers:
        rows = list(workers.map(obtain, FILES.items()))
    (DEST / 'provenance.json').write_text(json.dumps(rows, indent=2) + '\n')
    print('SONIC_PORTABLE_UI_DEPENDENCIES_READY')
