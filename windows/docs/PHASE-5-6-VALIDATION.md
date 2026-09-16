# Native telemetry and workload validation

Updated 2026-09-08. Phase 5 is verified. Phase 6 implementation and automated safety
checks are present; its real-run acceptance gate remains untested.
No DiskSpd benchmark has run on this machine.

## Telemetry evidence

The Windows adapter discovers the CPU and GPU names, binds storage counters to the
physical disk containing the private scratch volume, and marks unsupported metrics
unavailable. Authenticated WebSocket delivery enforces acknowledgments, bounded
clients and deadlines. Browser tests cover disconnect, stale values and reconnect
with a new agent session and token. Counter tests cover reset and invalid deltas.

Read-only collection over 63.279 seconds used 0.00849% normalized CPU and at most
25,694,208 private bytes. Ten adapter polls took at most 1.3113 ms each. These are
single local trials, not measurements of benchmark interference. Evidence files:
`out/telemetry-overhead.json`, `out/adapter-probe.json`, `out/live-inventory.json`,
`out/live-telemetry.json` and `out/live-ui.png` (generated, ignored by Git).

## Workload implementation

The native controller admits one run, checks fresh resources, owns the watchdog,
and accepts only canonical controls. Preparation and worst-case offered writes
count toward the 8 GiB budget, with 64 MiB reserved for artifacts/journaling. Missing CPU/SSD temperatures restrict runs to light
intensity and at most 15 seconds. Actual rate is integer bytes/ms (33,554,000 bytes/s
for light); the safety budget rounds upward. No raw devices or external paths.

Scratch uses a private same-user ACL and exclusive creation. Held handles prevent
target replacement during execution. DiskSpd shares read/write but not delete, so
cleanup reopens the target after execution and verifies the original volume/file
identity before deleting by handle. Recovery checks the bounded ownership manifest,
regular-file identity, single link, allowed filenames and non-reparse ancestry.
Ambiguous contents are preserved and block subsequent runs. An application lock
prevents two agents from recovering or executing against the same storage.

Preparation writes and flushes execute in an isolated preparation helper using an
explicitly inherited owned-file handle. Its parent keeps checking resources and
cancellation; initialization has a size-based deadline. Suspended children enter
a kill-on-close Job Object before resuming. The runner
bounds memory, output and duration; handles thread/reader failures; catches watchdog
errors; and force-stops after a bounded graceful-cancellation interval. Ordinary
unit tests use fake children and 4 KiB owned scratch fixtures, never a benchmark.
Windows kernel I/O stalls and injected OS allocation/wait failures are not measured.

SQLite schema 2 keeps request-ID tombstones, an append-only active sample journal,
recordings and raw XML artifacts. Duplicate IDs cannot start another run, including
after history deletion. Terminal metadata, artifact and recording are committed
together before completion is published. Crash recovery preserves prior samples
and adds an unavailable interrupted sample. Its elapsed time is the last known
clock value; `endedAt` records recovery finalization, not the unknown crash time.

Native replay preserves LIVE origin, measured/derived provenance and real sample
spacing. A recorder clock includes preparation. The `running` state covers the
helper invocation, including configured warmup/cooldown; only DiskSpd summaries
describe its measured interval. System metrics are not relabeled workload metrics.

## Automated checks and real admission

`tools/build-agent.ps1 -Configuration Release` runs ten CTest programs, including
controller denial without side effects, idempotency, cancellation, durable completion
and recovery; exact output caps and fake child failure; file sharing and orphan
identity mismatch; XML/DTD/count rejection; and shared contract fixtures.

`node tools/workload-admission-e2e.mjs` exercises the actual native admission API,
disabled Start button, changed controls and a 390-pixel layout. It never clicks
Start, even on a machine that passes admission. The screenshot is
`out/workload-lab.png`; actual read-only admission is `out/workload-admission.json`.
Before the user-requested policy revision, a trial reported 92.01 GiB available and a 188.01 GiB required reserve.
A 64 MiB target therefore needs approximately 96.1 GiB more free space at that
instant. Available space varies; refresh admission before retrying. Available RAM was
5.37 GiB versus 6.58 GiB required including reserve and buffers.

Remaining Phase 6 evidence: a bounded opt-in 64 MiB real read trial, validated raw
DiskSpd XML, recorded measurements and successful cleanup. Do not mark this gate
passed or begin dependent experiments until that evidence exists. No automatic
cleanup of user files and no reserve-policy relaxation is authorized or needed.

## Dependency and storage notes

DiskSpd is a separate pinned Microsoft binary dependency. Read its release license
before using `tools/setup-diskspd.ps1 -AcceptDiskSpdLicense`; installation verifies
archive and executable hashes and starts no workload. The source repository's MIT
license does not replace the release binary's license. Do not bundle that binary.
See the [official release](https://github.com/microsoft/diskspd/releases/tag/v2.3),
[arguments](https://github.com/microsoft/diskspd/wiki/Command-line-and-parameters)
and [file-opening implementation](https://github.com/microsoft/diskspd/blob/v2.3/IORequestGenerator/IORequestGenerator.cpp).

Under the packaged Codex host, Windows redirects LocalAppData to the package's
LocalCache. The agent resolves the final path before scratch ownership checks.
Outside that host, ordinary LocalAppData can select a different database directory.
Both remain local and outside OneDrive. Do not assume history crosses host contexts.

The RunRecording fixture is an authored fake-adapter contract test, not captured
hardware evidence. Its origin exercises the native branch of validation only.

Final automated result: 118 JavaScript tests, 10 native CTest programs, 25 schemas
and 24 root fixtures pass. Four C++ fake-adapter recordings (completed, cancelled, failed
and interrupted) also pass the frontend semantic validator. Production UI build
passes with a reported large-bundle warning; no workload overhead claim is made.

The final browser/API regression checks passed: six routes, simulation pause,
save/reload/replay/step/speed, native resource admission, changed controls, mobile
layout, token/origin enforcement, invalid imports, bounded JSON commands, stream
backpressure and real agent disconnect/restart. Diagnostic artifact preservation
and atomic rollback on artifact insertion failure are covered by native tests.

## User-requested correction: trial size and native hosting

On 2026-09-08 the user rejected the original 10%-of-volume disk reserve. The earlier
188GiB requirement above is historical and superseded. Native disk admission now
requires a fixed 2GiB free-space floor after the test file and 64MiB artifact/journal
allowance. RAM keeps a fixed 2GiB floor after actual planned buffers. Default trial
is 64MiB, five seconds, 100% reads after preparation. New boundary tests check exact
allocation-plus-reserve admission, one-byte-short denial and independence from total
drive capacity. Temperature, queue, write-budget, job and cleanup limits remain.

The port-8765 homepage previously returned 404 because Crow's filename sanitizer
rewrote the drive-letter colon in an absolute Windows path. The new regression
failed against that behavior before the fix. Server-owned paths now use the raw
static-file API after URL basename and resolved-directory validation. The native
hosting regression checks root HTML, scripts, styles, invalid asset paths and a
mounted Workload Lab on the production port. Earlier Vite-only browser checks did
not cover this path. No benchmark is started by these corrective checks.

Correction verification passed: tools/native-hosting-e2e.mjs reports HTTP 200 for
root HTML and compiled JS/CSS, rejection of invalid asset paths, and a mounted
Workload Lab at port 8765. The actual revised admission response is allowed=true,
with diskReserveBytes=2147483648, diskAllocationBytes=134217728,
ramReserveBytes=2147483648 and bufferBytes=269488128. The active run is null;
no hardware trial started. Screenshot: out/native-hosting.png.
