# Linux VM current state

Updated 2026-09-16.

## Implemented

Repository workspace, agent-neutral instructions and a staged port plan only.
The existing Windows application is isolated in `windows/` at the repository root.
No code was copied here merely to make this folder appear implemented.

## Verification

No Linux build, application test, VM trial or bare-metal trial has run.
Windows results do not satisfy Linux gates. Repository documentation links are
checked by the Windows documentation validator.

## Next task

Execute milestone L0 in [PORT-PLAN.md](PORT-PLAN.md): record the guest environment,
define Linux adapter and safety boundaries, then establish local reproducible
tooling and port the contracts/simulation with provenance and compatibility checks.
Do not start a hardware workload during setup.

## Pending gates

- L0: environment, architecture and portable foundation.
- L1: read-only Linux telemetry and local transport/UI.
- L2: bounded Linux workload execution and safety/recovery validation.
- L3: controlled experiments, analysis, learning and release preparation.
- Separate bare-metal Linux hardware and release validation.

The Windows Phase 6 real-run gate remains outstanding independently.

## Baseline handoff

The initial platform-separated checkpoint is tagged `windows-baseline-2026-09-16`.
Use that Git tag to identify the exact Windows source when porting code. Linux work
starts from this repository checkpoint; the tag does not signify a Windows release
or a passed real-hardware gate.
