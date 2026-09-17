# Linux VM current state

Updated 2026-09-17.

## Implemented

L0 (portable foundation) and L1 (read-only telemetry + transport) are done with
evidence below. A native Linux C++20 agent exists at `linux-vm/agent/`: it serves
real telemetry (`/proc`, `/sys`, dlopen'd NVML) over the same HTTP/WebSocket API
shape as the Windows agent, validates every response against the shared contracts,
and persists to SQLite. The L2 workload engine (fio-backed, cgroup-isolated) is
built and passes its full fake-adapter test suite; it has not yet run a real fio
workload because fio is not installed on this VM yet (deliberately — see "Next
task"). No hardware benchmark has been run. No UI exists yet.

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
  writing scratch files under `/tmp` would silently skip real disk I/O. The native
  agent's `require_durable_filesystem()` (`agent/scratch.hpp`) refuses tmpfs/overlay/
  network/vboxsf filesystems for scratch, so this can't happen silently.
- Host-side VM configuration (assigned vCPU/RAM/disk limits, host CPU/SSD model, virtual
  disk type — fixed vs dynamically allocated) is not visible from inside the guest and
  has not been supplied. Not invented; see "Known limitations" below.

## Toolchain installed

- Git 2.53.0, GitHub CLI 2.46.0 (authenticated as `shreshta-p`) — apt, sudo (2026-09-16).
- Node.js v24.21.0 / npm 11.19.0 — nvm 0.40.1, user-space, no sudo (2026-09-16).
- Python 3.14.4 present; `pip` still not installed (see "Known limitations").
- GCC 15.2.0, CMake 4.2.3, GNU Make 4.4.1 — apt, sudo (2026-09-17). Needed for the
  native agent; C++20 supported.
- `fio` still not installed — needed for the next task (real workload execution),
  not yet requested from the user.

## Ported code and provenance

See [PORT-PROVENANCE.md](PORT-PROVENANCE.md) for exactly what was copied from
`windows/` at commit `280ced5` (tag `windows-baseline-2026-09-16`), what's new/adapted,
and one deliberate, recorded contract-schema change (see below).

## Native agent architecture (`linux-vm/agent/`)

Ports `windows/agent`'s pure C++ business logic verbatim (`contracts.hpp`,
`safety.hpp`, `counter_math.hpp`, `store.hpp`, `json_input.hpp`,
`native_recording.hpp`, `security.hpp`, `workload_controller.hpp` — one token changed,
see PORT-PROVENANCE.md) and rewrites the Win32-specific adapters for Linux
(`platform.hpp`, `telemetry.hpp`, `system_resources.hpp`, `scratch.hpp`,
`process_job.hpp`, `main.cpp`) per `ADAPTER-BOUNDARIES.md`. `fio.hpp`/`fio_json.hpp`
replace `diskspd.hpp`/`diskspd_xml.hpp`; fio is pinned by its reported version
(`fio-3.41`) rather than a hardcoded binary hash, since it comes from the apt package
manager (signed) rather than a manually downloaded release. Process isolation uses a
delegated cgroup v2 scope with `cgroup.kill` (falls back to process-group signals if
delegation isn't available), not Windows Job Objects.

Dependencies (Crow, Asio, nlohmann-json, SQLite amalgamation, jsoncons) are fetched by
CMake `FetchContent` with the same pinned URLs/hashes as `windows/agent/CMakeLists.txt`
— these libraries are cross-platform; only compiler/link flags differ.

## Deliberate, recorded contract change

`contracts/v1/domain.schema.json`'s `WorkloadAdmission.engineVersion` was
`{"const": "2.3"}` (DiskSpd's exact pinned version, hardcoded into the schema).
Relaxed to a bounded string so fio's version can be honestly reported instead of
misreporting DiskSpd's. Every Windows-valid document is still valid under this
schema (backward compatible), so `schemaVersion` stays `"1.0.0"`. Full detail and
verification in [PORT-PROVENANCE.md](PORT-PROVENANCE.md).

## Verification (commands actually run, 2026-09-16/17, from `linux-vm/`)

TypeScript side (`linux-vm/`):
```
npm install; node tools/generate-contracts.mjs
diff contracts/src/index.ts ../windows/contracts/src/index.ts   # differs only in engineVersion type (expected, see above)
npx tsc --noEmit                # no errors
npm run verify                  # contracts:check + format:check + typecheck + test: all pass, 109 tests
diff simulation/fixtures/seed-42.json ../windows/simulation/fixtures/seed-42.json   # IDENTICAL
```

Native agent (`linux-vm/agent/`):
```
cmake -S . -B build && cmake --build build -j$(nproc)   # all 12 targets build clean (-Wall -Wextra -Werror)
cd build && ctest --output-on-failure
  # 10/10 tests pass: native_preflight, store_tests, contract_tests, json_input_tests,
  # counter_math_tests, safety_tests, process_job_tests, scratch_tests, fio_tests,
  # workload_controller_tests
./ioscope_agent ../.. --probe
  # real /proc,/sys read: CPU "Intel(R) Core(TM) i9-14900HX", storage device "sda"
  # resolved via /sys/dev/block, GPU honestly "unavailable" (no NVIDIA driver in VM),
  # 10 samples all pass TelemetryFrame schema validation, max poll 0.70ms (<250ms), exit 0
./ioscope_agent ../..    # full HTTP/WebSocket server
  curl /api/v1/bootstrap                    # real bearer token issued
  curl /api/v1/telemetry (unauthenticated)  # 401, correctly rejected
  curl /api/v1/telemetry (with token)       # real CPU/RAM/disk measurements returned
  curl -X POST /api/v1/runs/admission (WorkloadDefinition fixture)
  # -> allowed:false, reason "fio executable not found" (correct: fio isn't
  #    installed); restrictedThermals:true (correct: no CPU/SSD sensors in this VM);
  #    real diskFreeBytes/ramAvailableBytes/bufferBytes/plannedWriteBytes computed
  #    correctly from live statvfs/meminfo
  # verified after: ~/.local/share/ioscope/scratch/ is empty — denied admission had
  # zero side effects, matching the workload_controller_tests guarantee
```

`process_job_tests` (3.7s) exercises real fork/exec, cgroup-or-pgid cancellation,
SIGTERM/escalation timing, and bounded-output enforcement against actual child
processes on this VM — not a simulation of the mechanism. `scratch_tests` exercises
real file creation/identity/cleanup/orphan-recovery on the real ext4 filesystem.

CI: `.github/workflows/validate.yml` has a `linux-validation` job (TypeScript side
only; the native agent isn't wired into CI yet — see "Next task"). Not yet observed
running on GitHub Actions from this session.

## Known limitations / not yet done

- Python-based cross-language fixture validation
  (`windows/tools/validate_contracts.py`) and the Windows doc-link checker are not
  ported. The TS/Vitest suite and the native `contract_tests`/`safety_tests` already
  validate fixtures from two independent language runtimes now, which is meaningful
  cross-language coverage even without porting the Python script itself.
- fio is not installed; `FioEngine` has never executed a real fio process, only the
  fake-adapter `WorkloadController` tests. No numeric disk performance claim is made
  anywhere.
- Native agent isn't wired into `.github/workflows/validate.yml` yet — CMake
  `FetchContent` needs network access in CI, which needs verifying separately.
- No UI exists for Linux. The agent's `/` and `/assets/*` routes exist and correctly
  503 with "UI build missing" (matches Windows' behavior for the same case).
- Host-side VM specs (assigned resource limits, host disk type, physical host
  capacity) are still unknown; not requested from the user yet.
- fio's `--rate` limiting, `ramp_time`-as-warmup, and lack of a DiskSpd-style
  unmeasured cooldown tail are documented approximations in `agent/fio.hpp`, not
  verified against a real run yet.

## Pending gates

- [x] L0: environment, architecture and portable foundation.
- [x] L1: read-only Linux telemetry, transport — real HTTP/WebSocket server verified
      against live `/proc`/`/sys` data. No UI yet (not blocking; UI is separate from
      the telemetry/transport gate itself).
- [~] L2: native workload engine built, fake-adapter tests pass (10/10 native tests,
      including real process isolation and scratch management on this VM). Real fio
      execution and the bounded opt-in hardware trial are still outstanding.
- [ ] L3: controlled experiments, analysis, learning and release preparation.
- [ ] Separate bare-metal Linux hardware and release validation.

The Windows Phase 6 real-run gate remains outstanding independently and does not
block this work.

## Next task

Install the pinned `fio` package (requires sudo; will ask the user), then:
1. Verify `FioEngine::ready()`/`verify_fio()` against the real binary.
2. Run one real `FioEngine::execute()` against a tiny owned fixture (not the 64MiB/5s
   trial yet) to validate the fio JSON parser against real fio output, not just the
   hand-authored fixture in `fio_tests.cpp`.
3. Test cancellation, timeout, and interrupted-run recovery against the real process
   (process_job_tests/scratch_tests already prove the mechanism; this step proves it
   with fio specifically).
4. Only after that evidence exists: request explicit opt-in for the bounded 64MiB
   file / 5-second-read real trial, per the safety policy — never automatic.

## Baseline handoff

The initial platform-separated checkpoint is tagged `windows-baseline-2026-09-16`
(commit `280ced5`). Linux work is on branch `codex/linux-port`, currently unpushed.
