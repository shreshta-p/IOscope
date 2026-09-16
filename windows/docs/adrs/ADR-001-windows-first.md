# ADR-001: Native Windows first

Status: accepted for Phase 0 baseline, 2026-09-06.

## Context
WSL/Linux-first would alter storage paths and weaken laptop telemetry fidelity. Cross-platform adapters are a later extension.

## Decision
Windows 11 is the supported V1 runtime; Win32/PDH/NVML adapters behind domain interfaces.

## Consequences
Windows toolchain and hardware gates remain necessary; no Linux runtime or VM dependency.

Validation is governed by docs/19-ROADMAP.md. Revisit through a superseding ADR,
not an undocumented implementation shortcut.
