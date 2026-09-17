# IOscope for Linux

An independent native Linux implementation of IOscope, a systems performance
digital twin and workload laboratory. Ported from the Windows baseline
(commit `280ced5`, tag `windows-baseline-2026-09-16`) and verified against real
hardware in a VirtualBox VM guest.

**Status: V1 complete for this platform.** Telemetry, the real fio-backed
workload engine, all five experiment types (queue-depth sweep, block-size
sweep, buffered/unbuffered, first/repeated access, and a capability-gated
storage-to-GPU pipeline), the deterministic analyzer, and contextual Learn are
all implemented and verified end to end against real hardware in this VM and a
real browser. Phase 10 ("Ask the Analyzer") is deferred at the project owner's
request. Bare-metal Linux hardware validation is a separate, explicitly
out-of-scope gate — VM evidence describes the guest, never the physical host.

## Quick start

```bash
npm ci
npm run verify              # contracts, format, typecheck, 121 Vitest tests
npm run build                # production UI build
cd agent
cmake -S . -B build
cmake --build build -j2      # cap parallelism -- see AGENTS.md
ctest --test-dir build --output-on-failure   # 12 native test suites
sudo apt-get install fio     # only needed to run real workloads, not to build
./build/ioscope_agent ..
```

Open http://127.0.0.1:8765. Starting the agent does not start a workload —
every real fio-backed run requires explicit admission (disk/RAM/thermal
budgets, cumulative write caps) and, throughout this project's own development,
explicit human opt-in before any real hardware execution. Measured
fresh-clone-to-fully-verified time in the reference VM: ~79 seconds (`npm ci`
8.6s, `npm run verify` 7.8s, `npm run build` 0.9s, `cmake` configure 5.4s,
native build 50.8s, `ctest` 5.9s). Agent startup to first HTTP response: 47ms.
Telemetry sample polling: sub-millisecond, against a 250ms budget.

Read [AGENTS.md](AGENTS.md) and [docs/CURRENT-STATE.md](docs/CURRENT-STATE.md)
before making changes — they carry the exact commands, results, and remaining
limitations, and are updated after every meaningful milestone.

## What's real here

- **Telemetry**: real `/proc`/`/sys` reads, dlopen'd NVML/CUDA probing, served
  over the same HTTP/WebSocket API shape as the Windows agent.
- **Workload engine**: `fio` (pinned by reported version, not a hardcoded
  binary hash) replaces DiskSpd, run under `io_uring`, isolated via a delegated
  cgroup v2 scope with `cgroup.kill` escalation. Proven for real: completed,
  cancelled, crash-interrupted, and failed outcomes, plus orphan-scratch
  recovery with honest (never fabricated) recovery telemetry.
- **Experiments**: a serial phase-sequencing engine reusing the same
  single-workload controller per phase, with aggregate disk/write/wall-time
  budget admission checked before any phase starts. All 5 experiment types
  from the spec are real, working profiles in the UI, run against real fio.
- **Analyzer**: 6 deterministic rules (memory pressure, queue pressure,
  thermal warning, GPU transfer phase, VRAM pressure, completion anomaly) over
  a rolling sample window, each transition-only-emits-once with a debounce and
  a recovery event, verified against real telemetry (zero false positives on a
  healthy run) and synthetic boundary cases for what this VM's hardware can't
  exercise (no temperature sensors, no GPU).
- **Learn**: 13 topics, each with real, working links back into the live
  digital twin and into the specific experiment that demonstrates it — or an
  honest "no experiment exists for this yet" / "requires a supported GPU"
  where that's the truth.

## What isn't here

- Phase 10 ("Ask the Analyzer"), an optional LLM-backed Q&A layer — deferred
  entirely, no plumbing exists.
- A packaging/installer step — this runs from source, same as the Windows
  baseline; there's no `.deb`/AppImage build.
- Bare-metal Linux hardware validation — this is VM evidence throughout, which
  the port plan explicitly does not treat as a substitute for a physical-host
  gate.
- The Python cross-language fixture validator and Windows doc-link checker —
  not ported; the TypeScript/Vitest suite and native `contract_tests`/
  `safety_tests` already validate fixtures from two independent language
  runtimes, which covers the same ground.

See [docs/CURRENT-STATE.md](docs/CURRENT-STATE.md) for the complete,
milestone-by-milestone evidence trail — exact commands, real measured numbers,
and every limitation recorded as it was found, not smoothed over.

## Platform isolation

This workspace is independently buildable. The sibling
[Windows implementation](../windows/README.md) is reference material only —
never a runtime or build dependency. Portable code that was deliberately
copied is documented in [docs/PORT-PROVENANCE.md](docs/PORT-PROVENANCE.md)
with its baseline commit and any recorded, backward-compatible schema changes.
Do not add imports, workspace links, or build paths into `../windows/`.
