# ADR-006: Local browser and native persistence

Status: accepted for Phase 0 baseline, 2026-09-06.

## Context
Electron adds a runtime; cloud service violates local scope; UI-owned DB complicates lifecycle and reliable capture.

## Decision
Same-origin browser UI, loopback HTTP/WebSocket agent and one SQLite writer in local application data.

## Consequences
Protect loopback with exact origins/token; SQLite failures affect run reliability; native implementation deferred to phase gates.

Validation is governed by docs/19-ROADMAP.md. Revisit through a superseding ADR,
not an undocumented implementation shortcut.
