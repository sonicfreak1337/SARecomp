"""Exercise patch publication and failure recovery on small isolated fixtures."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--script', type=Path, required=True)
    p.add_argument('--decoder', type=Path, required=True)
    p.add_argument('--run', type=Path, required=True)
    args = p.parse_args()
    script, decoder = args.script.resolve(), args.decoder.resolve()
    root = args.run.resolve()
    root.mkdir(parents=True, exist_ok=False)
    assert os.getuid() != 0
    records = []

    def fixture(name, busy=False):
        run = root/name
        bundle, apps = run/'payload', run/'SARecomp-app'
        bundle.mkdir(parents=True); apps.mkdir()
        shutil.copyfile(decoder, bundle/'zstd'); (bundle/'zstd').chmod(0o755)
        old, other, target = run/'old', run/'other', run/'target'
        if busy:
            shutil.copyfile('/bin/sleep', old)
        else:
            old.write_bytes(bytes(range(256))*24)
        other.write_bytes(b'earlier version\0'*400)
        target.write_bytes(old.read_bytes()[:100]+b'fixed math\0'*30+old.read_bytes()[110:])
        subprocess.run([str(decoder), '-q', '--patch-from='+str(old), str(target),
                        '-o', str(bundle/'game.delta.zst')], check=True)
        metadata = ['SARECOMP-RUNTIME-PATCH-1', f'target\t{target.stat().st_size}\t{sha(target)}',
                    'base\t'+sha(old), 'delta\t'+sha(bundle/'game.delta.zst'),
                    'tool\t'+sha(bundle/'zstd'), 'supported\t'+sha(old), 'supported\t'+sha(other)]
        (bundle/'patch.tsv').write_text('\n'.join(metadata)+'\n')
        dirs = [apps/('1.0-candidate-'+c*16) for c in 'ab']
        for directory, source in zip(dirs, (old, other)):
            (directory/'resources').mkdir(parents=True)
            shutil.copyfile(source, directory/'game'); (directory/'game').chmod(0o755)
            (directory/'resources/payload-files.tsv').write_text(
                f'SARECOMP-PAYLOAD-1\ngame\t{source.stat().st_size}\t{sha(source)}\nasset\t1\tabc\n')
        (run/'SARecomp').mkdir()
        for name, data in [('story.vmu',b'story progress'), ('chao.vmu',b'chao progress'), ('sonic-display.ini',b'gameplay_timing=0\n')]:
            (run/'SARecomp'/name).write_bytes(data)
        (run/'steam-shortcut.txt').write_text(str(dirs[1]/'game'))
        return run, bundle, apps, dirs, target

    def snapshot(run):
        return {str(path.relative_to(run)):sha(path) for path in (run/'SARecomp').iterdir()}

    def apply(data, expect_success, env=None):
        run,bundle,apps,dirs,target = data
        saves, shortcut = snapshot(run), (run/'steam-shortcut.txt').read_bytes()
        result = subprocess.run(['bash',str(script),str(bundle),'--app-root',str(apps),'--headless'],
                                env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (run/'last-run.log').write_text(result.stdout)
        assert (result.returncode == 0) == expect_success, result.stdout
        assert snapshot(run) == saves and (run/'steam-shortcut.txt').read_bytes() == shortcut
        return result

    data = fixture('success')
    before = [sha(d/'game') for d in data[3]]
    apply(data, True)
    for directory, old in zip(data[3], before):
        assert sha(directory/'game') == sha(data[4])
        assert sha(directory/'game.pre-native-math-v1') == old
        assert sha(data[4]) in (directory/'resources/payload-files.tsv').read_text()
    apply(data, True)
    records.append('update-both-steam-launch-paths-and-idempotence')

    for name in ('damaged-game','damaged-delta','missing-v5'):
        data = fixture(name)
        if name=='damaged-game':
            (data[3][1]/'game').write_bytes(b'user-modified binary')
        elif name=='damaged-delta':
            (data[1]/'game.delta.zst').write_bytes(b'corrupt download')
        else:
            shutil.rmtree(data[3][0])
        before = {d:sha(d/'game') for d in data[3] if d.exists()}
        apply(data, False)
        assert all(sha(d/'game')==value for d,value in before.items())
        records.append(name+'-rejected-without-overwrite')

    data = fixture('running-game', busy=True)
    game = subprocess.Popen([str(data[3][0]/'game'),'30'])
    try:
        apply(data, False)
        assert sha(data[3][0]/'game') == sha(data[0]/'old')
    finally:
        game.terminate(); game.wait()
    records.append('running-game-rejected')

    data = fixture('publication-failure')
    shim = data[0]/'commands'; shim.mkdir()
    # Fail the manifest rename after the first new game has been published.
    # Subsequent real mv calls let the updater exercise its rollback handler.
    (shim/'mv').write_text('#!/bin/sh\ncount=0\n[ ! -f "$SARECOMP_PATCH_TEST_COUNT" ] || count=$(cat "$SARECOMP_PATCH_TEST_COUNT")\ncount=$((count+1))\nprintf "%s" "$count" > "$SARECOMP_PATCH_TEST_COUNT"\n[ "$count" -ne 2 ] || exit 42\nexec /usr/bin/mv "$@"\n')
    (shim/'mv').chmod(0o755)
    env=dict(os.environ,PATH=str(shim)+':'+os.environ['PATH'],SARECOMP_PATCH_TEST_COUNT=str(data[0]/'rename-count'))
    before={d:(sha(d/'game'),sha(d/'resources/payload-files.tsv')) for d in data[3]}
    apply(data, False, env)
    assert all((sha(d/'game'),sha(d/'resources/payload-files.tsv'))==value for d,value in before.items())
    apply(data, True)
    records.append('publication-failure-rolls-back-and-retry-succeeds')

    data = fixture('interrupted-manifest')
    apply(data, True)
    directory=data[3][0]
    # Model loss of power between the atomic game and manifest renames.
    replacement=directory/'resources/old-manifest'
    shutil.copyfile(directory/'resources/payload-files.tsv.pre-native-math-v1',replacement)
    os.replace(replacement,directory/'resources/payload-files.tsv')
    apply(data, True)
    assert sha(data[4]) in (directory/'resources/payload-files.tsv').read_text()
    records.append('interrupted-manifest-repaired')
    data = fixture('diagnostic-roundtrip')
    bundle=data[1]
    metadata=(bundle/'patch.tsv').read_text()
    (bundle/'patch.tsv').write_text(metadata+'diagnostics\ton\n')
    apply(data,True)
    expected=b'SARECOMP-DIAGNOSTICS-1\non\n'
    assert all((d/'.sarecomp-diagnostics').read_bytes()==expected for d in data[3])
    binaries=[(sha(d/'game'),(d/'game').stat().st_ino) for d in data[3]]
    for mode in ('off','on','on','off'):
        (bundle/'patch.tsv').write_text(metadata+'diagnostics\t'+mode+'\n')
        apply(data,True)
        assert all((d/'.sarecomp-diagnostics').read_bytes()==b'SARECOMP-DIAGNOSTICS-1\n'+mode.encode()+b'\n' for d in data[3])
        assert [(sha(d/'game'),(d/'game').stat().st_ino) for d in data[3]]==binaries
    records.append('diagnostics-on-off-roundtrip-retains-executable-inodes-and-saves')
    # Reject a policy redirected outside the owned installation.
    policy=data[3][0]/'.sarecomp-diagnostics'
    policy.unlink(); policy.symlink_to(data[0]/'SARecomp/story.vmu')
    apply(data,False)
    records.append('diagnostics-policy-symlink-rejected')
    data=fixture('cpu-update-from-native-math')
    metadata=(data[1]/'patch.tsv').read_text()+'patch-id\tperformance-20260916\nreference-backup\tgame.pre-diagnostics-v1\n'
    (data[1]/'patch.tsv').write_text(metadata)
    before={d:sha(d/'game') for d in data[3]}
    for directory in data[3]:
        (directory/'game.pre-native-math-v1').write_bytes(b'older existing backup')
    apply(data,True);apply(data,True)
    assert all(sha(d/'game.pre-performance-20260916')==old for d,old in before.items())
    assert all((d/'game.pre-native-math-v1').read_bytes()==b'older existing backup' for d in data[3])
    records.append('cpu-update-uses-distinct-backups-and-is-idempotent')

    for mode in ('valid','missing','damaged','symlink','rollback'):
        data=fixture('cpu-update-diagnostics-'+mode)
        run,bundle,apps,dirs,target=data
        shutil.rmtree(dirs[0])
        directory=dirs[1]
        policy=b'SARECOMP-DIAGNOSTICS-1\non\n'
        (directory/'.sarecomp-diagnostics').write_bytes(policy)
        reference=directory/'game.pre-diagnostics-v1'
        if mode=='symlink':reference.symlink_to(run/'old')
        elif mode!='missing':shutil.copyfile(run/'old',reference)
        if mode=='damaged':reference.write_bytes(b'bad reference program')
        (bundle/'patch.tsv').write_text((bundle/'patch.tsv').read_text()+
            'patch-id\tperformance-20260916\nreference-backup\tgame.pre-diagnostics-v1\n')
        before=(sha(directory/'game'),sha(directory/'resources/payload-files.tsv'))
        env=None
        if mode=='rollback':
            shim=run/'commands';shim.mkdir()
            (shim/'mv').write_text('#!/bin/sh\ncount=0\n[ ! -f "$SARECOMP_PATCH_TEST_COUNT" ] || count=$(cat "$SARECOMP_PATCH_TEST_COUNT")\ncount=$((count+1))\nprintf "%s" "$count" > "$SARECOMP_PATCH_TEST_COUNT"\n[ "$count" -ne 2 ] || exit 42\nexec /usr/bin/mv "$@"\n')
            (shim/'mv').chmod(0o755)
            env=dict(os.environ,PATH=str(shim)+':'+os.environ['PATH'],SARECOMP_PATCH_TEST_COUNT=str(run/'rename-count'))
        apply(data,mode=='valid',env)
        assert (directory/'.sarecomp-diagnostics').read_bytes()==policy
        if mode=='valid':
            assert sha(directory/'game')==sha(target)
            assert sha(directory/'game.pre-performance-20260916')==before[0]
            assert sha(reference)==sha(run/'old')
        else:
            assert (sha(directory/'game'),sha(directory/'resources/payload-files.tsv'))==before
        records.append('cpu-update-diagnostics-reference-'+mode)
    (root/'result.json').write_text(json.dumps({'passed':True,'checks':records},indent=2)+'\n')
    print('SONIC_RUNTIME_PATCH_TESTS_OK '+json.dumps(records))


if __name__ == '__main__':
    main()
