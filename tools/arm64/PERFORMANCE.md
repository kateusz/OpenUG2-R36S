# R36S performance capture and controlled comparison

The previous physical build launches, accepts controls and completed a driving
capture without an OOM kill. Its five-case comparison established pixel-sensitive
rendering (5.29 FPS native, 9.51 FPS at quarter pixel area). Those measurements
predate the production compositor and native HUD. The new rendering build needs
its own returned handheld comparison and driving capture; do not transfer those
FPS figures to it. Streaming preparation/stale targets remain a separate task.

## 1. Verify normal driving first

Start **NFSU2** normally with the 640x480 display / 320x240 scene, fast headlights, 500 m load / 450 m draw preset.
After loading, stay stationary for about 30 seconds, drive for at least 90 seconds
through several city blocks, and exit normally. If it closes early, preserve
logs before launching it again. On the card, collect from `F:/ports/OpenUG2/`
(device `/roms/ports/OpenUG2/`):

- `memory.csv`: flushed phase snapshots around prototype inventory, placement,
  preparation, texture binding, GPU batch upload and activation. Linux RSS,
  anonymous/file RSS, MemAvailable and explicit owner estimates identify the
  stage; geometry/source/texture fields are partial owner estimates, not a
  complete allocation ledger. Zero means not supplied at that phase. Texture
  estimates assume an RGBA full mip chain and exclude driver overhead.
- `performance.csv`: completed-frame wall time, process CPU time, section
  timings, scene/contact counts and optional GPU queries.
- `resource.log`: external memory/OOM sampling, raw per-core/process CPU
  counters, clocks, available GPU load sensors and temperatures.
- `last-run.log`, `system-before.log`, `system-after.log`: engine/streaming
  events, exit status and kernel evidence. Engine output is line-buffered when
  memory logging is enabled, so completed phase messages survive a later kill.

`OPENUG2_PERF_MONITOR=1` enables frame and memory logs. The resource monitor is
independent (`OPENUG2_RESOURCE_MONITOR=1`). Frame CSV userspace buffers flush
once per second; memory phase files close after each snapshot; the launcher
syncs logs about every three seconds. A power cut can still lose the latest
writes. Keep profiling enabled for both sides of any comparison.

State falls back to the home directory if the port is unwritable. The actual
path is printed in `last-run.log`; only that log is copied back to the card on
fallback. Normal logs are replaced on the next normal launch.

## 2. Run the production rendering comparison

Select **OpenUG2 Performance** in Ports. No input is needed. It runs eight fresh
120-frame processes with the same STREAML4RA spawn, car, heading, camera, settings
and deterministic frame-indexed simulation. Each case clones the same starting
settings/profile into its own writable conf/save directories. Player pose stays fixed; AI advances
one tick/frame. Every case removes generated vehicle shadows. HUD/menu/UI and
screenshots use the full display (default 640x480).

| Case | Scene | Headlight shadows | World shader | World order |
|---|---|---|---|---|
| native-a | native | reference 512 / 4 | legacy | legacy |
| scene-480 | 480x360 | reference 512 / 4 | legacy | legacy |
| scene-320 | 320x240 | reference 512 / 4 | legacy | legacy |
| headlights | 320x240 | fast 256 / 1 | legacy | legacy |
| world-shader | 320x240 | fast 256 / 1 | auto | legacy |
| depth | 320x240 | fast 256 / 1 | auto | depth |
| optimized-repeat | 320x240 | fast 256 / 1 | auto | depth |
| native-b | native | reference 512 / 4 | legacy | legacy |

Scaling uses a reusable RGBA8/D24 target and bilinear composition, followed by
native HUD/UI. `--perf-pixel-scale` is an older diagnostic viewport override;
combining it with a non-native scene request is rejected. Desktop/Vulkan remain
native. The R36S launcher defaults to 320x240 / fast / legacy shader / legacy
order. `OPENUG2_SCENE_RESOLUTION`, `OPENUG2_HEADLIGHT_SHADOWS`,
`OPENUG2_WORLD_SHADER` and `OPENUG2_WORLD_ORDER` in config.txt select the modes.
Shader/order candidates remain opt-in until handheld correctness and improvement
above repeat noise pass. `filtered` headlights use 256x256 with four samples;
`off` retains unshadowed beam illumination as a diagnostic.

All cases request swap interval 1 and record the actual driver value. Custom
OPENUG2_ARGS are excluded from the comparison to keep camera/event/drive inputs
fixed. Results go into a fresh `perf-compare-XXXXXX/` directory, with a binary
SHA-256 manifest, individual engine/frame/memory/resource/kernel logs and complete
640x480 screenshots. The entry preserves normal runtime logs, settings and saves byte-for-byte and
uses a separate `performance-entry-XXXXXX.log`. A failed case stops the sequence
and preserves the exit status and evidence.

```powershell
python tools/arm64/analyze-performance.py F:/ports/OpenUG2/perf-compare-XXXXXX
```

The analyzer excludes the first 20 frames and final screenshot frame (99 timed
rows per complete case), checks actual settings, and rejects attribution when
either native or optimized repeat drift exceeds **5%**. Missing cases, failed
settings or incomplete captures also invalidate the eight-case result. Inspect
clocks, thermals and background work before repeating. A passing timing gate
still requires visual correctness; compare specialized versus legacy at equal
scene resolution/shadow quality, and check opacity/transparency after reordering.

New CSV fields record display/requested/actual scene dimensions, requested and
effective shader/order/headlight modes, map size/sample count, caster draws,
specialization fallback batches, queue building, auxiliary passes, composite and
native HUD times. `cpu_car_shadow` remains zero for old tooling. Program/texture
counters count effective backend changes; `state_changes` covers tracked enable,
depth, blend-factor and color-mask mutations. Queue time is a subset of world
submission time, not an additional amount to sum into frame time.

## Interpretation

- `frame_ms`: loop wall time through presentation, before most CSV work. Segment
  FPS is 1000 / mean(frame_ms). Discard the first CPU-section row: initialization
  is charged to cpu_top/cpu_total there. Inspect streaming spikes separately.
- `process_cpu_ms`: C clock(), summed across process threads. A worker can make
  it exceed wall time. It is not main-thread CPU time.
- `cpu_*`: sequential wall intervals, including any driver waits. AI and drawing
  counts must remain comparable; wall_candidates is accumulated broad-phase
  work, not a count of triangle narrow-phase tests.
- `cpu_gpu_drain`: completion/driver wait introduced by the finish case. If swap
  time moves here, swap had been waiting for submitted rendering. This is not
  pure GPU execution time, and serialization can change total frame time.
- Quarter-pixel improvement with matching draws supports sensitivity to pixel
  work or bandwidth. Little scaling does not exclude vertex/geometry cost.
- Faster no-vsync with actual interval 0 versus 1 supports display-sync cost.
  Sensors with different sampling windows cannot replace GPU timings.
- GPU columns describe an earlier row identified by gpu_lag_frame. The existing
  EXT_timer_query path remains optional; EXT_disjoint_timer_query alone is not
  enabled. Blank cells are unavailable/not ready, never zero GPU work.

Native Mesa results validate the mechanism, not Mali/R36S attribution. Record
handheld clocks, temperatures, available memory and repeat drift before deciding
on another optimization. See the change report for build, test and deployment
results.

## Returned physical run - 2026-11-04

The user confirms normal gameplay and higher FPS. Logs show exit 0, no kernel
OOM messages and 6.86 FPS over the saved gameplay rows (previous capture 3.50).
This is not a controlled same-route comparison. No replacement neighborhood
activated: preparations took 21-28 seconds and completed targets became stale.
Physical streaming remains unresolved; Performance comparison has not run yet.
See [PERFORMANCE-RETURN-20261104.md](PERFORMANCE-RETURN-20261104.md).

## Returned physical comparison - 2026-11-04

The completed five-case R36S comparison shows 5.29 FPS full versus 9.51 FPS at
quarter pixel area, matching full-size images/draw counts and negligible repeat
drift. Roughly 130 ms of swap wait moves to explicit rendering completion.
The driver refused vsync-off (actual interval remains 1). Pixel-sensitive
rendering is now strongly implicated in this stationary scene; streaming is
still a separate open issue. See PERFORMANCE-COMPARISON-20261104.md.

## Rendering candidate implementation

See [RENDERING-OPTIMIZATIONS-20261104.md](RENDERING-OPTIMIZATIONS-20261104.md)
for implementation, regression coverage, build/deployment identity and the
remaining physical-device acceptance. Old five-case captures remain readable by
the analyzer and are evidence for the previous build only.
