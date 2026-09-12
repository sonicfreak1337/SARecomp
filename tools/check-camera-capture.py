"""Check a hidden game run's measured orbit, not just standalone camera math."""
import argparse
import json
import math
import re
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("run", type=Path)
parser.add_argument("--collision", action="store_true")
parser.add_argument("--return", dest="check_return", action="store_true")
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
if args.collision:
    assert re.search(r"SONIC_CAMERA_APPROACH .*done=1", log), "Walk did not reach the real wall"
    hits = [r for r in rows if float(r["contact"]) < 0.95]
    assert len(hits) >= 5, "No sustained collision with real geometry"
    assert min(float(r["boom"])/float(r["radius"]) for r in hits) < 0.9, "Collision did not retract the camera"
    restored = [r for r in rows if r["reset"] == "0" and r["shadow_restored"] == "1"]
    assert len(restored) > len(rows)*0.9, "Override still feeds Original camera history"
    horizontal = [r for r in rows if r["raw"] == "32767,3000"]
    assert len(horizontal) > 5 and all(float(r["stick"].split(",")[1]) == 0 for r in horizontal), "Cross-axis stick noise changed elevation"
    assert max(float(r["pitch"]) for r in horizontal)-min(float(r["pitch"]) for r in horizontal) < 0.001, "Horizontal turning drifted vertically"
    # The final quiet section follows the last deliberate vertical movement.
    quiet = []
    for r in reversed(rows):
        if r["stick"] != "0,0" or r["returning"] != "0" or r["walking"] != "0":
            break
        quiet.append(r)
    assert len(quiet) >= 30, "No sustained stationary release check"
    assert max(float(r["pitch"]) for r in quiet)-min(float(r["pitch"]) for r in quiet) < 0.001, "Released camera sank"
    result.update(scope="walk to Emerald Coast collision wall, orbit with collision, quiet stick hold",
                  collision_samples=len(hits), minimum_boom=min(float(r["boom"]) for r in hits),
                  shadow_restored_samples=len(restored), quiet_hold_samples=len(quiet),
                  hit_objects=sorted(set(r["collision_object"] for r in hits)))
if args.check_return:
    returns = re.findall(r"SONIC_CAMERA returned_to_original=1 frame=(\d+) idle=([\d.]+)", log)
    assert returns and all(float(idle) >= 3 for _, idle in returns), "No completed three-second Original return"
    first_return = int(returns[0][0])
    blend = [r for r in rows if r["returning"] == "1" and int(r["frame"]) < first_return]
    assert blend and any(r["walking"] == "1" for r in blend), "Original return was not activated by walking"
    assert all(float(r["idle"]) >= 3 for r in blend), "Original return started before timeout"
    takeover = [r for r in rows if int(r["frame"]) > first_return and r["reset"] == "1" and r["raw"] == "-32767,0"]
    assert takeover and takeover[0]["returning"] == "0", "Stick failed to retake control after Original return"
    result.update(original_return_frame=first_return, return_blend_samples=len(blend),
                  manual_retake_frame=int(takeover[0]["frame"]))
(args.run / "camera-check.json").write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
print(json.dumps(result))
