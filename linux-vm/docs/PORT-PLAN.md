# Linux VM port plan

This is a development handoff, not evidence of a working Linux port.
Run future Linux commands from `linux-vm/`. Keep dependencies and build output local
to this workspace, with no imports, symlinks or build dependencies on `windows/`.

## L0: environment and portable foundation

- Record Linux distribution, kernel, architecture, compiler, CMake, Node/Python
  versions, VirtualBox version and assigned CPU/RAM/storage configuration.
- Record the Windows baseline commit when it exists. Reference specifications in
  `windows/docs/` are Windows requirements; explicitly identify Linux replacements.
- Define Linux interfaces for telemetry, process lifetime, owned scratch, resource
  admission and workload execution. Do not replace Windows safety mechanisms with
  superficial API substitutions.
- Deliberately port canonical contracts, deterministic simulation and their tests;
  retain attribution, baseline details and golden fixtures. Establish a compatible
  versioned wire format with explicit unavailable capabilities and VM context.
- Add Linux-local dependency manifests, lockfiles, documented setup and verification
  commands, then Linux CI that performs no hardware workloads.

Exit: a clean Linux checkout can reproduce contract/simulation checks without a
Windows checkout at build/runtime, and the adapter design documents safety boundaries.

## L1: read-only application

- Implement supported guest CPU, RAM and storage discovery/counters with typed
  unavailable reasons, real timing and stale/disconnect handling.
- Port local authenticated transport, persistence, UI and replay; preserve original
  measurement origin and record virtualization context separately.
- Sensors and GPU features require capability discovery; absence is never zero.
- Validate functionality and measure overhead in the guest without launching workloads.

Exit: native Linux agent and UI run together; recordings validate and replay;
failure handling and guest measurement scope have test evidence.

## L2: safe Linux workload engine

- Evaluate a pinned fio version and map supported settings and summaries explicitly.
  Do not claim numerical equivalence with DiskSpd based only on matching options.
- Design process-tree termination, bounded output, deadlines, resource watchdogs,
  owned-file preparation/cleanup, active-run journaling and interruption recovery.
- Treat guest filesystem free space and host virtual-disk capacity as different
  constraints. Document how bounded trials are admitted safely in the chosen VM.
- Test with fake adapters and tiny owned fixtures before an opt-in real run.
- Preserve the initial 64 MiB/five-second read-trial constraint unless explicitly
  revised. Preparation writes must be budgeted and disclosed.

Exit: completed, failed, cancelled and interrupted runs have validated records;
cleanup and termination are verified; a bounded VM trial is labeled as VM evidence.

## L3: dependent features and release

Only after Linux workload prerequisites pass: serial controlled experiments,
deterministic analysis, contextual Learn, optional capability-gated GPU/Ask features,
packaging, dependency notices and measured end-to-end budgets.

VM evidence does not pass a physical-hardware gate. Perform separate opt-in bare-metal
Linux validation before claiming physical storage or hardware performance support.
