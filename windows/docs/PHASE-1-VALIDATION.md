# Phase 1 validation

Environment: Windows, Node 24.20.0, npm 11.19.0, Python 3.13.
Phase scope: contracts, pure simulation, normalized flow snapshots and export.

Implemented:
- Canonical schema → generated TypeScript with drift detection.
- AJV 2020-12 plus format validation and semantic boundary checks.
- Eight seeded scenarios, virtual clock, unavailable injection and bounded generation.
- Explicit simulated pipeline stages; canonical aggregate flow mapping.
- Versioned simulation manifest, metadata and replay snapshots.
- Exclusive-create export CLI and golden fixture stored in the workspace.
- Windows CI workflow; remote CI has not run because there is no remote.

Verification commands:
```powershell
npm ci
npm run verify
npm audit --audit-level=high
```

The verification suite covers 101 tests, strict TypeScript, generated-type drift,
formatting, 17 schema files / 16 root fixtures, 27 rejected Python mutations,
the twelve-snapshot exported golden recording, and local documentation checks.
Final local run: all checks passed after npm ci; 101 tests passed and the dependency
audit reported zero vulnerabilities. Documentation checker resolved 17 local links.
Golden SHA-256: 4c0dd06dd35c8825f8d91e1bd81ce362ca7ab71b517a053a1a217e01dee632d7.
Files remain uncommitted on codex/phase-1-simulation; nothing was pushed.

Test-first evidence: contract positives failed against unimplemented validator;
scenario positives failed against unimplemented generator; flow/record positives
failed against unimplemented mapping/recording; CLI export failed against its stub.
Each subsequently passed. Cross-process output comparison and frozen golden bytes
check determinism independently of repeated calls within one process.

Independent review reproduced ambiguous analyzer evidence and running metadata with
completed samples. Regression tests failed, then passed after source resolution and
terminal-state validation fixes. Additional regressions reject forged GPU stage
activity and inconsistent status clocks.

Dependency audit identified a Vitest UI-server advisory in the initial 4.0.18 choice.
Pinned Vitest 4.1.11; follow-up audit reported zero vulnerabilities. Tests use run mode
and no UI server. Lockfile preserves exact dependency resolution.

No UI/native agent/build, SQLite runtime, real telemetry, benchmark, hardware safety
or performance claim is made. A 6000-sample generator boundary test proves termination,
not target-machine sampling overhead. The next gate is Phase 2 UI shell.
