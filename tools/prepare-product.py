"""Create the writable dispatch/metadata work area, retaining the sealed AOT archive."""
import json
from pathlib import Path
import shutil
import sys
import hashlib
import subprocess

root = Path(__file__).resolve().parents[1]
working = root / '.local/working-product'
base = root / '.local/baseline/r354'
if not working.exists():
    shutil.copytree(base/'product/generated', working/'generated', copy_function=shutil.copyfile)
    shutil.copyfile(root/'.local/toolchain/r354.katana-native-port',working/'current.katana-native-port')
subprocess.run([sys.executable,str(root/'tools/rebind-working-copy.py')],check=True)
if '--refresh' in sys.argv:
    if (working/'current.katana-native-port').read_bytes() == (working/'next.katana-native-port').read_bytes():
        print('SONIC_DISPATCH_UNCHANGED aot_archive=unchanged')
        raise SystemExit(0)
    tool = root/'build-performance/sonic_native_provider_refresh.exe'
    subprocess.run([str(tool),str(working/'generated'),str(working/'current.katana-native-port'),
                    str(working/'next.katana-native-port')],check=True)
    shutil.copyfile(working/'next.katana-native-port',working/'current.katana-native-port')
    print('SONIC_DISPATCH_REFRESHED aot_archive=unchanged')
