# UI, twin and simulated recording evidence

Verified locally on Windows, 2026-09-06. Native Release compilation and four CTest
programs pass. The JavaScript suite passes 110 tests; schema generation, formatting,
strict types, 18 root fixtures, negative mutations and documentation links pass.

`node tools/ui-smoke.mjs` exercises six routes and simulation controls.
`node tools/replay-e2e.mjs` saves through the UI into SQLite, reloads, reopens the
saved run, checks RECORDED/SIMULATED provenance and exercises seek, step, speed and
end restart. Its own record is deleted afterward. `node tools/storage-smoke.mjs`
checks authorization, hostile Origin, roundtrip and invalid units, flows, queue
evidence, session identity and sample counts.

Native storage tests cover initial schema creation, parameterized statements,
durable reopen, unknown-version and corrupt database rejection. A separate test
process commits to WAL then exits without destructors; reopening recovers the
committed recording. No real user database is corrupted by these tests.

The procedural board supports selection, focus, isolate, reset, physical/data-path
views and keyboard component buttons. Its placement is illustrative. Unknown flow
metrics suppress particles. Replay preserves canonical flow snapshots. Measurement
mode uses demand rendering and pauses continuous particle animation.

Performance reports (local ignored out artifacts):
- scene-performance.json: 300 seconds, 1440x900 headless Chrome, p95 animation-frame
  interval 4.4 ms, 41 draw calls, 3,050 triangles, 42 geometries and one texture.
- stability-soak.json: frozen production build; 32 minutes total, two-minute warmup,
  explicit Chrome heap collection each minute. Retained heap 12,945,700 to 13,808,800
  bytes over the remaining 30 minutes: +6.67%, below the 10% target. No page errors.

Machine has Intel UHD driver 31.0.101.5186 and RTX 4080 Laptop driver 32.0.15.9200;
Windows power plan was Silent. Browser render adapter was not captured. These are
single headless trials, not display-presented FPS, benchmark distortion measurements
or guarantees for other hardware and power modes.

Development proxies /api through Vite; production serves UI and API on the same
origin. Crow 1.2 automatic OPTIONS handling runs before headers are parsed, so
dynamic cross-origin preflight must not be relied on. Host/Origin checks and Bearer
authentication still apply to actual requests and WebSocket admission.

Storage currently persists bounded complete simulation artifacts. Native workload
recording and indexed active-run journaling belong to the workload phase. Route
shells do not imply that unfinished workload/experiment/Learn/analyzer engines exist.
