# Linux VM current state

Updated 2026-09-16.

## Implemented

Milestone L0 (environment, adapter design, portable foundation) is done. The
`contracts/` and `simulation/` packages are ported from the Windows baseline with
their tests, and run standalone in this workspace with no Windows checkout present at
build or runtime. `linux-vm/docs/ADAPTER-BOUNDARIES.md` records the planned Linux
replacements for telemetry, workload execution, process cancellation, scratch, resource
admission and persistence. No telemetry, workload, transport, UI or native agent code
exists yet — those are L1/L2.

## Guest environment (recorded 2026-09-16)

- OS: Ubuntu 26.04.1 LTS ("Resolute Raccoon")
- Kernel: Linux 7.0.0-31-generic, x86_64
- Virtualization: `systemd-detect-virt` reports `oracle` (VirtualBox), guest additions
  7.2.6_Ubuntu, DMI vendor "innotek GmbH" — confirms this is the VirtualBox guest, not
  bare metal.
- CPU: Intel Core i9-14900HX, 6 vCPUs assigned (1 thread/core, 6 cores/socket per
  `lscpu`) — this is the VM's allocation, not the physical CPU's full core count.
- RAM: 7.2 GiB total (`free -h`).
- Storage: `/` is `/dev/sda2`, ext4, 25 GiB total / ~14 GiB free at time of recording.
  `/tmp` is `tmpfs` (RAM-backed, 3.7 GiB) — confirmed importantly different from `/`:
  writing scratch files under `/tmp` would silently skip real disk I/O.
- Host-side VM configuration (assigned vCPU/RAM/disk limits, host CPU/SSD model, virtual
  disk type — fixed vs dynamically allocated) is not visible from inside the guest and
  has not been supplied. Not invented; see "Known limitations" below.

## Toolchain installed (recorded 2026-09-16)

- Git 2.53.0, GitHub CLI 2.46.0 (authenticated as `shreshta-p`) — installed via `apt`
  (required sudo).
- Node.js v24.21.0 / npm 11.19.0 — installed via nvm 0.40.1 into `~/.nvm`, user-space,
  no sudo. Matches the Windows workspace's `engines.node: ">=24 <25"` constraint.
- Python 3.14.4 present; `pip`/`ensurepip` are not (Ubuntu splits them into a separate
  `python3-pip` apt package). Not installed yet — see "Known limitations."
- No C/C++ toolchain (gcc/g++/cmake/make) installed yet — not needed until native
  telemetry/agent work begins (L1+).
- `fio` deliberately not installed — L0 explicitly excludes launching or installing it.

## Ported code and provenance

See [PORT-PROVENANCE.md](PORT-PROVENANCE.md) for exactly what was copied from
`windows/` at commit `280ced5` (tag `windows-baseline-2026-09-16`), what's new/adapted,
and what was deliberately left out.

## Verification (commands actually run, 2026-09-16, from `linux-vm/`)

```
npm install                     # 70 packages, 0 vulnerabilities
node tools/generate-contracts.mjs
diff contracts/src/index.ts ../windows/contracts/src/index.ts   # -> IDENTICAL
npx tsc --noEmit                # -> no errors
npx vitest run                  # -> 6 test files, 109 tests, all passed
npm run verify                  # contracts:check + format:check + typecheck + test, all passed
diff simulation/fixtures/seed-42.json ../windows/simulation/fixtures/seed-42.json
                                 # -> IDENTICAL (golden recording matches Windows byte-for-byte)
npx tsx tools/export-simulation.ts --scenario model-loading --seed 42 --duration 12 \
  --output <scratch>/sim-export-test.json
                                 # -> exits 0, prints simulation.exported JSON line
```

All of the above passed with no modifications to the copied source files. This
confirms the v1 contracts and deterministic simulation are wire-compatible and
reproducible on Linux without a Windows checkout.

CI: `.github/workflows/validate.yml` now has a `linux-validation` job
(`ubuntu-latest`, working directory `linux-vm`, `npm ci && npm run verify`) alongside
the existing Windows job. Not yet observed running on GitHub Actions from this session
— push and check before relying on it as passing evidence.

## Known limitations / not yet done

- Python-based cross-language fixture validation
  (`windows/tools/validate_contracts.py`) and the Windows doc-link checker
  (`windows/tools/check_docs.py`) are not ported. The TS/Vitest suite already validates
  every fixture via `validateDomain`/`validateRecording`, so contract correctness has
  coverage; the Python validator would add cross-language parity assurance relevant
  once a native Linux agent exists. Deferred rather than installing `python3-pip` for
  no immediate consumer.
- No telemetry adapters, transport, UI, native agent, or workload engine exist for
  Linux. `ADAPTER-BOUNDARIES.md` is a design document, not implemented code.
- Host-side VM specs (assigned resource limits, host disk type, physical host
  capacity) are unknown and have not been requested from the user yet; guest-only
  facts above are all that's recorded until that's needed for L1/L2 admission logic.
- No real workload, benchmark, or fio installation has happened. Nothing here
  represents disk/CPU/GPU performance evidence of any kind — this milestone is
  contracts/tooling only.

## Pending gates

- [x] L0: environment, architecture and portable foundation — done, see above.
- [ ] L1: read-only Linux telemetry and local transport/UI.
- [ ] L2: bounded Linux workload execution and safety/recovery validation.
- [ ] L3: controlled experiments, analysis, learning and release preparation.
- [ ] Separate bare-metal Linux hardware and release validation.

The Windows Phase 6 real-run gate remains outstanding independently and does not
block this work.

## Next task

Begin L1: design and implement read-only Linux telemetry adapters per
`ADAPTER-BOUNDARIES.md` (`/proc/stat` CPU, `/proc/meminfo` RAM, `/sys/block/*/stat`
disk, capability-gated NVML/hwmon), with typed unavailable states — no zero/invented
values — and fake-adapter tests before touching real `/proc`/`/sys` reads in the
agent. Local transport/UI port follows once telemetry has test evidence.

## Baseline handoff

The initial platform-separated checkpoint is tagged `windows-baseline-2026-09-16`
(commit `280ced5`). Linux work is on branch `codex/linux-port`, currently unpushed.
