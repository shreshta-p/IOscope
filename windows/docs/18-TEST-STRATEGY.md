# Test strategy

Phase 0 runs schema metaschema validation, one valid fixture per root, semantic checks,
negative mutations and document-link/required-file checks. Commands in TOOLCHAIN.md.
This does not prove simulation, native compilation or hardware safety.

Phase 1: seeded byte-equivalence, different-seed effects, all eight scenarios, numerical
bounds, unavailable injection, stable ordering, golden fixture version and contract validation.
Phase 2–4: component tests, accessible mode badge and keyboard selection, WebGL fallback,
deterministic scene derivation, replay seek/step/speed/gaps and bounded-memory soak.
Phase 5: fake Win32/PDH/NVML failure matrices, counter warmup/reset, stale samples,
shutdown/reconnect and separately opt-in hardware/overhead report.
Phase 6: argv tests, XML fixtures, malformed/XXE output, resource admission, path attacks,
Job Object cancellation/crash/orphan tests, actual bounded scratch run only after passing.
Phase 7: serial order, invariant controls, cumulative budget, cancellation between phases,
phase timing and unsupported CUDA paths. No global cache flush.
Phase 8–10: evidence availability, rule thresholds/recovery/debounce, original replay
events, educational links and optional-AI failure with no lost local functionality.
Phase 11: packaged clean-user setup, E2E flagship runs, attribution and performance report.

C++ tests use CTest and a pinned lightweight framework; TS tests use Vitest.
Playwright covers start/cancel/save/replay/compare with fake agent before native E2E.
Schema fixtures are shared by TS and C++; reject version drift at CI.
Fix bugs with regression tests where practical. Never automatically run hardware
benchmarks in CI. Report skipped hardware checks by name, not as green success.
