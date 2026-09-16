# Telemetry

Acquire native Win32/PDH CPU time deltas, GlobalMemoryStatusEx memory, per-physical-disk
PDH read/write bytes per second, transfers per second, average queue and average
latency where supported. NVML loads dynamically for utilization, VRAM, temperature,
clocks and power only when each function succeeds. No privileged sensor driver in V1
baseline; SSD/CPU temperature and fans may remain unavailable.

Capability states: available, unavailable, unsupported, temporarily_errored.
Each measurement has metric/device IDs, unit, status, provenance, source, scope,
sample age, aggregation window and unavailable reason. Available means finite numeric
value; other states carry null. Counter reset/warmup yields unavailable rather than
negative rates. A rate needs two valid samples and elapsed monotonic time > 0.
Validate physical bounds; do not clamp bad sensor output into plausible telemetry.

Acquire at 1 Hz initially, deadline 250ms per adapter; stale after 3000ms.
Temperature safety sampling remains independent of render demand. Source timestamps
and age make mixed sampling cadences explicit. QPC-derived elapsed microseconds are
session-relative safe integers; UTC anchors are for display, not rate calculations.

System counters include background applications and IOscope. DiskSpd result metrics
are workload scoped and phase-window scoped. PDH average latency is not p95.
PDH queue is not exact NVMe hardware queue occupancy. No per-request tracing in V1.
Metric IDs/units are canonical in contracts/metric-catalog.v1.json.
Hardware inventory discovers capacities and names; supplied laptop specs are targets,
not discovered facts. Adapter reconnect does not reuse stale device bindings.
