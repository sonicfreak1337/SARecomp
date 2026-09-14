"""Prepare a project-local headless Linux test VM; never install a hypervisor."""
from pathlib import Path
import concurrent.futures
import hashlib
import json
import subprocess
import urllib.request
import sys
import ctypes

ROOT = Path(__file__).resolve().parents[1]
VM = ROOT / '.local/linux-test-vm'


def fetch(url, target, algorithm=None, expected=None):
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists() and expected and digest(target, algorithm) == expected:
        return target
    temporary = target.with_suffix(target.suffix + '.download')
    urllib.request.urlretrieve(url, temporary)
    if expected and digest(temporary, algorithm) != expected:
        raise RuntimeError('Download checksum mismatch: ' + target.name)
    temporary.replace(target)
    return target


def digest(path, algorithm='sha256'):
    with path.open('rb') as source:
        return hashlib.file_digest(source, algorithm).hexdigest()


def qemu():
    base = 'https://qemu.weilnetz.de/w64/qemu-w64-setup-20260811'
    expected = urllib.request.urlopen(base + '.sha512').read().decode().split()[0]
    archive = fetch(base + '.exe', VM / 'qemu-installer.exe', 'sha512', expected)
    seven = VM / '7zip/Files/7-Zip/7z.exe'
    if not seven.exists():
        msi = fetch('https://github.com/ip7z/7zip/releases/download/26.03/7z2603-x64.msi', VM / '7zip.msi')
        # Read the cabinet stream without invoking Windows Installer or UAC.
        api = ctypes.WinDLL('msi.dll')
        api.MsiOpenDatabaseW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint)]
        api.MsiDatabaseOpenViewW.argtypes = [ctypes.c_uint, ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_uint)]
        api.MsiRecordGetStringW.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_uint)]
        db = ctypes.c_uint()
        if api.MsiOpenDatabaseW(str(msi), None, ctypes.byref(db)):
            raise RuntimeError('Cannot read 7-Zip MSI')
        def query(sql):
            view = ctypes.c_uint()
            if api.MsiDatabaseOpenViewW(db, sql, ctypes.byref(view)) or api.MsiViewExecute(view, 0):
                raise RuntimeError('Cannot read MSI table')
            record = ctypes.c_uint()
            if api.MsiViewFetch(view, ctypes.byref(record)):
                raise RuntimeError('Missing MSI row')
            api.MsiCloseHandle(view)
            return record
        record = query('SELECT `Cabinet` FROM `Media`')
        size = ctypes.c_uint(1024); name = ctypes.create_unicode_buffer(1024)
        api.MsiRecordGetStringW(record, 1, name, ctypes.byref(size)); api.MsiCloseHandle(record)
        record = query("SELECT `Data` FROM `_Streams` WHERE `Name`='" + name.value.lstrip('#') + "'")
        cabinet = VM / '7zip.cab'
        with cabinet.open('wb') as output:
            while True:
                size = ctypes.c_uint(65536); block = ctypes.create_string_buffer(size.value)
                if api.MsiRecordReadStream(record, 1, block, ctypes.byref(size)):
                    raise RuntimeError('Cannot extract MSI cabinet')
                if not size.value: break
                output.write(block.raw[:size.value])
        api.MsiCloseHandle(record); api.MsiCloseHandle(db)
        (VM / '7zip').mkdir(exist_ok=True)
        subprocess.run(['expand.exe', '-F:*', str(cabinet), str(VM / '7zip')],
                       check=True, stdout=subprocess.DEVNULL, creationflags=0x08000000)
        for name in ('7z.exe', '7z.dll'):
            extracted = VM / '7zip' / ('_' + name)
            if extracted.exists(): extracted.replace(extracted.with_name(name))
        matches = list((VM / '7zip').rglob('7z.exe'))
        if len(matches) != 1:
            raise RuntimeError('Cannot locate extracted 7-Zip')
        seven = matches[0]
    if not (VM / 'qemu/qemu-system-x86_64.exe').exists():
        with (VM / 'qemu-extract.log').open('wb') as log:
            subprocess.run([str(seven), 'x', str(archive), '-o' + str(VM / 'qemu'), '-y'],
                           check=True, stdout=log, stderr=subprocess.STDOUT, creationflags=0x08000000)
    return {'qemu_sha512': expected}


def ubuntu():
    base = 'https://cloud-images.ubuntu.com/noble/20260911/'
    name = 'noble-server-cloudimg-amd64.img'
    manifest = urllib.request.urlopen(base + 'SHA256SUMS').read().decode()
    expected = next(row.split()[0] for row in manifest.splitlines() if row.split()[-1].lstrip('*') == name)
    fetch(base + name, VM / name, 'sha256', expected)
    return {'ubuntu_sha256': expected, 'ubuntu_source': base + name}


if __name__ == '__main__':
    VM.mkdir(parents=True, exist_ok=True)
    record = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as workers:
        for result in workers.map(lambda task: task(), (qemu, ubuntu)):
            record.update(result)
    (VM / 'dependencies.json').write_text(json.dumps(record, indent=2) + '\n')
    print('SONIC_LINUX_TEST_VM_DEPENDENCIES_READY', flush=True)
