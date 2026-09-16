# Contracts v1

Canonical definitions: v1/domain.schema.json. Public root files reference it.
JSON Schema draft 2020-12, version 1.0.0, closed objects and no remote resolution at
validation time. The ioscope.local IDs are identifiers, not hosted services.

Fixtures are hand-authored SIMULATED contract examples, not simulator output or
hardware captures. A zero engine digest identifies the synthetic fixture only;
real engine metadata must contain the actual executable hash.

Run: python tools/validate_contracts.py. The baseline checks schema validity, every
public root fixture, metric units/ranges, numeric finiteness, origin/provenance,
snapshot identity, flow evidence and negative mutations. JSON Schema cannot enforce
all cross-record references, time ordering, resource admission or state transitions.
Native and TS runtime validators must implement these before accepting external input.

## Invariants for runtime implementations
- Metric key unique per device/scope/frame; metric exists in catalog and unit matches.
- Available values finite and bounded; unavailable values null with reason.
- Live physical metrics only measured/derived; simulated frames only simulated metrics.
- Null means unknown, never zero. Stale data cannot drive active flow or safety admission.
- Every device reference belongs to inventory. Evidence refers to an existing sample
  and metric on that device. Window/age remain attached to evidence.
- Frame/flow sequence and time agree; replay run IDs agree across embedded objects.
- Strictly increasing sample sequence; elapsed time never decreases.
- Experiment phase ordinal unique/contiguous; only the declared variable changes.
- Terminal metadata has endedAt, and abnormal outcome has an explanatory reason.
- Simulated runs require seed/simulator version; live runs must not claim synthetic origin.
- Capacity relationships (used <= total), actual free reserves and write/time budgets
  are enforced beyond static request ranges.
- Workload state transitions: validating → preparing → running → stopping → terminal;
  preparation/running may terminate directly on error/cancel. No terminal restart.
- Recording envelopes preserve origin; unsupported versions are rejected, never guessed.

Metric catalog and flow semantics are canonical companion specifications. Derived
flow formula is presentation scaling, not device utilization. Default bandwidth scale
is fixed 1GiB/s so visuals can be compared; it is not a measured device maximum.
Flow state stores normalized outputs for deterministic replay across future mapper changes.

## Phase 1 additions

SimulationConfig and SimulationRecording are canonical public roots. The baseline
is unreleased; these additions extend its initial 1.0.0 specification before consumers
ship. Incompatible published changes require version negotiation.

npm run contracts:generate emits src/index.ts; npm run contracts:check rejects drift.
validateDomain provides offline AJV schema/format and semantic checks. validateRecording
checks inventory/evidence references, chronology, lifecycle and terminal agreement.
The simulation wrapper also validates cadence, sample count, UTC and seed agreement.
Dynamic resource admission remains the future native SafetyManager's responsibility.

Pipeline stage and H2D rate are explicit simulated or measured metrics, never inferred
from disk traffic. Stage markers are documented in flow-semantics.v1.json. Evidence
v1 lacks scope: ambiguity between scoped measurements is rejected rather than guessed.
Python checks fixtures and exported recordings; TypeScript additionally covers cross-record
references and lifecycle. They are complementary checks, not identical implementations.
