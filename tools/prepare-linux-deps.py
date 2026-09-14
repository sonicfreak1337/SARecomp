"""Prepare only project-local cross-build dependencies; never install system packages."""
from pathlib import Path, PurePosixPath
import concurrent.futures
import hashlib
import io
import json
import lzma
import os
import posixpath
import shutil
import tarfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DEPS = ROOT / '.local/linux-deps'
SYSROOT = ROOT / '.local/linux-sysroot'
DOWNLOADS = (
    ('zig', 'https://ziglang.org/download/0.16.0/zig-x86_64-windows-0.16.0.zip',
     '68659eb5f1e4eb1437a722f1dd889c5a322c9954607f5edcf337bc3684a75a7e'),
    ('sdl', 'https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz',
     '7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68'),
    ('ffmpeg', 'https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-09-12-13-12/ffmpeg-n8.1.2-52-g5a03dfa0f6-linux64-lgpl-shared-8.1.tar.xz',
     'c9fccd62f756986a657b18b863518c0b2e61686577579fdcb240c4cd7fc9416b'),
)
PACKAGES = '''libx11-dev libx11-6 libxext-dev libxext6 libxrandr-dev libxrandr2
libxcursor-dev libxcursor1 libxfixes-dev libxfixes3 libxi-dev libxi6 libxss-dev
libxss1 libxrender-dev libxrender1 libxtst-dev libxtst6 x11proto-dev xtrans-dev
libudev-dev libudev1 libasound2-dev libasound2 libpulse-dev libpulse0
libdbus-1-dev libdbus-1-3 libxkbcommon-dev libxkbcommon0 libxcb1-dev libxcb1
libxau-dev libxau6 libxdmcp-dev libxdmcp6'''.split()


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def download(url, sha, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists() and digest(target) == sha:
        return target
    temporary = target.with_name(target.name + '.download')
    urllib.request.urlretrieve(url, temporary)
    if digest(temporary) != sha:
        raise RuntimeError(f'Dependency digest mismatch: {target.name}')
    temporary.replace(target)
    return target


def prepare_archive(row):
    name, url, sha = row
    archive = download(url, sha, DEPS / url.rsplit('/', 1)[1])
    destination = DEPS / name
    stamp = destination / '.sonic-prepared-sha256'
    if stamp.exists() and stamp.read_text().strip() == sha:
        return
    destination.mkdir(parents=True, exist_ok=True)
    if archive.suffix == '.zip':
        with zipfile.ZipFile(archive) as source:
            source.extractall(destination)
    else:
        with tarfile.open(archive) as source:
            source.extractall(destination, filter='data')
    stamp.write_text(sha + '\n')
    print(f'LINUX_DEPENDENCY_READY {name} sha256={sha}', flush=True)


def deb_tar(path):
    data = path.read_bytes()
    if data[:8] != b'!<arch>\n':
        raise RuntimeError('Invalid Debian package container')
    offset = 8
    while offset + 60 <= len(data):
        header = data[offset:offset + 60]
        size = int(header[48:58]); name = header[:16].decode().strip().rstrip('/')
        start = offset + 60
        if name.startswith('data.tar.'):
            return tarfile.open(fileobj=io.BytesIO(data[start:start + size]))
        offset = start + size + size % 2
    raise RuntimeError('Debian package has no data archive')


def rooted(name):
    name = posixpath.normpath(name)
    path = PurePosixPath(name)
    if path.is_absolute() or '..' in path.parts:
        raise RuntimeError(f'Package path escapes local sysroot: {name}')
    return SYSROOT.joinpath(*path.parts)


def prepare_sysroot():
    lock = DEPS / 'debian-bullseye-packages.json'
    if lock.exists():
        rows = json.loads(lock.read_text())
    else:
        base = 'https://deb.debian.org/debian/'
        release = urllib.request.urlopen(base + 'dists/bullseye/Release').read().decode()
        sha = next(line.split()[0] for line in release.split('SHA256:\n', 1)[1].splitlines()
                   if line.strip().endswith('main/binary-amd64/Packages.xz'))
        index = download(base + 'dists/bullseye/main/binary-amd64/Packages.xz', sha,
                         DEPS / 'bullseye-Packages.xz')
        wanted = set(PACKAGES); rows = []
        for paragraph in lzma.decompress(index.read_bytes()).decode().split('\n\n'):
            fields = dict(line.split(': ', 1) for line in paragraph.splitlines()
                          if ': ' in line and not line.startswith(' '))
            if fields.get('Package') in wanted:
                rows.append({key: fields[key] for key in ('Package', 'Version', 'Filename', 'SHA256')})
                wanted.remove(fields['Package'])
        if wanted:
            raise RuntimeError(f'Missing sysroot packages: {sorted(wanted)}')
        lock.write_text(json.dumps(rows, indent=2) + '\n')
    def fetch(row):
        return download('https://deb.debian.org/debian/' + row['Filename'], row['SHA256'],
                        DEPS / 'debs' / Path(row['Filename']).name)
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        archives = list(pool.map(fetch, rows))
    links = []
    for archive in archives:
        with deb_tar(archive) as source:
            for member in source:
                target = rooted(member.name)
                if member.isfile():
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with source.extractfile(member) as src, target.open('wb') as dst:
                        shutil.copyfileobj(src, dst)
                elif member.issym() or member.islnk():
                    link = member.linkname
                    if link.startswith('/'):
                        link = link.lstrip('/')
                    elif member.issym():
                        link = posixpath.join(posixpath.dirname(member.name), link)
                    links.append((target, rooted(link)))
    # Links remain inside this sysroot, including Debian's absolute /lib links.
    pending = links
    while pending:
        remaining = []
        for target, source in pending:
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists() or target.is_symlink():
                continue
            try:
                os.symlink(os.path.relpath(source, target.parent), target)
            except OSError as error:
                if os.name != 'nt' or error.winerror != 1314:
                    raise
                # Windows without symlink privilege: materialize local file
                # aliases. The cross-build explicitly selects versioned .so
                # names so SDL never embeds a development-only .so basename.
                if source.is_file():
                    shutil.copyfile(source, target)
                else:
                    remaining.append((target, source))
        if len(remaining) == len(pending):
            break  # Documentation/directory aliases need not enter the sysroot.
        pending = remaining
    (DEPS / 'sysroot-links.json').write_text(json.dumps([
        [str(a.relative_to(SYSROOT)), str(b.relative_to(SYSROOT))]
        for a, b in links], indent=2) + '\n')
    print(f'LINUX_SYSROOT_READY packages={len(rows)}', flush=True)


def main():
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(prepare_archive, DOWNLOADS))
    archive = ROOT / '.local/baseline/r354/katana-source-178448be.zip'
    if digest(archive) != '85e5bd27952552f0957559cca82bde5b4024cd71e50e94650cd922f42430cd66':
        raise RuntimeError('Pinned SDK source digest mismatch')
    sdk = ROOT / '.local/linux-sdk'
    if not sdk.exists():
        with zipfile.ZipFile(archive) as source:
            source.extractall(sdk)
    prepare_sysroot()


if __name__ == '__main__':
    main()
