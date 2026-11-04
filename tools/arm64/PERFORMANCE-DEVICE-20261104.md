# R36S driving crash and first performance capture - 2026-11-04

## Confirmed result

The instrumented C82C50DF build launches and accepts controls, but this physical
run was killed by the kernel out-of-memory handler while driving. Startup/input
acceptance remains valid; sustained driving is failing. No device or graphics
settings were changed during this investigation.

Evidence was copied off the card before analysis to:
`dist/performance-check/device-crash-20261104-142743/`.
That directory holds the seven raw logs/config plus derived `analysis.json`.
The card binary SHA256 matches the deployed instrumented build:
`C82C50DF534337129190AF5E45BB07497962386B246E249B8897C25A501EC00F`.
The handheld clock reports 2024-09-02; uptime is used for the timeline.

## Crash evidence

`system-after.log` lines 79, 158-159:

```
[166.683225] world-prepare invoked oom-killer
[166.684145] Out of memory: Kill process 12745 (nfsu2)
[166.684907] Killed process 12745 (nfsu2) total-vm:1384168kB, anon-rss:501448kB, file-rss:267340kB
```

`last-run.log` confirms exit 137/SIGKILL. The victim's kernel-reported RSS is
768,788 KiB (750.8 MiB), not the 1.32 GiB virtual address-space size. Usable
system RAM is 919,104 KiB (897.6 MiB); swap is disabled. The last live sampler
at uptime 166.11 has 20,900 KiB (20.4 MiB) MemAvailable and 749,940 KiB RSS.
Sampling missed the exact allocation peak. Kernel free RAM at OOM was only
3,692 KiB. Memory recovered immediately after the process died.

Selected memory samples (KiB):

| Uptime seconds | Process RSS | System MemAvailable |
|---|---:|---:|
| 112.65 | 625,548 | 168,000 |
| 136.71 | 626,024 | 168,072 |
| 148.75 | 627,256 | 167,356 |
| 162.91 | 740,972 | 37,332 |
| 166.11 | 749,940 | 20,900 |

The stable playable RSS was about 611 MiB. It rose by about 121 MiB before the
last live sample. This is a streaming/allocation-peak lead, not proof of a
continuous leak or a complete per-resource allocation attribution.

Source matches the failing thread: `resident_worker` in `src/world_resident.c`
calls `world_resident_prepare`, which loads a detached CPU neighborhood via
`world_neighborhood_load`. `src/main.c` retains the current active resident
while the next neighborhood is prepared and uploaded, then retires the old
owner after activation. This overlap is the first memory path to profile and
reduce. There is no successful resident activation line in this capture, but
stdout was block-buffered, so its absence does not establish the exact stage.

## Performance evidence before the kill

217 complete CSV rows survived, spanning 62.28 seconds of completed loop work.
The first row has setup charged to the existing cpu_top/cpu_total and is
excluded from the following averages. The saved rows do not cover any final
incomplete frame or necessarily all frames completed before SIGKILL.

| Metric | Value |
|---|---:|
| Average FPS (216 rows) | 3.50 |
| Mean frame wall time | 285.32 ms |
| Median frame wall time | 258.80 ms |
| p95 frame wall time (nearest rank) | 433.53 ms |
| Mean process CPU time, summed over threads | 218.06 ms |
| Mean AI simulation section wall time | 101.26 ms |
| Mean world draw section wall time | 50.25 ms |
| Mean presentation/swap wall time | 103.18 ms |
| Mean player/input/world section wall time | 7.03 ms |

GPU timers were unavailable. During the approximate gameplay interval the
sysfs GPU load field averaged about 36%, with a maximum of 47%; the separate
instantaneous utilisation field varied more widely, up to 97%. These kernel
sensors have different sampling windows and cannot replace GPU timing. CPU
frequency samples stayed at 1,296,000 kHz, GPU at 520,000,000 Hz. Thermal samples
peaked at about 60.4 C; no frequency drop is visible in this sample set.

There is substantial CPU-side AI work, with wall_candidates usually 296,208
per frame and eight contact passes. That count is accumulated candidate work,
not proof that all candidates reached narrow-phase triangle tests. Presentation
is also expensive and may include GPU completion, display waits or memory
pressure. A GPU-only bottleneck is not established, and reducing resolution
alone will not resolve the confirmed OOM crash.

## Recommended order

1. Instrument and bound the peak of active + prepared + retired neighborhoods,
   parser scratch, decoded textures and GPU allocations. Reproduce a resident
   transition under a handheld-sized memory budget. Do not disable streaming
   or drop the active collision world as a purported fix.
2. After driving survives transitions, target the measured AI step cost,
   especially broad-phase wall/ground queries and repeated per-tick work.
3. Separate world draw submission from GPU/presentation waits using supported
   GPU timing or a controlled same-scene resolution comparison.

This investigation changes documentation only. No engine fix, reduced-radius
preset, swap configuration or new card deployment has been applied.
