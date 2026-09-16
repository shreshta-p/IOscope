# Phase plan and acceptance gates

Stop at each boundary if evidence is missing. Passing documentation checks does not
pass hardware or UI gates. Every gate records commands, versions, results and limitations
in CURRENT-STATE.md or a linked evidence report.

| Phase | Deliverable | Exit evidence |
|---|---|---|
| 0 | specs, ADRs, contracts, fixtures, toolchain strategy | all required docs exist; schemas/positive/negative fixtures and links pass; assumptions recorded |
| 1 | pure deterministic simulator | eight scenarios; same config/seed/version byte-identical; catalog/schema/semantic tests; no hardware I/O |
| 2 | six-route UI shell | mode/availability truth, fake source swapping, keyboard navigation, typecheck/build/component tests |
| 3 | fixed twin and flows | select/focus/isolate/reset and both views; mapping tests; WebGL fallback; measured FPS and memory report |
| 4 | recording/replay from simulated runs | seek/step/speed restore snapshots; events/gaps deterministic; SQLite migrations/corruption/recovery tests |
| 5 | native CPU/RAM/storage/GPU adapters | real discovery, unavailable states, reset/stale/reconnect/shutdown tests; sampling overhead report |
| 6 | safe DiskSpd wrapper | version/hash, command/parser fixtures, reserves/paths/cancel/job/orphan tests; bounded opt-in real run |
| 7A | QD experiment | fixed controls, ordered phases, results/save/replay and no cap/occupancy confusion |
| 7B | block sweep | unit-consistent comparisons, same fixed controls, recorded phases |
| 7C | cache mode experiment | cache-only variable, alignment validation, reversed order results |
| 7D | first/repeated access | uncontrolled-cache disclaimer, preparation recorded, no purge |
| 7E | GPU pipeline | real phase timing/checksum/release or explicit unsupported capability; simulated demo labeled |
| 8 | deterministic analyzer | positive/negative boundary cases, evidence references, recovery/debounce and replay stability |
| 9 | Learn | concise accurate topics with working component/experiment links and accessibility |
| 10 | optional Ask | bounded/redacted context, evidence/hypothesis separation, offline/failure tests |
| 11 | release polish | two flagship demos, setup/package test, measured budgets, screenshots, limitations, attribution |

Phase 7 is serial, not five parallel implementations. Phase 8 analysis events are not
required to be real diagnoses in earlier simulation scenes; those fixtures are labeled.
No native benchmark until Phase 6 safety gate. No product implementation in Phase 0.
