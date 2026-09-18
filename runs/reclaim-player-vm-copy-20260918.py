from pathlib import Path
import hashlib,json

root=Path('/home/sonic/preloaded-v1/player-operation-20260918')
path=root/'game'
assert root.resolve()==root and path.resolve()==path and path.is_file()
expected='de07199d47fc88114a05d701aee48c0d9c29384bba3d62d1ecd4a0c46ed0f1cd'
with path.open('rb') as stream:
    actual=hashlib.file_digest(stream,'sha256').hexdigest()
assert actual==expected
size=path.stat().st_size
path.unlink()
report={'removed_vm_duplicate':str(path),'sha256':actual,'bytes':size,'preserved_local_copy':'out/player-operation-linux-20260918/game','local_hash_checked_before_dispatch':True}
(root/'duplicate-reclaimed.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
