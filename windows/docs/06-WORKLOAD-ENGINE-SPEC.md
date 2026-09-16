# Workload engine

Domain request → SafetyManager admission → controller → DiskSpd adapter → process →
bounded XML parser → normalized run summaries. The UI never supplies executable,
shell flags, raw paths or device names. Agent owns one regular scratch target per run.
Pin/version/hash the installed DiskSpd executable and preserve exact argv in metadata.

Five controls: read percentage, discrete block bytes, total configured outstanding
limit 1–32, working set 64MiB–4GiB, intensity. Sequential/random is a separate toggle.
Initial implementation uses ONE worker and ONE file: DiskSpd -o therefore equals
configured total outstanding limit. Intensity limits offered bandwidth to
32/128/256 MiB/s for light/moderate/high, with exact rate in the start summary.
It does not silently multiply queue depth. Runtime target QD is immutable; changes
produce another phase after stop/settle. Configured QD is a cap, not observed occupancy.

Default measured duration 15s, maximum 60s, explicit warmup 0–5s and cooldown 0–5s
included in resource budgeting. Latency collection and XML results required.
Buffered uses explicit caching; unbuffered uses software-cache bypass only.
Do not use -Sh for a cache-only comparison because it also changes write-through.
Do not expose arbitrary extra flags. Alignment and device support checked before start.

| Preset | Pattern | Read % | Block | QD | Size | Intensity | Watch |
|---|---|---:|---|---:|---|---|---|
| Database random read | random | 100 | 4KiB | 8 | 512MiB | moderate | IOPS / latency |
| Streaming / large file | sequential | 100 | 1MiB | 4 | 1GiB | moderate | bandwidth |
| Mixed application | random | 80 | 64KiB | 4 | 512MiB | light | read/write and queue |
| AI model loading | staged GPU engine | 100 | 1MiB | 1 | 256MiB | light | phase durations |
| Sustained write | sequential | 0 | 256KiB | 4 | 1GiB | moderate | trend / sensor coverage |

AI preset delegates to the pipeline engine, never disguises DiskSpd as GPU loading.
File creation/initialization is a recorded preparation phase and consumes write budget.
Final DiskSpd percentiles are only available after successful parse; live p95 remains
Unavailable without an instrumented latency source. Malformed/truncated output yields
failed run with preserved diagnostic artifact, not zero-valued results.
