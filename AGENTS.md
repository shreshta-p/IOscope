# IOscope repository-wide instructions

## Start here
- Read [README.md](README.md), then the target platform's `AGENTS.md` and
  `docs/CURRENT-STATE.md` before editing.
- `windows/` contains the existing Windows implementation. `linux-vm/` is an
  independent Linux port workspace, currently planning only.
- Use `git status` before edits. Run platform commands from that platform directory.
- Keep project knowledge in versioned documents; no personal paths, editor extensions,
  AI services or installed skills may be required to build or validate the project.

## Platform isolation
- Source, dependencies, lockfiles, tests, builds and output belong inside the target
  platform folder. Do not introduce cross-platform-folder imports, workspace links,
  symlinks or build dependencies. Repository metadata and CI stay at the root.
- A Linux task must not silently modify Windows code. Document deliberate ports of
  portable code and preserve attribution; test wire compatibility explicitly.
- Windows contracts currently define the implemented v1 wire format. A future Linux
  copy must record its baseline and version changes; do not silently diverge under
  the same schema version or claim a Windows workload engine is available on Linux.
- Keep platform validation gates separate. Independent Linux groundwork does not
  require completing Windows's hardware trial. Dependent features still require
  evidence for their own platform's prerequisites.

## Safety and truth
- Missing or unsupported metrics remain unavailable. Simulation is always labeled;
  replay preserves original provenance. VM measurements describe the guest.
- No raw devices, arbitrary UI commands, administrator/root by default, protection
  changes or unbounded workloads. Native safety owns admission and cancellation.
- Hardware workloads are explicit opt-in, bounded, and never automatic CI benchmarks.
- Never recursively delete user-selected directories. Use owned scratch and verify
  file ownership and identity before cleanup.
- Never invent hardware evidence, pass untested gates or label the project complete
  while required work remains.

## Handoff and verification
- Update the platform's current-state document after meaningful changes with what
  changed, commands actually run, results, limitations and the next task.
- Keep historical evidence distinct from fresh verification. Record failed checks.
- Root CI must select a platform directory explicitly and use that platform's lockfile.
- Root `CLAUDE.md` imports this file. Platform `CLAUDE.md` files import local rules;
  keep substantive instructions here and in platform `AGENTS.md`, not duplicated.
