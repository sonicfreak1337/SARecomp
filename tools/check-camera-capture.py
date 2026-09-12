"""Check a hidden game run's measured orbit, not just standalone camera math."""
import argparse
import json
import math
import re
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("run", type=Path)
args = parser.parse_args()
log = (args.run / "stderr.log").read_text(encoding="utf-8")
rows = []
for line in log.splitlines():
    if not line.startswith("SONIC_CAMERA active=1 "):
        continue
    row = dict(re.findall(r"(\w+)=([^ ]+)", line))
    if not rows or row["frame"] != rows[-1]["frame"]:
        rows.append(row)
assert len(rows) > 30, "No sustained manual gameplay camera"
rotation = largest_rotation = 0.0
previous = None
aim_error = 0.0
for row in rows:
    yaw, pitch = float(row["yaw"]), float(row["pitch"])
    if row["reset"] == "1":
        rotation = 0.0
        previous = None
    if previous is not None:
        rotation += math.remainder(yaw - previous, 2 * math.pi)
        largest_rotation = max(largest_rotation, abs(rotation))
    previous = yaw
    eye, target = [tuple(map(float, row[k].split(","))) for k in ("eye", "target")]
    direction = tuple(b - a for a, b in zip(eye, target))
    radius = math.sqrt(sum(v * v for v in direction))
    forward = (-math.sin(yaw)*math.cos(pitch), math.sin(pitch), -math.cos(yaw)*math.cos(pitch))
    aim_error = max(aim_error, math.sqrt(sum((a/radius-b)**2 for a, b in zip(direction, forward))))
    assert row["state"] == "15" and row["level"] in ("0", "1"), "Non-gameplay override"
pitches = [math.degrees(float(r["pitch"])) for r in rows]
sticks = [tuple(map(float, r["stick"].split(","))) for r in rows]
assert math.degrees(largest_rotation) >= 360, "Incomplete continuous 360 degree orbit"
assert min(pitches) < -60 and max(pitches) > 3, "Missing vertical range"
assert any(x > 0.9 for x, y in sticks) and any(y > 0.9 for x, y in sticks) and any(y < -0.9 for x, y in sticks), "Missing right-stick inputs"
assert aim_error < 0.001, "Camera lost its character target"
assert "stop_reason=2" in log and "KATANA_NATIVE_PORT_CONTRACT failure=" not in log, "Run did not finish its expected diagnostic deadline"
captures = list((args.run / "frames").glob("frame-*.bmp"))
assert len(captures) >= 5, "Missing visual evidence"
result = dict(passed=True, gameplay_samples=len(rows), continuous_yaw_degrees=math.degrees(largest_rotation),
              pitch_min_degrees=min(pitches), pitch_max_degrees=max(pitches),
              maximum_aim_vector_error=aim_error, captures=len(captures),
              stop="expected diagnostic deadline", scope="stationary Emerald Coast orbit; inspect captures separately")
(args.run / "camera-check.json").write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
print(json.dumps(result))
