# ADR-005: Contract-first domain

Status: accepted for Phase 0 baseline, 2026-09-06.

## Context
Independent language interfaces drift; early binary protocols complicate inspection and compatibility.

## Decision
JSON Schema 2020-12 is canonical; fixtures and semantic checks precede simulator/UI/native implementations.

## Consequences
Generate types, validate at boundaries, version explicitly and migrate recorded copies only.

Validation is governed by docs/19-ROADMAP.md. Revisit through a superseding ADR,
not an undocumented implementation shortcut.
