# Phase 1 deterministic simulation implementation plan

**Goal:** Produce deterministic simulated domain recordings without hardware activity.
**Architecture:** A pure TypeScript generator with explicit seed, epoch and sample clock
emits canonical objects; a pure mapper derives flows. No UI or native agent.
**Tech stack:** Node 24, TypeScript strict, AJV 2020-12, generated types, Vitest.
**Spec:** [telemetry](../05-TELEMETRY-SPEC.md), [domain](../12-DATA-MODEL.md),
[phase gates](../19-ROADMAP.md), [contracts](../../contracts/README.md).

## Global constraints
Windows 11 is primary. Same seed/config/version produces byte-identical output.
No Date.now(), Math.random(), filesystem activity or sensor access inside generation.
Every generated frame uses simulated origin and simulated metric provenance.
All emitted objects pass schemas and semantic validation. Integer elapsedUs.

## Task 1 — Shared TypeScript contract workflow
Files: package.json, package-lock.json, contracts/package.json,
contracts/src/index.ts (generated), contracts/src/validate.ts,
contracts/tests/validation.test.ts, tools/generate-contracts.mjs.
- [x] Install and pin generator, AJV/ajv-formats, TypeScript and Vitest; npm workspaces.
- [x] Add a test loading every existing fixture and rejecting the current Python mutation
  cases; run npm test and confirm missing-validator failure.
- [x] Generate public types from domain.schema.json; compile all root schemas in one
  offline AJV registry. Implement validateDomain(name: string, value: unknown): void,
  including metric catalog, provenance, references and ordering invariants.
- [x] Test unknown versions, wrong units, NaN, missing values and duplicate metrics.
  Run npm test, npm run typecheck and Python baseline validators.
- [x] Regenerate and check no diff; update commands in AGENTS.md and TOOLCHAIN.md.

## Task 2 — Seeded virtual-clock generator
Files: simulation/package.json, simulation/src/config.ts, simulation/src/prng.ts,
simulation/src/generate.ts, simulation/tests/determinism.test.ts.
Interface:
```typescript
type Scenario = 'idle' | 'sequential-read' | 'random-read' | 'mixed' |
  'queue-saturation' | 'model-loading' | 'memory-pressure' | 'thermal-warning';
interface SimulationConfig {
  seed: number; epochUtc: string; durationSeconds: number;
  sampleIntervalMs: number; scenario: Scenario;
}
// finite generator, no wall-clock scheduler
generate(config: SimulationConfig): Iterable<TelemetryFrame>;
```
- [x] Write tests for identical serialized arrays with seed 42, changed output for
  seed 43 in a variable scenario, and rejected seed/time bounds.
- [x] Use unsigned xorshift32 with explicitly handled zero seed (map to fixed
  0x6d2b79f5); each step x ^= x<<13; x ^= x>>>17; x ^= x<<5; normalize >>>0.
  Fix PRNG consumption order in simulator version 1.0.0.
- [x] Validate integer seed 0..2^32-1, duration 1..600s and interval 100..1000ms.
  Emit t=0 through t<duration, UTC = epoch + t, sequence index, elapsedUs=t*1000.
- [x] Model scenario values as authored synthetic profiles; document assumptions,
  never label modeled curves measured. Idle low activity; read/mixed rates; queue
  saturation rising mean queue/latency; memory pressure falling available RAM;
  thermal warning threshold crossing; model loading explicit stage markers.
- [x] Test all eight scenarios for complete bounded frames and capability-based nulls.
  No “healthy-looking” substitute for an injected unavailable sensor.

## Task 3 — Flow snapshots and reproducibility fixtures
Files: simulation/src/flows.ts, simulation/src/record.ts,
simulation/tests/replay-input.test.ts, simulation/fixtures/seed-42.json.
Interface: deriveFlow(frame: TelemetryFrame): FlowState; record(config): ReplaySample[].
- [x] Write failing tests: same frame gives same flow; stale/null metric gives unknown
  with no particles; activity stays in [0,1]; evidence resolves to source frame.
- [x] Implement exactly contracts/flow-semantics.v1.json; use typed path registry,
  no sensor logic in renderer and no invented GPU movement.
- [x] Assemble deterministic IDs and self-contained snapshots, explicit synthetic
  status/events and inventory. No core diagnostic implementation in this phase.
- [x] Save a golden seed-42 recording, round-trip JSON and validate every sample.
  Seek a sample by index and prove telemetry/flow/events are unchanged.
- [x] Run TS tests/typecheck, schema-generation drift check and Python validators.
  Record results and known modeling assumptions in CURRENT-STATE.md.

Phase 1 ends when all eight scenarios, byte determinism, schema/semantic validation
and fixture round-trip pass. It does not include browser UI, real SQLite integration,
native telemetry or storage/GPU workloads. Implement inline with test-first steps;
do not start another phase simply because this plan is complete.

Completed 2026-09-06. See ../PHASE-1-VALIDATION.md for verification and limitations.
