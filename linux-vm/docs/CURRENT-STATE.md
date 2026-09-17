# Linux VM current state

Updated 2026-09-17 (V1 completion underway: Phase 7A queue-depth sweep proven
against real fio, Milestones 1-2 done).

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
characterization of anything.

The React/Three.js UI is also ported and verified in a real browser (screenshots
below): the 3D scene renders, live telemetry flows through the WebSocket into it in
real time, and the Workload Lab correctly shows real admission data and the honest
`fio-3.41` engine label. Windows' `native-hosting-e2e.mjs` and `ui-smoke.mjs`
scripted Playwright checks are now ported too (`linux-vm/tools/`) and have been run
for real against a live agent and a live vite dev server — see "Scripted browser
checks" below.

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
each terminal state).

**Failed (2026-09-17, real fio, standalone probe, not through the full HTTP
API)**: there is no organic way to make a correctly-admitted, correctly-prepared
real workload fail without either contriving a fault or genuinely exhausting a
resource (which the safety policy exists specifically to prevent), so this was
proven with a small standalone probe using the actual project code
(`Scratch`, `run_child_process`, `fio_arguments`, the real `/usr/bin/fio`
binary) with preparation deliberately skipped, leaving the scratch target at 0
bytes. Real fio, given `--allow_file_create=0` and a file smaller than the
requested `--size`, genuinely refuses to extend it:
```
Real fio exit code: 1
Real fio stderr: fio: file creation disallowed by allow_file_create=0
EngineResult.state (matches workload_engine.hpp's real branch): failed
EngineResult.reason: fio exited with code 1: fio: file creation disallowed by allow_file_create=0
```
This proves `workload_engine.hpp`'s `else if(child.exitCode)` branch against a
real fio exit code and real stderr text, not `FakeEngine`'s scripted failure.
One side effect worth recording: real fio's own failure path unlinked the
0-byte target file itself (not this project's code), which — combined with the
probe's already-artificial skipped preparation — left the run's
`ownership.json` manifest orphaned; cleaned up manually, and would otherwise
have been caught by `recover_scratch()`'s orphan sweep on the next agent start
like any other interrupted run. This is a byproduct of deliberately skipping
preparation to trigger the failure safely, not a defect in the normal
preparation-then-execute flow, which always fills the file to the correct size
before fio ever runs.

**Aborted (safety-watchdog breach)** remains proven only via the fake-adapter
`FakeEngine` in `workload_controller_tests.cpp`. Unlike "failed," this one
can't be safely proven with a real workload without genuinely breaching a
resource reserve or temperature threshold mid-run — exactly the situation the
safety policy exists to prevent test infrastructure from ever doing on
purpose. Left as fake-adapter-only by design, not oversight.

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

CI: `.github/workflows/validate.yml` has `linux-validation` (TypeScript: contracts,
simulation, and now UI — 118 tests) and `linux-agent-validation` (native: configure,
build, `ctest`, no fio install since none of those tests touch real fio) jobs. Both
confirmed passing on a real GitHub Actions run — see "CI confirmed on GitHub
Actions" below.

## UI verification (2026-09-17, real browser, not just build/typecheck)

Ported `windows/apps/ui/` to `linux-vm/apps/ui/` (see PORT-PROVENANCE.md for the
five small text/logic edits). `npm run build` and the full `npm run verify` (118
tests now: the 109 from before plus 9 UI logic tests for `session`/`comparison`/
`live-state`) pass. Built output is served correctly by the native agent (`/`,
`/assets/*` return 200).

Beyond build/typecheck, actually loaded the served page in a real Chromium browser
(Playwright, manually installed since `npx playwright install` timed out on this
network — `curl` alone fetched the same file fine, so this was a client-side
timeout quirk, not a real connectivity problem) and interacted with it:

- The 3D scene (`DigitalTwin.tsx`, React Three Fiber) renders — board, chips, fan
  detail — and the Storage chip's inspector shows the real discovered device name
  "Block device sda (system scope)", not a placeholder.
- Live telemetry visibly updates in the DOM within seconds of page load: real CPU
  utilization, RAM available, storage throughput, GPU "Unavailable" (honest, no
  driver in this VM) — confirming the WebSocket → React state → rendered-DOM path
  works, not just that the API responds to `curl`.
- Footer correctly reads "LIVE TELEMETRY · native Linux agent" and the sidebar
  "LINUX / NATIVE FIRST" — the two build-label edits render correctly.
- Navigated to Workload Lab: real admission data rendered (disk/RAM available,
  reserves, planned writes, offered rate all matching live values), thermal
  restriction messaging correctly shown ("CPU or SSD temperature coverage is
  unavailable..."), and the engine line correctly reads "Workload engine fio-3.41 /
  Verified" — the artifact-kind and engine-label edits both render correctly.
- One console 404 (`/favicon.ico` — no favicon declared, cosmetic, matches what
  Windows' `index.html` would also produce; not a Linux-specific defect).

Screenshots saved locally (git-ignored, like Windows' `out/native-hosting.png`):
`linux-vm/out/ui-live.png`, `linux-vm/out/ui-workload-lab.png`.

Not yet ported at the time of that manual check: Windows' automated
`native-hosting-e2e.mjs`/`ui-smoke.mjs`-style scripted browser checks. Now done —
see "Scripted browser checks" below.

## Scripted browser checks (2026-09-17)

`@playwright/test` (`1.63.0`, same version as `windows/package.json`) is now a real
`devDependency`. `linux-vm/tools/native-hosting-e2e.mjs` and `linux-vm/tools/ui-smoke.mjs`
are ported from `windows/tools/` verbatim in structure, with three Linux-specific
adaptations, all recorded here rather than silent: no hardcoded Chrome path (Windows
hardcodes `C:/Program Files/Google/Chrome/Application/chrome.exe`; there's no single
canonical system Chrome path across Linux distros, so these use Playwright's own
managed Chromium instead), `args: ['--no-sandbox']` (needed to launch as a
non-privileged user in this VM), and `channel: 'chromium'` (this Playwright version
otherwise resolves a separate `chromium_headless_shell` binary for headless launches,
which isn't installed here). The path-traversal probe strings were changed from
Windows-specific (`C:secret`) to Linux-relevant (`%2Fetc%2Fpasswd`,
`nested%2F..%2F..%2Fetc%2Fpasswd`); same intent (reject absolute/rooted and
parent-traversal asset paths), different OS-relevant payloads. Like Windows, neither
script is wired into CI — both are manual/local verification tools on both platforms.

Both actually run, not just written:

```
npm run build && ./agent/build/ioscope_agent .          # serves built UI on :8765
node tools/native-hosting-e2e.mjs
  # PASS: native homepage, JS/CSS assets, rejected asset paths, mounted Workload Lab;
  # no workload started

npm run dev                                              # vite dev server on :5173
node tools/ui-smoke.mjs
  # PASS: six routes, simulation/pause, no browser exceptions
```

`npm run verify` still passes in full (118 tests, contracts/format/typecheck) with
the new files included in `format:check`'s glob.

## L3 investigation (2026-09-17): already at parity, nothing to port

Before scaffolding L3 (experiments, analysis, Learn), checked what Windows itself
actually has — the port plan says L3 is "dependent features," and the repo policy is
to match Windows' own completion level, not invent functionality Windows lacks.
Windows has **no experiment execution engine**: `grep -ri experiment windows/agent/`
finds only a `workload_controller.hpp` field (`experimentExecutionId`, always
`null`). There is no `ExperimentController`, no phase-sequencing code, nothing that
reads `docs/07-EXPERIMENT-SPEC.md`'s table into a real execution. What exists is:
contracts (`ExperimentDefinition.schema.json`, `ExperimentPhase.schema.json`,
fixtures) and UI nav placeholders (`Experiments`/`Learn`/`Analyze` all render the
same generic scenario-picker text). `windows/docs/CURRENT-STATE.md` says so
explicitly: *"Experiments/Learn/Analyze still contain placeholder content. The
project is not V1 complete."* — this is Windows' own stated remaining work, gated
behind its own outstanding Phase 6 hardware trial.

Both pieces are already ported and verified identical:
`diff windows/contracts/v1/ExperimentDefinition.schema.json
linux-vm/contracts/v1/ExperimentDefinition.schema.json` and the `ExperimentPhase`
equivalent are byte-identical (they were swept in with the original `contracts/v1/*`
copy). `diff windows/apps/ui/src/App.tsx linux-vm/apps/ui/src/App.tsx` shows the
only two differences anywhere in the file are the two build-label strings already
documented in PORT-PROVENANCE.md — the Experiments/Learn/Analyze nav items and their
placeholder content are untouched, byte-identical to Windows.

Building a real experiment-execution engine now would mean building new product
functionality that doesn't exist on *either* platform, which is a different kind of
task than porting and would leave Linux ahead of Windows in a way the repo's own
"match Windows' completion level" instruction says not to do. L3 is therefore
already at full parity with Windows' current (incomplete) state; there is nothing
further to port. It stays unchecked below because it's genuinely not built — same
as Windows — not because Linux is behind.

## V1 completion: Experiments (Phase 7), in progress (2026-09-17)

After the Linux port reached parity with Windows (L0-L3 above), the user asked to
complete the rest of V1 (Windows' own `docs/COMPLETION.md` Phases 7-11: experiments,
deterministic analyzer, contextual Learn, optional Ask, packaging) rather than stop
at parity. Since this VM has no Windows toolchain and Windows' own Phase 6 hardware
gate is still outstanding, this work proceeds **Linux-only** (Windows stays
untouched at Phase 5), skips the GPU pipeline experiment's execution code (7E,
capability-gated only — no GPU in this VM), and defers Phase 10 ("Ask") — all
confirmed with the user before starting. Plan: `~/.claude/plans/woolly-waddling-kahan.md`.

**Milestone 1 (contracts + admission plumbing) — done, real HTTP evidence:**

- Three new additive `$defs` in `contracts/v1/domain.schema.json`: `ExperimentAdmission`,
  `StartExperimentRequest`, `ExperimentExecution` (a lightweight progress/index
  record — each phase is still an ordinary `RunRecording` linked by the existing
  `experimentExecutionId`/`phaseId` fields, not a wire duplicate). New fixtures for
  all three; `contract_tests` fixture count went from 24 to 27 automatically.
- New semantic validation in `contracts.hpp` for `ExperimentExecution`: phase
  ordinals must match array order, a phase's `runId`/`outcome` must be non-null iff
  its index is before `currentPhaseOrdinal`, and `status:"completed"` requires every
  phase actually completed.
- `WorkloadController::start()` gained an optional `(experimentExecutionId, phaseId)`
  parameter pair (default null,null) rather than duplicating its ~15-field metadata
  construction in a second place.
- `Store` gained `experiment_executions` and `experiment_requests` tables
  (`user_version` 2→3) with upsert/read/list/recovery methods, mirroring the
  existing `journal_runs`/`workload_requests` pattern.
- New `agent/experiment_controller.hpp`: `ExperimentController` reuses the *same*
  `WorkloadController` instance phase-by-phase (per
  `docs/plans/PHASE-6-WORKLOADS.md`: "each phase uses the same admitted
  controller"), getting the one-experiment-at-a-time invariant for free from
  `WorkloadController::busy_`/`Store`'s single-row journal constraint. Aggregate
  admission calls `admit()` per phase with a running `alreadyWritten` total (an
  existing parameter that was already present but previously always called with
  its default `0`), plus a 600s total-wall-time cap (`08-SAFETY-SPEC.md`:
  "experiment <=600s") — both checked *before* any phase starts, matching
  "reject before allocating." Settling sleeps between phases with no sampling
  ("no benchmark samples ... in statistics"). Cancellation stops the in-progress
  phase (via `WorkloadController::cancel()`) and prevents any further phase from
  starting ("stop cancels current and future phases").
- New `POST /api/v1/experiments/admission`, `POST /api/v1/experiments`,
  `GET /api/v1/experiments/active`, `POST /api/v1/experiments/cancel` routes.
- New `experiment_controller_tests.cpp` (`FakeEngine`, mirroring
  `workload_controller_tests.cpp`'s style): proves aggregate admission, a single
  denied phase denying the whole experiment, the 600s aggregate-wall-time
  rejection, a full 3-phase sequence with correct `experimentExecutionId`/`phaseId`
  linkage on each phase's real `RunRecording`, requestId idempotency, cancellation
  stopping future phases, and recovery of an orphaned "running" execution as
  "interrupted." All 11 native test binaries pass (`ctest`).
- Verified against the real running agent, not just tests: `POST
  /api/v1/experiments/admission` with a real 6-phase queue-depth-sweep definition
  (QD 1/2/4/8/16/32, 4 KiB random read, 64 MiB working set, 5s each) at `light`
  intensity returns `allowed:true`, correct aggregate `totalWallSeconds:40` and
  `totalWriteBytes`, and a full per-phase `phaseAdmissions` breakdown; the same
  definition at `moderate` intensity is correctly denied per phase with "Missing
  CPU/SSD temperatures require light intensity and at most 15 seconds" — the
  restricted-thermal-coverage policy enforced per phase, exactly as it already is
  for single ad hoc workloads. `GET /experiments/active` and `POST
  /experiments/cancel` both correctly return `null` with nothing running.
**Milestone 2 (7A queue-depth sweep, real fio, user opt-in given) — done:**

Ran the real 6-phase queue-depth sweep (QD 1/2/4/8/16/32, 4 KiB random reads, 64 MiB
working set, 5s measured per phase, `light` intensity, 2s settle) through the actual
HTTP API (`POST /api/v1/experiments`), polled to completion (~62s wall time,
overhead beyond the ~40s estimate matches the per-phase preparation cost already
seen in single-run evidence):

```
GET /api/v1/experiments/active (final) -> status: completed, currentPhaseOrdinal: 6
  all 6 phases: outcome completed, distinct real recordingIds
```

Every phase's `RunRecording` fetched and checked individually: all report
`experimentExecutionId` matching the execution, `phaseId` matching their own phase,
`engine: {"name":"fio","version":"fio-3.41"}`, `outcome: "completed"`. Real measured
summaries per phase (rate-capped by `light` intensity's 32 MiB/s cap, not a
performance characterization — engineering-validation evidence in a VM):

| QD | IOPS | Read B/s | Latency mean (ms) |
|---|---|---|---|
| 1 | 2524 | 10.3 MB/s | 0.33 |
| 2 | 4517 | 18.5 MB/s | 0.37 |
| 4 | 7325 | 30.0 MB/s | 0.42 |
| 8 | 7952 | 32.6 MB/s | 0.88 |
| 16 | 7693 | 31.5 MB/s | 1.95 |
| 32 | 6381 | 26.1 MB/s | 4.86 |

This is genuinely the pattern the experiment's own hypothesis describes:
concurrency raises throughput until the rate cap is reached (QD1→8), then further
concurrency only adds queueing latency while throughput plateaus and *falls*
(QD16→32) — a real, honest demonstration of the concept, not a scripted result.
`ls ~/.local/share/ioscope/scratch/` was empty after completion — all 6 phases'
owned files and manifests cleaned up correctly, no orphans.

Not yet done: 7B/7C/7D real runs (7A proved the sequencing engine works; those are
lower-evidence-burden per the plan), the Experiments UI, and cross-phase comparison
display.

## Known limitations / not yet done

- Python-based cross-language fixture validation
  (`windows/tools/validate_contracts.py`) and the Windows doc-link checker are not
  ported. The TS/Vitest suite and the native `contract_tests`/`safety_tests` already
  validate fixtures from two independent language runtimes now, which is meaningful
  cross-language coverage even without porting the Python script itself.
- "Aborted" (a safety-watchdog breach mid-run — disk/RAM reserve or temperature
  threshold crossed while a workload is active, see `safety.hpp`'s
  `runtime_breach`) is proven only via `FakeEngine`, by design: proving it for
  real would mean genuinely breaching a resource reserve mid-run, which the
  safety policy exists to prevent test infrastructure from ever doing on
  purpose. Completed, cancelled, interrupted, and failed outcomes *have* all
  been proven against real fio (see "Real workload evidence").
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
- [x] L1: read-only Linux telemetry, transport, and UI — real HTTP/WebSocket server
      verified against live `/proc`/`/sys` data; UI ported and verified rendering
      live telemetry in a real browser.
- [x] L2: native workload engine proven end-to-end against real fio, user-opted-in
      each time, for completed/cancelled/interrupted/failed outcomes (including
      orphan-scratch recovery and honest, never-fabricated, recovery telemetry) and
      the port plan's specifically named trial (64 MiB/5s, restricted to `light`
      intensity by the correctly-enforced missing-thermal-coverage policy). Only
      "aborted" (mid-run safety-watchdog breach) remains fake-adapter-only, by
      design — see "Known limitations."
- [x] L3 parity checkpoint (superseded): confirmed Linux matched Windows'
      placeholder-only Experiments/Learn/Analyze state — see "L3 investigation."
      The user then asked to complete V1 itself; see "V1 completion" above,
      now in progress.
- [ ] Phase 7A-7D (experiments): admission/sequencing engine done (Milestone 1);
      7A (queue-depth sweep) proven end-to-end against real fio, 6 linked phases,
      real measured evidence (Milestone 2). 7B-7D and the UI not yet done.
- [ ] Phase 7E (GPU pipeline): capability-gate only, not started.
- [ ] Phase 8 (deterministic analyzer), Phase 9 (Learn): not started.
- [ ] Phase 10 (Ask): deferred at the user's request.
- [ ] Phase 11 (packaging/polish): not started.
- [ ] Separate bare-metal Linux hardware and release validation.

The Windows Phase 6 real-run gate remains outstanding independently and does not
block this work.

## CI confirmed on GitHub Actions (2026-09-17)

`codex/linux-port` pushed (authorized by the user) and all three
`validate.yml` jobs passed on a real GitHub Actions run
(run `35183846093`): `windows-validation` (unaffected, still green),
`linux-validation` (TypeScript: contracts/simulation/UI, 35s), and
`linux-agent-validation` (native: CMake configure + build + full `ctest`
suite, 1m42s) — the first time the native agent has built and its tests run
on GitHub's infrastructure rather than only this VM. `FetchContent`'s network
dependency resolved fine in that environment.

## Next task

Continuing V1 completion per `~/.claude/plans/woolly-waddling-kahan.md`,
milestone 3: 7B (block-size sweep) and 7C (buffered/unbuffered) — new
`ExperimentDefinition`s only, no engine changes expected, proven with one real
run each. Then milestones 4-9: 7D (needs a scratch-reuse engine change), 7E
capability gate, the Experiments UI (definition picker, admission preview,
phase progress, cross-phase comparison), Phase 8 analyzer, Phase 9 Learn, and
Phase 11 polish — each with its own commit and real evidence before the next
starts, per the plan.

## Baseline handoff

The initial platform-separated checkpoint is tagged `windows-baseline-2026-09-16`
(commit `280ced5`). Linux work is on branch `codex/linux-port`, pushed to
`origin` (2026-09-17); no pull request opened yet.
