# Domain model

Canonical wire source: contracts/v1/domain.schema.json (JSON Schema 2020-12).
Public roots reference its definitions; no parallel type schemas.
Initial version is 1.0.0. Unknown versions are rejected. Closed objects mean even
additive changes require explicit negotiation/version fixture updates.

| Contract | Role |
|---|---|
| TelemetryFrame | session/run origin, monotonic sample, measurements |
| DeviceCapabilities | per-metric sensor state and privilege requirement |
| HardwareInventory | discovered devices and normalized capacity metrics |
| FlowState | versioned aggregate path encoding and evidence |
| WorkloadDefinition | bounded intent, no paths or executable input |
| WorkloadStatus | run lifecycle and progress |
| ExperimentDefinition | question, variables, controlled serial phases |
| ExperimentPhase | workload plus phase purpose and observations |
| RunMetadata | provenance, snapshots, execution identity, outcome/summaries |
| ReplaySample | self-contained telemetry/flow/events/status snapshot |
| AnalyzerEvent | observation, evidence, interpretation, confidence |
| SafetyEvent | rule, decision and supporting evidence |
| SystemError | stable code, operation, retryability, safe message |
| PresentationContext | mode plus immutable original origin |
| SimulationConfig | bounded seed, epoch/cadence, scenario and unavailable injection |
| SimulationRecording | config plus metadata and complete replay samples |

Origin live/simulated persists forever. Recorded is a presentation mode, never a
rewrite of measurement provenance. Provenance measured/derived/simulated/conceptual
is independent of availability. Conceptual values cannot masquerade as physical sensors.

Metric key is (deviceId, metricId, scope). Unit and bounds come from metric-catalog.
Elapsed microseconds and sequence are nonnegative JS-safe integers. Rates use byte/s;
sizes are bytes; display uses KiB/MiB/GiB. No timestamps as large nanosecond doubles.
ID references must resolve within session/run. Samples strictly increase by sequence
and nondecreasing elapsed time; duplicates are rejected except idempotent transport retry.

JSON Schema enforces structural/local numeric bounds. Application validators also
enforce metric-unit/provenance compatibility, device/evidence references, time ordering,
origin agreement, capacity relationships, run-state transitions and dynamic resource
budgets. See contracts/README.md for baseline fixture checks and future runtime duties.
