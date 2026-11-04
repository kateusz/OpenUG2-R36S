#!/usr/bin/env python3
"""Summarize engine CSV logs; no third-party dependencies or GPU-time guesses."""
import argparse
import csv
import math
from pathlib import Path
import re
import statistics


def summarize(path, warmup):
    with path.open(encoding="utf-8", errors="replace", newline="") as stream:
        # SIGKILL can leave an incomplete final row. Never turn blanks into zeros.
        rows = [r for r in csv.DictReader(stream) if None not in r and None not in r.values()]
    rows = rows[warmup:-1]  # final --shot frame includes screenshot/readback
    if not rows:
        raise ValueError(f"{path}: no complete rows after warm-up")

    def values(name):
        return [float(r[name]) for r in rows if r.get(name, "").strip()]

    def mean(name):
        data = values(name)
        return statistics.mean(data) if data else None

    frames = sorted(values("frame_ms"))
    modes = ("display_width", "display_height", "scene_width", "scene_height", "headlight_profile",
             "headlight_size", "headlight_samples", "world_shader_requested", "world_shader_effective",
             "world_order_requested", "world_order_effective")
    applied = {key: sorted({r[key] for r in rows if key in r}) for key in modes}
    result = {"complete_frames": len(rows) + warmup + 1, "applied": applied, "rows": len(rows), "mean_ms": statistics.mean(frames),
              "p95_ms": frames[math.ceil(.95 * len(frames)) - 1],
              "fps": 1000 / statistics.mean(frames)}
    for name in ("cpu_ai_step", "cpu_world_draw", "cpu_gpu_drain", "cpu_swap",
                 "process_cpu_ms", "gpu_total", "draws", "cpu_aux", "cpu_composite", "cpu_hud",
                 "cpu_queue", "headlight_caster_draws", "shader_fallbacks", "program_changes",
                 "texture_changes", "state_changes"):
        result[name] = mean(name)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path, help="comparison directory or one performance.csv")
    parser.add_argument("--warmup", type=int, default=20)
    args = parser.parse_args()
    if args.warmup < 1:
        parser.error("--warmup must be at least 1 (first CPU row includes setup)")
    if args.path.is_file():
        paths = [(args.path.stem, args.path)]
    else:
        names = ("native-a", "scene-480", "scene-320", "headlights", "world-shader", "depth", "optimized-repeat", "native-b")
        if not (args.path / "native-a").exists():
            names = ("full-a", "quarter", "full-b", "no-vsync", "finish") # previous diagnostic captures
        paths = [(name, args.path / name / "performance.csv") for name in names]
    print("case          rows    FPS   frame_ms  p95_ms  AI_ms  draw_ms  drain_ms  swap_ms  GPU_ms")
    summaries = {}
    for name, path in paths:
        if not path.exists():
            print(f"{name}: missing {path}")
            continue
        try:
            stats = summarize(path, args.warmup)
        except (ValueError, KeyError, TypeError) as error:
            print(f"{name}: invalid/incomplete capture: {error}")
            continue
        summaries[name] = stats
        def number(key):
            value = stats.get(key)
            return f"{value:8.2f}" if value is not None else "     n/a"
        print(f"{name:13} {stats['rows']:4d} {stats['fps']:6.2f} " +
              " ".join(number(k) for k in ("mean_ms", "p95_ms", "cpu_ai_step",
                                           "cpu_world_draw", "cpu_gpu_drain", "cpu_swap", "gpu_total")))
        if stats["applied"]["scene_width"]:
            print("  applied: " + "; ".join(f"{k}={','.join(v)}" for k,v in stats["applied"].items()))
            print("  composite/HUD/queue ms: " + "/".join(f"{stats[k]:.3f}" for k in ("cpu_composite", "cpu_hud", "cpu_queue")))
            print(f"  headlight caster draws={stats['headlight_caster_draws']}; shader fallback batches={stats['shader_fallbacks']}")
        engine_log = path.parent / "engine.log"
        if engine_log.exists():
            match = re.search(r"presentation: requested-vsync=(\d+) actual-swap-interval=(-?\d+)",
                              engine_log.read_text(encoding="utf-8", errors="replace"))
            if match:
                print(f"  swap interval requested={match[1]}, actual={match[2]}; mean draws={stats['draws']}")
                if match[1] != match[2]:
                    print("  Requested vsync was not applied; this case cannot establish a vsync effect.")
    valid = len(summaries) == len(paths)
    for first, repeat in (("native-a","native-b"),("depth","optimized-repeat"),("full-a","full-b")):
        if first in summaries and repeat in summaries:
            a,b=summaries[first]["mean_ms"],summaries[repeat]["mean_ms"]
            drift=100*(b/a-1)
            print(f"{first} -> {repeat} repeat drift: {drift:+.2f}%")
            if abs(drift)>5:
                valid=False
                print("INVALID attribution: over 5% repeat drift; inspect clocks, temperature and background work.")
    if len(paths)==8:
        expected = {
            "native-a":("640","480","reference","512","4","legacy","legacy"),
            "scene-480":("480","360","reference","512","4","legacy","legacy"),
            "scene-320":("320","240","reference","512","4","legacy","legacy"),
            "headlights":("320","240","fast","256","1","legacy","legacy"),
            "world-shader":("320","240","fast","256","1","auto","legacy"),
            "depth":("320","240","fast","256","1","auto","depth")}
        expected["native-b"]=expected["native-a"];expected["optimized-repeat"]=expected["depth"]
        keys=("scene_width","scene_height","headlight_profile","headlight_size","headlight_samples","world_shader_effective","world_order_effective")
        for name,stats in summaries.items():
            modes=stats["applied"]
            if stats["complete_frames"]!=120 or any(modes[k]!=[v] for k,v in zip(keys,expected[name])):
                valid=False;print(f"INVALID attribution: {name} did not apply its expected mode or complete exactly 120 frames.")
        print("Eight-case attribution: " + ("repeat/mode checks PASS (visual correctness still required)" if valid else "INVALID / incomplete"))
    print("Section times are wall times. drain_ms includes rendering completion/driver waits, not pure GPU time.")
    print("Compare adjacent fresh cases only after repeat, applied-mode and visual checks pass.")
    print("Use actual swap intervals for the no-vsync comparison. Missing GPU timings remain n/a.")


if __name__ == "__main__":
    main()
