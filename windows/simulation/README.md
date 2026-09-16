# Deterministic simulation

Pure TypeScript generation; no sensor, process, disk workload or GPU calls.
All profiles, hardware names and values are explicitly synthetic.

```powershell
npm ci
npm run verify
npm run simulation:export -- --scenario model-loading --seed 42 --duration 12 --output out/model-loading.json
```

Export creates a new JSON file and refuses to overwrite an existing one. It is the
only filesystem-writing simulation entrypoint; the generator itself is pure.
Available scenarios: idle, sequential-read, random-read, mixed, queue-saturation,
model-loading, memory-pressure, thermal-warning. Optional CLI controls:
--interval (100–1000ms), --epoch (UTC ISO timestamp), --output.

## Library

```typescript
import { generate, createRecording } from '@ioscope/simulation';
const config = {
  seed: 42, epochUtc: '2026-09-06T00:00:00Z',
  durationSeconds: 12, sampleIntervalMs: 1000,
  scenario: 'model-loading' as const,
};
const frames = [...generate(config)];
const recording = createRecording(config);
```

SimulationConfig and SimulationRecording are generated from canonical schema.
The recording includes normalized config, inventory, capabilities, run metadata and
self-contained replay snapshots. Validate imports with validateDomain('SimulationRecording', value).
Array access restores a snapshot; a playback scheduler/UI is Phase 4, not implemented here.

## Model assumptions and limits

Version 1 uses xorshift32, one draw per frame, fixed draw order and rounded metric
values. Zero seed maps to 0x6d2b79f5 to avoid an absorbing zero state.
Canonical config sorting and UTC normalization make IDs/output repeatable. IDs use a
non-cryptographic config hash; they are not globally unique run identifiers or integrity hashes.
Persistence must namespace imports rather than use these IDs as trusted global keys.

Time samples cover [0, duration), with the last sample marked completed and endedAt
equal to its timestamp. The requested duration controls the synthetic trace; metadata
workload settings are illustrative, bounded intent (duration capped at 60s), not an
executed DiskSpd configuration. Profiles do not predict laptop performance, obey real
hardware throughput limits, or conserve bytes across synthetic GPU phases.
No real benchmark summary or workload p95 is invented; p95 stays unavailable.

Queue saturation is a pedagogical rising queue/latency curve with a throughput plateau.
Memory pressure lowers modeled available RAM. Thermal-warning raises synthetic GPU
temperature through the warning threshold; it never heats hardware or implements a
SafetyManager. These are input fixtures for later deterministic diagnosis.

Model loading has six authored stages: prepare, allocate, read, H2D, compute, release.
Stages are explicit simulated metrics; events identify synthetic stage transitions.
H2D and compute flows require valid stage and activity evidence. Missing stage suppresses
phase labels and GPU flows. There is no real CUDA timing or model inference.

Mapping constants and path semantics live in contracts/flow-semantics.v1.json.
The fixed simulation bindings are cpu0/ram0/nvme0/gpu0/vram0; native topology binding
is a later adapter responsibility. Paths represent aggregates, not per-I/O operations.

## Golden fixture policy

fixtures/seed-42.json is a reviewed twelve-sample model-loading recording.
The test compares complete serialized bytes and asserts its stage sequence.
Profile, PRNG or mapping changes that alter output require an intentional version and
fixture review; do not blindly regenerate to make failing tests pass.
Python independently validates its schemas, units, origin and storage-flow scaling.
