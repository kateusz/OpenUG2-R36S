# Physical R36S render/presentation comparison - 2026-11-04

Completed capture: perf-compare-wBBLPe. All five cases exit 0. Binary SHA256:
213532EADAA62DF9C86FED53E5EF7F9C1C666E7E43C1546C494C513AB6428B7F.
Both card capture directories and launcher.log were copied to
`dist/transition-fix/device-comparisons-20261104/` before analysis.
The earlier perf-compare-gjfCBW contains full-a only, followed by a quarter
startup log and zero frame rows; it is incomplete and excluded from attribution.
The available evidence does not establish why that earlier attempt stopped.

## Validity

Each completed case has 120 rows; first 20 and last screenshot row excluded,
leaving 99 rows/case. Every case averages 647 draws and about 11.6 ms AI wall
time. Full-a/full-b differ by about 0.04%; all four full-size screenshots have
identical SHA256 507105bfabaf129d5b863259468b57bfeb106dbe9a56c2f20c77c0419311f1c5.
Full and quarter screenshots were visually inspected; both show the complete
same scene at their respective dimensions. Display remains 640x480 throughout.

| Case | FPS | Frame ms | AI ms | World draw ms | Drain ms | Swap ms |
|---|---:|---:|---:|---:|---:|---:|
| full-a | 5.29 | 189.18 | 11.61 | 27.81 | 0 | 130.91 |
| quarter pixels | 9.51 | 105.10 | 11.77 | 28.10 | 0 | 45.98 |
| full-b | 5.29 | 189.11 | 11.60 | 27.51 | 0 | 131.08 |
| no-vsync request | 5.29 | 188.95 | 11.55 | 27.94 | 0 | 130.84 |
| finish before swap | 5.27 | 189.68 | 11.57 | 28.20 | 130.16 | 0.87 |

## Attribution

Quarter pixel area reduces mean frame wall time by about 44% and raises
throughput about 80%, with essentially unchanged AI/draw-submission sections.
This strongly supports pixel-sensitive rendering/fill/shading or bandwidth
cost in this scene. It does not identify one shader/pass or exclude other CPU
and vertex costs in other scenes.

The finish case moves almost the entire 131 ms swap wait into explicit GL
completion, with total time nearly unchanged. Therefore swap was largely
waiting for submitted rendering/driver completion. This is not a direct GPU
execution timer, but combined with pixel scaling it strongly supports rendering
backpressure rather than treating the entire swap interval as display idle.

The no-vsync case requests 0 but logs actual-swap-interval=1. It is not a valid
vsync-off comparison. Do not conclude that disabling vsync has no benefit, or
that display waits are absent. GPU timer cells remain unavailable.

CPU clocks stay 1,296,000 kHz and GPU clocks 520,000,000 Hz in sampled data.
Maximum thermal reading is 61.7 C. No sampled frequency drop is present.
Minimum MemAvailable is 314,348 KiB (307 MiB); kernel snapshots contain no OOM
messages. This stationary comparison does not exercise background transitions.

## Next work supported by these results

The next graphics experiment should be an internal render-resolution scale,
with a full-display upscale and native-resolution HUD, measured in this same
scene. The present quarter case has no upscale and is not itself a ready normal
play preset or a guaranteed gameplay FPS result. Pass-level shading/overdraw
work can follow measurement of the remaining pixel cost.

Independently, the normal driving capture exposed 21-28 second preparation and
obsolete results without resident activation. Streaming throughput/coverage
remains a separate outstanding issue; this stationary benchmark cannot close it.
No engine, preset or device settings were changed during this log inspection.
