# IOscope Windows instructions

Scope: `windows/` only. Read the [repository rules](../AGENTS.md) first.
Run all commands below from this directory; source, build, output and documentation
paths in this file refer to this Windows workspace, not the Git repository root.
The Linux port has separate instructions and gates in `../linux-vm/`.

## Goal and current boundary
Build a technically honest Windows 11 systems performance digital twin.
Read docs/CURRENT-STATE.md first. Phases 0-5 are verified; Phase 6 real-run validation remains outstanding. Disk/RAM reserves were revised at user request; see docs/08-SAFETY-SPEC.md. Do not implement later phases while their prerequisite gates fail.

## Map and ownership
- docs/: normative subsystem specs, ADRs, roadmap, evidence and current state.
- contracts/v1/: canonical JSON Schema definitions; fixtures exercise each public root.
- tools/: baseline validators.
- apps/ui/: React/TypeScript, R3F scene; no native commands.
- agent/: C++20 core, telemetry, workloads, experiments, analysis, safety,
  storage and transport modules, each behind domain interfaces.
- simulation/: pure seeded generator, flow snapshots, recording and golden fixtures.
- Planned assets/: only licensed, provenance-reviewed resources.
- Planned tests/: cross-language fixtures, integration, E2E and opt-in hardware tests.

## Commands and conventions
Current: npm ci; npm run verify (types, formatting, tests, Python contracts/docs).
Export: npm run simulation:export -- --output out/simulation.json (refuses overwrite).
Install validator: python -m pip install -r tools/requirements.txt.
Build commands and tested prerequisites: docs/TOOLCHAIN.md.
UTF-8, LF, two-space JSON/TS/C++, four-space Python. TS strict mode and npm run format:check are active; future ESLint/clang-format; no broad lint suppression or unchecked any.
Use git status before edits. Do not silently rewrite an unfamiliar module.

## Contracts and truth
JSON Schema is canonical. Change schema, fixtures and semantic invariants together.
Generate types with npm run contracts:generate; npm run contracts:check detects drift.
Never edit contracts/src/index.ts by hand or maintain competing wire models.
Reject unsupported versions, nonfinite values and invalid units.
LIVE/RECORDED/SIMULATED is presentation mode; origin stays live/simulated on replay.
Never replace missing/stale/errored telemetry with zero or synthetic values.
Conceptual paths and configured outstanding limits are not measured hardware queues.

## Safety and adapters
No raw devices, arbitrary frontend paths/commands, admin-by-default, thermal stress,
firmware/BIOS changes or OS protection changes. Native SafetyManager owns admission
and watchdog decisions even if UI disconnects. Use agent-owned local scratch outside
OneDrive. Never recursively delete a user-selected directory.
Add adapters through capability discovery, typed unavailable reasons, timeouts,
RAII cleanup and fake-adapter tests before hardware integration.
Add experiments as serial controlled phases with budgets, hypotheses, sample scope,
abort behavior and reproducible run records. Read docs/07-EXPERIMENT-SPEC.md.

## UI and testing
Add UI through common data-source contracts, accessible component selection and
versioned flow semantics. No DiskSpd flags or sensor-specific logic in scene components.
Use deterministic tests for math, state transitions, replay and failure cases.
Hardware tests are explicitly opt-in, bounded and never automatic CI benchmarks.
Update docs/CURRENT-STATE.md and relevant specs with every phase exit.
Do not invent results, copy unlicensed assets, build generic command executors,
silently migrate unknown recordings or mark untested hardware gates passed.
