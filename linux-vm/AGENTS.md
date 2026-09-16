# IOscope Linux VM instructions

Read the [repository rules](../AGENTS.md), [current state](docs/CURRENT-STATE.md)
and [port plan](docs/PORT-PLAN.md) first. These rules apply to `linux-vm/` only.

This is a port workspace, not a working Linux application. Do not claim the Windows
agent builds on Linux or that Windows test results validate this platform.

Keep Linux source, dependency manifests, build scripts and tests here. The sibling
Windows workspace is reference material, not a runtime or build dependency.
Document the provenance of any source deliberately ported from it.

Start with reproducible setup and portable contract/simulation checks, then read-only
telemetry, then the safe workload engine. Each stage needs its own evidence before
dependent functionality proceeds. Do not implement later experimental features first.

Use a guest-local checkout and agent-owned scratch outside VirtualBox shared folders.
Record distribution, kernel, toolchain, VM configuration and exposed capabilities.
Guest telemetry is live guest telemetry, not simulated data or physical-host evidence.
Unavailable temperatures, GPU counters and host details must remain unavailable.
Guest free space alone does not establish host capacity for a growing virtual disk.
Keep trials bounded and explicit; do not install or launch fio as part of this handoff.

The Windows baseline uses a 64 MiB file and five seconds of reads after preparation.
Do not silently increase it during porting. Linux admission, preparation, cancellation,
process cleanup and file identity checks need independent design and validation.

Keep Linux progress in `docs/CURRENT-STATE.md`; document real setup/test commands as
they become available. No particular coding agent, editor or chat history is required.
