# Product requirements

| ID | Behavior | Acceptance owner |
|---|---|---|
| P1 | Live opens the twin with capability-aware CPU/RAM/storage/GPU metrics | Telemetry, UI |
| P2 | Visible LIVE, SIMULATED or RECORDED badge; replay also shows original origin | Contracts, UI |
| P3 | Orbit, pan, zoom, select, focus, isolate and reset; equivalent keyboard actions | 3D |
| P4 | Physical layout and conceptual data-path views with provenance legend | 3D |
| P5 | Five workload controls plus sequential/random; bounded advanced values | Workloads, Safety |
| P6 | Five serially implemented experiments with explanation and saved results | Experiments |
| P7 | Record, list, compare two, scrub, step, play/pause, replay speed | Persistence |
| P8 | Deterministic evidence/interpretation events and optional simple explanations | Analyzer |
| P9 | Learn topics link to component highlighting and experiments | Learn |
| P10 | Optional AI receives bounded structured context only | Analyzer |
| P11 | Missing sensors, disconnects and failures remain visible and recoverable | Errors |
| P12 | Agent independently validates budgets and can cancel/clean up | Safety |

Default Live never silently falls back to simulation. With no agent, show disconnected
and an explicit simulation entry. No hardware run starts from visiting a page.
All generated activity is recordable; recording is default on and admission fails if
the run cannot be created. Benchmark measurements must state system or workload scope.
Acceptance is phase-specific in 19-ROADMAP.md; visual polish alone cannot pass V1.
