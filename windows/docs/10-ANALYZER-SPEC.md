# Deterministic analyzer

Agent-owned, versioned rules consume normalized samples and emit immutable events:
observation, evidence references, interpretation, confidence and optional simple text.
Evidence references identify sample sequence, metric ID and device. Missing/stale
metrics suppress dependent rules. Simulation produces simulated evidence, not live claims.

Initial rules (at 1Hz, windows require complete valid samples):
- Memory pressure: available/total <10% for 5 samples; “may indicate memory pressure”.
- Queue pressure: last-five mean system queue exceeds first-five by >=2 AND mean
  storage latency rises >=25%, using 10 samples of the same device. “Consistent with
  increased outstanding storage activity”; no causal or workload-only claim.
- Thermal warning: mirror safety threshold evidence, never diagnose throttling from
  temperature alone. Throttling requires a reliable throttle flag or qualified clock/
  power evidence under comparable demand.
- GPU transfer phase: explicit instrumented phase marker, not inferred disk activity.
- VRAM pressure: used/total >=90% for 5 samples; suppress if either metric unavailable.
- Completion anomaly: expected terminal result missing or nonzero engine exit.

Event transition emits once, recovery emits once; 10s per-rule/device debounce.
Persist rule version, thresholds and original events; old replay does not rerun new rules.
System observations explicitly include background load. Offered-rate caps can limit
throughput and must be shown before interpreting low utilization as insufficient QD.

Ask the Analyzer is Phase 10, optional and disabled without configuration. Context:
run metadata, controls, compatible summaries, detected events and timeline markers;
no raw endless stream. Evidence IDs accompany claims; uncertain cause stays hypothesis.
Model errors do not affect deterministic analysis or workload execution. Never send
machine identifiers/paths/API keys; explicit user opt-in to external context transmission.
