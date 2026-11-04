# R36S rendering optimizations — 2026-11-04

## Delivered behavior

The GLES launcher requests a **320×240 3D scene**, bilinearly composed onto a
640×480 display, followed by native-resolution HUD, menus, pause UI and debug
overlays. Headlight world shadows default to **fast (256×256, one sample)**.
The 500 m load / 450 m view preset remains intact. World shader and submission
order default to **legacy** pending a matched physical-device comparison.

Generated vehicle shadows are removed throughout the fork: no low-quality blob
quads, projected body/wheel geometry, caster buffers, shadow-only ground queries,
actor/reload ownership or shadow pipeline remain. The unused projection API and
rounded shadow-footprint shader path are removed. Neon, headlight illumination,
world-geometry headlight occlusion and authored lighting remain. The old
`g_CarShadowEnable` serialized value still round-trips, the graphics menu hides
its row, and `cpu_car_shadow` stays zero in CSV captures.

## Implementation

- `--resolution` controls display/window dimensions. GLES
  `--scene-resolution native|WIDTHxHEIGHT` controls raster dimensions only.
  Projection and screen-effect coordinates remain tied to the display aspect,
  including when the scene target has a different aspect. Auxiliary environment
  and headlight passes finish before the scene pass starts.
- `SceneTarget` owns a reusable RGBA8 texture, capability-gated D24 depth target,
  fullscreen composite program and vertex buffer. It reallocates attachments
  only when scene dimensions change. Native rendering bypasses it. Unsupported
  D24, allocation failure and framebuffer failure release partial resources,
  log a native fallback and never substitute D16. Shutdown releases the target.
  Final screenshot readback follows the complete display/UI composition.
- `--perf-pixel-scale` remains a diagnostic viewport override and is rejected
  with any non-native scene-resolution request. It is absent from the new
  production comparison.
- `--headlight-shadows reference|filtered|fast|off` selects 512/4, 256/4, 256/1,
  or unshadowed beams. Shader preprocessor variants remove the extra fast-mode
  samples. Map texel offsets and depth/normal bias derive from selected map size.
  Beam anchors, shape, brightness, range and high/low behavior are unchanged.
  Maps/depth persist across frames. Resource/FBO failure retains unshadowed beams.
- `--world-shader auto|legacy` selects a GLES program with its own parameter
  block for eligible dry opaque/cutout batches. Canonical frame/material state
  is copied into that block. Directional light, vertex prelight, textures, fog,
  alpha cutout and headlight lighting/shadows remain. Paint/glass/clear-coat,
  environment and wet/reflection calculations are compiled out. Unsupported
  material features and shader failure use the general program. The general
  pipeline is restored before deferred surfaces and subsequent effects/cars.
- `--world-order depth|legacy` uses a reusable visible opaque/cutout queue.
  In-place heapsort orders 32 m nearest-AABB-distance buckets, then compatible
  material/texture state, with original batch index as the final tie-breaker.
  It allocates only when capacity grows; libc qsort scratch allocation is avoided.
  Allocation failure retains original submission. Existing visibility tests and
  batches remain intact. Blend stays back-to-front; additive ordering is unchanged.
  Moved prop bounds are read again each frame. No spatial cells are merged.
- Effective program/texture and tracked raster-state changes come from the
  existing GL state tracker. Redundant texture binds reuse that tracker rather
  than introducing a separate binding cache. Vulkan reports these GL counters as
  zero. Scene/shader GLES options are rejected outside the GLES build; native
  desktop/Vulkan defaults remain native.

Launcher variables are `OPENUG2_SCENE_RESOLUTION`,
`OPENUG2_HEADLIGHT_SHADOWS`, `OPENUG2_WORLD_SHADER`, and
`OPENUG2_WORLD_ORDER`. Their R36S defaults are `320x240`, `fast`, `legacy`,
and `legacy` respectively. Explicit `auto`/`depth` options enable candidates.

## Measurement and evidence

CSV retains old columns and adds actual/requested scene dimensions, display
dimensions, requested/effective headlight/shader/order modes, actual map size and
sample count, caster draws, specialization fallback batches, queue building,
auxiliary-pass, composite and native HUD times, and effective backend changes.
Queue time is a subset of world work. Section times remain wall intervals and
can include driver waits; missing GPU queries remain unavailable.

The Performance entry runs the eight fresh 120-frame cases in PERFORMANCE.md.
Each case clones the same settings/profile snapshot into an isolated conf/save
directory. It preserves normal runtime logs/settings/saves and writes separate
entry/case logs, source-settings hashes, binary SHA-256 and screenshots. It stops
on failure. The analyzer excludes frames 0–19 and the last screenshot frame and
requires correct applied modes and complete captures. Either repeat drifting
over 5% invalidates performance attribution.

The first PC Mesa/llvmpipe eight-case run completed with all applied modes and
no shader fallbacks. Specialized versus legacy, depth versus legacy, optimized
repeat and native repeat screenshots were **pixel-identical** at 640×480 in the
fixed scene. Native repeat drift was +69.04%, optimized repeat +7.57%, while the
ARM build ran concurrently. These timing results are **invalid for attribution**;
no FPS gain is claimed from them. Raw evidence is retained under
`dist/render-optimization/native-comparison/perf-compare-6xx2q6/`.

The earlier physical R36S 5.29 → 9.51 FPS quarter-pixel result predates this
compositor/native-HUD implementation. It supports the target, not a production
FPS promise. The previous physical driving capture completed without OOM, but
its slow preparation and stale streaming targets remain unresolved separately.

The final PC run, after projection and case-isolation fixes, is retained under
`dist/render-optimization/native-comparison/perf-compare-KSz7oo/` with its analysis
and `native-qa.json`. All eight cases completed 120 frames; applied modes passed,
vehicle-shadow time and specialization fallbacks were zero, and the same four
image pairs remained pixel-identical. Native repeat drift was **+1.10%** and
optimized repeat drift **−3.75%**, passing the repeat gate. These remain PC
software-renderer results. In this run the specialized-shader case was faster,
while adding depth ordering was slower; neither changes the handheld defaults.

## Verification

PC tests use Linux Mesa; they establish renderer behavior, not Mali performance.

| Coverage | Result |
|---|---|
| GLES RGBA8/D24 composition and bilinear edge | PASS |
| Single native display-pixel HUD line after scaling; top-down screenshot orientation | PASS |
| Actual 24-bit attachment; overlapping surfaces separated below D16 precision | PASS |
| Attachment reuse, scene resize, display resize/viewport restoration, shutdown | PASS |
| Injected depth allocation/FBO failure, absent D24 capability, partial cleanup/native fallback | PASS |
| General/specialized opaque/cutout, texture/prelight/fog, high/low headlights, all profiles | PASS, at most one channel level; fixed real-data scene exact |
| Headlight wall occlusion, cutout fences, independent lamps, high/low cutoff and state | PASS, reference/filtered/fast |
| Queue visible-index set, 32 m buckets, material grouping, stable coplanar ties, changed bounds and storage reuse | PASS |
| Real-data shader/order parity at equal scene/shadow settings | PASS, pixel-identical fixed scene |
| Global vehicle-shadow removal and inert legacy serialization/menu integration | PASS, source invariant plus settings/menu regressions |
| Weather, scene-cube reflection, wet rendering, car/glass lighting and state restoration | PASS |
| RHI frame/pass/layout, world texture/instance/residency, HUD state/caller state, settings | PASS |
| Runtime launcher exit 0/7/137, workers, eight cases, early failure and normal log/config/save preservation | PASS |
| Analyzer warm-up/screenshot exclusion and rejection of drift, fallback, missing/incomplete cases | PASS |
| GLES and native desktop builds; real-data composed screenshots | PASS |
| Vulkan game build and validation-layer RHI smoke | PASS, zero validation messages |
| Physical handheld new-build visual/performance/normal driving acceptance | PENDING |

`make render-optimization-check` runs the new GLES fixture and Python invariants.
`make headlight-render-test` runs all three retained shadow profiles. Existing
regression targets continue to cover transparent/effect state. The historical
retail settings vectors remain unchanged; the menu tests adapt their expected
rows for this fork's removed vehicle-shadow option and test the shorter list's
scrolling/caret/last-row behavior explicitly.

Real-data smoke covers low-quality HUMMER at 320×240/fast, high-quality SKYLINE at
480×360/filtered with environment capture, native desktop, and a 400×240 target
with a different display aspect. Vehicle-shadow code is globally absent for
player, traffic/opponents and model reload paths. Arbitrary moving/coplanar city
scenes and sustained driving still require the returned physical run.

Build logs, test logs, captures, the old card binary/config and preserved device
evidence are under `dist/render-optimization/`. Deployment identity and final
card verification are recorded below once the final ARM build is staged.

## Physical acceptance to return

1. Run **OpenUG2 Performance** through all eight cases and return its fresh
   `perf-compare-*` directory plus `performance-entry-*.log`.
2. Run **NFSU2** normally: 30 seconds stationary, at least 90 seconds driving,
   then exit normally. Preserve `last-run.log`, `performance.csv`, `memory.csv`,
   `resource.log`, and `system-before.log`/`system-after.log` before another launch.
3. Check native HUD/pause sharpness, road/terrain depth, fences/headlight occlusion
   and high/low beams. Check for absent vehicle shadows on player and AI cars.

Shader/order defaults remain legacy until matched handheld comparisons improve
above repeat noise without material/transparency regressions. No streaming,
simulation timing, collision, AI, clocks or reclamation policy changes are part
of this rendering implementation.

## Final ARM/card identity

- AArch64 GLES build: PASS; highest required GLIBC symbol version 2.29.
- QEMU ARM startup/input-discovery smoke: exit 0 using SDL dummy drivers. This
  smoke does not execute the handheld GPU rendering path.
- Deployed binary SHA-256: `4F10439F7E5944F851401AD596ABA03F94C28E9CBAA9D4B1B38D440767D2B921`.
- Additive `deploy.ps1 -SkipData`: **11 payload files and four top-level menu
  entries** match staging by SHA-256. Text payloads have LF endings and no BOM.
- Card ACLs verified for the port, binary/config/launcher and all four menu
  entries. Game data was not recopied or deleted.
- Ten existing card evidence files and 49 returned comparison files match their
  pre-deploy hashes/backups. The previous binary and config are preserved under
  `dist/render-optimization/device-before/`.
- Machine-readable card verification: `dist/render-optimization/deployment-verification.json`.
- **Physical new-build result remains PENDING.** Return the controlled and
  driving evidence above before judging FPS, headlight/road-depth appearance,
  sustained driving, or enabling shader/order candidates by default.
