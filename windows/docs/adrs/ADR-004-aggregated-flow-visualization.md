# ADR-004: Aggregated visual flows

Status: accepted for Phase 0 baseline, 2026-09-06.

## Context
Per-I/O animation is not meaningful at real rates; decorative activity undermines technical honesty.

## Decision
Use one versioned metric-to-flow mapping; store normalized flow snapshots with recordings.

## Consequences
No request identities/counts without tracing; cap particles; provenance and direction remain inspectable.

Validation is governed by docs/19-ROADMAP.md. Revisit through a superseding ADR,
not an undocumented implementation shortcut.
