# R36S transition memory, CPU queries and presentation comparison

This is the implementation follow-up to
[the returned OOM capture](PERFORMANCE-DEVICE-20261104.md).
Startup and controls have physical acceptance. Sustained driving on this new
candidate still requires a handheld run; native tests and deployment are
separate evidence. The 640x480 / 500 m load / 450 m draw preset is unchanged.

## Transition ownership and memory

The original native GLES/Mesa reproduction peaked at 1,164,588 KiB RSS during
two resident transitions. With a 700 MiB container cap and swap disabled it was
OOM-killed. The handheld's earlier kill was in the CPU preparation thread;
longer native driving also exposed upload and post-retirement peaks.

The final implementation addresses each measured source:

- Read-only STREAM, LOC4 and shared archives use file mappings with explicit
  mapping ownership. The region and instance parsers share reclaimable file
  pages instead of keeping two 113 MiB anonymous archive copies.
- The instance library inventories validated prototype identities, then keeps
  at most 16 MiB of reusable decoded geometry plus one newly decoded object.
  Placement still follows source order and owns its copied vertices. The
  existing decoder validates each inventory entry so malformed empty models
  cannot preempt a valid authored LOD fallback. Prototype vertex storage had
  previously reached 294,425,444 bytes (280.8 MiB).
- A new preparation drains the retired owner first. Synchronous placement
  joins an outstanding background loader and can reuse a completed matching
  candidate. This enforces active + candidate ownership rather than allowing
  active + retired + another preparation.
- Texture bindings hold references, including successfully acquired prefixes
  of a failed/partial build. Live residents share their images; unreferenced
  decoded image estimates are capped at 32 MiB and trimmed before another
  preparation. Cache hash entries survive eviction so probing stays valid.
- Ordinary ground GPU uploads retain whole chunks intersecting a 2x-resident-
  radius envelope (1000 m in this preset). Global CPU ground is retained: it
  also supplies city navigation labels and collision queries. The global batch
  grid origin and texture resolution order stay unchanged. The second route
  transition has 2,584 ordinary GPU batches versus the original 4,007.
- Retired mesh cleanup releases custom wall arrays too. At completed resident
  disposal, glibc returns freed arena pages with malloc_trim(0). Linux phase
  logs demonstrate that logical frees alone retained substantial RSS: one
  completed retirement fell from 661,056 to 498,852 KiB immediately after trim.
  This happens at disposal boundaries, not every frame, and is a no-op on
  other allocators/platforms. It can add a small retirement hitch.

The bounds concern these owners and reusable caches; they are not a global
hard limit on every allocation or a proof that every city/vehicle combination
fits the handheld. GPU-driver allocation overhead is not directly measured.

## Measured CPU work

The 1,800-frame native profile made about 274 million wg_tri_far calls.
Repeated ground queries in AI planning and tyre/support simulation were
recomputing triangle bounds. The resident ground grid now owns conservative
XY bounds for source-ordered groups of 16 triangles. Far groups are skipped
before the existing per-triangle bounds, barycentric, layer and wheel-support
calculations. Moving props bypass this static acceleration; allocation failure
falls back to the existing query path.

Matching non-profiled native builds, the same settings/scene, 1,800 deterministic
frames, excluding 60 warm-up frames and the final screenshot:

| AI section mean wall time | Before | After |
|---|---:|---:|
| cpu_ai_step | 1.8514 ms | 0.9070 ms |

This is a 51.0% reduction in the native AI section. The resulting screenshot
SHA256 is identical on both builds. This is not a measured R36S FPS gain.
The synthetic accelerated-versus-linear test checks 6,000 queries including
stacked layers, slopes, tolerance edges and partial final blocks, with exact
selected mesh/triangle/height/normal agreement.

## Controlled rendering/presentation experiment

The **OpenUG2 Performance** Ports entry runs full-a, quarter pixel area,
full-b, no-vsync and finish-before-swap. The player/camera and frame-indexed
simulation are reproducible. Each case has its own logs and screenshot in a
fresh perf-compare-XXXXXX directory. The comparison uses visible windows,
including on KMSDRM, and retains the display mode. It logs the actual applied
swap interval; refused settings cannot establish a vsync effect.

The finish case measures a separate cpu_gpu_drain interval after rendering
and before swap. That drains submitted GL work and serializes execution; it
measures remaining completion/driver waits, not pure hardware GPU time.
GPU timer availability is unchanged, with unsupported samples left blank.
See [PERFORMANCE.md](PERFORMANCE.md) for collection and interpretation and
the standard-library-only analyze-performance.py helper. A frame/readback
regression verifies that the half-size render reads the lower-left region
and that a subsequent normal frame restores the physical drawable size.
The full and quarter screenshots were visually inspected after that check.

## Validation and deployment

Final build, capped driving, regression and deployment checks passed.
Raw evidence is retained under
`dist/transition-fix/`; original handheld crash evidence remains under
`dist/performance-check/device-crash-20261104-142743/`.

- Native Linux GLES/Mesa software rendering, forced CPU DXT decode, container
  memory.max=734003200 (700 MiB), memory.swap.max=0: the two-transition route
  exits 0, and all five screenshots match the original baseline byte for byte.
  The 6,000-frame drive then completes three background resident swaps and
  exits 0. All three swaps have contact mask 0xf, inv_batch=0 and inv_src=0.
  OOM and OOM-kill counters remain 6 and 4 before/after (earlier failed
  experiments account for those existing counts). This is a bounded driving
  stress capture, not completion of the full RDA route/control audit.
- Process peak RSS is 790,872 KiB in that drive; read-only aliases of the same
  file pages can be counted more than once in process RSS. The container
  physically charges those shared pages once and enforces the 700 MiB cap.
  Do not report the RSS peak as below 700 MiB or as a handheld measurement.
- Native world-texture, world-resident, world-instance, world-collision,
  ai-drive, ground-motion, fence-impact, resource-map and rhi-frame targets
  pass. The renderer-boundary check passes. This covers partial/cancelled
  texture bindings, protected shared images, local ground GPU buffers versus
  unchanged CPU geometry, loader/ground ownership and the exact query tests.
- Launcher tests pass for normal exit 0, failure 7 and real SIGKILL/137,
  including sampler/flush-worker cleanup. Comparison tests verify all five
  cases, separate logs and stopping after a failed case.
- The final visible-window native comparison completes all five 120-frame
  cases. Every case reports 647 draws/frame, and all four full-size screenshots
  have the same SHA256. Full-a/full-b means are 23.14/23.46 ms (1.4% drift).
  Quarter area is 17.86 ms. The finish case moves 8.85 ms into cpu_gpu_drain,
  leaving 0.21 ms in swap. This validates the comparison mechanism under Mesa;
  it does not establish Mali hardware timings or an R36S bottleneck.
- The final ARM64 GLES build passes with maximum required GLIBC 2.29. Its
  --input-info smoke runs successfully under ARM emulation with dummy SDL
  (SDL 2.0.10, no joysticks). This is not a rendered ARM/device test.
- Deployed to F:/ports/OpenUG2. All 11 payload files and four top-level menu
  entries match the staged hashes; shell/config files have LF and no BOM.
  Everyone full-control ACLs were applied for the NTFS card. The six existing
  returned log files remain byte-identical, with another backup in
  dist/transition-fix/device-before-final-retry-*/.

Final ARM binary SHA256:
`213532EADAA62DF9C86FED53E5EF7F9C1C666E7E43C1546C494C513AB6428B7F`.

Relevant evidence: final-route.*, final-drive.*, final-700-before.events,
final-700-after.events, final-regressions.log, profile-frame-test.log, launcher-tests.log,
comparison/perf-compare-ykC1BM/, arm-input-info.log and
deployment-verification.json under dist/transition-fix/.


## Handheld acceptance still required

1. Start NFSU2 normally, wait stationary for about 30 seconds, drive through
   several blocks for at least 90 seconds and exit. Copy the normal logs first.
2. If driving survives, start OpenUG2 Performance and allow all five cases to
   finish without input. Copy the new comparison directory as well.
3. Compare the returned memory/OOM evidence, AI section, pixel-area sensitivity,
   actual swap intervals and wait migration before choosing further changes.

Do not mark the original R36S OOM resolved or attribute its remaining low FPS
to a GPU-only bottleneck until that new physical evidence is available.

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
