"""Start only the owned, headless Linux test machine, with localhost-only SSH."""
from pathlib import Path
import json
import os
import subprocess
import sys
import io

ROOT = Path(__file__).resolve().parents[1]
VM = ROOT / '.local/linux-test-vm'
sys.path.insert(0, str(VM / 'python'))
import pycdlib

VM.mkdir(parents=True, exist_ok=True)
key = VM / 'id_ed25519'
if not key.exists():
    subprocess.run(['ssh-keygen.exe', '-q', '-t', 'ed25519', '-N', '', '-f', str(key)], check=True)
public_key = key.with_suffix('.pub').read_text().strip()
seed = VM / 'seed.iso'
if not seed.exists():
    user_data = '''#cloud-config
hostname: sarecomp-test
users:
  - name: sonic
    groups: [sudo, audio, video, render]
    shell: /bin/bash
    sudo: ALL=(ALL) NOPASSWD:ALL
    lock_passwd: true
    ssh_authorized_keys:
      - ''' + public_key + '''
ssh_pwauth: false
disable_root: true
package_update: true
package_upgrade: false
packages: [xvfb, mesa-vulkan-drivers, vulkan-tools, libx11-6, libxcursor1, libxrandr2, libxi6, libasound2t64, libudev1, libpulse0, libdbus-1-3, libxss1, libxtst6, xauth]
runcmd:
  - [touch, /home/sonic/linux-test-ready]
'''
    iso = pycdlib.PyCdlib(); iso.new(interchange_level=3, joliet=3, vol_ident='cidata')
    for name, content in [('user-data', user_data), ('meta-data', 'instance-id: sarecomp-linux-20260914\nlocal-hostname: sarecomp-test\n')]:
        payload = content.encode()
        iso.add_fp(io.BytesIO(payload), len(payload), iso_path='/' + name.upper().replace('-', '_') + ';1', joliet_path='/' + name)
    iso.write(str(seed)); iso.close()
qemu_root = VM / 'qemu'
disk = VM / 'test.qcow2'
if not disk.exists():
    subprocess.run([str(qemu_root / 'qemu-img.exe'), 'create', '-f', 'qcow2', '-F', 'qcow2',
                    '-b', str(VM / 'noble-server-cloudimg-amd64.img'), str(disk), '48G'], check=True)
pid_file = VM / 'vm.json'
if pid_file.exists():
    old = json.loads(pid_file.read_text())
    # The PID is evidence for cleanup, not authority to stop/reuse a process.
    raise SystemExit('VM has already been started; inspect vm.json and its process before restarting.')
argv = [str(qemu_root / 'qemu-system-x86_64.exe'), '-accel', 'tcg,thread=multi', '-cpu', 'max',
        '-machine', 'q35,hpet=off', '-smp', '2', '-m', '6144', '-display', 'none', '-monitor', 'none',
        '-serial', 'file:' + str(VM / 'serial.log'), '-no-reboot',
        '-drive', 'file=' + str(disk) + ',format=qcow2,if=virtio',
        '-drive', 'file=' + str(seed) + ',format=raw,media=cdrom,readonly=on',
        '-nic', 'user,model=virtio-net-pci,hostfwd=tcp:127.0.0.1:22230-:22']
with (VM / 'qemu.log').open('ab') as log:
    process = subprocess.Popen(argv, stdin=subprocess.DEVNULL, stdout=log, stderr=log, creationflags=0x08000000)
pid_file.write_text(json.dumps({'pid': process.pid, 'executable': argv[0], 'ssh_port': 22230}, indent=2) + '\n')
print('SONIC_LINUX_TEST_VM_STARTED pid=' + str(process.pid) + ' ssh=127.0.0.1:22230')
