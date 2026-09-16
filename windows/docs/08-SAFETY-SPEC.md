# Safety manager

Native authority: validate → reserve → prepare → monitor → throttle/abort → cleanup.
No workload may launch before admission; UI validation is advisory.

| Resource | Current policy (revised 2026-09-08) |
|---|---|
| Disk | leave 2GiB free AFTER the test file plus 64MiB artifact/journal allowance |
| Test file | 64MiB–min(4GiB, current free minus reserve) |
| RAM | leave 2GiB available after planned buffers |
| VRAM | leave max(1GiB, 20% total) free; dataset <=512MiB |
| Queue / workers | total configured outstanding <=32; one worker/target initially |
| Duration | measurement <=60s; warmup/cooldown <=5s each; experiment <=600s |
| Writes | preparation + worst-case offered-rate × active time <=8GiB per experiment |
| Missing thermal coverage | light intensity and <=15s measurement per phase; disclose limitation |

The user rejected the original capacity-percentage reserve on 2026-09-08. The
current policy scales allocation with the actual target and buffers, with fixed
2GiB spare-space floors for disk and RAM. Spare space is not allocated by IOscope.
A default 64MiB, five-second read trial needs 128MiB maximum additional disk space
(including recording allowance) plus the 2GiB free-space floor: 2.125GiB free in total.
It writes the 64MiB file during preparation; measurement is read-only. No file grows
with total drive capacity. Larger offered traffic rewrites the same bounded file.
Policies are application defaults, not manufacturer guarantees.
CPU warning/abort 85/90°C, GPU 80/85°C, SSD 65/70°C if valid sensors exist; use a lower
reliably discovered vendor limit. Abort on one valid threshold-crossing sample.
At warning, prevent subsequent phase and stop current workload if engine cannot safely
adjust rate; do not claim dynamic throttling support from DiskSpd. Cooldown requires
10 consecutive seconds below warning minus 5°C on sensors that triggered it.
Once a sensor used for admission goes stale >3s, abort; never lose protection silently.
Missing temperatures at admission use the restricted policy, not invented readings.

Agent creates scratch under per-user local application data, never repository/OneDrive.
Reject UNC, raw volumes, alternate streams, junctions/reparse ancestors and existing
unowned files. Resolve and validate final opened handle location; admission requires
same-user ACL on scratch parent to reduce path races. Exclusive per-run ownership manifest
contains file identity and run ID. Cleanup verifies identity and ownership before deletion.
Never delete arbitrary directory trees. Startup quarantines ambiguous leftovers.

Use CreateProcessW directly with correctly escaped internal arguments; assign suspended
child to a kill-on-close Job Object before resume. Cancellation requests bounded graceful
stop, then terminates the owned job within 2s if necessary. Preserve partial outcome.
Close handles before cleanup. Watchdog and emergency stop work without the browser.
Recheck reserves at 1Hz; resource breach aborts. Failed cleanup is a safety event
with a retry action, not a swallowed exception. Opt-in tests exercise process crash,
disconnect, race attempts, disk exhaustion, stale sensors and orphan cleanup.
