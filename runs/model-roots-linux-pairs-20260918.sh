set -eu
export LP_NUM_THREADS=2
for scenario in gamma-emerald-coast knuckles-lost-world; do
  for mode in off on; do
    xvfb-run -a python3 /home/sonic/preloaded-v1/model-roots-20260918/benchmark-linux-stage.py --exe /home/sonic/preloaded-v1/model-roots-20260918/game --content /home/sonic/linux-test-installer-20260917/state-content/content-2939662201-3775219401 --lib /home/sonic/linux-test-installer-20260917/SARecomp-app/1.0-candidate-4bd2a5d52a5576a1/lib --run /home/sonic/preloaded-v1/model-roots-${scenario}-${mode}-20260918 --scenario "$scenario" --gameplay-timing original --aspect deck --begin-frame 5 --end-frame 25 --diagnostics off --transfer-plans installed --ram-regions installed --telemetry off --native-movement-contact "$mode" --native-model-submission "$mode" > /home/sonic/preloaded-v1/model-roots-${scenario}-${mode}-20260918.log 2>&1
  done
done
