# Phase 0 validation evidence

Date: 2026-09-06. Scope: specifications and initial contracts only.

Observed `python tools/validate_contracts.py`: PASS, 15 schema files (one canonical
definition bundle and 14 public roots), 14 valid root fixtures, one unavailable
measurement fixture, 27 rejected invalid mutations. All schema references resolve
from a local registry. Fixture flow activity matches the canonical scaling formula.

The invalid timestamp regression initially failed because jsonschema lacked its
optional date-time validator. Added pinned rfc3339-validator and a startup guard;
the invalid timestamp now rejects. This is validator-tooling evidence, not a native
telemetry or simulation implementation test.

Observed `python tools/check_docs.py`: PASS, 28 required documents, ADR presence,
AGENTS.md and 14 relative Markdown links. Six ADRs are present.

Not performed: C++ build, frontend build, deterministic simulation generation,
hardware inventory, NVML/CUDA checks, DiskSpd execution, safety runtime tests,
SQLite persistence, browser tests, asset performance or sampling overhead.
No benchmark or GPU activity was started; no reference asset was imported.

Phase 1 can begin from this baseline. Runtime semantic validation, generated language
types and additional negative fixtures are Phase 1 work, not implied by schema shape.
