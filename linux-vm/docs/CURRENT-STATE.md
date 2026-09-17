# Linux VM current state

Updated 2026-09-17.

## Implemented

L0, L1, and L2 are done with evidence below. A native Linux C++20 agent exists at
`linux-vm/agent/`: it serves real telemetry (`/proc`, `/sys`, dlopen'd NVML) over the
same HTTP/WebSocket API shape as the Windows agent, validates every response
against the shared contracts, persists to SQLite, and admits, runs, completes,
cancels, recovers from interruption, and cleans up real fio-backed workloads end to
end. With the user's explicit opt-in, several real bounded runs executed against
the real ext4 disk in this VM, culminating in the port plan's specifically named
trial (64 MiB file, 5 seconds of reads); see "Real workload evidence" below. These
are engineering-validation and safety-evidence runs in a VM, not a disk performance
characterization of anything. No UI exists yet.

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
- `fio-3.41` — apt, sudo (2026-09-17), matches the version pinned in `agent/fio.hpp`.

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

## Deliberate, recorded contract changes

Found only by actually running a request through the real agent — no fake-adapter
test exercises live engine/platform values. Three additive, backward-compatible
schema relaxations plus one matching C++ logic fix (every prior Windows-valid
document remains valid; `schemaVersion` stays `"1.0.0"`):

1. `WorkloadAdmission.engineVersion`: `{"const": "2.3"}` (DiskSpd's exact version)
   → bounded string, so fio's version is reported honestly instead of DiskSpd's.
2. `HardwareInventory.platform` enum: `["windows", "simulated"]` → added `"linux"`
   (there was no valid value a Linux agent could ever report).
3. `RunMetadata.artifacts[].kind` / `ArtifactPayload.kind` enums: added `"fio-json"`
   alongside `"diskspd-xml"` (fio's JSON result artifact has a different format).
4. `contracts.hpp`'s native-origin check hardcoded
   `metadata["inventory"]["platform"]=="windows"` — changed to accept `"linux"` too.

Full detail and verification in [PORT-PROVENANCE.md](PORT-PROVENANCE.md).

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

## Real workload evidence (2026-09-17, user opt-in given for this specific run)

After fixing the four contract issues above, one real fio-backed workload ran
end-to-end through the actual HTTP API (`POST /api/v1/runs`, workload: 64 MiB
working set — the policy's minimum, 4 KiB blocks, queue depth 1, random reads,
buffered, 1 second measured duration, light intensity/rate cap):

```
curl -X POST .../api/v1/runs {64MiB, 1s, random read, buffered, light}
  -> state: preparing, real runId
poll .../api/v1/runs/active
  -> state: completed, real recordingId, elapsedUs ~3.7M (prep + measured + overhead)
GET .../api/v1/recordings/<id>
  -> outcome: completed
  -> engine: {"name":"fio","version":"fio-3.41"} (honest, not "diskspd"/"2.3")
  -> inventory.platform: "linux"
  -> summaries: storage.read.bytes_per_second ~10.96 MB/s, storage.iops ~2676,
     storage.latency.mean ~0.32ms, storage.latency.p95 ~0.52ms
     (rate-capped by "light" intensity = 32 MiB/s cap; not a benchmark claim —
     one tiny run in a VM, not the sanctioned trial)
  -> artifacts: ["fio-json"] (correctly labeled, not "diskspd-xml")
ls ~/.local/share/ioscope/scratch/  -> empty (owned file + manifest cleaned up)
```

This is the first real (non-fake-adapter) proof that `FioEngine`'s preparation
helper, fio invocation, JSON parsing, contract validation, SQLite persistence, and
scratch cleanup all work together correctly on this VM. The four contract bugs
above (all Windows-specific hardcoded values with no valid Linux equivalent) were
found and fixed specifically because this real run was attempted — fake-adapter
tests use synthetic values that never touch these code paths, which is itself a
useful lesson about what fake-adapter coverage does and doesn't prove.

Two more real runs (still within the same opt-in, same tiny 64 MiB working set,
10-second workload so there was time to act mid-run) proved the remaining safety
paths against a real fio process, not just fake adapters or generic test binaries:

**Cancellation**: started a real run, waited for `state: running`, called
`POST /api/v1/runs/cancel`. Result: `outcome: cancelled`,
`abortReason: "User requested cancellation"`, 12 real telemetry samples captured
during the run, scratch fully cleaned up (`ls scratch/` empty afterward).

**Interruption recovery**: started a real run, waited until well into `running`,
then `kill -9`'d the agent process itself (simulating a crash) — leaving a real
64 MiB `data.bin` + `ownership.json` orphaned in scratch, with the fio child
process independently gone too (its cgroup scope contains only itself, so it
naturally exits/is reaped rather than surviving as a true orphan). On restart, the
agent silently recovered the orphaned scratch via `recover_scratch()` (verified
identity match, deleted) and `WorkloadController::recover()` marked the run
`outcome: interrupted`, `reason: "Agent restarted; interruption time is unknown"` —
critically, every measurement in the recovery sample is honestly `status:
unavailable, reason: "Agent restarted; no measurement at interruption"`, never a
stale or fabricated value presented as fresh evidence.

Completed, cancelled, and interrupted outcomes are now proven against a real fio
process end to end (matching the Windows Phase 5-6 evidence pattern of validating
each terminal state). "Failed" and "aborted" (safety-watchdog) outcomes remain
proven only via the fake-adapter `FakeEngine` in `workload_controller_tests.cpp`,
which deliberately exercises those states — triggering them with a real fio process
would need contriving a real failure (e.g. a bad argument or corrupted output),
which hasn't been attempted.

**The named trial**: with a second, separate explicit opt-in, ran the port plan's
specifically named configuration — 64 MiB working set, 5 seconds measured duration
(queue depth 8, random reads, buffered, `light` intensity — `moderate` was
correctly denied by admission with reason "Missing CPU/SSD temperatures require
light intensity and at most 15 seconds", confirming the restricted-thermal-coverage
policy is enforced for real, not just in the fake-adapter tests). Result:
`outcome: completed`, `engine: {"name":"fio","version":"fio-3.41"}`,
`inventory.platform: "linux"`, `artifacts: ["fio-json"]`, 10 real telemetry samples,
scratch fully cleaned up afterward. Measured summaries (read ~33.16 MB/s — at the
"light" intensity's 32 MiB/s rate cap, as expected; ~8096 IOPS; latency mean
~0.74ms, p95 ~1.07ms) are recorded for completeness but are, again, not a
performance characterization of anything — a rate-capped run in a VM.

CI: `.github/workflows/validate.yml` has a `linux-validation` job (TypeScript side
only; the native agent isn't wired into CI yet — see "Next task"). Not yet observed
running on GitHub Actions from this session.

## Known limitations / not yet done

- Python-based cross-language fixture validation
  (`windows/tools/validate_contracts.py`) and the Windows doc-link checker are not
  ported. The TS/Vitest suite and the native `contract_tests`/`safety_tests` already
  validate fixtures from two independent language runtimes now, which is meaningful
  cross-language coverage even without porting the Python script itself.
- Timeout (deadline-exceeded, as opposed to user-requested cancellation) has been
  proven against real child processes generically (`process_job_tests`) but not
  against a real in-flight fio run specifically. Cancellation and crash/interruption
  recovery *have* been proven against real fio (see "Real workload evidence").
- Native agent isn't wired into `.github/workflows/validate.yml` yet — CMake
  `FetchContent` needs network access in CI, which needs verifying separately.
- No UI exists for Linux. The agent's `/` and `/assets/*` routes exist and correctly
  503 with "UI build missing" (matches Windows' behavior for the same case).
- Host-side VM specs (assigned resource limits, host disk type, physical host
  capacity) are still unknown; not requested from the user yet.
- fio's `--rate` limiting, `ramp_time`-as-warmup, and lack of a DiskSpd-style
  unmeasured cooldown tail are documented approximations in `agent/fio.hpp`. The
  real runs so far (up to 5s measured, queue depth 8) didn't stress these heavily;
  not verified under sustained load, longer durations, or warmup/cooldown > 0.
- All real-run numbers recorded in this document (read bandwidth, IOPS, latency)
  are engineering-validation byproducts of short, rate-capped runs in a VM. They are
  not a disk performance characterization of anything, Linux's or otherwise, and
  must not be quoted as one.

## Pending gates

- [x] L0: environment, architecture and portable foundation.
- [x] L1: read-only Linux telemetry, transport — real HTTP/WebSocket server verified
      against live `/proc`/`/sys` data. No UI yet (not blocking; UI is separate from
      the telemetry/transport gate itself).
- [x] L2: native workload engine proven end-to-end against real fio, user-opted-in
      each time, for completed/cancelled/interrupted outcomes (including
      orphan-scratch recovery and honest, never-fabricated, recovery telemetry) and
      the port plan's specifically named trial (64 MiB/5s, restricted to `light`
      intensity by the correctly-enforced missing-thermal-coverage policy). A
      real-fio timeout (deadline, not user-cancel) case and "failed"/"aborted"
      outcomes remain fake-adapter-only — a real-hardware gap worth closing before
      claiming this fully bulletproof, but not blocking VM-scope L2 completion.
- [ ] L3: controlled experiments, analysis, learning and release preparation.
- [ ] Separate bare-metal Linux hardware and release validation.

The Windows Phase 6 real-run gate remains outstanding independently and does not
block this work.

## Next task

L2's VM-scope evidence is complete. Move to: (1) porting a minimal Linux UI (or
explicitly deferring it with rationale) for local transport/UI completeness, (2)
wiring the native agent into CI, (3) beginning L3 scaffolding (experiments,
analysis, Learn) at the same honesty bar Windows holds — matching Windows' own
current completion level (placeholder content, not fully built) rather than
inventing functionality Windows itself doesn't have working yet. Separately, and
lower priority: a real-fio timeout case and real "failed" outcome, and eventually
bare-metal Linux hardware validation (explicitly out of scope for a VM).

## Baseline handoff

The initial platform-separated checkpoint is tagged `windows-baseline-2026-09-16`
(commit `280ced5`). Linux work is on branch `codex/linux-port`, currently unpushed.
