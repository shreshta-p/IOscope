# Security and privileges

Threat model: malicious web origins reaching loopback, malformed requests/imports,
path traversal/reparse races, compromised/unexpected helper binaries and orphan writes.
Run as the interactive user. No service/elevation/driver installation in baseline.

Bind 127.0.0.1 only; packaged UI served by agent. Validate Host and Origin against an
exact allowlist; no wildcard CORS. Require a random per-launch session token on commands
and WebSocket establishment. Deliver via owned launcher/session bootstrap, not URL
query, logs or persistent browser storage. Token is local request protection, not a
multi-user authentication system. Browser WebSocket uses first-message authentication
with a 2s deadline; no samples/commands before it. Reject unexpected origins first.

Body <=64KiB for commands, import <=32MiB, XML <=16MiB, nesting <=32;
reject duplicate JSON keys, nonfinite numbers and unknown fields. Disable XML external
entities/DTDs. REST commands are allowlisted and schema validated, no generic executor.
Run IDs are opaque; frontend cannot choose filesystem paths. CSRF protection applies
even on loopback. Start request ID maps to one run; no repeated workload on retries.

Trust helper executable only after pinned hash/version validation. Load NVML from
verified system/vendor location, not current-directory search. Use bounded process
pipes, close inherited handles, job isolation and private scratch ACLs.
Optional AI keys remain native-side; external upload requires explicit opt-in.
Security tests precede hardware workload exposure.
