"""Exercise capture rejection with complete, drifting, missing and fallback cases."""
import csv
import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path

script = Path(__file__).with_name("analyze-performance.py")
spec = importlib.util.spec_from_file_location("performance", script)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
cases = ("native-a", "scene-480", "scene-320", "headlights", "world-shader", "depth", "optimized-repeat", "native-b")

with tempfile.TemporaryDirectory(prefix="openug2-perf-analysis-") as temporary:
    root = Path(temporary)
    for i, name in enumerate(cases):
        folder = root / name
        folder.mkdir()
        width, height = ((640, 480) if i in (0, 7) else (480, 360) if i == 1 else (320, 240))
        row = dict(frame=0, frame_ms=100, display_width=640, display_height=480,
                   scene_width=width, scene_height=height, headlight_profile="reference" if i in (0, 1, 2, 7) else "fast",
                   headlight_size=512 if i in (0, 1, 2, 7) else 256,
                   headlight_samples=4 if i in (0, 1, 2, 7) else 1,
                   world_shader_requested="auto" if i in (4, 5, 6) else "legacy",
                   world_shader_effective="auto" if i in (4, 5, 6) else "legacy",
                   world_order_requested="depth" if i in (5, 6) else "legacy",
                   world_order_effective="depth" if i in (5, 6) else "legacy",
                   cpu_composite=1, cpu_hud=1, cpu_queue=.1)
        with (folder / "performance.csv").open("w", newline="") as stream:
            writer = csv.DictWriter(stream, row)
            writer.writeheader()
            for frame in range(120):
                row["frame"] = frame
                row["frame_ms"] = 9999 if frame < 20 or frame == 119 else 100
                writer.writerow(row)
    def analyze():
        return subprocess.check_output([sys.executable, str(script), str(root)], text=True)
    assert module.summarize(root / "native-a/performance.csv", 20)["mean_ms"] == 100
    assert "repeat/mode checks PASS" in analyze()
    path = root / "optimized-repeat/performance.csv"
    original = path.read_text()
    path.write_text(original.replace(",100,", ",106,"))
    assert "over 5% repeat drift" in analyze()
    path.write_text(original.replace(",depth,depth,", ",depth,legacy,"))
    assert "did not apply its expected mode" in analyze()
    path.write_text(original)
    (root / "native-b/performance.csv").unlink()
    assert "INVALID / incomplete" in analyze()
    (root / "native-a/performance.csv").write_text("frame,frame_ms\n0,\n")
    assert "invalid/incomplete capture" in analyze()
print("PASS: warm-up/screenshot exclusion; drift, fallback, missing and incomplete capture rejection")
