# Local transport contract

Implemented agent version 0.6.0, protocol/schema version 1.0.0. Prefix `/api/v1`.
All routes bind to 127.0.0.1:8765. The production UI is served by the agent; Vite
at 127.0.0.1:5173 proxies API and WebSocket traffic during development.

| Operation | Input | Result |
|---|---|---|
| GET /bootstrap | none | Per-launch token, protocol/agent versions, feature flags |
| GET /inventory | none | HardwareInventory |
| GET /capabilities | none | DeviceCapabilities[] |
| GET /telemetry | none | Current TelemetryFrame |
| POST /runs/admission | WorkloadDefinition | WorkloadAdmission; no workload side effects |
| POST /runs | StartWorkloadRequest | WorkloadSnapshot; native admission repeated before work |
| GET /runs/active | none | WorkloadSnapshot |
| POST /runs/cancel | none | WorkloadSnapshot; requests cancellation of active run |
| GET /recordings | none | Up to 100 newest recording descriptors |
| POST /recordings | Recording | Opaque stored recording ID |
| GET /recordings/{id} | 32-character opaque ID | Recording |
| DELETE /recordings/{id} | 32-character opaque ID | Deletion acknowledgment |
| GET /artifacts/{id} | 48-character opaque ID | ArtifactPayload |

REST uses a Bearer token except bootstrap. Host and Origin are checked. Mutations
run only through explicit routes. Workload JSON commands are bounded to 64 KiB,
imports to 32 MiB and nesting to 32. Failed requests currently return 400 SystemError;
HTTP authorization errors use 401/403. Crow 1.2's unsupported 422 code is not used.
Do not rely on planned 202/409/422 response behavior from earlier drafts.

Start request IDs and exact canonical controls are durable in SQLite. An identical
retry returns its original snapshot, even after restart/history deletion. Reusing an
ID with different controls is rejected. One native run is admitted at a time. The
workload continues independently of browser connectivity. Completion is published
only after validated recording/artifacts commit. The current status is polled by UI.

WebSocket `/stream` requires a canonical StreamRequest with token within two seconds.
It then emits TelemetryEnvelope once per second, at most one unacknowledged frame.
The client acknowledges sequence using StreamRequest. A pending frame times out in
three seconds; at most eight clients connect. Reconnect receives a fresh snapshot,
not a fabricated backfill. Gap/session-reset notices are derived by the client.
Workload/analyzer event streaming and experiment routes remain future phases.

Security headers are included for allowed requests. Crow's automatic OPTIONS route
cannot reliably inspect Origin at its early parsing stage, so the UI uses same-origin
requests through native hosting or Vite proxy. Cross-origin preflight is not a supported
integration path. Tokens never appear in URLs or saved browser storage.
