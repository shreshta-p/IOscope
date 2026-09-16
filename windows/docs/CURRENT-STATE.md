# Current state

Updated 2026-09-16. Phases 0-5 are implemented and locally verified. Phase 6 native
workload implementation and automated safety checks are present; its real-run gate
has not yet been exercised on real hardware. Continue only when prerequisite gates have evidence.

## Delivered
Canonical schemas and generated TypeScript; eight deterministic scenarios; React
six-route shell; interactive R3F board; honest source labels; simulation and native
recording replay with original timing; SQLite history/import/export/comparison.
Read-only Windows CPU/RAM/PDH/NVML discovery and authenticated streaming work,
including unavailable values, stale/disconnect/reconnect behavior and overhead trials.

Native Workload Lab now shows real admission, typed controls, status and cancellation.
The engine pins DiskSpd, uses private owned scratch, isolated preparation and workload
children, resource/temperature watchdogs and bounded outputs. SQLite journals active
samples, preserves request-ID idempotency, commits completion and XML artifacts
atomically, and recovers interrupted evidence. Orphan cleanup verifies ownership and
file identity; ambiguous contents are preserved. Runs can inspect workload summaries
and download checksum-verified artifacts. No DiskSpd benchmark has run yet.

## Verification

The checks below were rerun successfully after the platform-folder reorganization
on 2026-09-16, including a fresh native Release build and the actual port 8765
hosting regression. See [reorganization evidence](REPOSITORY-LAYOUT.md). Earlier
performance trials were not repeated; no real hardware workload was started.
118 JavaScript tests and 10 native Release CTest programs pass. Native completed,
cancelled, failed and interrupted fake-adapter recordings pass TypeScript validation.
25 schemas, 24 root fixtures, 27 invalid mutations and the golden recording pass.
UI production build passes, with a large-bundle warning. Browser tests cover the
native admission response, disabled Start, control changes and mobile layout.
See [Phase 2-4 evidence](PHASE-2-4-VALIDATION.md) and
[Phase 5-6 evidence](PHASE-5-6-VALIDATION.md) for measured trials and limitations.

## Current trial and native hosting
The user rejected the original capacity-percentage reserve on 2026-09-08. The revised
policy leaves 2GiB disk space after the actual target and a 64MiB recording allowance,
and 2GiB RAM after planned buffers. Default: a 64MiB file and five seconds of reads;
preparation writes the file once. Total free disk space required is 2.125GiB, not
188GiB. The UI separates trial allocation from untouched spare-space requirements.
The real bounded trial, validated XML and cleanup evidence remain required before
Phase 6 passes. This correction does not itself start a benchmark.

Native hosting returned 404 because Crow sanitized the drive-letter colon in an
absolute Windows path. The server now validates asset basenames and resolved location
before passing server-owned paths to Crow's unsanitized file API. Missing UI builds
return an actionable 503. tools/native-hosting-e2e.mjs checks the actual port 8765
homepage, assets, invalid paths and mounted application, rather than only Vite.
That regression now passes on port 8765. Actual revised admission is allowed=true;
no workload was started. Screenshot: out/native-hosting.png.

## Run

All commands and relative paths below are rooted at `windows/`. The repository was
split into independent `windows/` and `linux-vm/` workspaces on 2026-09-16.
See [reorganization and handoff](REPOSITORY-LAYOUT.md). Linux implementation has not
started; its gates do not inherit the Windows results.
`npm run dev` serves http://127.0.0.1:5173 and proxies /api to the local native agent.
`./tools/build-agent.ps1 -Configuration Release` builds/tests the agent and helper.
Run `./build/native-vs16/Release/ioscope_agent.exe .` from this repository (VS2019).
`npm run build` creates the UI served at http://127.0.0.1:8765 by the agent.
`npm run verify` checks generation, formatting, types, tests, Python contracts/docs.
`npx tsx tools/native-recording-check.ts` checks native fake-adapter recordings after
native CTest. DiskSpd remains a separately licensed dependency; see the evidence doc.

## Remaining
Pass the Phase 6 hardware gate, then serial experiments, optional GPU capability,
deterministic analysis, contextual Learn, optional Ask and release packaging.
Experiments/Learn/Analyze still contain placeholder content. The project is not V1
complete. The [completion checklist](COMPLETION.md) records each remaining gate.
This workspace is the Windows baseline for the platform-separated repository.
Use `git status` and `git remote -v` for current checkout and publication state.
Keep DB/scratch in resolved LocalAppData outside OneDrive. Test fixtures never
represent real captured hardware evidence.
