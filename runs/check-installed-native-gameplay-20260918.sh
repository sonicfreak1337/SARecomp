#!/bin/bash
set -eu
root=/home/sonic/preloaded-v1
checks=$root/native-gameplay-checks-20260918
exe=$root/native-gameplay-install-20260918/SARecomp-app/1.0-candidate-2222222222222222/game
content=/home/sonic/linux-test-installer-20260917/state-content/content-2939662201-3775219401
lib=/home/sonic/linux-test-installer-20260917/SARecomp-app/1.0-candidate-4bd2a5d52a5576a1/lib
for mode in original recompiled; do
 LP_NUM_THREADS=2 xvfb-run -a python3 "$checks/benchmark-linux-stage.py" --exe "$exe" --content "$content" --lib "$lib" --run "$root/native-gameplay-installed-$mode-20260918" --scenario gamma-emerald-coast --gameplay-timing "$mode" --aspect deck --begin-frame 5 --end-frame 25 --diagnostics installed --transfer-plans installed --ram-regions installed --telemetry off --native-movement-contact installed --native-model-submission installed --native-land-render installed --native-object-contact installed --native-camera-operation installed --native-actor-operation installed --native-player-operation installed > "$root/native-gameplay-installed-$mode-20260918-command.log" 2>&1
 echo "COMPLETE installed $mode"
done
