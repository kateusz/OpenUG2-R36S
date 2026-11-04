# R36S returned normal run - 2026-11-04

User reports that normal NFSU2 works correctly and FPS increased. The controlled
OpenUG2 Performance comparison has not been run. The card binary matches final
213532EADAA62DF9C86FED53E5EF7F9C1C666E7E43C1546C494C513AB6428B7F.

Raw evidence was preserved at `dist/transition-fix/device-return-20261104-181335/` before analysis.
No engine, preset or card files were changed during this inspection.

## Observed result

- Engine exit 0, with a pause event before shutdown. External sampling spans
  uptime 305.34 to 442.68 (137.34 seconds including loading/exit).
- No OOM-killer, Out of memory or Killed process messages occur in either
  kernel snapshot. Swap remains disabled. Minimum sampled MemAvailable is
  122,716 KiB (119.8 MiB), versus 20.4 MiB before the previous kill.
- Process HWM is 789,100 KiB (770.6 MiB). Mapped archive aliases can count shared
  physical pages more than once in process RSS; use available memory and kernel
  evidence too. This observed run survived; universal/long-run stability is
  not established.
- 648 complete frame rows, with the first setup-contaminated CPU row excluded:
  647 rows cover 94.33 seconds, averaging 6.86 FPS (145.79 ms/frame), median
  132.35 ms and p95 264.84 ms. The earlier capture averaged 3.50 FPS. This is
  about 96% higher observed throughput, not a controlled same-route benchmark.

| Mean section wall time | Previous capture | This capture |
|---|---:|---:|
| AI simulation | 101.26 ms | 34.85 ms |
| World drawing | 50.25 ms | 28.32 ms |
| Presentation/swap | 103.18 ms | 48.85 ms |

Section intervals include waits. GPU timers remain unavailable; a GPU-only
bottleneck is not established. Maximum thermal sensor sample is 73.3 C; this
alone does not establish thermal throttling.

## Streaming limitation exposed by this drive

No `resident activated gen=` event occurs. Three background preparations run:
366.77 -> 394.29 (27.52 s), 394.54 -> 415.48 (20.94 s), and
416.91 -> 438.16 (21.25 s). The first two completed CPU results are explicitly
discarded as obsolete targets. The last reaches texture binding near shutdown,
but no activation is logged. The car travels to approximately (-857,158).

Therefore this run confirms normal play/exit and improved observed FPS, but
not successful handheld replacement-neighborhood activation. Global CPU ground
still supports queries, while the initial GPU resident has a finite envelope.
Longer travel can outrun that resident. Next streaming work should measure and
reduce preparation cost and review obsolete-target acceptance against actual
coverage; do not activate an uncovered candidate merely to suppress discards.

## Next evidence

The Performance entry remains useful to separate pixel work from completion
and display waits. Normal logs are now safely archived. Run that entry without
input and return its perf-compare directory. Keep successful gameplay acceptance
separate from the unresolved streaming and GPU/presentation attribution above.
