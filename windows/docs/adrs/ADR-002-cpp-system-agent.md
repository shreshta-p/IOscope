# ADR-002: C++20 native agent

Status: accepted for Phase 0 baseline, 2026-09-06.

## Context
Python is faster to prototype; Rust offers strong ownership, but the requested native C++ engineering focus favors C++20.

## Decision
Use modern C++20 with RAII for telemetry, safety, process lifecycle, persistence and local transport.

## Consequences
Need explicit fake interfaces, compiler setup and bounded concurrency; UI stays TypeScript.

Validation is governed by docs/19-ROADMAP.md. Revisit through a superseding ADR,
not an undocumented implementation shortcut.
