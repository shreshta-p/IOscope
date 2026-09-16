# Errors and recovery

| Condition | Result | Recovery |
|---|---|---|
| NVML absent / function unsupported | capability unsupported, metric null | other telemetry continues |
| Sensor permission denied | unavailable with requires-elevation reason | explain capability; no global elevation |
| Counter reset / invalid value | null measurement | retry next valid interval |
| Adapter timeout | temporarily_errored; independent deadline | bounded retry/backoff |
| WebSocket lost | visible stale/disconnected; no synthetic fallback | 0.5,1,2,4,8s capped retry with jitter |
| DiskSpd absent/version mismatch | start rejected | show installation/version requirement |
| Child crash / forced cancel | failed/aborted run, partial samples | close job and owned resources |
| SQLite failure | stop recording and active workload | preserve diagnostics, user retry after repair |
| Corrupt run | reject import/read | keep source and other records intact |
| Insufficient disk/memory | reject or runtime abort | show budget calculation |
| WebGL context loss | accessible metrics/list fallback | controlled renderer recreation |

SystemError carries stable code, operation, correlation ID, optional run ID, safe
message, retryable flag and timestamp. No swallowed exceptions or arbitrary exception
string exposed to browser. Differentiate cancelled, aborted, interrupted and failed.
Retries never implicitly restart a workload; a retried start request is idempotent.
