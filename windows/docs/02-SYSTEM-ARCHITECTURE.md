# System architecture

## Selected design
Local browser UI served by a per-user native C++20 agent in the packaged product.
Development uses Vite with an explicit allowed origin. No Electron, WSL or cloud
service is required. Native adapters own hardware; frontend owns presentation.
SQLite has one native writer. Simulation initially runs as a pure TypeScript library.

```text
apps/ui → DataSource interface → live transport | simulation | recorded reader
                                    ↓ normalized domain objects
                              timeline → flow mapper → R3F + inspectors

agent/core → telemetry adapters → normalized samples
           → workload controller → safety → DiskSpd adapter
           → experiment orchestrator → serial workload phases
           → deterministic analysis → events
           → storage writer → SQLite
           → transport → same-origin HTTP / WebSocket
```

Transport handles envelopes, not business decisions. Safety is not a frontend check.
Controllers depend on WorkloadEngine, TelemetryProvider, Clock, RunStore and
ResourceProbe interfaces with fake implementations. No UI dependency enters agent/core.

## Alternatives
Electron simplifies packaging but adds a runtime and does not remove the native agent.
A Python agent accelerates experiments but weakens the requested C++ focus and adds
packaging complexity. Select browser plus C++ agent; revisit desktop wrapping only
after interaction and launch requirements are measured.

## Scheduling and ownership
Telemetry acquisition defaults to 1 Hz; render clock is independent. Per-adapter
deadlines isolate blocked sensors. A bounded writer queue preserves ordered run
samples; overflow records a gap or aborts recording visibly, never fabricates samples.
The agent watchdog survives UI disconnect. Only one active hardware experiment/workload.
A process Job Object bounds descendants; per-run resources are RAII-owned.

## Delivery sequence
Contracts → deterministic simulation → shell → scene → replay → live adapters →
safe workload execution → experiments → analysis → learning → optional AI → polish.
See ADRs, transport spec, and roadmap for protocol and release gates.
