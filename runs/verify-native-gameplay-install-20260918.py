from pathlib import Path
import argparse,hashlib,json,os,subprocess
p=argparse.ArgumentParser()
p.add_argument('--patch',type=Path,required=True)
p.add_argument('--metadata',type=Path,required=True)
p.add_argument('--on',type=Path,required=True)
p.add_argument('--off',type=Path,required=True)
a=p.parse_args()
root=Path('/home/sonic/preloaded-v1/native-gameplay-install-20260918')
assert not root.exists() and os.getuid()!=0
root.mkdir();apps=root/'SARecomp-app';apps.mkdir()
meta=json.loads(a.metadata.read_text());target=meta['target_sha256'];size=meta['target_bytes']
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
assert sha(a.patch)==meta['sha256']
sources=[Path('/home/sonic/preloaded-v1/native-groups-install-20260918/SARecomp-app/1.0-candidate-2222222222222222'),Path('/home/sonic/preloaded-v1/native-groups-install-20260917/SARecomp-app/1.0-candidate-9999999999999999'),Path('/home/sonic/linux-test-installer-20260917/SARecomp-app/1.0-candidate-4bd2a5d52a5576a1')]
expected=['55889c92ee5c5ab6a34f1d8733a8698bab6da3b20eaa416fa48b593c0b335f71','b5599d23ab3bcf2a0f54207409ff31109b20c84b42782b4f2036fc8c5d447ecc','f7023f123e6fcc4d76361cf05eba131db8344197e38528eab5e6165e1b187f8b']
before=[]
for index,(source,base) in enumerate(zip(sources,expected)):
 assert sha(source/'game')==base
 d=apps/('1.0-candidate-'+str(index+1)*16);(d/'resources').mkdir(parents=True)
 os.link(source/'game',d/'game')
 manifest=(source/'resources/payload-files.tsv').read_bytes();(d/'resources/payload-files.tsv').write_bytes(manifest)
 policy=b'SARECOMP-DIAGNOSTICS-1\n'+(b'on\n' if index==0 else b'off\n');(d/'.sarecomp-diagnostics').write_bytes(policy)
 (d/'assets').symlink_to('/home/sonic/preloaded-v1/world-oracle-20260917/assets',target_is_directory=True)
 before.append((d,base,manifest,policy))
user=root/'user-data';user.mkdir()
for name in ('story.ksave','chao.ksave','display.ini'):(user/name).write_bytes(('preserve '+name+'\n').encode())
preserved={p.name:sha(p) for p in user.iterdir()}
def apply(package,log):
 r=subprocess.run(['bash',str(package.resolve()),'--app-root',str(apps),'--headless'],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=900)
 (root/log).write_text(r.stdout)
 assert r.returncode==0,r.stdout
 return r.stdout
apply(a.patch,'apply.log')
records=[]
for d,base,manifest,policy in before:
 assert sha(d/'game')==target and (d/'game').stat().st_size==size
 assert sha(d/'game.pre-native-gameplay-20260918')==base
 assert (d/'resources/payload-files.tsv.pre-native-gameplay-20260918').read_bytes()==manifest
 current=(d/'resources/payload-files.tsv').read_text().splitlines()
 assert [s for s in current if s.startswith('game\t')]==[f'game\t{size}\t{target}']
 assert [s for s in current if not s.startswith('game\t')]==[s for s in manifest.decode().splitlines() if not s.startswith('game\t')]
 assert (d/'.sarecomp-diagnostics').read_bytes()==policy
 records.append({'app':str(d),'base_sha256':base,'target_sha256':target,'inode':(d/'game').stat().st_ino,'mode':(d/'game').stat().st_mode})
assert 'already installed' in apply(a.patch,'apply-again.log')
for d,r in zip((v[0] for v in before),records):assert (d/'game').stat().st_ino==r['inode']
for package,mode in ((a.on,'on'),(a.off,'off'),(a.off,'off')):
 apply(package,'diagnostics-'+mode+'-'+package.name+'.log')
 for (d,_,_,_),r in zip(before,records):
  assert (d/'.sarecomp-diagnostics').read_text()=='SARECOMP-DIAGNOSTICS-1\n'+mode+'\n'
  assert sha(d/'game')==target and (d/'game').stat().st_ino==r['inode'] and (d/'game').stat().st_mode==r['mode']
for d,_,_,policy in before:(d/'.sarecomp-diagnostics').write_bytes(policy)
assert {p.name:sha(p) for p in user.iterdir()}==preserved
for source,base in zip(sources,expected):assert sha(source/'game')==base
report={'passed':True,'package_sha256':sha(a.patch),'programs':records,'all_three_supported_bases':True,'same_program_inode':len({r['inode'] for r in records})==1,'saves_chao_settings_preserved':True,'policy_preserved':True,'idempotent':True,'diagnostic_switches_passed':True,'original_installations_unchanged':True}
(root/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
