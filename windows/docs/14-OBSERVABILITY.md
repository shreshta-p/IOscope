# Observability

Structured JSON logs: UTC, level, component, eventCode, correlationId, optional runId,
durationMs and safe diagnostic fields. Log initialization, capability transitions,
launch/exit, safety actions, experiment transitions, reconnects and persistence errors.
Per-sample logging is off by default; bounded opt-in diagnostics, never API secrets.

Agent health includes acquisition duration, missed samples, adapter timeouts, transport
queue bytes, dropped presentation frames, writer queue depth, writer commit latency,
agent CPU/memory and UI frame timings. Keep self-overhead separate from measured workload.
Clock domain and aggregation window accompany timings. Avoid high-cardinality labels.
Rotate local logs at 10MiB ×5; redact user names/paths and machine IDs in export.

Benchmark overhead protocol: no-workload baseline, collector only, collector+recording,
collector+UI, repeated in alternating order. Record wall time, CPU, GPU, memory,
disk writes and workload throughput/latency changes. Report variance, never “zero overhead”.
